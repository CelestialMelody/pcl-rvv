# Phase 050 Result: point-type-expansion

## 当前结论

Phase 050 已完成 PointXYZ-like traits gate（点类型字段特征门禁）扩展。`features/include/pcl/features/impl/rops_estimation.hpp`
中的 `rotateCloudRVV()` 和 `getDistributionMatrixRVV()` 不再限制 exact `pcl::PointXYZ`，改为使用
`pcl::rvv::RVVXYZAoSFloatLayout<PointInT>` 判定 xyz 字段是否可按 float AoS（结构数组）访问；不满足
layout、规模、dense、bins、projection 或 AABB extent 条件时仍回到原标量主体。

接入后的代表性点型板卡 repeated benchmark（重复板卡性能测试）均为 positive：

| point type | case | runs | median speedup | min | max | B/A < 1 | checksum |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `PointXYZI` | `rops_production_pointxyzi_rotate_distribution_pipeline,points=65536,repeat=8` | 5 | 1.520x | 1.470x | 1.570x | 0/5 | match |
| `PointNormal` | `rops_production_pointnormal_rotate_distribution_pipeline,points=65536,repeat=8` | 5 | 1.590x | 1.570x | 1.610x | 0/5 | match |

Evidence Doctor（证据体检）两组均为 `Errors=0, Warnings=0, Suggestions=2`。Suggestions 仍是环境 metadata
和 binary identity（环境元数据与二进制身份）缺失；当前没有退化、checksum 不一致或方向反转，不阻塞本阶段
positive evidence（正向证据）判断。用户已确认板卡有收益即可采纳，当前 production patch（生产补丁）
进入已采纳生产行为。

`evidence_decision`: `adopted_production_detail_traits_xyz_aos_representative_types`

`production_decision`: 当前证据支持并已采纳 traits-gated production detail path。当前生产代码可覆盖满足
`RVVXYZAoSFloatLayout<PointInT>` 的 xyz float AoS 点型；本阶段实测代表为 `PointXYZI` 和 `PointNormal`。
这不声明所有自定义点型、额外字段输出语义、`Scalar=double` 或完整 public `computeFeature()` 性能已经逐一证明。

`continue_stop_decision`: stop-no-default-optimization-left。当前 topic 内已完成 component ablation、production
integration、代表性点型扩展和 S11 production closeout。剩余优化方向要么需要完整 public mesh workload/profile，
要么是 evidence metadata hardening（证据元数据加固），不建议作为默认连续优化继续扩大。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| A1 typed RED tests | done | `src/test_rops_estimation.cpp` 的 `PointXYZITraitsGateUsesRVVAndMatchesScalarReference`、`PointNormalTraitsGateUsesRVVAndMatchesScalarReference` | RVV build 下 typed production helper trace 命中，xyz、AABB 和 distribution matrices 与 PointXYZ scalar reference 对齐。 |
| A2 traits-gated production patch | done | `features/include/pcl/features/impl/rops_estimation.hpp` | exact `PointXYZ` 限制已移除；`RVVXYZAoSFloatLayout<PointT>`、dense、规模、bins、projection 和 extent gate 保留。 |
| A3 typed bench case | done | `src/bench_rops_estimation.cpp`、`script/generate_rops_repeated_summary.py`、`Makefile` | 新增 `PointXYZI` / `PointNormal` production-detail case-filter 和对应 summary / Doctor target。 |
| A4 correctness / asm | done | `make -C test-rvv/features/rops_estimation run_test_compare`、`dump_bench_rvv` | Std 7/7、RVV 16/16；post-review 补 public non-dense、helper invalid-input 和非 f32 layout fallback；bench full asm 中 typed `rotateCloudRVV` / `getDistributionMatrixRVV` 实例含 RVV 指令。 |
| A5 board repeated / Doctor | done | `log/board/phase050_pointxyzi_production_detail_repeated/*`、`log/board/phase050_pointnormal_production_detail_repeated/*` | 两组 5-run repeated board bucket 均为 positive，Doctor 均无 Error / Warning。 |
| A6 PI5 checkpoint docs | done | evaluation、roadmap、matrix、phase index、README、`doc-rvv/features/rops_estimation-RVV.zh.md` | topic-local 文档记录接入后板卡数据，保留代表性点型和完整 public path 的边界；长期 `doc-rvv` 记录已采纳生产行为。 |

