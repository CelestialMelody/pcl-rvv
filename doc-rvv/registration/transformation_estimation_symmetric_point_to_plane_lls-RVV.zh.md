# registration/transformation_estimation_symmetric_point_to_plane_lls RVV 生产接入记录

## 收尾摘要

`TransformationEstimationSymmetricPointToPlaneLLS::estimateRigidTransformation` 已完成第二轮 production integration loop（生产接入闭环）。production（生产源码）现在接入 traits-gated generic normal point type（用 PCL 点类型字段特征约束的泛型 normal 点类型）RVV 分流：`PointSource` 和 `PointTarget` 必须分别满足 `x/y/z/normal_x/normal_y/normal_z` 都是单个 `float` 字段、POD（普通数据布局）/ standard-layout（标准布局）和 offset alignment（字段偏移对齐）条件，`Scalar=float`，且入口必须是无 indices / 无 correspondences 的 full-cloud（全云顺序扫描）公开入口。indices、source+target indices、correspondences 和 `Scalar=double` 继续走原 `ConstCloudIterator` 标量路径。

QEMU correctness（QEMU 正确性验证，不代表真实性能）通过，bench（性能测试）输出合同可解析，反汇编证明真实 public estimator（公开估计器）会调用 `pcl::registration::detail::estimateSymmetricPointNormalFullCloudRVV<PointSource, PointTarget>`。生产 helper 模板实例覆盖 `PointNormal -> PointNormal` 和 `PointXYZINormal -> PointXYZINormal`，符号范围内可见 `vlse32.v`、`vcompress.vm`、`vcpop.m`、`vfmul.vv`、`vfadd.vv`、`vfsub.vv` 和自动 `vfredosum.vs`。板卡 `board_smoke` 已在 Milkv-Jupiter 上通过 18 个专项测试；production-direct full-cloud `PointNormal` 64K / 256K 为 `2.62x` / `2.47x`，`PointXYZINormal` 64K / 256K 为 `2.64x` / `1.89x`。因此本主题当前结论是 `production-ready/generic-normal-full-cloud`（泛型 normal 全云入口可生产接入）：只批准 full-cloud `Scalar=float` 且满足 normal traits gate 的点类型分流；correspondences 路径仍不接 production，退化主因仍是待消融假设。

## 1. 函数入口作用

`TransformationEstimationSymmetricPointToPlaneLLS` 用带 normal（法线）的 source cloud（源点云）和 target cloud（目标点云）估计 4x4 刚体变换。它来自 symmetric ICP（对称迭代最近点）公式：每个点对同时使用 source normal 和 target normal，把 `[(p + q).cross(n), n]` 作为一行 6 维线性系统，再由 6x6 normal-equation（法方程）求出小角度旋转和平移参数。

公开重载支持：

- 全云 source/target：第 `i` 个 source 与第 `i` 个 target 对应。
- source indices + target 全云：source 通过 index 间接访问，target 顺序访问。
- source indices + target indices：source/target 都通过 index 间接访问。
- correspondences：通过 `index_query/index_match` 指定点对。

所有公开重载最终进入同一个 protected helper：

```text
source / target iterator
  -> 读取 p、q、n1、n2
  -> enforce_same_direction_normals_ 法线选择
  -> finite check
  -> v = [(p + q).cross(n), n]
  -> ATA/ATb 累加
  -> Eigen LDLT solve
  -> construct 4x4 transform
```

`constructTransformationMatrix` 和 Eigen LDLT solve（Eigen LDLT 求解器）每次 estimate 调用只执行一次，不随点数增长，因此不是本轮首要 RVV 化对象。

## 2. 标量路径与诊断边界

production 源码在公开入口里构造 `ConstCloudIterator`（常量点云迭代器），把不同入口统一成 source/target 同步逐点流。helper 内部看不到“这是全云还是 correspondences”的显式分支；它只看到两个 iterator 依次给出的当前点。

逐点标量语义是：

1. 读取 `p = source.xyz`、`q = target.xyz`、`n1 = source.normal`、`n2 = target.normal`。
2. 如果 `enforce_same_direction_normals_` 开启，先计算 `n1.dot(n2)`。点积非负时 `n = n1 + n2`；点积为负时 `n = n1 - n2`。如果关闭该开关，始终使用 `n = n1 + n2`。
3. finite check 发生在合成 `n` 之后。`p`、`q` 或 `n` 任一分量不是 finite 时，标量循环 `continue`，该点不贡献法方程。
4. 对有效点构造：

