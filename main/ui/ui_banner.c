/* SPDX-License-Identifier: Apache-2.0
 * 中文：格言字号可调、默认96px，按实际字宽换行；保存/选字库/改字号时校验，不在绘制中打开字体。
 * English: Use adjustable type with a 96px default, wrap by real advances, and validate on save/font/size selection, without opening fonts during paint.
 */
#include "ui_banner.h"
#include "ui_kit.h"
#include "ui_bitmap.h"
#include "ttf_font.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "ui_lunar.h"
#include "esp_heap_caps.h"
static const char *const longest_date="2088年12月30日 · 星期三 · 癸亥猪年闰腊月三十";
static const char *const brand="Pico Reader · if Edition · MindReset";
static int width(const char *text,bool custom,int px){return custom?ttf_text_width_px(px,text):ui_text_fixed_width_px(px,text);}
static int ink_height(int px,const char *text,bool custom){int a=0,b=0;if(custom)ttf_measure_line_px(px,text,&a,&b);else ui_text_fixed_measure_px(px,text,&a,&b);return a+b;}
ui_banner_positions_t ui_banner_positions_size(const char *date,int quote_px){
    // 二分求最大字号，避免每次首页绘制逐字号栅格化整行日期。/ Binary-search size instead of rasterizing every intervening size on each home draw.
    int lo=12,hi=96;
    while(lo<hi){int mid=(lo+hi+1)/2;
        if(ui_text_fixed_width_px(mid,date)<=612&&ui_text_fixed_width_px(mid,longest_date)<=612)lo=mid;
        else hi=mid-1;
    }
    int px=lo;
    int dh=ink_height(px,date,false),bh=ink_height(18,brand,false);
    int qh=ink_height(quote_px,"国Ag我将宇宙随身携带",false);if(qh<1)qh=quote_px;
    int total=dh+bh+qh+2*UI_BANNER_GAP;
    int top=205-total/2,limit=UI_BANNER_BOTTOM-total-qh-UI_BANNER_GAP;
    if(top>limit)top=limit;
    if(top<82)top=82;
    return (ui_banner_positions_t){px,top,top+dh+UI_BANNER_GAP,top+dh+bh+2*UI_BANNER_GAP,qh,quote_px,content_home_quote_alignment()};
}
static bool quote_layout_size(const char *text,bool custom,int px,ui_banner_quote_t *out){
    memset(out,0,sizeof(*out));if(!content_home_quote_valid(text))return false;
    const char *next=text;
    for(int row=0;row<2&&*next;++row){size_t used=0;
        while(*next){unsigned char ch=(unsigned char)*next;size_t bytes=ch<0x80?1:ch<0xe0?2:ch<0xf0?3:4;
            memcpy(out->lines[row]+used,next,bytes);out->lines[row][used+bytes]=0;
            if(width(out->lines[row],custom,px)>612){out->lines[row][used]=0;break;}
            used+=bytes;next+=bytes;
        }
        if(out->lines[row][0])++out->count;
    }
    out->overflow=*next!=0;return !out->overflow;
}
static bool fits_active(const char *text,bool custom,ui_banner_positions_t p){
    ui_banner_quote_t q;if(!quote_layout_size(text,custom,p.quote_px,&q))return false;
    int y=p.quote_top;
    for(int i=0;i<q.count;++i){int h=ink_height(p.quote_px,q.lines[i],custom);
        if(y+h>UI_BANNER_BOTTOM)return false;
        y+=h+UI_BANNER_GAP;}
    return true;
}
bool ui_banner_quote_fits_size(const char *text,const char *path,int px){
    if(!content_home_quote_size_valid((unsigned)px))return false;
    ui_banner_positions_t p=ui_banner_positions_size(longest_date,px);
    if(!path||!path[0])return fits_active(text,false,p);
    char saved[TTF_FONT_PATH_MAX];strlcpy(saved,ttf_font_path(),sizeof(saved));int weight=ttf_get_weight();
    bool custom=ttf_font_open(path)==ESP_OK&&ttf_font_has_text(text),fit=false;
    if(custom)fit=fits_active(text,true,p);
    if(ttf_font_open(saved)!=ESP_OK)ttf_font_open_builtin();
    ttf_set_weight(weight);
    return custom?fit:fits_active(text,false,p);
}
void ui_banner_quote_draw(uint8_t *fb,const char *text,bool custom,ui_banner_positions_t p){
    ui_banner_quote_t q;quote_layout_size(text,custom,p.quote_px,&q);
    if(q.overflow){char *last=q.lines[1];size_t n=strlen(last);
        while(n&&width(last,custom,p.quote_px)+width("…",custom,p.quote_px)>612){do{--n;}while(n&&((unsigned char)last[n]&0xc0)==0x80);last[n]=0;}
        strlcat(last,"…",sizeof(q.lines[1]));if(last[0]&&q.count<2)q.count=2;
    }
    int y=p.quote_top;
    for(int row=0;row<q.count;++row){int above=0,below=0;
        if(custom)ttf_measure_line_px(p.quote_px,q.lines[row],&above,&below);
        else ui_text_fixed_measure_px(p.quote_px,q.lines[row],&above,&below);
        if(y+above+below>UI_BANNER_BOTTOM)break;
        enum EpdFontFlags align=p.quote_align==CONTENT_QUOTE_LEFT?EPD_DRAW_ALIGN_LEFT:p.quote_align==CONTENT_QUOTE_RIGHT?EPD_DRAW_ALIGN_RIGHT:EPD_DRAW_ALIGN_CENTER;
        int x=p.quote_align==CONTENT_QUOTE_LEFT?36:p.quote_align==CONTENT_QUOTE_RIGHT?648:342;
        if(p.quote_align==CONTENT_QUOTE_JUSTIFY)ui_text_justify_ink_top(fb,36,y,p.quote_px,q.lines[row],612,custom);
        else if(custom)ttf_draw_text_px(fb,x,y+above,p.quote_px,q.lines[row],align,0,15);
        else ui_text_fixed_ink_top(fb,x,y,p.quote_px,q.lines[row],align);
        y+=above+below+UI_BANNER_GAP;
    }
}

