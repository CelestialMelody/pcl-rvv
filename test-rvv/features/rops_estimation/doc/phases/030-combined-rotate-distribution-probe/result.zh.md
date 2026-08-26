# Phase 030 Result: combined-rotate-distribution-probe

## 当前结论

Phase 030 已完成 combined production-shaped diagnostic（组合生产形态诊断，测试专用链路尽量贴近生产调用边界）的验证。`rotateCloudAndDistributionMatricesRVV()` 在同一个 bench timing boundary（计时边界）中串联 `rotateCloudRVV()`、AABB（轴对齐包围盒）规约和三次 `getDistributionMatrixRVV()`，QEMU Std/RVV correctness（QEMU 正确性验证，不代表真实性能）、反汇编归属和板卡 5-run repeated benchmark（重复板卡性能测试）均已闭合到本阶段范围。

本阶段仍未修改 `features/include/pcl/features/impl/rops_estimation.hpp`。当前证据支持进入 PI1 production integration plan（生产接入计划），但还不能写成 最终生产行为：它不覆盖 LRF（Local Reference Frame，局部参考坐标系）、KdTree local surface（局部曲面搜索）、central moments（中心矩）、descriptor normalization（描述子归一化）、public dispatch（公开入口分流）、模板点型或真实 mesh 输入分布。

## 计划动作回填

| action | status | evidence | notes |
| --- | --- | --- | --- |
| A1 RED test | done | 首次 `make -C test-rvv/features/rops_estimation run_test_rvv` 因缺少 `rops::rotateCloudAndDistributionMatricesRVV()` 编译失败 | failure 来自缺少 candidate symbol，满足 test-first 入口 |
| A2 helper implementation | done | `include/impl/rops_components.hpp` | RVV helper 串联已有 rotate + distribution 候选，保持 rotated cloud buffer 和三个 projection matrix 的生产形态边界 |
| A3 combined bench | done | `src/bench_rops_estimation.cpp` case `rops_rotate_distribution_pipeline` | QEMU smoke checksum match；QEMU 只作为可运行性和日志形状，不作为性能结论 |
| A4 asm attribution | done | `build/asm/riscv/bench_rops_estimation_rvv.full.asm` | 同一 bench binary 中可见 rotate 热段的 `vlse32.v` / `vfmacc.vf` / `vsse32.v` / `vfredmin.vs` / `vfredmax.vs`，以及 distribution 热段的 `vfcvt.rtz.xu.f.v` 等指令 |
| A5 board repeated | done | `log/board/phase030_combined_rotate_distribution_repeated/summary.md`、`evidence_doctor.md` | 5-run median 1.630x，min 1.630x，max 1.640x，0/5 退化，checksum 全部一致 |

## 验证结果

| command | result | evidence path | role |
| --- | --- | --- | --- |
| `make -C test-rvv/features/rops_estimation run_test_compare` | pass：Std 7/7，RVV 7/7 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness 和日志形状 |
| QEMU bench smoke：`run_bench_std` / `run_bench_rvv` with `--points 1024 --iterations 1 --warmup 0 --repeat 1 --case-filter rops_rotate_distribution_pipeline` | pass；checksum match | `log/qemu/run_bench_std.log`、`log/qemu/run_bench_rvv.log` | qemu_smoke_only，不作为性能结论 |
| `make -C test-rvv/features/rops_estimation dump_test_rvv dump_bench_rvv` | pass；生成 test / bench asm | `build/asm/riscv/test_rops_estimation_rvv.full.asm`、`build/asm/riscv/bench_rops_estimation_rvv.full.asm` | RVV 路径和热点指令归属 |
| `make -C test-rvv/features/rops_estimation board_smoke ... --case-filter rops_rotate_distribution_pipeline` | pass；board test 7/7；single smoke 1.64x；checksum match | `log/board/analyze_bench_compare.log` | board smoke，只证明板卡可运行和方向检查 |
| 5-run repeated board collect | pass；median 1.630x，min 1.630x，max 1.640x，0/5 `B/A < 1`，checksum match | `log/board/phase030_combined_rotate_distribution_repeated/summary.md` | production-shaped diagnostic performance |
| `make -C test-rvv/features/rops_estimation doctor_phase030_board_summary` | pass；0 Error / 0 Warning / 2 Suggestion | `log/board/phase030_combined_rotate_distribution_repeated/evidence_doctor.md` | Evidence Doctor |

