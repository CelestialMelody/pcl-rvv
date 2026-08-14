# ICP `transformCloud` 函数级评估

## 当前 EvidenceDecision

当前为 `production_direct_positive`：RVV path 已接入
`registration/include/pcl/registration/impl/icp.hpp` 的
`IterativeClosestPoint<PointSource, PointTarget, Scalar>::transformCloud`。接入边界是
`Scalar=float`、`input.size() >= 32`、PCL RVV AoS layout traits 通过，且运行期 `x/y/z`
与可选 `normal_x/y/z` field offset 和 traits offset 一致；否则调用
`pcl::registration::detail::transformCloudStandard` 标量 fallback。

板卡证据来自 Milkv-Jupiter 5-run production direct repeated benchmark：
`test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md`。median speedup 为
`PointXYZ 64K=5.68x`、`PointXYZ 256K=5.30x`、`PointNormal 64K=3.76x`、
`PointNormal 256K=3.93x`。Evidence Doctor：
`test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md`，
Errors=0，Warnings=2，Suggestions=0。

## 函数语义

`computeTransformation` 在初始 guess、每轮 ICP 迭代和最终输出阶段调用 `transformCloud`。目标函数按
`setInputSource()` 解析出的运行期 field offset，用 `memcpy` 从 `PointSource` 读取 XYZ。XYZ 任一分量
不是有限值时跳过整个点；否则执行 3x4 rigid transform 并写回 XYZ。若 source 有 normals，函数再读取
`normal_x/y/z`，normal 任一分量非有限时只跳过 normal 写回，已经写出的 XYZ 保持变换后状态。函数声明允许
input 和 output 是同一对象。

RVV 实现保持这些语义：不负责 `output = input`，只按原函数写回应该写回的字段。因此 caller 预置 output、
resize 后 output 或 in-place output 的行为仍由原调用边界决定。

## EvidenceDecision 审计

| 维度 | 状态 | 证据 |
| --- | --- | --- |
| production patch | pass | `registration/include/pcl/registration/impl/icp.hpp` 中 Std helper、RVV helpers 和 dispatch gate。 |
| production direct correctness | pass | QEMU Std/RVV 12 tests passed；board RVV 12 tests passed。 |
| fallback correctness | pass | small input、`Scalar=double`、runtime offset mismatch 和 in-place tests。 |
| generic layout correctness | pass | `PointXYZI` / `PointXYZINormal` production direct tests。 |
| board performance | pass | 4 个 board repeated case median 3.76x 至 5.68x。 |
| Evidence Doctor | pass with accepted warnings | `PointXYZ 64K` long-tail / group-outlier warning 已解释。 |
| asm attribution | pass | RVV 指令簇位于 production `transformCloud` 符号内。 |
| QEMU bench policy | pass | QEMU bench compare 只作为历史 smoke，不参与性能结论。 |
| doc-suite parity | pass | `doc/phases/004-structure-parity-doc-suite/result.zh.md`。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `IterativeClosestPoint::computeTransformation` | production public entry | ICP 主循环中决定何时调用 `transformCloud`。 | `Registration::align` | `transformCloud` | 入口调用链审计。 | `registration/include/pcl/registration/impl/icp.hpp` |
| `IterativeClosestPoint::transformCloud` | production dispatch entry | cast matrix，尝试 RVV，失败后调用 Std fallback。 | ICP 主循环和最终输出变换 | `tryTransformCloudRVV` / `transformCloudStandard` | RVV 接入点。 | `registration/include/pcl/registration/impl/icp.hpp` |
| `transformCloudStandard` | production Std fallback | 保留上游标量语义，按运行期 offset 变换 XYZ 和可选 normals。 | `transformCloud` | none | fallback coverage。 | `registration/include/pcl/registration/impl/icp.hpp` |
| `tryTransformCloudRVV` | production dispatch helper | 检查 size、layout traits、runtime offsets 和 normal branch。 | `transformCloud` | RVV helpers | production boundary。 | `registration/include/pcl/registration/impl/icp.hpp` |
| `finiteMaskF32M2ICP` | production RVV helper | NaN/Inf finite mask。 | RVV helpers | RVV intrinsics | numerical gate。 | `registration/include/pcl/registration/impl/icp.hpp` |
| `transformCloudXYZRVV` | production RVV helper | `PointT` AoS XYZ full-cloud masked transform。 | `tryTransformCloudRVV` | RVV load/store helpers | `PointXYZ` / generic XYZ path。 | `registration/include/pcl/registration/impl/icp.hpp` |
| `transformCloudXYZNormalRVV` | production RVV helper | XYZ 和 normal 两套 finite mask。 | `tryTransformCloudRVV` | RVV load/store helpers | `PointNormal` / generic normal path。 | `registration/include/pcl/registration/impl/icp.hpp` |
| `support::transformCloudStd` | test reference | 复刻 production 标量语义。 | gtest | assertions | correctness oracle。 | `test-rvv/registration/icp/include/impl/icp_transform_cloud.hpp` |
| `support::transformCloudCandidate` | diagnostic candidate | Phase 001/002 test-only RVV candidate。 | gtest | test-only RVV helpers | diagnostic carry-over。 | `test-rvv/registration/icp/include/impl/icp_transform_cloud.hpp` |
| `src/test_icp.cpp` | correctness tests | 12 个 gtest 覆盖 production direct 与 fallback。 | `run_test_compare` / board test | production `transformCloud` | correctness gate。 | `test-rvv/registration/icp/src/test_icp.cpp` |
| `include/bench_icp.h` | bench wrapper | 通过 exposed ICP 调用 production `transformCloud` 并输出 4 个 case。 | `bench_icp.cpp` | production `transformCloud` | board performance。 | `test-rvv/registration/icp/include/bench_icp.h` |
| `collect_icp_board_repeated.py` | analysis script | 多轮 board compare 并生成 summary。 | Make target | repeated summary | board evidence collection。 | `test-rvv/registration/icp/script/collect_icp_board_repeated.py` |
| `summary.md` | evidence output summary | 记录 board repeated median/min/max。 | collector | docs / Doctor | performance summary。 | `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` |
| `evidence_doctor.md` | evidence output summary | 记录 Errors / Warnings / Suggestions。 | Evidence Doctor | docs | risk audit。 | `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md` |
| `doc/benchmark-and-evidence.zh.md` | documentation section | bench/evidence 主归属。 | README / evaluation | reviewer | evidence boundary。 | `test-rvv/registration/icp/doc/benchmark-and-evidence.zh.md` |
| `doc-rvv/registration/icp-RVV.zh.md` | production long-term doc | adopted production 行为和长期证据链。 | README / evaluation | reviewer | production maintenance。 | `doc-rvv/registration/icp-RVV.zh.md` |

