# Phase 040 Plan: production-integration-plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划）。目标是把 Phase 020 的
ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应）诊断正向结果转成生产补丁前合同：
冻结候选范围、fallback（回退路径）矩阵、production direct tests（真实生产路径测试）、
production asm attribution（生产反汇编归因）和 board production evidence（板卡生产证据）计划。

本阶段不修改 production 源码，不创建 `doc-rvv` 长期主题文档，不把诊断 speedup（加速比）写成
production-ready（可接入生产）。PI2 production patch（生产补丁）需要用户明确授权。

| 维度 | 本阶段范围 |
| --- | --- |
| production 边界 | 只写 PI1 计划和 topic-local 文档；`registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 保持不变。 |
| 候选入口 | 只规划 `estimateRigidTransformation(cloud_src, cloud_tgt, matrix)` 的 ordered-cloud-pair public overload。 |
| 点型 / Scalar | 初始生产候选收窄到 exact（精确点型匹配）`pcl::PointXYZ -> pcl::PointXYZ`、`Scalar=float`。 |
| 输入布局 | `cloud_src.size() == cloud_tgt.size()`、非空、规模不小于 16、两侧 `is_dense` 为 true，且两侧每个点通过 `pcl::isFinite`。 |
| 不覆盖范围 | source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair、`Scalar=double`、非 `PointXYZ` 泛型点型、小规模、非 dense 或非有限输入。 |
| 证据角色 | Phase 020 evidence 仍是 pre-production diagnostic（接入生产前诊断）；PI4 后才允许写 production performance（生产性能）结论。 |

## 当前状态清单

| 项 | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| production 源码 | unchanged | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` 无本 topic diff。 |
| correctness | pass | `run_test_compare`：Std / RVV 各 10 个 gtest 通过。 |
| QEMU smoke | pass / log-shape only | `log/qemu/evidence_manifest.json`、`log/qemu/evidence_doctor.md`，Errors=0、Warnings=0、Suggestions=0。 |
| asm attribution | pass for diagnostic | `log/qemu/asm_attribution.md`，candidate lambda 内联边界含 `vlsseg3e32.v`、`vfmacc`、`vfredosum`。 |
| board repeated | weak_positive diagnostic | `log/board/ordered_cloud_pair_repeated/summary.md`，5-run，4K positive，64K / 256K weak_positive。 |
| Evidence Doctor | pass for current manifests | QEMU 和 board 均为 0/0/0。 |
| evidence registry | fresh before PI1 docs | `make -C test-rvv/registration/transformation_estimation_2D evidence_status` 是恢复检查。 |
| generic point type strategy | loaded for PI1 | `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md` 已读取；本 PI1 选择 exact `PointXYZ` 初始 gate，不扩大到泛型。 |

## PI1 候选范围

### 采用的窄范围

PI2 若被授权，生产补丁只允许尝试下面这个入口：

```text
TransformationEstimation2D<pcl::PointXYZ, pcl::PointXYZ, float>::
estimateRigidTransformation(const pcl::PointCloud<pcl::PointXYZ>& cloud_src,
                            const pcl::PointCloud<pcl::PointXYZ>& cloud_tgt,
                            Matrix4& transformation_matrix)
```

候选算法沿用 Phase 020 的 two-pass centered fused 2D correlation accumulator（两遍中心化融合 2D 相关项累加器）：

1. 第一遍 RVV 求 source / target 的 x/y 质心。
2. 第二遍 RVV 累加中心化后的 `H00/H01/H10/H11`。
3. `atan2`、`cos/sin` 和 4x4 matrix 写回保持标量。

生产补丁必须复用公共 load helper（加载封装）：

```text
pcl::rvv_load::strided_load3_f32m2
```

PI2 不新增公开 API。优先在 `impl/transformation_estimation_2D.hpp` 的邻近 internal / `detail` 区域放置 RVV helper，
helper 返回 `bool` 并在命中时写回 `Matrix4`。public ordered overload 的形状保持为：

```text
已有 size mismatch 语义检查
  -> __RVV10__ 下尝试窄范围 RVV helper，成功则 return
  -> 构造 ConstCloudIterator 并调用现有 protected iterator helper
```

现有 protected iterator helper 继续承担 Std fallback（标准回退）边界。ordered-cloud-pair public overload
本身只保留参数检查、RVV 短路和 fallback 调用。

### 有意不采用的范围

