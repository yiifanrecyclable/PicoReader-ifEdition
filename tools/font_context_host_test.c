/* SPDX-License-Identifier: Apache-2.0
 * 两套真实字体的缓存、切换和释放回归。/ Regression for real font-context reuse, switching and release.
 */
#include <assert.h>
#include "../main/font/ttf_font.c"
const char *app_settings_fonts_dir(void){return "/missing";}
const char *app_settings_font_path(void){return TTF_FONT_BUILTIN;}
static uint8_t frame[684*1216/2],reference[684*1216/2];
static void sample(void){
    memset(frame,255,sizeof(frame));
    ttf_draw_text_px(frame,36,120,48,"阅读 Aa 0123",EPD_DRAW_ALIGN_LEFT,0,15);
}
int main(int argc,char **argv){
    assert(argc==2);
    assert(ttf_font_select_context(false,TTF_FONT_BUILTIN)==ESP_OK);
    sample();memcpy(reference,frame,sizeof(frame));
    int width=ttf_text_width_px(48,"阅读 Aa 0123");
    assert(ttf_font_select_context(true,argv[1])==ESP_OK);
    sample();unsigned loads=ttf_sim_font_loads;
    assert(s_font_contexts[1].state_glyf_ram==NULL&&s_font_contexts[1].state_gvar_ram==NULL);
    for(int i=0;i<100;++i){
        assert(ttf_font_select_context(false,TTF_FONT_BUILTIN)==ESP_OK);
        assert(ttf_text_width_px(48,"阅读 Aa 0123")==width);
        sample();assert(!memcmp(reference,frame,sizeof(frame)));
        assert(ttf_font_select_context(true,argv[1])==ESP_OK);sample();
    }
    assert(ttf_sim_font_loads==loads);
    assert(s_font_contexts[0].state_cache_bytes<=1280u*1024&&s_font_contexts[1].state_cache_bytes<=256u*1024);
    unsigned before_clear=ttf_sim_font_loads;
    assert(ttf_font_cache_clear_all()>0);
    assert(s_font_contexts[0].state_font_ready&&s_font_contexts[1].state_font_ready);
    assert(!s_font_contexts[0].state_cache_bytes&&!s_font_contexts[1].state_cache_bytes);
    assert(ttf_font_select_context(false,TTF_FONT_BUILTIN)==ESP_OK);sample();
    assert(!memcmp(reference,frame,sizeof(frame))&&ttf_sim_font_loads==before_clear);
    // 替换或打开失败只影响选中的槽；另一个字体及字形保持有效。/ Replacing or failing a face preserves the other slot.
    ttf_font_select_context(true,"/missing-font.ttf");
    assert(ttf_font_select_context(false,TTF_FONT_BUILTIN)==ESP_OK);
    sample();assert(!memcmp(reference,frame,sizeof(frame)));
    ttf_font_open_builtin();
    assert(s_font_contexts[1].state_font_ready);
    ttf_font_unload();ttf_font_unload();
    for(unsigned i=0;i<2;++i){
        ttf_context_t *ctx=&s_font_contexts[i];
        assert(!ctx->state_font_ready&&ctx->state_font_fd==-1&&!ctx->state_font_mem);
        assert(!ctx->state_font_data&&!ctx->state_file_loca&&!ctx->state_cache_bytes);
        assert(!ctx->state_io_data&&!ctx->state_io_run_buf&&!ctx->state_glyf_ram&&!ctx->state_gvar_ram);
    }
    assert(ttf_font_select_context(false,TTF_FONT_BUILTIN)==ESP_OK);sample();
    assert(!memcmp(reference,frame,sizeof(frame)));
    ttf_font_unload();
    puts("font contexts: 200 cached switches, stable glyphs, unmapped UI face, failure isolation and unload/reload passed");
    return 0;
}
