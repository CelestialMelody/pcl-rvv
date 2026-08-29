# BRISK 2D Optimization Roadmap

## 当前边界

当前 adopted 范围是 `Layer::halfsample()`、`Layer::twothirdsample()` 和 `ScaleSpace::constructPyramid`
中的 downsample helper 链。目标数据是 contiguous `uint8_t` organized image（连续组织图像字节）。
`BriskKeypoint2D::compute()` 已有 synthetic organized cloud public-entry evidence（公开入口证据），结果为
near-neutral；不覆盖真实 workload、AGAST/OAST detector、scale refinement 或 features 模块的 BRISK descriptor。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RVV halfsample stride-load | 当前源码 / SSSE3 先例 | `HALFSAMPLE` 派生层 | 小幅降低 downsample 成本 | halfsample 单项近阈值 | correctness、asm、5-run board、doctor | adopted | not_required |
| RVV twothirdsample stride-load | 当前源码 / SSSE3 先例 | `TWOTHIRDSAMPLE` 派生层 | 稳定弱正向 | 需要保持 `/9` floor 语义 | correctness、asm、5-run board、doctor | adopted | not_required |
| 更复杂的 packed / shuffle 形态 | 阶段反思 | downsample helper | 可能减少 load/store 或除法成本 | 代码复杂度上升，收益未证明 | RVV-vs-RVV detail A/B、asm、board | deferred | 只有完整 BRISK profile 指向 downsample 仍是瓶颈时恢复 |
| `BriskKeypoint2D::compute()` end-to-end bench | 阶段反思 | synthetic 完整 keypoint pipeline | 判断 adopted helper 对公开入口的真实影响 | synthetic 输入不代表真实 workload，AGAST/OAST 和 refinement 可能主导 | public-entry correctness、board bench、doctor | attempted / neutral | not_required for downsample closeout |
| AGAST/OAST detector RVV | 队列表保留候选 | detector row scan / score | 潜在主热点 | 深分支、输出顺序和 NMS 风险高 | component ablation、profile、correctness oracle | deferred | 另开 AGAST topic 或 BRISK detector phase |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | public `compute()` end-to-end 只作为后续扩展，不阻塞 downsample adoption | repeated board 显示 helper 链正向，但完整 pipeline 未覆盖 | 公开入口 oracle、board bench、doctor | medium |
| 000 | halfsample 复杂优化暂缓 | repeated board 已无反向但收益近阈值，复杂化不一定值得 | RVV-vs-RVV detail A/B | low |
| 010 | public `compute()` end-to-end 证据已补齐，显示 near-neutral | 公开入口 5-run board median 1.017x，说明 downsample 收益被 AGAST/OAST 等标量阶段稀释 | 若继续追求完整 pipeline，先补真实 workload profile 或 AGAST/OAST detector oracle | medium for new detector/profile phase |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| QEMU bench compare | QEMU 只证明正确性和日志形状，不能证明性能 | 不恢复为性能证据；只用于 smoke |
| SSSE3 bit-identical rounding | RISC-V / 非 x86 portable path 按标量 floor semantics 冻结；SSSE3 full-block 有 rounded average 差异 | 若上游要求跨架构逐位一致，再另做语义兼容审计 |
| 继续微调 downsample stride-load | Phase 010 显示完整 public compute 端到端收益仅 median 1.017x；更复杂 packed / shuffle 形态不太可能改变完整 pipeline 决策 | 只有真实 workload profile 显示 downsample 仍是公开入口主瓶颈，才恢复 RVV-vs-RVV detail A/B |