```text
a = (py + qy) * nz - (pz + qz) * ny
b = (pz + qz) * nx - (px + qx) * nz
c = (px + qx) * ny - (py + qy) * nx
d = (qx - px) * nx + (qy - py) * ny + (qz - pz) * nz
```

其中 `[a,b,c,nx,ny,nz]` 就是 `v`，`d` 是 `(q - p).dot(n)`。

5. `ATA += v * v.transpose()`，`ATb += v * d`。源码通过 `selfadjointView<Eigen::Upper>()` 只维护上三角视图。
6. 循环结束后执行一次 `M.ldlt().solve(ATb)`，再由 `constructTransformationMatrix` 生成 4x4 矩阵。

本轮 RVV 诊断为了让取数成本和入口形态可审查，把 production helper 中统一的 iterator 流拆成两条显式路径：全云顺序扫描和对应关系索引扫描。它们映射到真实公开入口，但仍是 test-rvv diagnostic（测试专用诊断），不是 production direct。

## 3. 覆盖范围与 fallback

当前 diagnostic 覆盖：

- `pcl::PointNormal` + `float` 输出矩阵的全云顺序扫描路径。
- correspondences 路径的乱序和重复 index 对拍。
- `enforce_same_direction_normals=true` 默认路径和显式关闭路径。
- 小规模 `n < 64` fallback（回退路径）。
- source/target 坐标和合成 normal 的 NaN/Inf 剔除。
- `vlmax_e32m2 <= 64` 固定 buffer gate（固定缓冲验收条件）。
- source/target 点数量可转 32-bit byte offset（32 位字节偏移）。

当前 production 覆盖：

- 满足 `RVV Generic Point Type Strategy` 的 full-cloud generic normal 点类型：source 和 target 分别有单个 `float x/y/z/normal_x/normal_y/normal_z` 字段，POD / standard-layout、`sizeof(PointT) == sizeof(POD)` 和字段 offset 对齐均成立。
- 2026-07-22 起，该 gate 由公共 `pcl::rvv::RVVXYZNormalFloatLayout<PointT>` 表达；本地 `SymmetricXYZNormalFloatLayout` 已删除。公共 trait 只负责编译期字段/layout 判断，不包含 symmetric LLS 的 full-cloud dispatch、normal equation 构造或 RVV intrinsic 实现。

当前 production 不覆盖：

- 非 normal 点类型或 traits 不完整的自定义点类型。
- `Scalar=double` 的模板边界。
- source indices + target 全云、source indices + target indices 两个 indexed 公开入口。
- correspondences 公开入口。

这些缺口不阻塞当前 `generic-normal-full-cloud` production-ready，因为 PI2 分流用 traits / Scalar / overload / runtime gate 把它们隔离到标量 fallback（回退路径）。它们仍阻止把结论扩大成 `Scalar=double`、indexed 或 correspondences production-ready。

## 4. 详细设计

| 暂存路径（staging） | 结构 / helper | 字段来源 | 下一段消费者 / tail | gate |
| --- | --- | --- | --- | --- |
| 全云 symmetric row | production helper 模板 / `accumulate_candidate_full` | production 使用 traits offset 读取 source/target `x/y/z/normal_x/normal_y/normal_z`；diagnostic 仍固定 `PointNormal` AoS（结构数组）布局 stride load（跨步加载） | `accumulate_staged_rows` 或 production 等价尾段压缩后累加 `ATA/ATb`，之后 Eigen solve | `n >= 64`、`vlmax_e32m2 <= 64`、32-bit byte offset、generic normal traits gate |
| 对应关系 symmetric row | `accumulate_candidate_correspondences` | 标量展开后的 source/target index，再用 gather（离散加载）读取点字段 | 同一 `accumulate_staged_rows` | correspondences 数量满足 gate，且展开后有效行不少于 64 |
| 法线选择 | `select_symmetric_normal_v` | source/target normal 的 dot mask（点积掩码） | `staged_symmetric_formula` | `enforce_same_direction_normals` 开关 |
| 公式暂存 | `staged_symmetric_formula` | `sx/sy/sz/tx/ty/tz/nx/ny/nz` | `accumulate_staged_rows` | 只由调用方 gate 控制 |

