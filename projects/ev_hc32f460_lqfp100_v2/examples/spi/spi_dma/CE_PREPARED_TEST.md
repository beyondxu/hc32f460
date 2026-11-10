# 按技术支持澄清优化CE窗口：预计算数据，不额外插A/D/S等待

分支 `codex/printer-ce-prepared`，基于54465b4接收审计版本。
用户当前共同电压15V，当前旧函数CE实测1.97us。此版为候选优化，不宣称已达到1.6us。
支持所说A/Section/Data无需额外间隔，不能解释为S低电平可以短于规格0.38us。

## 改动

- 每个地址四段原数据的读取、&0x3F、GPIO置位/清零查表全部移到CE低时完成。
- 预计算字的寄存器约束和编译屏障防止编译器把查表推回CE高窗口。
- 地址设置和首段D电平预置在CE低时完成，之后DMB，再开启CE。
- S1..S4四段仍在CE高内逐段完整采样，S4_HIGH之后才CE_LOW。
- 活动路径不再调用额外的delay_100ns/delay_40ns；四个delay_ts_400ns调用保留。
- 调用者的PRIMASK保存/恢复、CE低清零、真实地址清零、地址/列末静置与供电函数保持。
- 只用GPIO置位/清零寄存器和数据掩码，保留其他PA引脚，不写整口输出状态。

这里仅预置首段D电平，所有S采样仍在CE高内；不是先在CE低装载全部S再单独开CE的旧方案。
“无需间隔”指取消额外等待，不等于所有GPIO口物理同时翻转，端口写入仍按顺序执行。
预计算数据仍可能在目标编译器中有寄存器拆包或栈读取，不能把C源码长度当作脉宽。

## 部署：只替换用户本地的一个函数

用户有本地延时修改，不能直接切换此HC32分支或用仓库整份tph_m0333n.c覆盖本地文件。
[完整新函数文本](source/tph_fire_prepared.inc)只含一个函数和注释，与本分支生产函数测试一致。
全局delay_ts_400ns/delay_40ns等函数保留用户本地定义；原接收统计头文件也保留。

在Windows PowerShell，逐行执行：

```powershell
cd E:\AI_START\mcuHCF460\HC32F460_DDL_Rev3.3.0
git fetch origin
$firePath = "projects/ev_hc32f460_lqfp100_v2/examples/spi/spi_dma/source/tph_m0333n.c"
if (!(Test-Path "$firePath.before-prepared-ce.bak")) { Copy-Item $firePath "$firePath.before-prepared-ce.bak" }
git show origin/codex/printer-ce-prepared:projects/ev_hc32f460_lqfp100_v2/examples/spi/spi_dma/source/tph_fire_prepared.inc | Set-Clipboard
```

这些命令完成备份并把新函数放到剪贴板，还没有改写实际C文件。
在Keil打开上述仓库路径的tph_m0333n.c，以剪贴板中的完整函数替换原TPH_FireOneColumn定义。
其他函数及全局延时原样保留；不要另外#include这个inc文件。
保存后执行Rebuild all成功再下载，随后整机重启完成BK/HC握手。
本函数不增加串口打印，源码开头注释标记TPH_PREPARED_CE_v1；不以旧启动行判断列函数是否更新。
也可在浏览器打开上述完整函数链接复制，不依赖剪贴板命令。

## 验证顺序

1. 按用户之前可用的空载逻辑测量方式复核CE及S低电平，重点含当前测试用S2及收尾S4。
   S采样低电平必须仍至少0.38us，S4完成先于CE关闭；清零阶段也使用同一采样函数。
   函数名字/NOP注释不能保证实际宽度，编译布局、总线和GPIO寄存器调度均会影响测量。
   若任一采样不满足，则先调整/核实采样实现，不直接装头继续打印。
   空载测量只能初查逻辑时序，不能证明带头电压和热量正常。
2. 记录实际CE，不要求它一定为1.6us。确认采样后恢复打印，保持共同15V。
3. BK用已发布同地址D1/D2/03测试（c9d764a），不同时换图形、供电或清洁条件。
   接收审计已安装则继续核对RX_AUDIT与BK缓冲CRC。
4. 发新函数实测CE/S宽度、同地址纸面及日志。此版同时改变CE窗口与D/CE相对先后，
   即使出墨恢复，也不能仅据此证明先前一定是能量过高；继续按矩阵判断。

当前四段依次采样的规格下界之和为1.52us，尚有GPIO/CE边沿开销，软件不能不顾这些条件保证1.6us。
本版不通过截断S4或缩短S采样达到目标，也不更改仍有充分余量的单喷嘴请求节奏。

## 回退

在PowerShell复制备份恢复C文件：

```powershell
Copy-Item "$firePath.before-prepared-ce.bak" $firePath -Force
```

Keil Rebuild all、下载并整机重启。已安装的接收审计头文件不受此恢复影响。

## 已验证的范围

五项HC主机测试通过。锁存模型执行实际新函数和真实GPIO查表，模拟置位/清零寄存器，
覆盖六个单域、100字节混合位、末段/末地址、全零/NULL、输入不变、无多余锁存位、IRQ恢复及供电顺序。
模型启用地址/未定义行为检查；不依赖其时间模拟判断纳秒宽度。
-O2和-Os两档Cortex-M4对象编译通过；检查CE窗口有完整四段采样NOP块、没有函数调用，
原数据/查表访问位于窗口之前。Keil可能生成不同机器码，仍需本机测量。
函数文本与实际C函数一致，源码审查确认其他延时/清零/供电/查表函数字节未改。
本环境未完成完整Keil链接、烧录或物理时序/出墨验证。
