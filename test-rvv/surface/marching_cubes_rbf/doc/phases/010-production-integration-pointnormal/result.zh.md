# 010 production integration: PointNormal 窄范围接入结果

## 当前结论

本阶段完成 PI1-PI5：已在 `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` 接入 exact `pcl::PointNormal`
阶段性 RVV gate（门控），并补了 production direct（真实生产入口证据）测试和板卡 compare。证据支持
`adopted_production_behavior`：用户已确认保留当前 production patch，本阶段的 exact `pcl::PointNormal`
RVV 路径进入生产采用范围。

正式长期文档已创建为 `doc-rvv/surface/marching_cubes_rbf-RVV.zh.md`。该文档只覆盖 exact
`pcl::PointNormal`，不把泛型 normal 点型写成已覆盖。

恢复提示：这是 010 阶段的历史 closeout。020 阶段已把当前 production gate 扩展为
`RVVXYZNormalFloatLayout<PointNT>` traits gate，并刷新正式长期文档；恢复当前状态时以
`test-rvv/surface/marching_cubes_rbf/doc/phases/020-generic-normal-point-types/result.zh.md`、
phase index、optimization matrix 和 `doc-rvv/surface/marching_cubes_rbf-RVV.zh.md` 为准。

## 实际执行范围

| 项 | 计划 | 实际 |
| --- | --- | --- |
| production scope | `MarchingCubesRBF<PointNT>::voxelizeData()` | 已修改同一 impl 头；public API 不变。 |
| point type | exact `pcl::PointNormal` | 已按 `std::is_same_v<PointNT, pcl::PointNormal>` 分流。 |
| fallback | 非 RVV、非 `PointNormal`、`N < 16` | 已保留标量 helper fallback，并用 gtest 覆盖小规模与 `PointXYZINormal`。 |
| RVV 覆盖 | matrix fill + grid evaluation | 已接入；Eigen `fullPivLu()` 和 surface emission 保持既有路径。 |
| 文档边界 | topic-local result/matrix/roadmap/evaluation | 已回填；正式 `doc-rvv` 等用户确认。 |

## 文件变更

| 文件 | 变更 |
| --- | --- |
| `surface/include/pcl/surface/impl/marching_cubes_rbf.hpp` | 抽出 `marchingCubesRBFVoxelizeDataStandard()`，新增 `__RVV10__` 下的 `marchingCubesRBFVoxelizeDataRVV<PointNormal>()`。 |
| `test-rvv/surface/marching_cubes_rbf/include/impl/marching_cubes_rbf_core.hpp` | 新增 production probe helper，用真实 `MarchingCubesRBF<PointT>::voxelizeData()` 构造 direct evidence。 |
| `test-rvv/surface/marching_cubes_rbf/src/test_marching_cubes_rbf.cpp` | gtest 从 3 个扩到 6 个，新增 production direct、small fallback、非覆盖点型 fallback。 |
| `test-rvv/surface/marching_cubes_rbf/src/bench_marching_cubes_rbf.cpp` | 新增 `mcrbf_prod_pointnormal_*` production direct bench case。 |
| `test-rvv/surface/marching_cubes_rbf/Makefile` | 增加 production board target，并补 `pcl_common/lz4` 链接依赖。 |
| `test-rvv/surface/marching_cubes_rbf/script/generate_marching_cubes_rbf_evidence_manifest.py` | manifest 区分 component diagnostic 与 production direct。 |

## 命令和证据

| 类型 | 命令 / 路径 | 结果 |
| --- | --- | --- |
| QEMU correctness | `make -C test-rvv/surface/marching_cubes_rbf run_test_compare` | Std/RVV 各 6/6 通过。 |
| QEMU bench smoke | `make -C test-rvv/surface/marching_cubes_rbf run_bench_rvv BENCH_ARGS="--case-filter mcrbf_prod_pointnormal_n24_r18 --iterations 1 --warmup-iterations 0"` | RVV production case 可运行并输出 checksum；计时不作为性能证据。 |
| asm dump | `make -C test-rvv/surface/marching_cubes_rbf dump_bench_rvv` | bench RVV binary 含 `vle64/vse64/vfmacc/vfsqrt/vfredusum`。 |
| asm attribution | `nm -C build/riscv/bench_marching_cubes_rbf_rvv` | 存在 `pcl::detail::marchingCubesRBFVoxelizeDataRVV<pcl::PointNormal>` 与 `voxelizeData()` 符号。 |
| board production bench | `make -C test-rvv/surface/marching_cubes_rbf run_board_production_smoke fetch_board_logs run_board_evidence_doctor` | production direct 三个 case 均为 `1.08x`，checksum 一致。 |
| board correctness | `make -C test-rvv/surface/marching_cubes_rbf run_board_test fetch_board_logs` | 板卡 RVV gtest 6/6 通过。 |
| Evidence Doctor | `test-rvv/surface/marching_cubes_rbf/log/board/evidence_doctor.md` | `Errors=0, Warnings=8, Suggestions=0`；warnings 均为 `low_run_count`。 |
| whitespace | `git diff --check -- surface/include/pcl/surface/impl/marching_cubes_rbf.hpp test-rvv/surface/marching_cubes_rbf` | 通过。 |