全云顺序扫描的 VL chunk（可变向量长度分块）数据流：

```text
vsetvl
  -> vlse32.v 跨步加载 source xyz
  -> vlse32.v 跨步加载 target xyz
  -> vlse32.v 跨步加载 source normal
  -> vlse32.v 跨步加载 target normal
  -> dot(n1, n2) 生成同向 mask
  -> vmerge 选择 n1 + n2 或 n1 - n2
  -> 对 p/q/n 构造 finite mask
  -> 计算 a/b/c/d 公式
  -> vcpop + vcompress 写入 64-lane buffers
  -> buffer tail 累加 ATA/ATb
  -> Eigen LDLT solve + construct matrix
```

对应关系索引路径多一个标量展开阶段：

```text
扫描 correspondences
  -> 跳过负 index 和越界 index
  -> 展开 src_indices / tgt_indices vectors
  -> vsetvl
  -> vle32.v 连续加载 indices
  -> 计算 byte offset
  -> vluxei32.v gather source/target xyz 和 normal
  -> 复用同一套 normal-select / mask / formula / compress / tail
```

这种拆分的证据边界要看清：全云路径的取数形态较规则；correspondences 路径天然多了 index 展开、offset 计算和不规则 gather。板卡上的 indexed 退化只能说明当前组合不适合生产接入，不能单独证明 gather、展开、压缩或 tail 是唯一主因。

## 5. 关键实现片段

RVV helper 片段的职责是 gate、load/gather、合成 normal、finite mask 和公式 staging：

```cpp
if (n >= 64 && __riscv_vsetvlmax_e32m2() <= 64 && offset_fits_u32) {
  for (std::size_t i = 0; i < n;) {
    const std::size_t vl = __riscv_vsetvl_e32m2(n - i);
    // load source/target xyz and normals
    // select_symmetric_normal_v: dot mask -> n1+n2 or n1-n2
    // finite_mask on p/q/n, matching production continue
    // staged_symmetric_formula: produce a/b/c/d
    accumulate_staged_rows(a, b, c, d, nx, ny, nz, keep, vl, eq);
    i += vl;
  }
}
```

标量 tail 片段的职责是按压缩后的可见顺序继续 production 的法方程累加：

```cpp
const std::size_t kept = __riscv_vcpop_m_b16(keep, vl);
vcompress a/b/c/d/n into buffers;
for (std::size_t lane = 0; lane < kept; ++lane) {
  // Same row contribution as production M.rankUpdate(v) and ATb += v * d.
  accumulate ATA upper-triangle terms and ATb terms;
}
```

两个片段之间的边界是：RVV 接管原标量热点循环里的字段读取、法线选择、finite predicate 和逐点公式；标量 tail 继续 `ATA/ATb` 累加和后续 Eigen solve。tail 当前保留为 C++ 循环，是为了降低首轮语义风险；但反汇编显示 GCC 会把部分 tail 自动向量化为 `vfredosum.vs`。这意味着当前诊断不是严格线性标量 tail，生产接入前必须显式审计或控制规约结构。

## 6. 实现选择审计

直接向量规约暂缓。它可能减少 buffer 写回和 tail 成本，但会改变 `ATA/ATb` 的跨 lane 累加树。6x6 solve 对累加差异可能敏感，生产接入前必须有 adversarial case（对抗边界样本）、矩阵输出误差预算和目标硬件收益。

`vcompress + buffer + tail` 被用于当前 production direct 和 diagnostic 方案。它保留有效 lane 的扫描顺序，让 invalid lane 被剔除后仍对应 production 的 `continue` 语义；代价是额外 store/load 和 buffer tail 成本。板卡 production-direct full-cloud `PointNormal` 为 `2.62x` / `2.47x`，`PointXYZINormal` 为 `2.64x` / `1.89x`，说明这套成本在连续公开入口上可被收益覆盖，但 correspondences 退化说明它不能直接扩展到 indexed row。

