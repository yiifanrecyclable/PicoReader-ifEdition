"""设置页实际滚动反馈及字号选择回归。/ Actual settings scrolling and size-picker regression.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/apps/app_device_settings.c').read_text()
def block(start):
    at=s.index('{',start)+1
    depth,quote,escape=1,None,False
    while depth:
        c=s[at]
        if quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif c in "\"'":quote=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
        at+=1
    return s[start:at]
def function(name):
    m=re.search(r'^static [^\n]+\b'+name+r'\([^;{}]*\)\s*\{',s,re.M)
    assert m,name
    return block(m.start())
unit=r"""
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {uint8_t *fb;void *hl;} app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE} app_redraw_t;
typedef enum {UI_GESTURE_PRESS,UI_GESTURE_MOVE,UI_GESTURE_TAP,UI_GESTURE_CANCEL,UI_GESTURE_SWIPE_U,UI_GESTURE_SWIPE_D} gesture_t;
typedef struct {gesture_t type;int x,y,x0,y0;} ui_gesture_event_t;
enum {SETTINGS_MAIN,SETTINGS_UPGRADE,SETTINGS_SYSTEM_SIZE,SETTINGS_CLOUD,
      SETTINGS_SYSTEM_FONT,SETTINGS_WALLPAPER,SETTINGS_TEXT_EDIT,SETTINGS_HOME_QUOTE,SETTINGS_ONLINE_UPDATE};
enum EpdDrawMode {MODE_GC16,MODE_GL16};
#define UI_NAV_TOP 1136
#define UI_LOCK_WIDTH 684
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_LEFT 0
static const int E0470_WAVEFORM=0;
static int s_page;
static bool s_scroll_drag_consumed,s_scroll_present_pending;
static int paints,pushes;
static EpdRect pushed;
static enum EpdDrawMode push_mode;
static void render(app_ctx_t *ctx,uint8_t *fb){(void)ctx;(void)fb;++paints;}
static int update_display_area_with(void *hl,const int *wave,enum EpdDrawMode mode,EpdRect area){(void)hl;(void)wave;pushed=area;push_mode=mode;++pushes;return 0;}
static int update_display_area_diff_with(void *hl,const int *wave,enum EpdDrawMode mode,EpdRect area){return update_display_area_with(hl,wave,mode,area);}
static void guard_draw_result(void *hl,int status){(void)hl;assert(!status);}
static EpdRect ui_text_input_area(void){return (EpdRect){0,226,684,990};}
"""+function('scroll_gesture')+'\n'+function('settings_present')+'\n'
unit+=r"""
int main(void){
 app_ctx_t ctx={0};ui_gesture_event_t ev={.type=UI_GESTURE_PRESS,.x0=300,.y0=700,.x=300,.y=700};int offset=0;bool handled;
 s_page=SETTINGS_MAIN;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE&&handled);
 ev.type=UI_GESTURE_MOVE;ev.y=600;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE&&!offset);
 ev.y=580;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_AREA&&offset==204&&handled);
 assert(settings_present(&ctx,APP_REDRAW_AREA)&&paints==1&&pushes==1&&pushed.y==80&&pushed.height==1056&&push_mode==MODE_GL16);
 ev.y=400;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE&&offset==204);
 ev.type=UI_GESTURE_SWIPE_U;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE&&offset==204&&!s_scroll_drag_consumed);
 ev.type=UI_GESTURE_PRESS;scroll_gesture(&ev,&offset,334,&handled);ev.type=UI_GESTURE_SWIPE_U;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_AREA&&offset==334);assert(settings_present(&ctx,APP_REDRAW_AREA));
 assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE);
 ev.type=UI_GESTURE_SWIPE_D;ev.y=1000;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_AREA&&offset==34);assert(settings_present(&ctx,APP_REDRAW_AREA));
 ev.y0=1150;ev.y=400;ev.type=UI_GESTURE_MOVE;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE&&offset==34);
 ev.y0=60;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE);
 ev.type=UI_GESTURE_TAP;assert(scroll_gesture(&ev,&offset,334,&handled)==APP_REDRAW_NONE&&!handled);
 s_page=SETTINGS_SYSTEM_FONT;
 for(int i=0;i<80;++i){s_scroll_present_pending=true;assert(settings_present(&ctx,APP_REDRAW_AREA));assert(pushed.y==190&&pushed.height==946&&push_mode==MODE_GL16);}
 assert(pushes==83&&paints==83);
 const int pages[]={SETTINGS_SYSTEM_FONT,SETTINGS_WALLPAPER};
 for(unsigned i=0;i<sizeof(pages)/sizeof(pages[0]);++i){
   s_page=pages[i];s_scroll_present_pending=true;assert(settings_present(&ctx,APP_REDRAW_AREA));
   assert(push_mode==MODE_GL16&&pushed.y==190&&pushed.y+pushed.height==UI_NAV_TOP);
 }
 s_page=SETTINGS_TEXT_EDIT;assert(settings_present(&ctx,APP_REDRAW_AREA));assert(pushed.y==226&&pushed.y+pushed.height==1216&&push_mode==MODE_GL16);
 assert(!settings_present(&ctx,APP_REDRAW_PAGE));
 puts("PASS: immediate bounded scroll, one update per drag, no periodic GC16 during 80 swipes, header/nav excluded across all settings lists; preset selection is covered by test_system_text.py");
}
"""
with tempfile.TemporaryDirectory() as folder:
    c,binary=Path(folder)/'test.c',Path(folder)/'test'
    c.write_text(unit)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
