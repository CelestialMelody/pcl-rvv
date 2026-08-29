# Phase 010 Result: public compute end-to-end evidence

## 执行范围

本阶段按计划只补 `BriskKeypoint2D::compute()` public entry（公开入口）测试和 bench（性能测试）证据；
没有继续修改 production（生产源码）实现。Phase 000 已采纳的 downsample helper 保持不变。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| public fixture | done | `include/impl/brisk_2d_downsample_reference.hpp` | 新增 synthetic organized `PointXYZRGBA` cloud 和 keypoint checksum helper。 |
| public gtest | done | `make -C test-rvv/keypoints/brisk_2d run_test_compare` | QEMU Std/RVV 均运行 4/4 gtest pass，新增 `PublicComputeRunsOnSyntheticOrganizedCloud`。 |
| public bench case | done | `src/bench_brisk_2d.cpp` | 新增 `brisk_public_compute_320x240`，checksum Std/RVV 一致。 |
| asm | done | `make -C test-rvv/keypoints/brisk_2d dump_bench_rvv` | RVV bench binary 仍包含 downsample helper 相关 RVV 指令。 |
| board repeated | done | `SSH_AUTH_SOCK=/tmp/ssh-iEjvVUej0dT1/agent.101441 make -C test-rvv/keypoints/brisk_2d collect_public_repeated_board_evidence` | 5-run 板卡采集完成，每轮 gtest 4/4 pass。 |
| Evidence Doctor / registry | done | `make -C test-rvv/keypoints/brisk_2d record_public_repeated_board_evidence_state repeated_evidence_status` | `Errors=0, Warnings=0, Suggestions=3`；registry fresh。 |

## 证据分层

- correctness（正确性）：QEMU 和 board gtest 都包含 public compute smoke，Std/RVV 构建均通过。
- QEMU path（仿真路径）：只证明构建、公开入口可运行和日志形状；不作为性能证据。
- asm attribution（反汇编归属）：`bench_brisk_2d_rvv.full.asm` 中仍有 downsample RVV 指令。public compute 本身还包含
  AGAST/OAST 标量 detector，因此不把 RVV 指令归因扩展到 detector。
- board performance（板卡性能）：`brisk_public_compute_320x240` median speedup 为 `1.017x`，0/5 反向，按本阶段
  decision bucket 归为 `neutral`。
- production boundary（生产边界）：public case 通过真实 `BriskKeypoint2D<PointXYZRGBA>::compute()` 入口触达
  `ScaleSpace::constructPyramid()`；AGAST/OAST 和 refinement 仍为标量主成本。

## repeated board 决策桶

| case | median | min | max | B/A < 1 | decision |
| --- | ---: | ---: | ---: | ---: | --- |
| `brisk_halfsample_640x480` | 1.047x | 1.032x | 1.054x | 0/5 | weak-positive / near-threshold |
| `brisk_halfsample_641x481_tail` | 1.039x | 1.035x | 1.053x | 0/5 | near-threshold weak-positive |
| `brisk_twothirdsample_640x480` | 1.108x | 1.099x | 1.112x | 0/5 | weak-positive |
| `brisk_construct_pyramid_640x480` | 1.136x | 1.122x | 1.140x | 0/5 | weak-positive |
| `brisk_public_compute_320x240` | 1.017x | 1.014x | 1.023x | 0/5 | neutral / near-threshold |

Phase 010 使用 5 run、`--iterations 30 --warmup-iterations 5`。预算已用完，decision bucket 稳定；
public compute 没有反向，但收益太接近 1.0，不能写成完整 BRISK pipeline 稳定加速。

## Evidence Doctor 和 registry

Evidence Doctor：`Errors=0, Warnings=0, Suggestions=3`。三个 suggestion 分别是两个 halfsample case 和
`brisk_public_compute_320x240` 近阈值收益。处理动作：

- Phase 000 的 downsample helper adoption 仍保留，因为 helper / construct-pyramid 边界持续正向、实现小、fallback 清楚。
- public compute 只写成 correctness 闭合和 near-neutral（接近中性）端到端影响，不写成强收益。
- 后续若要追求完整公开入口收益，应先 profile（性能剖析）或做 AGAST/OAST detector phase，而不是继续微调 downsample。

Registry：`repeated_evidence_status` 输出 fresh。登记文件为 Phase 010 manifest、doctor Markdown 和 doctor JSON。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_public` for `brisk_public_compute_320x240`；其它 helper case 为 production detail/helper chain。 |
| A/B boundary | public `BriskKeypoint2D::compute()` wrapper，Std/RVV 使用同一 synthetic organized cloud、threshold 和 octaves。 |
| 当前决策问题 | public-entry impact，不是新的 RVV-family-selection。 |
| diagnostic 是否可外推到 production | public case 是真实 production entry，但 synthetic 输入不能代表真实图像分布。 |
| comparison-boundary / baseline mismatch 风险 | checksum、iterations、warmup 和 wrapper 一致；代表性风险来自 synthetic input 与真实 BRISK workload 的差异。 |
| weak / neutral 时是否允许继续 | neutral 不回滚 helper；它说明 downsample 收益被 AGAST/OAST 等标量阶段稀释。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要，本阶段没有新 RVV family。 |

## doc suite role inventory

| role | 状态 |
| --- | --- |
| topic_navigation | standalone:`README.zh.md` |
| testing_overview | standalone:`doc/testing-overview.zh.md` |
| correctness_tests | standalone:`doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone:`doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone:`doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone:`doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone:`doc/test-support-code-map.zh.md` |
| phase_index / matrix | standalone:`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` |
| evaluation_production | standalone:`doc/brisk_2d-evaluation.zh.md` |
| production_topic_doc | standalone:`doc-rvv/keypoints/brisk_2d-RVV.zh.md` |

## continue_stop_decision

`ready_for_review`。当前 downsample helper 已采纳，public compute 证据已补齐且显示端到端收益接近中性；
继续微调 downsample 的预期收益不值得。仍有潜在方向是 AGAST/OAST detector 或真实 BRISK workload profile，
但它们需要新的 oracle / profile 和更大 production 边界，默认不在当前 downsample phase 内继续。