## 验证结果

| command | result | evidence path | role |
| --- | --- | --- | --- |
| `make -C test-rvv/features/rops_estimation run_test_compare` | pass：Std 7/7，RVV 16/16 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | correctness（正确性）、public non-dense fallback、helper gate fallback 和 production helper trace。 |
| QEMU smoke：`--case-filter rops_production_pointxyzi_rotate_distribution_pipeline --points 1024 --bins 5 --repeat 2 --iterations 1 --warmup 0` | pass；checksum `13086495897199584369` | `log/qemu/run_bench_rvv.log` | qemu_smoke_only（只证明可运行和日志形状）。 |
| QEMU smoke：`--case-filter rops_production_pointnormal_rotate_distribution_pipeline --points 1024 --bins 5 --repeat 2 --iterations 1 --warmup 0` | pass；checksum `13086495897199584369` | `log/qemu/run_bench_rvv.log` | qemu_smoke_only；该日志会被后一次 smoke 覆盖，命令输出是本轮证据记录。 |
| `make -C test-rvv/features/rops_estimation dump_bench_rvv` | pass；bench asm 生成 | `build/asm/riscv/bench_rops_estimation_rvv.full.asm` | asm attribution（反汇编归属）。 |
| PointXYZI 5-run board repeated collect | pass；median `1.520x`，min `1.470x`，max `1.570x`，0/5 退化 | `log/board/phase050_pointxyzi_production_detail_repeated/summary.md` | production-detail board performance。 |
| PointNormal 5-run board repeated collect | pass；median `1.590x`，min `1.570x`，max `1.610x`，0/5 退化 | `log/board/phase050_pointnormal_production_detail_repeated/summary.md` | production-detail board performance。 |
| `doctor_phase050_pointxyzi_board_summary` / `doctor_phase050_pointnormal_board_summary` | pass；两组均 `0E/0W/2S` | 对应 `evidence_doctor.md` | Evidence Doctor。 |

## Phase scope 与未验证范围

`validated_scope`：production private `rotateCloud()` + `getDistributionMatrix()` dispatch，`PointXYZ`、`PointXYZI`
和 `PointNormal` 代表性 xyz float AoS 点型，`PointOutT=pcl::Histogram<135>`，dense synthetic transformed local cloud，
`points=65536`、`bins=5`、`repeat=8`，QEMU correctness、trace hit、asm attribution、board repeated 和 Evidence Doctor。

`unvalidated_scope`：完整 public `computeFeature()`、真实 mesh local surface 分布、LRF、central moments、
descriptor normalization、所有自定义 traits-compatible 点型逐个板卡覆盖、`Scalar=double`、非 dense 输入、
用户错误标记为 dense 的 NaN/Inf 输入、其它 bins / rotations / support radius 和目标硬件之外的性能。

`phase_closeout_boundary`：本阶段关闭 PointXYZ-like xyz AoS production detail 扩展的代表性点型证据，不关闭完整
RoPS descriptor、公有入口性能或额外字段输出语义。

## Evidence Doctor 和 registry

两组 Doctor 输入分别为：

- `log/board/phase050_pointxyzi_production_detail_repeated/evidence_manifest.json`
- `log/board/phase050_pointnormal_production_detail_repeated/evidence_manifest.json`

输出分别为：

- `log/board/phase050_pointxyzi_production_detail_repeated/evidence_doctor.md`
- `log/board/phase050_pointnormal_production_detail_repeated/evidence_doctor.md`