部分 vector accumulation（部分向量规约）是反汇编观察，不是手写承诺。`vfredosum.vs` 来自编译器对 tail 的自动向量化；它提供了一个可观察的性能/语义边界，当前由 production direct 测试和板卡结果覆盖。若后续修改编译器、flags、FMA 结构或规约结构，必须重新审计这项边界。

fused multiply-add（融合乘加）intrinsic 暂缓。当前公式保持 `vfmul + vfadd/vfsub`，便于审查是否复刻 production float 表达式。全二进制中可能存在 FMA 指令，但不能默认归因到当前 helper；如果后续为了全云 direct 提升收益而改用 FMA，需要 same-chain（同构链路）测试和热点符号反汇编。

数学函数和矩阵构造暂缓。`constructTransformationMatrix`、三角函数和 Eigen LDLT solve 每次 estimate 只执行一次，不是随点数增长的 VL chunk 热点。除非 profile（性能剖析）证明它们成为主成本，否则不值得手写 RVV。

## 7. 数值算例与 VL chunk

假设一个 chunk 有 4 个 lane，其中 lane 1 的 target normal 与 source normal 反向，lane 2 的 source `x` 是 NaN：

```text
lane:        0          1          2          3
dot(n1,n2): +0.90      -0.80      +0.70      +0.95
normal op:  n1 + n2    n1 - n2    n1 + n2    n1 + n2
finite p/q/n: true     true       false      true
keep mask:  1          1          0          1
```

RVV 会先为所有 lane 生成候选 `n` 和 `a/b/c/d`，然后用 keep mask 剔除 lane 2。`vcompress` 后 buffer 顺序是 lane `0,1,3`，这与 production 标量循环遇到 lane 2 时 `continue` 的可见顺序一致。

一个单点公式例子：

```text
p = (1.0, 2.0, 3.0)
q = (1.2, 1.8, 3.1)
n1 = (0, 0, 1)
n2 = (0, 0, -0.8)
enforce_same_direction_normals = true

dot(n1,n2) = -0.8，所以 n = n1 - n2 = (0, 0, 1.8)
p + q = (2.2, 3.8, 6.1)
q - p = (0.2, -0.2, 0.1)
a = 3.8 * 1.8 - 6.1 * 0 = 6.84
b = 6.1 * 0 - 2.2 * 1.8 = -3.96
c = 2.2 * 0 - 3.8 * 0 = 0
d = 0.2 * 0 + (-0.2) * 0 + 0.1 * 1.8 = 0.18
```

该 lane 对 `ATb` 的贡献是 `[a*d, b*d, c*d, nx*d, ny*d, nz*d]`，也就是 `[1.2312, -0.7128, 0, 0, 0, 0.324]`。这个例子展示了为什么法线同向选择必须在 finite mask 和公式 staging 之前完成。

## 8. Bench case 说明

bench 输入在计时前构造。source 是确定性解析二次曲面 `PointNormal`，target 由 `pcl::transformPointCloudWithNormals` 使用温和刚体变换生成，并固定翻转部分 target normal 来覆盖同向法线分支。`PointXYZINormal` production-direct case 由同一组几何点和 normal 字段转换得到，只改变 AoS layout（结构数组布局）和字段 offset，用来验证 traits-gated generic normal dispatch（泛型 normal 分流）而不是更换数据集。correspondences 是确定性 subset，包含非连续和重复 index；它主要用于暴露 indexed row 形态，不代表所有真实 correspondence 分布。

测量包含 normal-equation 构造、Eigen LDLT solve 和 4x4 矩阵构造。correspondences case 包含 candidate 内部 index 展开成本，因为当前 gather 方案必须付出这部分入口成本。

