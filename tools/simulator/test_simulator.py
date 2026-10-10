#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Native screen inventory and meaningful navigation/storage regression checks.

Stop the preview server before running. Test storage is restored after every test.
"""
# 旧入口依赖日常存储回放，停用以免测试覆写用户资料。
# Retired: replaying daily storage may overwrite user data; use isolated replacements.
raise RuntimeError("Legacy storage-replay test retired. Use tools/test_cloud_host.py, tools/test_cloud_etag.py and tools/test_if_simulator.py with isolated storage.")

from pathlib import Path
import shutil
import subprocess
import tempfile
import json
import unittest

from server import BUILD, ROOT, CARD, STORES, Preview
from views import VIEWS


class SimulatorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.saved = tempfile.TemporaryDirectory(dir=BUILD, prefix='test-backup-')
        for name, folder in STORES.items():
            if folder.exists():
                shutil.copytree(folder, Path(cls.saved.name)/name)

    @classmethod
    def restore(cls):
        for name, folder in STORES.items():
            if folder.exists():
                shutil.rmtree(folder)
            saved = Path(cls.saved.name)/name
            if saved.exists():
                shutil.copytree(saved, folder)

    def tearDown(self):
        self.restore()

    @classmethod
    def tearDownClass(cls):
        cls.restore()
        cls.saved.cleanup()

    def preview(self):
        return Preview(build_native=False)

    def view(self, p, name):
        return p.action({'action':'view', 'view':name})

    def tap(self, p, x, y):
        return p.action({'action':'gesture','type':'t','x':x,'y':y,'xx':x,'yy':y})

    def key(self, p, k):
        return p.action({'action':'key','key':k})

    def sample_documents_only(self):
        # Operate on fixtures during MD regressions; tearDown restores user files.
        for path in (CARD/'markdown').glob('*.md'):
            if path.name!='Markdown 阅读示例.md':path.unlink()
        for path in (CARD/'markdown/.index').glob('*.prefs'):path.unlink()

    def test_all_catalog_frames(self):
        for view in VIEWS.values():
            with self.subTest(view=view['id']):
                path = '/sdcard/books/每日笔记.epub' if view['native'].startswith('book-') else 'tools/simulator/fixtures/阅读示例.md'
                result = subprocess.run([str(BUILD/'render'),view['native'],path,
                    str(view['aux']),'48',str(BUILD/'test.bin'),str(BUILD/'titles.txt'),
                    str(view['mask']),'0,900,1800,3600,5400,7200,2700','builtin',''],
                    cwd=ROOT, capture_output=True,text=True,timeout=30)
                self.assertEqual(result.returncode,0,result.stderr)
                self.assertNotIn('runtime error:',result.stderr)
                self.assertNotIn('ERROR: AddressSanitizer',result.stderr)
                data=(BUILD/'test.bin').read_bytes()
                self.assertEqual(len(data),684*1216//2)
                self.assertNotEqual(data,bytes([255])*len(data))

    def test_reader_replay_and_tools(self):
        p=self.preview();self.view(p,'reader')
        self.assertGreater(p.state['pages'],1)
        self.key(p,2);self.assertEqual(p.state['page'],1)
        p.render();self.assertEqual(p.state['page'],1)
        self.assertEqual(self.key(p,1)['screen'],'reader-tools')
        self.assertEqual(self.tap(p,610,1140)['screen'],'reader-font')
        self.view(p,'reader-tools')
        self.assertEqual(self.tap(p,60,1140)['screen'],'reader-toc')

    def test_card_fonts_load_and_persist_separately(self):
        # Small licensed fixtures keep the check reproducible without user fonts.
        system_name='000-SimulatorSystem.ttf'
        reading_name='001-SimulatorReading.ttf'
        for name in (system_name,reading_name):
            target=CARD/'fonts'/name
            self.assertFalse(target.exists())
            shutil.copyfile(ROOT/'main/assets/builtin.ttf',target)
        p=self.preview()
        self.view(p,'settings-5')
        self.tap(p,250,422)  # First card font; row zero is the builtin choice.
        system_path='/sdcard/fonts/'+system_name
        self.assertEqual(p.state['system_font_path'],system_path)
        self.assertEqual(p.state['font_path'],system_path)
        self.assertFalse(p.state['font_builtin'])
        self.view(p,'reader-font-picker')
        self.tap(p,330,766)  # Second card font in the reading picker.
        reading_path='/sdcard/fonts/'+reading_name
        self.assertEqual(p.state['reading_font_path'],reading_path)
        self.assertEqual(p.state['font_path'],reading_path)
        self.assertFalse(p.state['font_builtin'])
        self.assertEqual(p.state['system_font_path'],system_path)
        self.key(p,2)
        p.render()
        self.assertEqual(p.state['font_path'],reading_path)
        self.view(p,'baseline-home')
        self.assertEqual(p.state['font_path'],system_path)
        restarted=self.preview()
        self.assertEqual(restarted.state['font_path'],system_path)
        self.view(restarted,'reader')
        self.assertEqual(restarted.state['font_path'],reading_path)

    def test_default_font_covers_dynamic_titles_and_common_fallback(self):
        p=self.preview();self.view(p,'settings-5');self.tap(p,250,330)
        self.assertEqual(p.state['font_path'],'/sdcard/fonts/PicoSans-Medium.ttf')
        self.assertEqual(p.state['system_font_path'],'')
        def probe(title):
            result=subprocess.run([str(BUILD/'render'),'font-probe',title,'0','48',str(BUILD/'font-probe.bin'),str(BUILD/'titles.txt'),'0','0,0,0,0,0,0,0','builtin'],cwd=ROOT,capture_output=True,text=True,timeout=30)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertNotIn('runtime error:',result.stderr)
            return json.loads(result.stdout)
        rare=probe('罄龘閱覽 “中文”')
        self.assertFalse(rare['builtin_text_covered'])
        self.assertTrue(rare['font_text_covered'])
        standard=CARD/'fonts/PicoSans-Medium.ttf'
        standard.rename(CARD/'fonts/standard-hidden.bin')
        common=probe('2001：太空漫游 雪崩 全球智能危机 ‘阅读’')
        self.assertTrue(common['builtin_text_covered'])

    def test_file_open_and_copy_replay(self):
        p=self.preview();self.view(p,'files-books')
        self.assertEqual(self.tap(p,260,260)['screen'],'reader')
        self.assertTrue(p.state['firmware_path'].endswith('.epub'))
        before={f.name:f.read_bytes() for f in (CARD/'books').iterdir() if f.is_file()}
        self.view(p,'files-home')
        self.assertEqual(self.tap(p,589,120)['screen'],'files-home')  # Removed BOOT hit area.
        self.assertEqual(self.tap(p,200,485)['screen'],'files-card')
        self.assertEqual(p.state['files_dir'],'/sdcard/books')
        self.assertEqual(self.tap(p,60,113)['screen'],'files-card')
        self.assertEqual(p.state['files_dir'],'/sdcard')
        self.assertEqual(self.tap(p,60,113)['screen'],'files-home')
        p.action({'action':'gesture','type':'l','x':200,'y':485,'xx':200,'yy':485})
        self.assertEqual(p.state['screen'],'files-actions')
        self.tap(p,342,980)  # Cancel the root-folder operation menu.
        self.view(p,'files-actions');self.tap(p,500,700)
        after={f.name:f.read_bytes() for f in (CARD/'books').iterdir() if f.is_file()}
        self.assertEqual(len(after),len(before)+1)
        new=set(after)-set(before)
        self.assertIn(after[new.pop()],before.values())
        p.render()
        self.assertEqual(after,{f.name:f.read_bytes() for f in (CARD/'books').iterdir() if f.is_file()})

    def test_markdown_content_outline_and_long_image(self):
        self.sample_documents_only()
        p=self.preview();self.view(p,'book-0')
        self.assertEqual(self.tap(p,250,120)['screen'],'documents')
        self.assertEqual(self.tap(p,150,260)['screen'],'reader')
        self.assertTrue(p.state['firmware_path'].endswith('Markdown 阅读示例.md'))
        self.assertEqual(self.key(p,1)['screen'],'reader-tools')
        self.assertEqual(self.tap(p,50,1140)['screen'],'reader-toc')
        self.assertEqual(p.state['toc_count'],2)
        self.tap(p,60,280);self.assertEqual(p.state['toc_count'],4)
        self.tap(p,84,390);self.assertEqual(p.state['toc_count'],5)
        self.assertEqual(self.tap(p,210,400)['screen'],'reader')
        self.key(p,1);self.assertEqual(self.tap(p,610,1140)['screen'],'reader-font')
        self.view(p,'reader')
        for _ in range(p.state['pages']):
            if p.state['document_image_ready']:break
            self.key(p,2)
        self.assertTrue(p.state['document_image_ready'])
        self.tap(p,342,630);self.assertTrue(p.state['document_zoom'])
        p.action({'action':'gesture','type':'p','x':342,'y':1000,'xx':342,'yy':1000})
        p.action({'action':'gesture','type':'m','x':342,'y':1000,'xx':342,'yy':400})
        self.assertEqual(p.state['image_top'],600)
        p.action({'action':'gesture','type':'u','x':342,'y':1000,'xx':342,'yy':400})
        self.assertEqual(p.state['image_top'],600)
        self.tap(p,342,630);self.assertFalse(p.state['document_zoom'])
        self.tap(p,55,110);self.assertEqual(p.state['screen'],'documents')

    def test_delete_last_document_cleans_assets_and_stays_deleted(self):
        self.sample_documents_only()
        p=self.preview();self.view(p,'documents')
        p.action({'action':'gesture','type':'l','x':150,'y':260,'xx':150,'yy':260})
        self.tap(p,342,621)
        self.assertTrue(p.state['document_delete_confirm'])
        self.tap(p,488,665)
        self.assertEqual(p.state['screen'],'documents')
        self.assertFalse((CARD/'markdown/Markdown 阅读示例.md').exists())
        self.assertFalse((CARD/'markdown/attachments/长图示例-test.png').exists())
        self.assertEqual(p.state['pages'],1)
        self.preview()  # The demo is not silently regenerated after deletion.
        self.assertFalse((CARD/'markdown/Markdown 阅读示例.md').exists())

    def test_document_pin_star_filter_and_revision_persistence(self):
        self.sample_documents_only()
        other=CARD/'markdown/AAA-unittest.md';other.write_text('# A\n\nBody')
        sample='/sdcard/markdown/Markdown 阅读示例.md'
        p=self.preview();self.view(p,'documents')
        self.assertTrue(p.state['document_first'].endswith('AAA-unittest.md'))
        p.action({'action':'gesture','type':'l','x':150,'y':351,'xx':150,'yy':351})
        self.assertTrue(p.state['document_menu'])
        self.tap(p,198,514)
        self.assertEqual(p.state['document_first'],sample)
        p.action({'action':'gesture','type':'l','x':150,'y':260,'xx':150,'yy':260})
        self.tap(p,486,514);self.tap(p,588,122)
        self.assertTrue(p.state['document_starred_only'])
        self.assertEqual(p.state['document_count'],1)
        self.assertEqual(p.state['document_first'],sample)
        meta=CARD/'markdown/.index/Markdown 阅读示例.md.pico'
        original_date=meta.read_text().splitlines()[1]
        source=CARD/'markdown/Markdown 阅读示例.md';source.write_text(source.read_text()+'\n\nUpdated.')
        p.action({'action':'reload-card'});self.view(p,'documents')
        self.assertEqual(p.state['document_first'],sample)
        self.assertEqual(meta.read_text().splitlines()[1],original_date)
        self.tap(p,588,122);self.assertEqual(p.state['document_count'],1)
        p.action({'action':'gesture','type':'l','x':150,'y':260,'xx':150,'yy':260})
        self.tap(p,486,514)
        self.assertEqual(p.state['document_count'],0)  # Unstar in the active filter.
        self.tap(p,476,122);self.assertEqual(p.state['document_count'],2)
        self.assertEqual(p.state['document_first'],sample)

    def test_markdown_upload_uses_tf_and_preserves_first_date(self):
        p=self.preview()
        p.upload({'name':'unittest-import.md','content':'# First\n\nBody'})
        self.assertEqual(p.state['screen'],'reader')
        self.assertEqual(p.state['reader_progress'],0)
        self.key(p,2);self.assertEqual(p.state['reader_progress'],100)
        self.assertEqual(p.state['firmware_path'],'/sdcard/markdown/unittest-import.md')
        metadata=CARD/'markdown/.index/unittest-import.md.pico'
        original_date=metadata.read_text().splitlines()[1]
        self.assertEqual((CARD/'markdown/unittest-import.md').read_text(),'# First\n\nBody')
        p.upload({'name':'unittest-import.md','content':'# Other\n\nBody'})
        self.assertEqual(metadata.read_text().splitlines()[1],original_date)
        with self.assertRaisesRegex(ValueError,'关联插图'):
            p.upload({'name':'unittest-import.md','content':'![x](attachments/missing.png)'})
        self.assertEqual((CARD/'markdown/unittest-import.md').read_text(),'# Other\n\nBody')

    def test_external_card_change_preserved(self):
        empty=CARD/'notes';empty.mkdir(exist_ok=True)
        p=self.preview();self.view(p,'reader');self.key(p,2)
        p.render();self.assertTrue(empty.is_dir())
        external=CARD/'books/external.txt';external.write_text('external test document')
        with self.assertRaisesRegex(ValueError,'外部修改'):
            p.render()
        self.assertEqual(external.read_text(),'external test document')
        p.action({'action':'reload-card'})
        self.assertEqual(external.read_text(),'external test document')

    def test_transfer_and_images(self):
        p=self.preview();self.view(p,'transfer-methods')
        self.assertEqual(self.tap(p,260,282)['screen'],'transfer-networks')
        self.assertEqual(self.tap(p,280,435)['screen'],'transfer-password')
        self.view(p,'files-home')
        self.assertEqual(self.tap(p,330,320)['screen'],'transfer-hotspot')
        self.view(p,'files-home')
        self.assertEqual(self.tap(p,540,320)['screen'],'transfer-usb')
        self.view(p,'image-list')
        self.assertEqual(self.tap(p,260,255)['screen'],'image-view')
        self.assertEqual(self.tap(p,320,600)['screen'],'image-list')

    def test_cloud_home_settings_and_single_selection_without_replay(self):
        p=self.preview();self.view(p,'baseline-home');self.tap(p,342,929)
        self.assertEqual(p.state['screen'],'cloud-sync');self.assertFalse(p.cloud.snapshot()['configured'])
        self.tap(p,342,540);self.assertEqual(p.state['screen'],'settings-18')
        self.view(p,'cloud-sync');p.cloud.entries=[{'href':'fixture.md','name':'测试文档.md','filename':'测试文档.md','bytes':42,'date_ms':1791248523000,'date_fallback':False,'selectable':True,'selected':False,'error':''}];p.cloud.update(message='测试目录');p.snapshot()
        p.capture_origin();proof=CARD/'notes/cloud-external-proof.txt';proof.write_text('completed download must survive');p.render()
        self.tap(p,606,287);self.assertTrue(p.cloud.entries[0]['selected']);self.assertTrue(proof.exists())
        p.render();p.snapshot();self.assertTrue(p.cloud.entries[0]['selected'])
        p.cloud.update(busy=True);p.snapshot();self.tap(p,342,1024)
        self.assertTrue(p.cloud.cancel.is_set());self.assertEqual(p.state['screen'],'cloud-sync')
        self.tap(p,610,1150);self.assertEqual(p.state['screen'],'cloud-sync')
        p.cloud.update(busy=False);p.snapshot();self.assertEqual(p.state['screen'],'settings-0')

    def test_settings_and_power_navigation(self):
        p=self.preview();self.view(p,'settings-0')
        self.assertEqual(self.tap(p,260,253)['screen'],'transfer-networks')
        self.view(p,'settings-0')
        for _ in range(2):
            p.action({'action':'gesture','type':'u','x':342,'y':1000,'xx':342,'yy':400})
        self.assertEqual(self.tap(p,260,1010)['screen'],'settings-17')
        self.assertEqual(self.tap(p,60,113)['screen'],'settings-0')
        self.assertEqual(self.tap(p,600,1140)['screen'],'settings-0')
        self.assertEqual(self.tap(p,420,1140)['screen'],'files-home')
        self.view(p,'reader');self.view(p,'power')
        self.assertEqual(self.tap(p,10,300)['screen'],'reader')
        self.view(p,'power')
        self.assertEqual(self.tap(p,460,566)['screen'],'shutdown')
        self.view(p,'menu')
        self.assertEqual(self.tap(p,200,560)['screen'],'settings-0')


if __name__=='__main__':
    unittest.main()
