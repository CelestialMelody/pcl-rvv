# registration/correspondence_estimation_organized_projection RVV 诊断说明

## Closeout 摘要

当前 CEOP production RVV 已覆盖 source gather / finite / transform staging、projection-pixel staging 和 target-predicate final predicate。真实生产入口在 RVV gate 命中时依次执行 `projectOrganizedProjectionCandidatesRVV()`、`projectOrganizedProjectionPixelsRVV()`、`acceptProjectedOrganizedProjectionCandidatesRVV()`，最后只保留 `pcl::Correspondence` append 和 stored distance 写出为标量。保留标量 append 是刻意边界：它避免可变数量结构体 scatter、输出顺序和 `Correspondence::distance` bit pattern 风险。

当前 closeout 证据完整：QEMU `run_test_compare` 中 std 构建 39 个测试运行、10 个 RVV-only 诊断按预期 skip，RVV 构建 39 个测试全部通过；QEMU bench checksum 对齐；板卡 `board_smoke` 39 个专项测试通过，production identity fake/explicit 为 `1.64x` / `1.65x`，production non-identity fake/explicit 均为 `2.36x`。因此本主题可以按 production-ready 处理，不建议继续推进 `pcl::Correspondence` append RVV 化作为默认下一步。

## 1. 函数入口作用

`pcl::registration::CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar>::determineCorrespondences` 用于在 registration 管线中生成 source 到 target 的 correspondence。它先把 source 点通过 `src_to_tgt_transformation_` 变换到 target 相机坐标系，再用 `fx/fy/cx/cy` 投影到 organized target cloud 的 `(u, v)` dexel。命中图像范围后，入口读取 `target_->at(u, v)`，按 depth threshold 和欧氏距离阈值过滤，最后按 source 扫描顺序写出 `pcl::Correspondence(index_query, index_match, distance)`。

该类的价值是避免每点 KdTree search。输入是 source cloud、organized target cloud、可选 source indices、相机内参、source-to-target 变换、depth threshold 和 `max_distance`；输出是有序 correspondence 向量。`determineReciprocalCorrespondences` 只是调用本入口。

## 2. 标量路径与诊断边界

生产源码位于 `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp`。核心流程是：

```cpp
for (const auto& src_idx : (*indices_)) {
  if (isFinite((*input_)[src_idx])) {
    p_src = src_to_tgt_transformation_ * (*input_)[src_idx].getVector4fMap();
    uv = projection_matrix_ * p_src3;
    if (uv[2] <= 0) continue;
    u = static_cast<int>(uv[0] / uv[2]);
    v = static_cast<int>(uv[1] / uv[2]);
    if (u/v inside target) {
      pt_tgt = target_->at(u, v);
      if (finite, depth ok, distance ok) append correspondence;
    }
  }
}
```

当前 production RVV 与标量片段的对应关系如下：

| 标量片段 | 当前 production RVV 状态 |
| --- | --- |
| `for (const auto& src_idx : (*indices_))` | RVV 按 VL chunk 处理 `indices`，每个阶段用 `vcompress` 保持标量扫描顺序。 |
| `isFinite((*input_)[src_idx])` | RVV source finite mask。 |
| `src_to_tgt_transformation_ * (*input_)[src_idx].getVector4fMap()` | RVV source transform staging；identity fast path 直接使用原始 source `x/y/z`，non-identity path 按 Eigen lowering 对齐 FMA 结构。 |
| `Eigen::Vector3f p_src3(p_src[0], p_src[1], p_src[2])` | `OrganizedProjectionCandidate` 保存 transform 后 `x/y/z`。 |
| `projection_matrix_ * p_src3` | RVV projection-pixel staging 使用固定投影矩阵形态计算 `uv0 = fx*x + cx*z`、`uv1 = fy*y + cy*z`；`uv2` 等价为 transform 后 `z`。 |
| `if (uv[2] <= 0) continue` | RVV source staging 使用 keep 条件 `transformed z > 0`。标量代码写的是拒绝条件，RVV mask 写的是保留条件。 |
| `u = static_cast<int>(uv[0] / uv[2])`、`v = static_cast<int>(uv[1] / uv[2])` | RVV 使用 `vfmacc`、`vfdiv.vv`、`vfcvt.rtz.x.f.v`。 |
| image bounds check | RVV in-bounds mask。 |
| `target.at(u, v)` | RVV 根据 `target_index = v * width + u` indexed gather target `x/y/z`。 |
| `isFinite(pt_tgt)` | RVV target finite mask。 |
| `std::abs(uv[2] - pt_tgt.z) > depth_threshold_` | RVV depth mask，使用 transform 后 `z` 与 target `z`。 |
| `(p_src3 - pt_tgt.getVector3fMap()).norm()` | RVV final distance predicate 使用 `vfmul + vfmacc + vfmacc + vfsqrt.f32`，与 Eigen `Vector3f::norm()` 的 float lowering 对齐。 |
| `dist < max_distance` | RVV predicate 保留 `double(float_norm) < max_distance` 语义，通过 `<` / `<=` 分支处理 float threshold 边界。 |
| `pcl::Correspondence(...)` append | 仍为标量；accepted lane 写出前重算 Eigen `norm()`，保持 stored distance bit pattern。 |

`organizedProjectionTransformIsIdentity()` 是 source transform staging 的运行时分支 helper。它判断的就是 `determineCorrespondences()` 传入 `projectOrganizedProjectionCandidatesRVV()` 的 `src_to_tgt_transformation_` 是否为精确 identity matrix。保留该 helper 的原因是：identity transform 在标量语义上等价于直接使用原始 source `x/y/z`，不需要执行 4x4 row-dot；non-identity transform 才进入按 Eigen lowering 对齐的 FMA staging。这个分支让 identity 常见路径少做乘加，也避免把原始坐标经过一遍无意义的 `1*x + 0*y + 0*z + 0` 求值后引入边界 lane 差异。

`uv[2] <= 0` 对应 RVV `transformed z > 0` 的原因是投影矩阵在构造函数中初始化为 `Eigen::Matrix3f::Identity()`，`initCompute()` 只更新 `(0,0)=fx_`、`(1,1)=fy_`、`(0,2)=cx_`、`(1,2)=cy_`。第三行保持 `[0, 0, 1]`，因此：

```text
uv[0] = fx * x + cx * z
uv[1] = fy * y + cy * z
uv[2] = z
```

标量 `if (uv[2] <= 0) continue;` 表示剔除非正深度；RVV keep mask 使用 `(z > 0)` 表示保留正深度 lane。两者是同一个 predicate 的相反表达方式，且 RVV 在 source staging 阶段提前执行该过滤，后续 projection-pixel staging 可以直接用正 `z` 做除法和像素截断。

当前生产源码在同一 impl header 中新增三阶段 RVV helper、分阶段标量 fallback tail 和原循环 fallback helper；`test-rvv/registration/correspondence_estimation_organized_projection/` 保留 production-shaped diagnostic、低层 helper、专项测试和 bench，用于阶段归因与回归验证。

### 2.1 生产接入前置观察

#### 2.1.1 full diagnostic 接近真实流程，test-only 诊断类已接入上游对象状态

`test-rvv` 里的 production-shaped diagnostic 使用 test-only 派生诊断类，直接继承 `CorrespondenceEstimationOrganizedProjection`，并沿用同名入口 `determineCorrespondences(correspondences, max_distance)`。这条路径会真实走到上游 `initCompute()`、`CorrespondenceEstimationBase` 的 `fake indices` 生成逻辑，以及 `setIndices()` 传入的 subset 生命周期。

代码上，低层 free function 仍然先构造 staging，再交给尾段；高层诊断类则直接进入上游对象状态后，再调用同名入口：

```cpp
std::vector<ProjectionCandidate> candidates;
if (projectCandidatesCandidate(input, target, indices, params, candidates)) {
  finishCorrespondencesFromCandidates(target, params, candidates, correspondences);
  return;
}
determineCorrespondencesStd(input, target, indices, params, correspondences);
```

```cpp
class CorrespondenceEstimationOrganizedProjectionDiagnostic
: public pcl::registration::CorrespondenceEstimationOrganizedProjection<PointSource,
                                                                        PointTarget,
                                                                        Scalar> {
public:
  void determineCorrespondences(pcl::Correspondences& correspondences,
                                const double max_distance) override
  {
    if (!this->initCompute())
      return;
    ...
  }
};
```

生产入口当前按 source staging、projection-pixel staging、target-predicate staging 逐级尝试；任一后续 RVV helper 未命中时，从当前 staging 回到对应标量 tail：

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

这两种结构的差异会影响生产判断：

- 低层 helper 看到的是固定的 `indices` 向量和已展开参数；
- 高层 test-only 派生类已经把 `setInputSource`、`setInputTarget`、`setIndices`、fake indices 和对象生命周期纳入同一个入口；
- 诊断 bench 的收益可能包含 staging helper 的局部开销；本轮新增 `ceop production ...` bench case 直接覆盖上游公开 API。

#### 2.1.2 staging 改变了计算结构

标量入口对每个 source 点流式执行：

```text
transform -> project -> target -> depth/distance -> append
```

RVV staging 把前置几何步骤拆成两段。诊断 helper 覆盖 source 4x4 transform staging；生产 helper 已采用相同的 Eigen-aligned FMA transform staging，identity transform 下保留原始 source xyz 快路径：