| case | 入口 | 规模 | 是否命中 RVV | 证明点 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- |
| `symmetric lls production-direct full-cloud pointnormal 65536` | 真实 public estimator 全云入口 | 64K | RVV 构建命中 production helper | full-cloud `PointNormal` / `float` 生产分流收益。 | 不证明 indexed 入口。 |
| `symmetric lls production-direct full-cloud pointxyzinormal 65536` | 真实 public estimator 全云入口 | 64K | RVV 构建命中 production helper 模板实例 | `PointXYZINormal` 泛型 normal layout 的 traits-gated 生产分流收益。 | 不证明所有自定义点类型或 indexed 入口。 |
| `symmetric lls full-cloud pointnormal 65536` | test-rvv diagnostic 全云顺序扫描 | 64K | RVV 构建命中 diagnostic helper | 诊断 wrapper 与生产分流的同构对照。 | 不代表真实 public estimator overhead。 |
| `symmetric lls correspondences pointnormal 65536` | 对应关系索引 | 64K 输入点 subset | RVV 构建命中 | index 展开 + gather 路径 correctness 和负向性能信号。 | 不能单独归因 gather、展开、压缩或 tail 哪一项最慢。 |
| `symmetric lls production-direct full-cloud pointnormal 262144` | 真实 public estimator 全云入口 | 256K | RVV 构建命中 production helper | 放大规模后的生产分流收益。 | 不证明 indexed row。 |
| `symmetric lls production-direct full-cloud pointxyzinormal 262144` | 真实 public estimator 全云入口 | 256K | RVV 构建命中 production helper 模板实例 | 放大规模后泛型 normal layout 仍有生产收益。 | 不证明 indexed row 或 `Scalar=double`。 |
| `symmetric lls full-cloud pointnormal 262144` | test-rvv diagnostic 全云顺序扫描 | 256K | RVV 构建命中 diagnostic helper | 放大规模后的诊断收益。 | 不代表泛型 production。 |
| `symmetric lls correspondences pointnormal 262144` | 对应关系索引 | 256K 输入点 subset | RVV 构建命中 | 大规模 indexed row 的负向性能证据。 | 不代表所有真实 correspondence 分布，也不能确认退化主因。 |

## 9. 测试、QEMU、反汇编和板卡证据

- QEMU test：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_test_compare`，std/RVV 各 18 个测试通过。
- QEMU bench：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_bench_compare`，日志可解析，checksum 基本对齐；QEMU 不作为性能结论。
- 反汇编：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls dump_bench_rvv`，摘要在 `output/qemu/rvv_asm_check.log`。摘录确认 public full-cloud estimator 调用 production helper `estimateSymmetricPointNormalFullCloudRVV`，生产 helper 符号范围内包含 `vlse32.v`、`vcompress.vm`、`vcpop.m`、`vfmul.vv`、`vfadd.vv`、`vfsub.vv` 和 `vfredosum.vs`。
- 板卡：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls board_smoke` 已通过专项测试和 bench compare，真实性能日志在 `output/board/analyze_bench_compare.log`。

板卡结果如下：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| production-direct `PointNormal` full-cloud 64K | 15.5195 | 5.9203 | 2.62x | 真实公开入口收益成立，接 production。 |
| production-direct `PointXYZINormal` full-cloud 64K | 15.5035 | 5.8683 | 2.64x | 泛型 normal traits-gated 入口收益成立，接 production。 |
| diagnostic full-cloud 64K | 7.8638 | 6.3597 | 1.24x | 诊断链路仍有收益，但不是最终生产结论。 |
| diagnostic correspondences 64K | 6.4343 | 10.0542 | 0.64x | indexed row 当前组合退化，不接 production。 |
| production-direct `PointNormal` full-cloud 256K | 61.9372 | 25.0582 | 2.47x | 放大规模后生产收益仍稳定。 |
| production-direct `PointXYZINormal` full-cloud 256K | 61.9022 | 32.7183 | 1.89x | 泛型 normal layout 放大规模后仍有收益。 |
| diagnostic full-cloud 256K | 31.4140 | 27.8024 | 1.13x | 诊断链路收益稳定。 |
| diagnostic correspondences 256K | 25.8557 | 62.3956 | 0.41x | indexed row 仍退化。 |

证据日志保留在 `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/output/`、`build/asm/` 和 `log/`，不作为默认提交内容。

## 10. 反汇编热点归属

`estimateSymmetricPointNormalFullCloudRVV<PointSource, PointTarget>` 是当前 production direct 热点符号。public full-cloud estimator 的反汇编调用点能跳到 `PointNormal -> PointNormal` 和 `PointXYZINormal -> PointXYZINormal` 两个 helper 模板实例，helper 符号范围内的 `vlse32.v` 对应 traits offset 字段 stride load，`vcompress.vm` 和 `vcpop.m` 对应有效 lane 压缩，`vfredosum.vs` 对应编译器自动生成的 buffer tail partial vector accumulation。

