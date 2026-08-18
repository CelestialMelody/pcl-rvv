# registration/correspondence_estimation_organized_projection 函数级 RVV 评估

## 范围

- 主题：`correspondence_estimation_organized_projection`
- 主文件：`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp`
- 公开入口：`pcl::registration::CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar>::determineCorrespondences`
- 专项目录：`test-rvv/registration/correspondence_estimation_organized_projection/`
- 模块依据：`doc-rvv/library-screening/registration/registration-function-evaluation-queue.zh.md` 的建议优化队列第二项。

## Closeout 状态

当前代码工作已达到 production-ready 边界：上游 production 入口已接入 source transform staging、projection-pixel staging 和 target-predicate final predicate；append 和 stored distance 写出保留标量。QEMU `run_test_compare` 通过，std 构建 39 个测试中 10 个 RVV-only 诊断按预期 skip，RVV 构建 39 个测试全部通过；QEMU bench checksum 对齐；板卡 `board_smoke` 39 个专项测试通过，production identity fake/explicit 为 `1.64x` / `1.65x`，production non-identity fake/explicit 均为 `2.36x`。

本评估文档保留较多诊断细节，是为了说明 test-rvv 中 39 个专项测试的保留理由和历史风险边界。当前不建议继续把 `pcl::Correspondence` append RVV 化作为默认任务；后续更适合进入 PR 级整理、模块 second-pass 状态更新或选择下一个 registration 候选。

## 函数级结论

`CorrespondenceEstimationOrganizedProjection` 用于把 source 点云按 `src_to_tgt_transformation_` 变换到 target 相机坐标系，再用相机内参投影到 organized target cloud 的 `(u, v)` 像素。它避免 KdTree；主循环由 source xyz 读取、4x4 变换、投影、`uv[2] > 0`、图像范围检查、target dexel 读取、depth threshold、欧氏距离阈值和 correspondence 顺序写出组成。当前 production RVV 覆盖 source `x/y/z` gather、source finite、4x4 transform staging、transformed `z > 0` 和 `vcompress` 保序 source staging；source staging 成功后继续接入 projection-pixel staging，RVV 计算 `u/v`、in-bounds mask 和 `target_index` 后保序压缩。identity fast path 和 verified non-identity transform staging 都进入该 projection-pixel production helper。target-predicate production 已接入最终 predicate：RVV gather target `x/y/z`，执行 target finite、depth threshold 和与 Eigen `Vector3f::norm()` lowering 对齐的 distance threshold，再 `vcompress` 保序输出候选；`pcl::Correspondence` 写出前仍按 production 标量 Eigen `norm()` 重算 stored distance，保持输出字段 bit pattern 和顺序 append 语义。

target-predicate 独立诊断、production-shaped prototype 评估和 production 接入均已完成。诊断 prototype 从 `ProjectedCandidate{source_index, target_index, x, y, z}` 后开始，RVV gather target `x/y/z`，批量执行 target finite、depth threshold 和 distance threshold mask，再用 `vcompress` 保序输出 `AcceptedCandidate{source_index, target_index, distance}`。production 版把 target gather、target finite、depth threshold 和最终 distance predicate 前移到 RVV；distance 计算使用与当前 Eigen `Vector3f::norm()` 标量 lowering 对齐的 `fmul/fmacc/fmacc/fsqrt.f32` 结构。由于标量谓词是 `double(float_norm) < max_distance`，production RVV 在 `static_cast<double>(float(max_distance)) < max_distance` 时使用 `dist <= float(max_distance)`，否则使用 `dist < float(max_distance)`，精确保留 double threshold 语义。QEMU `run_test_compare` 更新为 std 构建 39 个测试，其中 10 个 RVV-only 诊断按预期 skip；RVV 构建 39 个测试全部通过。板卡性能结论仍以 `board_smoke` 为准；QEMU timing 只作为 correctness、checksum、日志格式和指令路径证据。

当前覆盖：

- 通过 PCL traits 证明有单个 `float x/y/z` 字段的 source / target 点类型；
- 公共 trait 重构后，production impl 使用 `pcl::rvv::RVVXYZFloatLayout<PointT>` 替代本地 `OrganizedProjectionXYZFloatLayout`；该 trait 只表达 CEOP 原有的单个 `float x/y/z` 字段语义，不额外加入 POD / standard-layout gate，因此不收窄 `PointXYZI` 覆盖。
- 专项测试覆盖 `PointXYZ -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointXYZI -> PointXYZI`；
- 显式全量 `indices=[0,n)`，以及 test-only 派生诊断类和生产入口中的 fake indices、`setIndices()` subset；
- source finite、4x4 transform 后 `z > 0` 前置过滤；identity transform 保留原始 xyz 快路径并继续进入 projection-pixel production staging；non-identity transform 使用 Eigen-aligned FMA staging 后继续进入同一个 projection-pixel production helper；
- RVV staging 命中 `n >= 64` 且 `vlmax_e32m2 <= 64`；小规模、非 RVV 构建、更大 VLEN 或覆盖条件失败回退标量。
- target-predicate staging 已以最终 distance predicate 形式进入 production；`test-rvv` prototype 仍保留完整 distance mask 诊断，覆盖 fake indices、`setIndices()` subset、identity、non-identity、tight threshold、`PointXYZI` 和 small fallback。

## 函数族评估

| 函数 / 路径                                         | 优先级 | RVV 决策                                    | 覆盖 / 回退                                                                              |
| --------------------------------------------------- | ------ | ------------------------------------------- | ---------------------------------------------------------------------------------------- |
| `determineCorrespondences` 前置 source staging | 高     | 已扩大生产分流到 verified non-identity transform staging | `float x/y/z` traits guard、`indices.size() >= 64`、source / target byte offset 可用 `uint32_t` 表达 |
| projection-pixel staging | 高 | 已接入 production projection-pixel gate | RVV 计算 `u/v`、in-bounds mask 和 `target_index`，覆盖 identity 与 verified non-identity source staging；后续 target-predicate production helper 消费该 staging |
| full correspondence / production direct                      | 高     | 已接入 production 三阶段分流                              | RVV 覆盖 source transform、projection-pixel 和 target-predicate；`pcl::Correspondence` append 保持标量顺序 |
| target-predicate production staging | 高 | 已完成 test-rvv 独立诊断、production-shaped prototype，并以最终 distance predicate 接入 production | 从 projected staging 后 gather target `x/y/z`，RVV 执行 target finite、depth mask 和 Eigen-aligned distance threshold；accepted lane 仍标量重算 stored distance 后 append |
| 泛型 `PointSource` / `PointTarget`              | 中     | traits-gated 生产覆盖                       | 已验证 `PointXYZI` 组合；不满足 traits 时回退标量 |
| 任意 subset / 非连续 indices                        | 中     | 已由 production direct 和 production-shaped diagnostic 覆盖 | fake indices、乱序 / 重复 subset 的生命周期和排序语义已纳入专项测试 |
| target organized 间接读取                           | 中     | 已接入 production target gather                                    | `target_index` 由 projection-pixel staging 生成，target `x/y/z` 通过 indexed gather 读取                        |
| `determineReciprocalCorrespondences`              | 低     | 间接受益                                    | 原实现只调用 `determineCorrespondences`                                                |

## RVV 诊断设计

新增 `correspondence_estimation_organized_projection_diag.hpp`，只位于 `test-rvv` 专项目录：

- `ProjectionParams`：展开原对象中的 `src_to_tgt_transformation_`、`fx/fy/cx/cy`、`depth_threshold_` 和 `max_distance`；
- `ProjectionCandidate`：保存压缩后的 `source_index` 和变换后 `x/y/z`；
- `determineCorrespondencesStd`：保留标量公式；
- `projectCandidatesRVV`：`__RVV10__` 下用 PCL traits 判断 source / target 是否具备单个 `float x/y/z` 字段，再通过公共 `rvv_load::indexed_load3_fields_f32m2` 读取 source xyz，批量执行 4x4 变换和 source finite / `z > 0` 前置 mask，再用 `vcompress` 保序写 staging；
- `finishCorrespondencesFromCandidates`：标量计算 `u/v`、访问 organized target，执行 target finite、depth、distance 与 correspondence 写出；
- `determineCorrespondencesCandidate`：source candidate staging 成功时进入 staged tail，否则走 Std。
- `determineCorrespondencesProjectedIdentityCandidate`：只在 exact identity transform 下尝试把 `u/v` 截断和图像范围检查前移到 RVV；条件不满足时回退 Std。
- `determineCorrespondencesProjectedCandidate`：不加 identity gate，用于验证 non-identity transform staging 与 projection-pixel staging 组合后的 full correspondence 一致性。
- `AcceptedCandidate` / `acceptProjectedCandidatesRVV`：从 `ProjectedCandidate` 后开始，gather target `x/y/z`，执行 target finite、depth threshold 和 distance threshold mask，并用 `vcompress` 保序输出 accepted staging；distance 输出按压缩后 lane 使用标量 `double sqrt` 公式重算，保持 checksum 与最终 correspondence 一致。
- `determineCorrespondencesAcceptedCandidate`：组合 source staging、projection-pixel staging、target-predicate staging 和 append-only 标量 tail。
- `CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource, PointTarget, Scalar, DiagMode>`：test-only 派生上游 CEOP 类，外部使用 `setInputSource`、`setInputTarget`、可选 `setIndices` 和同名 `determineCorrespondences`，内部先走上游 `initCompute()`，再按 `DiagMode::Std`、`DiagMode::Candidate`、`DiagMode::ProjectedIdentity`、`DiagMode::Projected` 或 `DiagMode::Accepted` 对拍。

