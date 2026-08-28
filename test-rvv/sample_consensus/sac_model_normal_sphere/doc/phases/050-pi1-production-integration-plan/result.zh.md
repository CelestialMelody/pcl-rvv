# Phase 050: PI1 production integration plan 结果

## 阶段范围

| 项 | 内容 |
| --- | --- |
| validated_scope | PI1 计划本身：候选范围、source / normal layout gate、fallback、production direct 测试计划、PI2-PI5 暂停条件和用户授权边界。 |
| unvalidated_scope | production patch、production direct correctness、production asm、production board repeated、其它 normal 点型、自定义点型、`Scalar=double` 和 PI5 采纳 / 回滚决定。 |
| phase_closeout_boundary | 只关闭 PI1 plan-ready 状态；不把测试专用 diagnostic 升级为 production-ready。 |

## 实现结果

本阶段新增 `doc/phases/050-pi1-production-integration-plan/plan.zh.md`，并同步 README、phase index、optimization roadmap、optimization matrix、evaluation、topic-local role 文档和 sample_consensus 保留候选复筛清单。

production 文件 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 未修改。`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md` 仍为 not_applicable，因为当前没有 adopted production behavior（已采用生产行为）、用户确认保留的 production patch（生产补丁）或 PI5 生产证据闭环。

## PI1 决策

当前决策是 `PI1 plan ready / PI2 pending explicit user authorization`。Phase 000-030 的测试专用证据支持一个 bounded production probe（有界生产探针）：

- 三入口候选：`countWithinDistance`、`selectWithinDistance`、`getDistancesToModel`。
- source 点型：`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。
- normal 点型：初始 PI2 只覆盖 `pcl::Normal`。
- source gate：`pcl::rvv::RVVXYZAoSFloatLayout<PointT>`。
- normal gate：初始收窄到 `pcl::Normal`；其它 normal 点型进入后续 expansion queue。
- byte offset gate：source 和 normal cloud 分别满足 `rvvMaxU32ByteOffsetElements`。
- fallback：非 `__RVV10__`、小规模、unsupported layout / point type、非覆盖 normal 点型、规模超界和未来 `Scalar=double` 路径走标量。

## Diagnostic 到 production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | 当前仍是 `production-shaped diagnostic` / `component ablation`；没有 production direct。 |
| A/B boundary | 当前 A/B 在 test helper 边界；PI4 必须换成 production public 或 production detail helper 边界。 |
| 当前决策问题 | 是否允许进入 PI2 bounded production probe。答案是计划上可控，但需要用户明确授权。 |
| diagnostic 是否可外推到 production | 不能外推为采纳，只能外推为 PI2 候选。 |
| comparison-boundary / baseline mismatch 风险 | 已记录。public bench 行当前不是 production RVV 命中证据。 |
| weak / negative / neutral / unstable 时 bounded probe 条件 | `PointXYZI` 的 `vcompress` B/A 为 weak-positive；允许 PI2 的条件是保持 PI1 冻结范围，并用 production direct 重新判定。 |
| clean adoption 是否需要同一 production boundary RVV-vs-RVV A/B | 需要。PI5 前不能 clean-adopt。 |

## Point Type Expansion Queue

| 范围 | 当前状态 | 恢复条件 |
| --- | --- | --- |
| `PointXYZ/PointXYZI/PointXYZRGB/PointXYZRGBA + pcl::Normal` | PI1 候选范围。 | 用户授权 PI2 后做 production direct correctness、asm、board 和 Evidence Doctor。 |
| 其它 `PCL_NORMAL_POINT_TYPES` | deferred。 | 先补 normal layout correctness、fallback、board 和 doctor，不能借 `pcl::Normal` 外推。 |
| 自定义 registered source 点型 | deferred。 | 用户指定点型或新增 traits-based generic scope；需补 layout、fallback 和 board。 |
| `Scalar=double` 或非 float 字段 | not_applicable in current PI1。 | 需要独立数值策略和 traits / helper 证据。 |

## Evidence Doctor 和 registry

本阶段没有新 board benchmark（板卡性能测试）或新 manifest（证据清单），因此未新增 Evidence Doctor 报告。当前依赖 Phase 000-030 已登记摘要，完成前重新运行 `make -C test-rvv/sample_consensus/sac_model_normal_sphere evidence_status` 检查 freshness。

## 阶段结论

Phase 050 已完成 PI1 production integration plan。继续推进会进入 PI2 production patch，并修改 production 源码；按仓库规则，这一步需要用户明确授权。本阶段没有命中板卡不可用、Evidence Doctor Error、证据矛盾或 dirty isolation 不安全。

## 继续 / 停止判断

`continue_stop_decision`：turn_stop_deferred with stop_condition_hit。

`stop_condition_hit`：继续需要扩大到未授权 production 文件修改。板卡当前由用户说明可用，但本阶段不需要新板卡 run；板卡可用性会在 PI4 production evidence rerun 中使用。

`next_phase_default`：等待用户明确授权进入 PI2 production patch；授权后按 `plan.zh.md` 冻结的三入口、四种 source 点型、`pcl::Normal` normal cloud、fallback 和证据计划连续推进 PI2-PI5，并在 PI5 停在用户确认点。
