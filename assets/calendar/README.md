# 离线农历 / Offline lunar calendar

201条农历年起点与月长记录（农历1900–2100年），固件表体1608bytes，无网络查询。公历到农历转换使用月长查表，干支生肖以农历新年为界，处理闰月。超出表范围只显示公历和星期。

来源：[6tail lunar-python](https://github.com/6tail/lunar-python)，固定MIT版本1.4.8；开发生成用源码包SHA256 `3aa11cc73c25e70ddf0ba5bdac7398c03acc9491a3aa512a91c9642973b669d6`。MIT全文见LICENSE。运行`python3 tools/generate_lunar_table.py`重新生成，输入为build/calendar-deps/lunar_python-1.4.8.tar.gz。固件不包含Python库。2026-10-07等日期核对[香港天文台公历农历对照](https://www.hko.gov.hk/tc/gts/time/calendar/pdf/files/2026.pdf)。
