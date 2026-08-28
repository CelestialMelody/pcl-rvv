# Phase 025 Plan: empty indices correctness gap

## 阶段意图和边界

本阶段只关闭显式空 `indices_` 的 correctness（正确性）缺口。已有
`CloudOnlyIdentityIndicesMatchStandardPath` 和 `AdditionalAoSPointTypesIdentityMatchStandardPath`
使用空参数表示“不调用 `setIndices`”，实际覆盖的是默认整云 identity indices（恒等索引），不是显式空子集。

本阶段不修改 production（生产源码）RVV 实现，不新增性能结论，不扩大到其它 SAC 模型、其它 row source
policy（行来源策略）或 dedicated point-type performance（点型专用性能）。

## 当前状态清单

| area | 当前状态 | 路径 |
| --- | --- | --- |
| production RVV | Phase 000/010 已采纳；select/count 有 identity strided load，getDistances 保持 gather-only。 | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` |
| correctness suite | Phase 025 开始前 Std/RVV 各 6 个 gtest；缺显式空 `indices_` case。 | `test-rvv/sample_consensus/sac_model_plane/src/test_sac_model_plane.cpp` |
| board smoke | Phase 020 板卡 RVV 6/6 通过，但不含显式空 `indices_`。 | `run_board_base_plane_public_tests` |
| docs | Phase 025 开始前 README、correctness、evaluation、matrix、`doc-rvv` 仍记录 6 个 gtest。 | `test-rvv/sample_consensus/sac_model_plane/doc/` |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| explicit empty indices public entry | direct indexed cloud with empty subset | `PointXYZ` / `Eigen::VectorXf` / registered float xyz AoS | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` | 新增 gtest 后 `run_test_compare`；板卡 RVV smoke | not_applicable | 若板卡可用，重跑 `run_board_base_plane_public_tests` | not_applicable：无新 RVV 指令需求 | not_applicable：correctness-only | planned |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 新增显式空 `indices_` gtest | `src/test_sac_model_plane.cpp` | public entry 输出空 inliers、count 为 0、distances 为空，并与 Standard helper 一致。 |
| 更新 public filter | `Makefile` 的 `BASE_PLANE_PUBLIC_FILTER` | 板卡 public correctness alias 会运行新增 case。 |
| 刷新文档计数和脱敏命令 | topic README、correctness、testing overview、evaluation、matrix、phase result、Handoff、`doc-rvv` | 所有当前 suite 计数改为 7，板卡命令使用 `<agent-socket>/<board-user>/<board-ip>` 占位符。 |
| 验证 | `run_test_compare`、板卡 RVV smoke、`dump_bench_rvv`、`git diff --check` | 无 correctness failure；反汇编仍证明 getDistances gather-only。 |

## Evidence Doctor 和 registry 规则

本阶段不新增 repeated board performance（重复板卡性能）数据，不生成新的 performance manifest。
若 correctness 或 asm 验证改变当前结论，相关文档必须刷新；否则 Phase 000/010 的 Evidence Doctor 结果继续作为性能证据。

## 板卡复跑预算和决策桶

板卡只跑一次 correctness smoke（小型正确性验证），决策桶为 pass / fail。若板卡不可达，保留本地
Std/RVV correctness 并把板卡项标为 blocked；当前会话已确认板卡可用，因此默认执行。

## 继续 / 停止条件

显式空 `indices_` case 在 QEMU Std/RVV 和板卡 RVV smoke 中通过后，本阶段可关闭为
`adopted_for_correctness`. 若新增测试失败，先判断是否是 production 语义缺陷；只有确认需要时才改
production。若本阶段结束后 roadmap 和 matrix 仍无新的未阻塞 RVV performance candidate，则暂停性能搜索，
只保留 evidence metadata / registry hardening 作为提交或归档增强。
