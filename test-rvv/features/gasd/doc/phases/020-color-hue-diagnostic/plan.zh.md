# Phase 020 plan: color hue diagnostic

## 阶段意图和边界

本阶段把 `GASDColorEstimation::computeFeature` 中逐样本 color hue（颜色色相）计算拆成
test-only diagnostic（测试专用诊断）候选。目标是验证 RVV（RISC-V Vector，可变长度向量）
能否加速 `PointXYZRGBA` 样本的 `max/min/diff_inv/isfinite` 分支、`hue` 计算和 `hbin`
映射。

本阶段不修改 production（生产源码），不写真实 color histogram（颜色直方图），不覆盖
`addSampleToHistograms` 的 interpolation（插值）写回，不证明 public `compute()` 已经命中
RVV，也不扩大到泛型颜色点类型、`Scalar=double` 或 production dispatch（生产分流）。

## validated_scope / unvalidated_scope

| field | scope |
| --- | --- |
| validated_scope | `pcl::PointCloud<pcl::PointXYZRGBA>` synthetic color samples、`float`、dense finite coordinates、RGB hue / hbin staging buffer |
| unvalidated_scope | color histogram 写回、quadrilinear interpolation、public color compute dispatch、其它 RGB/RGBA layout、泛型 PointT traits、`Scalar=double` |
| phase_closeout_boundary | 只能关闭 hue / hbin staging 的 correctness、asm、board diagnostic 条目 |

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| Phase 000 copy candidate | diagnostic weak-positive | `doc/phases/000-current-state-and-gaps/result.zh.md`、`log/board/repeated_phase000_shape_copy/summary.md` |
| Phase 010 shape projection | diagnostic positive | `doc/phases/010-shape-sample-projection-diagnostic/result.zh.md`、`log/board/repeated_phase010_shape_projection_fast/evidence_doctor.md` |
| production color source | 原始标量实现，hue 分支在 `GASDColorEstimation::computeFeature` 内逐样本执行 | `features/include/pcl/features/impl/gasd.hpp` |
| test assets | 已有 `include/impl` helper、`src/test_gasd.cpp`、`src/bench_gasd.cpp`、topic-local manifest wrapper | `test-rvv/features/gasd/` |
| board | 当前会话确认可用 | Phase 010 fast repeated board 已完成并通过 Doctor |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| hue 分支包含逐样本 max/min、除法、分支选择和负值修正，可能是 color path 的可向量化片段 | `projectColorHueRVVToBuffers` 输出 `hue/hbin` staging buffer | RGB 是 AoS 字节字段，离散 byte load、mask 分支和整数转 float 可能抵消收益 | Std/RVV 对拍、QEMU smoke、asm、board repeated |
| grayscale（灰度，`max == min`）在 production 中通过 `diff_inv=inf` 和 `std::isfinite` 保持 `hue=0` | 标量 reference 和 RVV candidate 都显式覆盖 grayscale lane | RVV 若直接除以 0 后继续参与分支，可能产生 NaN 或错误 hbin | 边界测试覆盖 grayscale、R/G/B 为最大值和负 hue 修正 |
| 只测 hue / hbin staging 可隔离 RGB 分支成本 | staging buffer：`hue/hbin` | 仍不是 production-shaped full color histogram | EvidenceDecision 保持 diagnostic 边界 |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| C1 test-first 红灯 | 在 `src/test_gasd.cpp` 加 hue / hbin 对拍测试，先引用不存在 helper | `make run_test_rvv` 编译失败 | failure 来自缺少 color hue helper / 类型 |
| C2 scalar reference | `include/impl/gasd_reference.hpp` | 生成 `hue/hbin` staging buffer | 与 production 标量公式同构，覆盖 grayscale 和三种 max-channel 分支 |
| C3 RVV candidate | `include/impl/gasd_copy_candidate.hpp` | RVV 构建走 byte stride load、vector max/min、mask 分支和 float hbin；非 RVV fallback 到 scalar | `run_test_compare` 通过 |
| C4 bench case | `src/bench_gasd.cpp`、manifest wrapper metadata | case `candidate_color_hue_rvv` 输出 checksum / timing | QEMU 只作 smoke，性能等 board |
| C5 asm | `make dump_bench_rvv` | filtered asm 命中 hue helper 相关 RVV 指令 | 写入 result |
| C6 board repeated + Doctor | `make board_repeated ... --case-filter candidate_color_hue_rvv ...`、`make evidence_doctor_repeated` | 5-run summary + manifest + doctor | Errors=0；Warnings 解释或降级 |
| C7 文档回填 | result、matrix、roadmap、evaluation、README | 当前 phase 可恢复 | 写清继续 / 停止条件 |

## Evidence Doctor 和 registry 规则

Phase 020 继续使用 topic-local manifest wrapper。wrapper 必须能识别
`candidate_color_hue_rvv`：evidence role 为 `diagnostic`，A/B boundary 为 `test helper`，
timer boundary 为 `color hue and hbin staging only`。当前仍没有正式
`log/evidence_registry.json`；本阶段结束时先用 manifest + artifact tracking 承担 freshness
检查，并把 registry 接入保留为结构增强项。

## 板卡复跑预算和决策桶

预算为 5 次 repeated board run，iterations=10、warmup=2。decision bucket：
`positive` >= 1.10x 且无反向；`weak_positive` 为 median 1.03x-1.10x 且反向不超过 1/5；
`neutral` 为 0.97x-1.03x；`negative` < 0.97x；跨桶摇摆标 `unstable`。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar hue / hbin staging vs RVV hue / hbin staging |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 color hue 是否值得继续到 color histogram probe |
| diagnostic 是否可外推到 production | no；本阶段只计算 hue / hbin staging，不写真实 color histogram，也没有 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；真实 production 还包含 shape compute、color interpolation 写回、descriptor copy 和对象状态 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只允许作为后续 phase，且必须先补更接近 production 的 color histogram / public boundary 证据 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

默认继续到 C1-C7。合法停止条件：hue helper 无法在当前工具链编译、QEMU correctness 失败且无法修复、
板卡不可达、Evidence Doctor Error 无法消除、或继续需要 production 授权。
