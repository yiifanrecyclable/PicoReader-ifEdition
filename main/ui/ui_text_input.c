/* SPDX-License-Identifier: Apache-2.0
 * 无动画、无闪烁光标；UTF-8光标按字符移动，候选与所有按键固定在底部。
 * No animations/blinking cursor; UTF-8 cursor steps by character, with bottom-anchored keys/candidates.
 */
#include "ui_text_input.h"
#include "ui_nav.h"
#include "read_pico_search.h"
#include <stdio.h>
#include <string.h>

enum { LANGUAGE=40, SYMBOLS, SPACE, BACKSPACE, DONE, PREVIOUS, NEXT,
       CANDIDATE=47, LEFT=52, RIGHT, SHIFT, CLEAR, REVEAL, RAW, CANCEL, FIELD };
static size_t next_character(const char *s, size_t at) {
    if (!s[at]) return at;
    ++at;
    while (s[at] && ((unsigned char)s[at] & 0xc0) == 0x80) ++at;
    return at;
}
static size_t previous_character(const char *s, size_t at) {
    if (!at) return 0;
    do { --at; } while (at && ((unsigned char)s[at] & 0xc0) == 0x80);
    return at;
}
static void refresh_candidates(ui_text_input_t *in) {
    // 多取一字判断下一页，避免无限翻到空页。/ Fetch one extra candidate to bound paging.
    in->candidate_count = in->pinyin[0] ? ui_pinyin_candidates(in->pinyin,
        in->candidates, 6, in->candidate_page * 5) : 0;
}
static size_t safe_cursor(const ui_text_input_t *in) {
    size_t used = strlen(in->text), at = in->cursor < used ? in->cursor : used;
    while (at && ((unsigned char)in->text[at] & 0xc0) == 0x80) --at;
    return at;
}
void ui_text_input_open(ui_text_input_t *in, char *text, size_t cap,
                        bool chinese, bool secret, ui_input_mode_t initial) {
    memset(in, 0, sizeof(*in));
    in->text = text; in->capacity = cap; in->cursor = strlen(text);
    in->chinese_allowed = chinese; in->secret = secret;
    in->mode = in->letters_mode = chinese ? initial : UI_INPUT_ENGLISH;
}
void ui_text_input_clear(ui_text_input_t *in) {
    // 密码退出时清除完整缓冲及候选状态。/ Clear the entire secret buffer and composition on exit.
    if (in->text) {
        volatile char *p = in->text;
        for (size_t i = 0; i < in->capacity; ++i) p[i] = 0;
    }
    in->cursor = in->candidate_page = in->candidate_count = 0;
    memset(in->pinyin, 0, sizeof(in->pinyin));
    memset(in->candidates, 0, sizeof(in->candidates));
    in->notice[0] = 0; in->revealed = false;
}
bool ui_text_input_insert(ui_text_input_t *in, const char *text) {
    in->cursor = safe_cursor(in);
    size_t used = strlen(in->text), added = strlen(text);
    // 所有插入统一校验UTF-8，不能把光标或长度上限切在半个字符中。
    // Validate UTF-8 centrally so cursor movement and capacity limits never split a character.
    for(size_t i=0;i<added;){
        unsigned char c=text[i++];unsigned following=0;uint32_t cp=c,minimum=0;
        if(c<0x80){if(c<32||c==127)goto invalid;continue;}
        if(c>=0xc2&&c<=0xdf){following=1;cp=c&31;minimum=128;}
        else if(c>=0xe0&&c<=0xef){following=2;cp=c&15;minimum=2048;}
        else if(c>=0xf0&&c<=0xf4){following=3;cp=c&7;minimum=65536;}
        else goto invalid;
        while(following--){if(i>=added||((unsigned char)text[i]&0xc0)!=0x80)goto invalid;cp=(cp<<6)|((unsigned char)text[i++]&63);}
        if(cp<minimum||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff))goto invalid;
    }
    if (used + added >= in->capacity) {
        snprintf(in->notice, sizeof(in->notice), "内容已达到长度上限"); return false;
    }
    memmove(in->text + in->cursor + added, in->text + in->cursor, used - in->cursor + 1);
    memcpy(in->text + in->cursor, text, added); in->cursor += added; in->notice[0] = 0;
    return true;
