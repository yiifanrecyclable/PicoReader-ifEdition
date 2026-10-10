/* SPDX-License-Identifier: Apache-2.0
 * 中文：重放本地手势，以原固件回调决定页面状态。
 * English: Replay local gestures through production callbacks.
 */
#pragma once
#include "ui_gesture.h"
#include <stdio.h>
static void simulator_replay(app_ctx_t *ctx,
 app_redraw_t(*gesture)(app_ctx_t*,const ui_gesture_event_t*),
 app_redraw_t(*key)(app_ctx_t*,int),const char *events){
    bool held=false;
    for(const char *p=events;p&&*p;){
        char type;unsigned x=0,y=0,xx=0,yy=0;
        if(sscanf(p,"%c,%u,%u,%u,%u",&type,&x,&y,&xx,&yy)!=5)break;
        ctx->now_ms+=1000;
        ui_gesture_event_t e={.x0=x,.y0=y,.x=xx,.y=yy};
        if(type=='k'){if(key)key(ctx,x);}
        else if(gesture){
            if((type=='t'||type=='l')&&!held){e.type=UI_GESTURE_PRESS;gesture(ctx,&e);}
            e.type=type=='p'?UI_GESTURE_PRESS:type=='m'?UI_GESTURE_MOVE:type=='c'?UI_GESTURE_CANCEL:type=='t'?UI_GESTURE_TAP:type=='l'?UI_GESTURE_LONG_PRESS:type=='u'?UI_GESTURE_SWIPE_U:type=='d'?UI_GESTURE_SWIPE_D:type=='r'?UI_GESTURE_SWIPE_R:UI_GESTURE_SWIPE_L;
            gesture(ctx,&e);
            held=type=='p'?true:type=='m'?held:false;
        }
#ifdef SIM_REPLAY_TICK
        SIM_REPLAY_TICK(ctx);
#endif
        p=strchr(p,';');if(p)++p;
    }
}