```text
production: gather xyz -> identity fast path or 4x4 transform -> finite / z>0 mask -> vcompress -> 标量尾段
diagnostic: gather xyz -> 4x4 transform -> finite / z>0 mask -> vcompress -> 标量尾段
```

对应 helper 的边界更清楚：

```cpp
// RVV 只做 source 侧规整工作；u/v、target 读取和输出仍留在标量尾段。
vbool16_t keep = __riscv_vmand_mm_b16(
    __riscv_vmand_mm_b16(finiteMask(x, vl), finiteMask(y, vl), vl),
    finiteMask(z, vl),
    vl);
keep = __riscv_vmand_mm_b16(keep, __riscv_vmfgt_vf_f32m2_b16(tz, 0.0f, vl), vl);
```

这种 staging 先把保留下来的 lane 压缩到 candidate。生产路径的 candidate 保存 transform 后的 `x/y/z`；identity transform 下它与原始 source xyz 一致，non-identity transform 下它来自与 Eigen 标量 evaluator 对齐的 FMA 结构。

随后 production 继续尝试 projection-pixel staging 和 target-predicate staging。projection-pixel staging 把原标量尾段里的 `u/v` 截断和图像范围检查搬到 RVV 中；target-predicate staging 把 target gather、target finite、depth mask 和 final distance predicate 搬到 RVV 中。早期 projection 诊断使用分离的 `vfmul + vfadd` 计算 `(fx*x + cx*z)`，QEMU 对拍显示边界 lane 的 correspondence 数量不一致。复核反汇编后确认标量路径存在 `fmadd.s` contraction，本轮把 RVV 投影改成 `z*cx` 后用 `vfmacc` 融合 `fx*x`，fake indices 和 `setIndices()` subset 下的 projection-pixel 诊断恢复 full correspondence 一致。non-identity transform staging 诊断检查 `params.transform * getVector4fMap()` 这段 Eigen 标量求值结构。反汇编显示 Eigen 4x4 `coeff(row)` 使用两个部分和：`m1*y` 后用 `fmadd.s` 融合 `m0*x`，`m3*w` 后用 `fmadd.s` 融合 `m2*z`，再用 `fadd.s` 合并。test-only RVV transform staging 和 production helper 均按这个结构改成两组 `vfmacc.vf` 加一个 `vfadd.vv`，并通过 pixel-boundary、fake indices、`setIndices()` subset、small z 和 `PointXYZI` 组合诊断。accepted lane 写出 `pcl::Correspondence` 前仍用标量 Eigen `norm()` 重算 stored distance。

本轮生产优化可以命名为 `RVV source transform staging`。它对应原标量循环中的前置 source 处理段：

```cpp
for (const auto& src_idx : (*indices_)) {
  if (isFinite((*input_)[src_idx])) {
    const Eigen::Vector4f p_src(src_to_tgt_transformation_ *
                                (*input_)[src_idx].getVector4fMap());
    const Eigen::Vector3f p_src3(p_src[0], p_src[1], p_src[2]);
    const Eigen::Vector3f uv(projection_matrix_ * p_src3);

    if (uv[2] <= 0)
      continue;
```

其中 RVV 实际覆盖的是：

```text
src_idx 遍历 / gather input[src_idx].x/y/z
source finite check
identity fast path 或 Eigen-aligned 4x4 transform staging
z > 0 检查
vcompress 保序输出 source_index + x/y/z
```

`projection_matrix_ * p_src3` 在原标量代码中紧跟 transform。当前 production RVV 在 source staging 后接管投影截断和图像范围检查，生成 linear `target_index`；identity fast path 和 verified non-identity transform staging 都使用同一个 projection-pixel staging helper。随后 production RVV gather target `x/y/z` 并执行 target finite、depth mask 与 final distance predicate；append 和 stored distance 写出保持标量，以保持 `pcl::Correspondence::distance` 与 production 标量表达式一致。

## 3. 覆盖范围与 fallback

生产 RVV 覆盖：

- 通过 PCL traits 证明有 `x/y/z` 字段，且三个字段都是单个 `float` 的 source / target 点类型；
- 当前专项测试覆盖 `PointXYZ -> PointXYZ`、`PointXYZ -> PointXYZI`、`PointXYZI -> PointXYZI`；
- test-only 派生类和生产 direct class 测试覆盖公开调用形状 `determineCorrespondences(correspondences, max_distance)`；
- 上游 `initCompute()`、`CorrespondenceEstimationBase` fake indices 和 `setIndices()` subset 已纳入专项对拍与生产入口测试；
- `indices.size() >= 64`；
- `__riscv_vsetvlmax_e32m2() <= 64`；
- source / target 点数对应的 byte offset 能用 32-bit offset 表达；
- source `x/y/z` 通过 PCL `traits::offset<PointSource, fields::x/y/z>` 生成 byte offset，再用公共 `pcl::rvv_load::indexed_load3_fields_f32m2` gather。

`__riscv_vsetvlmax_e32m2() <= 64` 与当前 helper 使用的 RVV 向量变量类型绑定。CEOP production helper 使用 `vfloat32m2_t`、`vuint32m2_t`、`vint32m2_t`，也就是 `e32,m2`。在某个硬件 VLEN 下，`__riscv_vsetvlmax_e32m2()` 返回 `e32,m2` 一次最多能处理的 32-bit lane 数，近似为 `VLEN_bits * 2 / 32`。当前代码在 source 候选点生成、像素投影和 target 谓词筛选三个 helper 里都使用固定 `[64]` 栈 buffer 承接 `vcompress` 后的低 lane；最坏情况下所有 lane 都被保留，`keep_count` 可以等于 `vlmax_e32m2`。因此只有 `vlmax_e32m2 <= 64` 时，`vse32(..., keep_count)` 写入这些 buffer 才一定不会越界。

回退：

- 非 RVV 构建走 `determineCorrespondencesOrganizedProjectionStd`；
- 小规模 `indices.size() < 64`、更大 VLEN、非 `Scalar=float`、32-bit offset 边界失败走完整标量 fallback；
- 不满足 `float x/y/z` traits 条件的点类型走完整标量 fallback；
- source staging 成功但 `projectOrganizedProjectionPixelsRVV()` 失败时，`finishOrganizedProjectionCorrespondences()` 从 `OrganizedProjectionCandidate` 继续执行 projection、target predicate 和 append；
- projection-pixel staging 成功但 `acceptProjectedOrganizedProjectionCandidatesRVV()` 失败时，`finishOrganizedProjectionCorrespondencesFromProjected()` 从 `ProjectedOrganizedProjectionCandidate` 继续执行 target predicate 和 append；
- 三个 RVV staging helper 都成功时，只剩 `finishOrganizedProjectionCorrespondencesFromAccepted()` 做 append-only 标量写出；accepted lane 的 stored distance 已按 production 标量 Eigen `norm()` 重算。

当前 QEMU / board production case 的主路径预期是三个 RVV staging helper 都命中，然后进入 `finishOrganizedProjectionCorrespondencesFromAccepted()`。前两个 tail 在这些 case 中通常不会执行；它们服务于 partial fallback，例如 source staging 压缩后候选数低于后续 helper 的 `n >= 64` gate、projection-pixel staging 压缩后候选数低于 target-predicate helper gate，或后续 helper 因 VLEN / offset / 布局边界失败。

## 4. 详细设计

本主题采用“前置 RVV staging + 后续标量状态机”。标量实现每个点流式完成全部工作，不需要中间结构；production RVV 批量处理 source 读取、4x4 transform 和前置 mask，并把保留下来的 lane 压缩为 candidate：

### 4.1 staging 对照表

| staging 名称 | 结构体 | 生成 helper / 代码位置 | 字段来源 | 下一段消费者 / tail | gate |
| --- | --- | --- | --- | --- | --- |
| 诊断 source transform staging | `ProjectionCandidate` | `projectCandidatesRVV()`，`test-rvv/registration/correspondence_estimation_organized_projection/correspondence_estimation_organized_projection_diag.hpp` | `indices` 中的 source index；source `x/y/z`；identity 或 4x4 transform 后的 `x/y/z` | `finishCorrespondencesFromCandidates()` 标量执行投影、target、depth/distance 和 append；也可交给诊断 `projectPixelsRVV()` | diagnostic / bench；RVV 构建用 intrinsic，std 构建用 `projectCandidatesStd()` |
| 诊断 projection-pixel staging | `ProjectedCandidate` | `projectPixelsRVV()`，同一 diag header | `ProjectionCandidate` 的 `source_index` 和 `x/y/z`；RVV 计算出的 `target_index = v * width + u` | `finishCorrespondencesFromProjected()` 标量读取 target、执行 target finite、depth/distance 和 append | 现有上游式入口 `DiagMode::ProjectedIdentity` 只测 exact identity；free helper 可用于 non-identity 诊断 |
| 诊断 target-predicate staging | `AcceptedCandidate` | `acceptProjectedCandidatesRVV()`，同一 diag header | `ProjectedCandidate` 的 `source_index`、`target_index` 和 source `x/y/z`；RVV gather 的 target `x/y/z`；通过 predicate 后按标量公式重算的 `distance` | `finishCorrespondencesFromAccepted()` 只负责顺序 append `pcl::Correspondence` | diagnostic / bench；`DiagMode::Accepted` 覆盖 production-shaped 入口；production 采用同类 distance predicate，并在 append 前标量重算 stored distance |
| production source transform staging | `OrganizedProjectionCandidate` | `projectOrganizedProjectionCandidatesRVV()`，`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | `*indices_` 中的 source index；`*input_` 的 source `x/y/z`；identity fast path 或 Eigen-aligned 4x4 transform 后的 `x/y/z` | 进入 `projectOrganizedProjectionPixelsRVV()`；该 helper 失败时进入 `finishOrganizedProjectionCorrespondences()` 标量 projection tail | production；`__RVV10__`、`Scalar=float`、traits、size、VLEN gate |
| production projection-pixel staging | `ProjectedOrganizedProjectionCandidate` | `projectOrganizedProjectionPixelsRVV()`，`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | `OrganizedProjectionCandidate` 的 `source_index` 和 `x/y/z`；RVV `vfmacc + vfdiv + vfcvt.rtz` 得到的 `u/v` 和 `target_index` | 进入 `acceptProjectedOrganizedProjectionCandidatesRVV()`；该 helper 失败时进入 `finishOrganizedProjectionCorrespondencesFromProjected()` 标量 target/predicate/append tail | production RVV traits / size / VLEN gate；source staging 成功后尝试 |
| production target-predicate staging | `AcceptedOrganizedProjectionCandidate` | `acceptProjectedOrganizedProjectionCandidatesRVV()`，`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` | `ProjectedOrganizedProjectionCandidate`；target `x/y/z` gather；target finite、depth mask 和 Eigen-aligned distance predicate 后保留的 `source_index`、`target_index`、标量重算后的 `distance` | `finishOrganizedProjectionCorrespondencesFromAccepted()` append | production RVV traits / size / VLEN gate；projected staging 成功后尝试 |