| 范围 | PI1 决策 | 理由 / 恢复条件 |
| --- | --- | --- |
| traits-gated generic point types（按字段特征门控的泛型点类型） | deferred | Phase 020 只测 `PointXYZ`。扩大到 `PointXYZI`、`PointXYZRGB` 或自定义点型需要 traits / offset / layout tests 和板卡代表性点型证据。 |
| `Scalar=double` | fallback | 当前 RVV helper 和误差预算只覆盖 `float`。double 需要独立 reduction、asm 和 board 证据。 |
| source-indexed-cloud-pair | fallback | 需要 gather、valid-index semantics（有效索引语义）和 policy-specific bench。 |
| dual-indexed-cloud-pair | fallback | 双侧 gather 成本和索引语义未闭合。 |
| correspondence-pair | fallback | query / match 展开和非法 correspondence 语义未闭合。 |
| 非 dense 或任一点非有限 | fallback | 当前 production centroid 和 demean 组合语义不能被 dense RVV path 改写。 |
| 小规模输入 | fallback | Phase 020 候选以 `n >= 16` 命中 RVV；小规模收益不足且回退简单。 |

## Fallback Matrix

| gate | 触发条件 | 期望行为 | PI3 证据 |
| --- | --- | --- | --- |
| non-RVV build | 未定义 `__RVV10__` | 编译和行为完全走现有标量路径。 | Std build `run_test_compare`。 |
| row source 非 ordered | 调用 source-indexed、dual-indexed 或 correspondences overload | 不尝试 RVV，继续走现有 `ConstCloudIterator` 路径。 | 新增或保留 public semantics / valid row-source fallback tests。 |
| `Scalar` 非 float | `Scalar=double` | 不尝试 RVV。 | `TransformationEstimation2D<PointXYZ, PointXYZ, double>` correctness / fallback test。 |
| exact 点型不匹配 | `PointSource` 或 `PointTarget` 不是 `pcl::PointXYZ` | 不尝试 RVV。 | `PointXYZI` 或等价 xyz 点型 fallback compile / correctness test。 |
| 数量不匹配 | `cloud_src.size() != cloud_tgt.size()` | 保持现有 `PCL_ERROR` 后返回，输出矩阵不变。 | 已有 size mismatch test；PI3 继续保留。 |
| 空输入 | `cloud_src.empty()` 且数量相等 | 不尝试 RVV；走现有路径。 | PI3 补 public empty-input behavior test 或说明现有行为。 |
| 小规模 | `n < 16` | 不尝试 RVV；走现有路径。 | PI3 小规模 fallback test。 |
| 非 dense | `cloud_src.is_dense == false` 或 `cloud_tgt.is_dense == false` | 不尝试 RVV；走现有路径。 | PI3 非 dense x/y/z cases。 |
| 非有限点 | 任一 source / target 点 `pcl::isFinite` 为 false | 不尝试 RVV；走现有路径。 | 已有 x/y、z 非有限语义；PI3 转为 production fallback matrix。 |
| layout / helper 前提失败 | exact gate 以外或 helper 无法安全表达 | 不尝试 RVV。 | PI3 compile gate + asm / board 证明 covered case 命中，未覆盖 case fallback。 |

## 实现和测试动作

