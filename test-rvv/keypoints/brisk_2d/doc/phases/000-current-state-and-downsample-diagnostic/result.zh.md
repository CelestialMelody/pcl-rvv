# Phase 000 Result: BRISK Downsample Production Adoption

## 执行范围

计划范围与实际范围一致：本阶段只关闭 `Layer::halfsample()`、`Layer::twothirdsample()` 和
`ScaleSpace::constructPyramid()` 的 downsample helper 链，不关闭完整 `BriskKeypoint2D::compute()`、
AGAST/OAST detector 或 descriptor 路径。

## 动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| RED test | done | `make -C test-rvv/keypoints/brisk_2d generate_board_evidence_manifest` 在脚本缺失时失败 | 缺口定位到 topic-local manifest wrapper。 |
| portable fallback | done | `keypoints/src/brisk_2d.cpp`、`run_test_compare` | 非 RVV / 非 SSSE3 路径按 portable scalar semantics 工作。 |
| RVV candidate | done | `run_test_compare`、`dump_bench_rvv` | RVV path 正确性通过，反汇编显示 `vlse8` / `vzext` / `vdivu` / store 指令。 |
| repeated board | done | `log/board/repeated-phase000-downsample/run-01..05`，摘要见 `repeated-evidence-summary.md` | `constructPyramid` median 1.175x，`twothirdsample` median 1.115x。 |
| Evidence Doctor / registry | done | `repeated-evidence-doctor.md`、`log/evidence_registry.json` | `Errors=0, Warnings=0, Suggestions=2`；registry fresh。 |
| docs / closeout | done | README、evaluation、topic-local docs、`doc-rvv/keypoints/brisk_2d-RVV.zh.md` | production patch 按用户授权采纳，文档标为 weak-positive。 |

## 证据分层

- correctness（正确性）：QEMU / board gtest 均为 3/3 pass；测试逐字节对拍 downsample reference。
- QEMU path（仿真路径）：`run_test_compare` 证明构建、运行和日志形状；不作为性能证据。
- asm attribution（反汇编归属）：`bench_brisk_2d_rvv.full.asm` 中 downsample helper 附近出现 `vlse8`、
  `vzext`、`vdivu`、`vse8`、`vsse8`。
- board performance（板卡性能）：5-run repeated board 在 Milkv-Jupiter 上给出 weak-positive 桶。
- production boundary（生产边界）：真实 production helper 已修改；bench 通过 `brisk::Layer` 和
  `ScaleSpace::constructPyramid` 触达，不是纯 test-only helper。

## repeated board 决策桶

| case | median | min | max | B/A < 1 | decision |
| --- | ---: | ---: | ---: | ---: | --- |
| `brisk_halfsample_640x480` | 1.048x | 1.017x | 1.088x | 0/5 | weak-positive / near-threshold |
| `brisk_halfsample_641x481_tail` | 1.041x | 1.000x | 1.051x | 0/5 | weak-positive / near-threshold |
| `brisk_twothirdsample_640x480` | 1.115x | 1.105x | 1.140x | 0/5 | weak-positive |
| `brisk_construct_pyramid_640x480` | 1.175x | 1.125x | 1.200x | 0/5 | weak-positive |

本阶段不追加无界复跑。预算 5 run 已用完，decision bucket 稳定；halfsample 的近阈值 suggestion 已在
evaluation 和 `doc-rvv` 中降级说明。

## Evidence Doctor 和 registry

Evidence Doctor：`Errors=0, Warnings=0, Suggestions=2`。两个 suggestion 均为 halfsample 近阈值收益。
处理动作：保留为 weak-positive，不把 halfsample 单项写成强加速；采纳理由来自组合 `constructPyramid`
收益、实现简单、fallback 清楚和用户 prompt override。

Registry：`make -C test-rvv/keypoints/brisk_2d repeated_evidence_status` 输出 fresh。登记文件包括 manifest、
doctor Markdown 和 doctor JSON。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_direct`；生产源码 helper 已修改，bench 通过真实 `brisk::Layer` / `ScaleSpace` 触达。 |
| A/B boundary | production detail helper / ScaleSpace helper chain。 |
| 当前决策问题 | RVV-vs-scalar。当前只有一个 RVV family，不做 RVV-vs-RVV family selection。 |
| diagnostic 是否可外推到 production | 当前不是 test-only diagnostic；可用于 downsample helper production adoption。不能外推到完整 keypoint pipeline。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 两侧使用同一 bench wrapper、case、iterations 和 checksum policy；完整 `compute()` 仍未覆盖。 |
| weak / neutral 时是否允许 bounded production probe | 本轮用户授权板卡有收益即可采纳；当前结果为 weak-positive 且无反向。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要，因为不是多 RVV family 选择。若后续尝试 packed/shuffle family，则需要同边界 A/B。 |

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

`ready_for_review`。当前 roadmap 和 matrix 没有本 topic 授权范围内、高优先级且未阻塞的下一动作。
完整 `BriskKeypoint2D::compute()` 和 AGAST/OAST detector 是明确未覆盖范围；它们需要新的 oracle / profile
和 phase plan，不阻塞 downsample helper 的 adopted closeout。