这张表明确每个 staging 的代码位置。`source transform staging` 表示“source 候选点生成阶段”：读取 source、做 finite 检查、完成 identity / 4x4 transform 和正深度过滤，并输出 `OrganizedProjectionCandidate`。`projection-pixel staging` 表示“像素投影阶段”：把 transform 后的 source 坐标投影到 organized target 像素，计算 `target_index`，并输出 `ProjectedOrganizedProjectionCandidate`。`target-predicate staging` 表示“target 谓词筛选阶段”：读取 target 点并完成 target finite、depth 和 distance predicate，输出 `AcceptedOrganizedProjectionCandidate`。production projection-pixel staging 对应 `projectOrganizedProjectionPixelsRVV()` 写出的 `ProjectedOrganizedProjectionCandidate`。它在 `determineCorrespondences()` 内紧跟 `projectOrganizedProjectionCandidatesRVV()` 调用：source 候选点生成成功后先尝试像素投影；像素投影 helper 失败时回到 `finishOrganizedProjectionCorrespondences()` 标量计算 `u/v`。

三个 production RVV helper 对应原标量循环的连续三段：

| production helper | 阶段含义 | 对应标量代码片段 | 成功后输出 |
| --- | --- | --- | --- |
| `projectOrganizedProjectionCandidatesRVV()` | source 候选点生成 | `for (src_idx)`、`isFinite(input[src_idx])`、`src_to_tgt_transformation_ * getVector4fMap()`、`p_src3`、`uv[2] > 0` | `OrganizedProjectionCandidate{source_index, x, y, z}` |
| `projectOrganizedProjectionPixelsRVV()` | 像素投影与图像范围过滤 | `projection_matrix_ * p_src3`、`u/v = static_cast<int>(uv/z)`、image bounds check、`target_index = v * width + u` | `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}` |
| `acceptProjectedOrganizedProjectionCandidatesRVV()` | target 读取与 predicate 过滤 | `target.at(u,v)`、`isFinite(pt_tgt)`、depth threshold、`norm()` distance predicate、`dist < max_distance` | `AcceptedOrganizedProjectionCandidate{source_index, target_index, distance}` |

`determineCorrespondences()` 中的三个 `if (helper(...))` 不是新的业务 predicate，而是 RVV 阶段是否成功命中的 gate：满足 traits、规模、VLEN 和 32-bit offset 边界时，该阶段用 RVV 生成下一段 staging；否则从当前 staging 回到对应标量 tail。

`projectOrganizedProjectionPixelsRVV()` 的职责是把原标量循环中紧跟 transform 的 projection-pixel 阶段前移到 RVV。它消费 `OrganizedProjectionCandidate{source_index, x, y, z}`，其中 `x/y/z` 已经是 target camera 坐标；helper 批量计算 `u/v`，执行图像范围 mask，并生成线性 `target_index = v * width + u`。通过 mask 的 lane 被压缩为 `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}`，供后续 target-predicate helper 或 projected scalar tail 消费。

该 helper 中的固定临时数组：

```cpp
alignas(16) std::uint32_t source_buf[64];
alignas(16) std::uint32_t target_buf[64];
alignas(16) float x_buf[64];
alignas(16) float y_buf[64];
alignas(16) float z_buf[64];
```

用于完成“RVV SoA 压缩结果 -> C++ AoS staging 结构”的桥接。RVV 计算和 `vcompress` 后，各字段仍分别位于不同 vector register：

```text
source_index[] / target_index[] / x[] / y[] / z[]
```

而 production staging 是结构体数组：

```cpp
ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}
```

因此当前实现先把每个字段分别 `vse32` 到临时 SoA buffer，再用短标量循环拼回 AoS staging。`vcompress` 对每个字段使用同一个 `keep` mask，`vcpop` 得到的 `keep_count` 同时作为所有字段 store 和组装循环的长度，从而保持字段之间的 lane 对应关系和标量扫描顺序。`[64]` 与入口处 `__riscv_vsetvlmax_e32m2() <= 64` 的 gate 配套；更大 VLEN 自动回退标量，避免固定栈 buffer 越界。`alignas(16)` 为临时 store 提供稳定对齐，语义安全边界仍由 `vlmax` gate 保证。

这个 gate 不是业务规模阈值，也不是固定要求输入只能处理 64 个点。输入仍按 `__riscv_vsetvl_e32m2(n - i)` strip-mining 分 chunk 处理。`64` 限制的是“单个 chunk 最多可能压缩出多少 lane 并写入栈上临时数组”。例如 `VLEN=1024 bit` 时，`e32,m2` 的最大 lane 数是 64，固定 buffer 正好覆盖；若未来目标是 `VLEN=2048 bit`，`e32,m2` 最大 lane 数会到 128，当前 `[64]` buffer 就不再安全，因此 helper 返回 `false` 走标量或 staged fallback。后续若要支持更大 VLEN，可改为动态 scratch、固定 VL 分块、降低 LMUL 或 SoA staging，但需要重新跑 correctness、反汇编和板卡收益验证。

Segment store 可以在字段同宽、同类型、结构体连续且 LMUL / field count 组合合法时，把多个 vector register 直接交错写成 AoS，因而可能省掉这些临时 buffer。当前 CEOP projected staging 不采用该方案：`ProjectedOrganizedProjectionCandidate` 包含 `uint32_t, uint32_t, float, float, float` 混合字段；在 `e32m2` 下 5-field segment store 也不符合常见 `LMUL * NFIELDS <= 8` 约束。若把 float bit pattern reinterpret 成整数再 segment store，需要额外证明对象布局、padding、别名和后续 scalar tail 读取语义。该写出段尚未成为热点，当前板卡 full case 已证明临时 buffer 方案收益成立。

这种“多字段 `vcompress` + 临时 SoA buffer + 标量 AoS staging 组装”是常见 SIMD 工程折中，适合 CEOP 这类可变数量、保序、多字段候选输出。它避免在 production path 中引入复杂结构体 scatter、segment-store 布局前提或改变后续 scalar tail 的输入布局；是否继续把 append / AoS 写出 RVV 化，需要等该段成为明确热点并补充结构体布局、scatter / segment store 地址、stored distance bit pattern 和板卡收益证据。通用规则已整理到 `doc-rvv/rvv/RVV Multi-Field Compress Staging.zh.md`。

| 实体                                                      | 类型                 | 输入输出                                      | 标量语义来源                                              | 作用                                  |
| --------------------------------------------------------- | -------------------- | --------------------------------------------- | --------------------------------------------------------- | ------------------------------------- |
| `ProjectionParams`                                      | 参数结构             | 变换矩阵、`fx/fy/cx/cy`、阈值               | 原对象状态和入口参数展开                                  | 保证 std/RVV 使用同一对象参数         |
| `ProjectionCandidate`                                   | 诊断 staging 结构    | `source_index`、变换后 `x/y/z`            | 标量中 `p_src3` 和 `src_idx`                          | 供后续标量投影、target 读取和输出使用 |
| `projectCandidatesRVV`                                  | RVV helper           | source cloud + indices -> candidates          | `isFinite(source)`、4x4 transform、`z>0`              | 批量化可规整前置片段                  |
| `finishCorrespondencesFromCandidates`                   | 标量尾段             | candidates -> correspondences                 | `static_cast<int>(uv/z)`、`target.at`、depth/distance | 保持边界截断和输出顺序                |
| `AcceptedCandidate`                                     | 诊断 staging 结构    | `source_index`、`target_index`、`distance` | projected staging 后通过 target finite、depth 和 distance predicate 的 lane | 供 append-only 标量 tail 使用 |
| `acceptProjectedCandidatesRVV`                          | RVV helper           | projected + target cloud -> accepted          | target gather、target finite、depth threshold、distance threshold | target-predicate 独立诊断 |
| `finishCorrespondencesFromAccepted`                     | 标量 append tail     | accepted -> correspondences                   | `pcl::Correspondence(index_query,index_match,distance)` 顺序写出 | 保持输出结构体写出边界 |
| `CorrespondenceEstimationOrganizedProjectionDiagnostic` | test-only 派生入口   | 上游对象状态 -> correspondences               | 上游 `initCompute()`、`input_ / target_ / indices_`   | 用生产调用形状验证 candidate 和 projection-pixel 诊断 |
| `OrganizedProjectionCandidate`                          | 生产 staging 结构    | `source_index`、transform 后 `x/y/z` | 标量 `p_src3` 和 `src_idx`       | 上游生产入口 RVV staging              |
| `projectOrganizedProjectionCandidatesRVV`               | 生产 RVV helper      | `input_ / target_ / indices_` -> candidates | source finite、4x4 transform、`z>0`     | 生产 source transform staging 分流              |
| `organizedProjectionTransformIsIdentity`                | 生产分支 helper      | `src_to_tgt_transformation_` -> bool          | identity transform 下 `p_src3` 等于原始 source xyz      | 选择 identity fast path 或 non-identity FMA staging |
| `finishOrganizedProjectionCorrespondences`              | 生产标量 tail        | candidates -> correspondences                 | 原循环投影、target、depth/distance、append                | 生产 RVV path 的标量尾段              |
| `determineCorrespondencesOrganizedProjectionStd`        | 生产 fallback helper | 上游对象状态展开参数 -> correspondences       | 原 `determineCorrespondences` 循环                      | 非 RVV 与 fallback 权威路径           |

