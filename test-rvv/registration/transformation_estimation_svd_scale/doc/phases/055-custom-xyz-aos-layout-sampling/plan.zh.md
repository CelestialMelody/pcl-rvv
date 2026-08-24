# Phase 055 计划：custom xyz AoS layout sampling

## 阶段意图和边界

本阶段验证 `TransformationEstimationSVDScale` 当前 traits-gated xyz AoS（按 PCL traits 字段门控的 xyz 结构数组）production public path，是否能覆盖“不是 PCL 内建点型、但已注册 x/y/z 单 float 字段且字段 offset / stride 不同”的自定义点型样本。

本阶段不修改 production 源码，不扩大 production gate，也不验证 `Scalar=double`。若测试发现自定义 layout 不能编译或不能命中当前 traits gate，本阶段只记录为 layout 支撑缺口，不直接改生产入口。

`validated_scope`：

- source / target：两个测试本地注册点型，分别使用非零 `x` offset、非连续 `x/y/z` offset 和不同 `sizeof(PointT)`。
- row source：ordered-cloud-pair、source-indexed、dual-indexed、correspondence。
- `Scalar=float`、dense 输入、合法 deterministic indices / correspondences、4K correctness 和 4K/64K/256K bench label。

`unvalidated_scope`：

- 任意用户自定义点型全集、非 standard-layout POD、double xyz 字段、未注册字段、非 dense 输入、非法 index / correspondence、异常对齐、`Scalar=double`、新的 locality mitigation family。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| production gate | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` 已用 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 门控 ordered / row-source / correspondence RVV 路径。 |
| traits helper | `common/include/pcl/rvv_point_traits.h` 的 `RVVXYZAoSFloatLayout` 使用 PCL field traits offset，要求注册单 float x/y/z、POD standard-layout、`sizeof(PointT)==sizeof(POD)`、stride 和 field offset 按 float 对齐。 |
| 已有证据 | Phase 051/052/053/054 覆盖常见 PCL xyz AoS 点型和 row-source 全交叉；仍不覆盖自定义 layout / padding。 |
| 当前测试 | `src/test_tesvd_scale.cpp` 已有 ordered generic、more-generic、row-source representative 和 row-source all-more-generic correctness。 |
| 当前 bench | `src/bench_tesvd_scale.cpp` 已有 ordered / row-source common PCL 点型 case-filter；Makefile 已有 QEMU / board manifest 和 registry 生成链。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `custom-xyz-aos-layout-sampling` | 当前 production RVV load helper 使用 traits offset + `sizeof(PointT)` stride，因此不依赖 x/y/z 必须位于 0/4/8，也不依赖 PCL 内建点型。 | PCL 宏注册可能让 POD / offset 与测试结构不一致；parent scalar path 或 test helper 可能假设直接成员布局；board 上更大 stride 可能降低收益。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `custom-xyz-aos-layout-sampling` | ordered-cloud-pair | local custom source -> local custom target / `float` / nonzero xyz offsets | 新增 gtest：custom ordered public vs same-chain reference | 新增 case-filter：`custom-xyz-aos-layout-sampling` | planned 4K/64K/256K repeated | QEMU manifest 引用 public RVV bench asm | planned | planned |
| `custom-xyz-aos-layout-sampling` | source-indexed / dual-indexed / correspondence | 同上 | 新增 gtest：三类 row-source public vs selected-cloud reference | 同一 case-filter，按 row source / size 输出 label | planned 4K/64K/256K repeated | 同上 | planned | planned |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 RED correctness | `src/test_tesvd_scale.cpp` | 新增本地注册点型和 custom layout 测试后，先运行窄测试；若当前支撑缺失，应看到编译 / 测试失败。 |
| A2 GREEN 支撑 | 必要时只补 topic-local helper / bench；除非 RED 指向 production gate 缺陷，否则不改 production 源码。 | 窄测试通过，且 `RVVXYZAoSFloatLayout` 的 offset 断言证明样本不是 0/4/8 常见布局。 |
| A3 bench label | `src/bench_tesvd_scale.cpp`、Makefile / script | QEMU smoke 能生成 custom layout labels 和 `max_reference_error`。 |
| A4 QEMU / registry | Make target 和 evidence registry | `run_test_compare`、custom QEMU smoke、Evidence Doctor 和 registry fresh。 |
| A5 board repeated | board target / summary / Doctor | 有界 5-run repeated，按 ordered / row source / size 报告 decision bucket。 |
| A6 文档同步 | phase result、matrix、roadmap、README / evaluation / evidence docs | 写清自定义 layout 采样能证明什么、不能证明什么。 |

## Evidence Doctor 和 registry 规则

QEMU smoke 和 board repeated 必须生成 `evidence_manifest.json`，再运行 Evidence Doctor（证据体检）。Error 必须修复或降级；Warning 必须解释是否来自 stride / point type / size / row source；registry 必须在 result 前为 fresh。

## 板卡复跑预算和决策桶

- runs：5
- warm-up：沿用 topic 默认 `TESVD_SCALE_BOARD_BENCH_WARMUP_ITERATIONS=5`
- iterations：沿用 topic 默认 `TESVD_SCALE_BOARD_BENCH_ITERATIONS=20`
- decision bucket：`positive`、`weak_positive`、`neutral`、`negative`、`unstable` 继续沿用现有 summary / Evidence Doctor 口径。
- 若所有 custom layout case 为 positive，只关闭这两个本地注册点型样本；若出现 weak / negative / unstable，只降级对应 point type / row source / size，不回推否定 Phase 054 常见点型结论。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public custom layout sampling；测试本地注册点型通过真实 public overload。 |
| A/B boundary | Std 构建 public scalar path vs RVV 构建 public RVV path。 |
| 当前决策问题 | 当前 traits-gated production path 是否能覆盖非内建、不同 offset / stride 的 registered xyz AoS 样本。 |
| diagnostic 是否可外推到 production | 只能外推到本阶段两个注册样本代表的 layout 形态；不能外推到任意自定义点型全集。 |
| comparison-boundary / baseline mismatch 风险 | 有。自定义点型 stride / offset 与常见 PCL 点型不同，收益必须按 layout / row source / size 分开报告。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是真实 public overload；弱 / 负 / 中性 / 不稳定只关闭或降级该 layout slice，不触发新 production patch。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段不选择新 RVV family，只验证现有 adopted family 的 layout 证据边界。 |

## 继续 / 停止条件

本阶段完成条件是 custom layout correctness、QEMU smoke、board repeated、Evidence Doctor、registry 和文档同步全部闭合。若 custom point type 注册导致当前工具链无法表达需要的 layout，记录为 `blocked`，并把恢复条件写成“需要更窄的 PCL traits / POD 注册设计”。`Scalar=double` 仍保持独立候选，不在本阶段推进。
