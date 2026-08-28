# Phase 060 Plan: PI1 production-integration-plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划）。目标是把 Phase 050 的 positive production-shaped diagnostic（生产形态诊断正向结果）落成一个可审查、可暂停的 production patch（生产补丁）范围，但本阶段不修改 `recognition/src/linemod.cpp`。

候选只覆盖默认宏配置下的 `EnergyMaps -> LinearizedMaps` copy（拷贝）循环。当前 PI1 不覆盖 `LINEMOD_USE_SEPARATE_ENERGY_MAPS`、`detectTemplatesSemiScaleInvariant` 的 scale offset 形态、NMS（非极大值抑制）、averaged detection（邻域加权检测）或 score accumulation / threshold scan 的 RVV 化。

## Production 源码形态审计

| item | observation | PI1 decision |
| --- | --- | --- |
| `LinearizedMaps::initialize` | 每个 offset map 使用 `aligned_malloc(2 * mapsSize)`，`mapsSize = (width / step_size) * (height / step_size)` | RVV helper 只写前 `mapsSize` 字节，与现有标量 copy 循环一致；额外分配空间不作为本阶段优化对象 |
| `LinearizedMaps::operator()(map_col, map_row)` | 返回 `maps_[map_row * step_size_ + map_col]` | helper 必须保持 `map_row` major、`map_col` minor 的 64-map 顺序 |
| `LinearizedMaps::getOffsetMap(col,row)` | 后续 score accumulation 按 feature offset 读取已线性化 map | 本阶段只替换构造阶段 copy，不改变后续 offset 读取语义 |
| `matchTemplates` | 默认宏下有单组 `EnergyMaps -> LinearizedMaps` copy loop | PI2 首选覆盖；该入口没有 threshold detection sink，但可用 max detection correctness 验证输出 |
| `detectTemplates` | 默认宏下同样有单组 copy loop，后续含 threshold、NMS、averaging | PI2 覆盖默认非 separate-energy 路径；NMS/averaging 逻辑保持标量 |
| `detectTemplatesSemiScaleInvariant` | copy loop 与 `detectTemplates` 相似，但后续 `getOffsetMap` 使用 scaled feature offset | 当前 PI1 不覆盖，避免把 scale offset correctness 混入第一轮 production patch |
| `LINEMOD_USE_SEPARATE_ENERGY_MAPS` | 编译分支会维护 4 套 energy / linearized maps | 当前 PI1 不覆盖；该宏默认未启用，后续若启用需单独做 four-map helper 和 production direct evidence |

## 候选实现形态

| component | planned shape | reason |
| --- | --- | --- |
| production helper | 在 `recognition/src/linemod.cpp` 邻近匿名命名空间或文件局部区域新增 `linearizeEnergyMapStd` 与 `linearizeEnergyMapRVV` | 不改变 public API（公开接口）；把重复 copy loop 从入口主体抽出，便于 `__RVV10__` gate 和反汇编归属 |
| RVV body | `__RVV10__` 下用 `vlse8.v` 跨步读取 `energy_map[row * width + map_col]`，用 `vse8.v` 连续写 `linearized_map[row * lin_width + col]` | 与 Phase 040/050 证据中正向的 code shape 一致 |
| fallback | 非 RVV 构建、`step_size != 8`、`lin_width == 0`、`lin_height == 0`、输入/输出指针为空时走 Std helper | 保持现有边界，并避免无意义小规模 RVV 分流 |
| dispatch | 入口中 `maps.initialize(...)` 后，原 nested loop 改成一行 helper 调用；`__RVV10__` 下 helper 内部分流，否则直接标量 | 不扩大入口主体复杂度，保持 `matchTemplates` 和 `detectTemplates` 可读 |
| duplicate sites | PI2 仅改默认宏下 `matchTemplates` 和 `detectTemplates` 的 copy loop | 两个入口共享已验证布局；semi-scale 和 separate-energy 另列后续扩展 |

## Validated / Unvalidated Scope

