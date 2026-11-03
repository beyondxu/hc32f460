# 配套单色测试

此分支必须与 beyondxu/bk7258 的 `codex/printer-bounded-line` 配套使用。
保留原喷墨脉冲，新增 100 字节 PCTLv1 电源控制帧：byte0=0x80，byte1=1 开 / 2 关，
byte2..7="PCTLv1"，其余为零。普通喷嘴数据每字节仅允许低六位。
上电、自检后保持喷头关闭，只在接到合法 ON 命令后允许喷墨；非法控制帧关闭喷头。

使用原 MDK/spi_dma.uvprojx 编译烧录，无需增加 C 源文件。两个 MCU 都更新后再启用 BK 实际打印。
完整步骤见 BK 仓库项目目录的 [BOUNDED_LINE_TEST.md](https://github.com/beyondxu/bk7258/blob/codex/printer-bounded-line/sample_project-release-v3.1.1.8/bluetooth/dm_gatt_server/BOUNDED_LINE_TEST.md)。

仅供本次诊断，不要与旧 BK 固件混用。软件握手测试和 ARM 源文件对象编译通过，未实机验证。
