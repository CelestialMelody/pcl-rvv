# QEMU、反汇编与板卡验证

## QEMU

QEMU 用于：

- 构建验证。
- correctness 和 checksum。
- bench 日志格式检查。
- RVV 指令路径辅助证据。

QEMU 不用于判断 RVV 是否比标量更快。

## 反汇编

反汇编不只用于确认“有 RVV 指令”。还应检查：

- `vsetvli`。
- load/store、gather、stride、segment 指令。
- mask、`vcompress`、scatter。
- `vfmacc` / `vfmul` / `vfadd` 等求值结构。
- `vfcvt`、`fsrmi`、`fsrm`、`frrm` 等舍入相关指令。
- 自动向量化或规约重排是否影响标量对拍。

反汇编说明必须做热点归属，不能只列全二进制中出现过哪些指令：

- 标出关键 helper、loop 或符号附近的指令；如果符号被 inline（内联）或模板展开，说明定位依据。
- 区分当前 RVV helper 的指令、Eigen/libm/编译器自动向量化产生的指令、bench harness（性能测试外壳）或无关库代码。
- 对 FMA、规约、`vcompress`、gather/scatter 等影响性能或数值语义的指令，说明它们对应哪段源码和哪条 bench case。
- 如果负向性能可能来自某条指令模式，例如 gather、不规则访存、压缩写回或额外 store/load，文档应把这个判断写成“证据支持的假设”，并说明还需要 profile 或消融 bench 才能确认。

## 板卡或目标硬件

真实性能结论只来自目标硬件。文档应记录：

- 设备标签或目标类型。
- 数据集和参数。
- iterations。
- std/RVV 耗时和 speedup 计算方式。
- 日志相对位置。

技术文档不写 SSH 配置细节、个人路径、用户名或私有地址。板卡连接问题记录到工作日志或配置层。

## 板卡连接运维

板卡 SSH / rsync 问题属于 Makefile、workflow 或工作日志层，不进入主题技术文档。专项 Makefile 可以提供可覆盖变量：

```text
SSH_OPTS
SSH_CMD
RSYNC_SSH
```

`rsync` 应配套使用相同 SSH 配置。遇到系统 SSH 配置权限、sandbox socket 或连接问题时，先通过配置层和工作日志记录，不把它写成板卡不可用或性能验证失败。
