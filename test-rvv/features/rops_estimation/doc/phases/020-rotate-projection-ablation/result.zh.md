# Phase 020 Result: rotate-projection-ablation

## 当前结论

Phase 020 已完成 `ROPSEstimation::rotateCloud()` 的 test-only RVV candidate（测试专用 RVV 候选）验证。`rotateCloudRVV()` 在 QEMU（仿真器）Std/RVV 两种构建下与 production private helper oracle（生产私有 helper 参考真值）一致；板卡 5-run repeated component bench（重复组件性能测试）显示 positive diagnostic（正向诊断）信号。

本阶段仍不修改 production（生产源码）。原因是候选只覆盖 synthetic transformed local cloud（合成已变换局部点云）的 AoS stride load/store（结构数组跨步加载 / 存储）、3x3 rotation（旋转矩阵乘法）和 AABB min/max reduction（轴对齐包围盒最小 / 最大规约），不覆盖 LRF、KdTree local surface、distribution matrix、central moments、descriptor normalization 或 public dispatch。该结果支持进入 combined rotate + distribution matrix production-shaped diagnostic（组合生产形态诊断），不能单独证明 production-ready。

## 计划动作回填

| action | status | evidence | notes |
| --- | --- | --- | --- |
| A1 RED test | done | `make -C test-rvv/features/rops_estimation run_test_rvv` first failed with missing `rops::rotateCloudRVV` | failure 来自缺少 candidate symbol |
| A2 helper implementation | done | `include/impl/rops_components.hpp` | 非 RVV 构建走 `rotateCloudStd()`；RVV 构建用 `vlse32` / `vsse32` 处理 `PointXYZ` AoS，批量旋转并规约 AABB |
| A3 axis coverage | done | `src/test_rops_estimation.cpp` | 覆盖 X/Y/Z canonical axes（基准轴）和 tilted axis（倾斜轴），并新增 bench-shaped 1024 点样本 |
| A4 asm attribution | done | `build/asm/riscv/test_rops_estimation_rvv.full.asm`、`build/asm/riscv/bench_rops_estimation_rvv.full.asm` | `rotateCloudRVV` 热段出现 `vlse32.v` / `vfmacc.vf` / `vsse32.v` / `vfmin.vv` / `vfmax.vv` / `vfredmin.vs` / `vfredmax.vs` |
| A5 bench decision | done | `src/bench_rops_estimation.cpp`、`log/board/phase020_rotate_cloud_repeated/summary.md` | 追加 `rops_rotate_cloud_aabb` component bench；板卡 repeated 结果 positive，但只作为 diagnostic |

## 验证结果

