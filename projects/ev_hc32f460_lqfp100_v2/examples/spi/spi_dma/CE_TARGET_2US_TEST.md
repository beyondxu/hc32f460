# CE约2.0us目标版：保留四段采样，缩短额外间隔

分支codex/printer-ce-target-2us，基于69df81d微缩版；参照原3e7e161实测约2.3us。
只更新HC32。BK保持9d445f0（2秒等待、回程W图），VL/VP不同时更改。
不是已撤回的独立CE方案：CE开启后仍依次写D/S1..S4，S4采样完成才关CE。

本版相对原约2.3us基线：
- 延续上一档四段采样后各去掉5个NOP的修改，合计名义100ns。
- 去掉CE开启后的20个额外NOP，名义100ns；CE前地址建立延时保留。
- 每段采样前从5个NOP缩为1个NOP，四段合计去掉16个NOP，名义80ns。
- 四个delay_ts_400ns完全不变，D的volatile清零/置位写入先于S低，采样期间不换数据。
- 后续D切换仍在前一S回高后，保留编译器内存屏障和原中断保护、清零及电源顺序。

按200MHz/每NOP一周期估计，总缩短约280ns，2.3us→约2.02us，作为2.0us候选。
名字不表示实测值。编译器排布、GPIO总线、波形传播会影响CE及D/S的实际边沿。
本修改不是把0.4us低脉冲缩到0.3us，也没有在固定时间到达时截断未完成的S4。
未在本环境完成板上脉宽认证，不能直接标为已验证的2.0us出墨驱动。

## 更新

```powershell
cd E:\AI_START\mcuHCF460\HC32F460_DDL_Rev3.3.0
git stash push -m before-ce-target-2us
git fetch origin
git switch codex/printer-ce-target-2us
git pull --ff-only
git log -1 --oneline
```

Keil Rebuild全部并下载，新启动标记CE_TARGET_2US_v1。不stash pop，BK不用更新。

先用此前已能抓图的空载方式确认一次：CH1 CE/PB15、CH2 D2/PA2、CH3 S1/PC15、
CH4 S4/PC14；CH2上升沿约1.5V，500ns/div。无需抢抓第四段，第一段即可看四段序列中的S1/S4脉冲。
检查CE约2.0us、S1/S4低电平仍至少0.38us、D覆盖对应S采样、S4回高先于CE关闭。
若D/S覆盖或宽度不符合，先发图，不能仅凭删NOP认为正确，不继续压缩S采样。
该确认仅为空载逻辑检查，不代表装头后供电与内部加热正确。

确认后断电恢复喷头、不接探头，用与W相同喷头/纸/清洁方式，APP发原TOP/END图触发
固定青色短线一张，标E2；不是打印APP预览整图。发近照及两端日志、一次波形。
W的2秒等待、4次往返不进纸、每段2400点保持；不添加预喷、不改变电压或重复墨点。
比较缺线/连续性，不只看浓度。清洁和喷头状态变化仍限制因果判断。

回退：保存构建变动后切回codex/printer-section-pulse/3e7e161，Rebuild下载。
不要回到c929259独立CE实验。保留69df81d可作较小缩短幅度对照。

三项主机回归通过（锁存数据/中断和电源、握手、行统计）；驱动与main通过M4对象编译，
旧TPH_COLUMN_BYTES重复定义警告保留。未完成Keil全链接与板上时序/喷墨验证。