## 板卡性能结果

| case | Std ms | RVV ms | speedup | 证据角色 |
| --- | ---: | ---: | ---: | --- |
| `mcrbf_matrix_fill_n24` | 0.0688 | 0.0279 | 2.47x | component diagnostic |
| `mcrbf_matrix_fill_n40` | 0.2047 | 0.0826 | 2.48x | component diagnostic |
| `mcrbf_full_pipeline_n24_r18` | 5.4088 | 4.3453 | 1.24x | production-shaped diagnostic |
| `mcrbf_full_pipeline_n40_r20` | 13.1881 | 10.5322 | 1.25x | production-shaped diagnostic |
| `mcrbf_full_pipeline_n56_r18` | 17.0042 | 14.0820 | 1.21x | production-shaped diagnostic |
| `mcrbf_prod_pointnormal_n24_r18` | 4.7577 | 4.4221 | 1.08x | production direct |
| `mcrbf_prod_pointnormal_n40_r20` | 11.6229 | 10.7603 | 1.08x | production direct |
| `mcrbf_prod_pointnormal_n56_r18` | 15.4455 | 14.2380 | 1.08x | production direct |

## Evidence Doctor 解释

无 Error。8 个 Warning 均为 `low_run_count`，原因是当前 manifest 只有每个 case 的 summary speedup，
没有逐次 B/A values（候选相对基线收益序列）。bench 内部使用 `iterations=5`、`warmup=2`，但 analyzer
没有保留每次迭代的独立 timing，因此 Evidence Doctor 正确地把强稳定性结论降级。

处理动作：不伪造重复值；本阶段把 production evidence 判为 weak-positive（弱正向）。三个 production direct
case 同向约 `1.08x` 且 checksum 一致，满足计划中的 `>1.05x` 建议保留门槛；用户已确认保留后，
该证据成为 exact `PointNormal` adopted production behavior 的依据。

## diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | 已新增 `production_direct` case；诊断 case 仍只作支撑。 |
| A/B boundary | production direct case 通过真实 `MarchingCubesRBF<PointNormal>::voxelizeData()`。 |
| 当前决策问题 | `RVV-vs-scalar`：真实 public path RVV 是否快于标量。 |
| diagnostic 是否外推 | 不直接外推；生产结果以 `mcrbf_prod_pointnormal_*` 为准。 |
| baseline mismatch | 已降低：Std/RVV 使用同一 bench wrapper、同一 PointNormal cloud、同一 checksum policy。 |
| clean adoption | 仍需用户确认；泛型 point type expansion 未闭合。 |

## 阶段决策

`EvidenceDecision`: `adopted_production_behavior`

当前 patch 已保留；本阶段曾把泛型点型扩展列为下一阶段。020 阶段已经执行该扩展并将当前生产行为
更新为 traits-gated normal AoS path。

## point_type_expansion_queue

| phase | 范围 | 恢复条件 | 必需证据 |
| --- | --- | --- | --- |
| `020-generic-normal-point-types` | `PointXYZRGBNormal`、`PointXYZINormal`，以及 `RVVXYZNormalFloatLayout<PointNT>` 可表达的 normal-like AoS 类型 | 用户确认保留当前 `PointNormal` patch 后 | production direct correctness、fallback、bench、asm、board、Evidence Doctor |
| `030-production-repeated-values-summary` | 输出 per-iteration B/A values，消除 `low_run_count` warning | 若用户要求更强稳定性证据或 reviewer 要求 | 重复板卡 summary、binary hash、taskset/governor/freq/temperature |

## continue / stop decision

PI5 停止条件已由用户确认解除；本阶段完成 adoption closeout。历史 `next_phase_default` 已由 020
阶段闭合；当前默认恢复入口见 phase index 和 020 result。
