# 040 point-type-production-expansion 计划

## 阶段意图和边界

本阶段在用户确认采纳 030 生产补丁后，验证 `ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices&)` 的点类型扩展边界。当前 production（生产源码）已使用 `RVVXYZAoSFloatLayout<PointT>` 作为 xyz AoS layout gate（布局准入条件）；本阶段不修改 public API（公开接口），也不新增 window-open RVV，只补 `PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 的 production public correctness（真实公开入口正确性）、bench（性能测试）、asm（反汇编）和 Evidence Doctor（证据体检）证据。

本阶段证明范围：

| 维度 | 本阶段覆盖 |
| --- | --- |
| public entry | `ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices&)` |
| row source | full input cloud + current `ground` vector，真实 public overload |
| point type | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`；保留既有 `PointXYZ` 对照 |
| Scalar / layout | xyz 单个 `float` 字段，AoS layout，`RVVXYZAoSFloatLayout<PointT>` gate |
| 输入规模 | board bench 默认 `--size 262144 --half 4 --iterations 8 --warmup 2` |
| production 层级 | `production-public`，A/B boundary 为 `public_overload` |

本阶段不证明：

- 不证明所有 `PCL_XYZ_POINT_TYPES` 都有相同收益；normal 复合点型、用户自定义点型和 `Scalar=double` 仍需单独 phase。
- 不证明 OpenMP 多线程与 RVV 的组合收益；bench 继续用 `setNumberOfThreads(1)`。
- 不重开 window-open RVV。030 production public 已在标量 window-open 形态下正向，window-open 只有在后续 profile 或用户要求时再做。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| 生产补丁 | `extract` 先尝试 `apmfExtractRVV`，失败回到 `extractStd`。 | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` |
| 030 板卡证据 | `PointXYZ` dense median 1.60x，non-dense median 1.35x，均为 positive。 | `log/board/production-public-repeated-v2/summary.md` |
| correctness | Std/RVV 9 个测试通过；已有 `PointXYZI` public 对拍。 | `src/test_apmf.cpp` |
| bench | production bench 目前只构造 `PointXYZ` dense / non-dense。 | `src/bench_apmf_production.cpp` |
| manifest wrapper | production public 点型目前写死 `PointXYZ`。 | `script/generate_apmf_board_evidence_manifest.py` |

## Diagnostic-To-Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | `production-public` |
| A/B boundary | `public_overload` |
| 当前决策问题 | `RVV-vs-scalar`，判断同一点型公开入口 RVV 构建是否快于标量构建 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接跑 production public |
| comparison-boundary / baseline mismatch 风险 | 低。Std/RVV 两侧使用同一 bench wrapper、同一输入构造、同一参数和 checksum policy。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不需要新 probe。若新增点型不正向，本阶段只收窄对应点型的性能结论，不自动回滚已正向的 `PointXYZ` production patch。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要。本阶段不是新 RVV family selection（实现族选择），只是同一 adopted family 的点型覆盖扩展。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / board target | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `point-type-production-expansion` | full input cloud + current ground | `PointXYZI` / float / xyz AoS | production public `extract` | 新增 dense / non-dense public 对拍 | repeated board production public | `check_production_rvv_asm` | manifest + doctor | planned |
| `point-type-production-expansion` | full input cloud + current ground | `PointXYZRGB` / float / xyz AoS | production public `extract` | 新增 dense / non-dense public 对拍 | repeated board production public | 同上 | 同上 | planned |
| `point-type-production-expansion` | full input cloud + current ground | `PointXYZRGBA` / float / xyz AoS | production public `extract` | 新增 dense / non-dense public 对拍 | repeated board production public | 同上 | 同上 | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| 补 public correctness | `src/test_apmf.cpp` | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` dense / non-dense 都与 `PointXYZ` reference 对拍一致。 |
| 扩 production bench | `src/bench_apmf_production.cpp` | bench label 能输出 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 的 dense / non-dense。 |
| 扩 manifest 点型解析 | `script/generate_apmf_board_evidence_manifest.py` | 每个 production public label 的 `point_type` 字段按 case 正确写入。 |
| 本地验证 | Make target / 显式编译 | `run_test_compare`、`check_production_rvv_asm`、显式 RVV 编译通过。 |
| 板卡 repeated bench | `log/board/point-type-production-repeated-v1/` | 至少 3-run；若 bucket 稳定则关闭，若摇摆则最多再扩到 5-run。 |
| Evidence Doctor | `summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | Errors=0；Warnings / Suggestions 必须解释或降级边界。 |

## 板卡复跑预算和决策桶

默认先跑 3-run repeated board。若所有新增点型的 dense / non-dense decision bucket 均稳定在 `positive` 或 `weak-positive`，本阶段用 3-run 关闭；如果出现 near-threshold、方向摇摆或 Evidence Doctor finding，最多扩到 5-run。`positive` 使用 `min(speedup) >= 1.2`；`weak-positive` 使用 median >= 1.05；接近 1 或跨方向时写 `neutral` / `unstable`，不无限复跑。

## 继续 / 停止条件

若新增点型 production public 证据正向，保留泛型 `RVVXYZAoSFloatLayout<PointT>` gate，并把长期 `doc-rvv` 和 evaluation 更新为 adopted + point-type-expanded evidence。若某点型正确性失败或 Evidence Doctor Error 无法修复，停止并收窄 gate 或标为 blocked。若所有新增点型收益中性或负向但 correctness 通过，不自动回滚 `PointXYZ` adopted patch；需要在 result 中说明是否建议后续收窄性能声明或生产 gate。

下一阶段默认入口取决于本阶段结果：若点型扩展稳定正向且 roadmap 没有当前授权内的高优先级未阻塞动作，则进入 review-ready closeout；若仍存在未测 `PCL_XYZ_POINT_TYPES` 但属于更宽范围，写入后续扩展队列而不外推。

## 文档更新清单

- `doc/phases/040-point-type-production-expansion/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/approximate_progressive_morphological_filter-evaluation.zh.md`
- `doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md`
- `doc-rvv/library-screening/segmentation/segmentation-function-evaluation-queue.zh.md`
- `tmp/rvv-work-logs/segmentation/approximate_progressive_morphological_filter/current-handoff/`
