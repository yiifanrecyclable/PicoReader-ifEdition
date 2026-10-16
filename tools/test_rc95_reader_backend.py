#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""隔离卡上的真实EPUB索引冷读/热读/损坏恢复与固定根目录。/ Native EPUB cache recovery and fixed media roots on an isolated card."""
from pathlib import Path
import tempfile,shutil,subprocess,json,os
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='rc95-reader-',dir=ROOT/'build') as folder:
    base=Path(folder);card=base/'card';(card/'books').mkdir(parents=True)
    nvs=base/'build/simulator/nvs/read_pico';nvs.mkdir(parents=True)
    (nvs/'books_dir').write_bytes(b'/sdcard/legacy-books\0')
    (nvs/'fonts_dir').write_bytes(b'/sdcard/legacy-fonts\0')
    (card/'legacy-books').mkdir();(card/'legacy-books/keep.txt').write_text('keep')
    (base/'titles.txt').write_text('')
    for f in ('河边记事_ncx.epub','河边记事_nav.epub'):
        shutil.copyfile(ROOT/'build/book-fixtures/books'/f,card/'books'/f)
    env={**os.environ,'PICO_SIM_CARD_ROOT':str(card)}
    def frame(name,page=1):
        args=[str(ROOT/'build/simulator/render'),'book-1',f'/sdcard/books/{name}',str(page),'48','build/simulator/frame.bin','titles.txt','0',','.join(['0']*30),'builtin','','']
        result=subprocess.run(args,cwd=base,env=env,capture_output=True,text=True,timeout=30)
        assert result.returncode==0,result.stderr[-2000:]
        return json.loads(result.stdout)
    for name in ('河边记事_ncx.epub','河边记事_nav.epub'):
        cold=frame(name);warm=frame(name)
        assert cold['pages']==warm['pages'] and cold['reader_offset']==warm['reader_offset']
        assert cold['books_dir']=='/sdcard/books' and cold['fonts_dir']=='/sdcard/fonts'
    indexes=list((card/'.readpico/index').glob('*-epub.bin'))
    assert indexes,'No real native index cache was written'
    for cache in indexes:
        content=cache.read_bytes();assert len(content)>50
        cache.write_bytes(content[:-9])
    for name in ('河边记事_ncx.epub','河边记事_nav.epub'):
        recovered=frame(name);assert recovered['pages']>1 and recovered['reader_offset']>0
    # 根目录旧设置不再把书籍导向另一目录，且不移动/删除旧文件。
    # Legacy root preferences cannot reroute media or move/delete legacy files.
    assert (card/'legacy-books/keep.txt').read_text()=='keep'
print('rc95 backend: real NCX/NAV cache cold/warm/corruption recovery and legacy-root preservation passed')