RVV helper 不使用 `_rm` intrinsic，也不读写 FRM/FCSR。早期诊断曾把 `u/v` 截断也放进 RVV 阶段，QEMU 对拍发现边界 lane 的 correspondence 数量不一致；定位后确认投影表达式的 FMA contraction 与普通 `vfmul + vfadd` 顺序会影响接近像素边界的 `static_cast<int>` 结果。本轮将 projection-pixel 诊断中的 RVV 投影改为 `z*cx` 作为累加初值，再用 `vfmacc` 融合 `fx*x`，与标量编译器常见 contraction 形态对齐。修改后 projection-pixel 诊断在 QEMU std/RVV 下恢复 correspondence 完全一致。

当前生产 RVV 覆盖范围由三个 staging helper 组成。`projectOrganizedProjectionCandidatesRVV` 负责 gather source `x/y/z`、source finite、identity 快路径或 non-identity Eigen-aligned 4x4 transform、transformed `z > 0` mask 和 `OrganizedProjectionCandidate` 保序压缩。`projectOrganizedProjectionPixelsRVV` 继续读取 candidate staging，使用 RVV 计算 `u/v`、图像范围 mask 和 `target_index`，并压缩为 `ProjectedOrganizedProjectionCandidate`；identity fast path 和 verified non-identity transform staging 都进入这个 helper。`acceptProjectedOrganizedProjectionCandidatesRVV` 再 gather target `x/y/z`，执行 target finite、depth threshold 和最终 distance predicate，并压缩为 `AcceptedOrganizedProjectionCandidate`。accepted lane 的 `pcl::Correspondence::distance` 仍按标量 Eigen `norm()` 重算后写出，保持 stored distance 与 production 标量表达式绑定。

本轮公共 trait 重构不改变三阶段 production 分流：`Scalar=float`、`__RVV10__`、`indices.size() >= 64`、`vlmax_e32m2 <= 64`、source/target 32-bit byte offset 可表达、target organized 与 staged fallback 边界全部保持不变。专项测试源新增 static_assert，确认 `PointXYZ` / `PointXYZI` 满足 `RVVXYZFloatLayout`，而 `PointXYZI` 不满足 normal layout gate，防止把 CEOP 的字段 gate 和 symmetric LLS 的 normal gate 混用。

标量片段到 production RVV 状态的对应关系：

| 标量片段 | 当前 production RVV 状态 |
| --- | --- |
| `for (const auto& src_idx : (*indices_))` | RVV 分 chunk 处理，`vcompress` 保序输出 staging。 |
| `isFinite((*input_)[src_idx])` | RVV source finite mask。 |
| `src_to_tgt_transformation_ * getVector4fMap()` | RVV source transform staging；identity fast path 直接保存原始 `x/y/z`，non-identity path 按 Eigen lowering 对齐 FMA。 |
| `Eigen::Vector3f p_src3(...)` | staging 保存 transform 后 `x/y/z`。 |
| `projection_matrix_ * p_src3` | RVV projection-pixel staging 计算 `uv0 = fx*x + cx*z`、`uv1 = fy*y + cy*z`；`uv2` 等价为 transform 后 `z`。 |
| `if (uv[2] <= 0) continue` | RVV keep mask 使用 `z > 0`；标量是拒绝条件，RVV 是保留条件。 |
| `static_cast<int>(uv[0] / uv[2])` / `static_cast<int>(uv[1] / uv[2])` | RVV `vfmacc + vfdiv.vv + vfcvt.rtz.x.f.v`。 |
| image bounds check | RVV in-bounds mask。 |
| `target.at(u, v)` | RVV indexed gather target `x/y/z`，index 来自 `target_index = v * width + u`。 |
| `isFinite(pt_tgt)` | RVV target finite mask。 |
| `std::abs(uv[2] - pt_tgt.z) > depth_threshold_` | RVV depth mask。 |
| `(p_src3 - pt_tgt.getVector3fMap()).norm()` | RVV final distance predicate：`vfmul + vfmacc + vfmacc + vfsqrt.f32`。 |
| `dist < max_distance` | RVV predicate 保留 `double(float_norm) < max_distance` 语义。 |
| `pcl::Correspondence(...)` append | 标量 append；stored distance 写出前标量重算 Eigen `norm()`。 |

`uv[2] <= 0` 与 RVV `z > 0` 是同一深度 predicate 的两种表达。`projection_matrix_` 在构造时初始化为 identity，`initCompute()` 只写入 `fx/fy/cx/cy` 对应元素，第三行保持 `[0, 0, 1]`，所以 `uv[2] = p_src3.z`。标量代码用 `continue` 剔除 `uv[2] <= 0`，RVV mask 用 `z > 0` 保留正深度 lane。

### staging 对照表

| staging 名称 | 结构体 | 生成 helper / 代码位置 | 字段来源 | 下一段消费者 / tail | gate |
| --- | --- | --- | --- | --- | --- |
| 诊断 source transform staging | `ProjectionCandidate` | `projectCandidatesRVV()`，`test-rvv/.../correspondence_estimation_organized_projection_diag.hpp` | source index；source `x/y/z`；identity 或 4x4 transform 后 `x/y/z` | `finishCorrespondencesFromCandidates()` 或诊断 `projectPixelsRVV()` | diagnostic / bench |
| 诊断 projection-pixel staging | `ProjectedCandidate` | `projectPixelsRVV()`，`test-rvv/.../correspondence_estimation_organized_projection_diag.hpp` | `ProjectionCandidate`；RVV `u/v` 和 `target_index` | `finishCorrespondencesFromProjected()` | `DiagMode::ProjectedIdentity` 覆盖 identity；`DiagMode::Projected` 覆盖 non-identity production-shaped 诊断 |
| 诊断 target-predicate staging | `AcceptedCandidate` | `acceptProjectedCandidatesRVV()`，`test-rvv/.../correspondence_estimation_organized_projection_diag.hpp` | `ProjectedCandidate`；target `x/y/z` gather；target finite、depth/distance mask 后保留的 `source_index`、`target_index`、`distance` | `finishCorrespondencesFromAccepted()` append-only 标量 tail | `DiagMode::Accepted`；diagnostic / bench；production 采用同类 distance predicate，并在 append 前标量重算 stored distance |
| production source transform staging | `OrganizedProjectionCandidate` | `projectOrganizedProjectionCandidatesRVV()`，`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | `*indices_`；`*input_` source `x/y/z`；identity fast path 或 Eigen-aligned 4x4 transform 后 `x/y/z` | 进入 `projectOrganizedProjectionPixelsRVV()`；该 helper 失败时进入 `finishOrganizedProjectionCorrespondences()` | production RVV traits / size / VLEN gate |
| production projection-pixel staging | `ProjectedOrganizedProjectionCandidate` | `projectOrganizedProjectionPixelsRVV()`，`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | `OrganizedProjectionCandidate`；RVV `vfmacc + vfdiv + vfcvt.rtz` 得到的 `u/v` 和 `target_index` | `finishOrganizedProjectionCorrespondencesFromProjected()` | production RVV traits / size / VLEN gate；source staging 成功后尝试 |
| production target-predicate staging | `AcceptedOrganizedProjectionCandidate` | `acceptProjectedOrganizedProjectionCandidatesRVV()`，`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | `ProjectedOrganizedProjectionCandidate`；target `x/y/z` gather；target finite、depth mask 和 Eigen-aligned distance predicate 后保留的 `source_index`、`target_index`、标量重算后的 `distance` | `finishOrganizedProjectionCorrespondencesFromAccepted()` append | production RVV traits / size / VLEN gate；projected staging 成功后尝试 |

三个 production RVV helper 对应原标量循环的连续三段。`projectOrganizedProjectionCandidatesRVV()` 是 source 候选点生成阶段：读取 source、做 finite 检查、完成 identity / 4x4 transform 和正深度过滤，并输出 `OrganizedProjectionCandidate`。`projectOrganizedProjectionPixelsRVV()` 是像素投影阶段：把 transform 后的 source 坐标投影到 organized target 像素，计算 `target_index`，并输出 `ProjectedOrganizedProjectionCandidate`。`acceptProjectedOrganizedProjectionCandidatesRVV()` 是 target 谓词筛选阶段：读取 target 点并完成 target finite、depth 和 distance predicate，输出 `AcceptedOrganizedProjectionCandidate`。

| production helper | 阶段含义 | 对应标量代码片段 | 成功后输出 |
| --- | --- | --- | --- |
| `projectOrganizedProjectionCandidatesRVV()` | source 候选点生成 | `for (src_idx)`、`isFinite(input[src_idx])`、`src_to_tgt_transformation_ * getVector4fMap()`、`p_src3`、`uv[2] > 0` | `OrganizedProjectionCandidate{source_index, x, y, z}` |
| `projectOrganizedProjectionPixelsRVV()` | 像素投影与图像范围过滤 | `projection_matrix_ * p_src3`、`u/v = static_cast<int>(uv/z)`、image bounds check、`target_index = v * width + u` | `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}` |
| `acceptProjectedOrganizedProjectionCandidatesRVV()` | target 读取与 predicate 过滤 | `target.at(u,v)`、`isFinite(pt_tgt)`、depth threshold、`norm()` distance predicate、`dist < max_distance` | `AcceptedOrganizedProjectionCandidate{source_index, target_index, distance}` |

`determineCorrespondences()` 中的三个 `if (helper(...))` 不是新的业务 predicate，而是 RVV 阶段是否成功命中的 gate：满足 traits、规模、VLEN 和 32-bit offset 边界时，该阶段用 RVV 生成下一段 staging；否则从当前 staging 回到对应标量 tail。`finishOrganizedProjectionCorrespondences()` 是 pixel staging helper 失败时的标量 projection tail。

关键代码边界如下。RVV staging 只写出 `ProjectionCandidate`：

```cpp
pcl::rvv_load::indexed_load3_fields_f32m2<
    typename pcl::traits::POD<PointSource>::type, kXOff, kYOff, kZOff>(
    base, offsets, vl, x, y, z);

