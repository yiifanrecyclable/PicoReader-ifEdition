# SPDX-License-Identifier: Apache-2.0
"""原尺寸banner预览、字体和字号滑块，仅私有TF/NVS。 / Exact preview, fonts and size slider over private TF/NVS."""
from pathlib import Path
import json,os,shutil,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def tap(x,y):return f't,{x},{y},{x},{y}'
with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='banner-preview-') as temp:
    work=Path(temp);card=work/'card';out=work/'build/simulator';out.mkdir(parents=True)
    for folder in ('books','markdown/attachments','fonts'):(card/folder).mkdir(parents=True)
    shutil.copyfile(ROOT/'assets/fonts/PicoSans-Medium.ttf',card/'fonts/PicoSans-Medium.ttf')
    (out/'titles.txt').write_text('');env={**os.environ,'PICO_SIM_CARD_ROOT':str(card),'PICO_SIM_HOME_DAY':'20733'}
    def render(native='settings-21',events=''):
        p=subprocess.run([str(ROOT/'build/simulator/render'),native,'','0','48','build/simulator/frame.bin','build/simulator/titles.txt','0',','.join(['60']*30),'builtin',events,''],cwd=work,env=env,capture_output=True,text=True,timeout=30)
        assert p.returncode==0,p.stderr
        return json.loads(p.stdout),(out/'frame.bin').read_bytes()
    def same_as_home(frame):
        home,actual=render('baseline-home')
        assert frame[230*342:530*342]==actual[80*342:380*342]
    info,initial=render();assert info['quote_size']==96;same_as_home(initial)
    size=out/'nvs/if_content/quote_px'
    drag='p,432,848,432,848;m,432,848,612,848'
    info,moving=render(events=drag);assert info['quote_size']==120 and not size.exists() and not info['quote_settings_committed']
    assert moving[230*342:530*342]==initial[230*342:530*342]
    info,changed=render(events=drag+';r,432,848,612,848');assert info['quote_size']==120 and size.read_bytes()==bytes([120]) and info['quote_settings_committed']
    same_as_home(changed)
    info,small=render(events=tap(72,848));assert info['quote_size']==48 and size.read_bytes()==bytes([48]);same_as_home(small)
    render(events=tap(432,848));before=size.read_bytes()
    info,_=render(events=drag+';c,432,848,612,848');assert info['quote_size']==96 and size.read_bytes()==before and not info['quote_settings_committed']
    assert render(events=tap(340,608))[0]['settings_page']==23
    assert render(events=tap(340,698))[0]['settings_page']==22
    info,font=render('settings-22',tap(340,423));assert info['settings_page']==21 and info['quote_settings_committed'];same_as_home(font)
    assert (out/'nvs/if_content/quote_font').read_bytes()==b'/sdcard/fonts/PicoSans-Medium.ttf\0'
    quote=out/'nvs/if_content/home_quote';quote.write_bytes(('国'*18+'\0').encode())
    info,valid=render();same_as_home(valid)
    info,failed=render(events=tap(612,848));assert info['quote_size']==96 and size.read_bytes()==bytes([96]) and not info['quote_settings_committed']
    assert failed[230*342:530*342]==valid[230*342:530*342]
    assert render(events=tap(340,1092))[0]['route']==0
    # 四种排版的实际位置、默认居中、两端扩距与预览/首页一致。/ Verify alignment positions, centered defaults, justification and exact preview parity.
    alignment=out/'nvs/if_content/quote_align'
    assert not alignment.exists()
    quote.write_bytes(('国国国国'+'\0').encode())
    def tone(frame,x,y):
        value=frame[y*342+x//2];return value>>4 if x%2 else value&15
    def quote_ink(frame):
        ys=[y for y in range(230,530) if any(tone(frame,x,y)<8 for x in range(36,648))]
        rows=[]
        for y in ys:
            if not rows or y>rows[-1][-1]+1:rows.append([y])
            else:rows[-1].append(y)
        assert len(rows)==3,rows
        return {(x,y) for y in rows[2] for x in range(36,648) if tone(frame,x,y)<8},rows
    center=render()[1];center_ink,runs=quote_ink(center)
    def bounds(points):return min(x for x,y in points),max(x for x,y in points)
    assert abs(sum(bounds(center_ink))/2-342)<8
    assert all(13<=runs[i+1][0]-runs[i][-1]-1<=16 for i in range(2))
    for row in runs[:2]:
        pts={(x,y) for y in row for x in range(36,648) if tone(center,x,y)<8}
        assert abs(sum(bounds(pts))/2-342)<8
    frames={2:center}
    for option in (0,1,3,2):
        info,frame=render(events=tap(108+156*option,1006))
        assert info['settings_page']==21 and info['quote_settings_committed']
        assert alignment.read_bytes()==bytes([option]);same_as_home(frame)
        points,rows=quote_ink(frame);assert rows==runs
        assert frame[230*342:runs[1][-1]*342]==center[230*342:runs[1][-1]*342]
        frames[option]=frame
    left_ink,_=quote_ink(frames[0]);right_ink,_=quote_ink(frames[1]);spread_ink,_=quote_ink(frames[3])
    assert bounds(left_ink)[0]<60 and bounds(right_ink)[1]>625
    assert bounds(spread_ink)[0]<60 and bounds(spread_ink)[1]>625
    def normalize(points):
        x0=min(x for x,y in points);return {(x-x0,y) for x,y in points}
    assert normalize(left_ink)==normalize(center_ink)==normalize(right_ink)
    assert bounds(spread_ink)[1]-bounds(spread_ink)[0]>bounds(center_ink)[1]-bounds(center_ink)[0]+100
    # 英文优先扩大单词间的空格，不把单词内部的字母拆散。/ Expand English word spaces without stretching letters within words.
    quote.write_bytes(b'Mind reset\0');render(events=tap(108,1006));normal,rows=quote_ink(render()[1])
    render(events=tap(576,1006));spread,spread_rows=quote_ink(render()[1]);assert rows==spread_rows
    # 比较左端第一单词附近的完整像素，只有词距变化。/ Compare the unchanged leading word pixels; only the inter-word gap changes.
    left_frame=render(events=tap(108,1006))[1];justified=render(events=tap(576,1006))[1]
    for y in rows[2]:
        assert all(tone(left_frame,x,y)==tone(justified,x,y) for x in range(36,200))
    quote.write_bytes(('国'+'\0').encode());_,single=render();assert abs(sum(bounds(quote_ink(single)[0]))/2-342)<8
    quote.write_bytes(b'\0');same_as_home(render()[1])
    quote.write_bytes(('国国国国'+'\0').encode());alignment.write_bytes(bytes([99]));_,fallback=render()
    assert fallback[230*342:530*342]==center[230*342:530*342]
    alignment.unlink()
    (card/'fonts/PicoSans-Medium.ttf').unlink();same_as_home(render()[1])
    _,no_font=render(events=tap(576,1006));same_as_home(no_font)
    assert bounds(quote_ink(no_font)[0])[0]<60 and bounds(quote_ink(no_font)[0])[1]>625
    print('PASS: settings preview equals home pixel-for-pixel with default/custom/missing font; slider boundaries/persistence, move/cancel no writes, overflow rejection, four alignments, English word spacing, single-character/default fallback and return-to-preview verified.')
