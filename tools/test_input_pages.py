# SPDX-License-Identifier: Apache-2.0
"""Native entry/commit/cancel and keyboard pixels using an isolated card/NVS only."""
from pathlib import Path
import json, os, shutil, subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
def tap(x,y): return f't,{x},{y},{x},{y}'
def events(*steps): return ';'.join(steps)
clear=tap(573,400);language=tap(89,1152);done=tap(595,1152);cancel=tap(60,110)
# Explicit row/column calculation avoids ambiguous precedence.
def letters(text):
    keys="1234567890qwertyuiopasdfghjkl'zxcvbnm,./"
    return events(*(tap(67+61*(keys.index(c)%10),803+84*(keys.index(c)//10)) for c in text))
with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='input-pages-') as tmp:
    work=Path(tmp);card=work/'card';build=work/'build/simulator';build.mkdir(parents=True)
    for folder in ('books','markdown/attachments','fonts','system/ime'):(card/folder).mkdir(parents=True)
    (card/'books/Test.txt').write_text('Chapter\n\nText body.\n')
    (card/'markdown/Test.md').write_text('# 标题\n\n正文\n')
    shutil.copyfile(ROOT/'assets/fonts/PicoSans-Medium.ttf',card/'fonts/PicoSans-Medium.ttf')
    shutil.copyfile(ROOT/'assets/ime/pinyin.ifdict',card/'system/ime/pinyin.ifdict')
    (build/'titles.txt').write_text('');env={**os.environ,'PICO_SIM_CARD_ROOT':str(card)}
    def render(native,steps='',aux=0,path='/sdcard/books/Test.txt'):
        args=[str(ROOT/'build/simulator/render'),native,path,str(aux),'48','build/simulator/frame.bin','build/simulator/titles.txt','0',','.join(['60']*30),'builtin',steps,'']
        result=subprocess.run(args,cwd=work,env=env,text=True,capture_output=True,timeout=30)
        assert result.returncode==0,(native,result.stderr)
        return json.loads(result.stdout),(build/'frame.bin').read_bytes()
    # Every page uses byte-identical key rows; captions/candidates and commit labels may vary.
    entries=[('settings-19','',0),('settings-23','',0),('settings-0',tap(340,675),0),('settings-20',tap(340,254),0),('settings-20',tap(340,354),0),('settings-20',tap(340,454),0),('files-2','',0),('book-7','',0),('book-6','',0),('transfer-3','',0)]
    crop=None
    for native,steps,aux in entries:
        info,frame=render(native,steps,aux)
        keys=frame[766*342:1092*342]
        if crop is None:crop=keys
        assert keys==crop,('keyboard rows moved or changed',native)
        # Old app navigation is replaced by five keyboard controls, extending to y1200.
        assert frame[1152*342:1200*342]!=bytes([255])*(48*342)
    # Cancel edits leave device settings untouched; completed edits survive a new process.
    before={p.relative_to(build).as_posix():p.read_bytes() for p in (build/'nvs').rglob('*') if p.is_file()}
    info,_=render('settings-19',events(clear,language,letters('abc'),cancel));assert info['settings_page']==0
    after={p.relative_to(build).as_posix():p.read_bytes() for p in (build/'nvs').rglob('*') if p.is_file()}
    assert before==after
    info,_=render('settings-19',events(clear,language,letters('abc'),done));assert info['settings_page']==0
    values=[p for p in (build/'nvs').rglob('status_sig') if not p.parent.name.startswith('.')];assert len(values)==1 and values[0].read_bytes()==b'abc\0',[(p.relative_to(build).as_posix(),p.read_bytes()) for p in values]
    render('settings-19');assert values[0].read_bytes()==b'abc\0'
    # Exact full-pinyin phrase selection is available through the native signature page.
    info,_=render('settings-0',events(tap(340,675),clear,letters('yuedu'),tap(136,718),done));assert info['settings_page']==0
    values=[p for p in (build/'nvs').rglob('status_sig') if not p.parent.name.startswith('.')];assert len(values)==1 and values[0].read_bytes()=='阅读\0'.encode()
    # Rename preserves the locked extension; invalid names remain editable and retry succeeds.
    info,_=render('files-2',events(clear,language,letters('abc'),tap(188,1152),tap(250,1055),done));assert info['files_view']==2 and (card/'books/Test.txt').exists()
    info,_=render('files-2',events(clear,language,letters('abc'),done));assert info['files_view']==1
    assert (card/'books/abc.txt').exists() and not (card/'books/Test.txt').exists()
    info,_=render('files-2',events(clear,language,letters('cancelled'),cancel));assert info['files_view']==1
    assert (card/'books/abc.txt').exists() and not (card/'books/cancelled.txt').exists()
    # Search supports both direct Latin spelling and Chinese candidates, and returns to its own page.
    info,_=render('book-4',events(tap(550,251),letters('abc'),done));assert info['book_view']==4 and len([x for x in info['book_items'] if x])==1
    info,_=render('book-9',events(tap(550,251),letters('test'),cancel));assert info['book_view']==9
    info,_=render('transfer-3',events(letters('short'),done));assert info['transfer_view']==3
    info,_=render('transfer-3',events(letters('dummy'),cancel));assert info['transfer_view']==2
    info,_=render('settings-20',events(tap(340,454),letters('dummy'),cancel));assert info['settings_page']==20
    # 首页格言共用输入、取消不改数据；保存及留空均持久化并返回首页。
    # Home quotes share input; cancel preserves data, save/blank persist and return home.
    assert render('settings-21',tap(340,608))[0]['settings_page']==23
    assert render('settings-21',tap(340,698))[0]['settings_page']==22
    font=build/'nvs/if_content/quote_font'
    assert render('settings-22',tap(340,423))[0]['settings_page']==21
    assert font.read_bytes()==b'/sdcard/fonts/PicoSans-Medium.ttf\0'
    assert render('settings-22',tap(340,330))[0]['settings_page']==21 and font.read_bytes()==b'\0'
    quote=build/'nvs/if_content/home_quote'
    info,_=render('settings-23',events(clear,language,letters('abc'),cancel));assert info['settings_page']==21 and not quote.exists()
    info,_=render('settings-23',events(clear,language,letters('abc'),done));assert info['settings_page']==21 and quote.read_bytes()==b'abc\0'
    info,_=render('settings-23',events(clear,done));assert info['settings_page']==21 and quote.read_bytes()==b'\0'
    # 超出实际字体的两行限制时不提交，保留输入，缩短后仍能保存。
    # Reject over-two-row text without committing; retain the editor and allow a shorter retry.
    info,_=render('settings-23',events(clear,language,letters('w'*50),done));assert info['route']==-1 and info['settings_page']==23 and quote.read_bytes()==b'\0'
    info,_=render('settings-23',events(clear,language,letters('ok'),done));assert info['settings_page']==21 and quote.read_bytes()==b'ok\0'
    print('PASS: 10 native input entries have identical bottom key rows; UTF-8 phrases, settings persistence, cancellation, rename validation, search and WiFi validation verified on a private card.')