vfloat32m2_t tx_lo = __riscv_vfmul_vf_f32m2(y, transform(0, 1), vl);
tx_lo = __riscv_vfmacc_vf_f32m2(tx_lo, transform(0, 0), x, vl);
vfloat32m2_t tx_hi = __riscv_vfmv_v_f_f32m2(transform(0, 3), vl);
tx_hi = __riscv_vfmacc_vf_f32m2(tx_hi, transform(0, 2), z, vl);
vfloat32m2_t tx = __riscv_vfadd_vv_f32m2(tx_hi, tx_lo, vl);
// ty/tz use the same two-part FMA row-dot structure.
vbool16_t keep = finite(x) & finite(y) & finite(z) & (z > 0.0f);

const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(v_indices_u, keep, vl);
const vfloat32m2_t x_kept = __riscv_vcompress_vm_f32m2(tx, keep, vl);
const vfloat32m2_t y_kept = __riscv_vcompress_vm_f32m2(ty, keep, vl);
const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(tz, keep, vl);

out[kept + lane] =
    ProjectionCandidate{source_buf[lane], x_buf[lane], y_buf[lane], z_buf[lane]};
```

标量尾段逐 candidate 完成投影、target 读取和输出：

```cpp
const int u = static_cast<int>(uv0 / candidate.z);
const int v = static_cast<int>(uv1 / candidate.z);
if (u < 0 || u >= static_cast<int>(target.width) || v < 0 ||
    v >= static_cast<int>(target.height))
  continue;

const pcl::PointXYZ& pt_tgt = target.at(u, v);
if (!isFiniteXYZ(pt_tgt))
  continue;
if (std::abs(candidate.z - pt_tgt.z) > params.depth_threshold)
  continue;

if (dist < params.max_distance)
  correspondences[c_index++] = pcl::Correspondence(
      static_cast<int>(candidate.source_index), v * target.width + u,
      static_cast<float>(dist));
```

泛型点类型在诊断层使用 traits gate 收敛到 PCL 注册字段中的单个 `float x/y/z`：

```cpp
template <typename PointT, bool HasXYZ = pcl::traits::has_xyz<PointT>::value>
struct XYZFloatLayout : std::false_type {};

template <typename PointT>
struct XYZFloatLayout<PointT, true>
    : std::bool_constant<
          std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::x>::decomposed::type, float> &&
          std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::y>::decomposed::type, float> &&
          std::is_same_v<typename pcl::traits::datatype<PointT, pcl::fields::z>::decomposed::type, float> &&
          pcl::traits::datatype<PointT, pcl::fields::x>::decomposed::value == 1 &&
          pcl::traits::datatype<PointT, pcl::fields::y>::decomposed::value == 1 &&
          pcl::traits::datatype<PointT, pcl::fields::z>::decomposed::value == 1> {};
```

该 gate 已通过 QEMU std/RVV 对拍验证 `PointXYZ -> PointXYZI` 和 `PointXYZI -> PointXYZI`。production source transform staging 使用同一 traits 边界，identity 和 non-identity transform 均在满足条件时进入 RVV 前置 staging。

## 最小生产接入设计

生产改动位于 `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp`，不改变公开 API 和类成员布局。新增实体放在同一 impl header 的 `pcl::registration::detail` 中：

- `OrganizedProjectionCandidate`：生产 staging 结构，保存 `source_index` 和变换后的 `x/y/z`；identity transform 下直接保存原始 source xyz，non-identity transform 下保存 Eigen-aligned FMA staging 结果；
- `pcl::rvv::RVVXYZFloatLayout`：`__RVV10__` 下的公共 traits gate，要求 source / target 有 PCL 注册的单个 `float x/y/z` 字段；
- `organizedProjectionTransformIsIdentity`：source transform staging 的运行时分支 helper，判断入口传入的 `src_to_tgt_transformation_` 是否为精确 identity；identity 时直接 staging 原始 source xyz，non-identity 时进入 Eigen-aligned FMA staging；
- `projectOrganizedProjectionCandidatesRVV`：生产 RVV source staging helper，只在 `__RVV10__` 下编译；
- `ProjectedOrganizedProjectionCandidate`：projection-pixel production staging 结构，保存 `source_index`、`target_index` 和 transform 后 `x/y/z`；
- `projectOrganizedProjectionPixelsRVV`：production helper，使用 `vfmacc`、`vfdiv.vv`、`vfcvt.rtz.x.f.v` 和 in-bounds mask 生成 target linear index；
- `finishOrganizedProjectionCorrespondences`：pixel staging helper 失败后的标量 projection tail，使用与原循环一致的 `projection_matrix_ * p_src3`、`static_cast<int>`、`target.at(u,v)`、target finite、depth/distance 和 append 顺序；
- `finishOrganizedProjectionCorrespondencesFromProjected`：target-predicate RVV helper 失败时使用的 projected staging 标量 tail，按 `target_index` 读取 target，并保留 target finite、depth/distance 和 append 顺序；
- `AcceptedOrganizedProjectionCandidate`：target-predicate production staging 结构，保存通过 RVV final predicate 的 `source_index`、`target_index` 和标量重算后的 `distance`；
- `acceptProjectedOrganizedProjectionCandidatesRVV`：production helper，gather target `x/y/z`，执行 target finite、depth mask 和与 Eigen `Vector3f::norm()` lowering 对齐的 final distance predicate；
- `finishOrganizedProjectionCorrespondencesFromAccepted`：append-only 标量 tail，按 accepted staging 顺序写出 `pcl::Correspondence`；
- `determineCorrespondencesOrganizedProjectionStd`：原标量循环抽出的 fallback helper，保留原循环作为非 RVV、fallback 和未覆盖模板的标量路径。

`determineCorrespondences()` 的生产分流很短：

```cpp
#if defined(__RVV10__)
if constexpr (std::is_same_v<Scalar, float>) {
  std::vector<detail::OrganizedProjectionCandidate> candidates;
  if (detail::projectOrganizedProjectionCandidatesRVV(
          *input_, *target_, *indices_, src_to_tgt_transformation_, candidates)) {
    std::vector<detail::ProjectedOrganizedProjectionCandidate> projected;
    if (detail::projectOrganizedProjectionPixelsRVV(
            *target_, projection_matrix_, candidates, projected)) {
      std::vector<detail::AcceptedOrganizedProjectionCandidate> accepted;
      if (detail::acceptProjectedOrganizedProjectionCandidatesRVV(
              *target_, depth_threshold_, max_distance, projected, accepted)) {
        detail::finishOrganizedProjectionCorrespondencesFromAccepted(
            accepted, correspondences);
        return;
      }
      detail::finishOrganizedProjectionCorrespondencesFromProjected(
          *target_, depth_threshold_, max_distance, projected, correspondences);
      return;
    }
    detail::finishOrganizedProjectionCorrespondences(
        *target_, projection_matrix_, depth_threshold_, max_distance,
        candidates, correspondences);
    return;
  }
}
#endif

