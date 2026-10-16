# SPDX-License-Identifier: Apache-2.0
"""首页分列、真实续读顺序与点击边界；隔离TF/NVS。 / Native recent columns and hit bounds over private TF/NVS."""
from pathlib import Path
import json, os, subprocess, tempfile, shutil

ROOT = Path(__file__).resolve().parents[1]
def tap(x, y): return f't,{x},{y},{x},{y}'

with tempfile.TemporaryDirectory(dir=ROOT / 'build', prefix='home-layout-') as tmp:
    work = Path(tmp)
    card = work / 'card'
    build = work / 'build/simulator'
    build.mkdir(parents=True)
    for folder in ('books', 'markdown/attachments', 'fonts'):
        (card / folder).mkdir(parents=True)
    for name in ('Old.txt', 'New.txt', 'Unread.txt'):
        (card / 'books' / name).write_text('Chapter\n\nReading text.\n' * 20)
    for name in ('A.md', 'B.md', 'C.md', 'D.md', 'E.md', 'Unread.md'):
        (card / 'markdown' / name).write_text('# Title\n\nDocument body.\n')
    (build / 'titles.txt').write_text('')
    env = {**os.environ, 'PICO_SIM_CARD_ROOT': str(card), 'PICO_SIM_HOME_DAY': '20733'}

    def render(steps=''):
        result = subprocess.run([str(ROOT / 'build/simulator/render'), 'baseline-home', '', '0', '48',
            'build/simulator/frame.bin', 'build/simulator/titles.txt', '0', ','.join(['60'] * 30),
            'builtin', steps, ''], cwd=work, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0, result.stderr
        return json.loads(result.stdout), (build / 'frame.bin').read_bytes()

    def save(path):
        result = subprocess.run([str(ROOT / 'build/simulator/cloud-host'), 'progress-save', path],
            cwd=work, env=env, capture_output=True, text=True, timeout=30)
        assert result.returncode == 0 and json.loads(result.stdout)['error'] == 0, result.stderr

    def tone(frame, x, y): return (frame[y * 342 + x // 2] >> (4 if x % 2 else 0)) & 15
    def open_at(x, y, name):
        info, _ = render(tap(x, y))
        assert info['route'] == 1 and Path(info['book_path']).name == name, info

    # 未读内容不能冒充最近阅读；已删除同步板块不再响应。 / Unread imports are absent; retired sync rows are inert.
    assert render(tap(100, 900))[0]['route'] == -1
    assert render(tap(400, 830))[0]['route'] == -1
    assert render(tap(580, 117))[0]['route'] == -1
    geometry=render()[0]['home_geometry'];content_y,row_h,rows=geometry[:3]
    def check_rules(frame):
        for i in range(rows+1):
            y=content_y+i*row_h
            assert all(tone(frame,x,y)==8 for x in range(272,648)),y
        assert all(tone(frame,240,y)==12 for y in range(content_y,content_y+rows*row_h+1))
    check_rules(render()[1])
    save('/sdcard/books/Old.txt')
    save('/sdcard/books/New.txt')
    for name in ('E.md', 'A.md', 'B.md', 'C.md', 'D.md'):
        save('/sdcard/markdown/' + name)
    # 最后阅读是MD仍显示最新图书；文档列表只按阅读排序。 / A last-open MD never becomes the book cover.
    (build / 'nvs/rp_books/last').write_bytes(b'/sdcard/markdown/D.md\0')
    prefs = card / 'markdown/.index'
    prefs.mkdir(exist_ok=True)
    (prefs / 'A.md.prefs').write_text('pico-md-prefs-1\n1\n')
    (prefs / 'Unread.md.sync').write_text('2099-01-01\n')
    open_at(100, 900, 'New.txt')
    for row, name in enumerate(('D.md', 'C.md', 'B.md', 'A.md')):
        open_at(400, content_y+row*row_h+row_h//2, name)
    for x in (225, 240, 260):
        assert render(tap(x, 900))[0]['route'] == -1
    assert render(tap(400, 1120))[0]['route'] == -1
    assert render(tap(400, 740))[0]['route'] == -1
    _, frame = render()
    check_rules(frame)
    def check_document_group(frame,row):
        start=content_y+row*row_h
        expected=[0]*113+[12]*227
        bars=[y for y in range(start+1,start+row_h) if [tone(frame,x,y) for x in range(272,612)]==expected]
        assert len(bars)==4 and bars==list(range(bars[0],bars[0]+4)),(row,bars)
        ink=[y for y in range(start+1,start+row_h) if any(tone(frame,x,y)==0 for x in range(272,612))]
        assert abs((min(ink)+max(ink))/2-(start+row_h/2))<=4,(row,min(ink),max(ink))
        assert bars[0]>min(ink)
    for row in range(rows):check_document_group(frame,row)
    grid_y,col_x,col_step,row_step,month_x=geometry[3:8]
    assert tone(frame,col_x-22,grid_y)==14 and tone(frame,col_x+21,grid_y+43)==14
    assert any(tone(frame,x,y)==0 for y in range(grid_y+2*row_step,grid_y+2*row_step+44) for x in range(month_x-80,month_x))
    info,_=render('l,400,285,400,285');assert info['route']==3 and info['home_quote_requested']
    # 缺失/改变大小/清进度后下一个有效阅读项递补。 / Missing, changed-size and forgotten reads are skipped.
    (card / 'markdown/D.md').unlink()
    (card / 'markdown/E.md').unlink()
    (card / 'markdown/C.md').write_text('# Changed size\n')
    open_at(400,content_y+row_h//2,'B.md')
    open_at(400,content_y+row_h+row_h//2,'A.md')
    for y in (content_y+2*row_h+row_h//2,content_y+3*row_h+row_h//2):
        assert render(tap(400, y))[0]['route'] == -1
    _, frame = render()
    check_rules(frame)
    assert all(tone(frame, x, y) == 15 for y in range(content_y+2*row_h+1,content_y+3*row_h) for x in range(272, 648))
    assert all(tone(frame, x, y) == 15 for y in range(content_y+3*row_h+1,content_y+4*row_h) for x in range(272, 648))
    # 同名左右标题使用完全相同字号，长文档标题两行与横条也整体居中。
    # Matching book/document captions use identical type; two-line document groups stay centered too.
    (card / 'markdown/New.md').write_text('# New\n\nReading body.\n')
    save('/sdcard/markdown/New.md')
    _, frame = render()
    long_name = 'LongTitle' * 10 + '.md'
    (card / 'markdown' / long_name).write_text('# Long title\n\nText.\n')
    save('/sdcard/markdown/' + long_name)
    _, frame = render()
    check_document_group(frame, 0)
    check_rules(frame)
    # 一行改为两行只向下延伸，日期/品牌/格言首行起点不动；间距按墨迹一致。
    # A second quote row extends down without moving date/brand/first-quote anchors; ink gaps match.
    quote_file=build/'nvs/if_content/home_quote';quote_file.parent.mkdir(exist_ok=True)
    def banner_runs(frame):
        ys=[y for y in range(80,370) if any(tone(frame,x,y)<8 for x in range(36,648))]
        runs=[]
        for y in ys:
            if not runs or y>runs[-1][1]+1:runs.append([y,y])
            else:runs[-1][1]=y
        return runs
    last=None
    for n in range(1,30):
        quote_file.write_bytes(('国'*n+'\0').encode());_,frame=render();runs=banner_runs(frame)
        if len(runs)==4:
            assert last and len(last[1])==3
            first,r1=last;assert frame[80*342:(r1[2][1]+1)*342]==first[80*342:(r1[2][1]+1)*342]
            assert runs[:3]==r1
            assert abs((r1[1][0]-r1[0][1])-(r1[2][0]-r1[1][1]))<=2
            assert 185<=(r1[0][0]+r1[2][1])/2<=220
            assert r1[2][1]-r1[2][0]>48
            break
        last=(frame,runs)
    else:raise AssertionError('Quote never wrapped into two visible rows')
    quote_file.unlink()
    # 独立格言字库只影响banner第三行，绘制后原字库恢复；缺失资源回退。
    # An independent quote font only affects the third banner line, restores the active face and falls back if missing.
    font=card/'fonts/PicoSans-Medium.ttf';shutil.copyfile(ROOT/'assets/fonts/PicoSans-Medium.ttf',font)
    before,base=render()
    prefs_font=build/'nvs/if_content/quote_font';prefs_font.parent.mkdir(exist_ok=True)
    prefs_font.write_bytes(b'/sdcard/fonts/PicoSans-Medium.ttf\0')
    after,custom=render();assert before['font_path']==after['font_path']
    assert base[80*342:170*342]==custom[80*342:170*342] and base[370*342:]==custom[370*342:]
    assert any(tone(custom,x,y)<15 for y in range(240,320) for x in range(36,648))
    font.unlink();render()
    # 通过原生进度文件移除记录，避免依赖额外宿主命令。 / Remove only the private progress record, without new host commands.
    for entry in (build / 'nvs/rp_books').glob('b_*'):
        if b'/sdcard/books/New.txt\0' in entry.read_bytes(): entry.unlink()
    open_at(100, 900, 'Old.txt')
    print('PASS: native recent-read separation/order, responsive ruled rows/blanks and centered title/progress groups, hit bounds, missing/replaced/forgotten reads and independent banner fonts.')