| scope type | content |
| --- | --- |
| validated_scope | 默认宏下单个 `EnergyMaps` bin 到单个 `LinearizedMaps` 对象的 64 个 offset maps；`width` / `height` 可被 `step_size=8` 整除；byte map contiguous storage |
| unvalidated_scope | separate-energy 四套 map、semi-scale scaled feature offset、真实 modality 分布、NMS/averaging output ordering、其它 step size、异常宽高不能整除时的长期语义 |
| point_type_expansion_queue | not applicable；LINEMOD 该段输入是 `unsigned char` image map，不涉及 PCL point type（点类型）或 `Scalar` |
| phase_closeout_boundary | PI1 只能批准或拒绝进入 PI2；不能写成 production adopted |

## 测试和证据计划

| PI step | artifact / command | required evidence |
| --- | --- | --- |
| PI2 RED | 在 topic-local production direct test 中先引用 planned production-visible behavior；或新增对真实 `LINEMOD::matchTemplates` / `detectTemplates` 的 regression case（回归测试）并在 production helper 缺失时观察失败 | TDD RED 必须先于 production patch；如果测试不能在缺 helper 时失败，需重写测试 |
| PI2 GREEN | 修改 `recognition/src/linemod.cpp`，抽出 Std/RVV helper 并替换默认宏下 `matchTemplates` / `detectTemplates` copy loop | 公开 API 不变；非 RVV 构建自然走 Std |
| PI3 production direct tests | 运行现有 topic correctness，并补真实入口命中 / fallback case | `matchTemplates` 和 `detectTemplates` 输出与标量一致；小规模或非 RVV 构建 fallback 可证明 |
| PI4 asm | dump production-linked 或 dedicated production-direct bench 二进制 | RVV 指令能归属到 production helper、其 inline clone 或真实 public entry hot boundary |
| PI4 board repeated | 新建或扩展 production-direct board collect target，至少 5-run、warm-up 5、iterations 200 | `production-public` 或 `production-detail` evidence role；QEMU timing 不参与性能结论 |
| PI4 doctor / registry | 为 production direct summary 生成 manifest、doctor 和 registry 记录 | Evidence Doctor 无未处理 Error；registry freshness 为 fresh |
| PI5 decision | 汇总 production diff、测试命令、asm、board、doctor 和 registry | 无论 positive 或 negative，都停在用户确认点；未确认前不做 adopted closeout 或 rollback |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 050 是 `production_shaped_diagnostic`；PI4 必须升级到 `production-public` 或 `production-detail` |
| A/B boundary | Phase 050 是 `test_helper`；PI4 目标是 `public overload` 或 `production detail helper` |
| 当前决策问题 | PI1 回答 implementation-shape（实现形态）是否可控；PI4/PI5 回答 production RVV-vs-scalar 是否成立 |
| diagnostic 是否可外推到 production | `unknown until PI4`；Phase 050 支持 PI1/PI2 探针，不支持 clean adoption |
| comparison-boundary / baseline mismatch 风险 | `yes`；production direct 需要覆盖真实 `LinearizedMaps` 对象、入口分流、输出容器和后续读取 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | not applicable to current positive；若 PI4 转弱或负，保留 patch 并在 PI5 请求用户确认 rollback，不自动撤回 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个 RVV copy family，不需要 family selection；若新增 fused / multi-bin family，再补 RVV-vs-RVV A/B |

## PI1 Gate

PI1 gate 判定为：候选范围可控，但 PI2 production patch 需要用户明确授权。默认可进入 PI2 的条件是用户确认“进入 / 推进 production integration loop”或等价授权；否则停在本计划。

## Continue / Stop Conditions

- `stop_condition_hit: production_patch_requires_explicit_authorization`。
- 若用户授权 PI2-PI5，下一步从本计划的 PI2 RED 开始，先写真实入口 / fallback 测试，再改 production。
- 若用户只要求继续诊断而不改 production，下一阶段应转为 semi-scale / separate-energy / production-direct bench 设计，不触碰生产源码。
