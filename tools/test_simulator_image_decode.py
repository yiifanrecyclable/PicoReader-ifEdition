# SPDX-License-Identifier: Apache-2.0
"""宿主PNG/JPEG行方向与裁剪验证，完全使用合成图片。 / Check host image row orientation using synthetic images."""
from pathlib import Path
import subprocess, tempfile
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='image-orientation-') as temp:
    work=Path(temp)
    source=work/'test.c'
    source.write_text(r'''
#include <stdio.h>
#include <stdlib.h>
#include "book_cover.h"
#include "esp_err.h"
esp_err_t book_epub_cover(const char*p,uint8_t**d,size_t*n,bool*png){(void)p;(void)d;(void)n;(void)png;return ESP_FAIL;}
bool book_auto_cover_render(const char*p,const char*t,const char*a,unsigned w,unsigned h,uint8_t*out){(void)p;(void)t;(void)a;(void)w;(void)h;(void)out;return false;}
int main(int argc,char**argv){if(argc!=4)return 1;FILE*f=fopen(argv[1],"rb");if(!f)return 2;fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);uint8_t*d=malloc(n);if(!d||fread(d,1,n,f)!=n)return 3;fclose(f);unsigned w=atoi(argv[2]),h=atoi(argv[3]);uint8_t*out=malloc(w*h);if(!out||!book_image_grayscale(d,n,true,w,h,out))return 4;fwrite(out,1,w*h,stdout);free(d);free(out);return 0;}
''')
    binary=work/'decode'
    subprocess.run(['cc','-std=gnu11','-I'+str(ROOT/'tools/simulator/include'),'-I'+str(ROOT/'main/book'),
        str(source),str(ROOT/'tools/simulator/image_decode.c'),'-framework','CoreFoundation','-framework','CoreGraphics','-framework','ImageIO','-o',str(binary)],check=True)
    im=Image.new('RGB',(64,64));pix=im.load()
    for y in range(64):
        for x in range(64):
            g=(30 if x<32 else 80) if y<32 else (160 if x<32 else 230)
            pix[x,y]=(g,g,g)
    for ext in ('png','jpg'):
        path=work/f'quadrants.{ext}';im.save(path,quality=100)
        for w,h in ((64,64),(32,32),(48,24),(24,48)):
            out=subprocess.check_output([str(binary),str(path),str(w),str(h)])
            samples=[out[y*w+x] for x,y in ((w//4,h//4),(3*w//4,h//4),(w//4,3*h//4),(3*w//4,3*h//4))]
            assert all(abs(a-b)<=4 for a,b in zip(samples,(30,80,160,230))),(ext,w,h,samples)
    print('PASS: PNG/JPEG rows stay upright at native/scaled/cropped sizes; firmware decoder unchanged.')
