/* SPDX-License-Identifier: Apache-2.0
 * 中文：按离线月长表解算，生肖干支以农历年为界。 / English: Decode month lengths; cycle/zodiac years begin at lunar new year.
 */
#include "ui_lunar.h"
#include <stdio.h>
#include "ui_lunar_data.h"
bool ui_lunar_date(int32_t day, ui_lunar_date_t *out) {
    if (!out || day < lunar_years[0].start) return false;
    size_t n=sizeof(lunar_years)/sizeof(lunar_years[0]), year=n-1;
    while(year && lunar_years[year].start>day)--year;
    int left=day-lunar_years[year].start;
    unsigned bits=lunar_years[year].months, leap=bits>>13;
    for(unsigned slot=0;slot<12+(leap!=0);++slot){
        int length=29+((bits>>slot)&1);
        if(left<length){*out=(ui_lunar_date_t){.year=1900+(int)year,
            .month=leap&&slot>=leap?(int)slot:(int)slot+1,.day=left+1,.leap=leap&&slot==leap};return true;}
        left-=length;
    }
    return false;
}
bool ui_lunar_label(int32_t day,char *out,size_t cap){
    if(!out||!cap)return false;
    out[0]=0;
    ui_lunar_date_t d;
    if(!ui_lunar_date(day,&d))return false;
    static const char *const stems[]={"甲","乙","丙","丁","戊","己","庚","辛","壬","癸"};
    static const char *const branches[]={"子","丑","寅","卯","辰","巳","午","未","申","酉","戌","亥"};
    static const char *const animals[]={"鼠","牛","虎","兔","龙","蛇","马","羊","猴","鸡","狗","猪"};
    static const char *const months[]={"正","二","三","四","五","六","七","八","九","十","冬","腊"};
    static const char *const numbers[]={"一","二","三","四","五","六","七","八","九","十"};
    char date[16];
    if(d.day<=10)snprintf(date,sizeof(date),"初%s",numbers[d.day-1]);
    else if(d.day<20)snprintf(date,sizeof(date),"十%s",numbers[d.day-11]);
    else if(d.day==20)snprintf(date,sizeof(date),"二十");
    else if(d.day<30)snprintf(date,sizeof(date),"廿%s",numbers[d.day-21]);
    else snprintf(date,sizeof(date),"三十");
    int n=snprintf(out,cap,"%s%s%s年%s%s月%s",stems[(d.year-4)%10],branches[(d.year-4)%12],
        animals[(d.year-4)%12],d.leap?"闰":"",months[d.month-1],date);
    return n>=0&&(size_t)n<cap;
}
