# Phase 000 Result: base-plane select/getDistances production

## 执行范围

本阶段按 `plan.zh.md` 修改 `SampleConsensusModelPlane` 的三个基础平面距离入口。生产补丁已经把 `selectWithinDistance`、`countWithinDistance` 和 `getDistancesToModel` 分成 Standard helper（标量 helper）与 RVV helper（RVV 辅助函数），并在 `__RVV10__`、`RVVXYZAoSFloatLayout<PointT>` 和 32-bit byte offset gate（32 位字节偏移准入条件）成立时进入 RVV 路径。

阶段没有扩大到 normal-plane、sphere/circle、SAC 方法后处理、`Scalar=double` 或不满足 registered single-float xyz traits 的点型。`PointXYZI` 已补 QEMU correctness，用于证明 traits gate 不只覆盖 exact `PointXYZ`；其它 PointXYZ-like 点型仍需要独立扩展证据。

## 计划动作回填

| action | status | evidence | 结论 |
| --- | --- | --- | --- |
| 写 Std/RVV helper 分层 | done | `sample_consensus/include/pcl/sample_consensus/sac_model_plane.h`；`sample_consensus/include/pcl/sample_consensus/impl/sac_model_plane.hpp` | 公开入口只做输入检查、RVV dispatch（分流）和 Standard fallback（标量回退）。 |
| 新增 independent test harness | done | `src/test_sac_model_plane.cpp`；`make -C test-rvv/sample_consensus/sac_model_plane run_test_compare` | Phase 000 当时 Std/RVV 两个构建各 3 个测试通过；Phase 025 后当前套件已扩展为 7 个测试。 |
| 新增 bench harness | done | `src/bench_sac_model_plane.cpp`；`make -C test-rvv/sample_consensus/sac_model_plane USE_PCL_RVV10=0 TARGET_BENCH=bench_sac_model_plane_std build/riscv/bench_sac_model_plane_std`；`dump_bench_rvv` | Std/RVV bench 二进制均可编译；没有运行 QEMU bench compare。 |
| QEMU correctness | done | `log/qemu/run_test_std.log`；`log/qemu/run_test_rvv.log` | QEMU 证明 correctness 和日志形状，不证明性能。 |
| asm attribution | done | `build/asm/riscv/bench_sac_model_plane_rvv.full.asm`；`build/asm/riscv/bench_sac_model_plane_rvv.asm` | 三个目标 RVV helper 附近可定位 `vluxei32`、`vfmacc`、`vfabs`、`vmflt`、`vcpop`；select helper 另有 `vcompress` 和压缩写回。 |
| board evidence | done | `SSH_AUTH_SOCK=<agent-socket> make -C test-rvv/sample_consensus/sac_model_plane board_smoke`；`log/board/repeated/run-01..run-05/` | 5-run repeated board（重复板卡测试）均通过 board gtest，三个 public entry 的 Std/RVV 对比均为 positive。 |
| Evidence Doctor | done with suggestions | `make -C test-rvv/sample_consensus/sac_model_plane generate_board_repeated_evidence_manifest`；`doc/phases/000-base-plane-select-distance-production/evidence-doctor.md` | JSON manifest 生成后 doctor 报告 Errors=0、Warnings=0、Suggestions=6；剩余建议是环境字段和二进制身份。 |

## 正确性证据

`run_test_compare` 在 Phase 000 当时的 Std 和 RVV 构建中各运行 3 个测试；Phase 025 后当前套件已扩展为 7 个测试。Phase 000 覆盖项为：

| TEST | 覆盖内容 | 证据边界 |
| --- | --- | --- |
| `PublicEntriesMatchDirectRVVForSupportedLayout` | `PointXYZ`、乱序 `indices_`、`selectWithinDistance` / `countWithinDistance` / `getDistancesToModel` 公开入口和 RVV helper 对拍。 | 证明 direct indexed cloud 下的公开入口输出和 helper 输出一致。 |
| `PointXYZILayoutMatchesStandardPath` | `PointXYZI` registered single-float xyz layout。 | 证明当前 traits gate 可覆盖一个非 exact `PointXYZ` 点型；不证明其它点型的性能。 |
| `SelectHelperResizesEmptyOutputBuffers` | helper 直接调用时 `inliers` 和 `error_sqr_dists_` 为空的写回合同。 | 证明 helper 不依赖 public entry 预先 reserve/resize。 |

## 反汇编归属

`dump_bench_rvv` 生成的 full asm 中有明确符号边界：

| helper | 关键指令 | 位置 |
| --- | --- | --- |
| `countWithinDistanceRVV` | `vluxei32.v`、`vfmacc.vv`、`vfabs.v`、`vmflt.vf`、`vcpop.m` | `build/asm/riscv/bench_sac_model_plane_rvv.full.asm` |
| `getDistancesToModelRVV` | `vluxei32.v`、`vfmacc.vv`、`vfabs.v`、`vfwcvt.f.f.v`、`vse64.v` | `build/asm/riscv/bench_sac_model_plane_rvv.full.asm` |
| `selectWithinDistanceRVV` | `vluxei32.v`、`vfmacc.vv`、`vfabs.v`、`vmflt.vf`、`vcpop.m`、`vcompress.vm`、`vse32.v`、`vse64.v` | `build/asm/riscv/bench_sac_model_plane_rvv.full.asm` |

## diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | correctness 是 production-public（真实公开入口）证据，asm 是 production-detail（生产内部 helper 归属）证据，5-run board performance 已作为 production-public 性能证据闭合。 |
| A/B boundary | 公开入口和 protected helper 均来自生产 header；bench A/B 是 Std/RVV 两个构建的 public overload，在同一输入、iteration、warmup 和 checksum 边界内比较。 |
| 当前决策问题 | Phase 000 覆盖范围内的 RVV-vs-scalar 性能已闭合为 positive；用户确认板卡真实接入收益可采纳后，当前 production patch 已保留。 |
| diagnostic 是否可外推到 production | 当前不依赖 diagnostic 外推；生产补丁已由 public entry correctness、asm 和 board direct 证据共同验证。 |
| comparison-boundary / baseline mismatch 风险 | 当前 5-run board 使用同一 synthetic dataset、65536 点、200 次 iteration、5 次 warmup 和 checksum `65740`；剩余风险是缺少 taskset、governor、freq、temperature 和 binary hash 元数据。 |
| weak / negative / neutral / unstable 时是否允许 bounded production probe | 允许；当前 patch 已从 bounded production probe（有界生产探针）升级为 adopted production behavior。若后续新 fast path 出现 weak / negative / neutral / unstable，必须在独立 phase 中决定是否撤回该新 fast path。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 本阶段是 Std/RVV public 入口接入，不做新 RVV family selection；若后续改写 select/getDistances 的实现族，再补同边界 RVV-vs-RVV A/B。 |

## Board performance 与 Evidence Doctor

板卡证据来自 `log/board/repeated/run-01..run-05/` 的 5-run repeated board。每轮 bench 输入都是 `PointXYZ`、65536 点、direct indexed `indices_`、200 次 iteration 和 5 次 warmup；Std/RVV checksum 均为 `65740`。`B/A = Std_ms / RVV_ms`，大于 1 表示 RVV 更快。

| public entry | B/A values | median | min | max | decision bucket |
| --- | --- | --- | --- | --- | --- |
| `selectWithinDistance` | 3.1746, 3.2461, 3.1965, 3.2748, 3.1928 | 3.1965x | 3.1746x | 3.2748x | positive |
| `countWithinDistance` | 1.6673, 1.6768, 1.6678, 1.6861, 1.6662 | 1.6678x | 1.6662x | 1.6861x | positive |
| `getDistancesToModel` | 2.3726, 2.3211, 2.3695, 2.1138, 2.3943 | 2.3695x | 2.1138x | 2.3943x | positive |

Evidence Doctor 结果在 `evidence-doctor.md`，输入 manifest 是 `log/board/evidence_manifest.json`。报告为 Errors=0、Warnings=0、Suggestions=6。剩余建议是缺少 taskset、governor、freq、temperature 和 binary hash（等价二进制身份）字段；这些不阻塞当前 positive 桶，但 PI5 Handoff 必须说明环境 metadata（元数据）边界。

`evidence_registry_status=manifest_available_registry_not_available`。本阶段新增 topic-local manifest generator（当前主题本地证据清单生成器）`script/generate_board_evidence_manifest.py` 和 Makefile target `generate_board_repeated_evidence_manifest`。完整 registry 仍未接入；raw repeated logs 默认留在本地，不进入提交边界，除非用户明确要求提交脱敏日志。

## Continue / Stop Decision

`current_decision=adopted_production_behavior`。当前 production public（真实公开入口）证据支持保留本阶段 production patch：QEMU correctness、反汇编归属、5-run board performance 和 Evidence Doctor 均未发现阻塞项。用户确认板卡真实接入测试有收益即可采纳后，`doc-rvv/sample_consensus/sac_model_plane-RVV.zh.md` 成为适用的长期 production 文档。

`next_phase_default`：进入 `010-identity-index-strided-load`，先通过 bench case 拆分和证据计划验证 identity indices 是否值得新增 strided load 快速路径。若该方向证据不足或退化，保留 Phase 000 采纳实现并停止当前优化方向。

## 后续范围

| item | status | 恢复条件 |
| --- | --- | --- |
| board production evidence | done | 5-run repeated board positive，Evidence Doctor 无 Error/Warning。 |
| `PointXYZRGB/RGBA` 等点型扩展 | `adopted for representative correctness` | Phase 020 已补代表点型 correctness；dedicated board performance 不外推。 |
| evidence registry / manifest | manifest done, registry deferred | 当前 manifest 可供 doctor 复核；完整 registry 可在采纳后或提交前补。 |
| topic-local doc suite | done | 已补 README、testing overview、correctness、benchmark/evidence、optimization evidence 和 test-support code map。 |
