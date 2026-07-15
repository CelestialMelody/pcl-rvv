# RVV 数学 helper 专项测试

本目录承载 std/libm 数学 helper 的 RVV 向量化专项资产，包括参数脚本、Sollya/LP 工具、QEMU 正确性验证、板卡运行和调用方形态冒烟测试。

`test-rvv/common/common` 只保留 common.hpp 的 bench/test；原先放在那里的 `acos`、`atan2`、`expf`、`logf`、`sinf/cosf` 数学 helper 测试已迁移到这里，避免 common 模块测试和数学专项测试双入口并存。

## 目录

| 路径 | 说明 |
| --- | --- |
| `Makefile` | 开发机参数脚本、x86/RISC-V 构建、QEMU 运行、部署 |
| `board.mk` | 板卡侧单函数运行，日志按函数写入 `output/<func>/` |
| `script/parms.py` | 通用数学常量打印脚本 |
| `script/lp_minimax.py` | 共享 LP/minimax helper |
| `script/sollya_utils.py` | 共享 Sollya 运行与解析 helper |
| `acos/` | `acos_test.cpp` 与 `script/parms_acos.py` |
| `atan2/` | `atan2_test.cpp`、参数脚本和 atan Sollya 脚本 |
| `expf/` | `expf_test.cpp`、Remez/Taylor 对比、参数脚本和 Sollya 脚本 |
| `logf/` | `logf_test.cpp`、`parms_log1p.py` 和 Sollya 脚本 |
| `sincos/` | finite-domain（有限输入域）`sinf/cosf` 实验测试、RangeImageSpherical 形态冒烟测试、参数脚本 |

构建、日志和输出按函数分桶：

```bash
build/<arch>/<func>/
output/qemu/<func>/
output/board/<func>/
log/<func>/
```

## 参数脚本

```bash
make parms
make parms_acos
make parms_atan2
make parms_expf
make parms_log1p
make parms_sincos
```

Makefile 会在本目录创建 `.venv`，并安装 `numpy`/`scipy`。Sollya 相关脚本保留在各函数 `script/` 下；默认参数报告不要求必须安装 Sollya，显式 `--run-sollya` 时才需要。

## 开发机运行

RISC-V/QEMU 默认：

```bash
make run_acos_test
make run_atan2_test
make run_expf_test
make run_expf_remez_vs_taylor
make run_logf_test
make run_sincos_test
make run_sincos_range_smoke
```

x86 smoke：

```bash
make ARCH=x86 run_sincos_test
```

全量正确性验证和冒烟测试需要显式请求：

```bash
make run_math_all
```

默认 target 只打印帮助，不会自动跑全部实验或性能测试。

## sincos 说明

`sincos/sincos_test.cpp` 验证 paired `sinf/cosf` 的 finite-domain（有限输入域）实验 helper，覆盖 bounded mask range reduction（有限域 mask 分段范围约化）、dense/adversarial（密集网格/边界对抗点）输入、特殊值，以及 scalar/RVV same-chain（标量/RVV 同构链路）。

`sincos/sincos_range_smoke.cpp` 独立覆盖 RangeImageSpherical 的调用方形态角度/距离输入。它是下游证据，不替代参数脚本、数学专项测试或标量/RVV 同构链路对拍。

`sincos/prototype_include/pcl/range_image/impl/range_image_spherical.hpp` 是仅测试使用的生产接入原型，通过测试专用头文件覆盖展示 RangeImageSpherical 的接入方式。`common/include/pcl/range_image/impl/range_image_spherical.hpp` 保持未修改；这个原型只服务 `run_sincos_range_image_spherical_integration_test`。

性能测试与正确性验证分离：

```bash
make run_sincos_test
make run_sincos_bench
make run_sincos_range_smoke
```

## 板卡

部署单函数：

```bash
make deploy_sincos_test
make deploy_sincos_range_smoke
```

板卡侧运行：

```bash
cd /root/pcl-test/rvv/math
make -f board.mk run_sincos_test
make -f board.mk run_sincos_bench
make -f board.mk run_sincos_range_smoke
```

`run_sincos_test` 只跑正确性验证；`run_sincos_bench` 才会传 `--bench`。

## 清理

```bash
make clean_acos
make clean_atan2
make clean_expf
make clean_logf
make clean_sincos
make clean_math
```
