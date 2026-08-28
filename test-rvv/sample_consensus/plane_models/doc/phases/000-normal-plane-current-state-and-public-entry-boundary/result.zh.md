# Normal-plane 当前状态与公开入口边界 Phase Result

## 结论

本阶段关闭为 `positive`。`SampleConsensusModelNormalPlane<PointXYZ, Normal>` 的三条已有 RVV helper 仍能被公开入口命中；不满足 curvature float layout（曲率字段为 float 的布局条件）的注册 normal 类型会回退到标量公开入口；直接调用 select helper 时，空输出缓冲区不会再写越界。

本结论只覆盖 `PointXYZ + Normal`、`indices_` 有序索引表、`threshold` 接口为 `double` 但 RVV 内部用 `float` 计算的当前生产边界。它不外推到未注册点类型、非 float curvature、自定义 AoS（结构数组）布局全集、非法索引或新的 RVV 实现族选择。

## 本阶段改动

| 类型 | 路径 | 内容 |
| --- | --- | --- |
| production | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` | `selectWithinDistanceStandard` 和 `selectWithinDistanceRVV` 在直接 helper 调用时补齐 `inliers` 与 `error_sqr_dists_` 的最小 resize，修复空缓冲区写回合同。 |
| test | `test-rvv/sample_consensus/plane_models/src/test_sample_consensus_plane_models.cpp` | 新增公开入口命中 RVV、unsupported curvature layout 回退标量、select helper 空缓冲区 resize 三个测试；新增 `NormalWithDoubleCurvature` 注册点类型作为 fallback case。 |
| evidence | `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json` | 记录本轮 QEMU、board、asm 证据边界，供 Evidence Doctor 消费。 |
| evidence | `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md` | Evidence Doctor 结果为 Errors=0、Warnings=0、Suggestions=0。 |

## RED / GREEN 过程

| 步骤 | 命令 | 结果 | 说明 |
| --- | --- | --- | --- |
| 既有基线 | `make -C test-rvv/sample_consensus/plane_models run_test_rvv` | 19 tests passed | 改动前已有测试通过。 |
| RED | `make -C test-rvv/sample_consensus/plane_models run_test_rvv TEST_ARGS="$(pwd)/test-rvv/sample_consensus/plane_models/pcd/sac_plane_test.pcd --gtest_filter=SampleConsensusModelNormalPlane.SelectHelperResizesEmptyOutputBuffers"` | QEMU segmentation fault | 直接 helper 调用时输出缓冲区为空，写回合同不安全。 |
| GREEN | 同上 | passed | production resize 修复后，直接 helper 调用不再越界。 |
| 新增覆盖 | `make -C test-rvv/sample_consensus/plane_models run_test_rvv TEST_ARGS="$(pwd)/test-rvv/sample_consensus/plane_models/pcd/sac_plane_test.pcd --gtest_filter=SampleConsensusModelNormalPlane.PublicEntriesMatchDirectRVVForSupportedLayout:SampleConsensusModelNormalPlane.PublicEntriesFallbackForUnregisteredNormalLayout:SampleConsensusModelNormalPlane.SelectHelperResizesEmptyOutputBuffers"` | 3 tests passed | 公开入口 RVV 命中、fallback、helper 缓冲区三项闭合。 |

## 正确性证据

| 证据 | 命令 | 结果 | 路径 |
| --- | --- | --- | --- |
| QEMU Std/RVV compare | `make -C test-rvv/sample_consensus/plane_models run_test_compare` | Std build 22 tests passed；RVV build 22 tests passed | `test-rvv/sample_consensus/plane_models/log/qemu/run_test_std.log`、`test-rvv/sample_consensus/plane_models/log/qemu/run_test_rvv.log` |
| Board unit test | `make -C test-rvv/sample_consensus/plane_models run_board_test fetch_board_logs` | 22 tests passed；phase 010 后默认使用板卡侧 PCD fixture | `test-rvv/sample_consensus/plane_models/log/board/run_test.log` |

QEMU 结果只作为 correctness（正确性）和日志形状证据；性能结论以下面的 board compare 为准。

## 性能与反汇编证据

| 证据 | 命令 | 结果 | 路径 |
| --- | --- | --- | --- |
| RVV asm dump | `make -C test-rvv/sample_consensus/plane_models dump_bench_rvv` | `selectWithinDistanceRVV`、`countWithinDistanceRVV`、`getDistancesToModelRVV` 均可定位 RVV 指令 | `test-rvv/sample_consensus/plane_models/build/asm/riscv/bench_sac_normal_plane_rvv.full.asm`、`test-rvv/sample_consensus/plane_models/build/asm/riscv/bench_sac_normal_plane_rvv.asm` |
| Board compare | `make -C test-rvv/sample_consensus/plane_models run_board_bench_compare fetch_board_logs` | 三项均为 positive；phase 010 已修正默认远端 PCD 参数，不再需要手工 override | `test-rvv/sample_consensus/plane_models/log/board/analyze_bench_compare.log` |

Board compare 结果：

| benchmark item | Std Avg | RVV Avg | speedup |
| --- | ---: | ---: | ---: |
| `selectWithinDistance` | 0.6845 ms | 0.0699 ms | 9.79x |
| `countWithinDistance` | 0.6797 ms | 0.0590 ms | 11.52x |
| `getDistancesToModel` | 0.7702 ms | 0.0732 ms | 10.52x |

目标符号内 RVV 指令计数：

| symbol | RVV instruction lines |
| --- | ---: |
| `SampleConsensusModelNormalPlane<PointXYZ, Normal>::selectWithinDistanceRVV` | 56 |
| `SampleConsensusModelNormalPlane<PointXYZ, Normal>::countWithinDistanceRVV` | 77 |
| `SampleConsensusModelNormalPlane<PointXYZ, Normal>::getDistancesToModelRVV` | 56 |

## Evidence Doctor

| 输入 | 输出 | 结果 |
| --- | --- | --- |
| `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json` | `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md`、`.json` | Errors=0，Warnings=0，Suggestions=0 |

本阶段的 manifest 把 board benchmark 标为 `production_shaped_diagnostic`：bench 直接调用 protected helper，因此它证明的是 hot path（热点路径）性能，不单独证明公开入口分流。公开入口分流和 fallback 由 QEMU / board unit test 证明。

## 工具 / 环境问题

首次执行 `run_board_bench_compare fetch_board_logs` 失败，原因是 host 侧共享规则把 `BENCH_ARGS` 的本机绝对 PCD 路径传到板卡。topic 的 `board.mk` 已定义正确的 `REMOTE_PCD_FILE`，但共享规则覆盖了 board 侧默认值。phase 010 已在 topic Makefile 中给 board target 设置 board-side PCD 参数；当前日志来自无需手工 override 的默认 board run。

板卡日志包含 clock skew warning（板卡脚本时间戳相对主机在未来）。本轮 benchmark 与 unit test 都重新构建并重新部署二进制；该 warning 作为环境记录保留，不改变 positive bucket。

## EvidenceDecision

`EvidenceDecision = production patch retained / phase 000 positive / continue next unblocked phase`

理由：

- 公开入口和直接 RVV helper 的输出在支持布局上匹配。
- unsupported curvature layout case 在 RVV build 下走标量公开入口并保持输出一致。
- helper 空输出缓冲区的真实 RED 已被 production resize 修复，并由 QEMU 与板卡测试覆盖。
- board performance 三项均远高于 1.2x positive 阈值。
- Evidence Doctor 无 error / warning。

## 未闭合项

| 项 | 状态 | 为什么未闭合 | 下一步 |
| --- | --- | --- | --- |
| `plane_models` test support structure | `closed in phase 010` | 测试 / bench 源码已迁入 `src/`，README 和 topic-local role docs 已补齐。 | 后续只按新 phase result 恢复。 |
| board harness remote fixture path | `closed in phase 010` | phase 010 已给 board targets 设置 board-side PCD 参数，当前 board compare / board test 均无需手工 override。 | 若后续要推广到共享规则，另开 harness cleanup topic。 |
| evidence registry / manifest alias | `closed in phase 020` | topic-local wrapper、doctor target、registry record/check 已验证，`evidence_status` 为 fresh。 | 默认下一阶段转向 repeated board summary。 |
| generic PointT / PointNT 扩展 | `deferred` | 当前证据只覆盖 `PointXYZ + Normal` 和注册 fallback case，不覆盖全部点类型布局。 | 若用户要扩大 coverage，另建 point-type expansion phase，先补 trait/fallback 编译测试和 board evidence。 |

默认恢复动作：进入 `030-normal-plane-repeated-board-summary`。