invalid:
    snprintf(in->notice,sizeof(in->notice),"输入内容无效");return false;
}
static bool commit_candidate(ui_text_input_t *in, size_t index) {
    if (index >= in->candidate_count || index >= 5) return false;
    if (!ui_text_input_insert(in, in->candidates[index])) return false;
    in->pinyin[0] = 0; in->candidate_page = 0; refresh_candidates(in); return true;
}
static const char *symbol(unsigned page, int index) {
    static const char *const rows[] = {
        "0","1","2","3","4","5","6","7","8","9",
        "!","@","#","$","%","^","&","*","(",")",
        "-","_","+","=","[","]","{","}","<",">",
        "/","\\","|",":",";","'","\"",".",",","?",
        "0","1","2","3","4","5","6","7","8","9",
        "`","~","，","。","！","？","：","；","、","…",
        "（","）","【","】","《","》","“","”","‘","’",
        "「","」","『","』","—","·","/","@","_","-"
    };
    return rows[(page % 2) * 40 + index];
}
static void key_label(const ui_text_input_t *in, int index, char text[8]) {
    if (in->mode == UI_INPUT_SYMBOLS) snprintf(text, 8, "%s",
        symbol(!in->chinese_allowed && index>=12 ? 0 : in->symbol_page, index));
    else {
        const char *keys = "1234567890qwertyuiopasdfghjkl'zxcvbnm,./";
        char c = keys[index];
        if (in->mode == UI_INPUT_ENGLISH && in->uppercase && c >= 'a' && c <= 'z') c -= 32;
        text[0] = c; text[1] = 0;
    }
}
EpdRect ui_text_input_key_rect(int id) {
    if (id >= 0 && id < 40) return (EpdRect){40 + id % 10 * 61, UI_INPUT_KEYS_TOP + id / 10 * UI_INPUT_KEYS_STEP, 55, 74};
    static const int x[] = {40, 144, 238, 442, 546}, widths[] = {98, 88, 198, 98, 98};
    if (id >= LANGUAGE && id <= DONE) return (EpdRect){x[id-40], UI_INPUT_FOOTER_TOP, widths[id-40], 96};
    if (id == PREVIOUS) return (EpdRect){40, UI_INPUT_KEYBOARD_TOP, 42, 64};
    if (id == NEXT) return (EpdRect){602, UI_INPUT_KEYBOARD_TOP, 42, 64};
    if (id >= CANDIDATE && id < CANDIDATE+5) return (EpdRect){88+(id-CANDIDATE)*102, UI_INPUT_KEYBOARD_TOP, 96, 64};
    if (id >= LEFT && id <= CLEAR) return (EpdRect){40+(id-LEFT)*154, 368, 142, 64};
    if (id == REVEAL || id == RAW) return (EpdRect){40, 450, 220, 64};
    if (id == CANCEL) return (EpdRect){24, 76, 100, 80};
    if (id == FIELD) return (EpdRect){40, 234, 604, 104};
    return (EpdRect){0};
}
EpdRect ui_text_input_area(void) { return (EpdRect){0, 226, UI_LOCK_WIDTH, UI_LOCK_HEIGHT-226}; }
static void button(uint8_t *fb, int id, const char *label, bool dark) {
    EpdRect r=ui_text_input_key_rect(id);
    ui_fill_round_rect(fb, r, 10, dark ? UI_GRAY_BLACK : UI_GRAY_WHITE);
    ui_draw_round_rect(fb, r, 10, 0x70);
    int px=ui_text_fit_px(UI_TEXT_BODY,label,r.width-8,r.height-12);
    ui_text_fixed_vc(fb, r.x+r.width/2, r.y+r.height/2, px, label,
                      EPD_DRAW_ALIGN_CENTER, dark);
}
static size_t visible_start(const ui_text_input_t *in, int width) {
    char prefix[257]; size_t start=0;
    while (start < in->cursor) {
        size_t n=in->cursor-start; if (n >= sizeof(prefix)) break;
        if (in->secret && !in->revealed) {
            size_t chars=0; for (size_t i=start; i<in->cursor; i=next_character(in->text,i)) ++chars;
            memset(prefix,'*',chars); prefix[chars]=0;
        } else { memcpy(prefix,in->text+start,n); prefix[n]=0; }
        if (ui_text_fixed_width_px(ui_text_effective_px(UI_TEXT_BODY),prefix) <= width-20) break;
        start=next_character(in->text,start);
    }
    return start;
}
static int field_width(const char *suffix) {
    int suffix_width = suffix && *suffix ? ui_text_fixed_width_px(ui_text_effective_px(UI_TEXT_CAPTION),suffix)+24 : 0;
    return 568-suffix_width;
}
void ui_text_input_draw(uint8_t *fb, const ui_text_input_t *in, const char *title,
                        const char *hint, const char *suffix, const char *done, const char *notice) {
    // 防止外部替换缓冲后留下过期光标；绘制不修改编辑状态。/ Clamp stale cursors locally without mutating on draw.
    ui_text_input_t current = *in; current.cursor = safe_cursor(in); in = &current;
    ui_clear_page(fb); ui_nav_status(fb); ui_nav_back(fb,36,79);
    ui_text_fixed_vc(fb,342,107,ui_text_effective_px(UI_TEXT_TITLE),title,EPD_DRAW_ALIGN_CENTER,false);
    ui_text_fixed(fb,40,176,ui_text_effective_px(UI_TEXT_CAPTION),hint ? hint : "",EPD_DRAW_ALIGN_LEFT,false);
    EpdRect field=ui_text_input_key_rect(FIELD); ui_draw_round_rect(fb,field,12,UI_GRAY_BLACK);
    int width=field_width(suffix); size_t start=visible_start(in,width), at=start, cursor_x=58;
    int x=58; char visible[1025]; size_t used=0; visible[0]=0;
    while (in->text[at]) {
        size_t next=next_character(in->text,at); char glyph[8]; size_t n=next-at;
        if (in->secret && !in->revealed) {glyph[0]='*';glyph[1]=0;}
        else {memcpy(glyph,in->text+at,n);glyph[n]=0;}
        int advance=ui_text_fixed_width_px(ui_text_effective_px(UI_TEXT_BODY),glyph);
        size_t bytes=strlen(glyph);
        if (x+advance > 58+width || used+bytes>=sizeof(visible)) break;
        memcpy(visible+used,glyph,bytes+1);used+=bytes;x+=advance;
        at=next; if (at<=in->cursor) cursor_x=x;
    }
    // 整行一次绘制，共用基线；不能按各字母的墨迹高度逐字居中。/ Draw the entire field on one baseline instead of centering each glyph's ink independently.
    ui_text_fixed_vc(fb,58,286,ui_text_effective_px(UI_TEXT_BODY),visible,EPD_DRAW_ALIGN_LEFT,false);
    epd_fill_rect((EpdRect){cursor_x,286-(ui_text_effective_px(UI_TEXT_BODY)+16)/2,2,ui_text_effective_px(UI_TEXT_BODY)+16},UI_GRAY_BLACK,fb);
    if (suffix && *suffix) ui_text_fixed_vc(fb,626,286,ui_text_effective_px(UI_TEXT_CAPTION),suffix,EPD_DRAW_ALIGN_RIGHT,false);
    button(fb,LEFT,"‹",false); button(fb,RIGHT,"›",false);
    button(fb,SHIFT,in->uppercase?"小写":"大写",in->uppercase); button(fb,CLEAR,"清空",false);
    if (in->secret) button(fb,REVEAL,in->revealed?"隐藏密码":"显示密码",false);
    else if (in->pinyin[0]) button(fb,RAW,"输入原文",false);
    const char *message=in->notice[0]?in->notice:notice;
    if (message && *message) ui_text_fixed(fb,40,550,ui_text_effective_px(UI_TEXT_CAPTION),message,EPD_DRAW_ALIGN_LEFT,false);
    char label[80];
    snprintf(label,sizeof(label),"%s%s%.32s",in->mode==UI_INPUT_CHINESE?"中文拼音":in->mode==UI_INPUT_SYMBOLS?(in->symbol_page?"数字 / 符号  2/2":"数字 / 符号  1/2"):in->uppercase?"英文大写":"英文小写",
        in->pinyin[0]?"  ·  ":"",in->pinyin);
    ui_text_fixed(fb,40,644,ui_text_effective_px(UI_TEXT_BODY),label,EPD_DRAW_ALIGN_LEFT,false);
    button(fb,PREVIOUS,"‹",false); button(fb,NEXT,"›",false);
    for (int i=0;i<5;++i) button(fb,CANDIDATE+i,
        (size_t)i<in->candidate_count?in->candidates[i]:"",i==0&&in->candidate_count);
    for (int i=0;i<40;++i) {char key[8];key_label(in,i,key);button(fb,i,key,false);}
    button(fb,LANGUAGE,in->chinese_allowed?(in->letters_mode==UI_INPUT_CHINESE?"中 / EN":"EN / 中"):"EN",false);
    button(fb,SYMBOLS,in->mode==UI_INPUT_SYMBOLS?"ABC":"123",false);
    button(fb,SPACE,"空格",false); button(fb,BACKSPACE,"删除",false); button(fb,DONE,done?done:"完成",true);
}
static void place_cursor(ui_text_input_t *in, int x) {
    size_t at=visible_start(in,field_width(in->suffix)),best=at; int left=58,distance=10000;
    do {
        int d=x>left?x-left:left-x; if(d<distance){distance=d;best=at;}
        if(!in->text[at])break;
        size_t next=next_character(in->text,at); char glyph[8];size_t n=next-at;
        if(in->secret&&!in->revealed){glyph[0]='*';glyph[1]=0;}else{memcpy(glyph,in->text+at,n);glyph[n]=0;}
        left+=ui_text_fixed_width_px(ui_text_effective_px(UI_TEXT_BODY),glyph);at=next;
    } while(left<=626);
    in->cursor=best;
}
ui_input_result_t ui_text_input_gesture(ui_text_input_t *in, const ui_gesture_event_t *ev) {
    in->cursor = safe_cursor(in);
    int id=-1;
    if(ev->type==UI_GESTURE_SWIPE_L || ev->type==UI_GESTURE_SWIPE_R){
        if(ev->y0<UI_INPUT_KEYBOARD_TOP || ev->y0>=UI_INPUT_KEYS_TOP)return UI_INPUT_NONE;
        id=ev->type==UI_GESTURE_SWIPE_L?NEXT:PREVIOUS;
    } else if(ev->type==UI_GESTURE_TAP){
        for(int i=0;i<=FIELD;++i){
            if(i==REVEAL&&!in->secret)continue;
            if(i==RAW&&(in->secret||!in->pinyin[0]))continue;
            if(ui_rect_hit(ui_text_input_key_rect(i),ev->x0,ev->y0)&&ui_rect_hit(ui_text_input_key_rect(i),ev->x,ev->y)){id=i;break;}
        }
    }
    if(id<0)return UI_INPUT_NONE;
    in->notice[0]=0;
    if(id==CANCEL)return UI_INPUT_CANCEL;
    if(id==DONE){
        if(in->pinyin[0]){snprintf(in->notice,sizeof(in->notice),"请先选择候选字，或输入原文");return UI_INPUT_CHANGED;}
        return UI_INPUT_DONE;
    }
    if(id>=CANDIDATE&&id<CANDIDATE+5)commit_candidate(in,id-CANDIDATE);
    else if(id==PREVIOUS||id==NEXT){
        if(in->mode==UI_INPUT_SYMBOLS)in->symbol_page^=1;
        else if(id==PREVIOUS&&in->candidate_page){--in->candidate_page;refresh_candidates(in);}
        else if(id==NEXT&&in->candidate_count>5){++in->candidate_page;refresh_candidates(in);}
    } else if(id==BACKSPACE){
        if(in->pinyin[0]){in->pinyin[strlen(in->pinyin)-1]=0;in->candidate_page=0;refresh_candidates(in);}
        else if(in->cursor){size_t prev=previous_character(in->text,in->cursor);memmove(in->text+prev,in->text+in->cursor,strlen(in->text+in->cursor)+1);in->cursor=prev;}
    } else if(id==CLEAR)ui_text_input_clear(in);
    else if(id==REVEAL)in->revealed=!in->revealed;
    else if(id==RAW){if(ui_text_input_insert(in,in->pinyin)){in->pinyin[0]=0;in->candidate_page=0;refresh_candidates(in);}}
    else if(id==LANGUAGE||id==SYMBOLS||id==SHIFT||id==LEFT||id==RIGHT||id==FIELD){
        if(in->pinyin[0]){snprintf(in->notice,sizeof(in->notice),"请先选择候选字，或输入原文");return UI_INPUT_CHANGED;}
        if(id==LANGUAGE){in->letters_mode=in->chinese_allowed&&in->letters_mode==UI_INPUT_ENGLISH?UI_INPUT_CHINESE:UI_INPUT_ENGLISH;in->mode=in->letters_mode;}
        else if(id==SYMBOLS)in->mode=in->mode==UI_INPUT_SYMBOLS?in->letters_mode:UI_INPUT_SYMBOLS;
        else if(id==SHIFT){in->uppercase=!in->uppercase;in->letters_mode=in->mode=UI_INPUT_ENGLISH;}
        else if(id==LEFT)in->cursor=previous_character(in->text,in->cursor);
        else if(id==RIGHT)in->cursor=next_character(in->text,in->cursor);
        else place_cursor(in,ev->x0);
    } else if(id==SPACE){
        if(in->pinyin[0]&&in->candidate_count)commit_candidate(in,0);
        else if(in->pinyin[0]){if(ui_text_input_insert(in,in->pinyin)){in->pinyin[0]=0;refresh_candidates(in);}}
        else ui_text_input_insert(in," ");
    } else if(id<40){
        char key[8];key_label(in,id,key);
        if(in->mode==UI_INPUT_CHINESE&&key[0]>='a'&&key[0]<='z'){
            size_t n=strlen(in->pinyin);
            if(n+1<sizeof(in->pinyin)){in->pinyin[n]=key[0];in->pinyin[n+1]=0;in->candidate_page=0;refresh_candidates(in);}
            else snprintf(in->notice,sizeof(in->notice),"拼音已达到长度上限");
        } else if(in->pinyin[0])snprintf(in->notice,sizeof(in->notice),"请先选择候选字，或输入原文");
        else ui_text_input_insert(in,key);
    }
    return UI_INPUT_CHANGED;
}
