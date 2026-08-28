# Phase 050: 点类型扩展计划

## 阶段意图和边界

Phase 045/046 已采纳 `selectWithinDistanceRVV` 的 `vcompress` production path，但当前 board performance（板卡性能）只覆盖 `PointXYZ`。生产 gate 使用 `pcl::rvv::RVVXYZFloatLayout<PointT>`，理论上可覆盖注册了单个 float x/y/z 字段的 PointXYZ-like 点型。本阶段先补 `PointXYZI` dedicated board evidence（独立板卡证据），验证额外字段和更大 stride 是否仍值得走当前 production dispatch。

本阶段不扩大 public API，不改 `getDistancesToModel`，不把 `PointXYZI` 结果外推到 `PointXYZRGB`、`PointXYZRGBA` 或自定义点型。若 `PointXYZI` 接入后 board 结果为 negative 或 Evidence Doctor 对 select 行报未处理 Error，需要重新审视 production gate 边界并暂停给用户 / reviewer 判断。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| production 状态 | `selectWithinDistance` 当前采用 `vcompress` RVV path，gate 为 `RVVXYZFloatLayout<PointT>` + 32-bit offset。 |
| correctness | gtest 已有 `PointXYZILayoutMatchesReference`，证明 `PointXYZI` x/y/z traits/offset correctness。 |
| bench 缺口 | `src/bench_sac_model_sphere.cpp` 固定 `using PointT = pcl::PointXYZ`，第三个 CLI 参数不会选择 `PointXYZI`。 |
| RED 证据 | `make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZI'` 后，`Dataset:` 不包含 `PointXYZI`，说明 bench 还不能产生点类型 case。 |
| manifest 缺口 | `generate_sphere_board_evidence_manifest.py` 固定 `point_type: PointXYZ`，metadata name / gate 也固定 PointXYZ。 |

## validated_scope / unvalidated_scope

| 字段 | 范围 |
| --- | --- |
| validated_scope | `PointXYZI`、direct indexed `indices_`、float x/y/z、AoS stride、65536 点、public `selectWithinDistance` / `countWithinDistance` / `getDistancesToModel` bench 输出；性能结论只看 board repeated。 |
| unvalidated_scope | `PointXYZRGB`、`PointXYZRGBA`、自定义 registered xyz 点型、`Scalar=double`、其它 row source、其它规模。 |
| point_type_expansion_queue | 本阶段闭合 `PointXYZI`；若 positive，可把 RGB/RGBA 留作后续 phase 或记录为低优先级扩展；若 negative，暂停审查 gate。 |
| phase_closeout_boundary | 只关闭 `PointXYZI` performance row；不关闭全部泛型模板点型。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board target | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production select dispatch with `vcompress` point-type expansion | direct indexed `indices_` | `PointXYZI`, float x/y/z, extra intensity ignored | `run_test_compare` | `collect_point_type_repeated_board_evidence` with `PointXYZI` | `dump_bench_rvv` keeps `selectWithinDistanceRVV` attribution | `run_point_type_board_evidence_doctor` | planned |

## 实现和测试动作

| 动作 | 产物 | 预期证据 |
| --- | --- | --- |
| bench point-type 参数 | `src/bench_sac_model_sphere.cpp` | `BENCH_ARGS='128 1 PointXYZI'` 输出 `Dataset: ... PointXYZI ...`。 |
| manifest point_type 解析 | `script/generate_sphere_board_evidence_manifest.py` | manifest comparisons 的 `point_type`、`name`、`gate` 使用日志中的 `PointXYZI`。 |
| Make evidence targets | `Makefile` | Phase 050 collect / generate / doctor / record / status target 可恢复。 |
| QEMU correctness / asm | `run_test_compare`、`clean_bench_rvv dump_bench_rvv` | correctness 通过；`selectWithinDistanceRVV` 仍有 RVV 指令和 `vcompress.vm`。 |
| board repeated | `collect_point_type_repeated_board_evidence` | 5-run board summary，decision bucket 按 B/A 判断。 |
| registry | `record_point_type_board_evidence_state point_type_evidence_status` | summary evidence fresh。 |

## Evidence Doctor 和 registry 规则

Phase 050 使用独立 manifest / doctor：

- `doc/phases/050-point-type-expansion/point-type-repeated-evidence-manifest.json`
- `doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.md`
- `doc/phases/050-point-type-expansion/point-type-repeated-evidence-doctor.json`

Doctor Error 若属于 `getDistancesToModel` 当前未采纳行，按 Phase 045 规则降级；若属于 public `selectWithinDistance` 的 checksum、strict A/B 或退化，则不能关闭为 adopted。

## 板卡复跑预算和决策桶

预算为 5-run board repeated，一次完成后若 decision bucket 稳定则不追加复跑。`B/A > 1.0` 且全 run 正向记为 positive-stable；小幅收益但全 run 正向记为 weak-positive；跨 1 摆动记为 unstable；`B/A <= 1.0` 多数或全数为 negative。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production direct point-type expansion |
| A/B boundary | Std build public `selectWithinDistance<PointXYZI>` vs RVV build public `selectWithinDistance<PointXYZI>` |
| 当前决策问题 | 当前泛型 layout gate 对 `PointXYZI` 是否值得保持生产分流 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接跑 production public entry。 |
| comparison-boundary / baseline mismatch 风险 | 低；Std/RVV 两侧使用同一 bench wrapper 和同一 point type 参数。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已在当前 production gate 内；若负向，需要暂停审查是否收窄 gate。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要，且本阶段 board repeated 正是该证据。 |

## 文档更新清单

完成后更新本 phase result、optimization matrix、roadmap、benchmark/evidence、testing overview、optimization evidence、`doc-rvv` 的点类型边界，以及 Handoff。