detail::determineCorrespondencesOrganizedProjectionStd(...);
```

显式传参的原因是这些生产 helper 位于 `pcl::registration::detail`，属于 free helper。它们没有类成员函数里的 `this`，无法直接访问 `input_`、`target_`、`indices_`、`projection_matrix_`、`depth_threshold_` 或 `src_to_tgt_transformation_`，所以由入口把实际需要的对象状态传入。该设计避免修改公开类声明和成员布局。

`finishOrganizedProjectionCorrespondences` 和 `finishOrganizedProjectionCorrespondencesFromProjected` 是分阶段 fallback tail。production RVV 入口按 source 候选点生成、像素投影、target 谓词筛选逐级尝试：

- source staging 成功但 `projectOrganizedProjectionPixelsRVV()` 失败时，`finishOrganizedProjectionCorrespondences()` 消费 `OrganizedProjectionCandidate{source_index, x, y, z}`，从 transform 后的 `x/y/z` 继续执行投影、target 读取、depth/distance 和 append；
- projection-pixel staging 成功但 `acceptProjectedOrganizedProjectionCandidatesRVV()` 失败时，`finishOrganizedProjectionCorrespondencesFromProjected()` 消费 `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}`，不再计算 `u/v` 或 image bounds，只继续 target finite、depth/distance 和 append；
- target-predicate staging 成功时，target gather、target finite、depth mask 和 final distance predicate 已完成，只调用 `finishOrganizedProjectionCorrespondencesFromAccepted()` 做 append-only 标量写出。

因此，这两个 tail 的存在不是因为 Correspondence 前的 production 成功路径仍需标量补算，而是为了保留中间 RVV helper 未命中时的 staged fallback。std/fallback 路径是 `determineCorrespondencesOrganizedProjectionStd`，它仍然遍历 `indices`、读取 `input[src_idx]`，并保持原 `determineCorrespondences` 的标量流程。

当前 QEMU / board production case 的预期热路径是三个 RVV helper 连续成功后进入 `finishOrganizedProjectionCorrespondencesFromAccepted()`。`finishOrganizedProjectionCorrespondences()` 和 `finishOrganizedProjectionCorrespondencesFromProjected()` 在这些 case 中通常不会执行；它们保留给 partial fallback，例如前一阶段 `vcompress` 后候选数低于后续 helper 的 `n >= 64` gate，或后续 helper 因 VLEN、offset、布局边界失败。

生产 fallback 条件：

- 非 `__RVV10__` 构建；
- `Scalar` 非 `float`；
- source 或 target 不满足单个 `float x/y/z` traits gate；
- `indices_->size() < 64`；
- source/target 点数对应的 byte offset 或 indices 数量超出当前 32-bit 边界；
- `__riscv_vsetvlmax_e32m2() > 64`，避免固定临时 lane buffer 在更大 VLEN 上越界。

`vlmax_e32m2 <= 64` gate 与当前 RVV 变量类型相关。production helper 使用 `vfloat32m2_t`、`vuint32m2_t` 和 `vint32m2_t`，对应 `e32,m2`；`__riscv_vsetvlmax_e32m2()` 返回当前硬件 VLEN 下该类型一次最多能处理的 32-bit lane 数，近似为 `VLEN_bits * 2 / 32`。helper 内的 `source_buf[64]`、`target_buf[64]`、`x/y/z_buf[64]` 等固定栈 buffer 用于承接 `vcompress` 后的低 lane；最坏情况下所有 lane 都保留，`keep_count == vlmax_e32m2`。因此 `vlmax_e32m2 > 64` 时继续 `vse32(..., keep_count)` 可能越界，当前实现选择回退标量或 staged fallback。这个 gate 不限制总输入规模；总输入仍由 `__riscv_vsetvl_e32m2(n - i)` 分 chunk 处理。

第二阶段生产路径接入 `u/v` 投影截断、图像范围检查和 `target_index` staging；第三阶段生产路径接入 target gather、target finite、depth mask 和最终 distance predicate。`pcl::Correspondence` 写出仍保持标量，accepted lane 的 distance 值由 production 标量 Eigen `norm()` 重算。`ProjectionPixelRvvDiagnosticMatchesScalar`、两个 projection-pixel 入口诊断测试、两个 non-identity projection-pixel production-shaped 诊断和 identity / non-identity production boundary 测试共同覆盖 `vfmacc` 对齐后的 `u/v` 语义。`ProductionDistanceBoundaryPredicateMatchesScalar` 覆盖 distance 边界 lane：标量 float norm 等于 `float(max_distance)`、但 double threshold 略大时，RVV predicate 必须接受 lane，与 `double(float_norm) < max_distance` 一致。`DistanceRvvMatchesEigenNormBits` 证明当前 RVV distance bit pattern 与 Eigen `Vector3f::norm()` 一致；`DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` 证明 `<` / `<=` 分支保留 double threshold 语义。`acceptProjectedCandidatesRVV()` 作为 test-rvv 完整 distance-mask prototype 保留；production 对应 helper 是 `acceptProjectedOrganizedProjectionCandidatesRVV()`。

`projectOrganizedProjectionPixelsRVV()` 中的 `source_buf[64]`、`target_buf[64]`、`x_buf[64]`、`y_buf[64]`、`z_buf[64]` 是 projection-pixel staging 的布局桥接。RVV 侧以 SoA register 形式分别保存 `source_index`、`target_index` 和 transform 后 `x/y/z`，并对每个字段使用同一个 `keep` mask 执行 `vcompress`；production staging 侧需要写出 AoS 结构 `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}`。当前实现先用 `vse32` 把压缩后的低 `keep_count` lane 落到这些临时 SoA buffer，再用短标量循环组装结构体。该写法是多字段稀疏 SIMD 输出的常见折中：保留 `vcompress` 的保序语义，避免复杂结构体 scatter，并把可变数量结构体 append 继续留给清晰的 staging / scalar tail。固定长度 64 与 `vlmax_e32m2 <= 64` gate 配套；更大 VLEN 回退标量。Segment store 理论上可以省掉临时 SoA buffer，但当前 projected staging 是 `uint32_t, uint32_t, float, float, float` 混合字段，且 `e32m2` 下 5-field segment store 不满足常见 LMUL / field count 约束；若降 LMUL、拆 store 或 reinterpret float bit pattern，需要重新证明布局、别名、padding 和板卡收益。通用模式记录在 `doc-rvv/rvv/RVV Multi-Field Compress Staging.zh.md`。

## 标量流程与 RVV 分阶段流程

标量实现是流式处理：每个 source index 立即完成变换、投影、target 读取和 correspondence 写出，不需要中间结构。

RVV 诊断和 production 路径拆成三个可独立验证的 staging 阶段，最后只保留 correspondence append 为标量：

| 阶段                | 标量语义来源                                                                                        | RVV / 标量职责                                    |
| ------------------- | --------------------------------------------------------------------------------------------------- | ------------------------------------------------- |
| source 读取         | `(*input_)[src_idx]`                                                                              | RVV gather `x/y/z`，保留 `source_index`       |
| 4x4 transform        | `src_to_tgt_transformation_ * getVector4fMap()`                                                   | identity 直接保留 source xyz；non-identity 使用两个 FMA 部分和 |
| 前置筛选            | source finite、transformed `z > 0`                                                               | RVV mask 合并                                     |
| source candidate staging | 标量无中间结构                                                                                  | `vcompress` 保序保存 `ProjectionCandidate` / `OrganizedProjectionCandidate` |
| 像素投影            | `static_cast<int>(uv/uv_z)`、image bounds、`target_index = v * width + u`                         | RVV `vfmacc + vfdiv + vfcvt.rtz`，再 `vcompress` 保存 projected staging |
| target 谓词         | `target_->at(u,v)`、target finite、depth threshold、distance threshold                            | RVV indexed gather、mask 和 distance predicate，保存 accepted staging |
| 输出写出            | `correspondences[c_index++]`                                                                       | 标量 append；stored distance 写出前按 Eigen `norm()` 重算 |

staging 的额外内存流量是生产接入风险；full diagnostic 和 production bench 必须证明它没有抵消前置 RVV 收益。当前板卡结果已经证明 source、projection-pixel 和 target-predicate 三段接入后仍有 production speedup。

## 风险和处理

| 风险                          | 处理                                                                               |
| ----------------------------- | ---------------------------------------------------------------------------------- |
| `static_cast<int>` 截断语义 | production 已使用 `vfmacc + vfdiv + vfcvt.rtz` 前移到 RVV；边界测试和反汇编证明与标量 contraction 对齐；不使用 `_rm`，不污染 FRM |
| target organized 间接访问     | production 已使用 indexed gather 前移到 RVV；保留诊断测试覆盖 target finite、depth/distance mask 和第二次 `vcompress` 保序 |
| FMA 与标量表达式浮点差        | transform / projection / distance predicate 按反汇编对齐 FMA 结构；distance threshold 额外保留 `double(float_norm) < max_distance` 语义；专项测试要求 index 和数量完全一致，stored distance 值只允许 `1e-6f` 容差        |
| 泛型点类型                    | production 和诊断均通过 traits gate 覆盖单个 `float x/y/z` 字段；专项覆盖 `PointXYZ -> PointXYZ`、`PointXYZ -> PointXYZI` 和 `PointXYZI -> PointXYZI` |
| 小规模启动成本                | `n < 64` 回退标量                                                                |

## 测试计划

专项测试：`test-rvv/registration/correspondence_estimation_organized_projection/test_correspondence_estimation_organized_projection.cpp`

测试按证据层级组织。低层 helper 测试用于定位数值或 staging 问题；test-only 派生类测试用于覆盖上游对象状态；production 测试直接调用真实公开入口；projection-pixel 和 target-predicate 诊断测试保留为阶段归因证据。

| 测试 | 层级 | 作用 |
| ---- | ---- | ---- |
| `CandidateMatchesScalar` | 低层 helper | 显式 `[0,n)` indices 下，对拍 RVV source transform staging + 标量 tail 与手写标量公式。 |
| `CandidateSupportsPointXYZITarget` | 低层 helper / traits | 验证 target 带额外字段时，traits gate 和标量 tail 仍按 `x/y/z` 语义工作。 |
| `CandidateSupportsPointXYZISourceAndTarget` | 低层 helper / traits | 验证 source gather offset 和 target tail 都支持 `PointXYZI`。 |
| `SmallInputFallbackMatchesScalar` | 低层 helper / fallback | 覆盖 `n < 64` helper fallback，不让小输入误进 RVV staging。 |
| `DiagnosticFakeIndicesMatchesScalar` | production-shaped diagnostic | 继承上游 CEOP，覆盖 `initCompute()` 和 PCLBase fake indices。 |
| `DiagnosticSubsetIndicesMatchesScalar` | production-shaped diagnostic | 覆盖 `setIndices()` 的乱序、重复 subset，检查输出顺序和重复 query 保留。 |
| `PclClassMatchesScalarFormula` | 标量 reference 校验 | 证明手写标量 reference 与上游类在 identity baseline 上一致。 |
| `ProductionFakeIndicesMatchesScalar` | production direct | 真实 production 入口的 identity/fake-indices 主证据。 |
| `ProductionSubsetIndicesMatchesScalar` | production direct | 真实 production 入口的乱序、重复 subset 证据。 |
| `ProductionSupportsPointXYZITarget` | production direct / traits | 真实 production 入口覆盖 `PointXYZ -> PointXYZI`。 |
| `ProductionSupportsPointXYZISourceAndTarget` | production direct / traits | 真实 production 入口覆盖 `PointXYZI -> PointXYZI`。 |
| `ProductionSmallInputFallbackMatchesScalar` | production direct / fallback | 真实 production 入口覆盖 `n < 64` fallback。 |
| `ProductionDistanceBoundaryPredicateMatchesScalar` | production direct / distance boundary | 构造 scalar float norm 位于 `float(max_distance)` 附近的 lane，验证 RVV distance predicate 与 `double(float_norm) < max_distance` 一致。 |
| `DistanceRvvMatchesEigenNormBits` | RVV-only distance 诊断 | 直接对拍 `vfmul + vfmacc + vfmacc + vfsqrt.f32` 与 Eigen `Vector3f::norm()` 的 float bit pattern。 |
| `DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` | RVV-only threshold 诊断 | 覆盖 `float(max_distance)` 等于、低于 double threshold 的边界，验证 RVV `<` / `<=` 选择等价于标量 double predicate。 |
| `ProductionIdentityProjectionPixelBoundaryMatchesScalar` | production direct / projection boundary | 真实 production 入口覆盖 identity projection-pixel staging 的整数像素边界数据。 |
| `ProductionNonIdentityTransformFakeIndicesMatchesScalar` | production direct | non-identity production gate 在 fake indices 下与标量一致。 |
| `ProductionNonIdentityTransformSubsetIndicesMatchesScalar` | production direct | non-identity production gate 在乱序、重复 subset 下与标量一致。 |
| `ProductionNonIdentityTransformSmallZPointXYZIMatchesScalar` | production direct / traits / 数值边界 | 覆盖小正 `z`、tight threshold 和 `PointXYZI` 布局组合。 |
| `ProductionNonIdentitySmallInputFallbackMatchesScalar` | production direct / fallback | 扩大 transform gate 后继续覆盖 non-identity 小输入 fallback。 |
| `NonIdentityTransformRvvStagingBoundaryMatchesScalar` | RVV-only 低层诊断 | 隔离 4x4 transform staging，固定像素边界 adversarial 数据。 |
| `NonIdentityTransformDiagnosticFakeIndicesMatchesScalar` | RVV-only production-shaped diagnostic | 覆盖 non-identity helper 在 fake indices 对象状态下的一致性。 |
| `NonIdentityTransformDiagnosticSubsetIndicesMatchesScalar` | RVV-only production-shaped diagnostic | 覆盖 non-identity helper 在乱序、重复 subset 下的一致性。 |
| `NonIdentityTransformSmallZPointXYZIMatchesScalar` | RVV-only 低层诊断 / traits | 独立于 production direct case，保留 small-z `PointXYZI` helper 语义探针。 |
| `TightDepthThresholdRejectsMatches` | 标量 tail guard | 验证 RVV staging 后仍由标量 tail 正确执行 depth threshold。 |
| `ProjectionPixelRvvDiagnosticMatchesScalar` | projection-pixel 低层诊断 | 验证 `vfmacc` 对齐后的 `u/v` staging 与标量一致；production projection-pixel gate 已启用同类阶段。 |
| `TargetPredicateRvvDiagnosticMatchesScalar` | target-predicate 低层诊断 | 验证 target gather、target finite、depth/distance mask 和 accepted staging 与标量一致。 |
| `TargetPredicateDiagnosticFakeIndicesMatchesScalar` | target-predicate production-shaped diagnostic | 覆盖 fake indices 对象状态下的 target-predicate staging。 |
| `TargetPredicateDiagnosticSubsetIndicesMatchesScalar` | target-predicate production-shaped diagnostic | 覆盖乱序、重复 subset 下第二次 `vcompress` 的输出顺序。 |
| `TargetPredicateSupportsPointXYZISourceAndTarget` | target-predicate traits | 验证 source/target `PointXYZI` 下 target gather offset 和 append tail。 |
| `TargetPredicateSmallInputFallbackMatchesScalar` | target-predicate fallback | 覆盖 accepted-stage helper 小输入 fallback。 |
| `TargetPredicateTightThresholdsMatchScalar` | target-predicate 边界 | 提高 depth/distance rejection 压力，验证 predicate 顺序与标量一致。 |
| `NonIdentityTargetPredicateDiagnosticFakeIndicesMatchesScalar` | RVV-only target-predicate diagnostic | 覆盖 non-identity transform + projection-pixel + target-predicate 组合。 |
| `NonIdentityTargetPredicateDiagnosticSubsetIndicesMatchesScalar` | RVV-only target-predicate diagnostic | 覆盖 non-identity 组合下的 shuffled duplicate indices 保序。 |
| `IdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar` | projection-pixel production-shaped diagnostic | 在上游式 fake indices 入口下验证 identity projection-pixel 诊断。 |
| `IdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar` | projection-pixel production-shaped diagnostic | 在乱序、重复 subset 下验证 projected staging 保序。 |

### 测试保留策略

当前 39 个专项测试不是全部等价。closeout 后按下面口径保留：

- `production direct` 测试必须保留。它们直接调用上游真实 `CorrespondenceEstimationOrganizedProjection::determineCorrespondences()`，覆盖 fake indices、显式 subset、traits、小输入 fallback、identity / non-identity、projection boundary、distance boundary 和 small-z 组合，是 production 接入证据。
- `RVV-only bit / threshold / adversarial helper` 测试必须保留。`DistanceRvvMatchesEigenNormBits`、`DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate`、`NonIdentityTransformRvvStagingBoundaryMatchesScalar` 等测试固定了曾经导致 checksum 或边界 lane 变化的问题，不能由普通 production smoke 替代。
- `target-predicate`、`projection-pixel` 和 `source candidate` 的低层 / production-shaped diagnostic 暂时保留。它们用于把失败归因到三个 staging 阶段之一，尤其在后续修改 FMA 结构、buffer 组织、LMUL / VLEN gate 或 traits gate 时可以快速定位。
- `PclClassMatchesScalarFormula` 暂时保留。它证明本目录的手写标量 reference 与上游类一致；后续若新增上游 gtest 并直接覆盖同一 baseline，可重新评估是否删除。

可合并候选主要是旧的 production-shaped diagnostic，而不是 production direct 或 RVV-only 边界测试：

- `DiagnosticFakeIndicesMatchesScalar` / `DiagnosticSubsetIndicesMatchesScalar` 与 production direct fake/subset 有重叠，但仍覆盖 test-only source candidate 入口和 `initCompute()` 形状；若后续只保留 production direct，可删除这两个并在文档中说明 source candidate 入口只由低层 helper 覆盖。
- `IdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar` / `IdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar` 与 production identity projection boundary 和 production fake/subset 有重叠，但仍隔离 projection-pixel stage；若 production direct 覆盖进一步增强，可合并到 `ProjectionPixelRvvDiagnosticMatchesScalar` 加 subset case。
- `TargetPredicateDiagnosticFakeIndicesMatchesScalar` / `TargetPredicateDiagnosticSubsetIndicesMatchesScalar` 与 production direct 主路径有重叠，但仍隔离 accepted staging 的第二次 `vcompress` 顺序；若后续添加 production direct 的 accepted-stage counter 或 trace，可重新评估。

本轮不删除上述可合并候选，原因是 CEOP 刚完成 target-predicate production 接入，且历史上 transform / projection FMA 边界曾经出现误判。先保留阶段化诊断，降低后续 closeout 改动或 review 修正时的定位成本。已经删除的重复项只有 `ProductionProjectionBoundaryMatchesScalar`，因为它与 `ProductionFakeIndicesMatchesScalar` 使用同一输入，没有新增 projection-boundary adversarial 条件。

已删除 `ProductionProjectionBoundaryMatchesScalar`：它使用 `makeSource(8192)`、`makeTarget(320,240)`、`makeParams()` 和 fake-indices production 入口，输入与 `ProductionFakeIndicesMatchesScalar` 完全相同，没有额外构造 projection boundary adversarial 数据。实际 projection boundary 风险由 `ProjectionPixelRvvDiagnosticMatchesScalar` 和 non-identity boundary 数据覆盖。

上游测试：当前 `test/registration` 未发现 `CorrespondenceEstimationOrganizedProjection` 的直接 gtest；本轮用专项公开类测试覆盖生产入口。若后续添加上游 gtest，应把本专项中的 fake indices、乱序 / 重复 subset、`PointXYZI`、small fallback 和像素边界 case 迁移到上游测试体系。

## Bench 计划

专项 bench：`test-rvv/registration/correspondence_estimation_organized_projection/bench_correspondence_estimation_organized_projection.cpp`

输出保持可解析：

- `Dataset: synthetic PointXYZ source and organized PointXYZ target; projection staging plus full correspondence diagnostic`
- `Iterations: 30`
- 每个 case 一行 `<name> : <avg> ms/iter`，详情行包含 `Total Time` 和 checksum。

| case                                               | 入口                                           | 规模 / 参数                                                | 路径含义                                    |
| -------------------------------------------------- | ---------------------------------------------- | ---------------------------------------------------------- | ------------------------------------------- |
| `ceop projection-staging pointxyz 64K`           | `projectCandidatesCandidate`                 | 64K source，640x480 organized target，固定内参             | 只测前置 RVV staging，不直接代表生产收益    |
| `ceop projection-staging pointxyz 256K`          | `projectCandidatesCandidate`                 | 256K source，同上                                          | 规模放大诊断                                |
| `ceop identity projection-pixel diagnostic pointxyz 64K`  | `determineCorrespondencesProjectedIdentityCandidate` | identity gate，staging + RVV `u/v` 截断 / 范围检查 + 标量 target / 输出 | 诊断 identity 条件下投影前移是否可继续 RVV 化 |
| `ceop identity projection-pixel diagnostic pointxyz 256K` | `determineCorrespondencesProjectedIdentityCandidate` | 同上                                                       | 诊断增量在大规模下的语义和性能              |
| `ceop target-predicate staging pointxyz 64K` | `projectCandidatesCandidate` + `projectPixelsCandidate` + `acceptProjectedCandidatesCandidate` | 64K source，同参数 | 只测 target gather、target finite、depth/distance mask 和 accepted staging |
| `ceop target-predicate staging pointxyz 256K` | 同上 | 256K source，同参数 | target-predicate staging 规模放大诊断 |
| `ceop identity target-predicate diagnostic pointxyz 64K` | `determineCorrespondencesAcceptedCandidate` | 64K source，同参数 | RVV target predicates + append-only 标量 tail |
| `ceop identity target-predicate diagnostic pointxyz 256K` | 同上 | 256K source，同参数 | target-predicate full diagnostic 规模放大 |
| `ceop full-correspondence pointxyz 64K`          | `determineCorrespondencesCandidate`          | staging + target finite/depth/distance/output              | 接近真实入口的 full diagnostic              |
| `ceop full-correspondence pointxyz 256K`         | `determineCorrespondencesCandidate`          | 同上                                                       | 判断标量尾段和 staging 内存流量是否稀释收益 |
| `ceop upstream-like std fake-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Std>::determineCorrespondences` | 继承上游 CEOP，不调用 `setIndices()` | 真实 `initCompute()` / fake indices 下的标量对照 |
| `ceop upstream-like candidate fake-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Candidate>::determineCorrespondences` | 同上 | 真实 `initCompute()` / fake indices 下的 RVV staging candidate |
| `ceop upstream-like accepted fake-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Accepted>::determineCorrespondences` | 同上 | 真实 `initCompute()` / fake indices 下的 target-predicate prototype |
| `ceop upstream-like std explicit-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Std>::determineCorrespondences` | 继承上游 CEOP，调用 `setIndices(IndicesPtr)` | 显式 indices 生命周期下的标量对照 |
| `ceop upstream-like candidate explicit-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Candidate>::determineCorrespondences` | 同上 | 显式 indices 生命周期下的 RVV staging candidate |
| `ceop production fake-indices pointxyz 64K` | 上游 `CorrespondenceEstimationOrganizedProjection::determineCorrespondences` | 不调用 `setIndices()` | 真实生产入口 fake indices 分流 |
| `ceop production explicit-indices pointxyz 64K` | 上游 `CorrespondenceEstimationOrganizedProjection::determineCorrespondences` | 调用 `setIndices(IndicesPtr)` | 真实生产入口显式 indices 分流 |
| `ceop upstream-like accepted non-identity fake-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Accepted>::determineCorrespondences` | non-identity transform，不调用 `setIndices()` | non-identity target-predicate prototype |
| `ceop upstream-like accepted non-identity explicit-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Accepted>::determineCorrespondences` | non-identity transform，调用 `setIndices(IndicesPtr)` | 显式 indices 下的 non-identity target-predicate prototype |
| `ceop production non-identity fake-indices pointxyz 64K` | 上游 `CorrespondenceEstimationOrganizedProjection::determineCorrespondences` | non-identity transform，不调用 `setIndices()` | 真实生产入口 non-identity gate checksum 证据 |
| `ceop production non-identity explicit-indices pointxyz 64K` | 上游 `CorrespondenceEstimationOrganizedProjection::determineCorrespondences` | non-identity transform，调用 `setIndices(IndicesPtr)` | 显式 indices 下的 non-identity gate checksum 证据 |

## 当前状态

- 函数级评估：已完成 non-identity production gate 扩大评估。
- RVV 实现：已在上游模板 impl 头接入 source transform staging、projection-pixel staging 和 target-predicate final distance predicate；完整 distance mask 版本也保留在 `test-rvv` 诊断 / prototype 层。
- 专项 test / bench / Makefile / board.mk：已建立。
- QEMU：专项 test、bench compare 通过，bench 已包含真实 production case。
- 反汇编：已确认 `vluxei32.v`、`vfmacc.vf`、`vfadd.vv`、`vcompress.vm`、`vcpop.m`、`vse32.v`、`vsetvli ... e32,m2` 路径；test-only non-identity transform staging 已由分离 `vfmul + vfadd` 改为与 Eigen 4x4 evaluator 对齐的两个 FMA 部分和。
- 板卡：target-predicate production 接入后 `board_smoke` 已补跑。39 个专项测试通过；target-predicate staging 为 `1.68x` / `1.69x`；identity target-predicate diagnostic 为 `1.60x` / `1.62x`；上游式 accepted prototype fake/explicit 为 `1.63x` / `1.62x`，non-identity fake/explicit 均为 `2.12x`；production fake/explicit 为 `1.64x` / `1.65x`，non-identity production fake/explicit 均为 `2.36x`，checksum 均保持对齐。

## 验证结果

- QEMU 专项测试：`make -C test-rvv/registration/correspondence_estimation_organized_projection run_test_compare` 通过。std 构建运行 39 个测试，其中 10 个 RVV-only 诊断按预期 skip；RVV 构建 39 个测试全部通过。其中 `CandidateSupportsPointXYZITarget` 和 `CandidateSupportsPointXYZISourceAndTarget` 覆盖 traits-gated `PointXYZI` 组合；`DiagnosticFakeIndicesMatchesScalar` 和 `DiagnosticSubsetIndicesMatchesScalar` 覆盖继承上游类后的真实 `initCompute()`、fake indices 和 `setIndices()` subset；production direct class 测试覆盖 identity fake/subset、identity projection-pixel boundary、distance predicate boundary、`PointXYZI`、small fallback、non-identity fake/subset、non-identity projection boundary、small-z `PointXYZI` 和小输入 fallback；`DistanceRvvMatchesEigenNormBits` 与 `DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` 覆盖 RVV distance bit pattern 和 double threshold predicate；`NonIdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar` 和 `NonIdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar` 覆盖 non-identity projection-pixel production-shaped 诊断。
- QEMU bench：`make -C test-rvv/registration/correspondence_estimation_organized_projection run_bench_compare` 通过，`output/qemu/analyze_bench_compare.log` 无 `未解析`、`n/a`、`Total Time 不计算`。identity production fake/explicit checksum 均为 `10393124863019881355`；non-identity production fake/explicit checksum 均为 `13376866430852120216`。QEMU 只作为构建、checksum、日志格式和指令路径证据。
- 反汇编：`make -C test-rvv/registration/correspondence_estimation_organized_projection dump_bench_rvv` 生成 `build/asm/riscv/bench_correspondence_estimation_organized_projection_rvv.full.asm`；`build/asm/riscv/bench_correspondence_estimation_organized_projection_rvv.asm` 确认 `vsetvli ... e32,m2`、`vluxei32.v`、`vfmul.vf`、`vfmacc.vf`、`vfdiv.vv`、`vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vse32.v`；production 符号 `acceptProjectedOrganizedProjectionCandidatesRVV<PointXYZ>` 附近可见 target/projected gather 的 `vluxei32.v`、depth mask `vmfle.vf`、distance predicate 的 `vfmul.vv`、`vfmacc.vv`、`vfsqrt.v`、`vmfle.vf` / `vmflt.vf`、`vcompress.vm`、`vcpop.m` 和 `vse32.v`；压缩后 stored distance 标量重算仍可见 `fsqrt.s`、`fcvt.d.s` 和 `flt.d`。
- 板卡验证：`make -C test-rvv/registration/correspondence_estimation_organized_projection board_smoke` 通过，日志在 `output/board/`。

Milkv-Jupiter 板卡结果：

| case                                                       | Std ms/iter | RVV ms/iter | speedup | 结论 |
| ---------------------------------------------------------- | ----------: | ----------: | ------: | ---- |
| `ceop projection-staging pointxyz 64K`                   |      7.5715 |      1.4612 |   5.18x | 局部 source staging 收益成立 |
| `ceop projection-staging pointxyz 256K`                  |     30.2948 |      5.8886 |   5.14x | 大规模局部 staging 收益成立 |
| `ceop identity projection-pixel diagnostic pointxyz 64K` |     20.0170 |     11.6048 |   1.72x | `u/v` 前移 diagnostic 在板卡上有收益 |
| `ceop identity projection-pixel diagnostic pointxyz 256K` |     76.9705 |     45.2790 |   1.70x | 大规模 projection-pixel diagnostic 有收益 |
| `ceop target-predicate staging pointxyz 64K` |     18.5612 |     11.0294 |   1.68x | target gather + predicate staging 收益成立 |
| `ceop target-predicate staging pointxyz 256K` |     72.8146 |     43.1484 |   1.69x | 大规模 target-predicate staging 收益成立 |
| `ceop identity target-predicate diagnostic pointxyz 64K` |     20.0242 |     12.4832 |   1.60x | target-predicate full diagnostic 有收益 |
| `ceop identity target-predicate diagnostic pointxyz 256K` |     78.5207 |     48.4039 |   1.62x | 大规模 target-predicate full diagnostic 有收益 |
| `ceop full-correspondence pointxyz 64K`                  |     19.5974 |     13.1481 |   1.49x | full diagnostic 仍有收益 |
| `ceop full-correspondence pointxyz 256K`                 |     75.9962 |     51.6861 |   1.47x | 大规模 full diagnostic 仍有收益 |
| `ceop upstream-like accepted fake-indices pointxyz 64K` |     20.0774 |     12.3510 |   1.63x | 上游式 target-predicate prototype 有收益 |
| `ceop upstream-like accepted explicit-indices pointxyz 64K` |  20.1948 |     12.4807 |   1.62x | 显式 indices 下 prototype 有收益 |
| `ceop production fake-indices pointxyz 64K`              |     21.7347 |     13.2438 |   1.64x | production final distance predicate 收益成立 |
| `ceop production explicit-indices pointxyz 64K`          |     21.7268 |     13.1382 |   1.65x | 显式 indices 下 production final distance predicate 收益成立 |
| `ceop upstream-like accepted non-identity fake-indices pointxyz 64K` | 15.9257 | 7.5150 | 2.12x | non-identity prototype 有收益 |
| `ceop upstream-like accepted non-identity explicit-indices pointxyz 64K` | 16.0207 | 7.5549 | 2.12x | 显式 indices 下 non-identity prototype 有收益 |
| `ceop production non-identity fake-indices pointxyz 64K` |     16.7381 |      7.0985 |   2.36x | non-identity production final distance predicate 收益成立 |
| `ceop production non-identity explicit-indices pointxyz 64K` |  16.7513 |      7.1087 |   2.36x | 显式 indices 下 non-identity production final distance predicate 收益成立 |

更新后的板卡 `board_smoke` 结果显示 39 个专项测试全部通过。target-predicate staging 64K/256K checksum 分别为 `4417340144073031140` 和 `15052876296552908126`，std/RVV 对齐；identity target-predicate diagnostic 64K/256K checksum 分别为 `140547372586179969` 和 `13594340667184250285`，std/RVV 对齐。上游式 accepted prototype 的 identity fake/explicit checksum 均为 `140547372586179969`，non-identity fake/explicit checksum 均为 `13376866430852120216`。production fake/explicit checksum 仍为 `10393124863019881355`，non-identity production fake/explicit checksum 仍为 `13376866430852120216`。上表和本段均来自 final distance predicate 接入后的板卡 rerun 日志。

增量诊断 `projection-pixel diagnostic` 的早期 QEMU 对拍显示 RVV 与 Std 的 correspondence 数量不一致。复核反汇编后确认标量路径存在 `fmadd.s`，原 RVV 诊断使用分离的 `vfmul + vfadd`，边界 lane 会在 RTZ 输入处产生差异。本轮把 RVV 投影改成 `z*cx` 后用 `vfmacc` 融合 `fx*x`，`u/v` 前移路径在 fake indices 和 `setIndices()` subset 下均与标量 correspondence 完全一致。该结果说明问题可以通过求值结构对齐解决；当前 production 已接入 projection-pixel staging、target gather、target finite、depth mask 和 final distance predicate，输出 append 与 stored distance 写出保留标量。

这次失败属于手工展开 Eigen / 标量表达式后的机器级求值结构差异。源码层同样是 `(fx*x + cx*z) / z`，但标量编译器会对 `a*b + c*d` 做 FMA contraction；RVV intrinsic 若写成两个乘法加一个加法，会多出中间舍入。`vfcvt.rtz.x.f.v` 只保证同一个输入值按向 0 截断，无法修正转换前已经不同的投影值。诊断流程按“QEMU correspondence 数量差异 -> 构造像素边界数据 -> 查标量与 RVV 反汇编 -> 识别 `fmadd.s` vs `vfmul + vfadd` -> 改成 `vfmacc` -> 回归 fake indices / subset / boundary case”推进。结论是该类问题不应直接判定为无法 RVV 化；先判断差异是否来自可控的 FMA contraction、舍入模式、运算重排或 FRM/FCSR，再决定通过 FMA 形态、显式舍入、边界 lane 标量回退或保留标量尾段解决。

## 增量诊断结果

这一步对应 `determineCorrespondencesProjectedIdentityCandidate` 和 `DiagMode::ProjectedIdentity`，它只在 exact identity transform 下把标量尾段中的投影截断和图像范围检查前移到 RVV，后面的 target 读取和输出仍保持标量，方便把差异归因到投影阶段：

```cpp
const vint32m2_t u =
    __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv0, z, vl), vl);
