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
- RowSourcePolicy 只负责 row source。finite mask（有限值掩码）、formula（公式）、
  staging/reduction（暂存 / 规约）、`accepted_points`、`ATA/ATb` 或输出容器属于 shared math pipeline（共享数学流水线）或后段。
- mixed fields（混合字段）、point traits（点类型字段特征）、AoS stride（结构数组跨步）、
  gather（离散加载）、valid-index-only（仅有效索引）、production predicate（生产谓词）和
  invalid lane finite mask（无效通道有限值掩码）必须作为测试矩阵条目审计。
- diagnostic evidence（诊断证据）只说明诊断层证明了什么。未接 production 的诊断结论必须写“诊断证据链”，不能写成 production-ready（生产就绪）。

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
- FMA（融合乘加）和 reduction（规约）相关变体必须有误差预算、反汇编归属和必要的板卡 A/B；不能因为源码表面不是 fused 写法就默认禁止 fused，也不能跳过 near-cancellation（近抵消）样本。
- production direct 只批准已经有真实 dispatch、fallback、反汇编符号归属和 repeated board（重复板卡测试）性能证据的入口、点类型、`Scalar`、布局和规模。

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