早期诊断曾把 `u/v` 截断和图像范围检查放在 RVV 阶段。QEMU 对拍显示 correspondence 数量不一致，原因是投影表达式中的 FMA contraction 与普通乘加顺序在像素边界附近会改变 `static_cast<int>` 的输入。本轮把 RVV projection-pixel 诊断改为 `vfmacc` 后，`ProjectionPixelRvvDiagnosticMatchesScalar`、`IdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar` 和 `IdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar` 均通过。non-identity transform staging 已按 Eigen 4x4 evaluator 的两个 FMA 部分和结构重写，`NonIdentityTransformRvvStagingBoundaryMatchesScalar` 及其 fake indices、subset、small z、`PointXYZI` 扩展测试均通过。production helper 已采用同一 FMA 结构，并新增 production non-identity direct class 测试。

当前生产 RVV 覆盖范围分成 source transform staging、projection-pixel staging 和 target-predicate staging：

- RVV 已覆盖 source 侧规整工作：按 `indices` gather `input[src_idx].x/y/z`，执行 identity fast path 或 Eigen-aligned 4x4 transform，构造 source finite 与 transformed `z > 0` mask，并用 `vcompress` 保序写出 `ProjectionCandidate{source_index, x, y, z}`。
- projection-pixel staging 已进入 production：`u = static_cast<int>((fx*x + cx*z) / z)`、`v = static_cast<int>((fy*y + cy*z) / z)` 使用 `vfmacc`、`vfdiv.vv` 和 `vfcvt.rtz.x.f.v` 批量执行，并通过 in-bounds mask 生成 `target_index`。该阶段消费 source staging 后的 `x/y/z`，覆盖 identity fast path 和 verified non-identity transform staging。
- target-predicate staging 已进入 production：`target.at(u, v)` 对应的 target `x/y/z` 不连续读取由 RVV indexed gather 完成，随后执行 target finite、depth mask 和 Eigen-aligned distance predicate。
- 输出写入保留在标量尾段：`correspondences[c_index++] = ...` 是可变数量、保序、结构体写出。当前实现由 `vcompress` 后的 accepted staging 驱动标量 append，并在写出前标量重算 stored distance，避免引入结构体 scatter 语义风险。

target-predicate diagnostic 从 `ProjectedCandidate` staging 后开始：RVV gather `target.points[target_index].x/y/z`，批量执行 target finite、depth threshold 和 distance threshold mask，再用 `vcompress` 保序输出 `AcceptedCandidate{source_index, target_index, distance}`。production 对应 helper 使用同一 target gather、finite、depth mask 和 distance predicate；最后的 `pcl::Correspondence` append 仍由标量 `finishCorrespondencesFromAccepted()` / `finishOrganizedProjectionCorrespondencesFromAccepted()` 完成，stored distance 在 accepted lane 上按 Eigen `norm()` 重算。

代码上，这个边界对应两个 helper 的分工。`projectCandidatesRVV` 只生成 candidate staging：

```cpp
const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
const vuint32m2_t offsets =
    __riscv_vmul_vx_u32m2(v_indices_u, sizeof(PointSource), vl);
pcl::rvv_load::indexed_load3_fields_f32m2<
    typename pcl::traits::POD<PointSource>::type, kXOff, kYOff, kZOff>(
    base, offsets, vl, x, y, z);

// Non-identity uses the Eigen-aligned two-part FMA row-dot form.
vfloat32m2_t tx_lo = __riscv_vfmul_vf_f32m2(y, transform(0, 1), vl);
tx_lo = __riscv_vfmacc_vf_f32m2(tx_lo, transform(0, 0), x, vl);
vfloat32m2_t tx_hi = __riscv_vfmv_v_f_f32m2(transform(0, 3), vl);
tx_hi = __riscv_vfmacc_vf_f32m2(tx_hi, transform(0, 2), z, vl);
vfloat32m2_t tx = __riscv_vfadd_vv_f32m2(tx_hi, tx_lo, vl);
// ty/tz use the same structure.
vbool16_t keep = finite(x) & finite(y) & finite(z) & (z > 0.0f);

const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(v_indices_u, keep, vl);
const vfloat32m2_t x_kept = __riscv_vcompress_vm_f32m2(tx, keep, vl);
const vfloat32m2_t y_kept = __riscv_vcompress_vm_f32m2(ty, keep, vl);
const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(tz, keep, vl);

for (std::size_t lane = 0; lane < keep_count; ++lane)
  out[kept + lane] =
      ProjectionCandidate{source_buf[lane], x_buf[lane], y_buf[lane], z_buf[lane]};
```

`finishCorrespondencesFromCandidates` 消费 staging 并保持标量投影、target 间接访问和输出 append：

```cpp
for (const auto& candidate : candidates) {
  const float uv0 = params.fx * candidate.x + params.cx * candidate.z;
  const float uv1 = params.fy * candidate.y + params.cy * candidate.z;
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

  const float dx = candidate.x - pt_tgt.x;
  const float dy = candidate.y - pt_tgt.y;
  const float dz = candidate.z - pt_tgt.z;
  const double dist = std::sqrt(static_cast<double>(dx) * dx +
                                static_cast<double>(dy) * dy +
                                static_cast<double>(dz) * dz);
  if (dist < params.max_distance)
    correspondences[c_index++] = pcl::Correspondence(
        static_cast<int>(candidate.source_index), v * target.width + u,
        static_cast<float>(dist));
}
```

为了让诊断入口和生产入口更容易对应，当前 test-rvv 还提供了 `CorrespondenceEstimationOrganizedProjectionDiagnostic<PointSource, PointTarget, Scalar, DiagMode>`。它继承上游 CEOP 类，外部调用仍是：

```cpp
ce.setInputSource(source);
ce.setInputTarget(target);
ce.setIndices(indices_ptr);  // 可选；不设置时由 PCLBase::initCompute() 生成 fake indices
ce.determineCorrespondences(correspondences, max_distance);
```

该类的 `determineCorrespondences()` 先调用上游 `Base::initCompute()`，再从 `this->input_`、`this->target_`、`this->indices_`、`this->fx_`、`this->src_to_tgt_transformation_` 等成员展开参数。`DiagMode::Std` 调用标量公式，`DiagMode::Candidate` 只替换 source staging。这样可以把“公开对象状态是否一致”和“RVV staging 是否一致”放进同一个入口对拍。

生产入口采用相同阶段边界，并把原循环抽成 fallback helper。第一版生产代码没有新增类成员，也没有改变公开 API；所有新增实体都在 impl header 的 `pcl::registration::detail` 中。

这些 `detail` helper 是 free helper，内部没有 `this`，也不能直接访问 `input_`、`target_`、`indices_`、`projection_matrix_`、`depth_threshold_` 等类成员。因此生产入口会把 helper 实际需要的对象状态显式传入。这个拆分只改变源码组织方式；`determineCorrespondencesOrganizedProjectionStd` 保存原标量循环语义，作为非 RVV 和 fallback 的权威实现。

`finishOrganizedProjectionCorrespondences` 和 `finishOrganizedProjectionCorrespondencesFromProjected` 保留为分阶段 fallback tail。当前 production RVV 是逐级扩大覆盖面的结构：

```text
source 候选点生成
  -> 像素投影
    -> target 谓词筛选
      -> correspondence append

对应 helper：
projectOrganizedProjectionCandidatesRVV()
  -> projectOrganizedProjectionPixelsRVV()
    -> acceptProjectedOrganizedProjectionCandidatesRVV()
      -> finishOrganizedProjectionCorrespondencesFromAccepted()
```

`finishOrganizedProjectionCorrespondences()` 只在 source staging 已成功、但 projection-pixel staging 没有命中时使用。它不再遍历 `indices_`，也不再读取 `input_[src_idx]`；它消费 `OrganizedProjectionCandidate{source_index, x, y, z}`，从 transform 后的 `x/y/z` 继续执行投影、target 读取、depth/distance 和 append。