## Evidence Doctor 和 registry

Evidence Doctor 输入为 `log/board/phase030_combined_rotate_distribution_repeated/evidence_manifest.json`，输出为 `log/board/phase030_combined_rotate_distribution_repeated/evidence_doctor.md`。结果：0 Error、0 Warning、2 Suggestion。

Suggestions：

- `environment_metadata_missing`：缺少 taskset、governor、freq、temperature。当前 5-run 无方向反转、无 `B/A < 1`，作为 production-shaped diagnostic 可接受；PI production direct 证据应补这些环境字段或继续把该风险列入 Handoff。
- `binary_identity_missing`：缺少 binary hash。当前每轮通过 Makefile 重新部署、checksum 一致且方向稳定；若后续 production bench 出现长尾或方向反转，应记录二进制身份并清理旧日志后重跑。

`log/evidence_registry.json` 尚未创建；本阶段 summary / manifest / doctor 是 topic-local 生成证据，默认按 summary-only 策略处理，raw logs 不默认提交。板卡运行期间远端 `script/rvv-board-run.mk` 反复出现 future modification time / clock skew warning（时钟偏移警告），但构建、运行、checksum 和抓日志均完成；该 warning 不改变本阶段 decision bucket。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | same bench wrapper；Std build scalar fallback vs RVV build test-only `rotateCloudAndDistributionMatricesRVV()` |
| 当前决策问题 | implementation-shape；组合 rotate + distribution helper 是否支持进入 PI1 production integration plan |
| diagnostic 是否可外推到 production | unknown。它比单 component 更接近 production，但仍缺 LRF、KdTree local surface、central moments、descriptor normalization、public dispatch 和模板点型证据 |
| comparison-boundary / baseline mismatch 风险 | yes。production 中该链路嵌在 keypoint × axis × rotation 循环中，buffer 生命周期、allocator 行为、真实 local surface size 和 mesh 分布可能不同 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段实际为 positive。若后续 PI direct 变弱或退化，应先审计 production buffer 生命周期和真实输入分布，不直接 clean adopt |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。本阶段没有 production patch；PI5 后保留 / 回滚仍需用户确认 |

## Phase scope 与未验证范围

`validated_scope`：`pcl::PointXYZ` / float / AoS synthetic finite transformed local cloud，axis-angle 样本，rotated cloud + AABB + XY/XZ/YZ 三个 distribution matrices，QEMU Std/RVV correctness，bench asm attribution，板卡 repeated production-shaped diagnostic。

`unvalidated_scope`：完整 ROPS descriptor、LRF、真实 mesh local surface 分布、central moments fusion、descriptor normalization、其它点型、production dispatch、不同 rotations / bins / support radius、`PointXYZ` 以外布局和 `Scalar` 扩展。

`point_type_expansion_queue`：若进入 production integration loop（生产接入闭环），PI1/PI2 只允许先做 `pcl::PointXYZ` / float / AoS exact gate 或明确的 traits gate；其它点型需要独立 phase 和 fallback 测试。

## 阶段反思和下一步

combined probe 的板卡结果比两个单 component 仍更强：Phase 010 distribution matrix median 1.440x，Phase 020 rotate + AABB median 1.220x，本阶段 combined median 1.630x。该结果说明 rotated cloud 写回、三次 projection staging 和标量 scatter 没有把收益稀释掉，值得进入 PI1。

下一默认动作：创建 `040-production-integration-plan` 或等价 PI1 阶段，读取 `rvv-implementation` 细则，限定 production patch 范围、fallback/gate、production direct correctness、public entry bench、asm attribution 和 board repeated 证据。PI5 是用户检查点；无论证据支持采纳还是回滚，都不能在未确认时自行最终采用或撤回 production patch。

## Artifact tracking

本阶段新增 / 更新的 topic-local 源码、脚本和文档位于 `test-rvv/features/rops_estimation/**`。`build/` 和 raw `log/` 为生成证据；`summary.md` / `evidence_manifest.json` / `evidence_doctor.md` 只因被 phase result 引用而成为 summary-only 审查候选，不默认提交 raw run logs。
