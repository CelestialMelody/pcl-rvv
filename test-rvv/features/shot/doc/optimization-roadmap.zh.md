# SHOT Optimization Roadmap

## 当前默认恢复动作

1. Restore at `PI3-shape-bin-production-rollback-closeout`. The PI2 production patch has been rolled back after user confirmation, and `features/include/pcl/features/impl/shot.hpp` currently has no SHOT RVV production path.
2. Default next action is no-production closeout verification. If correctness, evidence freshness and diff checks remain clean, stop the topic as `rollback/no-production`.
3. Do not continue interpolation scalar-tail staging or color staging by default. Phase 050 was 0.97x after fix, Phase 080 was unstable at 0.84x / 1.12x / 1.17x, and Phase 100 representative alias run was 0.83x.
4. Do not create `doc-rvv/features/shot-RVV.zh.md`. Component positive and production-detail weak-positive do not override production-public negative evidence.

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| public SHOT fixed-LRF diagnostic scaffold | current source shape | SHOT352 / SHOT1344 public entry with provided frames | 建立可复现 correctness、bench 和 board 入口。 | 不隔离 LRF 自动估计；bench 仍包含 search。 | QEMU correctness、asm dump、board smoke、Evidence Doctor manual check。 | attempted: scaffold closed, no production speedup | Phase 000 |
| shape-bin dot helper RVV | `createBinDistanceShape` loop | normal dot + bin distance for SHOT352 / SHOT1344 | 批量 normal dot 和 finite mask，可能降低每邻域点前置成本。 | SoA 输入不是 production AoS，后续 histogram scatter 可能主导。 | same-chain correctness、component bench、asm attribution、board A/B。 | partial-production-candidate: SoA component 2.38x-2.47x | Phase 020 |
| shape-bin AoS layout RVV | Phase 020 mismatch audit | `pcl::Normal` 连续 AoS normal batch | 检查 stride load 是否保留 shape-bin 算法收益，缩小 SoA 与 production 数据布局差距。 | 仍不覆盖 arbitrary indices gather、protected estimator state 和 `PCL_WARN` 副作用。 | AoS same-chain correctness、component bench、asm、board A/B、Evidence Doctor。 | partial-production-candidate: contiguous AoS component 1.76x-1.90x | Phase 030 |
| shape-bin indexed gather RVV | Phase 030 mismatch audit | `pcl::Normal` + `pcl::Indices` normal batch | 直接评估 production 最关键的非连续 normal 访问和 NaN 计数边界。 | gather 和 offset staging 会吃掉部分连续 AoS 收益；NaN warning 副作用仍需 production direct 验证。 | indexed correctness、component bench、asm、board A/B、Evidence Doctor。 | attempted: component 1.65x-1.86x drove PI2, but production-public later 0.98x / 0.99x | Phase 040 / PI2 |
| shape-bin indexed production probe | PI1 + user authorization | `createBinDistanceShape` production helper, `PointNT` normal traits + AoS layout | 真实生产 helper 只接 shape-bin indexed gather，验证局部收益能否转成 public entry 收益。 | public entry 可能被 search、interpolation、normalization、output copy 和 dispatch overhead 稀释。 | production direct tests、production asm、production-detail board、public board、Evidence Doctor、registry。 | rollback/no-production: detail 1.07x, public 0.98x / 0.99x | none; resume only with new production profile or independent user authorization |
| descriptor normalization / copy RVV | `normalizeHistogram` and output copy loops | 352 / 1344 contiguous descriptor arrays | 顺序数组循环，fallback 简单。 | 除法和 sqrt 只执行一次，收益可能被插值和 search 稀释。 | component ablation、public bench、asm、board evidence。 | partial-production-candidate | PI1 if production authorized |
| interpolation geometry staging RVV | `interpolateSingleChannel` / `interpolateDoubleChannel` | indexed surface gather、三轴投影和 distance staging | 可能通过批量投影、距离和 valid mask 暂存减少标量开销。 | 多组 double staging arrays 增加 store/load；不覆盖 histogram scatter、`acos` / `atan2`。 | same-chain correctness、component bench、asm、board A/B、Evidence Doctor。 | attempted: after valid-mask fix still 0.97x | Phase 050 |
| interpolation bin-selection / scalar-tail diagnostic | Phase 050 result | descriptor volume selection before histogram write | 避免多组 double staging arrays，只尝试更窄的 bin-selection 或 scalar-tail 辅助。 | 分支多且仍可能被 scatter 主导。 | 新 plan、correctness、component bench、board A/B。 | attempted / unstable: 0.84x, 1.12x, 1.17x | Phase 080 |
| color LAB distance helper | `RGB2CIELAB` and color bin loop | SHOT1344 color path arithmetic after LAB normalization | 已归一化 L/a/b 数组的 abs/add/clamp/bin 算术可批量化。 | 不覆盖 RGB2CIELAB LUT、indexed `PointXYZRGBA` load、`std::vector::push_back` 或完整 color interpolation。 | same-chain correctness、component bench、asm、3 次 targeted board A/B、Evidence Doctor。 | partial-production-candidate / arithmetic-only: 1.10x-1.23x | Phase 060 / next RGB-LUT diagnostic |
| RGB/LUT indexed color diagnostic | Phase 060 mismatch audit | SHOT1344 color path before color distance | 检查 RGB2CIELAB LUT 和 indexed RGBA 访问是否吞掉 arithmetic-only 收益。 | protected helper、LUT 边界、PointXYZRGBA layout 和 production push-back 仍需拆清。 | plan、same-chain correctness、component bench、asm、board A/B、Evidence Doctor。 | attempted / neutral-weak: scalar LAB staging shape 1.02x | Phase 070 |
| structure parity / doc suite | Phase 080 reflection + phase-loop quality gate | topic-local README、evaluation、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map、phase index | 让 reviewer 不依赖聊天上下文即可定位 target、case、helper、证据白名单和 production doc 适用性。 | 纯文档阶段不改变性能；必须避免虚构不存在 target。 | doc-suite role inventory、target granularity audit、artifact tracking scan、diff/check。 | adopted for structure | Phase 090 |
| evidence registry / target alias structure | Phase 090 target granularity audit | topic-local registry / freshness check、常用 correctness alias、board case alias | 降低手动复跑和 stale log 风险，让 Evidence Doctor 输入更可恢复。 | 需要确保不虚构 run labels，不把 alias 当新证据。 | Makefile target、registry / freshness script 或等价登记、doctor rerun、diff check。 | adopted for structure | Phase 100 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| Phase 000 | public fixed-LRF smoke 没有收益，转向组件消融。 | 公开入口 smoke 被 search、interpolation 和未归因 RVV 指令稀释，无法定位 helper 价值。 | normalization / shape-bin component same-chain、asm、board A/B、Evidence Doctor。 | high |
| Phase 010 | normalization component 有稳定正向，但 public 入口仍不稳定。 | 3 次 board run 中 normalization 352 为 1.58x-1.59x、1344 为 1.50x-1.51x；public fixed-LRF 约 0.98x-1.04x。 | production probe plan 或 shape-bin component diagnostic。 | high |
| Phase 020 | shape-bin SoA component 有稳定正向，但 production 布局尚未闭合。 | 3 次 board run 中 `shape_bin_component` 为 2.38x-2.47x；Evidence Doctor Error 仍只影响 public fixed-LRF 入口，不支持 production 结论。 | AoS layout component diagnostic 或显式授权后的 PI1 production probe。 | high |
| Phase 030 | shape-bin AoS contiguous component 仍保持正向。 | 3 次 targeted board run 中 `shape_bin_aos_component` 为 1.76x-1.90x；stride load 成本可见但未抵消收益。 | indexed gather component diagnostic 或显式授权后的 PI1 production probe。 | high |
| Phase 040 | shape-bin indexed gather component 仍保持正向，但慢于 contiguous AoS。 | 3 次 targeted board run 中 `shape_bin_indexed_component` 为 1.65x-1.86x；`vluxei32` gather、offset staging 和 NaN count 没有抵消组件收益。 | PI1 shape-bin indexed production probe plan；PI2 需用户授权修改 production。 | high |
| Phase 050 | interpolation geometry staging arrays 不适合作为生产探针输入。 | 首轮 0.73x 暴露重复 scalar sqrt；修正后仍只有 0.97x，Doctor Error 标记 B/A 低于 1。 | color LAB distance component diagnostic，或更窄的 interpolation bin-selection / scalar-tail diagnostic。 | high |
| Phase 060 | color LAB distance arithmetic 本身有组件收益，但仍不代表完整 color path。 | 3 次 targeted board run 为 1.10x、1.22x、1.23x；Evidence Doctor 只有 low-run warning。 | RGB2CIELAB LUT + indexed `PointXYZRGBA` production-shaped diagnostic，确认真实颜色前半段是否保留收益。 | high |
| Phase 070 | indexed RGB/LUT scalar staging 基本吞掉了 Phase 060 的算术收益。 | targeted board run 为 1.02x；Evidence Doctor 有 low-run warning 和 near-threshold suggestion。 | interpolation bin-selection / scalar-tail diagnostic；若以后恢复 color，优先做 direct byte/LUT gather 消融而不是 staging arrays production probe。 | high |
| Phase 080 | interpolation bin-selection scalar-tail staging 不稳定。 | targeted board runs 为 0.84x、1.12x、1.17x，checksum 一致但 decision bucket 摇摆；Doctor 只看到最后一轮 low-run summary。 | structure parity / doc suite phase；后续插值候选必须先有 production profile 或 histogram scatter 问题重定义。 | high |
| Phase 090 | topic-local doc suite 已补齐，但 target alias / registry 仍是结构缺口。 | 新增 testing overview、correctness tests、benchmark/evidence、optimization evidence 和 test-support code map；审计发现细分 correctness alias、board repeated alias 和 registry 仍未接入。 | evidence registry / target alias diagnostic。 | high |
| Phase 100 | registry / target alias 已接入，当前授权范围内的结构缺口闭合。 | 新增 correctness alias、board case alias、per-run output dirs、registry record / status；代表 board alias run 为 0.83x，Doctor Errors=1、Warnings=1。 | 若用户授权，进入 PI2 shape-bin indexed production probe；否则 stop in diagnostic state。 | high |
| PI2 | shape-bin production probe 不支持采纳。 | 真实 helper direct board 为 1.07x weak-positive，但 public SHOT352 / SHOT1344 为 0.98x / 0.99x，两个 public Doctor 均有 degradation Error。 | 用户已确认回滚；PI3 做 no-production closeout，不创建 production `doc-rvv`。 | high |
| PI3 | 回滚后没有值得默认继续的 SHOT descriptor RVV production 方向。 | shape-bin public negative，interpolation / color staging negative 或不稳定；normalization 虽为 component positive，但没有 production profile 支持其能突破 public 入口稀释。 | stop as rollback/no-production；未来若恢复，应先有 production profile 或独立 PI1 授权。 | high |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| SHOT OMP production integration | 队列表明确 OMP 文件只作对照；OpenMP/RVV 嵌套会混淆调度和收益归因。 | scalar SHOT diagnostic 有稳定生产证据，且用户另行授权 OMP topic。 |
| direct production patch in Phase 000 | 当前只有函数级评估和 scaffold，缺少同边界 correctness、asm、board 和 Evidence Doctor。 | Phase 000 证据闭合并进入 S10，且 EvidenceDecision 支持 production integration loop。 |
| interpolation geometry staging arrays production probe | Phase 050 修正后仍为 0.97x，且该形态需要额外写出多组 double staging arrays，不覆盖完整 histogram scatter。 | 只有后续找到更窄的 interpolation subcandidate、或 production direct profile 显示 geometry projection 是独立主瓶颈时再恢复。 |
| interpolation bin-selection scalar-tail staging production probe | Phase 080 的 3 次 targeted board run 跨 negative / positive bucket 摇摆，且 branch selection 仍是标量 staging，不覆盖完整 interpolation。 | 只有 production profile 显示 bin-selection 是独立主瓶颈，或重新定义为 histogram scatter / direct production boundary 的新问题时再恢复。 |
| color LAB staging arrays production probe | Phase 070 indexed RGB/LUT scalar staging + RVV LAB distance 只有 1.02x，且需要额外 LAB arrays，不覆盖 vector push 和完整 color interpolation。 | 只有 direct RGB/LUT gather 消融明显正向，或 production profile 显示颜色 LUT / distance 是独立主瓶颈时再恢复。 |
| shape-bin indexed production patch adoption | PI2 production-public SHOT352 / SHOT1344 接入后为 0.98x / 0.99x，且 Evidence Doctor 对两个 public case 均报退化 Error；局部 1.07x 不足以抵消端到端成本。 | 只有用户明确接受 public-entry 风险并要求保留，或授权新的 repeated production-public run 预算且结果转为 positive / weak-positive 时再恢复采纳讨论。 |
| descriptor normalization production probe | normalization component 稳定正向，但 public entry 历史约 1x，且 shape-bin PI2 已证明局部 helper 正向可能被端到端成本吞掉；没有 profile 说明 normalization 是独立主瓶颈。 | 只有 production profile 显示 normalization / output copy 占比足够，或用户明确授权新的独立 PI1 production probe 时再恢复。 |