`finishOrganizedProjectionCorrespondencesFromProjected()` 只在 projection-pixel staging 已成功、但 target-predicate staging 没有命中时使用。它消费 `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}`，因此不再计算 `u/v`，也不再做 image bounds check；它只按 `target_index` 读取 target，继续执行 target finite、depth/distance 和 append。

当 `acceptProjectedOrganizedProjectionCandidatesRVV()` 成功时，target gather、target finite、depth mask 和 final distance predicate 都已完成。此时不会调用上述两个 fallback tail，只调用 `finishOrganizedProjectionCorrespondencesFromAccepted()` 做 append-only 标量写出。std/fallback 路径仍由 `determineCorrespondencesOrganizedProjectionStd` 遍历 `indices`，与原实现保持同一流程。

在当前已验证的 production case 中，预期执行路径是三个 RVV helper 连续成功：

```text
projectOrganizedProjectionCandidatesRVV()
  -> projectOrganizedProjectionPixelsRVV()
    -> acceptProjectedOrganizedProjectionCandidatesRVV()
      -> finishOrganizedProjectionCorrespondencesFromAccepted()
```

因此 `finishOrganizedProjectionCorrespondences()` 和 `finishOrganizedProjectionCorrespondencesFromProjected()` 更接近“保险丝”而不是热路径。它们避免中间阶段失败时丢弃已完成的前置 RVV staging，也避免为了某个后续 helper 的 gate 失败而回到完整标量入口重新遍历 `indices_`。

### 4.1 泛型点类型诊断

上游类是 `CorrespondenceEstimationOrganizedProjection<PointSource, PointTarget, Scalar>` 模板。标量实现可以通过 `getVector4fMap()`、`target.at(u,v)`、`pt_tgt.z` 和 `getVector3fMap()` 让编译器按具体点类型展开。RVV staging 需要在进入 intrinsic 前明确 `x/y/z` 的字段类型、byte offset 和点 stride。

本轮在诊断层加入 traits-gated 泛型：`XYZFloatLayout<PointT>` 使用 PCL 注册字段判断 `PointT` 是否有 `x/y/z`，并确认三个字段的 decomposed datatype 都是单个 `float`。满足条件的 `PointSource` 可以进入 RVV source transform staging；不满足条件时 helper 返回 `false`，外层回退完整标量路径。

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

新增 QEMU 对拍覆盖：

- `CandidateSupportsPointXYZITarget`：`PointXYZ` source + organized `PointXYZI` target；
- `CandidateSupportsPointXYZISourceAndTarget`：`PointXYZI` source + organized `PointXYZI` target。

结果：std 构建运行 39 个专项测试，其中 10 个 RVV-only 诊断按预期 skip；RVV 构建 39 个专项测试全部通过。`PointXYZI` 两个组合与标量 correspondence 完全一致；`DiagnosticFakeIndicesMatchesScalar` 和 `DiagnosticSubsetIndicesMatchesScalar` 覆盖继承上游类后的 fake indices 与显式 subset；production direct class 测试覆盖 identity fake/subset、identity projection boundary、distance predicate boundary、`PointXYZI`、small fallback、non-identity fake/subset、non-identity projection boundary、small-z `PointXYZI` 和小输入 fallback；`DistanceRvvMatchesEigenNormBits` 与 `DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` 覆盖 RVV distance bit pattern 和 double threshold predicate；`NonIdentityTransformRvvStagingBoundaryMatchesScalar`、`NonIdentityTransformDiagnosticFakeIndicesMatchesScalar`、`NonIdentityTransformDiagnosticSubsetIndicesMatchesScalar` 和 `NonIdentityTransformSmallZPointXYZIMatchesScalar` 覆盖对齐后的 test-only 4x4 transform staging；`NonIdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar` 和 `NonIdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar` 覆盖 non-identity transform staging 与 projection-pixel staging 的组合。该结果说明 traits-gated source transform staging、projection-pixel staging 和 target-predicate staging 可以覆盖常见 `float x/y/z` 布局，并已进入上游生产分流。target-predicate 诊断已补充 fake indices、subset、non-identity、tight threshold、`PointXYZI` 和 small fallback 覆盖；production 版使用 final distance predicate + 标量 stored distance 写出。

增量推进顺序已经完成到 target-predicate production：先证明 `u/v` 的 RVV 截断语义，再证明 `target.at(u,v)` 等价 gather 和 mask 组合，最后把 distance predicate 前移到 RVV。当前未 RVV 化的是最终 `pcl::Correspondence` append 和 stored distance 写出。

`projection-pixel` 把 `u/v` 截断和图像范围检查从标量尾段前移到 RVV。source transform staging 生成变换后的 `x/y/z`，pixel staging 再用这些 `x/y/z` 计算 `u/v` 和 `target_index`。test-rvv 中保留 `DiagMode::ProjectedIdentity` 和 `determineCorrespondencesProjectedIdentityCandidate` 作为独立诊断，production 中对应 `projectOrganizedProjectionPixelsRVV`。新增 production staging 为 `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}`：

```cpp
const vint32m2_t u =
    __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv0, z, vl), vl);
const vint32m2_t v =
    __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv1, z, vl), vl);

vbool16_t keep = __riscv_vmsge_vx_i32m2_b16(u, 0, vl);
keep = keep & (u < target.width) & (v >= 0) & (v < target.height);

const vint32m2_t target_index_i =
    __riscv_vadd_vv_i32m2(__riscv_vmul_vx_i32m2(v, target.width, vl), u, vl);

source_kept = vcompress(source_index, keep);
target_kept = vcompress(target_index_i, keep);
x_kept = vcompress(x, keep);
y_kept = vcompress(y, keep);
z_kept = vcompress(z, keep);
```

对应测试将该路径固定为语义一致诊断：

```cpp
diag::determineCorrespondencesStd(*source, *target, indices, params, scalar);
diag::determineCorrespondencesProjectedCandidate(*source, *target, indices, params, projected);

expectSameCorrespondences(scalar, projected);
```

QEMU 结果显示 `ProjectionPixelRvvDiagnosticMatchesScalar` 通过，含义是 RVV 构建下 `ProjectedCandidate` 路径与 Std correspondence 完全一致。反汇编确认该诊断路径命中 `vfmacc.vf`、`vfdiv.vv`、`vfcvt.rtz.x.f.v`、`vluxei32.v` 和 `vcompress.vm`。早期失败原因收敛到 RVV 投影截断 / 求值顺序与标量边界语义不一致；`vfmacc` 对齐后该 correctness 问题已解除。

target-predicate 诊断从 `ProjectedCandidate` 开始，把 target 读取和 predicate 前移到 RVV：

```cpp
pcl::rvv_load::indexed_load3_fields_f32m2<
    typename pcl::traits::POD<PointTarget>::type, kXOff, kYOff, kZOff>(
    target_base, target_offsets, vl, tx, ty, tz);

keep = finite(tx) & finite(ty) & finite(tz);
dz = sz - tz;
keep &= abs(dz) <= params.depth_threshold;
dist2 = dx * dx + dy * dy + dz * dz;
dist = sqrt(dist2);
keep &= dist < params.max_distance;

source_kept = vcompress(source, keep);
target_kept = vcompress(target_index, keep);
```

diagnostic accepted staging 的输出距离按压缩后的 lane 使用标量 `double sqrt` 公式重算。这保留了最终 `pcl::Correspondence::distance` 的 checksum 语义；test-rvv prototype 中 RVV `vfsqrt.v` 用于 distance threshold mask。production 版 `acceptProjectedOrganizedProjectionCandidatesRVV()` 使用同样的 `vfsqrt.v` 路径执行最终 distance predicate，并在 accepted lane 写出前用 production 标量 Eigen `norm()` 重算 stored distance。`finishCorrespondencesFromAccepted()` 只按 `AcceptedCandidate` 顺序写出 `pcl::Correspondence`，所以第二次 `vcompress` 后的输出顺序与标量逐点 append 顺序一致。

这里的精确边界来自 CEOP 的输出语义。sample_consensus 的 RANSAC 类路径可以把阈值附近少量内点差异当作浮点舍入容差处理；CEOP 输出的是确定的 correspondence 序列。任意一个 distance predicate 在阈值边界改变保留/丢弃，会改变 `correspondences` 的数量、后续元素位置和 checksum。因此 production 只有在证明求值结构与阈值谓词一致后才能让 RVV distance mask 成为最终裁决。

本轮已完成当前目标构建下的 distance final predicate 检验。标量代码是：

```cpp
const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();
if (dist < max_distance) append;
```

其中 `(p_src3 - pt_tgt.getVector3fMap())` 是 `Eigen::Vector3f`，`norm()` 的核心计算先发生在 float 域，赋给 `double` 是结果拓宽。float 到 double 的拓宽符合标量路径；真正需要保留的是 `double(float_norm) < max_distance`，不能简化成 `float_norm < float(max_distance)`。当前反汇编显示 production scalar tail 使用 `fmul.s + fmadd.s + fmadd.s + fsqrt.s + fcvt.d.s + flt.d`。production RVV 按同一 float32 结构写为 `vfmul.vv + vfmacc.vv + vfmacc.vv + vfsqrt.v`；当 `static_cast<double>(float(max_distance)) < max_distance` 时使用 `vmfle.vf`，否则使用 `vmflt.vf`，从而等价于标量 double predicate。`DistanceRvvMatchesEigenNormBits` 证明 RVV distance 与 Eigen `Vector3f::norm()` 的 float bit pattern 一致；`DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` 证明 `<` / `<=` 选择覆盖 double threshold 边界。