ui_banner_positions_t ui_banner_positions(const char *date){return ui_banner_positions_size(date,content_home_quote_size());}
bool ui_banner_quote_layout(const char *text,bool custom,ui_banner_quote_t *out){return quote_layout_size(text,custom,content_home_quote_size(),out);}
bool ui_banner_quote_fits_font(const char *text,const char *path){return ui_banner_quote_fits_size(text,path,content_home_quote_size());}
void ui_banner_brand_draw(uint8_t *fb,ui_banner_positions_t p){
    ui_text_fixed_ink_top(fb,342,p.brand_top,18,brand,EPD_DRAW_ALIGN_CENTER);
    int text_width=ui_text_fixed_width_px(18,brand);
    int rule_width=(588-text_width)/2-14;
    if(rule_width<1)return;
    int y=p.brand_top+ink_height(18,brand,false)/2;
    epd_fill_rect((EpdRect){48,y,rule_width,1},0x88,fb);
    epd_fill_rect((EpdRect){636-rule_width,y,rule_width,1},0x88,fb);
}
void ui_banner_date_line(uint32_t day,bool valid,char *line,size_t cap){
    static const char *const weekdays[]={"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
    struct tm date;char lunar[80];time_t seconds=(time_t)((uint64_t)day*86400);
    strlcpy(line,"请设置日期与时间",cap);
    if(valid&&gmtime_r(&seconds,&date)){bool have=ui_lunar_label((int32_t)day,lunar,sizeof(lunar));
        snprintf(line,cap,"%d年%d月%d日 · %s%s%s",date.tm_year+1900,date.tm_mon+1,date.tm_mday,weekdays[date.tm_wday],have?" · ":"",have?lunar:"");}
}
bool ui_banner_preview(const char *text,const char *path,int px,content_quote_align_t align,uint32_t day,bool valid,uint8_t **out,bool *fits){
    *out=NULL;*fits=false;if(!content_home_quote_size_valid((unsigned)px)||!content_home_quote_alignment_valid((unsigned)align))return false;
    uint8_t *page=heap_caps_malloc(UI_LOCK_WIDTH*UI_LOCK_HEIGHT/2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    uint8_t *image=heap_caps_malloc(UI_LOCK_WIDTH*300/2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!page||!image){free(page);free(image);return false;}
    memset(page,255,UI_LOCK_WIDTH*UI_LOCK_HEIGHT/2);char date[160];ui_banner_date_line(day,valid,date,sizeof(date));
    ui_banner_positions_t p=ui_banner_positions_size(date,px);p.quote_align=align;
    ui_text_fixed_ink_top(page,342,p.date_top,p.date_px,date,EPD_DRAW_ALIGN_CENTER);
    ui_banner_brand_draw(page,p);
    bool custom=false;
    if(path&&path[0]){char saved[TTF_FONT_PATH_MAX];strlcpy(saved,ttf_font_path(),sizeof(saved));int weight=ttf_get_weight();
        custom=ttf_font_open(path)==ESP_OK&&ttf_font_has_text(text);
        if(custom){ui_banner_quote_draw(page,text,true,p);*fits=fits_active(text,true,p);}
        if(ttf_font_open(saved)!=ESP_OK)ttf_font_open_builtin();
        ttf_set_weight(weight);
    }
    if(!custom){ui_banner_quote_draw(page,text,false,p);*fits=fits_active(text,false,p);}
    ui_bitmap_capture(page,(EpdRect){0,80,UI_LOCK_WIDTH,300},image);
    free(page);*out=image;return true;
}