const vint32m2_t v =
    __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv1, z, vl), vl);
// keep = in-bounds mask
const vint32m2_t target_index_i =
    __riscv_vadd_vv_i32m2(__riscv_vmul_vx_i32m2(v, target.width, vl), u, vl);
```

测试结果将这条路径固定记录为与 Std 一致：

```cpp
diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
diag::determineCorrespondencesProjectedCandidate(*source, *target, indices, params, projected);

expectSameCorrespondences(scalar, projected);
```

QEMU bench 中新增的 `identity projection-pixel diagnostic` 显示 checksum 与 Std 一致：64K case 均为 `140547372586179969`；256K case 均为 `13594340667184250285`。QEMU 计时只作为日志完整性信息记录，不作为性能结论。反汇编确认该路径命中 `vfmacc.vf`、`vfdiv.vv`、`vfcvt.rtz.x.f.v`、`vluxei32.v` 和 `vcompress.vm`。该结果把 `u/v` 截断固定为已通过 correctness 的 production 阶段；target gather 和 final distance predicate 已在 target-predicate production 接入中完成验证。

`vfcvt.rtz.x.f.v` 能保证同一个有限、可表示的 `float` 输入按向 0 截断转成 `int32_t`，对应 C++ `static_cast<int>` 的截断方向。早期失败点在转换前的投影表达式。标量路径先算：

```cpp
const float uv0 = params.fx * candidate.x + params.cx * candidate.z;
const int u = static_cast<int>(uv0 / candidate.z);
```

修复后的 RVV 路径先算：

```cpp
vfloat32m2_t uv0 = __riscv_vfmul_vf_f32m2(z, params.cx, vl);
uv0 = __riscv_vfmacc_vf_f32m2(uv0, params.fx, x, vl);
const vint32m2_t u =
    __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv0, z, vl), vl);