| command | result | evidence path | role |
| --- | --- | --- | --- |
| `make -C test-rvv/features/rops_estimation run_test_compare` | pass：Std 6/6，RVV 6/6 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness 和日志形状 |
| `make -C test-rvv/features/rops_estimation dump_test_rvv` | pass：生成 test asm | `build/asm/riscv/test_rops_estimation_rvv.asm`、`.full.asm` | correctness binary asm attribution |
| `make -C test-rvv/features/rops_estimation dump_bench_rvv` | pass：生成 bench asm | `build/asm/riscv/bench_rops_estimation_rvv.asm`、`.full.asm` | bench binary asm attribution |
| QEMU bench smoke：`run_bench_std` / `run_bench_rvv` with `--points 1024 --iterations 1 --warmup 0 --repeat 1 --case-filter rops_rotate_cloud_aabb` | pass；aggregate checksum match | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log` | qemu_smoke_only，不作为性能结论 |
| `make -C test-rvv/features/rops_estimation board_smoke ... --case-filter rops_rotate_cloud_aabb` | pass；board test 6/6；single smoke 1.21x；checksum match | `log/board/analyze_bench_compare.log` | board smoke |
| 5-run repeated board collect | pass；median 1.220x，min 1.200x，max 1.220x，0/5 `B/A < 1`，checksum match | `log/board/phase020_rotate_cloud_repeated/summary.md` | diagnostic component performance |
| `make -C test-rvv/features/rops_estimation doctor_phase020_board_summary` | pass；0 Error / 0 Warning / 2 Suggestion | `log/board/phase020_rotate_cloud_repeated/evidence_doctor.md` | Evidence Doctor |

## Evidence Doctor 和 registry

Evidence Doctor 输入为 `log/board/phase020_rotate_cloud_repeated/evidence_manifest.json`，输出为 `log/board/phase020_rotate_cloud_repeated/evidence_doctor.md`。结果：0 Error、0 Warning、2 Suggestion。

Suggestions：

- `environment_metadata_missing`：缺少 taskset、governor、freq、temperature。当前 5-run 无方向反转或长尾，结论可保留为 diagnostic；后续 production-shaped 或 production-direct 证据应补环境字段。
- `binary_identity_missing`：缺少 binary hash。当前每轮通过 Makefile 重新部署同名二进制且 checksum 一致，作为 component diagnostic 可接受；若后续出现方向反转，应补二进制身份或清理后重跑。

`log/evidence_registry.json` 尚未创建；本阶段 summary / manifest / doctor 是 topic-local 生成证据，默认按 summary-only 策略处理，raw logs 不默认提交。

## Checksum 策略说明

初版 rotate bench 使用逐点量化哈希。QEMU smoke 和板卡 smoke 显示 production oracle 与 RVV candidate 的逐点 correctness 在 2e-5 容差内通过，但逐点哈希会被 FMA（融合乘加）和 Eigen 标量 / RVV 运算顺序的低位差异放大成 checksum mismatch。最终 bench 改为 aggregate signature（聚合签名）：记录 rotated cloud 的 size、三轴求和、三轴绝对值求和和 AABB min/max 的 1e-3 量化摘要。真正的逐点语义仍由 gtest 对拍承担，bench checksum 只作为粗粒度日志一致性信号。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic component correctness + diagnostic component board performance |
| A/B boundary | same bench wrapper；Std build scalar fallback vs RVV build test-only `rotateCloudRVV()` |
| 当前决策问题 | implementation-shape；rotateCloud + AABB 是否值得与 distribution matrix 组成 combined production-shaped probe |
| diagnostic 是否可外推到 production | no。它不覆盖 complete `ROPSEstimation::computeFeature()`、LRF、mesh local surface、distribution matrix、central moments、descriptor normalization 或 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes。当前 helper 只处理 `PointXYZ` synthetic cloud；production 是模板入口，实际调用频率受 keypoints、rotations、local surface size 和 bins 影响 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for rotate component alone；positive 时也只允许作为 combined probe 的前置线索 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段没有 production patch，也没有完整 public entry 证据 |

## Phase scope 与未验证范围

`validated_scope`：`pcl::PointXYZ` / float / AoS synthetic finite transformed local cloud，X/Y/Z 和 tilted axis，rotated point output 与 AABB min/max，QEMU Std/RVV correctness，test / bench asm attribution，板卡 repeated component diagnostic。

`unvalidated_scope`：完整 ROPS descriptor、LRF、真实 mesh local surface 分布、distribution matrix + rotate combined boundary、central moments fusion、descriptor normalization、其它点型、production dispatch、不同 rotations / bins / support radius。

`point_type_expansion_queue`：本阶段不扩点型；若后续进入 production probe，再为 exact type gate 与 PointXYZ-like traits 建独立 phase。

## 阶段反思和下一步

rotateCloud + AABB 和 distribution matrix binning 两个 component 都在板卡上有 positive diagnostic 信号：Phase 010 median 1.440x，Phase 020 median 1.220x。下一阶段默认进入 `030-combined-rotate-distribution-probe`：把 rotateCloud 输出和三个 projection 的 distribution matrix 组合在同一个 bench boundary（计时边界）里，验证 component 正向在更接近 production 的组合链路里是否仍保留。如果 combined probe 仍 positive，才考虑 PI1 production integration plan（生产接入计划）；如果 combined probe neutral / negative，则记录错配审计，不单独推进 production patch。

## Artifact tracking

本阶段新增 / 更新的 topic-local 源码、脚本和文档位于 `test-rvv/features/rops_estimation/**`。`build/` 和 raw `log/` 为生成证据；`summary.md` / `evidence_manifest.json` / `evidence_doctor.md` 只因被 phase result 引用而成为 summary-only 审查候选，不默认提交 raw run logs。
