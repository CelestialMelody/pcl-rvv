# debayer phase index

| phase | 状态 | 默认恢复动作 | 文档 |
| --- | --- | --- | --- |
| `000-current-state-and-bilinear-interior` | complete / no-production for current bilinear family | 不继续当前 `debayerBilinear` 内区 RVV family；若要恢复 debayer topic，先新建 `010-edge-aware-profile-or-branch-distribution` 或等价 profile / 分支分布诊断 phase。 | `000-current-state-and-bilinear-interior/plan.zh.md`、`000-current-state-and-bilinear-interior/result.zh.md` |

当前 topic scope（主题范围）是 `io/src/debayer.cpp` 的 Bayer RGB 转换族。当前 phase scope（阶段范围）只覆盖 `DeBayer::debayerBilinear` 的 full-size contiguous Bayer（连续 Bayer 输入）内区 2x2 stencil（邻域模板）诊断，不覆盖 Bayer 输入 padding、边界行列、edge-aware（边缘感知）和 production dispatch（生产分流）。

## 当前恢复判断

`000-current-state-and-bilinear-interior/result.zh.md` 已记录两种 `debayerBilinear` 内区 RVV candidate（候选链路）在板卡同边界 benchmark（性能测试）中稳定为 negative：strided-store 约 0.94x / 0.95x，segmented-store 约 0.93x。当前 topic 不建议把 bilinear inner family（双线性内区实现族）接入 production（生产源码），也不建议继续做同族 store / LMUL 微调。

后续只有在出现新的 profile（性能剖析）、load-count ablation（加载数量消融）或 edge-aware 分支分布证据时恢复。恢复入口应先写新的 phase plan；不要从当前 negative diagnostic（诊断证据）直接进入 production integration loop（生产接入闭环）。
