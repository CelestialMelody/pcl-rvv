# ICP transformCloud 优化证据索引

## 本文职责

本文把当前采用、拒绝或暂缓的优化方式映射到 production 代码、test support、gtest、bench、board evidence
和风险边界。详细代码地图见 `doc/test-support-code-map.zh.md`，长期 production 行为见
`../../../doc-rvv/registration/icp-RVV.zh.md`。

## 当前结论摘要

`IterativeClosestPoint::transformCloud` 已采用 `Scalar=float` + AoS layout gate 的 RVV full-cloud path。
当前 EvidenceDecision 为 `production_direct_positive`，板卡 median speedup 为：

| family | representative cases | median speedup |
| --- | --- | --- |
| `xyz-masked-store-m2` | `PointXYZ` 64K / 256K | 5.68x / 5.30x |
| `xyz-normal-two-mask-m2` | `PointNormal` 64K / 256K | 3.76x / 3.93x |

## 优化方式总表

| 优化方式 | 状态 | production path | correctness | board evidence | 边界 |
| --- | --- | --- | --- | --- | --- |
| `xyz-masked-store-m2` | adopted | `transformCloudXYZRVV` | `ProductionDirectPointXYZMatchesScalar` | `xyz 64K/256K` repeated | 只覆盖 `Scalar=float`、AoS XYZ layout、runtime offsets 匹配。 |
| `xyz-normal-two-mask-m2` | adopted | `transformCloudXYZNormalRVV` | `ProductionDirectPointNormalFiniteBranches` | `xyz-normal 64K/256K` repeated | XYZ finite mask 与 normal finite mask 分离，normal 非有限不回滚 XYZ。 |
| production generic layout gate | adopted with representative performance | `tryTransformCloudRVV` + `pcl::rvv` traits | `PointXYZI` / `PointXYZINormal` direct tests | performance 由 `PointXYZ` / `PointNormal` 代表 | 未知自定义点型必须 traits 和 runtime offsets 同时通过。 |
| Std fallback extraction | adopted | `pcl::registration::detail::transformCloudStandard` | small input、`Scalar=double`、offset fallback tests | not_applicable | 保留上游标量语义，供 dispatch fallback 和 asm attribution。 |
| QEMU bench compare default guard | adopted workflow guard | `test-rvv/mk/rvv-topic.mk` | not_applicable | not_applicable | QEMU bench compare 只允许显式 `qemu_smoke_only`。 |
| `IterativeClosestPointWithNormals` | rejected for current topic | override 调用 `pcl::transformPointCloudWithNormals` | not_applicable | not_applicable | 另属 transforms / normals 函数族。 |
| indices / correspondences row source | not_applicable with evidence | ICP 主循环其它阶段 | not_applicable | not_applicable | `transformCloud` 输入已经 materialized，不读取 correspondence row source。 |

## 标量路径与 RVV 路径差异

| 项 | Std helper | RVV helper | 语义保护 |
| --- | --- | --- | --- |
| 入口 | `transformCloudStandard` | `tryTransformCloudRVV` 分流后调用 RVV helper | production direct tests 对拍。 |
| 数据访问 | per-point `memcpy` field offsets | strided segmented load/store by traits layout | runtime offset gate 不匹配则 fallback。 |
| XYZ finite | 每点 `std::isfinite` | `finiteMaskF32M2ICP` 三分量 mask 合并 | NaN/Inf tests。 |
| normal finite | XYZ 有限后再检查 normal | normal mask 与 XYZ mask 合并 | normal 非有限测试。 |
| 写回 | 只写 XYZ 和 optional normal | masked store 只写同一字段 | generic point tests 检查 intensity / curvature 保持。 |
| 小规模 | 标量循环 | `input.size() < 32` 不进入 RVV | small input test。 |
| `Scalar=double` | cast 到 `Matrix4f` 后标量执行 | `if constexpr (Scalar=float)` 才尝试 RVV | double fallback test。 |

## 代码级证据索引

| 对象 | 层级 | 证据角色 |
| --- | --- | --- |
| `IterativeClosestPoint::transformCloud` | production dispatch entry | `Scalar=float` gate、RVV 短路、Std fallback 调用。 |
| `pcl::registration::detail::transformCloudStandard` | production Std fallback | 上游标量语义保留点；fallback 和 asm attribution 的锚点。 |
| `finiteMaskF32M2ICP` | production RVV helper | NaN / Inf finite mask。 |
| `transformCloudXYZRVV` | production RVV helper | `PointXYZ` 和 generic XYZ layout RVV full-cloud transform。 |
| `transformCloudXYZNormalRVV` | production RVV helper | `PointNormal` 和 generic XYZ+normal layout RVV full-cloud transform。 |
| `tryTransformCloudRVV` | production dispatch helper | size、layout traits、runtime offsets 和 normal branch gate。 |
| `support::transformCloudStd` | test reference | correctness oracle。 |
| `support::transformCloudCandidate` | diagnostic candidate | Phase 001/002 的 test-only candidate。 |
| `src/test_icp.cpp` | correctness tests | 12 个 gtest 覆盖 production direct 与 fallback。 |
| `include/bench_icp.h` | bench harness | production direct transformCloud timing。 |
| `script/collect_icp_board_repeated.py` | board evidence script | 多轮 board compare summary。 |

## 细粒度 Target 字典

| target | 证据角色 |
| --- | --- |
| `run_test_compare` | QEMU Std/RVV correctness。 |
| `record_qemu_correctness_state` | 登记 QEMU correctness evidence。 |
| `run_board_test fetch_board_logs` | 板卡 correctness。 |
| `collect_board_transform_cloud_repeated` | board repeated production direct summary。 |
| `run_board_evidence_doctor` | board Evidence Doctor。 |
| `record_board_evidence_state` | 登记 board production direct evidence。 |
| `dump_bench_rvv` | RVV binary asm attribution input。 |
| `evidence_status` | evidence freshness 和 doc-ref 检查。 |

## 当前可提交证据

| 证据 | 路径 | 结论 |
| --- | --- | --- |
| board repeated summary | `log/board/transform_cloud_repeated/summary.md` | 4 个 case median 3.76x 至 5.68x。 |
| board Evidence Doctor | `log/board/transform_cloud_repeated/evidence_doctor.md` | Errors=0，Warnings=2，Suggestions=0。 |
| asm attribution | `doc/asm-attribution.zh.md` | RVV 指令簇属于 production `transformCloud` 符号。 |
| phase result | `doc/phases/003-production-integration/result.zh.md` | production direct positive。 |
| doc-suite parity | `doc/phases/004-structure-parity-doc-suite/result.zh.md` | 文档套件对齐完成。 |

## 结论边界

- `PointXYZI` / `PointXYZINormal` 的性能没有单独 repeated；当前以主布局族 representative performance 支撑。
- `PointXYZ 64K` 有 long-tail / group-outlier warning；结论保留 min/median/max，不跨 case 外推。
- 当前优化不覆盖 ICP 端到端、nearest-neighbor search、correspondence rejection、SVD solve 或
  `IterativeClosestPointWithNormals`。
