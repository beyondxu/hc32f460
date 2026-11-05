# 分段独立使能对照版

基线 f5d53d4，分支 codex/printer-section-pulse。只更新HC32；BK保持
codex/printer-full-page-groups / 7e642a4、3170步进纸与同一TOP/END结构图。

## 为什么改

规格书M0333N Ver1.2纸面第11页：S低电平取样、高电平锁存；CE高有效。
典型图的tFire标在同一S相邻采样之间，典型2us、最大2.2us。
旧代码每个地址先拉高CE，再依次写S1..S4，S4完成立即关CE。
因此各段锁存数据在CE高期间保留的时间不同，不能把整段CE宽度当作每段的加热时间。
此差异可能造成缺线，但尚未证明它是照片中缺线的唯一原因。

## 对照实现

这不是直接复制规格书的连续CE流水时序，而是隔离每段的诊断实现：

- CE低时只锁存当前段数据，其他三段保持零。
- 开CE，用DWT核心周期计数计时，目标400周期（200MHz约2us），随后关CE。
- CE低时清除全部段锁存，再处理下一段；地址和原100字节列映射不变。
- 空段跳过，不给额外喷墨脉冲；每个源bit只在所属段发一次。
- 只在单个CE窗口屏蔽中断，随后恢复调用者的PRIMASK。
- 初始化检查200MHz与DWT运行状态，失败时不使能喷头电源、不执行喷墨。

不增加电压、不通过重复喷墨增浓。不改变字车/进纸、图像缩放或选色。
同一地址的最多24个点拆成最多4组、每组最多6个点；峰值同时使能数降低。
每列处理时间可能增加，列的位置仍由BK步进位置控制，需实测总耗时。

DWT计数并非引脚脉宽证明：计时起点在CE写入之前，轮询和GPIO总线仍有开销。
主机模型只证明事件顺序和相同计时算法，不能证明板上ns级波形或保证出墨。
Rebuild后的真实CE宽度需确认约2us且不超过规格书2.2us；不要修改周期数加浓。

## Windows更新

在仓库根目录执行。stash保存Keil输出与个人工程设置；不要stash pop到新分支。

```powershell
cd E:\AI_START\mcuHCF460\HC32F460_DDL_Rev3.3.0
git stash push -m before-section-pulse
git fetch origin
git switch codex/printer-section-pulse
git pull --ff-only
git log -1 --oneline
```

打开 projects/ev_hc32f460_lqfp100_v2/examples/spi/spi_dma/MDK/spi_dma.uvprojx，
Rebuild全部，然后下载本次生成固件；不要只烧旧hex。启动应见：

```
[TPH TIMING] SECTION_PULSE_v1 core=200000000 cycles=400 clock_ready=1 target_ns=2000 scope_required
[HC32 CONTROL] PCTLv1 ROW_AUDIT_v1 LATCH_RESET_v1 SECTION_PULSE_v1 ready power=OFF
```

clock_ready=0或TIMING ERROR时不要继续打印，发启动日志；此时ROW的fired仍只表示调用次数。

## 本次验证

先确认CE PB15脉宽，触发选CE上升沿、水平约1us/div，正常非空段是一组独立高脉冲；
S1 PC15/S2 PH1/S3 PH0/S4 PC14的数据采样在对应CE高脉冲之前。
不要求再次打印微小覆盖图。换纸、恢复起点，两端重启，APP发送同一完整结构图一次。
BK仍为10次往返/5次进纸（该结构图）；等待PAGE_DONE。保留照片和两端日志，
比较竖框、斜线、数字2/4/5及END是否恢复，不以颜色变深单独判断成功。

若需回退：保存构建产物后切回codex/printer-latch-reset（f5d53d4）并Rebuild下载。

## 自动验证

三个主机脚本测试通过：分段独占、所有100字节/6域、均等模型计时、计数溢出、空/NULL、
计时不可用时不发脉冲、锁存清零、电源顺序、中断恢复、握手和行统计。
驱动tph_m0333n.c与main.c通过Cortex-M4对象编译；保留原有TPH_COLUMN_BYTES重定义警告。
本环境未完成Keil全工程链接或板上测试。