#### 4.1.1 FMA contraction 语义对齐诊断

这次问题属于把依赖 Eigen 小矩阵 / 标量表达式求值的标量算法转换为手工 RVV 矢量化实现后，机器级 instruction ordering 改变导致的语义差异。源码层看起来同样是在计算：

```cpp
uv0 = fx * x + cx * z;
u = static_cast<int>(uv0 / z);
```

浮点加法和乘法在 IEEE-754 下按每条指令舍入到目标格式，`(a*b) + (c*d)` 的结果不满足实数代数中的结合律和分配律。`vfmul + vfadd` 会先把两个乘积分别舍入为 `float`，再对加法结果舍入；FMA / fused multiply-add 在一条指令中计算乘加，并只在最终结果处舍入一次。因此，即使源码公式相同，`mul; mul; add` 与 `fmadd` 的最低位也可能不同。标量编译器可以把 `a*b + c*d` 的一部分收缩为 FMA。早期 RVV diagnostic 手写成两个乘法和一次加法：

```cpp
uv0 = __riscv_vfmul_vf_f32m2(x, params.fx, vl);
uv0 = __riscv_vfadd_vv_f32m2(
    uv0, __riscv_vfmul_vf_f32m2(z, params.cx, vl), vl);
```

QEMU 对拍中，接近像素整数边界的 lane 出现 correspondence 数量差异。进一步查看 `dump_bench_rvv` 生成的反汇编后，标量路径中可见 `fmadd.s` 形态，说明标量送入 `static_cast<int>` 的值来自不同的乘加收缩结构。排查同时确认 `vfcvt.rtz.x.f.v` 截断方向和 FRM/FCSR 状态没有解释该现象；根因在转换前的投影表达式输入值。该输入值一旦跨过整数边界，向 0 截断会选择相邻像素，后续 `target.at(u,v)`、depth/distance 和 append 都沿着不同路径执行。

修复时把 RVV 表达式改成一项先乘入累加器，另一项使用 `vfmacc` 融合：

```cpp
uv0 = __riscv_vfmul_vf_f32m2(z, params.cx, vl);
uv0 = __riscv_vfmacc_vf_f32m2(uv0, params.fx, x, vl);
```

这与反汇编显示的标量 contraction 更接近。修复后 fake indices、`setIndices()` subset 和 projection boundary case 的 full correspondence 对拍恢复一致，反汇编也确认 `vfmacc.vf` 与 `vfcvt.rtz.x.f.v` 同时存在。

这个案例说明：当 RVV 手工展开 Eigen 矩阵乘法、投影、多项式、距离公式或其它浮点表达式时，需要把源码公式、标量反汇编和 RVV intrinsic 序列一起检查。若差异来自可控的乘加收缩、舍入模式或运算重排，应优先尝试用 FMA 形态、显式舍入 intrinsic、局部边界回退或保持标量尾段来解决。只有在无法稳定对齐所有编译器 / 构建选项下的求值结构，或者边界回退成本与复杂度过高时，才把该 RVV 阶段判为不能进入 production。

文档中的“不进入 production”判断需要按下面顺序给出证据：

1. 已有最小反例能稳定复现 std/RVV 输出差异，并能定位到具体表达式或状态更新。
2. 反汇编确认差异来自 instruction ordering、FMA contraction、舍入模式、临时精度、自动向量化或目标 ISA 行为之一。
3. 已尝试与标量对齐的可行手段，包括 fused intrinsic、调整累加初值和操作数顺序、显式舍入 intrinsic、固定 FRM/FCSR、局部标量求值或边界 lane 回退。
4. 若仍不一致，需要说明失败原因：例如 RVV 规约树形顺序无法复刻标量线性顺序，跨 lane 重排改变状态机可见顺序，编译器在不同构建下选择不同 contraction，或边界 lane 检测覆盖不了所有会改变输出的情况。
5. 若可通过边界回退解决，需要估算边界判定成本、回退比例、输出顺序维护成本和板卡收益；成本不可接受时，该阶段保留为 diagnostic / bench-only。
6. 若无法证明所有公开输入下 output index、数量、状态更新和 checksum 与标量一致，生产路径必须继续 fallback 到标量或停在已证明安全的前一阶段。

`vfcvt.rtz.x.f.v` 本身提供向 0 截断语义。它能保证同一个有限、可表示的 `float` 输入按 RTZ 转成 `int32_t`，语义对应 C++ `static_cast<int>` 的截断方向。本诊断失败点在完整投影表达式：`vfcvt.rtz` 的输入来自 RVV 计算出的 `(fx*x + cx*z) / z`，该值需要与标量路径中送入 `static_cast<int>` 的值逐 bit 一致。

标量尾段的投影计算是：

```cpp
const float uv0 = params.fx * candidate.x + params.cx * candidate.z;
const int u = static_cast<int>(uv0 / candidate.z);
```

修复后的 RVV 诊断路径使用与标量 contraction 更接近的 FMA 形态：

```cpp
vfloat32m2_t uv0 = __riscv_vfmul_vf_f32m2(z, params.cx, vl);
uv0 = __riscv_vfmacc_vf_f32m2(uv0, params.fx, x, vl);

const vint32m2_t u =
    __riscv_vfcvt_rtz_x_f_v_i32m2(__riscv_vfdiv_vv_f32m2(uv0, z, vl), vl);
```

当投影值远离整数像素边界时，1 ulp 量级的差异通常不会改变 `u/v`。当投影值靠近边界，例如 `319.99997` 或 `320.00003`，标量和 RVV 在乘加顺序、FMA contraction、除法舍入或中间临时值上出现极小差异后，RTZ 结果可能从 `319` 变成 `320`，或从 `320` 变成 `319`。随后的图像范围检查、`target_index = v * width + u`、target depth/distance 检查都会沿着不同像素继续执行，最终表现为 correspondence 数量或 checksum 不一致。

因此，`u/v` 投影 RVV 化已经具备 production correctness 证据，并已覆盖 identity fast path 与 verified non-identity transform staging。后续若继续扩展，应补更密集的接近整数边界、负投影坐标和小 `z` 专项数据；当前 target gather、target finite、depth mask 和 final distance predicate 已进入 production，并通过板卡 full case 证明收益。

QEMU bench 只记录 checksum 和日志格式，时间不参与性能判断：

| case                                               | Std ms/iter | RVV ms/iter | speedup | 结论                               |
| -------------------------------------------------- | ----------: | ----------: | ------: | ---------------------------------- |
| `ceop identity projection-pixel diagnostic pointxyz 64K`  |     19.8425 |     44.7004 | QEMU 计时不判性能 | checksum 一致 |
| `ceop identity projection-pixel diagnostic pointxyz 256K` |     78.3453 |    178.1389 | QEMU 计时不判性能 | checksum 一致 |

因此 projection-pixel 增量诊断结论已经从语义失败推进到 correctness 通过，并成为 production 前段。target 间接读取、target finite、depth mask 和 final distance predicate 也已作为第三阶段 production helper 接入；剩余标量边界是 `pcl::Correspondence` append 和 stored distance 写出。

### 4.2 核心片段

```cpp
const vint32m2_t v_indices = __riscv_vle32_v_i32m2(indices.data() + i, vl);
const vuint32m2_t offsets =
    __riscv_vmul_vx_u32m2(v_indices_u, sizeof(PointSource), vl);
pcl::rvv_load::indexed_load3_fields_f32m2<
    typename pcl::traits::POD<PointSource>::type, kXOff, kYOff, kZOff>(
    base, offsets, vl, x, y, z);

// Identity and verified non-identity production both continue with
// projection-pixel staging.
tx = x;
ty = y;
tz = z;
keep = finite(x) & finite(y) & finite(z) & (z > 0);

source_kept = vcompress(source_index, keep);
x_kept = vcompress(tx, keep);
y_kept = vcompress(ty, keep);
z_kept = vcompress(tz, keep);
```

`vcompress` 只移除 scalar 也会跳过的 invalid / behind-camera lane，不改变保留 lane 的相对顺序。identity 路径继续批量计算投影像素：

```cpp
uv0 = z * cx;
uv0 = fma(fx, x, uv0);
u = rtz(uv0 / z);
target_index = v * width + u;
projected = vcompress({source_index, target_index, x, y, z}, in_bounds);
```

后续 target-predicate production helper 按 projected staging 顺序 gather target 并完成：

```cpp
pt_tgt = target.points[target_index];
target finite -> depth/distance predicate -> accepted staging
```

因此 correspondence 的 `index_query` 顺序和 scalar 扫描顺序一致。

FRM/FCSR：最终 helper 不使用 `_rm` intrinsic，不读写 FRM/FCSR；identity production 的 `u/v` float-to-int 使用 `vfcvt.rtz.x.f.v` 明确向 0 截断。

## 5. 数值算例与 VL chunk 图示

设一个 VL chunk 中有 4 个 source 点，`fx=300`、`fy=310`、`cx=160`、`cy=120`，变换为 identity：

| lane | source index | source `(x,y,z)`     | RVV keep | 标量尾段投影                                 |
| ---: | -----------: | ---------------------- | -------- | -------------------------------------------- |
|    0 |           10 | `(0.20, 0.10, 2.0)`  | keep     | `u=int((300*0.20+160*2)/2)=190`，`v=135` |
|    1 |           11 | `(NaN, 0.1, 2.0)`    | drop     | source 非 finite，标量也跳过                 |
|    2 |           12 | `(0.00, 0.00, -1.0)` | drop     | `z<=0`，标量也跳过                         |
|    3 |           13 | `(-0.30, 0.20, 3.0)` | keep     | `u=130`，`v=140`                         |