```

当 `(fx*x + cx*z) / z` 靠近整数像素边界，例如 `319.99997` 或 `320.00003`，标量和 RVV 在乘加顺序、FMA contraction、除法舍入或临时值上出现 1 ulp 左右差异后，RTZ 结果会落到相邻像素。当前修复通过 `vfmacc` 对齐 contraction 后通过 QEMU 对拍。target-predicate production direct case 已补齐，板卡结果证明新增 staging 的维护成本与收益匹配。

## 生产接入前置复核

### full diagnostic 与上游对象状态

早期 full diagnostic 只通过 free function 显式传入 `indices` 和 `ProjectionParams`，便于把差异归因到 staging helper：

```cpp
std::vector<ProjectionCandidate> candidates;
if (projectCandidatesCandidate(input, target, indices, params, candidates)) {
  finishCorrespondencesFromCandidates(target, params, candidates, correspondences);
  return;
}
determineCorrespondencesStd(input, target, indices, params, correspondences);
```

当前已新增 test-only 派生诊断类，直接继承上游 CEOP，调用面与生产对象一致：

```cpp
diag::CorrespondenceEstimationOrganizedProjectionDiagnostic<
    pcl::PointXYZ, pcl::PointXYZ, float, diag::DiagMode::Candidate> ce;
