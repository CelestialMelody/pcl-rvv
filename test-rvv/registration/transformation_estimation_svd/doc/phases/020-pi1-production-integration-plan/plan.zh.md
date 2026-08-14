# PI1 计划：ordered-cloud-pair 泛型 xyz AoS f32 production integration

## 阶段意图和边界

本阶段最初只写 production integration plan（生产接入计划），不直接修改 production（生产源码）。目标是判断 Phase 010 的 `fused_ordered_cloud_pair_accum` positive diagnostic（正向诊断，历史 alias：`fused_full_cloud_accum`）能否安全落到 `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` 的真实 public ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应）入口。

执行修订：用户在 2026-08-14 明确要求“板卡应该可以访问了，请继续推进优化工作”。按 `.agents/skills/rvv-workflow/references/topic-lifecycle.zh.md`，PI1 gate 可闭合且用户授权继续 production integration loop（生产接入闭环）时，同轮继续 PI2-PI5。因此本 plan 保留 PI1 冻结边界，实际 production patch、production direct tests、board repeated 和 EvidenceDecision 写入 `result.zh.md`。

PI1 修订说明：早期计划把 row source policy 写成 `full-cloud`，并把第一版 production probe（生产探针）收窄到 exact `PointXYZ -> PointXYZ`。按当前 `.agents` 规则，row source policy 应命名为 ordered-cloud-pair；`full-cloud` 只可作为历史 bench label 或 public overload（公开重载）口语说明。由于生产入口本身是模板点类型，并且公共 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` gate（门控条件）已经存在，PI1 不应把 exact `PointXYZ` 固化为默认生产边界。`PointXYZ` 只作为代表性性能样本之一。

PI1 推荐采用 layout-gated generic production path（布局门控泛型生产路径）：

- public entry（公开入口）：`estimateRigidTransformation(const pcl::PointCloud<PointSource>&, const pcl::PointCloud<PointTarget>&, Matrix4&)`。
- row source policy（行来源策略）：ordered-cloud-pair。
- 点型和 `Scalar`：`Scalar == float`；`PointSource` 和 `PointTarget` 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value` 与 `pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value`。
- 代表性点型证据：至少覆盖 `PointXYZ -> PointXYZ`、`PointXYZI -> PointXYZI` 和 `PointXYZRGB -> PointXYZRGB` 的 production-direct correctness（真实生产路径正确性）或 fallback/path-hit 验收；板卡 repeated performance（重复板卡性能）至少覆盖 `PointXYZ`，若扩大性能结论到 mixed-field（混合字段）点型则补对应代表性板卡 case。
- 对象状态：只在 `use_umeyama_ == true` 的默认路径尝试 RVV；`use_umeyama_ == false` 保持标量。
- 输入：`source.size() == target.size()`、`source.is_dense && target.is_dense`、规模达到 small-input gate。
- 不覆盖：source-indexed-cloud-pair、dual-indexed-cloud-pair、correspondence-pair、`Scalar=double`、非 dense 或异常输入。不满足 xyz AoS layout gate 的点型自然 fallback。

这个范围比 test-only helper 更接近 production 模板入口，但证据边界仍要分层：`PointXYZI/RGB` 和其它 gate-allowed（门控允许）点型可通过 production-direct correctness 证明语义与分流成立；未逐类型上板的点型只能继承代表性性能判断，不能写成逐类型性能已证明。

## 当前状态清单