`accumulate_candidate_full` 仍是 full-cloud 诊断热点符号，用于和生产 direct 对照。`accumulate_staged_rows` 中的 `vcompress.vm` 和 `vcpop.m` 对应有效 lane 压缩，`vfredosum.vs` 对应编译器自动生成的 buffer tail partial vector accumulation。

`accumulate_candidate_correspondences` 符号存在，全二进制摘录可见 `vluxei32.v` gather。当前摘要足以证明 RVV 二进制存在 gather 形态，但如果后续要做更强归因，仍需按符号范围输出 gather、vcompress、vfredosum 和 FMA 的细分统计。

当前反汇编和板卡证据组合支持两条结论：full-cloud generic normal / `float` production direct 有稳定板卡收益，correspondences candidate 当前整体退化。它不能支持“correspondences 慢的主因就是 gather”或“buffer + tail 是唯一瓶颈”这类单因归因。要闭合负向归因，需要新增消融 bench：只做 index 展开不 gather、连续 index 但保留 gather 形态、去掉 `vcompress` 的 masked reduction 原型、固定或禁用自动 `vfredosum` tail。

## 11. 生产接入评估

`EvidenceDecision = production-ready/generic-normal-full-cloud`。

接入范围：

- 真实 public full-cloud overload。
- `PointSource` 和 `PointTarget` 分别满足 generic normal traits gate：`x/y/z/normal_x/normal_y/normal_z` 均为单个 `float` 字段，POD / standard-layout、`sizeof(PointT) == sizeof(POD)` 和字段 offset 对齐成立。
- `Scalar=float`。
- `__RVV10__` 打开、`n >= 64`、`vlmax_e32m2 <= 64`、32-bit byte offset 可表达。

fallback 范围：

- 非 RVV 构建。
- 小输入或 VLEN/buffer gate 不满足。
- `Scalar=double`。
- traits 不满足或 layout gate 不满足的点类型。
- source indices、source+target indices 和 correspondences overload。

correspondences 路径不接 production。板卡已经给出负向性能信号，但退化主因仍是待消融假设；文档不把它归因到单一原因。

## 12. Production integration closeout

本轮按 handoff 完成 production integration loop（生产接入闭环）。PI1 冻结的范围是全云连续入口：

```cpp
estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                            const pcl::PointCloud<PointTarget>& cloud_tgt,
                            Matrix4& transformation_matrix) const
```

第二轮 PI2 production patch（生产补丁）把 full-cloud dispatch（分流逻辑）从 exact `PointNormal` 扩展到 traits-gated generic normal 点类型。分流仍放在该 overload 完成 source/target size check 之后、构造 `ConstCloudIterator` 之前。indices、source indices + target indices 和 correspondences 三个公开 overload 不改动，继续构造 iterator 并进入原标量 helper。这样做的原因是当前板卡收益只来自全云顺序扫描；indexed row 的退化仍是 index 展开、gather、压缩和 tail 等多因素待消融假设。

PI2 采用受 traits gate 约束的泛型 normal production patch，而不是无条件泛型接入：

| gate（门禁） | 当前 production 行为 | fallback（回退路径） |
| --- | --- | --- |
| 编译宏 | 只在 `__RVV10__` 下编译 RVV helper | 非 RVV 构建完全保留当前标量 helper。 |
| 点类型 | `PointSource` 和 `PointTarget` 分别通过 `pcl::traits::has_xyz`、`has_normal`、`datatype`、`offset`、POD / standard-layout 和 alignment gate | traits 不完整、字段不是单个 `float`、POD 大小不匹配或 offset 不对齐时走标量。 |
| `Scalar` | 只允许 `Scalar=float` | `Scalar=double` 继续走标量。 |
| 入口形态 | 只允许无 indices / 无 correspondences 的 full-cloud | indices 和 correspondences overload 不分流。 |
| 规模 | `n >= 64` | 小规模继续标量，避免 staging 固定成本吞掉收益。 |
| VLEN / buffer | `vlmax_e32m2 <= 64`，匹配固定压缩 buffer 容量 | VLEN 超出 buffer 容量时标量。 |
| finite / non-dense | 不把 `cloud.is_dense` 作为 gate；RVV keep mask 继续检查 source/target xyz 和合成 normal | invalid lane 通过 `vcompress` 剔除，语义对应 production 的 `continue`。 |
| normal 字段 | source/target 分别证明 `normal_x/y/z` 为单个 `float` 字段 | 非 normal 点类型或未注册 normal traits 的自定义类型不进入 RVV。 |

