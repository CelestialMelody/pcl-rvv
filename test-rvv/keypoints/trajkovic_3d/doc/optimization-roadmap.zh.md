# Trajkovic 3D Optimization Roadmap

## 当前边界

当前 topic 来自 keypoints 第二轮执行清单，目标源码是 `keypoints/include/pcl/keypoints/impl/trajkovic_3d.hpp`。当前已采纳窄范围 production RVV：precomputed normals 条件下的 `FOUR_CORNERS` / `EIGHT_CORNERS`、3x3、organized dense public `compute()` 路径。normal estimation（法线估计）、NMS（非极大值抑制）和更宽点类型仍是独立搜索空间。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `four-corners-response-rvv` | 当前源码 fixed-neighbor normal stencil | `FOUR_CORNERS` response map | 减少每像素 dot / sqrt / min 公式成本 | finite gate 和 null normal 语义；production NMS 稀释 | correctness、asm、board repeated、Evidence Doctor | adopted | Phase 000 + Phase 010 |
| `production-dispatch` | Phase 000 positive 后进入 public boundary | public `detectKeypoints()` | 真实入口收益 | normal estimation / NMS 可能稀释，模板点类型 gate 需谨慎 | PI1-PI5 production direct tests、board、doc-rvv | adopted narrow | Phase 010 |
| `eight-corners-response-rvv` | 与 2D Trajkovic 同构的 8 邻域公式 | `EIGHT_CORNERS` response map | 覆盖另一公开 method | 公式更长且标量临时 vector 语义要对齐 | 独立 correctness、asm、board、doctor | adopted | Phase 020 |
| `point-type-expansion` | 泛型点类型策略和当前 traits gate | `PointXYZ` 之外的 xyz / normal float layout | 扩大 production 命中范围 | 当前 board 只覆盖 `PointXYZ + Normal`，不能外推 | 代表点型 public tests、fallback、asm、board、doctor | deferred | new expansion phase |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | response-only helper 的局部收益高，但不能替代 production public 证据 | diagnostic 结果 median 2.317x-3.596x | Phase 010 public compute evidence | high, completed |
| Phase 010 | public compute 包含 NMS 后仍有 1.769x median speedup；non-dense controls 应作为 fallback control 而非 RVV speedup case | production manifest 只纳入 dense main path，避免把 fallback case 混入主路径收益 | 后续若扩展 non-dense RVV，必须独立建 phase | adopted current scope |
| Phase 020 | `EIGHT_CORNERS` 在同一 public boundary 上也保持正向，且 FOUR_CORNERS 回归控制未退化 | eight-corners helper 与 four-corners helper 可共享同一生产分流与证据链 | 当前 adopted scope 已闭合，可只保留 broader adoption 候选 | adopted current scope |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| normal estimation RVV | normal 生成在 `IntegralImageNormalEstimation`，不是本文件 response map 第一目标 | 若 production direct 显示 normal estimation 稀释 Trajkovic response 收益，可引用已有 integral_image_normal 证据或另开 topic |
| NMS RVV | 排序、occupancy map 和 critical `push_back` 有输出顺序风险 | response map 已采纳后，若 profile 证明 NMS 成本主导再评估 |
| non-dense RVV public path | 当前 production gate 让 non-dense input / normals 回退标量，保留原 invalid handling 语义 | 需要专门证明 mask 成本、invalid 分布和 public board 收益 |
