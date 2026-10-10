/* SPDX-License-Identifier: Apache-2.0
 * 固定64字节排序记录，二分读取；不把词库整体装入RAM，不持有跨事件TF句柄。
 * Binary-search fixed 64-byte sorted records; no whole-dictionary RAM load or cross-event TF handles.
 */
#include "ui_pinyin.h"
#include "read_pico_search.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool record(FILE *file, uint32_t index, char row[64]) {
    if(fseek(file,16L+(long)index*64,SEEK_SET)||fread(row,1,64,file)!=64)return false;
    if(!memchr(row,0,33)||!memchr(row+33,0,31))return false;
    for(const char *p=row;*p;++p)if(*p<'a'||*p>'z')return false;
    size_t n=strlen(row+33);
    if(!n||n>=UI_PINYIN_WORD_BYTES||n%3)return false;
    for(size_t i=33;i<33+n;i+=3){
        unsigned char a=row[i],b=row[i+1],c=row[i+2];
        if(a<0xe4||a>0xe9||(b&0xc0)!=0x80||(c&0xc0)!=0x80)return false;
        uint32_t cp=((a&15)<<12)|((b&63)<<6)|(c&63);
        if(cp<0x4e00||cp>0x9fff)return false;
    }
    return true;
}
static bool bound(FILE *file,uint32_t count,const char *pinyin,bool upper,uint32_t *index){
    uint32_t low=0,high=count;char row[64];
    while(low<high){uint32_t mid=low+(high-low)/2;if(!record(file,mid,row))return false;
        int cmp=strcmp(row,pinyin);if(cmp<0||(upper&&cmp==0))low=mid+1;else high=mid;}
    *index=low;return true;
}
static size_t phrases(const char *pinyin,char (*out)[UI_PINYIN_WORD_BYTES],size_t cap,size_t skip,size_t *total){
    *total=0;FILE *file=fopen(UI_PINYIN_DICTIONARY_PATH,"rb");if(!file)return 0;
    unsigned char header[16];char row[64];size_t filled=0;
    if(fread(header,1,16,file)!=16||memcmp(header,"IFPY001\0",8)||
        header[12]||header[13]||header[14]||header[15])goto finish;
    uint32_t count=header[8]|((uint32_t)header[9]<<8)|((uint32_t)header[10]<<16)|((uint32_t)header[11]<<24);
    if(!count||count>100000||fseek(file,0,SEEK_END)||ftell(file)!=16L+64L*count)goto finish;
    uint32_t first,last;
    if(!bound(file,count,pinyin,false,&first)||!bound(file,count,pinyin,true,&last))goto finish;
    *total=last-first;
    for(size_t i=skip;i<*total&&filled<cap;++i){
        if(!record(file,first+(uint32_t)i,row)||strcmp(row,pinyin)){filled=0;*total=0;goto finish;}
        memcpy(out[filled++],row+33,strlen(row+33)+1);
    }
finish:
    fclose(file);return filled;
}
size_t ui_pinyin_candidates(const char *pinyin,char (*out)[UI_PINYIN_WORD_BYTES],size_t cap,size_t skip){
    if(!pinyin||!out||!cap||cap>6)return 0;
    size_t len=strnlen(pinyin,33);if(!len||len>=33)return 0;
    for(size_t i=0;i<len;++i)if(pinyin[i]<'a'||pinyin[i]>'z')return 0;
    size_t total=0,filled=len>1?phrases(pinyin,out,cap,skip,&total):0;
    if(filled==cap)return filled;
    uint32_t chars[6];size_t n=read_pico_search_candidates(pinyin,chars,cap-filled,skip>total?skip-total:0);
    for(size_t i=0;i<n;++i){uint32_t cp=chars[i];char *word=out[filled++];
        word[0]=0xe0|(cp>>12);word[1]=0x80|((cp>>6)&63);word[2]=0x80|(cp&63);word[3]=0;}
    return filled;
}