| id | 动作 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| PI2-A1 | 在 production impl header 中新增窄范围 RVV helper。 | `registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | 无 public API 变更；helper 只在 `__RVV10__` 下编译；未覆盖范围返回 false。 |
| PI2-A2 | ordered-cloud-pair public overload 尝试 RVV 短路。 | 同上 | size mismatch 语义保持；RVV 成功后 return；失败自然落回现有 iterator helper。 |
| PI3-A1 | 新增 production direct correctness tests。 | `src/test_te2d.cpp` 或后续拆分文件 | PointXYZ/float ordered case 与 expected transform、现有 public path、near-cancellation 预算一致。 |
| PI3-A2 | 新增 fallback tests。 | 同上 | non-RVV、double、non-PointXYZ、小规模、非 dense / 非有限、indices / correspondences 均有单独证明或明确不适用理由。 |
| PI4-A1 | 新增 production public bench case-filter 和 repeated board target。 | `src/bench_te2d.cpp`、`Makefile`、`board.mk`、topic-local script | `ordered-cloud-pair-public` 生成 post-production summary / manifest / doctor。 |
| PI4-A2 | 扩展 asm attribution 到 production boundary。 | `script/generate_te2d_asm_summary.py` | 能区分 production helper / public overload / bench wrapper / Eigen / stdlib RVV 指令。 |
| PI4-A3 | 接入 Evidence Doctor 和 registry。 | `log/board/ordered_cloud_pair_public_repeated/**`、`log/qemu/**`、`log/evidence_registry.json` | Doctor Errors=0；Warnings 若存在必须解释或降级。 |
| PI5-A1 | 再次 EvidenceDecision。 | phase result、evaluation、适用的 `doc-rvv` | 只有 production direct correctness、fallback、asm、board 和 doctor 均闭合后，才写 production-ready。 |

## Evidence Doctor 和 Registry 规则

PI4 需要新增 production evidence manifest（生产证据清单），不复用 Phase 020 的 pre-production diagnostic manifest。
推荐输出：

```text
test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/qemu/production_public/evidence_doctor.md
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/evidence_doctor.md
```

Registry label 使用：

```text
qemu-te2d-production-public-smoke-pi4
board-te2d-production-public-repeated-pi4
```

`evidence_role` 分别为 `qemu_smoke_only` 和 `post_production_performance`。如果 manifest 只能解析 Markdown
summary，Evidence Doctor 必须标成 metadata incomplete（元数据不完整）并降级，不能写完整 production evidence。

## Board 复跑预算和决策桶

PI4 初始 run budget（复跑预算）沿用 Phase 020：

| 参数 | 计划值 |
| --- | --- |
| run count | 5 |
| iterations | 20 |
| warm-up iterations | 5 |
| case-filter | `ordered-cloud-pair-public` |
| device | 重新读取 board config，目标硬件仍按 env var / local config 标记。 |

决策桶：

- `positive`：median B/A 稳定大于 1.10，且没有 `B/A < 1`。
- `weak_positive`：median B/A 大于 1.03，min 不低于 1.0，且 Evidence Doctor 没有未处理 Warning。
- `neutral`：median 接近 1，或 min / max 跨越方向但无 correctness / asm 问题。
- `negative`：median 小于 1，或 `B/A < 1` 频率说明 production path 不值得保留。
- `unstable`：一次预算后跨桶摇摆；最多允许再做一次同边界 5-run 确认，除非用户扩大预算。

PI5 只有在 `positive` 或 `weak_positive` 且第 1、2 主门槛闭合时才可考虑 production-ready：RVV public path
必须比 Std public path 更好，且静态实现质量、asm 归属和 fallback 矩阵可维护。

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | bench / board target | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production dispatch | ordered-cloud-pair | exact `PointXYZ -> PointXYZ`, `Scalar=float`, dense finite AoS | planned PI3 production direct + fallback tests | planned PI4 board public repeated | planned production helper / public boundary attribution | planned QEMU + board doctor | PI1 planned; PI2 needs authorization |
| production dispatch | generic xyz point types | traits-gated xyz AoS, `Scalar=float` | not_yet_covered | not_yet_covered | not_run | not_run | deferred |
| production dispatch | indexed / correspondences | gather / query-match row source | not_yet_covered | not_yet_covered | not_run | not_run | not_applicable in PI1 |

## 阶段完成条件

PI1 完成条件：

- 候选范围、fallback matrix、PI2 implementation shape、PI3 tests、PI4 evidence commands 和 PI5 决策条件写入 phase plan / result。
- 文档明确 exact `PointXYZ` 是有意收窄，不是泛型点类型已经成立。
- `doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` 继续为 `not_applicable`。
- Handoff 将 `next_phase_default` 写成 `PI2-production-patch-if-user-authorizes`，并列出替代路径。

PI2 暂停条件：

- 需要修改 `transformation_estimation_2D.h` 的 public / protected API。
- helper 为了闭合 production path 需要扩大到泛型点类型、indices、correspondences 或公共 RVV API。
- fallback gate 无法单独测试。
- production asm attribution 只能看到整份二进制 RVV 指令，不能归到 production helper / public boundary。
- board production repeated 证据不可用或与 Phase 020 诊断方向矛盾。
- Evidence Doctor Error 未修复。

## 文档更新清单

本阶段更新：

- `doc/phases/040-production-integration-plan/plan.zh.md`
- `doc/phases/040-production-integration-plan/result.zh.md`
- `README.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/phases/README.zh.md`
- `doc/transformation_estimation_2D-evaluation.zh.md`
- `doc/optimization-evidence.zh.md`
- 当前 Handoff Packet

PI5 通过前不创建 production 长期主题文档。

## Roadmap 同步动作

本阶段把 `production integration` 从 `planned` 更新为 `PI1-plan-complete / PI2-needs-user-authorization`。
`generic point type production` 保持 deferred；`030-row-source-family-carryover` 仍是用户不授权 production
patch 时的默认技术续作。

## Continue / Stop 条件

`continue_stop_decision`：PI1 完成后停止在 production patch 授权边界。

`stop_condition_hit`：PI2 会修改 production 源码，需要用户明确授权。

`next_phase_default`：`PI2-production-patch-if-user-authorizes`。如果用户不授权 production patch，
默认恢复到 `030-row-source-family-carryover`。