结果均为 0 Error、0 Warning、2 Suggestion。`environment_metadata_missing` 和 `binary_identity_missing` 不阻塞当前
positive decision bucket；若后续严格归档或出现方向反转，应补 taskset、governor、freq、temperature、binary hash。

`log/evidence_registry.json` 仍未创建；本 topic 没有统一 `evidence_status` target。当前 freshness check 使用人工路径核对：
summary、manifest、Doctor 和 phase result 均指向同一 run label。raw `run-*` 日志默认不提交。该缺口归类为
evidence metadata hardening（证据元数据加固）：它会提高长期归档可复核性，但不改变当前算法收益判断；
本轮停止条件是“继续会转入证据归档/工具化范围，而不是当前 ROPS 算法优化”。

## Doc-suite parity 审计

| area | current shape scan | quality bar | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| README navigation | 本 phase 新增 `README.zh.md` | README 负责入口、命令、证据白名单 | adopted | topic-local 入口可定位 evaluation、phase 和 summary；长期 `doc-rvv` 已按 adopted production behavior 发布 | 保持同步 |
| testing_overview / correctness_tests | 合并在 `README.zh.md` 与 evaluation 的测试章节 | 当前 topic gtest 数量可用合并章节承载 | adopted | `run_test_compare` 覆盖 Std/RVV 计数、public non-dense fallback、helper gate fallback 和 typed scope | 当前范围不拆；若后续新增 public workload 或更多点型矩阵，再随新 phase 拆独立 role 文档 |
| benchmark_and_evidence | 合并在 README、phase result 和 summary | summary 是 bench 统计主归属 | adopted | Phase 040/050 summary 与 Doctor 已生成 | 保持 summary-only |
| optimization_evidence | `optimization-matrix.zh.md` + roadmap | matrix 记录 candidate 到证据的映射 | adopted | Phase 000-050 均有 matrix row | 无 |
| test_support_code_map | 合并在 evaluation Traceability Map | 当前测试支撑主要是一个 include 聚合头、一个 impl header、test / bench / script | adopted | map 可跳到源码、bench 和 script | 若 topic 继续扩张再拆分 |
| production_topic_doc | 已创建 `doc-rvv/features/rops_estimation-RVV.zh.md` | 只写最终生产行为或用户确认保留的 production patch | adopted | 用户确认已解除，长期文档引用接入后板卡数据 | 保持与生产源码和 summary 同步 |
| artifact tracking | 当前 topic 文件多为 untracked；长期 `doc-rvv` 已移出本阶段发布边界 | 新增/更新文件列入 topic commit boundary；raw logs 默认排除 | adopted | `git status --short --untracked-files=all -- test-rvv/features/rops_estimation` 可扫描 | 提交时精确选择 |
| evidence registry / unified status | `log/evidence_registry.json` 与统一 `evidence_status` target 不存在 | 归档级证据可复核性应由 registry 或等价 target 增强 | turn_stop_deferred with stop_condition_hit | 当前 summary、manifest、Doctor 人工 freshness 已核对；继续会进入证据归档工具化范围，不改变算法取舍 | 用户要求严格归档或出现长尾/方向反转时另开 metadata hardening phase |

## 后续方向判断

不建议默认继续的方向：

- 完整 public `computeFeature()`：需要真实 mesh workload、search/LRF/profile，已经超出当前 production detail phase；没有 profile 时容易把 search、Eigen 和 descriptor tail 混成一个不清楚的结论。
- descriptor normalization：135 维连续 reduction + scale 理论上可试，但工作量小、收益可能被前段稀释，优先级低。
- scatter 去标量化：distribution matrix 的 bin scatter 有冲突和顺序语义风险；当前 RVV 已把 row/col staging 与旋转部分拿到稳定收益。
- evidence metadata hardening：值得做但属于证据归档质量，不是算法优化收益方向。

本 topic 当前默认暂停在 S11 closeout 后：production detail patch 已按用户确认采纳，长期 `doc-rvv`
已经创建。若后续要继续，应另开新 phase 或新 topic，先补 public workload/profile 或严格证据 metadata 需求。
