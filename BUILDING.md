# Pico Reader IF Edition 对应源码

对应版本0.3.3-rc83-if.8，项目Read_Pico_IF，基于Pico Reader rc83。包括应用、修改后的epdiy、固定第三方组件、内建字体和实际构建配置，可修改并重新编译、链接。许可见LICENSE、THIRD_PARTY_NOTICES.md及组件许可证。

## 构建和重新链接

按Espressif官方指南安装ESP-IDF及其工具链，使用SDK提交fff9895c82d744c7237be8847347bdd1b07c6643（v6.1开发树）：https://github.com/espressif/esp-idf/tree/fff9895c82d744c7237be8847347bdd1b07c6643 。执行SDK安装脚本及export脚本后，在本目录执行：

```sh
idf.py -B build -D SDKCONFIG=sdkconfig.release build
```

源码在main/、components/、managed_components/，依赖固定于dependencies.lock。另需联网安装SDK及工具链。改动epdiy或应用后使用相同命令重新编译链接；依照新生成build/flash_args写入自己的四段产物，不要以原发布应用替代重新链接结果。此构建不以安全启动或Flash加密限制用户替换。构建时间会影响二进制摘要，不要求逐字节复现。sdkconfig.defaults为产品默认；CI配置只供编译检查，不替代板级时序。

额外TF标准字体不链接进应用，按assets/fonts/README.md下载准备，或使用独立资源包；此源码附内建实际使用字库。

主体Apache-2.0，epdiy LGPL-3.0-or-later，其他组件依各自许可。原作者：https://wegooo-cell.github.io/read-pico-reader/ ，原项目：https://github.com/wegooo-cell/read-pico-reader 。IF为独立衍生版本。

发布应用SHA256：cfc4639b227a5dce9bba131e7d96abc1ee13ff6a893609648306e0c0ada472f3。
