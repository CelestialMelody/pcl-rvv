# Phase 040 plan: histogram write probe

## 阶段意图和边界

本阶段把 Phase 030 的 trilinear interpolation arithmetic / index staging（三线性插值算术 / 索引暂存）
继续推进到 histogram write probe（直方图写回探针）。目标是验证在加入
`hists[grid_idx + offset][h_idx] += weight` 写回之后，RVV（RISC-V Vector，可变长度向量）暂存带来的
算术 / 索引收益是否仍然可见。

本阶段仍不修改 production（生产源码），也不实现不安全的 vector scatter accumulate（向量分散累加）。
RVV candidate 只负责计算 `grid_idx/h_idx/w000..w111` staging buffer（暂存缓冲区），写回阶段使用和标量
baseline 相同的 scalar accumulation（标量累加）。这样可以隔离写回边界成本，同时避免同一个 histogram
cell 被多个向量 lane（向量通道）同时更新时破坏加法语义。

## validated_scope / unvalidated_scope

| field | scope |
| --- | --- |
| validated_scope | `PointXYZ` synthetic shape samples、`float`、Phase 010 projection staging、`INTERP_TRILINEAR`、flat histogram grid（扁平直方图网格）、dense finite samples |
| unvalidated_scope | `Eigen::VectorXf` 真实容器布局、`INTERP_NONE`、`INTERP_QUADRILINEAR`、color interpolation、public compute dispatch、其它 PointT traits、`Scalar=double` |
| phase_closeout_boundary | 只能关闭 trilinear staging + scalar write probe 的 correctness、asm、board diagnostic 条目 |

## 当前状态清单

| area | state | evidence |
| --- | --- | --- |
| Phase 030 trilinear arithmetic / index staging | diagnostic positive，存在 long-tail warning | `doc/phases/030-interpolation-ablation/result.zh.md`、`log/board/repeated_phase030_trilinear_interpolation/evidence_doctor.md` |
| production write source | `addSampleToHistograms` 对 8 个相邻 cell 的同一 `h_idx` 做 `+=` | `features/include/pcl/features/impl/gasd.hpp` |
| histogram container | production 使用 `std::vector<Eigen::VectorXf>`；本阶段用 flat vector 保持 offset 语义但不证明 Eigen heap layout | 本 plan 的 validated / unvalidated scope |
| board | 当前会话确认可用 | Phase 030 repeated board 已完成 |

## 假设与候选族

| hypothesis | candidate | risk | validation |
| --- | --- | --- | --- |
| Phase 030 的算术 / 索引收益在加入 8 次写回后仍可能为正 | `accumulateTrilinearHistogramRVVStaged`：RVV staging + scalar write | staging buffer 写回和 histogram 清零可能吞掉收益 | same-chain correctness、QEMU smoke、asm、board repeated |
| 不做 vector scatter accumulate 可以先避免冲突语义风险 | 写回循环保持标量 `+=` | 不能证明真正全向量化 histogram write | EvidenceDecision 保持 diagnostic 边界，后续 production-shaped combined path 单独评估 |
| flat histogram 能让 offset 和 checksum 更容易审查 | `std::vector<float>` 布局为 `cell * (hists_size + 2) + h_idx` | 不能覆盖 `Eigen::VectorXf` per-cell allocation、cache locality 和 allocator 成本 | result 中明确 mismatch audit |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| D1 test-first 红灯 | 在 `src/test_gasd.cpp` 增加 histogram write probe 对拍测试，先引用不存在 helper | `make run_test_rvv` 编译失败 | failure 来自缺少 histogram write probe helper / 类型 |
| D2 scalar reference | `include/impl/gasd_reference.hpp` | 标量路径从 projection 直接计算 bin、8 个权重并写 flat histogram | 与 production trilinear 写回公式同构 |
| D3 RVV staged candidate | `include/impl/gasd_copy_candidate.hpp` | RVV path 复用 Phase 030 staging，再用同一 scalar write helper 写 flat histogram | `run_test_compare` 通过 |
| D4 bench case | `src/bench_gasd.cpp`、manifest wrapper metadata | case `candidate_trilinear_histogram_write_rvv` 输出 checksum / timing | QEMU 只作 smoke，性能等 board |
| D5 asm | `make dump_bench_rvv` | RVV bench asm 命中 staging helper 指令；写回仍为 scalar boundary | 写入 result |
| D6 board repeated + Doctor | `make board_repeated ... --case-filter candidate_trilinear_histogram_write_rvv ...`、`make evidence_doctor_repeated` | 5-run summary + manifest + doctor | Errors=0；Warnings 解释或降级 |
| D7 文档回填 | result、matrix、roadmap、evaluation、README | 当前 phase 可恢复 | 写清继续 / 停止条件 |

## Evidence Doctor 和 registry 规则

本阶段继续使用 topic-local manifest wrapper。wrapper 必须识别
`candidate_trilinear_histogram_write_rvv`：evidence role 为 `diagnostic`，A/B boundary 为 `test helper`，
timer boundary 为 `trilinear interpolation staging plus scalar flat-histogram accumulation`。当前仍没有正式
`log/evidence_registry.json`；本阶段结束时先用 manifest + artifact tracking 承担 freshness（新鲜度）
检查，并把 registry 接入保留为结构增强项。

## 板卡复跑预算和决策桶

预算为 5 次 repeated board run，iterations=10、warmup=2。decision bucket：
`positive` >= 1.10x 且无反向；`weak_positive` 为 median 1.03x-1.10x 且反向不超过 1/5；
`neutral` 为 0.97x-1.03x；`negative` < 0.97x；跨桶摇摆标 `unstable`。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper：scalar trilinear compute+write vs RVV staging+scalar write |
| 当前决策问题 | RVV-vs-scalar diagnostic；判断写回加入后是否仍值得继续到 production-shaped combined path |
| diagnostic 是否可外推到 production | no；flat histogram 不是 `Eigen::VectorXf`，也没有 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes；真实 production 还有 `Eigen::VectorXf` per-cell allocation、descriptor copy、shape_samples_ 对象状态和 public wrapper |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但必须先解释 flat-vs-Eigen mismatch；负向时不能直接写 no-production |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

默认继续到 D1-D7。合法停止条件：helper 无法在当前工具链编译、QEMU correctness 失败且无法修复、
板卡不可达、Evidence Doctor Error 无法消除、或继续需要 production 授权。
