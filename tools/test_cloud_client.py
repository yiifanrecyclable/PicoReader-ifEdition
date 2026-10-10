# SPDX-License-Identifier: Apache-2.0
"""Loopback-only DAV fixtures; never read a real Keychain item or Nutstore account.
Production C validates every listing/manifest and performs local commits.
"""
# 旧入口依赖日常存储回放，停用以免测试覆写用户资料。
# Retired: replaying daily storage may overwrite user data; use isolated replacements.
raise RuntimeError("Legacy storage-replay test retired. Use tools/test_cloud_host.py, tools/test_cloud_etag.py and tools/test_if_simulator.py with isolated storage.")

from pathlib import Path
import base64,copy,hashlib,json,os,shutil,subprocess,sys,tempfile,threading,time,unittest,urllib.parse
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
sys.path.insert(0,str(Path(__file__).resolve().parent/'simulator'))
from server import ROOT,BUILD,CARD,STORES
from cloud_client import DavClient,CloudError,Cancelled,CloudController,NativeBridge,PROPFIND
class MockKeychain:
    values={}
    def save(self,account,password):self.values[account]=password
    def get(self,account):return self.values.get(account)
    def forget(self,account):self.values.pop(account,None)
class Handler(BaseHTTPRequestHandler):
    def log_message(self,*args):pass
    def do_PROPFIND(self):
        self.server.methods.append(self.command);self.rfile.read(int(self.headers.get('Content-Length',0)))
        root='/dav/PicoBox/';rows=[f'<d:response><d:href>{root}</d:href><d:propstat><d:prop><d:resourcetype><d:collection/></d:resourcetype></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>']
        for path,(body,etag) in self.server.files.items():
            if '/' in path:continue
            href=root+urllib.parse.quote(path,safe='');rows.append(f'<d:response><d:href>{href}</d:href><d:propstat><d:prop><d:getetag>{etag.replace(chr(34),"&quot;")}</d:getetag><d:getcontentlength>{len(body)}</d:getcontentlength><d:creationdate>2026-10-06T01:02:03Z</d:creationdate><d:getlastmodified>Tue, 06 Oct 2026 01:02:03 GMT</d:getlastmodified><d:resourcetype/></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response>')
        data=('<d:multistatus xmlns:d="DAV:">'+''.join(rows)+'</d:multistatus>').encode();self.send_response(207);self.send_header('Content-Length',len(data));self.end_headers();self.wfile.write(data)
    def do_GET(self):
        self.server.methods.append(self.command);
        if self.server.auth_failure:self.send_error(401);return
        path=urllib.parse.unquote(urllib.parse.urlsplit(self.path).path).removeprefix('/dav/PicoBox/')
        if self.server.retry:
            self.server.retry-=1;self.send_response(429);self.send_header('Retry-After','0');self.send_header('Content-Length','0');self.end_headers();return
        if path not in self.server.files:self.send_error(404);return
        body,etag=self.server.files[path]
        if self.headers.get('If-Match') not in (None,etag):self.send_error(412);return
        self.send_response(200);self.send_header('ETag',etag);self.send_header('Content-Length',len(body));self.end_headers();self.wfile.write(self.server.corrupt.get(path,body))
class CloudClientTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.saved=tempfile.TemporaryDirectory(dir=BUILD,prefix='cloud-network-backup-')
        for key,folder in STORES.items():
            if folder.exists():shutil.copytree(folder,Path(cls.saved.name)/key)
    @classmethod
    def restore(cls):
        for key,folder in STORES.items():
            if folder.exists():shutil.rmtree(folder)
            if (Path(cls.saved.name)/key).exists():shutil.copytree(Path(cls.saved.name)/key,folder)
    @classmethod
    def tearDownClass(cls):cls.restore();cls.saved.cleanup()
    def setUp(self):
        self.http=ThreadingHTTPServer(('127.0.0.1',0),Handler);self.http.files={};self.http.methods=[];self.http.retry=0;self.http.corrupt={};self.http.auth_failure=False
        self.thread=threading.Thread(target=self.http.serve_forever,daemon=True);self.thread.start();self.base=f'http://127.0.0.1:{self.http.server_port}/dav/PicoBox/'
        self.client=DavClient({'account':'fixture@example.test','folder':'PicoBox'},'synthetic-app-password',test_base=self.base)
    def tearDown(self):self.client.close();self.http.shutdown();self.http.server_close();self.thread.join();self.restore()
    def fixture(self,images=False):
        source=ROOT/'plugins/obsidian-pico/tests/fixtures';name='sample-image-manifest.json' if images else 'sample-manifest.json';m=json.loads((source/name).read_text());m['source_filename']='测试云文档.md';m['title']='显示标题不同于原文件名';href=m['article_id']+'.pico.json';self.http.files[href]=(json.dumps(m,ensure_ascii=False).encode(),'"manifest-v1"');self.http.files[m['body']['path']]=((source/('sample-image-body.md' if images else 'sample-body.md')).read_bytes(),'"body-v1"')
        for asset in m['assets']:self.http.files[asset['path']]=((source/'sample-image.png').read_bytes(),'"image-v1"')
        return m,href
    def test_manifest_and_raw_files_are_listed_without_writes(self):
        self.fixture();self.http.files['中文 #%.md']=(b'# plain\n','"raw"');self.http.files['unsupported.pdf']=(b'%PDF','"pdf"')
        entries=self.client.listing();self.assertEqual({e['filename'] for e in entries},{'测试云文档.md','中文 #%.md'});self.assertTrue(all(e['selectable'] for e in entries));self.assertTrue(set(self.http.methods)<= {'GET','PROPFIND'})
    def test_v2_bundle_is_downloaded_checked_and_committed(self):
        manifest,href=self.fixture(True);entry=self.client.listing()[0];self.client.download_one(entry)
        self.assertEqual((CARD/'markdown/测试云文档.md').read_bytes(),self.http.files[manifest['body']['path']][0])
        for asset in manifest['assets']:self.assertEqual((CARD/'markdown/attachments'/asset['path'].rsplit('/',1)[-1]).read_bytes(),self.http.files[asset['path']][0])
        self.assertFalse((CARD/'.pico-sync/transaction').exists());self.assertTrue((CARD/'markdown/.index/测试云文档.md.sync').exists())
    def test_corrupt_body_keeps_prior_document(self):
        manifest,href=self.fixture();old=CARD/'markdown/测试云文档.md';old.write_bytes(b'# previous\n');NativeBridge().require('refresh','/sdcard/markdown/测试云文档.md')
        entry=self.client.listing()[0];self.http.corrupt[manifest['body']['path']]=b'# wrong\n'
        with self.assertRaises(CloudError):self.client.download_one(entry)
        self.assertEqual(old.read_bytes(),b'# previous\n')
    def test_plain_markdown_percent_hash_filename_roundtrips(self):
        self.http.files['中文 #%.md']=(b'# plain\n','"raw-v1"');entry=self.client.listing()[0];self.client.download_one(entry);self.assertEqual((CARD/'markdown/中文 #%.md').read_bytes(),b'# plain\n')
    def test_midflight_changed_manifest_is_not_silently_substituted(self):
        manifest,href=self.fixture();entry=self.client.listing()[0];data,_=self.http.files[href];self.http.files[href]=(data,'"manifest-v2"')
        with self.assertRaises(CloudError):self.client.download_one(entry)
        self.assertFalse((CARD/'markdown/测试云文档.md').exists())
    def test_retry_and_cancel_are_bounded(self):
        self.fixture();self.http.retry=2;entries=self.client.listing();self.assertEqual(len(entries),1);self.client.cancel.set()
        with self.assertRaises(Cancelled):self.client.download_one(entries[0])
    def test_config_has_no_password_and_duplicate_names_are_rejected(self):
        with tempfile.TemporaryDirectory(dir=BUILD) as temp:
            controller=CloudController(keychain_factory=MockKeychain,private_dir=Path(temp))
            snapshot=controller.configure({'account':'fixture@example.test','folder':'可自定义目录','password':'synthetic-app-password'})
            self.assertEqual(snapshot['folder'],'可自定义目录');self.assertNotIn('password',json.loads(controller.config_file.read_text()));self.assertNotIn('synthetic-app-password',json.dumps(snapshot))
            controller.entries=[{'filename':'Plan.md','selectable':True,'selected':True},{'filename':'plan.md','selectable':True,'selected':True}]
            with self.assertRaises(CloudError):controller.start(download=True)
    def test_dtd_and_entity_listing_is_rejected(self):
        file=BUILD/'bad-dav.xml';file.write_text('<!DOCTYPE x [<!ENTITY a "data">]><d:multistatus xmlns:d="DAV:">&a;</d:multistatus>')
        result=NativeBridge().call('listing',file,'/dav/PicoBox/');self.assertNotEqual(result['error'],0)
    def test_auth_failure_stops_listing_and_entire_selected_batch(self):
        self.fixture();self.http.auth_failure=True
        with self.assertRaises(CloudError) as caught:self.client.listing()
        self.assertTrue(caught.exception.fatal);self.assertEqual(self.http.methods.count('GET'),1)
        self.http.auth_failure=False;self.http.files.clear();self.http.files.update({'first.md':(b'# first\n','"one"'),'second.md':(b'# second\n','"two"')});entries=self.client.listing()
        for entry in entries:entry['selected']=True
        self.http.auth_failure=True;self.http.methods.clear()
        with tempfile.TemporaryDirectory(dir=BUILD) as temp:
            MockKeychain.values['fixture@example.test']='synthetic-app-password'
            controller=CloudController(keychain_factory=MockKeychain,private_dir=Path(temp),client_factory=lambda *a,**kw:DavClient(*a,**kw,test_base=self.base))
            controller.config={'account':'fixture@example.test','folder':'PicoBox','configured':True};controller.entries=entries;controller.worker(True,entries)
            self.assertFalse(controller.snapshot()['busy']);self.assertEqual(controller.snapshot()['failed'],1);self.assertEqual(self.http.methods.count('GET'),1)
    def test_manifest_without_etag_still_pins_selected_revision(self):
        manifest,href=self.fixture();body,_=self.http.files[href];self.http.files[href]=(body,'');entry=self.client.listing()[0]
        source=b'# revised\n';revision=hashlib.sha256(source).hexdigest();manifest['revision_id']=revision;manifest['body'].update(sha256=revision,bytes=len(source),path=f'_pico/{manifest["article_id"]}/{revision}/body.md')
        self.http.files[href]=(json.dumps(manifest).encode(),'');self.http.files[manifest['body']['path']]=(source,'"revision"')
        with self.assertRaises(CloudError):self.client.download_one(entry)
        self.assertFalse((CARD/'markdown/测试云文档.md').exists())
    def test_txt_and_epub_follow_configured_tf_books_directory(self):
        directory=CARD/'notes';directory.mkdir(exist_ok=True)
        nvs=BUILD/'nvs/read_pico';nvs.mkdir(parents=True,exist_ok=True);(nvs/'books_dir').write_bytes(b'/sdcard/notes\0')
        self.http.files={'云端图书.epub':((CARD/'books/每日笔记.epub').read_bytes(),'"epub"'),'云端纯文本.txt':('中文TXT测试\n'.encode(),'"txt"')}
        for entry in self.client.listing():self.client.download_one(entry)
        for name,(content,_) in self.http.files.items():self.assertEqual((directory/name).read_bytes(),content)
if __name__=='__main__':unittest.main()