`vcompress` 后 staging 顺序为 source `10, 13`。标量尾段继续按这个顺序访问 `target(190,135)` 和 `target(130,140)`，再应用 depth / distance 阈值。

简图：

```text
source lanes:  [10] [11] [12] [13]
keep mask:      1    0    0    1
vcompress:     [10] [13]
tail scalar:   project -> target.at(u,v) -> append correspondence
```

## 6. Bench Case 说明

所有 bench 使用同一 synthetic `PointXYZ` source、同一 organized `PointXYZ` target、同一 `ProjectionParams`。speedup = Std 平均耗时 / RVV 平均耗时。

| case                                                           | 入口                                                                                                          | 数据 / 参数                                                            | 路径含义                                                           |
| -------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------- | ------------------------------------------------------------------ |
| `ceop projection-staging pointxyz 64K`                       | `projectCandidatesCandidate`                                                                                | 64K source，640x480 target，`indices=[0,n)`                          | 只测 source gather、4x4 transform、finite /`z>0` mask 和 staging |
| `ceop projection-staging pointxyz 256K`                      | 同上                                                                                                          | 256K source，同参数                                                    | 局部片段规模放大                                                   |
| `ceop identity projection-pixel diagnostic pointxyz 64K`              | `determineCorrespondencesProjectedIdentityCandidate`                                                                | 64K source，identity gate，staging + RVV `u/v` 截断 / 范围检查 + 标量 target / 输出 | 诊断 identity 条件下投影前移是否可继续 RVV 化 |
| `ceop identity projection-pixel diagnostic pointxyz 256K`             | 同上                                                                                                          | 256K source，同参数                                                    | 诊断增量在大规模下的语义和性能                                     |
| `ceop target-predicate staging pointxyz 64K` | `projectCandidatesCandidate` + `projectPixelsCandidate` + `acceptProjectedCandidatesCandidate` | 64K source，identity 参数 | 只测 target gather、target finite、depth/distance mask 和 accepted staging |
| `ceop target-predicate staging pointxyz 256K` | 同上 | 256K source | target-predicate staging 规模放大诊断 |
| `ceop identity target-predicate diagnostic pointxyz 64K` | `determineCorrespondencesAcceptedCandidate` | 64K source，append-only scalar tail | full diagnostic：RVV target predicates + 标量 append |
| `ceop identity target-predicate diagnostic pointxyz 256K` | 同上 | 256K source | target-predicate full diagnostic 规模放大 |
| `ceop full-correspondence pointxyz 64K`                      | `determineCorrespondencesCandidate`                                                                         | staging + 标量投影、target、depth/distance、输出                       | 接近真实入口的 full diagnostic                                     |
| `ceop full-correspondence pointxyz 256K`                     | 同上                                                                                                          | 256K source                                                            | 检查 staging 额外内存流量和标量尾段是否稀释收益                    |
| `ceop upstream-like std fake-indices pointxyz 64K`           | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Std>::determineCorrespondences`       | 不调用 `setIndices()`，由上游 `initCompute()` 生成 fake indices    | 公开调用形状下的标量对照                                           |
| `ceop upstream-like candidate fake-indices pointxyz 64K`     | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Candidate>::determineCorrespondences` | 同上                                                                   | 公开调用形状下的 RVV staging candidate                             |
| `ceop upstream-like accepted fake-indices pointxyz 64K`      | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Accepted>::determineCorrespondences`  | 同上                                                                   | 公开调用形状下的 target-predicate prototype                        |
| `ceop upstream-like std explicit-indices pointxyz 64K`       | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Std>::determineCorrespondences`       | 调用上游 `setIndices(IndicesPtr)`                                    | 显式 indices 生命周期对照                                          |
| `ceop upstream-like candidate explicit-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Candidate>::determineCorrespondences` | 同上                                                                   | 显式 indices 下的 RVV staging candidate                            |
| `ceop upstream-like accepted explicit-indices pointxyz 64K`  | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Accepted>::determineCorrespondences`  | 同上                                                                   | 显式 indices 下的 target-predicate prototype                       |
| `ceop upstream-like accepted non-identity fake-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Accepted>::determineCorrespondences` | non-identity transform，不调用 `setIndices()` | non-identity target-predicate prototype |
| `ceop upstream-like accepted non-identity explicit-indices pointxyz 64K` | `CorrespondenceEstimationOrganizedProjectionDiagnostic<..., DiagMode::Accepted>::determineCorrespondences` | non-identity transform，调用 `setIndices(IndicesPtr)` | 显式 indices 下的 non-identity target-predicate prototype |
| `ceop production fake-indices pointxyz 64K`                  | 上游 `CorrespondenceEstimationOrganizedProjection::determineCorrespondences`                                | 不调用 `setIndices()`                                                | 真实生产入口 fake indices 分流                                     |
| `ceop production explicit-indices pointxyz 64K`              | 上游 `CorrespondenceEstimationOrganizedProjection::determineCorrespondences`                                | 调用 `setIndices(IndicesPtr)`                                        | 真实生产入口显式 indices 分流                                      |

## 7. 测试、QEMU、反汇编和板卡证据

专项测试按审核目标分组如下。完整逐项说明见函数级评估文档。

| 测试组 | 覆盖测试 | 审核目的 |
| ------ | -------- | -------- |
| 低层 source staging | `CandidateMatchesScalar`、`CandidateSupportsPointXYZITarget`、`CandidateSupportsPointXYZISourceAndTarget`、`SmallInputFallbackMatchesScalar` | 验证 helper 与标量公式一致，覆盖 traits 和小输入 fallback。 |
| 上游式诊断入口 | `DiagnosticFakeIndicesMatchesScalar`、`DiagnosticSubsetIndicesMatchesScalar`、`PclClassMatchesScalarFormula` | 验证 test-only 派生类覆盖 `initCompute()`、fake indices、`setIndices()` 和标量 reference。 |
| identity production direct | `ProductionFakeIndicesMatchesScalar`、`ProductionSubsetIndicesMatchesScalar`、`ProductionSupportsPointXYZITarget`、`ProductionSupportsPointXYZISourceAndTarget`、`ProductionSmallInputFallbackMatchesScalar` | 直接调用真实 production 入口，覆盖 identity fast path、traits、subset 和 fallback。 |
| non-identity production direct | `ProductionNonIdentityTransformFakeIndicesMatchesScalar`、`ProductionNonIdentityTransformSubsetIndicesMatchesScalar`、`ProductionNonIdentityTransformSmallZPointXYZIMatchesScalar`、`ProductionNonIdentitySmallInputFallbackMatchesScalar` | 证明扩大后的 production gate 在 adversarial transform、subset、small-z traits 和小输入 fallback 下与标量一致。 |
| non-identity RVV-only 诊断 | `NonIdentityTransformRvvStagingBoundaryMatchesScalar`、`NonIdentityTransformDiagnosticFakeIndicesMatchesScalar`、`NonIdentityTransformDiagnosticSubsetIndicesMatchesScalar`、`NonIdentityTransformSmallZPointXYZIMatchesScalar` | 独立固定 Eigen-aligned FMA transform staging 语义，避免 production 测试成为唯一证据。 |
| 标量 tail 与 projection-pixel | `TightDepthThresholdRejectsMatches`、`ProductionIdentityProjectionPixelBoundaryMatchesScalar`、`ProjectionPixelRvvDiagnosticMatchesScalar`、`IdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar`、`IdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar` | 验证 depth tail 仍正确；覆盖 production pixel staging 和 test-only projection-pixel 入口证据。 |
| target-predicate 诊断 | `TargetPredicateRvvDiagnosticMatchesScalar`、`TargetPredicateDiagnosticFakeIndicesMatchesScalar`、`TargetPredicateDiagnosticSubsetIndicesMatchesScalar`、`TargetPredicateSupportsPointXYZISourceAndTarget`、`TargetPredicateSmallInputFallbackMatchesScalar`、`TargetPredicateTightThresholdsMatchScalar`、`NonIdentityTargetPredicateDiagnosticFakeIndicesMatchesScalar`、`NonIdentityTargetPredicateDiagnosticSubsetIndicesMatchesScalar` | 覆盖 target gather、target finite、depth/distance mask、第二次 `vcompress` 保序、`PointXYZI` traits、小输入 fallback、tight threshold 和 non-identity 组合。 |
| production distance 边界 | `ProductionDistanceBoundaryPredicateMatchesScalar` | 构造 scalar float norm 位于 `float(max_distance)` 附近的 lane，验证 production RVV distance predicate 与 `double(float_norm) < max_distance` 一致。 |
| RVV distance bit 对拍 | `DistanceRvvMatchesEigenNormBits` | 直接对拍 RVV `vfmul + vfmacc + vfmacc + vfsqrt.f32` 与 Eigen `Vector3f::norm()` 的 float bit pattern。 |
| RVV double threshold 谓词 | `DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` | 覆盖 `float(max_distance)` 等于、低于 double threshold 的边界，验证 RVV `<` / `<=` 选择等价于标量 double predicate。 |

`ProductionProjectionBoundaryMatchesScalar` 已删除。该测试与 `ProductionFakeIndicesMatchesScalar` 使用相同数据和入口，没有新增 boundary adversarial 条件；projection boundary 风险由 projection-pixel diagnostic 和 non-identity boundary 数据覆盖。

- 专项测试：`test-rvv/registration/correspondence_estimation_organized_projection/output/qemu/run_test_std.log`、`run_test_rvv.log`。std 构建运行 39 个测试，其中 10 个 RVV-only 诊断按预期 skip；RVV 构建 39 个测试全部通过。其中 `CandidateSupportsPointXYZITarget` 和 `CandidateSupportsPointXYZISourceAndTarget` 覆盖 traits-gated `PointXYZI` 组合；`DiagnosticFakeIndicesMatchesScalar` 和 `DiagnosticSubsetIndicesMatchesScalar` 覆盖继承上游类后的 fake indices 与 `setIndices()` subset；生产入口 direct class 测试覆盖 identity fake/subset、identity projection boundary、distance predicate boundary、`PointXYZI`、small fallback、non-identity fake/subset、non-identity projection boundary、small-z `PointXYZI` 和小输入 fallback；`DistanceRvvMatchesEigenNormBits` 与 `DistanceRvvThresholdPredicateMatchesDoubleScalarPredicate` 覆盖 RVV distance bit pattern 和 double threshold predicate；`NonIdentityProjectionPixelDiagnosticFakeIndicesMatchesScalar` 和 `NonIdentityProjectionPixelDiagnosticSubsetIndicesMatchesScalar` 覆盖 non-identity projection-pixel production-shaped 诊断。
- QEMU bench：`output/qemu/run_bench_std.log`、`run_bench_rvv.log`、`analyze_bench_compare.log`；日志可解析，无 `未解析`、`n/a`、`Total Time 不计算`。identity production fake/explicit checksum 均为 `10393124863019881355`；non-identity production fake/explicit checksum 均为 `13376866430852120216`。QEMU 只作为构建、checksum、日志格式和指令路径证据。
- 反汇编：`build/asm/riscv/bench_correspondence_estimation_organized_projection_rvv.asm` 确认 `vsetvli ... e32,m2`、`vluxei32.v`、`vfmul.vf`、`vfmacc.vf`、`vfdiv.vv`、`vcompress.vm`、`vcpop.m`、`vse32.v`，并确认 projection-pixel 诊断路径出现 `vfcvt.rtz.x.f.v`。production 符号 `acceptProjectedOrganizedProjectionCandidatesRVV<PointXYZ>` 附近可见 target/projected gather 的 `vluxei32.v`、depth mask `vmfle.vf`、distance predicate 的 `vfmul.vv`、`vfmacc.vv`、`vfsqrt.v`、`vmfle.vf` / `vmflt.vf`、以及 `vcompress.vm`、`vcpop.m` 和 `vse32.v`；压缩后 stored distance 标量重算仍可见 `fsqrt.s`、`fcvt.d.s` 和 `flt.d`。
- 板卡：`output/board/run_test.log`、`run_bench_std.log`、`run_bench_rvv.log`、`analyze_bench_compare.log`。

Milkv-Jupiter 最新结果：

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

target-predicate production-shaped prototype 已补板卡结果，并已按 final distance predicate 语义接入 production。接入后 `board_smoke` 39 个专项测试全部通过。QEMU 语义门禁已恢复通过：64K diagnostic checksum 均为 `140547372586179969`；256K diagnostic checksum 均为 `13594340667184250285`；上游式 accepted prototype 的 identity fake/explicit checksum 均为 `140547372586179969`，non-identity fake/explicit checksum 均为 `13376866430852120216`。QEMU 计时只作为日志完整性信息记录，不作为性能结论。板卡显示 production fake/explicit checksum 均为 `10393124863019881355`，speedup 为 `1.64x` / `1.65x`；non-identity production fake/explicit checksum 均为 `13376866430852120216`，speedup 均为 `2.36x`。

## 8. 生产接入评估

本轮结论是可以接入第一版最小生产路径，且已经实施。接入依据：

- 可证明语义一致的生产 RVV 范围清晰：source `x/y/z` gather、source finite、identity fast path 或 Eigen-aligned non-identity transform staging、transformed `z > 0` 和 `vcompress` staging。
- production-shaped 诊断已经覆盖真实 `initCompute()`、fake indices、`setIndices()` subset、`PointXYZI` 组合和 full 输出顺序。
- 板卡 identity production fake/explicit case 最新为 `1.64x` / `1.65x`，non-identity production fake/explicit case 均为 `2.36x`；生产改动保持在 source staging helper、pixel staging helper、target-predicate staging helper、标量 accepted tail 和原循环 fallback helper 内。
- 投影截断和图像范围检查已经进入第二阶段 production RVV；target gather、target finite 和 depth mask 已进入第三阶段 production RVV。
- distance mask 在 test-rvv prototype 中保留，production 已接入 final distance predicate。新增 RVV-only bit 对拍证明 `vfmul + vfmacc + vfmacc + vfsqrt.f32` 与 Eigen `Vector3f::norm()` 一致，新增 threshold 诊断证明 RVV `<` / `<=` 分支等价于 `double(float_norm) < max_distance`。板卡 prototype 显示 target-predicate staging `1.68x` / `1.69x`，identity target-predicate full diagnostic `1.60x` / `1.62x`，上游式 accepted prototype fake/explicit 为 `1.63x` / `1.62x`，non-identity fake/explicit 均为 `2.12x`。

已实施的最小侵入方案：

1. 在 `correspondence_estimation_organized_projection.hpp` 的 impl header 内增加 `pcl::registration::detail` helper，RVV helper 限定在 `#if defined(__RVV10__)`。
2. traits gate 只作为 RVV staging 入口条件：source / target 必须有 PCL 注册的单个 `float x/y/z` 字段；`Scalar` 必须是 `float`；identity transform 走原始 xyz 快路径，non-identity transform 走 Eigen-aligned FMA staging；尺寸和 `vlmax_e32m2 <= 64` 条件失败时回退。
3. 原标量循环抽为 `determineCorrespondencesOrganizedProjectionStd`，作为所有 fallback 的权威路径。
4. RVV 分流先生成 `OrganizedProjectionCandidate{source_index, x, y, z}`，再尝试生成 `ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}`；projected staging 成功后尝试 `AcceptedOrganizedProjectionCandidate{source_index, target_index, distance}`，其中 target gather、target finite、depth mask 和 final distance predicate 用 RVV，stored distance 值用标量 Eigen `norm()` 重算。
5. `determineCorrespondences` 的公开 API、类成员布局和 `determineReciprocalCorrespondences` 行为保持不变。

