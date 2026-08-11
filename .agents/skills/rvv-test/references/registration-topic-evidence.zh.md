# Registration Topic 证据清单

本文适用于 PCL registration（配准）模块的 RVV topic（主题），尤其是
transformation estimation（变换估计）、point-to-plane LLS（点到平面最小二乘）、
weighted（加权）变体、symmetric point-to-plane（对称点到平面）和 correspondence
estimation（对应关系估计）。它补充通用 `rvv-test` 规则，不替代
`test-taxonomy.zh.md`、`entry-shapes-and-test-support.zh.md`、
`numerical-consistency.zh.md` 和 `performance-and-ablation.zh.md`。

## 共同证据边界

- public entry（公开入口）是否真实命中，必须和 test-only wrapper（测试专用包装）分开。
- row source policy（行来源策略）必须逐项列出：full-cloud（全云顺序扫描）、
  source-indexed（源索引路径）、dual-indices（双索引路径）和 correspondences（对应关系路径）。
- row source policy 只定义 row 的入口形态，不等于整条优化 family。full-cloud 里采纳的
  block-reduction、A/B/C/N block groups、fused formula 或 ILP code shape，可以作为
  candidate family 迁移到其它 policy 的起点，但不能默认继承为其它 policy 的结论。
  worker 必须按 policy 逐一尝试、记录或说明不适用原因。
- row source scope decision（行来源范围决策）必须逐项映射到代码路径、test target、bench target、board evidence 和当前状态。full-cloud adopted、indexed deferred 或 correspondences rejected 这类结论不能只写成自然语言。
- row source scope decision 不能只依赖默认综合 bench。默认综合 bench 可以作为 smoke 或 broad diagnostic（宽口径诊断）。用于判断 full-cloud、source-indexed、dual-indices 或 correspondences 取舍时，应提供专门的 row-source case-filter / make target，或把该项写成 `diagnostic gap`。
- row source bench target 应配套板卡入口。推荐形态是 `run_bench_row_sources`、`run_board_bench_row_sources` 和 `collect_board_row_sources_repeated` 分别覆盖 QEMU 窄 smoke、单次板卡 smoke 和 repeated board 诊断；默认不在 QEMU 上运行完整 bench。没有 repeated board 时，row source 取舍只能停留在诊断候选。
- registration topic 的测试数据应同时保留 deterministic corpus（确定性样本集）和 seeded random stress（带种子随机压力样本）：前者用于稳定回归和精确复现，后者用于暴露漏掉的边界。文档必须写清 corpus label、seed 和它们对应的 row source / point type / size，不能把一次偶然正向当成全覆盖。
- row source 诊断若稳定正向，worker 必须把触发日志单独写入证据索引，例如单次板卡 `analyze_bench_compare.log` 或等价摘要，再进入 production integration loop（生产接入闭环）。不能只凭默认综合 bench 口头升级，也不能把触发日志混写成最终 production summary。
- row source diagnostic 出现稳定正向时，worker 必须进入 production integration loop（生产接入闭环）：补 production helper、public dispatch、fallback / gate tests、dedicated bench target、board smoke、repeated board summary，以及 asm attribution 或等价路径证据。若仍不接 production，文档必须写出阻塞条件、负向证据或维护成本。
- row source 升级为 production 前，必须检查当前 topic 是否已有 adopted implementation family。若 full-cloud 已采用 block-reduction、A/B/C/N block groups、fused formula 或 ILP code shape，新 row source 不能默认沿用早期 staged-row / compressed-tail helper。worker 必须新增同边界 implementation-family comparison，或在 evaluation 中写出不适用原因，例如重复 gather 成本、寄存器压力、spill、VLEN / LMUL 限制、index staging 成本或 correctness 风险。
- 如果某个 policy 已有 adopted family，而另一个 policy 还没有尝试过该 family，worker 的默认顺序是先做 family carry-over audit：先在 test-rvv 中补同 family 的 policy-specific candidate、bench 和 board 证据，再决定 production integration。不要把一个 policy 的 positive summary 直接外推到其它 policy。
- row source 状态变化后，topic 文档中的范围决策表、target 表、EvidenceDecision、可提交证据白名单、evidence registry 状态和 `.gitignore` allowlist 必须同步更新。不能让旧的 `deferred` / `仅诊断` 结论和新的 production evidence 同时存在。
- RowSourcePolicy 只负责 row source。finite mask（有限值掩码）、formula（公式）、
  staging/reduction（暂存 / 规约）、`accepted_points`、`ATA/ATb` 或输出容器属于 shared math pipeline（共享数学流水线）或后段。
- mixed fields（混合字段）、point traits（点类型字段特征）、AoS stride（结构数组跨步）、
  gather（离散加载）、valid-index-only（仅有效索引）、production predicate（生产谓词）和
  invalid lane finite mask（无效通道有限值掩码）必须作为测试矩阵条目审计。
- diagnostic evidence（诊断证据）只说明诊断层证明了什么。未接 production 的诊断结论必须写“诊断证据链”，不能写成 production-ready（生产就绪）。

板卡 repeated run 之前，baseline 和 candidate 应分别通过 topic-local smoke。smoke 必须验证编译、RVV gate 命中、单次运行完成、checksum/accepted-point 输出以及同边界 correctness。这个步骤用于阻止 benchmark helper 的递归、错误模板实例化或 layout 绑定错误进入正式采集。repeated board summary 必须写清 run budget、decision bucket 和 registry / manifest / Evidence Doctor 路径；若人工复跑覆盖旧日志但未登记，row source 结论先标为 stale / refresh pending。

