# Phase 061 计划：affine index fast-path production probe

## 阶段意图和边界

Phase 060 的 RVV-vs-RVV detail A/B 显示：当 source-indexed、dual-indexed 和 correspondence 的输入是 step=1 contiguous offset slice（连续偏移片段）时，test-only affine fast path 在板卡上 6/6 positive，median B/A `1.676x` 到 `2.750x`。本阶段把这个候选推进到 bounded production probe（有界生产探针）。

本阶段允许修改 production 源码，但只覆盖：

- `Scalar=float`。
- dense xyz AoS layout gate 已通过。
- `nr_points >= 16`。
- source-indexed：`indices_src` 是 step=1 contiguous；target cloud 是 selected target cloud，按 offset 0 连续读取。
- dual-indexed：`indices_src` 和 `indices_tgt` 都是 step=1 contiguous。
- correspondence：`index_query` 和 `index_match` 都是 step=1 contiguous，且按 correspondence 顺序递增。

本阶段不覆盖 stride > 1、reverse、shuffle、sorted-copy branch、custom layout 新证据、`Scalar=double`、非法 index / correspondence 或非 dense 输入。未命中 fast path 时必须保持现有 gather / sorted-copy / 父类 fallback 行为。

## 当前状态清单

| area | current state |
| --- | --- |
| Phase 060 detail A/B | board 6/6 positive，Doctor `0/3/0`；但未计入通用 detection。 |
| production row-source path | 当前 source-indexed / dual-indexed / correspondence 走 gather；correspondence 还存在 size/disorder sorted-copy branch。 |
| correctness | 当前 Std/RVV 22 tests passed。 |
| production risk | 新 branch 必须在 sorted-copy heuristic 前后保持语义清晰：contiguous fast path 只能处理连续 order，shuffle-like correspondence 仍应保留 sorted-copy 逻辑。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness target | bench target | board evidence | decision |
| --- | --- | --- | --- | --- | --- | --- |
| `affine-index-fast-path-production-probe` | source-indexed | `PointXYZ -> PointXYZ` / `float` / dense xyz AoS / contiguous offset | new public correctness guard | production probe case-filter | required | planned |
| `affine-index-fast-path-production-probe` | dual-indexed | 同上 | 同上 | 同上 | required | planned |
| `affine-index-fast-path-production-probe` | correspondence | 同上 | 同上 | 同上 | required | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 production contiguous detection | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | helper 能识别 step=1 contiguous `pcl::Indices` 和 `pcl::Correspondences`，并返回 offset/count。 |
| A2 production fast accumulation | 同上 | source-indexed / dual-indexed / correspondence 命中时使用 contiguous offset RVV accumulation。 |
| A3 correctness guard | `src/test_tesvd_scale.cpp` | public overload 覆盖三类 contiguous row-source 输入，与 selected-cloud reference 一致；非 contiguous 测试继续覆盖现有路径。 |
| A4 bench case-filter | `src/bench_tesvd_scale.cpp` / Makefile | 新增 production public probe case-filter，比较 Std/RVV public path；不再使用 test-only candidate label。 |
| A5 evidence | QEMU / board / Doctor / registry | QEMU smoke clean；board repeated positive 或按证据降级。 |
| A6 文档回填 | result、matrix、roadmap、topic docs | 若 positive，进入 PI5 用户确认；若 negative / unstable，保留 patch 等待用户决定回滚或继续。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production_public_affine_index_fast_path_probe`。 |
| A/B boundary | Std public row-source path vs RVV public row-source path，fast path 由 production dispatch 命中。 |
| 当前决策问题 | Phase 060 positive candidate 接入 production 后，包含 detection 的 public path 是否仍有收益。 |
| diagnostic 是否可外推到 production | Phase 060 不能直接外推；本阶段用 production public probe 补齐。 |
| comparison-boundary / baseline mismatch 风险 | 低于 Phase 060，但仍需确认 case-filter 只命中 contiguous fast path，不混入 sorted-copy 或 shuffle path。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 probe；若弱 / 负 / 不稳定，暂停在 PI5 检查点，等待用户决定。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | Phase 060 已提供 detail A/B；本阶段补 public Std/RVV probe。 |

## 板卡复跑预算和决策桶

- 使用现有 5-run repeated board target。
- 64K / 256K × 3 row source，共 6 个 case。
- `positive`：median B/A >= 1.20 且没有高频退化。
- `weak_positive`：1.05 <= median B/A < 1.20；只在代码小、fallback 简单时可提交 PI5 positive packet。
- `neutral` / `negative` / `unstable`：保留 patch，不收口 adoption，等待用户确认回滚或继续诊断。

## 继续 / 停止条件

- 若 production public probe positive：输出 PI5 用户确认包，等待用户明确是否采纳。
- 若证据不支持：输出 PI5 negative / rollback option，等待用户授权。
- 若 correctness / QEMU 失败：修复或降级，不跑 board。
- 本阶段完成前不得把 production patch 视为 adopted。
