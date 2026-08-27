# Phase 030 plan: interpolation ablation

## 阶段意图和边界

本阶段把 `GASDEstimation::addSampleToHistograms` 中的 trilinear interpolation
（三线性插值）算术和索引计算拆成 test-only diagnostic（测试专用诊断）候选。目标是验证
RVV（RISC-V Vector，可变长度向量）能否加速默认 shape path（形状路径）的 `coords -= 0.5`、
`floor`、`grid_idx/h_idx` 和 8 个 spatial weights（空间权重）计算。

本阶段不写真实 `Eigen::VectorXf` histogram（直方图），不处理 scatter conflict（分散写冲突），
不覆盖 quadrilinear interpolation（四线性插值）的 hue 维度权重，不修改 production（生产源码），
也不证明 public `compute()` 已经命中 RVV。它是 interpolation write probe（插值写回探针）之前的
component ablation（组件消融）。

## validated_scope / unvalidated_scope

| field | scope |
| --- | --- |
| validated_scope | `PointXYZ` synthetic shape samples、`float`、Phase 010 projection staging、`INTERP_TRILINEAR`、dense finite samples |
| unvalidated_scope | `INTERP_NONE`、`INTERP_QUADRILINEAR`、真实 histogram writes、color interpolation、public compute dispatch、其它 PointT traits、`Scalar=double` |
| phase_closeout_boundary | 只能关闭 trilinear arithmetic / index staging 的 correctness、asm、board diagnostic 条目 |

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| Phase 010 shape projection | diagnostic positive | `doc/phases/010-shape-sample-projection-diagnostic/result.zh.md`、`log/board/repeated_phase010_shape_projection_fast/evidence_doctor.md` |
| Phase 020 color hue | diagnostic positive | `doc/phases/020-color-hue-diagnostic/result.zh.md`、`log/board/repeated_phase020_color_hue/evidence_doctor.md` |
| production interpolation source | 原始标量实现，写入 `addSampleToHistograms` 内部 | `features/include/pcl/features/impl/gasd.hpp` |
| board | 当前会话确认可用 | Phase 020 repeated board 已完成并通过 Doctor |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| trilinear 的 index + 8-weight arithmetic（算术）本身可能有 RVV 收益 | `computeTrilinearInterpolationRVVToBuffers` 从 projection staging 生成 `grid_idx/h_idx/w000..w111` | float floor / unsigned conversion 边界、8 个输出 buffer 写回可能抵消收益 | Std/RVV 对拍、QEMU smoke、asm、board repeated |
| 先不写 histogram 能隔离 scatter 前成本 | staging buffers 只保存目标 cell 和权重 | 不能证明真实 `hists[grid_idx + offset][h_idx] += weight` | EvidenceDecision 保持 diagnostic 边界，并把 histogram write probe 放入下一 phase |
| production 默认 shape interp 是 `INTERP_TRILINEAR`，优先级高于 color 默认 `INTERP_NONE` | 只覆盖 shape trilinear | 不覆盖 color quadrilinear 或用户显式设置的其它 interp | matrix 保留未验证范围 |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| D1 test-first 红灯 | 在 `src/test_gasd.cpp` 加 trilinear staging 对拍测试，先引用不存在 helper | `make run_test_rvv` 编译失败 | failure 来自缺少 interpolation helper / 类型 |
| D2 scalar reference | `include/impl/gasd_reference.hpp` | 生成 `grid_idx/h_idx/w000..w111` staging buffer | 与 production trilinear 分支同构 |
| D3 RVV candidate | `include/impl/gasd_copy_candidate.hpp` | RVV 构建走 vector floor/trunc、index arithmetic、weight arithmetic；非 RVV fallback 到 scalar | `run_test_compare` 通过 |
| D4 bench case | `src/bench_gasd.cpp`、manifest wrapper metadata | case `candidate_trilinear_interpolation_rvv` 输出 checksum / timing | QEMU 只作 smoke，性能等 board |
| D5 asm | `make dump_bench_rvv` | filtered asm 命中 interpolation helper 相关 RVV 指令 | 写入 result |
| D6 board repeated + Doctor | `make board_repeated ... --case-filter candidate_trilinear_interpolation_rvv ...`、`make evidence_doctor_repeated` | 5-run summary + manifest + doctor | Errors=0；Warnings 解释或降级 |
| D7 文档回填 | result、matrix、roadmap、evaluation、README | 当前 phase 可恢复 | 写清继续 / 停止条件 |

## Evidence Doctor 和 registry 规则

Phase 030 继续使用 topic-local manifest wrapper。wrapper 必须能识别
`candidate_trilinear_interpolation_rvv`：evidence role 为 `diagnostic`，A/B boundary 为
`test helper`，timer boundary 为 `trilinear interpolation arithmetic/index staging only`。
当前仍没有正式 `log/evidence_registry.json`；本阶段结束时先用 manifest + artifact tracking
承担 freshness（新鲜度）检查，并把 registry 接入保留为结构增强项。

## 板卡复跑预算和决策桶

预算为 5 次 repeated board run，iterations=10、warmup=2。decision bucket：
`positive` >= 1.10x 且无反向；`weak_positive` 为 median 1.03x-1.10x 且反向不超过 1/5；
`neutral` 为 0.97x-1.03x；`negative` < 0.97x；跨桶摇摆标 `unstable`。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar trilinear staging vs RVV trilinear staging |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断 interpolation arithmetic/index 是否值得继续到 histogram write probe |
| diagnostic 是否可外推到 production | no；本阶段只计算 index 和 weights，不写真实 histogram，也没有 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；真实 production 还包含 scatter writes、boundary bins、descriptor copy 和对象状态 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只能作为后续 phase，且必须先解释 scatter/write boundary 是否会吞掉收益 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

默认继续到 D1-D7。合法停止条件：interpolation helper 无法在当前工具链编译、QEMU correctness
失败且无法修复、板卡不可达、Evidence Doctor Error 无法消除、或继续需要 production 授权。