| area | 当前状态 | 证据路径 |
| --- | --- | --- |
| diagnostic correctness | QEMU Std/RVV 各 8 个 gtest 通过；board RVV smoke 8 个 gtest 通过 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`、`log/board/test_smoke/run_test.log` |
| diagnostic asm | bench helper 可见 `vlsseg3e32.v`、`vfadd.vv`、`vfmacc.vv`、`vfredosum.vs` | `build/asm/riscv/bench_transformation_estimation_svd_rvv.asm` |
| board diagnostic | same-boundary fused Std/RVV median `3.081x` / `3.179x` / `3.157x` | `log/board/fused_full_cloud_repeated/summary.md` |
| board Evidence Doctor | Errors=0、Warnings=4、Suggestions=0；Warnings 已解释为证据边界问题 | `log/board/fused_full_cloud_repeated/evidence_doctor.md` |
| production source | 未修改；当前 ordered-cloud-pair public entry 总是构造 iterator 后进入 shared helper | `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` |
| doc-rvv | not_applicable | 无 production patch / PI5 证据闭环 |

## 生产接入候选设计

PI2 若获得授权，候选补丁应保持 public API（公开接口）不变，并让未覆盖路径自然落回现有标量实现。

建议实现形态：

1. 在 ordered-cloud-pair public overload 完成 size mismatch（数量不一致）检查后，加入 `#ifdef __RVV10__` 包裹的 RVV 尝试。
2. RVV 尝试放入 `pcl::registration` 内部 helper，例如 `estimateRigidTransformationSVDOrderedCloudPairRVV(...)`，返回 `bool` 表示是否命中；返回 `false` 时原入口继续构造 `ConstCloudIterator` 并调用现有 helper。
3. helper 内部 gate：
   - `std::is_same_v<Scalar, float>`；
   - `pcl::rvv::RVVXYZAoSFloatLayout<PointSource>::value`；
   - `pcl::rvv::RVVXYZAoSFloatLayout<PointTarget>::value`；
   - `use_umeyama_ == true`；
   - `cloud_src.is_dense && cloud_tgt.is_dense`；
   - `nr_points >= 16`，该阈值来自 test-only candidate 的 small-input fallback；
   - `nr_points` 可安全转为 RVV helper 使用的 loop count。
4. helper 复用 Phase 000 公式：两次 ordered-cloud-pair 遍历或一轮 RVV chunk 累加 source sum、target sum 和 target-source cross sum；3x3 SVD tail 保留 Eigen。
5. production 注释只解释 scope / fallback / diagnostic 证据边界，不逐行解释 intrinsic。

`Scalar=double` 暂不纳入 RVV；该路径应保持标量。source-indexed-cloud-pair、dual-indexed-cloud-pair 和 correspondence-pair 也不随 ordered-cloud-pair 自动接 production，后续按 row-source family carry-over audit（行来源族迁移审计）独立推进。

## 测试和证据计划

| gate | PI2/PI3 产物 | 完成判据 |
| --- | --- | --- |
| production direct correctness | 新增或扩展 `src/test_tesvd.cpp` production-direct gtest | `PointXYZ`、`PointXYZI`、`PointXYZRGB` 的 ordered-cloud-pair public entry 在 RVV / Std 构建下与 reference 在误差预算内一致。 |
| small-input fallback | production-direct small input test | `nr_points < 16` 时输出仍与标量路径一致；若无法直接观测 path hit，用反汇编和 bench 区分。 |
| `use_umeyama_ == false` fallback | 构造 `TransformationEstimationSVD(false)` test | 保持现有 centroid / demean / correlation 标量路径。 |
| `Scalar=double` fallback | `TransformationEstimationSVD<PointXYZ, PointXYZ, double>` test | double 构建不命中 RVV，结果与标量路径一致。 |
| layout-gate representative tests | `PointXYZI` 和 `PointXYZRGB` production-direct smoke，必要时补自定义 xyz AoS fixture | 证明 source / target 分别按当前点类型 offset 和 stride 读取，不复用 `PointXYZ` layout。 |
| indexed / correspondences fallback | source indices、dual indices、correspondences smoke | 这些公开入口不经过 ordered-cloud-pair RVV fast path。 |
| production asm attribution | `dump_bench_rvv` 或新增 production direct dump target | RVV 指令归属到 production helper / public ordered-cloud-pair inline boundary，不只存在于 test-support helper。 |
| production board bench | 新增 production-direct repeated target | 同一 public ordered-cloud-pair boundary 下 Std/RVV 5-run median 进入 `positive` 或至少 `weak_positive`；QEMU timing 不参与决策。 |
| Evidence Doctor / registry | production direct summary / manifest / doctor / registry | Errors=0；Warnings 有解释；`evidence_status` fresh。 |

