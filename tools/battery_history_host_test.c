/* SPDX-License-Identifier: Apache-2.0
 * 中文：真实持久化、坏槽恢复、有界时间记录；仅在临时目录运行。
 * English: Real persistence, corrupt-slot recovery and bounded time series; run only in a temporary directory.
 */
#include "battery_history.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
int html_test_fail_after = -1;
static uint32_t start=1791676800U;
static size_t count(void) {size_t n=0; battery_history_samples(&n);return n;}
int main(void) {
    battery_history_observe(1,50,3800,false,true);assert(count()==0);
    battery_history_observe(start,-1,3800,false,true);assert(count()==0);
    battery_history_observe(start,101,3800,false,true);assert(count()==0);
    battery_history_observe(start,80,3800,false,true);assert(count()==1);
    battery_history_observe(start+899,79,3790,false,true);assert(count()==1);
    battery_history_observe(start+900,78,3770,false,true);assert(count()==2);
    battery_history_reset();
    battery_history_observe(start+900,78,3770,false,true);assert(count()==2);
    battery_history_observe(start+1800,79,3800,true,true);assert(count()==3);
    // Newest slot0 is deliberately truncated; previous valid slot1 remains readable.
    FILE *f=fopen("history/battery-0.bin","wb");assert(f);fputs("bad",f);fclose(f);
    battery_history_reset();battery_history_observe(start+900,78,3770,false,true);assert(count()==2);
    battery_history_observe(start+1800,79,3800,true,true);assert(count()==3);
    size_t n=0;const battery_sample_t *s=battery_history_samples(&n);assert(s[2].charging==1);
    battery_history_observe(start+900,80,3800,false,true);assert(count()==3);
    // A missing card uses RAM only and reloads the original card without mixing sessions.
    battery_history_observe(start+2700,77,3760,false,false);assert(count()==1);
    battery_history_observe(start+1800,79,3800,true,true);assert(count()==3);
    for(unsigned i=3;i<700;++i)battery_history_observe(start+i*900,70,3700,false,true);
    s=battery_history_samples(&n);assert(n==BATTERY_HISTORY_MAX);
    assert(s[n-1].at-s[0].at<=BATTERY_HISTORY_WINDOW);
    for(size_t i=1;i<n;++i)assert(s[i].at>s[i-1].at);
    battery_history_reset();battery_history_observe(start+699*900,70,3700,false,true);assert(count()==BATTERY_HISTORY_MAX);
    // Seven days with no measurements are genuinely empty, never interpolated.
    battery_history_observe(start+699*900+BATTERY_HISTORY_WINDOW+1,60,3650,false,true);assert(count()==1);
    battery_history_reset();
    assert(unlink("history/battery-0.bin")==0);assert(unlink("history/battery-1.bin")==0);assert(rmdir("history")==0);
    f=fopen("history","wb");assert(f);fclose(f);
    battery_history_observe(start,50,3800,false,true);assert(count()==1); // storage failure still allows current session
    battery_history_reset();
    puts("PASS battery samples: persistence, invalid readings, interval, bad slot recovery, clock reversal, card changes, bounds, gaps and write failure");
}
