# RVV 数学函数专项文档

本目录用于记录 RVV 数学函数 helper 的实现方案、系数来源、误差证据和接入边界。这里的“数学函数 helper”指 `atan2`、`acos`、`expf`、`logf`、`sinf/cosf` 这类接近 std/libm 数学函数的 RVV 近似实现，通常会配套参数脚本、专项 C++ 测试、QEMU 正确性验证、反汇编和板卡证据。

PCL common 模块函数的完整向量化文档仍放在 `doc-rvv/common/`。例如 centroid、transforms、norms、gaussian 这类文档应继续描述 common 模块的公开入口、数据布局、分派策略、回退条件和下游测试。

如果 common 模块函数依赖数学 helper，应在 common 文档中链接到本目录的数学专项文档，不要复制数学细节。这样可以避免同一组系数、误差口径或特殊值合同在多个文档里分叉。common 文档只需要说明它如何调用 helper、输入域是否满足 helper 合同，以及下游误差或回退边界。

当前目录中的主要文档：

| 文档 | 说明 |
| --- | --- |
| `std-math-vectorization.zh.md` | std/libm 风格 RVV 数学函数向量化总则，说明语义合同、系数来源、证据矩阵、目录归属和 production gate |
| `atan2-RVV.zh.md` | `pcl::atan2_RVV_f32m2` 的范围约化、多项式核函数、象限重构和测试证据 |
| `expf-RVV.zh.md` | `pcl::expf_RVV_f32m2` 的 `ln2` 约化、多项式和 `2^n` 重构 |
| `logf-RVV.zh.md` | `pcl::logf_RVV_f32m2` 的尾数/指数拆分、`log1p` 多项式和特殊值合并 |
| `getAcuteAngle3DRVV.zh.md` | `acos_RVV_f32m2` 的数学 helper 证据，并说明它对应 `getAcuteAngle3D` 的批量锐角调用场景 |
| `remez-coeffs.zh.md` | Remez、LP/minimax、Sollya 参数脚本的统一说明 |
| `sincos-RVV.zh.md` | finite-domain `sinf/cosf` paired helper 的受限原型、证据矩阵和 production gate 状态 |

新增数学 helper 文档时，优先放在本目录，并同步 `test-rvv/rvv/math/README.zh.md` 与对应 skill 规则。
