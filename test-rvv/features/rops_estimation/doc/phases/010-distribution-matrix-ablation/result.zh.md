# Phase 010 Result: distribution-matrix-ablation

## 当前结论

Phase 010 已完成 `ROPSEstimation::getDistributionMatrix()` 的 test-only RVV candidate（测试专用 RVV 候选）验证。`getDistributionMatrixRVV()` 在 QEMU（仿真器）Std/RVV 两种构建下与 production private helper oracle（生产私有 helper 参考真值）一致；板卡 5-run repeated component bench（重复组件性能测试）显示 positive diagnostic（正向诊断）信号。

本阶段仍不修改 production（生产源码）。原因是候选计时包含 row/col staging（行列暂存）和标量 scatter（标量累加），且完整 RoPS 路径还包含 LRF、rotateCloud、mesh local surface、central moments 和 descriptor normalization。该结果支持继续做 rotate/projection + AABB 消融，不能单独证明 production-ready。

## 计划动作回填

| action | status | evidence | notes |
| --- | --- | --- | --- |
| A1 RED test | done | `make -C test-rvv/features/rops_estimation run_test_rvv` first failed with missing `rops::getDistributionMatrixRVV` | failure 来自缺少 candidate symbol |
| A2 helper implementation | done | `include/impl/rops_components.hpp` | RVV build 使用 `vlse32` 跨步加载 AoS xyz，批量计算 row/col；scatter 保持标量 |
| A3 projection coverage | done | `src/test_rops_estimation.cpp` | 覆盖 XY / XZ / YZ projection 和 max-bin clamp；修正为 RTZ（round toward zero，向 0 截断）以匹配 C++ cast |
| A4 asm attribution | done | `build/asm/riscv/test_rops_estimation_rvv.full.asm`、`build/asm/riscv/bench_rops_estimation_rvv.full.asm` | `getDistributionMatrixRVV` 中出现 `vlse32.v` / `vfsub.vf` / `vfmul.vf` / `vfcvt.rtz.xu.f.v` / `vminu.vv` / `vmerge.vvm` / `vse32.v` |
| A5 bench decision | done | `src/bench_rops_estimation.cpp`、`log/board/phase010_distribution_matrix_repeated/summary.md` | 追加 component bench；板卡 repeated 结果 positive，但只作为 diagnostic |

## 验证结果

| command | result | evidence path | role |
| --- | --- | --- | --- |
| `make -C test-rvv/features/rops_estimation run_test_compare` | pass：Std 4/4，RVV 4/4 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness 和日志形状 |
| `make -C test-rvv/features/rops_estimation dump_test_rvv` | pass：生成 test asm | `build/asm/riscv/test_rops_estimation_rvv.asm`、`.full.asm` | correctness binary asm attribution |
| `make -C test-rvv/features/rops_estimation dump_bench_rvv` | pass：生成 bench asm | `build/asm/riscv/bench_rops_estimation_rvv.asm`、`.full.asm` | bench binary asm attribution |
| QEMU bench smoke：`run_bench_std` / `run_bench_rvv` with `--points 1024 --iterations 1 --warmup 0 --repeat 1` | pass；checksum match | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log` | qemu_smoke_only，不作为性能结论 |
| `make -C test-rvv/features/rops_estimation board_smoke ...` | pass；board test 4/4；single smoke 1.39x；checksum match | `log/board/analyze_bench_compare.log` | board smoke |
| 5-run repeated board collect | pass；median 1.440x，min 1.420x，max 1.460x，0/5 `B/A < 1`，checksum match | `log/board/phase010_distribution_matrix_repeated/summary.md` | diagnostic component performance |
| `make -C test-rvv/features/rops_estimation doctor_phase010_board_summary` | pass；0 Error / 0 Warning / 2 Suggestion | `log/board/phase010_distribution_matrix_repeated/evidence_doctor.md` | Evidence Doctor |

## Evidence Doctor 和 registry

Evidence Doctor 输入为 `log/board/phase010_distribution_matrix_repeated/evidence_manifest.json`，输出为 `log/board/phase010_distribution_matrix_repeated/evidence_doctor.md`。结果：0 Error、0 Warning、2 Suggestion。

Suggestions：

- `environment_metadata_missing`：缺少 taskset、governor、freq、temperature。当前 5-run 无方向反转或长尾，结论可保留为 diagnostic；后续 production-shaped 或 production-direct 证据应补环境字段。
- `binary_identity_missing`：缺少 binary hash。当前每轮通过 Makefile 重新部署同名二进制且 checksum 一致，作为 component diagnostic 可接受；若后续出现方向反转，应补二进制身份或清理后重跑。

`log/evidence_registry.json` 尚未创建；本阶段 summary / manifest / doctor 是 topic-local 生成证据，默认按 summary-only 策略处理，raw logs 不默认提交。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic component correctness + diagnostic component board performance |
| A/B boundary | same bench wrapper；Std build scalar fallback vs RVV build test-only `getDistributionMatrixRVV()` |
| 当前决策问题 | implementation-shape；distribution matrix binning 是否值得继续进入 combined / production-shaped probe |
| diagnostic 是否可外推到 production | no。它不覆盖 complete `ROPSEstimation::computeFeature()`、LRF、rotateCloud、mesh local surface、central moments 或 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes。当前 helper 显式分配 row/col staging buffer；production 若接入可能需要不同 buffer 生命周期，且 scalar scatter 仍保留 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for this component alone；positive 时也只允许作为 combined probe 的前置线索 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段没有 production patch，也没有完整 public entry 证据 |

## Phase scope 与未验证范围

`validated_scope`：`pcl::PointXYZ` / float / AoS synthetic finite rotated local cloud，XY / XZ / YZ projection，5 bins，QEMU Std/RVV correctness，bench binary asm attribution，板卡 repeated component diagnostic。

`unvalidated_scope`：完整 ROPS descriptor、LRF、rotateCloud + AABB、descriptor normalization、其它点型、真实 production dispatch、production buffer 生命周期、不同 bins 和真实 mesh local surface 分布。

`point_type_expansion_queue`：本阶段不扩点型；若进入 production probe，再为 `PointXYZI`、`PointXYZRGBA`、`PointNormal` 或 PointXYZ-like traits 单独建 phase。

## 阶段反思和下一步

distribution matrix binning 的板卡信号稳定正向，说明批量 row/col 计算值得保留为候选。但 scalar scatter 与 staging 让它不适合单独接入 production。下一阶段默认进入 `020-rotate-projection-ablation`：验证 `rotateCloud()` / projection 前置旋转和 AABB min/max 是否能与 Phase 010 的 binning 候选形成更接近 production 的 combined path。

## Artifact tracking

本阶段新增 / 更新的 topic-local 源码、脚本和文档位于 `test-rvv/features/rops_estimation/**`。`build/` 和 raw `log/` 为生成证据；`summary.md` / `evidence_manifest.json` / `evidence_doctor.md` 只因被 phase result 引用而成为 summary-only 审查候选，不默认提交 raw run logs。
