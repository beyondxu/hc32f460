# 接收域/地址/S段与整段CRC统计（不更改喷射驱动）

分支 `codex/printer-domain-audit`，以3e7e161为基础，但硬件用户有自己当前实测1.97us的本地驱动修改。
因此部署时只取一个头文件，不切换HC32到此分支，不stash/覆盖用户的喷射函数或延时修改。
此分支唯一产品源码修改为source/printer_row_audit.h；main.c、tph_m0333n.c和tph_driver.c与基础提交相同。

```powershell
cd E:\AI_START\mcuHCF460\HC32F460_DDL_Rev3.3.0
git fetch origin
$hcAuditPath = "projects/ev_hc32f460_lqfp100_v2/examples/spi/spi_dma/source/printer_row_audit.h"
if (!(Test-Path "$hcAuditPath.before-rx-audit.bak")) { Copy-Item $hcAuditPath "$hcAuditPath.before-rx-audit.bak" }
git restore --source=origin/codex/printer-domain-audit -- $hcAuditPath
```

打开此目录对应的Keil SPI DMA工程，Rebuild all成功后下载。
原启动标记可不变，每次扫描关头后应新增一条HC32 RX_AUDIT。

```text
[HC32 RX_AUDIT] bytes=118000 crc32=... D=03 A=00000002 S=02 dots_D1_D6=895,895,0,0,0,0
```

这表示收到了地址2/S2、D1与D2各895个请求点。地址位图A按1<<(addr-1)，S按1<<(section-1)。
D为六个收到的数据位按位OR。dots_D1_D6是六域分别累计的选中位数，不是墨滴检测。
CRC32/IEEE初值FFFFFFFF、末异或FFFFFFFF，按收到的每列100字节连续计算，包括所有零列。
只计head_powered期间的数据列，不包含PCTL电源命令；故可与BK的118000字节扫描缓冲CRC比较。

统计在原TPH_FireOneColumn返回后的既有audit调用中执行，不插入CE高电平窗口，不改变输入数据。
256项查表在开始统计时初始化，增加约1KB静态RAM；每列增加少量处理开销，不宣称列间隔绝对不变。
列内不打印，关头末尾新增一条紧凑统计（测试中小于180字符）；原握手超时100ms，
新增日志在115200波特下串行传输约十余毫秒，实机仍应观察是否出现超时/取消。

本统计不能验证GPIO输出或内部加热。匹配CRC只支持排除本次BK到HC32缓冲区的内容差异。
技术支持新给出的能量参考与用户单路供电约束应一并核实，不能以CRC正常就扩大未知供电条件下的喷射次数。

四项主机回归通过：原锁存/IRQ/供电模型、实际握手循环、原行统计、独立CRC/分域审计。
新测试覆盖全零列、两地址三掩码、六域混合、原输入不被修改、reset/end/invalid行为，及M4对象编译。
本环境未完成完整Keil链接与烧录；最终运行仍使用用户本地喷射驱动版本。
