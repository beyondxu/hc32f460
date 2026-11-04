# 同纸定位图接收统计（只更新HC32）

BK当前双色样张的品红三段可见，而青色左侧段不明显；青色单独测试能打印三段。
本分支 `codex/printer-row-audit` 只加入接收/函数调用统计，不更改波形、电压、脉宽或控制协议。
BK保持 `codex/printer-cmy-rows`，ALIGN_PATTERN=2、CHANNEL_MASK=3、列距3000、方向-1。
本次不改BK，不继续组合颜色或调整位置补偿。

## 更新

在Windows的HC32仓库根目录执行：

```powershell
git stash push -m "before-row-audit"
git fetch origin
git switch codex/printer-row-audit
git pull --ff-only
git log -1 --oneline
```

不要stash pop。若有其他文件冲突，保留改动，不强制覆盖。
用原 `MDK/spi_dma.uvprojx` Rebuild并烧录HC32；新文件为头文件，不需增加C源文件到工程。
两端断电重启，HC32启动应看到 `PCTLv1 ROW_AUDIT_v1 ready power=OFF`。
换新纸、恢复字车起点，用APP发送原图片触发同样的双色定位图，一次往返、不走纸。

## 正常双色任务预期

HC32在喷头ON时重置统计，在接收普通列且喷头开启时记录；在OFF且关闭电源后打印汇总。
不会在每列的计数函数中输出串口，也不修改100字节数据或握手决策。
原有首次非零列日志保留。

```text
[HC32 ROW_AUDIT] columns=1180 fired=1180 invalid=0 canceled=0 other_bits=0
[HC32 ROW_C] dots=450 bars=150,150,150 outside=0 first=359 last=908
[HC32 ROW_M] dots=450 bars=150,150,150 outside=0 first=330 last=879
[HC32 ROW_Y] dots=0 bars=0,0,0 outside=0 first=-1 last=-1
```

计数固定对应BK同纸定位图：C slot73/0x02、M slot46/0x08、Y slot15/0x20，
补偿59/30/0列，三个150点区间的逻辑起点300/500/700。
如果改了BK的行号、列距、方向或图案，bars/outside就不再可直接和上述值比较。

- columns：ON期间收到的完整普通数据帧数，不包括控制帧。
- fired：合法且已执行返回TPH_FireOneColumn的帧数，包括全零帧；不代表物理出墨成功。
- dots/bars：指定槽位的数据bit计数；invalid不为0时，不能把它们都算作已实际执行的点。
- canceled：ON期间未完成DMA就撤回请求的次数；非零时列位置参考可能已偏移。
- other_bits：除三个指定槽位bit以外收到的低六位有效bit计数。

请发完整HC32汇总、BK任务日志及纸面照片。
若每段各150且仍缺墨，下一步重点查实际输出时序、喷头端供电及出墨状态；
若计数缺失，则先查接收/缓冲/列序列。不凭fired计数声称喷嘴已经正确发火。

## 验证

主机测试覆盖双色/三色计数、主动缺失一个青色bit、非法数据、结束/重置边界及输入不被修改；
原DMA/握手/电源控制测试通过。main.c通过ARM对象编译（既有COLUMN_BYTES重定义警告仍在）。
未在本环境生成完整MDK固件或实机验证。统计也会增加少量处理开销，并非硬件逻辑分析仪测量。
