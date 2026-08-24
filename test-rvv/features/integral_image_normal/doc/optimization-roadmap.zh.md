# Integral Image Normal Optimization Roadmap

## 当前边界

当前 topic scope 是 `IntegralImageNormalEstimation` 的 organized image normal 主路径。Phase 050 已把
`computeFeature()` 的 depth-change map 和 distance-map initialization 前缀接入 production probe
（生产探针），真实 public `compute()` 入口板卡证据为 weak-positive（弱正向）。用户已确认采纳并保留该
production patch；当前 map-prep 前缀为 adopted production behavior（已采用生产行为）。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| map-prep RVV masked stores | current source shape | depth-change map and distance init | 减少逐像素分支和初始化成本 | 诊断 helper 不覆盖完整 public `compute()` | unit correctness, QEMU, asm, board diagnostic bench, Evidence Doctor | diagnostic-positive / source for production probe | closed by Phase 050 |
| map-prep production probe | Phase 000 positive result + user PI2/PI5 authorization | bounded `computeFeature()` prefix | 把已证明的 map-prep 收益接入真实 public `compute()` | production direct 只有 1.06x mean / median，且各有 1/5 run 低于 1；覆盖范围仍限 `RVVXYZAoSFloatLayout<PointInT>` | PI1 plan, production direct correctness, asm, 5-run board, Evidence Doctor, PI5 user decision | adopted weak-positive production-public | closeout in `doc-rvv/features/integral_image_normal-RVV.zh.md` |
| topic-local doc suite | phase-loop structure audit | diagnostic evidence and recovery docs | 让 reviewer 不依赖对话即可复核 target、证据和代码地图 | production direct docs 需等 PI2/PI5 | role docs, README inventory, phase result closeout | adopted | refresh after PI2 if production patch is authorized |
| average 3D gradient diff buffer RVV | queue hint and source loop | `initAverage3DGradientMethod()` diff_x / diff_y | 大图 tail 输入有局部收益 | 常规规模 near-threshold 且 1/5 退化；production 还包含 integral image 构建和泛型 `PointInT` layout gate | dedicated correctness, asm, board bench, Evidence Doctor | attempted / weak-size-dependent-diagnostic | resume only after full profile or production-shaped ablation shows diff buffer dominates |
| average 3D gradient production-shaped profile | Phase 020 resume condition | diff-buffer + integral image construction + normal query diagnostic | 判断 diff-buffer 是否在完整 AVERAGE_3D_GRADIENT 链路中主导 | 使用测试专用积分图 / query harness，不是 production direct；profile total 方向摇摆 | QEMU log-shape, asm, 5-run board, Evidence Doctor | attempted / negative-unstable | do not production-patch diff-buffer |
| exact PCL IntegralImage2D boundary profile | Phase 030 open question | diff-buffer +真实 PCL `IntegralImage2D<float,3>::setInput()` +真实 PCL query loop | 判断 Phase 030 负向是否来自测试专用积分图近似 | 仍是 test helper；`pcl_avg3d_profile_*` 仅 near-threshold，组件长尾明显 | QEMU label/checksum smoke, asm, 5-run board, Evidence Doctor | attempted / weak-unstable | do not production-patch diff-buffer; return to map-prep PI2 authorization gate |
| distance transform rewrite | current source shape | first / second pass distance propagation | 可能减少 map 后处理成本 | 行内依赖强，算法替换风险高；当前 production closeout 不宜继续扩大范围 | algorithm proof, correctness corpus, board ablation | deferred / separate-follow-up | only if future profile shows distance propagation dominates |
| production integration closeout | Phase 050 evidence + user PI5 confirmation | bounded production helper in `computeFeature()` | 正式记录已采用生产行为 | weak-positive 需要保留风险边界；其它方向不外推 | user confirmation, doc-rvv topic doc, final verification | adopted | no further production expansion in current topic |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | map-prep production probe | 5-run board diagnostic 为 stable positive，足以进入生产接入计划，但不能直接采纳 production | PI1 plan、production direct correctness、fallback、asm、board repeated、Evidence Doctor、PI5 用户确认 | high |
| 000-doc-suite | topic-local doc suite | Phase 000 生成 board summary 和 Evidence Doctor，需要独立 role 文档承载 target 字典、证据边界和代码地图 | production direct 发生后刷新对应 role 文档 | completed |
| 020 | diff-buffer profile gate | `avg3d_diff_320x240` near-threshold 且出现 1/5 退化，说明该候选不应单独推进 production；若要恢复应先证明它在完整 AVERAGE_3D_GRADIENT 中占主成本 | full profile、production-shaped ablation 或同 production boundary A/B | medium |
| 030 | exact PCL IntegralImage2D boundary profile | production-shaped profile 显示完整 `avg3d_profile_641x481_tail` mean `0.98x` 且 2/5 退化；积分图构建和 query 成本占比大，但当前 harness 不是 PCL 真实 `IntegralImage2D` | exact PCL `IntegralImage2D::setInput()` boundary profile、或新的 prefix/query algorithm proof | low until user chooses non-production exploration |
| 040 | exact PCL boundary did not reverse diff-buffer decision | `pcl_avg3d_profile_320x240` mean `1.01x` 且 1/5 退化；`pcl_avg3d_profile_641x481_tail` mean `1.04x` 但 2/5 退化；`pcl_iin_setinput_dxdy_641x481_tail` 3/5 退化 | 若未来继续，需要另开更细的 PCL integral/query follow-up 或 production direct plan，不能直接接 diff-buffer | low / separate follow-up |
| 050 | production direct confirms small public-entry benefit | `prod_compute_avg_depth_320x240` 和 `prod_compute_avg_depth_641x481_tail` 的 median / mean 都为 `1.06x`，但各有 1/5 run 低于 1 | 用户已确认采纳；长期 `doc-rvv` 已创建 | completed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| direct vectorization of `computePointNormal()` | integral image queries, Eigen solver and viewpoint flip make it unsuitable as first slice | profiling shows per-point solver dominates and a smaller numeric kernel can be isolated |
| distance transform rewrite | 行内依赖和算法替换风险高；Phase 000 只证明 map-prep 前缀，不证明两遍传播值得重写 | production/full profile 或 component ablation 显示 distance propagation 是主成本 |
| average 3D gradient diff buffer production patch | Phase 030 production-shaped profile 显示 total 链路弱 / 不稳定：`avg3d_profile_641x481_tail` mean `0.98x`、2/5 退化；diff-buffer 局部收益被积分图构建和 query 成本稀释 | 只有 exact production profile 反转该结论、且用户授权 production patch 时恢复 |
| exact PCL IntegralImage2D diff-buffer production patch | Phase 040 exact PCL profile 仍是 weak / unstable：`pcl_avg3d_profile_320x240` mean `1.01x`、`pcl_avg3d_profile_641x481_tail` mean `1.04x` 且 2/5 退化，组件 `setInput` / query 长尾明显 | 只有 production direct evidence 或另一个同边界 RVV-vs-RVV family A/B 明确反转，并且用户授权 production patch 时恢复 |
| immediate second production patch in this topic | 当前唯一正向生产证据来自 map-prep 前缀；其它方向要么已被证据降级，要么需要算法级证明和新的 profile 问题 | PI5 完成后，如用户想继续，可另开 distance transform / normal solver / PCL integral query 专项 phase |
