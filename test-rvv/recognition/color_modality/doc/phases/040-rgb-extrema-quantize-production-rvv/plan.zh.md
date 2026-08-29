# Phase 040: RGB extrema quantize production RVV plan

## 阶段意图和边界

本阶段在 Phase 030 已采纳的 production direct（真实生产路径）基础上，继续尝试把 `ColorModality<PointXYZRGB>::quantizeColors()` 的 RGB extrema quantize（RGB 极值颜色量化）接入 RVV。Phase 030 已证明公开 `processInputData()` 中的 3x3 color filter RVV 路径在板卡上为 positive；本阶段只评估同一公开入口内新增 quantize RVV 是否还能带来增益。

`validated_scope`：`ColorModality<PointXYZRGB>::processInputData()`，organized `PointXYZRGB`，`width >= 3 && height >= 3`，`float` 标量语义等价的 8-bin RGB extrema 量化，接已有 RVV filter 和 `QuantizedMap::spreadQuantizedMap()`。

`unvalidated_scope`：其它带 `r/g/b` 字段的模板点型、非标准布局、非 organized 输入、`extractFeatures()`、`computeDistanceMap()`、其它 row source 和完整模板生成端到端 profile。

## 候选族

| candidate family | idea | risk | expected evidence |
| --- | --- | --- | --- |
| `cm-rgb-extrema-quantize-rvv` | 用 `offsetof(PointXYZRGB, r/g/b)` 和 `vlse8` 从 AoS 点云中读取 RGB byte；用整数缩放比较复刻 production float 距离选择和 tie-break（并列时选择较早 bin） | Point layout 必须限制在 `PointXYZRGB`，`dist_7 * 1.5` 需要用 `2x` 缩放等价表达，避免浮点比较漂移 | forced scalar/RVV public entry 对拍、QEMU smoke、asm gate、board repeated、Evidence Doctor |

## 实现和测试动作

| action | artifact / command | done condition |
| --- | --- | --- |
| 扩展生产 helper | `recognition/include/pcl/recognition/color_modality.h` | `processInputData()` 在 `__RVV10__` 下先尝试 quantize RVV，再走 filter RVV 和 spread；失败回退 `processInputDataStd()` |
| 扩展测试支撑 | `test-rvv/recognition/color_modality/src/test_cm.cpp` | forced scalar/RVV public entry 对拍仍覆盖 quantized/spreaded map；新增或现有 path hook 能区分 RVV |
| 扩展 bench | `src/bench_cm.cpp` / Makefile 默认 production_process case | case label 不变，新的 RVV build 使用新生产路径 |
| 运行验证 | `run_test_compare`、QEMU smoke、`check_cm_rvv_asm` | correctness / path-hit / asm 通过，QEMU timing 不作性能结论 |
| 板卡证据 | `board_repeated record_evidence_state_repeated` | 5-run production direct summary 与 Evidence Doctor 刷新 |

## Evidence Doctor 和决策桶

- repeated board：5 runs，20 iterations，3 warm-up。
- positive：median B/A >= 1.20 且 `B/A < 1` 为 0。
- weak-positive：median 1.05-1.20 且无退化。
- neutral / negative / unstable：不自动回滚，先记录 mismatch / family-selection 边界，再决定是否保留 Phase 030 或继续试探。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production direct |
| A/B boundary | public overload |
| 当前决策问题 | RVV-family-selection：新增 quantize RVV 是否优于 Phase 030 的已有 production RVV family |
| diagnostic 是否可外推到 production | not_applicable，本阶段直接跑 production direct |
| comparison-boundary / baseline mismatch 风险 | yes；若只看 public Std/RVV positive，不能证明新 family 优于 Phase 030，需要用 Phase 030 summary 作历史 production RVV baseline 对照 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已经是 bounded probe；若不 positive 或低于 Phase 030，应恢复 Phase 030 filter-only RVV |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes；本阶段至少用同一 case 的 Phase 030 当前 summary 做 family-selection 对照，必要时保留为 bounded candidate |

## 继续 / 停止条件

若 Phase 040 production direct 相比 Phase 030 不低于当前收益且 Evidence Doctor 无 Errors，则采纳 quantize+filter production family。若收益低于 Phase 030 或出现 correctness / asm / doctor blocker，则拒绝本阶段 quantize RVV，保留 Phase 030。若 Phase 040 采纳后仍有未阻塞高价值方向，下一阶段只能来自 `extractFeatures()` / `computeDistanceMap()` profile 或更细的 quantize/filter消融；没有 profile 证据时停止并整理。