生产接入后的验证状态：

- QEMU `run_test_compare`：std 构建运行 39 个测试，其中 10 个 RVV-only 诊断 skip；RVV 构建 39 个测试全部通过。
- QEMU `run_bench_compare`：identity production fake/explicit checksum 均为 `10393124863019881355`；non-identity production fake/explicit checksum 均为 `13376866430852120216`。
- 板卡 `board_smoke`：39 个专项测试通过；target-predicate staging 64K/256K 为 `1.68x` / `1.69x`，identity target-predicate diagnostic 64K/256K 为 `1.60x` / `1.62x`；上游式 accepted prototype fake/explicit 为 `1.63x` / `1.62x`，non-identity fake/explicit 均为 `2.12x`；production fake/explicit case checksum 均为 `10393124863019881355`，speedup 为 `1.64x` / `1.65x`；non-identity production fake/explicit case checksum 均为 `13376866430852120216`，speedup 均为 `2.36x`。

## 9. 结论与后续方向

`CorrespondenceEstimationOrganizedProjection` 的前置 source transform staging、projection-pixel staging 和 target-predicate staging 可以 RVV 化并保持 correspondence 语义。target-predicate diagnostic 和 production-shaped prototype 已通过 correctness、反汇编和板卡验证；production final distance predicate 接入后，identity production fake/explicit case 为 `1.64x` / `1.65x`，non-identity production fake/explicit case 均为 `2.36x`。

当前 production RVV 覆盖 source gather、4x4 transform staging、source finite、transformed `z > 0`、candidate compress、`u/v` 截断、图像范围、`target_index` compress、target `x/y/z` gather、target finite、depth mask 和 final distance predicate。输出 append 与 stored distance 写出仍为标量。`test-rvv` 中保留完整 target finite/depth/distance mask prototype；production 版已通过 distance bit 对拍和 double threshold 诊断，保证确定性 correspondence 序列。

后续可继续评估三个增量方向：

- 投影与阈值边界扩展：当前已用 `vfmacc` 对齐标量 contraction，并通过 QEMU / 板卡测试；后续可继续补更密集的整数边界、负投影坐标、小 `z` 和阈值邻域数据。
- VLEN 与临时 buffer 泛化：当前固定 `[64]` 栈 buffer 与 `vlmax_e32m2 <= 64` gate 配套；若目标板卡出现更大 VLEN，可评估动态 scratch、固定 VL 分块或 SoA staging，但需要重新验证复杂度和板卡收益。
- 输出写入：当前 accepted lane 到 `pcl::Correspondence` 的 append 和 stored distance 写出保留标量。继续 RVV 化需要证明结构体布局、scatter 地址、可变数量保序写出和 stored distance bit pattern 都可控；目前收益风险比不支持作为下一步默认方向。

这些增量需要在独立诊断中重新证明语义和 full production case 收益。当前 production-ready 边界是 source transform staging、projection-pixel staging 和 target-predicate staging；最终 append / stored distance 写出仍是刻意保留的标量边界。
