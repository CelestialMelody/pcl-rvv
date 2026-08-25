# Phase 020 Point Type Expansion Result

## 实际执行范围

本阶段没有修改 production（生产源码）。实际执行范围与 `plan.zh.md` 一致：在 Phase 010 已采纳的
`OrganizedEdgeBase<PointT, pcl::Label>::compute()` depth label path（深度标签路径）上，补齐更多
`RVVXYZAoSFloatLayout<PointT>` 点型的 production-public（真实公开入口）证据。

已验证范围：

- `PointT`：`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal`。
- `PointLT`：仍固定为 `pcl::Label`。
- 入口：真实 public `compute()` / `extractEdges()`，bench wrapper 只负责合成 organized grid 并调用公开入口。
- 数据布局：AoS 单 float xyz 字段；本阶段只读取 z 字段并写 label bit。

未验证范围：

- 泛型 `PointLT` 或自定义 label 字段 gate。
- 所有 PointXYZ-like traits 集合、混合 source/target 点型、自定义点型、非 float z 或非标准 AoS。
- RGB / normal Canny 派生入口和 `assignLabelIndices()` RVV 化。
- 真实数据集输入分布。

## 计划动作回填

| 动作 | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 typed production correctness | done | `make -C test-rvv/features/organized_edge_detection run_test_compare` | Std/RVV 构建各 7/7 pass，新增 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` production compute 对拍。 |
| A2 point-type production bench | done | `make -C test-rvv/features/organized_edge_detection run_bench_production_rvv BENCH_ARGS='--iterations 1 --warmup 0 --case-filter prod_depth_pointxyzi_finite_320x240,prod_depth_pointxyzrgb_nan_boundary_320x240'` | QEMU smoke（小型验证）只证明 bench label、case-filter 和日志形状可运行，不作为性能证据。 |
| A3 manifest / registry | done | `log/board/point-type-evidence_manifest.json`、`log/evidence_registry.json` | Phase 020 四个 point-type case 已登记为 fresh。 |
| A4 asm | done | `make -C test-rvv/features/organized_edge_detection check_production_rvv_asm` | production bench 中 `organizedEdgeDepthLabelsRVV` 仍含关键 RVV 指令。 |
| A5 board repeated | done | `make -C test-rvv/features/organized_edge_detection run_board_organized_edge_detection_point_type_repeated` | 5-run board summary 全部 checksum match，四项 decision bucket 均为 positive。 |
| A6 docs | done | 本文、phase index、optimization matrix、roadmap、evaluation、README、`doc-rvv` | 文档同步到 Phase 020 当前 truth，未把已测具体点型外推成完整泛型结论。 |

## Correctness / QEMU

`run_test_compare` 的 Std / RVV 构建各有 7 个 gtest 全部通过。新增 production direct 测试分别构造
`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` organized cloud，并用真实 `compute()` 与 test-local scalar
reference（测试本地标量参考链路）对拍 label bits 和 label index 顺序。

QEMU bench smoke 只用于确认 point-type case label、`--case-filter` 和 production bench binary 可运行。QEMU
timing（仿真器计时）不参与本阶段性能判断。

## Board Performance

板卡证据来自 `log/board/point-type-repeated-summary.md`，证据角色为 `production-public`，A/B boundary
（对照边界）为 public `OrganizedEdgeBase<PointT, pcl::Label>::compute()`。5-run 预算已用完，四个 case 的
decision bucket 均稳定为 `positive`。

| case | runs | mean B/A | median B/A | min B/A | max B/A | checksum |
| --- | --- | --- | --- | --- | --- | --- |
| `prod_depth_pointxyzi_finite_320x240` | 5 | `5.327x` | `5.380x` | `5.151x` | `5.390x` | match |
| `prod_depth_pointxyzrgb_finite_320x240` | 5 | `5.437x` | `5.438x` | `5.380x` | `5.479x` | match |
| `prod_depth_pointxyzrgbnormal_finite_320x240` | 5 | `4.158x` | `4.216x` | `3.952x` | `4.271x` | match |
| `prod_depth_pointxyzrgb_nan_boundary_320x240` | 5 | `2.822x` | `2.828x` | `2.805x` | `2.832x` | match |

`PointXYZRGBNormal` 的 median 低于同组其它 finite 点型，但仍远高于 `1.05x` threshold（阈值），且 min B/A
也保持正向。因此本阶段把它单独报告为 positive，不继承 `PointXYZI` / `PointXYZRGB` 的更高收益。

## Evidence Doctor 和 Registry

| 项 | 结果 | 处理 |
| --- | --- | --- |
| Evidence Doctor | `log/board/point-type-evidence_doctor.md`：Errors=0，Warnings=1，Suggestions=4 | Warning 是 `PointXYZRGBNormal` group outlier（组内离群）；按点型分开报告，不把其它点型收益外推给它。Suggestions 均为环境 metadata 缺失，不阻塞当前 positive 结论。 |
| evidence registry | `log/evidence_registry.json` 记录 summary、manifest、doctor 为 fresh | `evidence_status` 应继续扫描 Phase 020 result、summary、manifest 和 doctor。 |

未解决 warning 的结论边界：`PointXYZRGBNormal` 可能因为 stride、layout、字段 offset 或 cache locality
（缓存局部性）表现低于同组其它点型；当前证据仍支持该具体点型 positive，但不支持“所有更大 stride 点型都有同等收益”。

## Diagnostic-to-Production Mismatch Audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public`。 |
| A/B boundary | public `OrganizedEdgeBase<PointT, pcl::Label>::compute()`。 |
| 当前决策问题 | scope expansion：同一 adopted RVV family 是否可扩到更多已测 `PointT`。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接使用真实 public entry。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 使用同一 bench wrapper、同一点型、同一 organized grid 和同一 timer boundary。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 当前没有弱 / 负 / 中性 / 不稳定 case；若后续点型出现该 bucket，只限制该点型 adopted scope，不自动回滚 Phase 010。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；本阶段没有选择新 RVV family，只扩展同一 production helper 的已验证点型范围。 |