## 文档分工审计

| 信息类型 | 主归属 | evaluation 中的角色 |
| --- | --- | --- |
| 测试类型、入口和覆盖矩阵 | `doc/testing-overview.zh.md` | 引用，不复制完整矩阵。 |
| gtest case 语义 | `doc/correctness-tests.zh.md` | 引用关键覆盖结论。 |
| bench label、board repeated、Doctor 和提交边界 | `doc/benchmark-and-evidence.zh.md` | 引用 performance / risk 摘要。 |
| 优化方式和证据映射 | `doc/optimization-evidence.zh.md` | 引用 adopted / rejected 状态。 |
| test support 代码地图 | `doc/test-support-code-map.zh.md` | Traceability Map 覆盖关键对象。 |
| adopted production 行为 | `../../../doc-rvv/registration/icp-RVV.zh.md` | 引用长期结论和维护边界。 |
| doc-suite parity gate | `doc/phases/004-structure-parity-doc-suite/result.zh.md` | 作为 closeout validity 证据。 |

## 当前证据路径

| 路径 | 摘要 |
| --- | --- |
| `test-rvv/registration/icp/log/qemu/run_test_std.log` | QEMU 标量构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/qemu/run_test_rvv.log` | QEMU RVV 构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/board/run_test.log` | 板卡 RVV 构建 gtest：12 tests passed。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` | Milkv-Jupiter 5-run production direct repeated summary。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_manifest.json` | `evidence_role=production_direct`，包含 production boundary 和 asm boundary。 |
| `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md` | Errors=0，Warnings=2，Suggestions=0。 |
| `test-rvv/registration/icp/doc/asm-attribution.zh.md` | production symbol 反汇编归因。 |
| `test-rvv/registration/icp/log/evidence_registry.json` | evidence freshness registry。 |

历史 QEMU bench 文件 `test-rvv/registration/icp/log/qemu/run_bench_std.log`、
`test-rvv/registration/icp/log/qemu/run_bench_rvv.log`、
`test-rvv/registration/icp/log/qemu/analyze_bench_compare.log`、
`test-rvv/registration/icp/log/qemu/evidence_manifest.json` 和
`test-rvv/registration/icp/log/qemu/evidence_doctor.md` 只保留为 `qemu_smoke_only` 历史证据。
公共 Makefile 已默认禁止 QEMU `run_bench_compare`，除非显式 `ALLOW_QEMU_BENCH_COMPARE=1`。

## Accepted Risks

| 风险 | 当前处理 |
| --- | --- |
| `PointXYZ 64K` repeated warning | min/median/max 为 5.29x/5.68x/6.50x，全部 run 明显正向；不降级，但不把该 case 收益外推到其它 case。 |
| generic 点型 performance | `PointXYZI` / `PointXYZINormal` 只有 production direct correctness；性能由 `PointXYZ` / `PointNormal` 代表。 |
| microbench 范围 | 当前 bench 只计 `transformCloud` full-cloud，不声称 ICP 端到端整体加速。 |
| `IterativeClosestPointWithNormals` | 不走本函数；若优化应另开 topic。 |
| QEMU bench smoke 历史文件 | 只作历史日志形状，不能进入性能排序。 |

## 质量门禁摘要

| gate | status | evidence | missing_items |
| --- | --- | --- | --- |
| `current_optimization_section_ready` | pass | `../../../doc-rvv/registration/icp-RVV.zh.md` 的“当前采用的优化方式”。 | none |
| `correctness_efficiency_evidence_chain_ready` | pass | 本文 EvidenceDecision 审计和长期文档证据链。 | none |
| `document_ownership_matrix_ready` | pass | 本文“文档分工审计”。 | none |
| `traceability_map_ready` | pass | 本文 Traceability Map。 | none |
| `doc_suite_parity_closeout_ready` | pass | Phase 004 result。 | none |
| `bench_backend_choice_ready` | pass | `doc/benchmark-and-evidence.zh.md`：性能只看 board / target hardware。 | none |
| `qemu_bench_smoke_scope_ready` | pass | QEMU bench compare guard 和 historical smoke 边界。 | none |
| `board_evidence_paths_ready` | pass | board summary / Doctor / manifest 路径。 | none |
| `ready_for_review_validity_check` | pass | doc suite parity 全部 adopted；无 `phase_deferred + unblocked`。 | none |
