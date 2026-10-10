# SPDX-License-Identifier: Apache-2.0
"""Exercise document storage in a temporary TF card, including all PNG filters."""
from pathlib import Path
import shutil,subprocess,tempfile,struct,zlib,os
import sys
sys.path.insert(0,str(Path(__file__).resolve().parent/'simulator'))
from server import ROOT,BUILD,CARD,STORES

def png(filter,width=17,height=25):
    row_size=(width+1)//2;previous=bytes(row_size);rows=bytearray()
    def paeth(a,b,c):
        p=a+b-c;pa,pb,pc=abs(p-a),abs(p-b),abs(p-c)
        return a if pa<=pb and pa<=pc else b if pb<=pc else c
    for y in range(height):
        pixels=[(x*3+y*5)%16 for x in range(width)]+([0] if width%2 else [])
        row=bytes(pixels[x]<<4|pixels[x+1] for x in range(0,len(pixels),2))
        rows.append(filter)
        for x,value in enumerate(row):
            a=row[x-1] if x else 0;b=previous[x];c=previous[x-1] if x else 0
            predictor=(0,a,b,(a+b)//2,paeth(a,b,c))[filter]
            rows.append((value-predictor)&255)
        previous=row
    def chunk(t,p):return struct.pack('>I',len(p))+t+p+struct.pack('>I',zlib.crc32(t+p)&0xffffffff)
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,4,0,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')

with tempfile.TemporaryDirectory(dir=BUILD,prefix='document-isolated-') as tmp:
    work=Path(tmp);card=work/'card';assets=card/'markdown/attachments'
    assets.mkdir(parents=True);(work/'build/simulator').mkdir(parents=True)
    fixtures=work/'plugins/obsidian-pico/tests/fixtures';fixtures.mkdir(parents=True)
    shutil.copyfile(ROOT/'plugins/obsidian-pico/tests/fixtures/sample-image.png',fixtures/'sample-image.png')
    for i in range(5):(assets/f'unittest-filter{i}.png').write_bytes(png(i))
    (assets/'长图示例-test.png').write_bytes(png(0,684,5000))
    subprocess.run([str(ROOT/'build/document-tests/documents')],cwd=work,
                   env={**os.environ,'PICO_SIM_CARD_ROOT':str(card),'PICO_TEST_FATFS_RENAME':'1'},check=True)