生产源码结构：

```text
full-cloud public overload
  -> size check
  -> #if defined(__RVV10__)
       try estimateSymmetricPointNormalFullCloudRVV(...)
       if true: return
     #endif
  -> ConstCloudIterator source/target
  -> existing scalar helper
```

RVV helper 只接管逐点热点：stride load source xyz、target xyz、source normal、target normal，完成同向 normal 选择、finite mask、`(p + q).cross(n)` 和 `(q - p).dot(n)` staging，然后按压缩后的有效 lane 累加 normal equation。Eigen LDLT solve 和 4x4 matrix 构造仍在 helper 内保持标量结构。

第二轮 PI3 production direct tests（生产直连测试）已新增真实公开入口测试，而不是只调用 test-rvv diagnostic wrapper：

- full-cloud `PointNormal` / `float`：RVV build 调用真实 estimator，矩阵与 std build 或同构 reference 在容差内一致。
- full-cloud `PointXYZINormal` / `float`：真实 estimator 与 `PointNormal` reference 对拍，证明 traits offset / stride / normal 字段映射成立。
- mixed layout：`PointXYZINormal -> PointNormal` 真实 estimator 与 `PointNormal` reference 对拍，证明 source/target layout gate 分别计算。
- `setEnforceSameDirectionNormals(false)`：真实 estimator 命中同一 full-cloud 分流并保持关闭 gate 的语义。
- invalid lane：source/target xyz 或合成 normal 含 NaN/Inf 时，真实 estimator 与标量 reference 一致。
- small input fallback：`n < 64` 仍走标量并输出一致。
- indexed fallback：source indices、source+target indices 和 correspondences 真实 overload 继续输出标量结果，不因 full-cloud 分流而改变。
- `Scalar=double` fallback：编译并运行 `PointNormal, PointNormal, double` 和 `PointXYZINormal, PointXYZINormal, double` case，确认不进入 RVV 分流。

PI4 production evidence rerun（生产证据重跑）已执行：

```text
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_test_compare
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_bench_compare
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls dump_bench_rvv
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls board_smoke
```

bench 已新增 `PointNormal` 和 `PointXYZINormal` production direct case，均调用真实 public estimator 的 full-cloud overload。反汇编摘要证明生产 helper 模板实例命中 `vlse32.v` / `vcompress.vm` / `vcpop.m`，并标注 `vfredosum.vs` 仍来自压缩 buffer tail。板卡结果已重新生成：`PointNormal` production-direct 64K / 256K 为 `2.62x` / `2.47x`，`PointXYZINormal` production-direct 64K / 256K 为 `2.64x` / `1.89x`。

PI5 结论：`production-ready/generic-normal-full-cloud`。它不是所有 symmetric LLS 入口 production-ready；`Scalar=double`、indexed 和 correspondences 路径保持标量，不能声称 correspondences 退化主因已经定位。

2026-07-22 公共 trait 重构只替换了 layout 判断和 32-bit byte offset helper，未扩大 production 行为：仍只有 full-cloud、`Scalar=float`、generic normal traits gate 命中时尝试 RVV；indices、correspondences 和 `Scalar=double` 继续按本节 fallback 走标量。

## 13. 后续方向

本轮完成 symmetric LLS 的 generic normal full-cloud production closeout。结论不是“整个 symmetric LLS 泛型入口已可生产接入”，而是“全云连续 `Scalar=float` 且 source/target 满足 generic normal traits gate 的路径已接入 production；对应关系索引路径不接生产”。

后续如果要扩大范围，必须重新走候选评估：`Scalar=double` 需要重新定义 RVV double 公式和误差边界；indices 和 correspondences 需要消融 bench 分离 index 展开、gather、压缩和 tail 成本。correspondences 的负向归因仍需消融 bench，不应把退化写成单一原因。