ce.setInputSource(source);
ce.setInputTarget(target);
ce.setIndices(pcl::make_shared<pcl::Indices>(indices));  // 可选
ce.determineCorrespondences(correspondences, params.max_distance);
```

该入口内部先调用 `Base::initCompute()`，因此 target organized 检查、`CorrespondenceEstimationBase` target 状态、`PCLBase` fake indices 和 `setIndices()` subset 生命周期已经进入诊断验证。测试覆盖 `DiagnosticFakeIndicesMatchesScalar` 与 `DiagnosticSubsetIndicesMatchesScalar`。

上游生产入口仍在同一个循环里完成所有步骤：

```cpp
for (const auto& src_idx : (*indices_)) {
  if (isFinite((*input_)[src_idx])) {
    const Eigen::Vector4f p_src(src_to_tgt_transformation_ *
                                (*input_)[src_idx].getVector4fMap());
    const Eigen::Vector3f p_src3(p_src[0], p_src[1], p_src[2]);
    const Eigen::Vector3f uv(projection_matrix_ * p_src3);
    ...
  }
}
```

该入口证明 test-only diagnostic 可以覆盖上游对象状态。生产源码已经接入 source transform staging、projection-pixel staging 和 target-predicate final distance predicate；production fake/explicit、identity projection boundary、distance predicate boundary、non-identity fake/explicit case 的 QEMU checksum 均已验证。target gather、target finite、depth mask 和 final distance predicate 已进入 production，append 与 stored distance 写出保持标量。

### staging 改变计算结构

上游标量循环是逐点流式结构：

```text
source finite -> transform -> project -> target.at(u,v) -> depth/distance -> append
```

RVV 诊断最初把前置 transform 改成 source candidate staging；当前 production 已继续扩展到 projection-pixel staging 和 target-predicate staging：

```text
source gather -> transform -> finite/z mask -> vcompress -> ProjectionCandidate[]
ProjectionCandidate[] -> RVV u/v + bounds -> ProjectedCandidate[]
ProjectedCandidate[] -> RVV target gather + finite/depth/distance predicate -> AcceptedCandidate[]
AcceptedCandidate[] -> 标量 append
```

关键代码边界是 `projectCandidatesRVV` 中的 keep mask 和压缩写 staging：

```cpp
vbool16_t keep = finite(x) & finite(y) & finite(z) & (tz > 0.0f);
source_kept = vcompress(source_index, keep);
x_kept = vcompress(tx, keep);
y_kept = vcompress(ty, keep);
z_kept = vcompress(tz, keep);
```

该结构保持了保留 lane 的相对顺序，也把原来的流式计算改为“先写 staging、再读 staging”。本轮复核反汇编后，non-identity transform staging 已完成语义对齐：Eigen 4x4 `coeff(row)` 的机器级结构是两个部分和，`m1*y` 后用 `fmadd.s` 融合 `m0*x`，`m3*w` 后用 `fmadd.s` 融合 `m2*z`，最后 `fadd.s` 合并两个部分和。RVV helper 对应改成 `vfmul(y, m1) -> vfmacc(m0, x)`、`vfmv(m3) -> vfmacc(m2, z)`、`vfadd` 合并。`NonIdentityTransformRvvStagingBoundaryMatchesScalar` 和 fake indices、subset、small z、`PointXYZI` 诊断说明这组 adversarial 数据已经恢复 full correspondence 一致。production helper 已采用同一 FMA 结构，production non-identity fake/subset/small-z 测试和 QEMU checksum case 均通过。尾段继续标量执行的代码与上游 `target_->at(u,v)`、`pt_tgt.z` 和 `getVector3fMap()` 保持同一语义。

## 生产接入判断

当前结论：target-predicate 值得接入 production，并已按最小侵入方案接入。判断依据是 production-shaped 诊断已经覆盖真实 `initCompute()`、fake indices、`setIndices()` subset 和常见 `float x/y/z` 点类型；projection-pixel 的旧边界失败已通过 `vfmacc` 对齐标量 contraction 解决；target-predicate 诊断已覆盖 target gather、target finite、depth/distance mask 和第二次 `vcompress` 保序；板卡显示 production fake/explicit 为 `1.64x` / `1.65x`，non-identity production fake/explicit 均为 `2.36x`。production helper 的 distance predicate 使用与当前 Eigen `Vector3f::norm()` lowering 对齐的 `fmul/fmacc/fmacc/fsqrt.f32` 结构，并通过 `<` / `<=` 分支精确保留 `double(float_norm) < max_distance`。CEOP 输出是确定的 correspondence 序列，不是 RANSAC 内点统计；阈值边界 lane 改变会改变输出数量、后续元素位置和 checksum。

distance RVV 化已经完成当前目标构建下的最终 predicate 检验。`const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();` 中的差值向量是 `Eigen::Vector3f`，`norm()` 主要计算先在 float 域完成，赋给 `double` 是结果拓宽。float 到 double 的拓宽符合标量语义；真正需要保留的是 `double(float_norm) < max_distance` 这个谓词，而不是简单的 `float_norm < float(max_distance)`。本轮新增 bit-level 诊断证明 RVV `vfmul + vfmacc + vfmacc + vfsqrt.f32` 与 Eigen 标量 `Vector3f::norm()` 的 float 结果一致，并新增阈值诊断证明 `<` / `<=` 分支与标量 double predicate 一致。accepted lane 写出 `pcl::Correspondence::distance` 前仍标量重算 Eigen `norm()`，因此输出结构体中的 distance 值继续由 production 标量表达式决定。

最小生产方案已经按下面边界实施：

1. RVV helper 放在 `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` 的 `pcl::registration::detail` 中，只在 `#if defined(__RVV10__)` 下编译。
2. traits gate 只接受 source / target 具备单个 `float x/y/z` 字段的点类型；`Scalar` 只接受 `float`；identity transform 直接 staging 原始 source xyz，non-identity transform 使用 Eigen-aligned FMA staging；尺寸、32-bit byte offset、小规模和 `vlmax` 条件失败时回退标量。
3. 原标量循环抽为 `determineCorrespondencesOrganizedProjectionStd`，作为非 RVV 构建、未覆盖模板、小规模和 fallback 的标量路径。
4. 标量 tail 抽为 `finishOrganizedProjectionCorrespondences`、`finishOrganizedProjectionCorrespondencesFromProjected` 和 `finishOrganizedProjectionCorrespondencesFromAccepted`，分别覆盖 pixel staging 失败后的标量投影尾段、target-predicate helper 失败后的 projected 标量尾段，以及 accepted staging 成功后的 append-only 尾段。
5. production path 接入 `u/v` 投影 RVV 化、in-bounds staging、target gather、target finite、depth mask 和最终 distance predicate；correspondence append 与 stored distance 写出仍为标量 tail。

生产接入后的验证状态：

- `run_test_compare`：std 构建运行 39 个测试，其中 10 个 RVV-only 诊断 skip；RVV 构建 39 个测试全部通过；
- `run_bench_compare`：identity production fake/explicit checksum 均为 `10393124863019881355`；non-identity production fake/explicit checksum 均为 `13376866430852120216`；
- QEMU 计时不作为性能结论。
- `board_smoke`：板卡 39 个专项测试通过；target-predicate staging 64K/256K 为 `1.68x` / `1.69x`，identity target-predicate diagnostic 64K/256K 为 `1.60x` / `1.62x`；上游式 accepted prototype fake/explicit 为 `1.63x` / `1.62x`，non-identity fake/explicit 均为 `2.12x`；production fake/explicit checksum 均为 `10393124863019881355`，speedup 为 `1.64x` / `1.65x`；non-identity production fake/explicit checksum 均为 `13376866430852120216`，speedup 均为 `2.36x`。
