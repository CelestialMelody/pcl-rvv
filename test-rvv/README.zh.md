# test-rvv 说明

本目录存放面向 RISC-V RVV 的独立测试与基准：各子目录自带 `Makefile`（交叉编译 + 本机 QEMU 运行或部署到板卡），不依赖 PCL 顶层 CMake 测试目标。

## 本机配置

采用 `test-rvv/mk/rvv-env.mk` 的测试会自动推导 PCL 源码目录：`PCL_SRC_DIR` 默认为 `test-rvv` 的父目录。因此仓库放在任意路径下时，一般不需要配置 PCL 源码路径。

交叉编译依赖目录 `RV_INSTALL_DIR` 默认推导为 `$(PCL_SRC_DIR)/../riscv`。这只是一种常见的兄弟目录布局约定；如果你的依赖不在该位置，请复制示例配置并填写本机路径：

```bash
cp test-rvv/config.mk.example test-rvv/config.mk
```

常用配置项：

| 变量 | 含义 |
| --- | --- |
| `PCL_SRC_DIR` | PCL 源码目录，默认自动推导为 `test-rvv` 的父目录 |
| `RV_INSTALL_DIR` | RISC-V 依赖安装目录，默认 `$(PCL_SRC_DIR)/../riscv` |
| `PCL_INSTALL_ROOT` | RISC-V 版 PCL 安装目录，默认 `$(RV_INSTALL_DIR)/pcl-rvv` |
| `CROSS_COMPILE` | 工具链前缀，默认 `riscv64-unknown-linux-gnu-` |
| `REMOTE_USER` / `REMOTE_IP` | 板卡 SSH 登录配置，仅部署到板卡时需要 |
| `BOARD_LABEL` | 板卡结果报告中的设备标签 |

如果默认路径不存在，公共环境文件会在编译前输出明确错误，提示设置 `RV_INSTALL_DIR`、`PCL_INSTALL_ROOT` 或创建 `test-rvv/config.mk`。

本机私有配置文件 `test-rvv/config.mk`、Codex/agent 工具目录不会提交到仓库。

## 顶层 `Makefile`：向板卡同步精简动态库

目标：`deploy_lib`

在本机收集交叉编译测试所需的 `.so`，`strip` 后通过 `rsync` 同步到远端板卡，便于在硬件上直接运行已部署的可执行文件（配合各子目录的 `board.mk`）：

1. 在 `./slim_lib` 中汇总（从本机 `WORKSPACE` 布局复制）：
  - `$(PCL_INSTALL_ROOT)/lib` 下 PCL 相关 `lib*.so*`
  - 依赖：`boost`、`lz4`、`hdf5`、`flann`、`libpng`、`zlib`、`gtest` 各 `lib` 目录
  - 工具链 sysroot 中的基础库：`libstdc++`、`libc`、`libm`、`libgcc_s`
2. 对 `slim_lib/*.so*` 执行 `riscv64-unknown-linux-gnu-strip -s`
3. `ssh` 在远端创建 `REMOTE_LIB_DIR`，再用 `rsync -avzP` 将 `slim_lib/` 内容同步到该目录
4. 删除本地临时目录 `./slim_lib`

### 使用前需修改的变量


| 变量               | 含义                                            |
| ---------------- | --------------------------------------------- |
| `REMOTE_USER`    | SSH 登录用户，需在 `test-rvv/config.mk` 中设置或命令行传入 |
| `REMOTE_IP`      | 板卡 IP                                         |
| `REMOTE_LIB_DIR` | 远端存放动态库的目录（默认 `~/pcl-test/lib`）               |
| `RV_INSTALL_DIR` | RISC-V 依赖安装目录，用于推导 `PCL_INSTALL_ROOT`、`RISCV_DEPS` |


工具链前缀默认是 `riscv64-unknown-linux-gnu-`；如果工具链不在 `PATH` 中，可在 `test-rvv/config.mk` 中设置 `CROSS_COMPILE=/opt/riscv64/bin/riscv64-unknown-linux-gnu-`。sysroot 由 `$(CC) -print-sysroot` 自动获取。

### 命令示例

```bash
cd test-rvv
make deploy_lib
```

依赖：ssh、rsync、交叉工具链（含 `strip`），且本机已通过 `test-rvv/config.mk` 或命令行变量配置好 PCL 与各 `RISCV_DEPS` 库路径。

## 构建模板迁移状态

当前已采用公共 topic 模板 `test-rvv/mk/rvv-topic.mk` 的目录包括。该模板内部会引入 `test-rvv/mk/rvv-env.mk` 统一管理路径、工具链、依赖目录和板卡配置：

- `filters/approximate_voxel_grid`
- `filters/bilateral`
- `filters/conditional_removal`
- `filters/convolution_3d`
- `filters/covariance_sampling`
- `filters/extract_indices`
- `filters/grid_minimum`
- `filters/radius_outlier_removal`
- `registration/correspondence_estimation_organized_projection`
- `registration/transformation_validation_euclidean`

早期测试如 `test-rvv/2d`、`test-rvv/common/*`、`test-rvv/sample_consensus/*` 仍是自包含 Makefile，后续应逐步迁移到公共模板后再统一清理路径配置。

### 与子目录 `board.mk` 的配合

板卡上运行测试时，`LD_LIBRARY_PATH` 必须包含上述同步目录。

## 其他说明

### QEMU 中 RVV 性能不如标量代码的现象

根据官方Issue [#2137 RISC-V Vector Slowdowns](https://gitlab.com/qemu-project/qemu/-/issues/2137) 以及 reddit 社区讨论，在QEMU模拟 RISC-V 架构 时，开启 RVV 向量扩展自动向量化编译的二进制程序，运行速度远慢于非向量化版本；且这类向量优化的程序在真实 RISC-V 硬件上是提速的，仅在 QEMU 模拟环境中出现严重降速。故为正常现象。（实际在物理硬件上验证是有明显提升的）
