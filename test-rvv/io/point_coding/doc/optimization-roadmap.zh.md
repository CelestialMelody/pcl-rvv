# point_coding Optimization Roadmap

## 当前边界

当前 topic 已完成 production integration loop（生产接入闭环）。Phase 060 在真实
`PointCoding<PointXYZ>::decodePoints` production boundary（生产边界）下完成 decode RVV patch、
correctness、反汇编和 10-run 板卡复跑：production-direct median 为
`1.22x / 1.23x / 1.18x / 1.16x`，Evidence Doctor 为 `Errors=0，Warnings=1`。Phase 070 进一步把 gate 扩为
`pcl::rvv::kRVVXYZAoSPointCompatible<PointT>`，在 `PointXYZI` / `PointXYZRGB` 代表点型上完成 10-run
production-direct board repeated：median 为 `1.27x / 1.20x / 1.27x / 1.19x`，Evidence Doctor 为
`Errors=0，Warnings=2`。用户已确认接入后板卡测试有收益即可采纳，因此当前 decode 是 adopted production behavior（已采纳生产行为）。Phase 020 证明 f64 exact encode quantize（精确双精度编码量化）正确但太重，encode 仍不接 production。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production-shaped leaf-size sweep | Phase 000 board warning 和筛选文档里的 leaf size 风险 | test-rvv helper，不改 production | 判断真实 leaf 小规模是否吞掉收益 | synthetic case 仍不像完整 octree | correctness、bench、board repeated、doctor | attempted / diagnostic-positive for encode | completed in Phase 010 |
| f64 exact encode quantize | Phase 000 f32-vs-double correctness 问题 | encode path | 让量化也能 RVV 化，而不是 gather 后回标量 | f64 convert/div 成本过高，tiny leaf 退化 | correctness、asm、board A/B、doctor | rejected for default path | completed in Phase 020 |
| input-domain + boundary lane fallback | Phase 020 f64 negative 后的剩余路线 | encode path | 只让远离整数边界的 lane 走 f32 fast path，其它 lane 回退标量 | 需要可证明 error bound；当前 production source 没有足够合同 | production 输入域审计、boundary correctness、asm、board A/B | blocked / rejected for implementation without contract | completed in Phase 030 |
| contiguous decode production-shaped context scout | Phase 040 decode-only 10-run weak-positive | decode output segment 到 `PointCoding` 对象状态 | 验证 decode 是否能穿透更接近 production 的 iterator / diff vector 状态 | 仍可能被完整 decompression pipeline 稀释；Phase 050 有 7 个 warning | production-shaped correctness、board repeated、doctor | attempted / weak-positive with instability warnings | completed in Phase 050 |
| full octree shaped context | retained candidate rescreen 的 octree family 风险 | combined point coder context | 判断 traversal / entropy 是否稀释 | 会接近 production 边界，可能需要更多授权 | production-shaped diagnostic plan、fallback audit | attempted / weak-positive with instability warnings for point-coder-only multileaf | completed in Phase 055 |
| public octree roundtrip feasibility | Phase 055 仍未覆盖 tree traversal / entropy | test-rvv public roundtrip smoke | 判断不改 production 能否构造公开压缩/解压往返诊断 | 不能写成 RVV 收益；只证明入口可运行 | plan、correctness smoke、QEMU gtest shape | completed feasibility smoke | completed in Phase 056 |
| production integration loop | 用户授权 PI1 后的 Phase 060 | `io/include/pcl/compression/point_coding.h` | 真实 production decode 收益 | Phase 060 的批准范围是 exact `PointXYZ`；traits-gated representative point types 已由 Phase 070 关闭，public end-to-end 已由 Phase 080 后续审计为 weak / near-threshold | PI1-PI5、production direct、board repeated、doctor | adopted production behavior | completed; formal doc-rvv created |
| PointXYZ-like traits expansion | Phase 060 exact gate 后自然扩展 | `PointCoding<PointT>::decodePoints` traits-gated xyz 点型 | 扩大覆盖到 traits-gated xyz AoS 点型 | 需要 traits / POD / offset / stride gate 和 fallback 证据 | correctness、fallback、asm、board repeated、doctor | adopted production behavior | completed in Phase 070 |
| full public octree end-to-end timing | Phase 070 后剩余边界 | `OctreePointCloudCompression` public encode/decode stream | 判断 point coder direct 收益是否被 tree traversal / entropy 稀释 | public boundary 结果接近阈值且有退化频率 Error / Warning | Phase 080 plan、public correctness、production-public board、doctor | attempted / weak-near-threshold; no further automatic production work | completed in Phase 080 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | encode quantize 安全修正 | 完整 f32 RVV 公式触发差 1 correctness 风险。 | 对抗样本、边界 lane fallback 或 double RVV A/B。 | high |
| 010 | decode 先降级，不作为生产探针主线 | Historical `decode_contiguous_16384` 有退化，后续需稳定性复核。 | 20-run / trace / asm attribution 后再恢复。 | medium |
| 020 | f64 exact quantize 拒绝为默认路线 | correctness 成立，但 tiny leaf 和中等 leaf 退化。 | 若继续 encode 量化，只能先做输入域 / error-bound 审计，再设计 boundary fallback。 | high |
| 030 | boundary fallback 需要输入域合同 | `PointCoding` 接收任意 double reference 和 float resolution，源码内没有足够范围 gate。 | 获得 production 输入域合同、profile 证据或用户确认的误差合同。 | high |
| 040 | decode-only 稳定性为弱正向 | 10-run decode median 均大于 1，Evidence Doctor 只有 `decode_contiguous_64` 长尾 warning。 | 下一阶段只能做 test-only production-shaped context scout。 | high |
| 050 | decode context 仍弱正向但有不稳定 warning | 10-run context median 均大于 1，但有退化和 long-tail warning。 | 若继续，先做 full-octree-shaped test-only scout 或 trace。 | high |
| 055 | multi-leaf context 仍弱正向且 warning 下降 | 10-run multileaf median 均大于 1，doctor 为 `Errors=0，Warnings=3`。 | 后续 Phase 056 已完成 public roundtrip feasibility。 | high |
| 056 | public roundtrip smoke 可构造 | 新增 smoke 调用真实 public encode/decode 往返。 | 继续到生产接入前必须先由用户授权 PI1。 | high |
| 060 | production-direct decode 有收益 | 真实 `PointCoding<PointXYZ>::decodePoints` 在板卡 10-run 中所有规模 median/min 均正向。 | 用户已确认采纳，正式 doc-rvv 文档已创建；若继续，进入 traits expansion phase。 | high |
| 070 | traits-gated decode 有收益 | `PointXYZI` / `PointXYZRGB` 代表点型 production-direct repeated 全部 median/min 正向，额外字段保持测试通过。 | 后续 Phase 080 已测 public boundary；当前不建议继续堆同类 synthetic traits case。 | medium |
| 080 | public boundary 收益被稀释 | public decode / roundtrip median 只有 `1.01x..1.02x`，Doctor 为 `Errors=1，Warnings=2，Suggestions=4`。 | 不建议继续自动改 production；若未来有真实 public workload/profile，再另开 profile / ablation phase。 | low |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| direct production patch without PI5 confirmation | 已解除：用户已确认采纳 Phase 060/070 patch。 | not_applicable。 |
| full f32 encode quantize v0 | correctness 边界差 1，不满足 same-chain 语义。 | 有安全修正或经用户 / reviewer 接受的误差合同。 |
| f64 exact encode quantize default path | correctness 成立但板卡 tiny leaf 和中等 leaf 退化频率过高。 | 仅作为显式 probe 复核 exact double semantics 成本；不作为默认 candidate 恢复。 |
| f32 fast path + boundary lane fallback | 当前源码无法证明全局 f32-vs-double 误差界。 | 获得 production 输入域合同、profile 证据或用户确认的误差合同后。 |
| more representative traits-only cases | Phase 070 已证明 traits gate 在 intensity/color representative 上正向；继续同类 synthetic case 很难改变当前生产决策。 | 出现具体用户点型、layout 风险或 reviewer 要求补某一已知 PCL 点型。 |
| more public octree synthetic runs | Phase 080 已显示 near-threshold 且有退化频率 Error / Warning；继续同类 synthetic run 只会更精细描述稀释。 | 出现真实 public workload、profile 指向 point coder decode 主导，或需要验证特定 compression profile。 |

## 默认恢复队列

| priority | phase | scope | status | resume condition |
| ---: | --- | --- | --- | --- |
| 1 | `070-pointxyz-like-traits-expansion` | generic point type expansion | adopted / closeout completed | Phase 070 证据支持采纳，正式 doc-rvv 已刷新。 |
| 2 | `080-public-octree-end-to-end` | public entry timing | attempted / weak-near-threshold; closeout completed | Phase 080 已完成；没有稳定 public-positive 结论。 |

当前没有同一 production helper 或 public octree synthetic boundary 内的 high-priority unblocked next action。继续优化需要新的真实 workload / profile / 具体点型需求，而不是继续微调现有 decode helper。
