# Phase 010: vcompress select 消融结果

## 阶段范围

| 项 | 内容 |
| --- | --- |
| validated_scope | `PointXYZ + Normal`、`PointXYZI + Normal`，direct indexed source/normal，测试专用 `selectWithinDistanceVCompressCandidate`。 |
| unvalidated_scope | production dispatch（生产分流）、其它点型 / normal layout、`Scalar=double`、`countWithinDistance`、`getDistancesToModel` 和非 indexed 入口。 |
| phase_closeout_boundary | 本阶段只关闭 select 写回实现族消融，不关闭 production adoption（生产采纳）。 |

## 实现结果

新增 `selectWithinDistanceVCompressCandidate`，在 RVV 构建下走 `vcompress`（RVV 保序压缩）路径；不满足 RVV gate 或非 RVV 构建时回退到 `selectWithinDistanceScalarReference`。RVV helper 先按 `indices_` gather source xyz 和 normal xyz，计算 Phase 000 已验证的 final distance，再用 mask 压缩 inlier index 和 distance：index 经 `vcompress + vse32` 写回，distance 经 `vcompress + vfwcvt + vse64` 写入 `error_sqr_dists_`。

该 helper 只服务 test-rvv topic。production 文件 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 未修改。

## 执行结果

| 证据类型 | 命令 / 路径 | 结果 | 边界 |
| --- | --- | --- | --- |
| TDD RED | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_rvv` | 按预期编译失败，错误为 `selectWithinDistanceVCompressCandidate` 缺失。 | 证明新增 gtest 能捕捉候选入口缺失。 |
| QEMU correctness（QEMU 正确性） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare` | Std/RVV 各 4 个 gtest 通过。新增测试验证 `vcompress` 保序和 double error 写回。 | QEMU 不证明真实性能。 |
| 反汇编归属 | `make -C test-rvv/sample_consensus/sac_model_normal_sphere clean_bench_rvv dump_bench_rvv`；`build/asm/riscv/bench_sac_model_normal_sphere_rvv.asm` | 可见 `vcompress.vm`、`vfwcvt` / `vse64` 相关写回和 Phase 000 已有 gather / sqrt / mask 指令。 | 归属到测试专用 bench binary，不是 production helper。 |
| Board PointXYZ | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase010-pointxyz REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase010-pointxyz BENCH_ARGS='65536 200 PointXYZ'` | 板卡 gtest 4/4 通过；`vcompress` select Std/RVV speedup `3.581x`；同 RVV build 内 B/A 为 `1.256x`。 | 单次 board smoke，只支撑消融初筛。 |
| Board PointXYZI | `make -C test-rvv/sample_consensus/sac_model_normal_sphere board_smoke OUTPUT_DIR_BOARD=log/board-phase010-pointxyzi REMOTE_BOARD_OUTPUT_DIR=<REMOTE_BOARD_OUTPUT_DIR>/log/board-phase010-pointxyzi BENCH_ARGS='65536 200 PointXYZI'` | 板卡 gtest 4/4 通过；`vcompress` select Std/RVV speedup `3.084x`；同 RVV build 内 B/A 为 `1.180x`。 | `PointXYZI` 的实现族 B/A 是 weak-positive（弱正向），生产接入前需要 repeated board 或 PI 证据复核。 |
| Evidence Doctor（证据体检） | `doc/phases/010-vcompress-select-ablation/board-evidence-doctor.md` | `Errors=0`、`Warnings=10`、`Suggestions=0`；全部 Warning 为 `low_run_count`。 | 不作为强 production performance（生产性能）结论。 |
| Evidence registry（证据登记表） | `make -C test-rvv/sample_consensus/sac_model_normal_sphere record_phase010_evidence_state` 与 `evidence_status` | registry check 为 `fresh`。 | Phase 000 / Phase 010 summary、manifest、doctor 均已登记。 |

## 板卡摘要

| case | point type | baseline ms | candidate ms | B/A speedup | bucket | evidence role |
| --- | --- | ---: | ---: | ---: | --- | --- |
| scalar writeback RVV select -> `vcompress` RVV select | `PointXYZ + Normal` | 3.4133 | 2.7178 | 1.256x | positive | component ablation |
| scalar writeback RVV select -> `vcompress` RVV select | `PointXYZI + Normal` | 3.7140 | 3.1470 | 1.180x | weak_positive | component ablation |

同一日志中，`vcompress` select 相对 Std fallback 的 speedup 分别为 `3.581x` 和 `3.084x`。这些数字说明 `vcompress` 写回在当前测试专用边界内优于 Phase 000 scalar lane writeback；它不能证明真实 public entry（公开入口）已经有 RVV 分流。

## Evidence Doctor 解释

当前 Evidence Doctor 没有 Error，因此 Phase 010 可用于实现族筛选。10 个 Warning 都是 `low_run_count`：本阶段是单次 board smoke，不记录异常频率、温度 / governor / freq，也不能稳定绑定 production performance。由于 `PointXYZ` 的 B/A 为 `1.256x`，`PointXYZI` 的 B/A 为 `1.180x`，方向均未跨过 `1.0x`，本阶段不自动扩大到 5-run；但 PI1/PI4 若进入生产接入闭环，应重新跑 production boundary 的 repeated board 或至少有等价有界复跑预算。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | component ablation + production-shaped diagnostic。 |
| A/B boundary | 同一 RVV build 内，测试专用 scalar-writeback select helper 对测试专用 `vcompress` select helper。 |
| 当前决策问题 | implementation-shape：是否优先把 `vcompress` 作为后续 PI1 候选实现族。 |
| 是否可外推到 production | 不能直接外推。production 源码没有 dispatch、fallback gate、生产直连测试或生产反汇编证据。 |
| baseline mismatch 风险 | Phase 010 的实现族 B/A 使用同一 RVV build，减少了 Std/RVV 指标混用风险；但 wrapper/reduction 本来不同，因此是 component ablation，不是 strict production A/B。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | `PointXYZI` 只有 weak-positive，不阻止有界生产探针；PI1 必须冻结点型、layout、fallback 和停止条件。 |
| clean adoption 是否需要 production boundary 内证据 | 需要。必须完成 PI1-PI5，并在 PI5 暂停等待用户确认采纳或回滚。 |

## 阶段结论

Phase 010 决策为 `vcompress-select-positive-diagnostic / PI1-candidate`。推荐的生产接入候选是 `selectWithinDistance` 的 `vcompress` 写回形态；`countWithinDistance` 可沿用 Phase 000 mask count 形态作为 PI1 同组候选。`getDistancesToModel` 仍是 dense-store audit（稠密写回审计）暂缓项，不随本阶段一起接入。

下一默认动作是 `PI1 production_integration_plan`，但按仓库规则，修改 production 源码需要用户明确授权进入 production integration loop（生产接入闭环）。在授权前，本 topic 停在 `pending_user_authorization_for_PI1/PI2`，不改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp`。
