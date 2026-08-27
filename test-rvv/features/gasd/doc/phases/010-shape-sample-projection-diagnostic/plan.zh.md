# Phase 010 plan: shape sample projection diagnostic

## 阶段意图和边界

本阶段把 `GASDEstimation::computeFeature` 逐样本循环中的 shape sample projection（形状样本投影）拆成 test-only diagnostic（测试专用诊断）候选。目标是验证 RVV（RISC-V Vector，可变长度向量）能否加速每个样本的 `x/y/z` 归一化坐标和距离 histogram bin（直方图 bin）计算。

本阶段不修改 production（生产源码），不写真实 histogram，不覆盖 interpolation（插值）写回，不证明 public `compute()` 已经命中 RVV，也不扩大到 color hue、`Scalar=double` 或泛型点类型 production gate。

## validated_scope / unvalidated_scope

| field | scope |
| --- | --- |
| validated_scope | `pcl::PointCloud<pcl::PointXYZ>` synthetic transformed samples、`float`、`max_coord > 0`、`distance_normalization_factor > 0`、shape projection staging buffer |
| unvalidated_scope | `addSampleToHistograms` 写回、trilinear / quadrilinear interpolation、color hue projection、public compute dispatch、其它 PointT traits、`Scalar=double` |
| phase_closeout_boundary | 只能关闭 projection staging 的 correctness、asm、board diagnostic 条目 |

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| Phase 000 copy candidate | diagnostic weak-positive | `doc/phases/000-current-state-and-gaps/result.zh.md`、`log/board/repeated_phase000_shape_copy/summary.md` |
| production source | 原始标量实现 | `features/include/pcl/features/impl/gasd.hpp` |
| test assets | 已有 include / include/impl / src / script 结构 | `test-rvv/features/gasd/` |
| board | 当前会话确认可用 | Phase 000 `board_repeated` 已完成 |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| 逐样本 `x/y/z` 归一化和 `sqrt` + fractional bin 属于比 copy 更有价值的热点 | `projectShapeSamplesRVVToBuffers` 使用 strided load、`vfmul`、`vfmadd`、`vfsqrt`、float-to-int trunc 做正数 floor | AoS（结构数组）跨步加载和 staging buffer 写回可能抵消收益 | Std/RVV 对拍、QEMU smoke、asm、board repeated |
| 将 projection 和 histogram write 分离可保留后续 interpolation 设计空间 | staging buffer：`grid_x/grid_y/grid_z/dbin` | 这仍不是 production-shaped full histogram | 在 EvidenceDecision 中保持 diagnostic 边界 |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| B1 test-first 红灯 | 在 `src/test_gasd.cpp` 加 projection 对拍测试，先引用不存在 helper | `make run_test_rvv` 编译失败 | failure 来自缺少 projection helper / 类型 |
| B2 scalar reference | `include/impl/gasd_reference.hpp` | 生成 `grid_x/grid_y/grid_z/dbin` staging buffer | 与生产公式同构，正数 floor 边界明确 |
| B3 RVV candidate | `include/impl/gasd_copy_candidate.hpp` | RVV 构建走 strided load + vector math，非 RVV fallback 到 scalar | `run_test_compare` 通过 |
| B4 bench case | `src/bench_gasd.cpp`、Makefile repeated args 可按 case 覆盖 | case `candidate_shape_projection_rvv` 输出 checksum / timing | QEMU 只作 smoke，性能等 board |
| B5 asm | `make dump_bench_rvv` | filtered asm 命中 projection 相关 RVV 指令 | 写入 result |
| B6 board repeated + Doctor | `make board_repeated GASD_REPEATED_BENCH_ARGS='--case-filter candidate_shape_projection_rvv ...'`、`make evidence_doctor_repeated` | 5-run summary + manifest + doctor | Errors=0；Warnings 解释或降级 |
| B7 文档回填 | result、matrix、roadmap、evaluation、Handoff | 当前 phase 可恢复 | 写清继续 / 停止条件 |

## Evidence Doctor 和 registry 规则

Phase 010 使用同一个 topic-local manifest wrapper（主题本地证据清单包装脚本）。若新增 projection case，wrapper 必须能识别 case metadata：evidence role 为 `diagnostic`，A/B boundary 为 `test helper`，timer boundary 为 `shape projection staging only`。当前仍没有正式 `log/evidence_registry.json`；本阶段结束时先用 manifest + artifact tracking 承担 freshness（新鲜度）检查，并把 registry 接入作为后续结构动作。

## 板卡复跑预算和决策桶

预算沿用 5 次 repeated board run，iterations=10、warmup=2。decision bucket：`positive` >= 1.10x 且无反向；`weak_positive` 为 median 1.03x-1.10x 且反向不超过 1/5；`neutral` 为 0.97x-1.03x；`negative` < 0.97x；跨桶摇摆标 `unstable`。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar projection vs RVV projection staging |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 projection 是否比 copy 更值得继续 |
| diagnostic 是否可外推到 production | no；本阶段只计算 staging buffer，不写真实 histogram |
| comparison-boundary / baseline mismatch 风险 | yes；真实 production 还包含 interpolation 写回、descriptor copy 和 object state |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但必须先完成 diagnostic-to-production mismatch audit，并且只作为后续 phase，不直接改 production |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

默认继续到 B1-B7。合法停止条件：projection helper 无法在当前工具链编译、QEMU correctness 失败且无法修复、板卡不可达、Evidence Doctor Error 无法消除或继续需要 production 授权。
