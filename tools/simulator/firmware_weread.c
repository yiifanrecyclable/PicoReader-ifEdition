/* SPDX-License-Identifier: Apache-2.0
 * 中文：使用正式微读页面和明确标注的演示快照，不请求账号或云端内容。
 * English: Production WeRead UI with labeled demo snapshots; no account or cloud requests.
 */
#include "weread_service.h"
#include "app_registry.h"
#include <stdio.h>
#include <string.h>
static weread_snapshot_t preview_weread;
bool weread_configure(const char *cache,const char *books){return true;}
bool weread_start(weread_action_t action,unsigned page,unsigned index){
 preview_weread.action=action;preview_weread.page=page;preview_weread.revision++;
 if(action==WEREAD_LOGOUT){memset(&preview_weread,0,sizeof(preview_weread));return true;}
 if(action==WEREAD_SYNC&&!preview_weread.logged_in){preview_weread.state=WEREAD_QR;preview_weread.active=true;snprintf(preview_weread.qr,sizeof(preview_weread.qr),"https://weread.qq.com/web/confirm?uid=IF-DEMO&code=NOT-A-LOGIN");}
 else if(action==WEREAD_DOWNLOAD){preview_weread.state=WEREAD_FAILED;preview_weread.error=101;}
 return true;
}
bool weread_start_batch(unsigned page,const weread_selection_t *selection,unsigned count){preview_weread.state=WEREAD_FAILED;preview_weread.error=101;return true;}
void weread_snapshot(weread_snapshot_t *out){*out=preview_weread;}
bool weread_set_include_images(bool enabled){return true;}
void weread_stop(void){preview_weread.active=false;preview_weread.state=WEREAD_CANCELLED;}
#define app_weread simulator_descriptor_weread
#include "../../main/apps/app_weread.c"
#include "replay.h"
extern int simulator_route;
void simulator_weread(uint8_t *fb,int mode,const char *events){
 memset(&preview_weread,0,sizeof(preview_weread));
 if(mode==1){preview_weread.logged_in=true;preview_weread.state=WEREAD_COMPLETE;preview_weread.total=preview_weread.count=3;
  for(unsigned i=0;i<3;i++){snprintf(preview_weread.books[i].id,sizeof(preview_weread.books[i].id),"if-demo-%u",i);snprintf(preview_weread.books[i].title,sizeof(preview_weread.books[i].title),"演示书籍 %u",i+1);snprintf(preview_weread.books[i].author,sizeof(preview_weread.books[i].author),"Pico Reader IF Edition");}
 }
 app_ctx_t ctx={.fb=fb,.now_ms=1000};on_enter(&ctx);
 snprintf(s_message,sizeof(s_message),"演示模式：请在 Pico 真机登录与下载");
 simulator_replay(&ctx,on_gesture,on_key,events);refresh_snapshot();render(&ctx,fb);
 if(ctx.request_app)simulator_route=app_index_of(ctx.request_app);
}
