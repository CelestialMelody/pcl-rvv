# Phase 040 Result: production-integration-plan

## 当前结论

Phase 040 已完成 PI1-PI5 production integration loop（生产接入闭环）。`features/include/pcl/features/impl/rops_estimation.hpp`
新增了有界 RVV（RISC-V Vector，可伸缩向量）production detail path（生产私有 helper 路径）：
`ROPSEstimation::rotateCloud()` 和 `ROPSEstimation::getDistributionMatrix()` 在 `__RVV10__` 构建、
exact `pcl::PointXYZ`、float AoS（结构数组）、dense cloud（PCL 约定的有限点云标志）和足够规模下先尝试
RVV helper，gate（准入条件）不满足时继续走原标量主体。

接入后的板卡 repeated benchmark（重复板卡性能测试）为 positive：5-run speedup 为
`1.700x, 1.700x, 1.700x, 1.690x, 1.700x`，median `1.700x`，`B/A < 1` 为 `0/5`，
checksum 全部一致。Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=2`。
当前证据支持保留 exact `PointXYZ` production detail scope。Phase 040 完成时 PI5 仍是用户检查点；
该检查点在后续用户确认和 Phase 050 后解除，当前采用状态以 Phase 050 result、evaluation 和长期 `doc-rvv` 为准。

本阶段证据仍是 production-detail（生产私有 helper 直连）而非完整 public `computeFeature()` 性能证据。
它证明真实 production private helper dispatch（生产私有 helper 分流）在当前合成 transformed local cloud
边界下快于标量路径；它不证明 LRF（Local Reference Frame，局部参考坐标系）、KdTree local surface
（局部曲面搜索）、central moments（中心矩）、descriptor normalization（描述子归一化）或完整 mesh 输入
public path 已优化。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| PI2-A1 production trace tests | done | `src/test_rops_estimation.cpp` 的 `RopsProductionRVV.*` cases | RVV build 下 production helper 命中 trace；小规模和 non-dense fallback 不命中。 |
| PI2-A2 production RVV helper and dispatch | done | `features/include/pcl/features/impl/rops_estimation.hpp` | 新增 `pcl::detail::rops::rotateCloudRVV()` 和 `getDistributionMatrixRVV()`；公开 API 不变。 |
| PI3-A1 fallback tests | partial | `SmallCloudFallsBackToScalarPath`、`NonDenseCloudFallsBackToScalarPath` | 小规模、non-dense 和非 RVV 构建由现有 compare 覆盖；exact 点型外的 fallback / 扩展留到 Phase 050 单独闭合。 |
| PI4-A1 production-detail bench case | done | `src/bench_rops_estimation.cpp` case `rops_production_rotate_distribution_pipeline` | bench label 区分 production-detail，不复用 diagnostic case。 |
| PI4-A2 QEMU smoke, asm, board and Doctor | done | `log/board/phase040_production_direct_repeated/*`、`build/asm/riscv/bench_rops_estimation_rvv.full.asm` | QEMU 只作可运行性；asm 有 RVV 指令；board bucket 为 positive；Doctor 无 Error / Warning。 |
| PI5-A1 EvidenceDecision | done | 本 result、summary、Doctor、optimization matrix | exact `PointXYZ` production detail scope 为 positive evidence；Phase 040 当时停在 PI5 检查点，后续确认后由 Phase 050 和 S11 closeout 升级为已采纳生产行为。 |

## 验证结果

| command | result | evidence path | role |
| --- | --- | --- | --- |
| `make -C test-rvv/features/rops_estimation run_test_compare` | pass：Std 7/7，RVV 11/11 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | correctness（正确性）和 production helper trace。 |
| QEMU bench smoke with `--case-filter rops_production_rotate_distribution_pipeline --points 1024 --bins 5 --repeat 2 --iterations 1 --warmup 0` | pass；checksum `13086495897199584369` | `log/qemu/run_bench_rvv.log` | qemu_smoke_only（只证明可运行和日志形状）。 |
| `make -C test-rvv/features/rops_estimation dump_bench_rvv` | pass；bench asm 生成 | `build/asm/riscv/bench_rops_estimation_rvv.full.asm` | asm attribution（反汇编归属）。 |
| 5-run board repeated collect | pass；median `1.700x`，min `1.690x`，max `1.700x`，0/5 退化 | `log/board/phase040_production_direct_repeated/summary.md` | production-detail board performance。 |
| `make -C test-rvv/features/rops_estimation doctor_phase040_board_summary` | pass；`0E/0W/2S` | `log/board/phase040_production_direct_repeated/evidence_doctor.md` | Evidence Doctor。 |

## Board 结果

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `rops_production_rotate_distribution_pipeline,points=65536,repeat=8` | 5 | 1.700x | 1.690x | 1.700x | 0/5 | positive |

每轮明细：

| run | Std us/iter | RVV us/iter | speedup | checksum |
| --- | ---: | ---: | ---: | --- |
| `run-01` | 434232.8168 | 255784.4272 | 1.700x | match |
| `run-02` | 435384.6961 | 256002.3439 | 1.700x | match |
| `run-03` | 435904.1356 | 255993.5481 | 1.700x | match |
| `run-04` | 432565.8586 | 255994.4314 | 1.690x | match |
| `run-05` | 434744.7336 | 255995.8022 | 1.700x | match |

板卡构建日志中出现 clock skew warning（构建时间戳偏移警告）。它来自远端文件时间戳环境；当前构建、运行、
checksum、summary 和 Evidence Doctor 均完成，Doctor 未将其判为 Error 或 Warning，因此不改变本阶段
positive decision bucket（正向决策桶）。

## Evidence Doctor 和 registry

Evidence Doctor 输入为 `log/board/phase040_production_direct_repeated/evidence_manifest.json`，输出为
`log/board/phase040_production_direct_repeated/evidence_doctor.md`。结果：0 Error、0 Warning、2 Suggestion。

Suggestions：

- `environment_metadata_missing`：缺少 taskset、governor、freq、temperature。当前 5-run 没有退化和长尾，
  该建议不阻塞采纳；若后续出现方向反转，应优先补这些环境字段。
- `binary_identity_missing`：缺少 binary hash。当前 checksum 一致且 Makefile 同边界重跑稳定；严格归档或
  后续异常时再补二进制身份。

`log/evidence_registry.json` 尚未创建；本 topic 目前没有 `evidence_status` target。当前 freshness check
（证据新鲜度检查）采用人工路径核对：Phase 040 summary、manifest、Doctor 均存在并互相指向同一
`phase040_production_direct_repeated` run label；evaluation、matrix、roadmap 和长期 `doc-rvv` 将在收口后引用
这组路径。raw `run-*` 日志默认不提交。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_detail` |
| A/B boundary | production private helper：真实 `ROPSEstimation::rotateCloud()` 与 `getDistributionMatrix()` dispatch，bench wrapper 只负责调用和计时 |
| 当前决策问题 | RVV-vs-scalar：接入生产源码后的 private helper path 是否仍快于标量主体 |
| diagnostic 是否可外推到 production | 不依赖 Phase 030 外推作最终结论；本阶段使用接入后的 production-detail board 数据 |
| comparison-boundary / baseline mismatch 风险 | 低于 Phase 030，但仍不是完整 public `computeFeature()`；不包含 search、LRF、moments 和 normalization |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段实际为 positive；若后续 point type 或 public compute 变弱，只降级对应扩展，不撤销本 exact scope |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段是新 production helper 对原标量 helper 的同边界比较，不是在已有 RVV family 之间做选择 |

## Phase scope 与未验证范围

`validated_scope`：`pcl::PointXYZ` / `pcl::Histogram<135>`、float AoS、dense synthetic transformed local cloud、
`points=65536`、`bins=5`、`repeat=8`，真实 production private `rotateCloud()` + `getDistributionMatrix()`
dispatch，QEMU correctness、production helper trace、asm attribution、board repeated 和 Evidence Doctor。

`unvalidated_scope`：完整 public `computeFeature()`、真实 mesh local surface 分布、LRF、central moments、
descriptor normalization、其它 `PCL_XYZ_POINT_TYPES`、`Scalar=double`、非 dense 输入、用户错误标记为 dense 的
NaN/Inf 输入、不同 rotations / bins / support radius 和目标硬件之外的性能。

`point_type_expansion_queue`：`features/src/rops_estimation.cpp` 在常规构建中对 `PCL_XYZ_POINT_TYPES` 预编译
`ROPSEstimation<InT, Histogram<135>>`。由于当前 RVV stage 只读取和写回 xyz，并且后续 distribution matrix
只消费 xyz，`PointXYZI`、`PointXYZRGBA`、`PointNormal` 等常见 xyz AoS 点型存在独立扩展价值。该扩展不能由
Phase 040 外推，必须新建 Phase 050，补 traits gate、typed correctness、fallback、asm、board repeated 和 Doctor。

## EvidenceDecision

`evidence_decision`: `adopted_production_detail_pointxyz`

`production_decision`: 当前证据支持保留 Phase 040 production patch；该检查点后续已由用户确认解除。当前证据范围只覆盖
`pcl::PointXYZ` 的 private helper detail path；不改 public API，不声明完整 public descriptor 加速。其它点型由 Phase 050
代表性扩展证据独立关闭。

`continue_stop_decision`: Phase 040 的 PI5 检查点已由用户确认解除。当前工作区已经进一步完成 Phase 050 代表性点型扩展证据采集；
这些额外证据作为用户确认后 adopted production behavior 的范围补充，不外推到完整 public descriptor 或所有自定义点型。
