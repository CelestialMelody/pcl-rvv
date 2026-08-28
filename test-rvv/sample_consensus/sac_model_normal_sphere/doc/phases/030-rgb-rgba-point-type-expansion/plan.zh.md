# Phase 030: PointXYZRGB / PointXYZRGBA 点型扩展计划

## 阶段意图和边界

Phase 000-020 已覆盖 `PointXYZ + Normal` 与 `PointXYZI + Normal`。当前 helper 使用 `RVVXYZAoSFloatLayout<PointT>` 读取 source xyz，并使用 `RVVNormalFloatLayout<PointNT>` 读取独立 normal cloud。按照泛型点类型 gate（点类型准入条件）策略，`PointXYZRGB` 和 `PointXYZRGBA` 的颜色字段不是 normal-sphere 距离公式输入，但它们的 AoS stride（结构数组跨步）和字段 offset 必须单独验证，不能从 `PointXYZI` 外推。

本阶段只扩展测试专用 diagnostic（诊断）边界：新增 RGB/RGBA correctness（正确性）和 bench（性能测试）入口，采集单次 board smoke，更新 manifest / Evidence Doctor / registry。不修改 production 源码，不把结果写成泛型 production adoption（生产采纳）。

| scope item | 本阶段冻结值 |
| --- | --- |
| validated_scope | `PointXYZRGB + Normal`、`PointXYZRGBA + Normal`，direct indexed source/normal，float xyz input，独立 `pcl::Normal` cloud。 |
| unvalidated_scope | 自定义 registered xyz 点型、`PointXYZRGBNormal` 这类 source 自带 normal 字段但仍使用独立 normal cloud 的组合、其它 normal 点型、`Scalar=double`、非 indexed 入口、production dispatch。 |
| point_type_expansion_queue | 本阶段闭合 RGB/RGBA；自定义点型需要先定义代表类型和字段语义，production 接入需要 PI1-PI5。 |
| phase_closeout_boundary | 只关闭 RGB/RGBA 测试专用点型扩展，不关闭全部模板点类型或 production。 |

## RED 证据

计划落盘前，为确认当前缺口，已运行：

```bash
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'
```

该命令返回 `Unsupported point type: PointXYZRGB (expected PointXYZ or PointXYZI)`，exit code 为 2。这个顺序早于本计划落盘，result 中会记录为 phase-order deviation（阶段顺序偏差）；后续实现仍按 TDD red/green（先失败再通过）闭合该具体缺口。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| source layout gate | helper 已按 `RVVXYZAoSFloatLayout<PointT>` 读取 source xyz；理论上可覆盖 RGB/RGBA，但未验证。 | `include/impl/sac_model_normal_sphere_access.hpp` |
| normal layout gate | normal cloud 仍使用 `pcl::Normal`，由 `RVVNormalFloatLayout<PointNT>` gate。 | `include/impl/sac_model_normal_sphere_access.hpp` |
| correctness | 当前 gtest 覆盖 `PointXYZ`、`PointXYZI` 和退化方向；未覆盖 RGB/RGBA。 | `src/test_sac_model_normal_sphere.cpp` |
| bench CLI | 当前只接受 `PointXYZ` / `PointXYZI`，RED 已证明 RGB/RGBA 缺口。 | `src/bench_sac_model_normal_sphere.cpp` |
| manifest | 当前 `DATASET_RE` 可解析任意点型文本；Make target 尚未提供 RGB/RGBA Phase 030 evidence。 | `script/generate_normal_sphere_evidence_manifest.py` |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| point type expansion | indexed source + indexed normal | `PointXYZRGB + Normal` / float input / AoS | count/select/getDistances production-shaped diagnostic | new RGB correctness case + `run_test_compare` | RGB bench CLI smoke and board smoke | run Phase 030 board smoke | same candidate helper instantiation | run Phase 030 doctor | planned |
| point type expansion | indexed source + indexed normal | `PointXYZRGBA + Normal` / float input / AoS | count/select/getDistances production-shaped diagnostic | new RGBA correctness case + `run_test_compare` | RGBA bench CLI smoke and board smoke | run Phase 030 board smoke | same candidate helper instantiation | run Phase 030 doctor | planned |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| 扩 correctness | `src/test_sac_model_normal_sphere.cpp` / `include/test_sac_model_normal_sphere.h` | 新增 RGB/RGBA layout case，Std/RVV QEMU 通过。 |
| 扩 bench fixture | `include/bench_sac_model_normal_sphere.h` | RGB/RGBA 填充颜色字段；距离公式只读 xyz。 |
| 扩 bench CLI | `src/bench_sac_model_normal_sphere.cpp` | `PointXYZRGB` / `PointXYZRGBA` 不再返回 unsupported。 |
| 扩 evidence target | `Makefile` / manifest generator | Phase 030 可生成 RGB/RGBA summary、manifest、doctor 并登记 registry。 |
| QEMU smoke | `run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'` 和 RGBA | Dataset 行显示目标点型，命令返回 0。 |
| board smoke | RGB/RGBA 各一次 `board_smoke` | 板卡 gtest 通过，bench 日志可解析。 |

## Evidence Doctor 和 registry 规则

Phase 030 生成：

- `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-summary.md`
- `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-manifest.json`
- `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-doctor.md`
- `doc/phases/030-rgb-rgba-point-type-expansion/board-evidence-doctor.json`

Evidence Doctor 如果只报告 `low_run_count`，结果可作为 single board smoke 的点型扩展诊断；若 RGB/RGBA 任一入口出现 negative 或 checksum mismatch，不能据此直接否定 production，需要先记录 diagnostic-to-production mismatch audit。

## 板卡复跑预算和决策桶

本阶段对 RGB/RGBA 各使用 `1/5` single board smoke。若任何主候选 speedup 接近 `0.95x-1.05x` 或出现跨点型方向反转，可同边界最多复跑一次；若仍摇摆则标为 unstable。桶定义沿用 Phase 020：`>=1.20x` positive，`>=1.05x` weak_positive，`0.95x-1.05x` neutral，`<0.95x` negative。

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic。 |
| A/B boundary | board Std build fallback vs board RVV build test-only candidate。 |
| 当前决策问题 | point-type expansion：RGB/RGBA source layout 是否能加入 PI1 候选讨论。 |
| diagnostic 是否可外推到 production | 不能直接外推。production 源码未改，缺少 production direct test、fallback 和 production asm。 |
| comparison-boundary / baseline mismatch 风险 | 有。测试专用派生类和 public overload 边界不同；颜色字段不参与当前公式，只能证明 source xyz layout。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许在用户授权后作为有界生产探针；弱 / 负诊断不能自动推出 no-production。 |
| clean adoption 是否需要同一 production boundary 内证据 | 需要 PI1-PI5，并在 PI5 后等待用户明确确认。 |

## 继续 / 停止条件

若 RGB/RGBA correctness、bench smoke、board smoke、Evidence Doctor 和 registry 均闭合，本阶段将更新 roadmap / matrix / evaluation，把 RGB/RGBA 标为已验证的测试专用点型扩展。继续到 production 仍需要用户明确授权；自定义点型扩展若没有代表类型定义，可写成 not_applicable 或 deferred with resume condition。