## 泛型 Gate 与代表性点型

当 production gate（生产门控）覆盖 layout-gated generic point types（布局门控泛型点类型）集合，
但 board（板卡）性能只覆盖 representative pointtypes（代表性点类型）时，主题文档和 Handoff
Packet（交接数据包）必须同时写清：

- gate-allowed（门控允许）集合：哪些字段、`Scalar`、layout（布局）、offset（偏移）和规模条件会命中 RVV。
- correctness（正确性）覆盖：QEMU、production direct tests（真实生产路径测试）、fallback tests（回退路径测试）和 traits（字段特征）测试覆盖了哪些组合。
- performance（性能）覆盖：repeated board（重复板卡测试）实际覆盖的代表性 source / target 点型组合、规模和 case filter（用例过滤条件）。
- boundary（证据边界）：未逐类型上板的 gate-allowed 点型只能继承 correctness 证据和代表性性能判断，不能写成逐类型性能已证明。
- risk（风险）和扩展条件：如果代表性点型不足以覆盖 mixed fields（混合字段）、异常 AoS stride（结构数组跨步）或 gather 形态，应说明是否需要收窄 gate，或要求新增板卡 A/B 后再扩大结论。

## Transformation Estimation / LLS

点到平面、对称点到平面和加权 LLS topic 至少审计：

- 标量参考链路来自真实 production 源码、同构 test reference（测试参考链路）还是 double reference（双精度参考）。
- `accepted_points`、finite mask、法方程输入行、`ATA/ATb`、Eigen solve（Eigen 求解器）和最终 matrix（矩阵）分别有什么证据。
- source/target 点类型字段是否真实满足 gate：source `x/y/z`、target normal 字段、weighted 路径的 weight 字段或对应权重容器。
- symmetric 变体必须说明 source normal、target normal、两侧残差公式和方向符号是否与标量语义一致。
- weighted 变体必须说明 weight load（权重加载）、`weight * normal`、无效权重或索引边界是否在 correctness 和 bench 计时边界内。
- public input semantics 必须先按 production API 审计。数量不匹配、空输入、0 权重、负权重、NaN/Inf 权重、非法 indices 和非法 correspondences 不能混成一类；其中非法 index / correspondence 只有在公开入口声明或源码边界明确时才写成测试合同。
- FMA（融合乘加）和 reduction（规约）相关变体必须有误差预算、反汇编归属和必要的板卡 A/B；不能只因源码表面未使用 fused 写法就默认禁止 fused，也不能跳过 near-cancellation（近抵消）样本。
- production direct 只批准已经有真实 dispatch、fallback、反汇编符号归属和 repeated board（重复板卡测试）性能证据的入口、点类型、`Scalar`、布局和规模。

### fused formula 证据闭环

fused formula（融合公式）在 registration topic 里要把四层证据闭起来：`correctness`、`component no-solve`、`production-shaped full estimate`、`representative point types`。如果任一层缺证据，就只能写成 diagnostic（诊断）或 candidate（候选），不能直接写 production。

当前这类 topic 的写法还应区分两种“看起来更快”的来源：一种是源码层面的 code-shape preference，另一种是真实不同机器码。像 `AbcdFused` 与 `AbcdFusedIlp` 这种 case，如果 asm 等价，就只能把 `Ilp` 写成源码调度诊断，不要把它当成已证实的独立机器码收益；只有 production-symbol asm attribution 和多轮板卡 B/A 都闭合后，才可以把它写成 production 候选。

board summary 里还要保留 warm-up、run count、绑核、governor、freq 和温度，并且用同一份 RVV binary 内的 RVV-vs-RVV B/A 做判断；不要把不同 std/RVV speedup 互相比较，也不要用 QEMU timing 代替板卡证据。

registration fused formula 的 helper A/B 必须同边界。`block-baseline` 和 `block-fused-*` 应共享 test_support helper 或 production helper 的外壳；真实 public overload 与 test_support helper 的对比只能作为 mixed-boundary cross-check（混合边界交叉检查），不能写成公式消融主表。

## Correspondence Estimation

对应关系估计 topic 至少审计：

- 输出语义：query/match index（查询 / 匹配索引）、distance、顺序、去重策略、无匹配 sentinel（哨兵值）和阈值边界。
- organized projection（有组织投影）或图像式邻域必须说明像素邻域、投影公式、边界裁剪、NaN/Inf、遮挡或深度竞争如何影响输出。
- 如果 RVV 只覆盖候选生成、投影、mask、压缩或评分前段，必须写清 scalar tail（标量尾段）如何继续维护输出顺序和对象状态。
- production-shaped diagnostic 可以证明入口形态和局部收益，但不能替代 production dispatch。public-entry-shaped 也不等于 production direct。
- performance（性能）结论必须来自目标硬件；QEMU timing（QEMU 计时）只用于路径和日志形状。

## 回头完善旧 Topic

历史文档中如果出现带 only 后缀的 diagnostic 标签，回头完善时按语义改写：

- 若结论是不接 production，写“未接 production 的诊断结论”，并补“诊断证据链”。
- 若结论是先用诊断验证价值，写 `diagnostic`（诊断），并列出进入 production integration loop（生产接入闭环）前还缺的证据。
- 若已有 production direct 证据，升级为“正确性与高效性证据链”，不要保留旧诊断标签作为最终结论。
