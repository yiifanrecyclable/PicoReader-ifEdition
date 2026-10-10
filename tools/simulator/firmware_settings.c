/* SPDX-License-Identifier: Apache-2.0
 * 中文：直接编译正式设置页；宿主禁止修改系统时钟。
 * English: Compile production settings UI; never change the host clock.
 */
#include <sys/time.h>
static int preview_settimeofday(const struct timeval *t,const void *zone){(void)t;(void)zone;return 0;}
#define settimeofday preview_settimeofday
#define app_device_settings simulator_descriptor_settings
#include "../../main/apps/app_device_settings.c"
#include "replay.h"
extern int simulator_route;
int simulator_settings(uint8_t *fb,int page,int scroll,const char *events){
 bool quote=page==23;if(quote)app_device_settings_request_home_quote();
 app_ctx_t ctx={.fb=fb,.now_ms=1000};on_enter(&ctx);if(quote)home_quote_editor_open();
 if(page==SETTINGS_SYSTEM_FONT||page==SETTINGS_QUOTE_FONT)ttf_font_scan();
 if(page==SETTINGS_WALLPAPER||page==SETTINGS_WALLPAPER_PREVIEW){wallpaper_scan();s_wallpaper_selected=s_wallpaper_count?0:-1;}
 if(page==SETTINGS_TEXT_EDIT)profile_editor_open(true);
 s_page=quote?SETTINGS_TEXT_EDIT:(page==SETTINGS_RETIRED_4||page==SETTINGS_RETIRED_15||page==SETTINGS_RETIRED_16||page==SETTINGS_RETIRED_17||page==SETTINGS_RETIRED_18)?SETTINGS_MAIN:(settings_page_t)page;s_main_scroll=scroll;
 if(s_page==SETTINGS_HOME_QUOTE)quote_preview_prepare(s_quote_px,false);
 s_year=2026;s_month=10;s_day=5;s_hour=12;s_minute=0;
 simulator_replay(&ctx,on_gesture,on_key,events);render(&ctx,fb);
 if(ctx.request_app){extern const app_desc_t app_transfer;simulator_route=ctx.request_app==&app_transfer?5:app_index_of(ctx.request_app);}
 return s_home_quote_editor&&s_page==SETTINGS_TEXT_EDIT?23:s_page;
}
