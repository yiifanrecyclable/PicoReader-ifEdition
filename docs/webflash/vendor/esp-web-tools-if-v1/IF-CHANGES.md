# ESP Web Tools · IF 网页适配

源自本项目保留的ESP Web Tools发行模块，原来源：https://github.com/esphome/esp-web-tools 。保留完整Apache-2.0 LICENSE；除下述安装策略外，不改变串口传输、芯片识别、地址写入、MD5校验或复位代码。

IF用户修订：在线安装只写同版引导器、分区表、应用与OTA初始数据，默认/全部入口禁用整片擦除。使用独立IF目录，不覆盖原作者的刷机资源。

修改install-dialog-im156JnI.js：两个首次安装分支直接_startInstall(false)，不进入擦除选择页；_startInstall忽略擦除参数并固定_installErase=false。IF清单new_install_improv_wait_time=0，禁用未实现的Improv探测，安装后结束到完成状态；其余代码保持原样。

The IF fork bypasses both first-install erase selectors and forces every _startInstall call to preserve unrelated flash sectors. Serial transport, chip selection, MD5 verification and reset logic are unchanged. No Improv probing is configured for this firmware.

Original dialog SHA256: 6dcfc30fb4bbf18e19a141c5eb9a694edafc5d4480b45762c221173f47effdb5
Modified dialog SHA256: 28e1cf76436a40ccc166330b3dc7ba7b53351e2583eb34cc23fc38dfb16e135c
