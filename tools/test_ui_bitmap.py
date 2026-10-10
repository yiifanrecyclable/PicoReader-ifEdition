# SPDX-License-Identifier: Apache-2.0
"""Check cached logical images against physical framebuffer rotation, without a real device."""
from pathlib import Path
import subprocess, tempfile
ROOT=Path(__file__).resolve().parents[1]
SOURCE=r'''
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ui_bitmap.h"
static enum EpdRotation rotation;
static int width,height;
int epd_width(void){return width;}
int epd_height(void){return height;}
enum EpdRotation epd_get_rotation(void){return rotation;}
uint8_t epd_get_pixel(int x,int y,int w,int h,const uint8_t *fb){
    assert(x>=0&&x<w&&y>=0&&y<h);
    uint8_t b=fb[(size_t)y*((w+1)/2)+x/2];return x&1?b&0xf0:b<<4;
}
static void physical(int *x,int *y){
    int a=*x,b=*y;
    if(rotation==EPD_ROT_PORTRAIT){*x=width-1-b;*y=a;}
    if(rotation==EPD_ROT_INVERTED_PORTRAIT){*x=b;*y=height-1-a;}
    if(rotation==EPD_ROT_INVERTED_LANDSCAPE){*x=width-1-a;*y=height-1-b;}
}
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t *fb){
    physical(&x,&y);assert(x>=0&&x<width&&y>=0&&y<height);
    size_t at=(size_t)y*width/2+x/2;
    fb[at]=x&1?(fb[at]&15)|(c&240):(fb[at]&240)|(c>>4);
}
void epd_draw_rotated_image(EpdRect r,const uint8_t *image,uint8_t *fb){
    for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)
        epd_draw_pixel(r.x+x,r.y+y,epd_get_pixel(x,y,r.width,r.height,image),fb);
}
static uint8_t logical(const uint8_t *fb,int x,int y){physical(&x,&y);return epd_get_pixel(x,y,width,height,fb);}
int main(void){
    size_t bytes=684*1216/2;
    uint8_t *page=malloc(bytes),*target=malloc(bytes),*before=malloc(bytes),*image=malloc(684*300/2);
    assert(page&&target&&before&&image);
    for(int rot=0;rot<4;++rot){
        rotation=(enum EpdRotation)rot;
        width=rot%2?1216:684;height=rot%2?684:1216;
        memset(page,255,bytes);memset(target,0xa5,bytes);memcpy(before,target,bytes);
        for(int y=0;y<1216;++y)for(int x=0;x<684;++x)
            epd_draw_pixel(x,y,((x/9+y/5)%16)<<4,page);
        ui_bitmap_capture(page,(EpdRect){0,80,684,300},image);
        ui_bitmap_draw(target,(EpdRect){0,230,684,300},image);
        for(int y=0;y<1216;++y)for(int x=0;x<684;++x){
            if(y>=230&&y<530)assert(logical(target,x,y)==logical(page,x,y-150));
            else assert(logical(target,x,y)==logical(before,x,y));
        }
    }
    free(page);free(target);free(before);free(image);
    puts("PASS: cached banner round-trip in all four physical rotations; status/navigation and other pixels preserved.");
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='bitmap-rotation-') as tmp:
    work=Path(tmp);source=work/'test.c';binary=work/'test';source.write_text(SOURCE)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(ROOT/'tools/simulator/include'),'-I'+str(ROOT/'main/ui'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
