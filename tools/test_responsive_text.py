# SPDX-License-Identifier: Apache-2.0
"""Production geometry, paging and clicks across all UI size presets; private TF/NVS. / 四档实际排版、分页与点击，隔离TF/NVS。"""
from pathlib import Path
import json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def tap(x,y):return f't,{x},{y},{x},{y}'
with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='responsive-ui-') as tmp:
    work=Path(tmp);card=work/'card';out=work/'build/simulator';out.mkdir(parents=True)
    for folder in ('books','markdown','fonts','font','pictures'):(card/folder).mkdir(parents=True)
    for i in range(24):
        (card/f'markdown/文档{i:02}.md').write_text('# 标题\n\n'+'正文内容。\n'*120)
        (card/f'books/图书{i:02}.txt').write_text('Chapter\n\n'+'正文 English text.\n'*100)
    shutil.copy2(ROOT/'assets/fonts/PicoSans-Medium.ttf',card/'fonts/PicoSans-Medium.ttf')
    moon=ROOT.parent/'TF卡/fonts/MoonStarsKai-Regular.ttf'
    if moon.is_file():shutil.copy2(moon,card/'fonts/MoonStarsKai-Regular.ttf')
    (out/'titles.txt').write_text('');env={**os.environ,'PICO_SIM_CARD_ROOT':str(card),'PICO_SIM_HOME_DAY':'20737'}
    def render(view,events='',aux=0,path='/sdcard/books/图书00.txt'):
        p=subprocess.run([str(ROOT/'build/simulator/render'),view,path,str(aux),'48','build/simulator/frame.bin','build/simulator/titles.txt','0',','.join(['60']*30),'builtin',events,''],cwd=work,env=env,text=True,capture_output=True,timeout=30)
        assert p.returncode==0,(view,p.stderr)
        return json.loads(p.stdout),(out/'frame.bin').read_bytes()
    # Seed real last-read records, giving all four recent documents and one book.
    for path in ['/sdcard/books/图书00.txt']+[f'/sdcard/markdown/文档{i:02}.md' for i in range(4)]:
        render('book-1',path=path)
    sizes=(100,120,140,160)
    for face in ('', '/sdcard/fonts/MoonStarsKai-Regular.ttf') if moon.is_file() else ('',):
        # Use native UI to choose a face; ordering is MoonStarsKai then PicoSans.
        info,_=render('settings-5');top,step=info['settings_geometry'][6:8]
        render('settings-5',tap(300,top+(step if face else 0)+(step-12)//2))
        heights=[];body_sizes=[];reader=None;banner=None;capsule=None
        for i,size in enumerate(sizes):
            state,frame=render('settings-5',tap(108+i*156,300))
            assert state['system_size']==size and state['settings_page']==5
            # Square corners of a circle must not select an adjacent preset.
            assert render('settings-5',tap(58,250))[0]['system_size']==size
            info,frame=render('settings-0');g=info['settings_geometry'];heights.append(g[4]);body_sizes.append(info['ui_body_px'])
            assert render('settings-0',tap(300,g[1]+g[4]//2))[0]['settings_page']==5
            for row,page in enumerate((2,14,24,11,27,13)):
                target=g[2]+row*g[4]+g[4]//2;scroll=min(g[5],max(0,target-970))
                assert render('settings-0',tap(300,target-scroll),scroll)[0]['settings_page']==page
            info,frame=render('book-8');rows=info['document_rows'];step=info['document_row_step']
            assert info['pages']==(24+rows-1)//rows
            current=frame[84*342:161*342]
            # Only the capsule region is fixed; management/sort text is scalable.
            current=b''.join(frame[y*342+18:y*342+170] for y in range(84,161))
            if capsule is None:capsule=current
            else:assert current==capsule,'content capsule must remain fixed'
            render('book-8',tap(200,224+(rows-1)*step+(step-10)//2))
            info,_=render('book-8','u,300,800,300,500')
            assert info['reader_page']==1
            info,_=render('book-8',aux=1);expected=info['document_page_first']
            info,_=render('book-8',tap(200,224+(step-10)//2),1);assert Path(info['book_path']).name==Path(expected).name,(size,rows,step,expected,info['book_path'],info['reader_page'],info['book_view'])
            info,frame=render('baseline-home');home=info['home_geometry']
            assert home[2]==(3 if size==160 else 4)
            assert home[0]>home[10] and home[0]+home[1]*home[2]<=1120
            assert home[4]-22>=36 and home[4]+6*home[5]+22<=648
            current=frame[80*342:380*342]+b''.join(frame[y*342+170:(y+1)*342] for y in range(80))
            if banner is None:banner=current
            else:assert banner==current,'status and banner must remain fixed across sizes'
            # Last recent item uses precisely the same row geometry as its painted separator.
            info,_=render('baseline-home',tap(400,home[0]+(home[2]-1)*home[1]+home[1]//2));assert info['route']==1
            state,current=render('book-1')
            if reader is None:reader=current
            else:assert reader[80*342:]==current[80*342:],'reading body/tools must retain independent typography'
            for view in ('files-0','files-1','book-4','book-9','settings-9','settings-20','transfer-2'):
                render(view)
            state,_=render('settings-20','u,300,850,300,500')
            assert state['settings_geometry'][9]<=state['settings_geometry'][10]
        assert heights==sorted(heights) and heights[-1]>heights[0]
        assert body_sizes==[28,34,39,45],body_sizes
print('PASS: 3 roles/4 presets/2 faces, circular hits, responsive row heights/pages/list clicks, XL 3 recent rows, fixed status/banner/capsule, reading independence and scrollable help')
