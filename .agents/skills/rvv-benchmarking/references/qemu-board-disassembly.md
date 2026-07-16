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