## 板卡复跑预算和决策桶

PI4 复用 Phase 010 的有界预算：

| 项 | 设置 |
| --- | --- |
| 初始 run budget | 5-run repeated board。 |
| 追加预算 | 最多 1 次同边界确认复跑；仅在 decision bucket 摇摆或 Evidence Doctor 要求确认时使用。 |
| iterations / warm-up | 默认 20 iterations、5 warm-up iterations。 |
| positive | production direct median `> 1.15` 且无高频 `B/A < 1`。 |
| weak_positive | median `>= 1.03` 且 min `>= 0.97`，且实现足够小、fallback 简单。 |
| neutral / negative / unstable | 按 Phase 010 规则降级，不进入 production-ready。 |

如果 production direct 结果与 test-only diagnostic 方向相反，优先相信 production direct，并把 test-only positive 降级为机制诊断。

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `production_direct_dispatch` | ordered-cloud-pair | generic xyz AoS / `float` | public ordered-cloud-pair overload, `use_umeyama_ == true` | planned production direct + fallback + representative point type tests | planned production-direct bench | planned | planned production symbol attribution | planned | PI1 planned | 等用户授权后进入 PI2 |
| `fused_ordered_cloud_pair_accum` | ordered-cloud-pair | `PointXYZ` / `float` | test-only candidate；legacy alias 保留 `fused_full_cloud_accum` / `fused-full-cloud` | done | done | positive diagnostic | bench helper asm done | board doctor Warnings explained | source evidence | feed PI1 |
| `generic_xyz_aos_dispatch` | ordered-cloud-pair | `PointXYZI/RGB/custom xyz AoS` / `float` | public ordered-cloud-pair overload | representative tests planned | representative bench planned if performance claim expands | missing | planned | planned | fold into PI1/PI2 | 不另判为拒绝；作为 PI1 production gate 的组成部分 |
| `source_indexed_fused_accum` | source-indexed-cloud-pair | `PointXYZ` / `float` | public indexed overload | missing | missing | missing | missing | missing | deferred | ordered-cloud-pair PI1 后做 row-source audit |

## 暂停条件

PI1 后续不得直接进入 production patch，如果命中任一条件：

- 用户未授权 production source edit（生产源码编辑）。
- 泛型 xyz AoS traits / offset gate 无法在 production 中清晰表达，或代表性点型 correctness 失败。
- 需要覆盖 `Scalar=double`、source-indexed-cloud-pair、dual-indexed-cloud-pair 或 correspondence-pair 才能让 ordered-cloud-pair 设计成立。
- fallback gate 无法在 public overload 中保持清晰，或需要修改 public API / protected API。
- production direct correctness、asm attribution 或 board repeated 不能形成同边界证据。
- Evidence Doctor 出现 Error 或 production direct bucket 为 negative / unstable。

## 文档更新清单

如果 PI2-PI5 获得授权并闭合，需要同步：

- `doc/phases/020-pi1-production-integration-plan/result.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_svd-evaluation.zh.md`
- `README.zh.md`
- PI5 通过后才创建或更新 `doc-rvv/registration/transformation_estimation_svd-RVV.zh.md`

## 继续 / 停止条件

`continue_stop_decision`：PI1 plan 已冻结候选范围；2026-08-14 用户授权继续后进入 PI2-PI5。未获得类似授权时，本 plan 仍不应被解读成自动修改 production 的许可。

`next_phase_default`：执行结果见 `result.zh.md`。PI5 通过后默认下一阶段为 `030-row-source-audit`，范围是 source-indexed、dual-indices 和 correspondences 的独立候选 / 证据审计，不自动扩大当前 ordered-cloud-pair production patch。
