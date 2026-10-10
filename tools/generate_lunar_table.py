# SPDX-License-Identifier: Apache-2.0
"""生成离线农历月长表（MIT 6tail lunar_python 1.4.8）。 / Generate a compact offline lunar-month table."""
from pathlib import Path
from datetime import date
import hashlib, sys, tarfile, tempfile
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'build/calendar-deps/lunar_python-1.4.8.tar.gz'
SOURCE_SHA256="3aa11cc73c25e70ddf0ba5bdac7398c03acc9491a3aa512a91c9642973b669d6"
if hashlib.sha256(SOURCE.read_bytes()).hexdigest()!=SOURCE_SHA256:raise ValueError("Pinned lunar source digest mismatch")
with tempfile.TemporaryDirectory() as tmp:
    with tarfile.open(SOURCE) as archive: archive.extractall(tmp,filter='data')
    package=next(Path(tmp).glob('lunar_python-*'))
    sys.path.insert(0,str(package))
    from lunar_python import LunarYear, Solar
    records=[]
    for year in range(1900,2101):
        months=[m for m in LunarYear.fromYear(year).getMonths() if m.getYear()==year]
        assert len(months) in (12,13)
        solar=Solar.fromJulianDay(months[0].getFirstJulianDay())
        start=(date(solar.getYear(),solar.getMonth(),solar.getDay())-date(1970,1,1)).days
        bits=0;leap=0
        for i,m in enumerate(months):
            assert m.getDayCount() in (29,30)
            bits|=(m.getDayCount()==30)<<i
            if m.getMonth()<0:leap=-m.getMonth()
        records.append((start,bits|(leap<<13)))
    out=ROOT/'main/ui/ui_lunar_data.h'
    out.write_text('/* SPDX-License-Identifier: MIT\n * 生成数据，勿手改。 / Generated data; do not edit.\n * Derived from 6tail lunar_python 1.4.8; assets/calendar/LICENSE.\n */\nstatic const struct { int32_t start; uint32_t months; } lunar_years[] = {\n'+''.join(f'    {{{a},0x{b:x}}},\n' for a,b in records)+'};\n')
    target=ROOT/'assets/calendar';target.mkdir(parents=True,exist_ok=True)
    license=next(package.glob('LICENSE*'));(target/'LICENSE').write_bytes(license.read_bytes())
    (target/'README.md').write_text('# 离线农历 / Offline lunar calendar\n\n201条农历年起点与月长记录（农历1900–2100年），固件表体1608bytes，无网络查询。公历到农历转换使用月长查表，干支生肖以农历新年为界，处理闰月。超出表范围只显示公历和星期。\n\n来源：[6tail lunar-python](https://github.com/6tail/lunar-python)，固定MIT版本1.4.8；开发生成用源码包SHA256 `'+hashlib.sha256(SOURCE.read_bytes()).hexdigest()+'`。MIT全文见LICENSE。运行`python3 tools/generate_lunar_table.py`重新生成，输入为build/calendar-deps/lunar_python-1.4.8.tar.gz。固件不包含Python库。2026-10-07等日期核对[香港天文台公历农历对照](https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2026.pdf)。\n')
    print('Generated 201 lunar records / 1608 data bytes')
