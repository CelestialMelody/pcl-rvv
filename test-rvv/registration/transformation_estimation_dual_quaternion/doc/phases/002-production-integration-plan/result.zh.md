# Phase 002 Result：production-integration-plan

## 结论

本阶段尝试把 Phase 001 的 ordered-cloud-pair C1/C2 RVV accumulation candidate 临时接入 `TransformationEstimationDualQuaternion::estimateRigidTransformation(cloud_src, cloud_tgt, Matrix4f&)` production public entry。QEMU correctness 通过，asm dump 能看到该临时路径使用 `vlse32.v`、`vfwcvt.f.f.v` 和 `vfredosum.vs` 一类 RVV 指令；但 Milkv-Jupiter 5-run production-direct repeated 结果为 `neutral`，Evidence Doctor 对 4K 给出 Error，并对 64K / 256K 给出退化频率 warning。

EvidenceDecision：`no_production_after_direct_neutral`。临时 production patch 已撤回，当前 production 源码保持标量。Phase 001 的 test-only helper 诊断收益仍保留为候选信息，但不能升级为 production-ready。

## 执行动作

| action | 状态 | 产物 | 结论 |
| --- | --- | --- | --- |
| A1 production helper / dispatch | attempted then reverted | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | QEMU 正确但板端 production-direct 不达标，已撤回 |
| A2 production boundary tests | done | `src/test_tedq.cpp` | Std/RVV 各 9 tests passed；覆盖 `PointXYZI` public、`Scalar=double`、source-indexed、dual-indexed 和 correspondence identity 边界 |
| A3 production evidence target | done | `Makefile`、`script/generate_tedq_board_repeated_summary.py`、`.gitignore` allowlist | 可记录 `public-dual-quaternion` production-public repeated summary |
| A4 board production-direct evidence | done | `log/board/production_public_full_cloud_repeated/*` | 4K / 64K / 256K 均为 `neutral`；doctor 有 Error / Warnings |
| A5 docs / matrix | done | README、evidence、roadmap、phase matrix | 当前结论改为 no-production |

## Board Production-Direct 结果

证据路径：

- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_full_cloud_repeated/summary.md`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_full_cloud_repeated/evidence_manifest.json`
- `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_full_cloud_repeated/evidence_doctor.md`

摘要：

| size | B/A values | median | bucket | doctor |
| --- | --- | ---: | --- | --- |
| 4K | `1.002, 1.001, 0.992, 1.008, 0.988` | 1.001 | `neutral` | Error：2/5 低于 1 |
| 64K | `0.993, 1.000, 1.003, 1.002, 1.013` | 1.002 | `neutral` | Warning：1/5 低于 1 |
| 256K | `0.997, 1.001, 1.003, 1.001, 1.006` | 1.001 | `neutral` | Warning：1/5 低于 1 |

checksum 均一致。QEMU timing 不进入性能结论。

## 解释

Phase 001 的同边界 test-only helper 把 C1/C2 accumulation 单独放大，板端 median B/A 约为 2.014x / 2.066x / 2.097x；但 production public entry 每次还包含 Eigen 4x4 self-adjoint solve、quaternion 后处理、公开入口模板与分流门控成本。直接接入后总时间只在 1.00x 附近抖动，说明当前接法没有把局部收益转成公开入口收益。

因此，本阶段不能保留 production dispatch，也不能创建 `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md` 作为 adopted production behavior 文档。

## 后续恢复建议

1. 先做 component no-solve / accumulation-only 消融，确认生产入口里 C1/C2 前端真实占比是否足够。
2. 若继续 production 方向，应先缩小 timer boundary 或降低 dispatch / register pressure 成本，再重跑 production-public repeated。
3. source-indexed、dual-indexed 和 correspondence-pair 仍保持独立待评估，不能继承 ordered-cloud-pair 诊断结论。

## 提交边界

| 路径 | 状态 | 说明 |
| --- | --- | --- |
| `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | unchanged | 临时 production patch 已撤回 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_full_cloud_repeated/summary.md` | to-be-staged | summary evidence |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_full_cloud_repeated/evidence_manifest.json` | to-be-staged | Evidence Doctor manifest |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_full_cloud_repeated/evidence_doctor.md` | to-be-staged | 记录 Error / Warnings，用于阻止 production 采纳 |
| `test-rvv/registration/transformation_estimation_dual_quaternion/log/board/production_public_full_cloud_repeated/run-*/*.log` | ignored-local | raw board run logs，不默认提交 |
