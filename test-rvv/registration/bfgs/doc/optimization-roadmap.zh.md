# bfgs 优化路线图

## 当前边界

当前 topic 只覆盖 `registration/include/pcl/registration/bfgs.h` 的 BFGS 优化器诊断。production 源码保持不变，`doc-rvv/registration/bfgs-RVV.zh.md` 不适用。当前确认 caller 是 GICP 的 `Vector6d` BFGS 路径；其它 registration 调用方需要重新用源码或 profile 证明。Phase 020 已证明 test-only direction update candidate 可编译、可在 QEMU 运行，但 Milkv-Jupiter board repeated 为 negative，因此默认停止 production 推进。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| Eigen vector expression asm baseline | 当前源码 `dot()`、`norm()`、向量表达式 | `moveTo()`、`slope()`、`minimizeInit()`、direction update | 判断 compiler / Eigen 是否已经生成 RVV，避免重复手写 | 内联模板归属困难；小维度可能无收益 | QEMU build、asm attribution、case symbol map | `partial` | production 前再细化 |
| Direction update fused diagnostic | `minimizeOneStep()` 中多个 dot/norm + linear combination | GICP-shaped `Vector6d` 和 128 维诊断 | 减少多次小向量遍历和临时表达式成本 | 维度 6 可能太小；functor cost 可能主导 | correctness、asm、board microbench、doctor | `board negative / no-production` | stop |
| Move-to + slope combined diagnostic | `moveTo(alpha)` 后常接 `slope()` | line-search cache path | 可能合并 `x_alpha` 生成和 dot | cache key、functor 回调和内存写回语义风险 | cache tests、same-chain checksum、asm | `correctness passed / no default expansion` | stop，除非用户要求负向原因分析 |
| GICP caller hotspot audit | confirmed caller | `estimateRigidTransformationBFGS()` | 判断局部 BFGS 是否影响真实 GICP | nearest search、cost functor、gradient 可能主导 | caller-shaped smoke、profile 或 board timing boundary | `not_unblocked` | direction update negative，默认不执行 |
| Line-search scalar control | source scan | `lineSearch()` / `interpolate()` | none | 标量控制流和 functor 回调为主 | boundary correctness only | `not_applicable with evidence` | 无 |

## 默认恢复队列

1. `000-current-state-and-gaps`：Phase 000 建立 topic-local scaffold、evaluation、roadmap 和 matrix。
2. `010-diagnostic-scaffold-and-asm-probe`：已创建 Makefile、test support、correctness tests 和 asm probe；仍不改 production。
3. `020-board-direction-update-diagnostic`：已完成 board repeated microbench 和 Evidence Doctor；结果 negative。
4. `diagnostic closeout`：默认停止。`030-caller-hotspot-audit` 不再自动排队。
5. `PI1-production-integration-plan`：当前不允许；只有用户明确授权且另有新证据时才可重开。

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | 先做 Eigen baseline asm | BFGS 已使用 Eigen 表达式，小维度下自动向量化可能已经足够 | asm attribution | high |
| 000 | 把 line-search 标成 not_rvv_target | 源码显示标量控制流和 functor 回调主导 | boundary tests | high |
| 000 | GICP-only caller scope | 仓库引用只确认 GICP 使用 BFGS | caller smoke / profile | medium |
| 010 | 先测 board direction-update，而不是直接进 production | QEMU correctness 和 doctor clean 只证明 test-only helper 可继续；还缺真实性能 | board repeated summary / doctor | high |
| 010 | Move-to+slope 保持暂缓 | 正确性已通过，但它比 direction update 更接近 line-search cache 语义，production 外推风险更高 | direction-update board 结果 | medium |
| 020 | board negative 后停止默认推进 | `direction-update-vector6` / `vector128` 5-run B/A 全部低于 1 | Phase 020 result / doctor Errors=2 | high |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| Production dispatch in `bfgs.h` | 当前没有 correctness、asm、board 或 caller hotspot 证据 | Phase 010 / 020 / 030 均成立且用户授权 |
| NDT caller claim | 当前源码检索未确认 NDT 使用 `bfgs.h` | 找到真实 caller 或 profile 证据 |
| Line-search RVV | 标量控制流、cache 和 functor 回调为主 | 只有 profile 显示 line-search 内部纯向量表达式是热点才重开 |
| Direction update production probe | Phase 020 board repeated 为 negative，doctor 报 2 个 degradation error | 用户明确要求 bounded negative-analysis 并给出新的正向假设 |
