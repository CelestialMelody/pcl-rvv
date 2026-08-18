# 诊断文档与生产回退

## 授权边界

诊断文档必须写清当前主题属于哪一类：

- `local fragment`：只证明局部片段正确或有潜力。
- `production-shaped diagnostic`：test-only 入口接近生产调用形状，但尚未改生产源码。
- `production direct`：真实生产入口已接入并直接验证。
- `bench-only / diagnostic`：默认只在专项 test/bench、诊断文档和状态表内活动，不承诺生产分流。
- `生产回退说明`：曾尝试生产接入，但目标硬件性能或维护边界不成立，生产路径已回收。

## Production-shaped 与 Production Direct

`production-shaped diagnostic` 是 test-only 入口接近真实生产调用形状；它不等同于真实生产入口已接入。文档需要用代码或伪代码对照两者：

```text
diagnostic entry: public-like setup -> candidate RVV stage -> scalar tail
production entry: real public API -> real dispatch/gate -> production RVV or Std
```

若已经有真实生产接入，再补：

```text
production direct case: real public API -> real production RVV gate -> checksum / bench / target hardware
```

代码对照至少说明：

- diagnostic 是否继承目标上游类、共同父类，还是 wrapper。
- diagnostic 复用了哪些公开 setter、默认参数、准备流程、缓存和输出语义。
- 真实 production gate 位于哪里，哪些条件命中 RVV，哪些条件落回 Std。
- test-only helper、诊断宏或局部 staging 是否仍存在，以及是否参与默认生产分流。

## A/B 对比口径

报告 A/B、fused-vs-baseline 或其它候选对比时，文档必须有单独的“对比口径”段落或表格，至少列出：

- A/B 含义：A 是哪个 baseline，B 是哪个 candidate。
- A/B 两侧调用路径：test-only helper、public-like wrapper、真实 public overload、production dispatch 等。
- A/B 是否同边界：同边界时写清共享的 wrapper、gate、mask、reduction、solve 和 checksum 口径；混合边界时必须显式命名为交叉检查，并说明不能作为严格候选收益。
- `std/RVV speedup` 的定义：同一个 case 内 `std_ms / rvv_ms`，不能直接解释为 candidate 相对 baseline 的收益。
- candidate-vs-baseline 指标的定义和方向，例如 `B/A = A_rvv_ms / B_rvv_ms`，并说明 `>1` 才表示 B 更快。
- direct diagnostic、production-shaped diagnostic、production direct 各自能证明什么，是否可作为 production evidence。

## 未覆盖生产面

production-shaped 证据必须列出尚未覆盖的生产面，常见项包括：

- fake indices、显式 subset、mask、乱序或重复 index。
- 点类型 traits、字段类型、organized cloud、dense/non-dense。
- 对象生命周期、缓存刷新、search object、target/input 切换。
- warning、error、early return、异常路径和副作用。
- 上游原始测试、公开 API 组合或多模块调用路径。

未覆盖生产面不能埋在段落里，应使用表格或清单，并写明它们是否阻塞生产接入。

## 接入前后 Evidence 区分

生产接入前的 evidence 可来自 local fragment、full diagnostic 或 production-shaped diagnostic。生产接入后必须补真实 production direct evidence。

文档应分开记录：

- 接入前：local fragment correctness、local microbench、full diagnostic checksum、full diagnostic target hardware 结果。
- 接入后：真实公开入口专项测试、真实 production case checksum、QEMU 路径和反汇编、目标硬件 production case。
- 仍保留的 diagnostic：用于局部归因、边界复现、fallback 验证或回归，不再写成未来生产候选的单独证据。

若当前环境无法跑目标硬件或 production case，closeout 写清待补的具体命令和缺口，不把缺口写成已完成结论。

## 诊断 / bench-only 升级生产

升级生产路径不能只看 microbench 或局部片段收益。至少需要：

- full diagnostic 或 production case 在目标硬件上稳定收益。
- RVV 覆盖入口主成本，或入口极常用且改动很小。
- checksum、专项测试、必要上游测试通过。
- 反汇编确认预期 RVV 指令路径。
- fallback 边界清晰。
- 语义风险和维护复杂度可控。

如果 local fragment 快，但 full 入口被后续主成本稀释，默认不接生产。文档写清“不接生产”的原因，避免后续只引用片段 speedup 重新推进。

## 主成本稀释复核

当 full diagnostic 或 production case 被 sort、search、tree traversal、map、Eigen、状态机、整点复制或外部库逻辑稀释时，不能只写“被稀释”。评估和主题文档应补：

- 公开入口调用点。
- wrapper / virtual / dispatch 层。
- 真实实现层。
- 主要耗时代码片段。
- 该主成本是规整 VL chunk 线性算子，还是强分支、不规则访存、树遍历、候选集合维护、solver 状态机。
- local fragment、full diagnostic、production case 的耗时拆分。
- 可 RVV 片段占 full 入口比例和理论收益上限。

若热点边界可定位但不适合当前手写 RVV，写明重新评估需要的新算法、批量接口、数据布局或 profile 证据。

## 生产回退说明

如果已经修改生产源码但目标硬件性能不成立，应回收默认生产分流。正确但不加速的 helper 可保留在专项诊断代码或显式诊断宏中。文档必须记录：

- 还原范围。
- 保留的 test/bench 证据。
- 反汇编证据。
- 目标硬件结果。
- 不接生产原因。
- 后续重新评估条件。

## 语义异常记录

遇到 checksum、fallback 或 1 ulp 级异常时，文档按下面链路记录：

```text
现象 -> 最小复现 -> 路径命中 -> 反汇编线索 -> 因果实验 -> 修复或暂缓 -> 回归测试
```

如果使用显式 FRM/FCSR、fused intrinsic、边界 lane 标量回退或局部优化限制，评估文档和主题文档都要说明原因和证据。