## Optimization Matrix 更新

- `depth_labels_point_type_expansion`：`PointXYZI`、`PointXYZRGB`、`PointXYZRGBNormal` 的 finite 320x240 case
  进入 `covered_by_phase020`；`PointXYZRGB` 的 NaN boundary case 也进入 `covered_by_phase020`。
- `depth_labels_production_rvv`：adopted scope 从 Phase 010 的 `PointXYZ + pcl::Label` 扩展为
  `PointXYZ / PointXYZI / PointXYZRGB / PointXYZRGBNormal + pcl::Label` 的已测 production-public evidence。
- `PointLT` 泛型、自定义点型、RGB / normal Canny 和 `assignLabelIndices()` 不随本阶段关闭。

## Continue / Stop Decision

本阶段完成。当前 depth production path 的已测点型扩展已经闭合 correctness、QEMU smoke、asm、board repeated、
Evidence Doctor 和 registry；继续做“泛型 `PointLT` label gate”会修改 production gate 设计和输出 label traits
语义，收益没有当前 profile 支撑，不建议在本 topic 内继续默认推进。

后续方向按边界拆分：

- `030-rgb-normal-derived-entries`：属于 RGB / normal Canny 前处理的独立诊断，不应从 depth path 外推。
- `assign-label-indices-ablation`：只有 profile 显示 label index 收集成为主成本时恢复。
- 更宽自定义点型 / `PointLT` 泛型：需要新的 traits 设计、fallback 测试、production direct bench 和板卡证据；当前不建议继续优化。

默认下一动作：刷新文档和验证后停在 review / commit 边界；没有当前授权范围内、同一 depth path 上仍值得继续的未阻塞优化动作。
