# Phase 050: PI1 production integration plan

## 阶段意图和边界

本阶段把 Phase 000-030 的 production-shaped diagnostic（生产形态诊断，测试专用但尽量贴近公开入口的数据流）结果整理成 PI1 production integration plan（生产接入计划）。PI1 只冻结候选范围、fallback（回退路径）、dispatch（分流逻辑）、点型 / layout gate（布局验收条件）和 PI2-PI5 证据计划；本阶段不修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp`，不创建 production 长期 `doc-rvv` 文档。

当前可规划的候选范围：

- `countWithinDistance`：RVV indexed AoS gather（按 `indices_` 离散加载数组结构点字段）+ normal gather + distance mask + `vcpop` count。
- `selectWithinDistance`：优先采用 Phase 010 的 `vcompress`（RVV 保序压缩）写回 index 和 distance。
- `getDistancesToModel`：采用 Phase 020 的 RVV distance + `vfwcvt.f.f.v` + `vse64.v` dense double store（稠密双精度写回）。
- 代表 source 点型：`PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`。
- normal 点型：当前只把 `pcl::Normal` 作为 PI1 代表 normal cloud；其它 `PCL_NORMAL_POINT_TYPES` 需要后续点型扩展 phase。

本阶段不证明：

- 真实 production direct（真实生产路径证据）已经存在。
- 泛型自定义 registered point type（已注册自定义点型）都可走 RVV。
- `Scalar=double` 或非 float xyz/normal 字段可走 RVV。
- 非 `indices_` 入口、非法 index、NaN/Inf 输入、其它 normal layout 或其它 sample_consensus 模型可共享结论。

## 当前状态清单

| 输入 | 当前事实 | PI1 使用方式 |
| --- | --- | --- |
| production 源码 | 三个公开入口仍是标量循环。 | 作为 PI2 patch 的待修改对象；本阶段只引用源码事实。 |
| Phase 000 | `PointXYZ/PointXYZI + Normal` count/select candidate positive diagnostic。 | count 形态进入候选；select scalar writeback 只作为 Phase 010 对照。 |
| Phase 010 | `vcompress` select 相对 scalar-writeback candidate 为正向。 | select 的默认 production family。 |
| Phase 020 | getDistances dense-store candidate 在 `PointXYZ/PointXYZI + Normal` 为正向。 | 可纳入同一 PI1 候选集，但 PI4 必须重跑 production boundary。 |
| Phase 030 | RGB/RGBA source layout 对三入口和 `vcompress` 仍为正向。 | PI1 代表点型范围加入 `PointXYZRGB` / `PointXYZRGBA`。 |
| Phase 040 | topic-local doc suite 已补齐。 | 可作为 reviewer 恢复入口；不替代 production direct 证据。 |

## 候选范围和 layout gate

PI2 若获授权，生产 RVV 尝试应采用模板 traits gate（点类型字段特征验收）而不是 exact `PointXYZ` gate：

| 对象 | PI1 gate | 理由 | fallback |
| --- | --- | --- | --- |
| source `PointT` | `pcl::rvv::RVVXYZAoSFloatLayout<PointT>::value` | 目标算法只读 source `x/y/z`，且需要 AoS byte offset gather。 | gate 失败时走现有标量入口。 |
| normal `PointNT` | 初始生产 patch 建议收窄为 `std::is_same_v<PointNT, pcl::Normal>`；同时保留 `pcl::rvv::RVVNormalFloatLayout<PointNT>` 作为后续扩展候选。 | 当前 correctness / board 只覆盖 `pcl::Normal`；`RVVNormalFloatLayout` 可证明字段语义，但未覆盖其它 normal 点型证据。 | 非 `pcl::Normal` 先走标量，下一 phase 再扩展。 |
| index type | `std::is_same_v<pcl::index_t, std::int32_t>` | `vcompress` 和 current helper 按 32-bit index store/load 对齐。 | 不满足时编译期不启用 RVV helper。 |
| byte offset | `input_->points.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()` 且 `normals_->points.size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT>()` | 当前 indexed helper 使用 32-bit byte offsets。 | 规模超过边界时走标量。 |
| work item count | `indices_->size()` 达到 PI2 冻结的最小收益阈值。 | 小规模 RVV setup 和 buffer resize 可能不能摊薄。 | 小规模走标量；阈值由 PI2 bench smoke 校准。 |

初始生产 patch 不应把 `PointXYZRGBNormal`、`PointXYZINormal`、自定义 xyz 点型或其它 normal 点型写成已覆盖。source 侧 RGB/RGBA 只证明颜色字段不参与本算法公式；它不证明输出整点语义，因为本算法输出的是 count、inliers、error 和 dense distances，不构造 `PointT`。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 当前输入证据是 `production-shaped diagnostic` 和 `component ablation`；PI2-PI5 后才可能产生 `production-public` 或 `production-detail` 证据。 |
| A/B boundary | Phase 000/020 是 test helper vs public scalar；Phase 010 是 test helper RVV-vs-RVV family A/B；PI4 必须改成真实 public overload 或 production detail helper。 |
| 当前决策问题 | 当前只回答 implementation-shape 和 bounded production probe 是否可控；不回答 clean adoption。 |
| diagnostic 是否可外推到 production | 只能外推为 PI2 bounded probe 的候选输入。它复用 `indices_`、source/normal gather 和输出合同，但没有真实 production dispatch、fallback tests、production asm 或 production board repeated。 |
| comparison-boundary / baseline mismatch 风险 | 存在。测试专用 `SampleConsensusModelNormalSphereAccess` helper 与 production public entry 分层不同；public benchmark 行当前仍是标量上下文，不是 RVV production baseline。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | Phase 010 的 `PointXYZI` B/A 为 weak-positive，但三入口 Std/RVV diagnostic 仍正向；允许窄范围 PI2 probe，条件是 point type、normal type、规模、fallback 和 production direct tests 都不扩大。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要。尤其 `selectWithinDistance` 必须确认 production detail 中 `vcompress` family 仍优于 scalar-writeback family，或明确只按当前 adopted family 接入。 |

## PI2-PI5 计划

| PI 阶段 | 动作 | 完成判据 | 暂停条件 |
| --- | --- | --- | --- |
| PI2 production_patch | 在 production 头内抽出 `*_Std` / `*_RVV` 或等价 internal helper，使 public entry 保持“语义检查 -> RVV 短路 -> Std fallback”形状。 | `__RVV10__` 关闭时原标量语义不变；RVV helper 只覆盖 PI1 冻结范围。 | 需要改 public API、公共 traits API、其它 topic 或扩大 normal 泛型范围。 |
| PI3 production_direct_tests | 补真实公开入口命中 RVV 的 correctness、fallback 和非 RVV build 证据。 | 至少覆盖三入口、四种 source 点型、`pcl::Normal`、小规模 fallback、layout/type fallback、非 RVV build。 | fallback gate 无法隔离，或测试需要覆盖 PI1 外点型。 |
| PI4 production_evidence_rerun | 重跑 QEMU correctness、production asm attribution（反汇编归属）和 board production bench。 | 关键 RVV 指令归属到 production helper；board summary 和 Evidence Doctor 无 Error。 | 板卡不可达、asm 无法归属、Evidence Doctor Error 或 production 性能不成立。 |
| PI5 production_evidence_decision | 重新执行 EvidenceDecision（证据决策）。 | 输出 production diff、公开入口、命令、board / doctor 结果和采纳或回滚建议。 | 必须停在用户确认点；不能自动采纳或回滚。 |

## 生产直连测试计划

| 类别 | 最小测试 | 证明内容 |
| --- | --- | --- |
| public entry correctness | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 真实 public overload 对拍原标量 helper。 | production dispatch 命中后输出一致。 |
| fallback correctness | 非 RVV 构建、小规模、非覆盖 normal 点型、unsupported source layout 或 32-bit byte offset 超界。 | 非覆盖路径自然回到标量。 |
| point type correctness | `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA + Normal`。 | 代表 source layout 在 production 入口下仍成立。 |
| output contract | select 保序 inliers、`error_sqr_dists_` double 写回、getDistances dense size 与顺序。 | `vcompress` 和 dense store 不改变公开输出合同。 |
| production asm | 三个 production RVV helper 或符号范围。 | `vfsqrt.v`、normal angle helper、`vcpop.m`、`vcompress.vm`、`vfwcvt.f.f.v`、`vse64.v` 可归属。 |
| board production bench | 同一 production build 下 Std/RVV 或可控 fallback A/B。 | 目标硬件真实性能，不使用 QEMU timing。 |

## Evidence Doctor 和 registry 规则

本阶段不生成新的 board 数值，因此不新增 Evidence Doctor 报告。PI2-PI5 获授权后必须为 production direct summary 生成新的 manifest 和 Evidence Doctor 输入；不能复用 Phase 000-030 的 test-only manifest 作为 production manifest。当前 registry freshness 仍由 `make -C test-rvv/sample_consensus/sac_model_normal_sphere evidence_status` 检查 Phase 000-030 摘要。

## 板卡复跑预算和决策桶

PI4 建议从 bounded rerun budget（有界复跑预算）开始：

- 每个覆盖点型至少 5 run repeated board；若 bucket 稳定为 positive / weak-positive / neutral / negative，则停止复跑并记录桶。
- 若同一入口在 5 run 内跨 positive 与 negative 摇摆，最多追加 5 run；仍摇摆则标为 unstable 并交给用户 / reviewer。
- QEMU 只作为 correctness 和日志形状证据，不进入性能桶。

## 继续 / 停止条件

本阶段完成条件是 PI1 计划可让下一轮 worker 不依赖对话即可进入 PI2，且明确 PI2 需要用户授权。当前停止条件是：继续会修改 production 文件，超出“未明确授权不修改生产源码”的默认边界。

默认下一动作：等待用户明确授权进入 PI2 production patch；授权后按本计划冻结范围连续推进 PI2-PI5，并在 PI5 停在用户采纳 / 回滚确认点。
