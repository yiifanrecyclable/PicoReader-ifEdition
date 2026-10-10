# SPDX-License-Identifier: Apache-2.0
"""离线农历显示的边界、新年和闰月测试。 / Offline lunar bounds, new year and leap-month tests."""
from pathlib import Path
from datetime import date
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
def day(y,m,d):return (date(y,m,d)-date(1970,1,1)).days
cases=[(day(2026,10,7),2026,8,27,0,'丙午马年八月廿七'),
       (day(2026,2,17),2026,1,1,0,'丙午马年正月初一'),
       (day(2026,2,16),2025,12,29,0,'乙巳蛇年腊月廿九'),
       (day(2025,7,25),2025,6,1,1,'乙巳蛇年闰六月初一')]
with tempfile.TemporaryDirectory(dir=ROOT/'build',prefix='lunar-test-') as temp:
    work=Path(temp);source=work/'test.c';binary=work/'test'
    source.write_text('#include <assert.h>\n#include <string.h>\n#include "ui_lunar.h"\nint main(void){ui_lunar_date_t d;char s[80];\n'+''.join(
        f'assert(ui_lunar_date({a},&d));assert(d.year=={y}&&d.month=={m}&&d.day=={n}&&d.leap=={leap});assert(ui_lunar_label({a},s,sizeof(s))&&!strcmp(s,"{label}"));\n' for a,y,m,n,leap,label in cases)+
        f'assert(!ui_lunar_date({day(1900,1,30)},&d));assert(!ui_lunar_date({day(2102,1,1)},&d));assert(!ui_lunar_label({cases[0][0]},s,3));return 0;}}')
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(ROOT/'main/ui'),str(source),str(ROOT/'main/ui/ui_lunar.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
    print('PASS: offline lunar labels match new-year/date anchors, leap months, bounds and output capacity.')
