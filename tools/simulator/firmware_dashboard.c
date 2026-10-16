/* SPDX-License-Identifier: Apache-2.0
 * 中文：原版首页和手势，图书读取虚拟 SD，硬件状态为样例。
 * English: Production home and gestures over virtual SD and modeled hardware.
 */
#define app_dashboard simulator_descriptor_dashboard
#include "../../main/apps/app_dashboard.c"
#include "replay.h"
#include "app_registry.h"
extern int simulator_route;
void simulator_dashboard(uint8_t *fb,const uint32_t *days,const char *events){
 app_ctx_t ctx={.fb=fb,.now_ms=1000};on_enter(&ctx);on_tick(&ctx);on_tick(&ctx);
 s_reading_days_valid=true;memcpy(s_reading_days,days,30*sizeof(*days));
 const char *day=getenv("PICO_SIM_HOME_DAY");if(day){unsigned long value=strtoul(day,NULL,10);if(value>=19723&&value<50000)s_reading_end_day=(uint32_t)value;}
 prepare_quote_bitmap();
 simulator_replay(&ctx,on_gesture,on_key,events);render(&ctx,fb);
 if(ctx.request_app)simulator_route=app_index_of(ctx.request_app);
}

int simulator_home_geometry(int item) {
 home_geometry_t g=home_geometry();
 const int values[]={g.content_y,g.row_height,g.rows,g.grid_y,g.col_x,g.col_step,g.row_step,g.month_x,g.week_y,g.heat_title,g.recent_rule,g.recent_title};
 return item>=0 && item<(int)(sizeof(values)/sizeof(values[0]))?values[item]:0;
}
