# registration/transformation_estimation_symmetric_point_to_plane_lls RVV 生产接入记录

## 收尾摘要

`TransformationEstimationSymmetricPointToPlaneLLS::estimateRigidTransformation` 已完成第三轮 production integration loop（生产接入闭环），并在后续 dispatch structure cleanup（分流结构收口）中对齐 PCL 既有 SIMD 源码组织风格。production（生产源码）现在用 row-source policy（行来源策略）统一两条已获证据支持的数据流：full-cloud（全云顺序扫描）和 source indices + target full-cloud（source 索引 + target 全云）。两条路径共用后半段 RVV symmetric row pipeline（对称行公式流水线）：法线同向选择、finite mask（有限值掩码）、`(p + q).cross(n)` 公式、`vcompress` 压缩和 `ATA/ATb` 尾段累加；policy 只负责“前半段如何取 source/target row”。公开 overload 现在保持为“语义检查 -> RVV 短路 -> Std fallback”的小型分发层，原标量路径集中到 `estimateSymmetricPointNormal*Std` helper，RVV 路径仍由 `estimateSymmetricPointNormal*RVV` helper 负责。`PointSource` 和 `PointTarget` 仍必须分别满足 `x/y/z/normal_x/normal_y/normal_z` 都是单个 `float` 字段、POD（普通数据布局）/ standard-layout（标准布局）和 offset alignment（字段偏移对齐）条件，`Scalar=float`。source+target indices、correspondences 和 `Scalar=double` 继续走 `Std` 标量路径。

QEMU correctness（QEMU 正确性验证，不代表真实性能）通过，bench（性能测试）输出合同可解析，反汇编证明真实 public estimator（公开估计器）会调用 `pcl::registration::detail::estimateSymmetricPointNormalRowsRVV<RowSourcePolicy>`。生产 helper 模板实例覆盖 full-cloud 的 `PointNormal -> PointNormal`、`PointXYZINormal -> PointXYZINormal`，以及 source-indexed 的 `PointNormal -> PointNormal`、`PointXYZINormal -> PointXYZINormal`。反汇编中 full-cloud policy 可见 `vlse32.v` stride load；source-indexed policy 可见 `vle32.v` 读取 source index stream、`vmul.vx` 生成 byte offset、source `vluxei32.v` gather 和 target `vlse32.v` stride load。板卡 `board_smoke` 已在 Milkv-Jupiter 上通过 25 个专项测试；production-direct full-cloud `PointNormal` 64K / 256K 为 `2.71x` / `2.70x`，`PointXYZINormal` 64K / 256K 为 `2.71x` / `2.48x`；production-direct source-indexed `PointNormal` 为 `2.11x` / `1.87x`，`PointXYZINormal` 为 `2.10x` / `1.90x`。diagnostic dual-indices 仍为 `0.80x` / `0.63x`，correspondences 为 `0.77x` / `0.86x`。因此本主题当前 production 结论升级为 `production-ready/generic-normal-full-cloud-and-source-indexed`（泛型 normal 全云和 source 单侧索引入口可生产接入）：只批准 full-cloud 与 source-indexed `Scalar=float` 且满足 normal traits gate 的点类型分流；dual-indices 和 correspondences 不接 production，correspondences 退化主因仍是多因素待消融假设。

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

标量源码通过 `estimateSymmetricPointNormal*Std` helper 构造 `ConstCloudIterator`（常量点云迭代器），把不同入口统一成 source/target 同步逐点流。该标量 helper 内部看不到“这是全云还是 correspondences”的显式分支；它只看到两个 iterator 依次给出的当前点。公开 overload 不再直接展开 iterator fallback，而是只做参数检查、RVV 短路和 Std helper 调用。

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

RVV 诊断为了让取数成本和入口形态可审查，把 production helper 中统一的 iterator 流拆成四条显式路径：全云顺序扫描、source indices + target full-cloud、source indices + target indices、对应关系索引扫描。第三轮生产接入只把前两条证据闭合的数据流接入 production：full-cloud 和 source indices + target full-cloud。dual-indices 和 correspondences 仍只作为 test-rvv diagnostic（测试专用诊断），不是 production direct。

## 3. 覆盖范围与 fallback

当前 diagnostic 覆盖：

- `pcl::PointNormal` + `float` 输出矩阵的全云顺序扫描路径。
- source indices + target full-cloud 的单侧 gather 消融。
- source indices + target indices 的双侧 gather 消融。
- correspondences 路径的乱序和重复 index 对拍。
- `enforce_same_direction_normals=true` 默认路径和显式关闭路径。
- 小规模 `n < 64` fallback（回退路径）。
- source/target 坐标和合成 normal 的 NaN/Inf 剔除。
- `vlmax_e32m2 <= 64` 固定 buffer gate（固定缓冲验收条件）。
- source/target 点数量可转 32-bit byte offset（32 位字节偏移）。

当前 production 覆盖：

- 满足 `RVV Generic Point Type Strategy` 的 full-cloud generic normal 点类型：source 和 target 分别有单个 `float x/y/z/normal_x/normal_y/normal_z` 字段，POD / standard-layout、`sizeof(PointT) == sizeof(POD)` 和字段 offset 对齐均成立。
- 公共 trait 重构后，该 gate 由 `pcl::rvv::RVVXYZNormalFloatLayout<PointT>` 表达；本地 `SymmetricXYZNormalFloatLayout` 已删除。公共 trait 只负责编译期字段/layout 判断，不包含 symmetric LLS 的 full-cloud dispatch、normal equation 构造或 RVV intrinsic 实现。

当前 production 不覆盖：

- 非 normal 点类型或 traits 不完整的自定义点类型。
- `Scalar=double` 的模板边界。
- source indices + target indices 的双侧 indexed 公开入口。
- correspondences 公开入口。

这些缺口不阻塞当前 `generic-normal-full-cloud-and-source-indexed` production-ready，因为 PI2 分流用 traits / Scalar / overload / runtime gate 把它们隔离到标量 fallback（回退路径）。它们仍阻止把结论扩大成 `Scalar=double`、dual-indices 或 correspondences production-ready。

## 4. 详细设计

| 暂存路径（staging） | 结构 / helper | 字段来源 | 下一段消费者 / tail | gate |
| --- | --- | --- | --- | --- |
| 全云 symmetric row | production helper 模板 / `accumulate_candidate_full` | production 使用 traits offset 读取 source/target `x/y/z/normal_x/normal_y/normal_z`；diagnostic 仍固定 `PointNormal` AoS（结构数组）布局 stride load（跨步加载） | `accumulate_staged_rows` 或 production 等价尾段压缩后累加 `ATA/ATb`，之后 Eigen solve | `n >= 64`、`vlmax_e32m2 <= 64`、32-bit byte offset、generic normal traits gate |
| source 单侧 indexed row | production `SymmetricSourceIndexedRowSource` / test-rvv `SourceIndexedRowSource` | source index vector + source gather；target 使用紧凑 full-cloud stride load | 共用 row pipeline，压缩后累加 `ATA/ATb` | production 覆盖 `Scalar=float` + generic normal traits gate + valid-index precondition；test-rvv 继续保留诊断 candidate |
| 双侧 indexed row | `accumulate_candidate_dual_indices` | source/target 两个独立 index vector；两侧 gather | 同一 `accumulate_staged_rows` | test-rvv valid-index-only 诊断；不接 production |
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

本轮两个 indexed 消融的 VL chunk 形态：

```text
source indices:
  -> vle32.v load source indices
  -> vluxei32.v gather source xyz/normal
  -> vlse32.v stride load target xyz/normal
  -> 复用同一套 normal-select / mask / formula / compress / tail

dual indices:
  -> vle32.v load source indices
  -> vle32.v load target indices
  -> vluxei32.v gather source xyz/normal
  -> vluxei32.v gather target xyz/normal
  -> 复用同一套 normal-select / mask / formula / compress / tail
```

这种拆分的证据边界要看清：全云路径的取数形态较规则；source 单侧 indexed 只增加一侧 gather；双侧 indices 有两侧 gather，本轮使用独立 target index stream 来覆盖 row 配对语义，但没有 correspondence parsing；correspondences 路径天然多了 query/match 展开和临时 index vector 组织。indexed 消融只覆盖有效 index stream；标量参考 helper 对负数或越界 index 的跳过是诊断防御，不是 production 非法 index 语义，因为 RVV candidate 不做同等过滤。板卡上的 correspondences 退化只能说明当前组合不适合生产接入，不能单独证明 gather、展开、压缩或 tail 是唯一主因。

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

`vcompress + buffer + tail` 被用于当前 production direct 和 diagnostic 方案。它保留有效 lane 的扫描顺序，让 invalid lane 被剔除后仍对应 production 的 `continue` 语义；代价是额外 store/load 和 buffer tail 成本。板卡 production-direct full-cloud `PointNormal` 为 `2.71x` / `2.70x`，`PointXYZINormal` 为 `2.71x` / `2.48x`；production-direct source-indexed `PointNormal` 为 `2.11x` / `1.87x`，`PointXYZINormal` 为 `2.10x` / `1.90x`。这说明该成本在两条已接入公开入口上可被收益覆盖；但 dual-indices 和 correspondences 仍为负向或不稳定，不能直接扩展到双侧 indexed 或完整 correspondences row。

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

bench 输入在计时前构造。source 是确定性解析二次曲面 `PointNormal`，target 由 `pcl::transformPointCloudWithNormals` 使用温和刚体变换生成，并固定翻转部分 target normal 来覆盖同向法线分支。`PointXYZINormal` production-direct case 由同一组几何点和 normal 字段转换得到，只改变 AoS layout（结构数组布局）和字段 offset，用来验证 traits-gated generic normal dispatch（泛型 normal 分流）而不是更换数据集。indexed 消融使用有效、非连续且含重复的 `pcl::Indices`。source-indices case 在计时前把 target 压成紧凑全云，使计时内只有 source gather 和 target stride load；dual-indices case 直接传 source/target 两条不同的 index vector，不包含 correspondence parsing 或 weight 展开。correspondences 是确定性 subset，包含非连续和重复 index；它主要用于暴露完整对应关系入口形态，不代表所有真实 correspondence 分布。

测量包含 normal-equation 构造、Eigen LDLT solve 和 4x4 矩阵构造。correspondences case 包含 candidate 内部 index 展开成本，因为当前 gather 方案必须付出这部分入口成本。

| case | 入口 | 规模 | 是否命中 RVV | 证明点 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- |
| `symmetric lls production-direct full-cloud pointnormal 65536` | 真实 public estimator 全云入口 | 64K | RVV 构建命中 production helper | full-cloud `PointNormal` / `float` 生产分流收益。 | 不证明 indexed 入口。 |
| `symmetric lls production-direct full-cloud pointxyzinormal 65536` | 真实 public estimator 全云入口 | 64K | RVV 构建命中 production helper 模板实例 | `PointXYZINormal` 泛型 normal layout 的 traits-gated 生产分流收益。 | 不证明所有自定义点类型或 indexed 入口。 |
| `symmetric lls full-cloud pointnormal 65536` | test-rvv diagnostic 全云顺序扫描 | 64K | RVV 构建命中 diagnostic helper | 诊断 wrapper 与生产分流的同构对照。 | 不代表真实 public estimator overhead。 |
| `symmetric lls source-indices pointnormal 65536` | source indices + target full-cloud production direct | 64K 输入点 subset | 真实 public estimator 命中 `SourceIndexedRowSource` | 单侧 source gather + target stride 的 production direct 板卡收益。 | 不证明双侧 gather、correspondences 或非法 index 行为。 |
| `symmetric lls dual-indices pointnormal 65536` | source indices + target indices 诊断 | 64K 输入点 subset | RVV 构建命中两条独立 index stream 的双侧 gather | 双侧 gather 成本和 row 配对语义，不含 correspondence parsing / weight 展开。 | 不证明完整 correspondences、非法 index 或 production indexed dispatch。 |
| `symmetric lls correspondences pointnormal 65536` | 对应关系索引 | 64K 输入点 subset | RVV 构建命中 | index 展开 + gather 路径 correctness 和负向性能信号。 | 不能单独归因 gather、展开、压缩或 tail 哪一项最慢。 |
| `symmetric lls production-direct full-cloud pointnormal 262144` | 真实 public estimator 全云入口 | 256K | RVV 构建命中 production helper | 放大规模后的生产分流收益。 | 不证明 indexed row。 |
| `symmetric lls production-direct full-cloud pointxyzinormal 262144` | 真实 public estimator 全云入口 | 256K | RVV 构建命中 production helper 模板实例 | 放大规模后泛型 normal layout 仍有生产收益。 | 不证明 indexed row 或 `Scalar=double`。 |
| `symmetric lls full-cloud pointnormal 262144` | test-rvv diagnostic 全云顺序扫描 | 256K | RVV 构建命中 diagnostic helper | 放大规模后的诊断收益。 | 不代表泛型 production。 |
| `symmetric lls source-indices pointnormal 262144` | source indices + target full-cloud production direct | 256K 输入点 subset | 真实 public estimator 命中 `SourceIndexedRowSource` | 放大规模后单侧 source gather + target stride 的 production direct 板卡收益。 | 不证明双侧 gather、correspondences 或非法 index 行为。 |
| `symmetric lls dual-indices pointnormal 262144` | source indices + target indices 诊断 | 256K 输入点 subset | RVV 构建命中两条独立 index stream 的双侧 gather | 放大规模后双侧 gather 成本和 row 配对语义，不含 correspondence parsing / weight 展开。 | 不证明完整 correspondences、非法 index 或 production indexed dispatch。 |
| `symmetric lls correspondences pointnormal 262144` | 对应关系索引 | 256K 输入点 subset | RVV 构建命中 | 大规模 indexed row 的负向性能证据。 | 不代表所有真实 correspondence 分布，也不能确认退化主因。 |

## 9. 测试、QEMU、反汇编和板卡证据

- QEMU test：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_test_compare`，std/RVV 各 25 个测试通过，其中覆盖 production direct source-indices、source-indexed `Scalar=double` fallback、source-indexed mixed layout、source-indexed invalid lane、独立 target index stream 的 dual-indices、accepted_points、finite mask 和矩阵结果。
- QEMU bench：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_bench_compare`，日志可解析，checksum 基本对齐；QEMU 不作为性能结论。
- 反汇编：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls dump_bench_rvv`，摘要在 `output/qemu/rvv_asm_check.log`。摘录确认 public full-cloud 和 source-indices estimator 分别调用 `SymmetricFullCloudRowSource` 与 `SymmetricSourceIndexedRowSource` 的共享 production helper；source-indexed helper 中可见 `vle32.v`、`vmul.vx`、source `vluxei32.v` 和 target `vlse32.v`。dual-indices 仍是 test-rvv diagnostic lambda，可见两条独立 index load 和 source/target 两组 `vluxei32.v`；这不证明 dual-indices production dispatch。
- 板卡：`make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls board_smoke` 已通过专项测试和 bench compare，真实性能日志在 `output/board/analyze_bench_compare.log`。

板卡结果如下：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| production-direct `PointNormal` full-cloud 64K | 16.0400 | 5.9216 | 2.71x | 真实公开入口收益成立，接 production。 |
| production-direct `PointXYZINormal` full-cloud 64K | 16.0146 | 5.9035 | 2.71x | 泛型 normal traits-gated 入口收益成立，接 production。 |
| production-direct `PointNormal` source-indices 64K | 12.0123 | 5.6970 | 2.11x | 有效 source index stream 下真实公开入口收益成立，接 production。 |
| production-direct `PointXYZINormal` source-indices 64K | 11.9900 | 5.7087 | 2.10x | 泛型 normal source-indexed 入口收益成立，接 production。 |
| diagnostic full-cloud 64K | 9.5015 | 5.5691 | 1.71x | 诊断链路仍有收益，但不是最终生产结论。 |
| diagnostic source-indices 64K | 7.9827 | 6.2866 | 1.27x | 单侧 gather 诊断仍提供取数形态对照；production 结论以真实 public source-indexed case 为准。 |
| diagnostic dual-indices 64K | 8.7335 | 10.9079 | 0.80x | 双侧 gather 和独立 row 配对语义在 64K 转负；不接 production。 |
| diagnostic correspondences 64K | 7.8728 | 10.2090 | 0.77x | 完整 correspondences 组合仍退化，不接 production。 |
| production-direct `PointNormal` full-cloud 256K | 63.9674 | 23.7303 | 2.70x | 放大规模后生产收益仍稳定。 |
| production-direct `PointXYZINormal` full-cloud 256K | 64.0034 | 25.7566 | 2.48x | 泛型 normal layout 放大规模后仍有收益。 |
| production-direct `PointNormal` source-indices 256K | 47.7063 | 25.4518 | 1.87x | 放大规模后有效 source index stream 仍有 production direct 收益。 |
| production-direct `PointXYZINormal` source-indices 256K | 47.8044 | 25.2074 | 1.90x | 泛型 normal source-indexed 放大规模后仍有收益。 |
| diagnostic full-cloud 256K | 37.9238 | 28.5084 | 1.33x | 诊断链路收益仍在。 |
| diagnostic source-indices 256K | 31.9641 | 25.3826 | 1.26x | 单侧 gather 诊断仍有正向信号；production 结论以真实 public source-indexed case 为准。 |
| diagnostic dual-indices 256K | 35.2729 | 55.5724 | 0.63x | 双侧 gather 和独立 row 配对语义在 256K 转负；不接 production。 |
| diagnostic correspondences 256K | 31.6106 | 36.5483 | 0.86x | 完整 correspondences 组合仍退化。 |

证据日志保留在 `test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls/output/`、`build/asm/` 和 `log/`，不作为默认提交内容。

## 10. 反汇编热点归属

`estimateSymmetricPointNormalRowsRVV<RowSourcePolicy>` 是当前 production direct 共享热点模板。public full-cloud estimator 的反汇编调用点能跳到 `SymmetricFullCloudRowSource<PointNormal, PointNormal>` 和 `SymmetricFullCloudRowSource<PointXYZINormal, PointXYZINormal>` 实例；source-indices public overload 能跳到对应的 `SymmetricSourceIndexedRowSource` 实例。full-cloud helper 的 `vlse32.v` 对应 traits offset 字段 stride load，source-indexed helper 还包含 `vle32.v` index load、`vmul.vx` byte offset 和 source `vluxei32.v` gather。`vcompress.vm` 和 `vcpop.m` 对应有效 lane 压缩，`vfredosum.vs` 对应编译器自动生成的 buffer tail partial vector accumulation。

`accumulate_candidate_full` 仍是 full-cloud 诊断热点符号，用于和生产 direct 对照。`accumulate_staged_rows` 中的 `vcompress.vm` 和 `vcpop.m` 对应有效 lane 压缩，`vfredosum.vs` 对应编译器自动生成的 buffer tail partial vector accumulation。

`SourceIndexedRowSource` 在 production 中通过 `estimateSymmetricPointNormalRowsRVV` 直接服务真实 source-indices overload；bench 中的同名 diagnostic policy 仍用于取数形态对照。production source-indexed 范围可见 `vle32.v` 读取 index、`vmul.vx` 形成 byte offset、`vluxei32.v` 读取 source 字段以及 `vlse32.v` 读取 target 字段。`accumulate_candidate_dual_indices` 仍是独立诊断实现；其输入来自两条不同的 source/target index vector，反汇编范围记录两条 index load 和两侧 `vluxei32.v`。这条证据证明 source-indexed production path 和 dual-indices diagnostic 的取数形态按设计命中，但不证明 dual-indices production dispatch。

`accumulate_candidate_correspondences` 符号存在，全二进制摘录可见标量 query/match 展开后进入 `vluxei32.v` gather。当前摘要足以证明 RVV 二进制存在 gather 形态，但如果后续要做更强归因，仍需按符号范围输出 gather、vcompress、vfredosum 和 FMA 的细分统计。

当前反汇编和板卡证据组合支持三条结论：full-cloud generic normal / `float` production direct 有稳定板卡收益；source indices + target full-cloud 的 `Scalar=float` production direct 在有效 index stream 下也有板卡收益；双侧 gather 成本可以与 correspondences 额外展开成本分离，但独立 target index stream 后 dual-indices 仍在板卡上转负。它不能支持“correspondences 慢的主因就是 gather”或“buffer + tail 是唯一瓶颈”这类单因归因。要闭合负向归因，还需要新增消融 bench：只做 correspondence 展开不 gather、连续 index 但保留 gather 形态、去掉 `vcompress` 的 masked reduction 原型、固定或禁用自动 `vfredosum` tail，以及真实 ICP correspondence 分布 profile。

## 11. 生产接入评估

`EvidenceDecision = production-ready/generic-normal-full-cloud-and-source-indexed`。

接入范围：

- 真实 public full-cloud overload。
- 真实 public source indices + target full-cloud overload，前提是 `indices_src.size() == cloud_tgt.size()` 且调用方提供有效 index stream。
- `PointSource` 和 `PointTarget` 分别满足 generic normal traits gate：`x/y/z/normal_x/normal_y/normal_z` 均为单个 `float` 字段，POD / standard-layout、`sizeof(PointT) == sizeof(POD)` 和字段 offset 对齐成立。
- `Scalar=float`。
- `__RVV10__` 打开、`n >= 64`、`vlmax_e32m2 <= 64`、32-bit byte offset 可表达。

fallback 范围：

- 非 RVV 构建。
- 小输入或 VLEN/buffer gate 不满足。
- `Scalar=double`。
- traits 不满足或 layout gate 不满足的点类型。
- source+target indices 和 correspondences overload。

source indices + target full-cloud 已按 production direct 证据接入，但这是 valid-index-only 合同：RVV helper 不做负数或越界 index 过滤，继承 PCL 公开入口的有效索引前提。source+target indices 和 correspondences 路径都不接 production。本轮 indexed 消融说明双侧 gather 成本可与 correspondence 展开分离；dual-indices correctness 和 bench 使用了独立 target index stream，但板卡 `0.80x` / `0.63x` 已经转负，没有 production dispatch、generic normal indexed gate 或 fallback 矩阵。correspondences 板卡仍为负向，退化主因保持为多因素待消融假设；文档不把它归因到单一原因。

## 12. Production integration closeout

本轮按 handoff 完成 production integration loop（生产接入闭环）。PI1 冻结的范围是两条已获板卡正向证据的数据流：全云连续入口，以及 source indices + target full-cloud 入口：

```cpp
estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                            const pcl::PointCloud<PointTarget>& cloud_tgt,
                            Matrix4& transformation_matrix) const

estimateRigidTransformation(const pcl::PointCloud<PointSource>& cloud_src,
                            const pcl::Indices& indices_src,
                            const pcl::PointCloud<PointTarget>& cloud_tgt,
                            Matrix4& transformation_matrix) const
```

PI2 production patch（生产补丁）把 full-cloud dispatch（分流逻辑）和 source-indexed dispatch 都接到同一个 `estimateSymmetricPointNormalRowsRVV<RowSourcePolicy>` 后半段。full-cloud 使用 `SymmetricFullCloudRowSource`，source indices + target full-cloud 使用 `SymmetricSourceIndexedRowSource`。后续结构收口把公开 overload 整理为“size check / indices size check -> RVV 短路 -> `estimateSymmetricPointNormal*Std` fallback”，使标量 fallback 与 RVV 主路径都具有稳定 helper 名称。source indices + target indices 和 correspondences 两个公开 overload 不接 RVV，直接调用对应 `Std` helper。dual-indices 使用独立 target index stream 后已转负，不能替代 production direct 证据；correspondences 的退化仍是 index 展开、gather、压缩、tail、row 分布和入口展开等多因素待消融假设。

PI2 采用受 traits gate 约束的泛型 normal production patch，而不是无条件泛型接入：

| gate（门禁） | 当前 production 行为 | fallback（回退路径） |
| --- | --- | --- |
| 编译宏 | 只在 `__RVV10__` 下编译 RVV helper | 非 RVV 构建完全保留当前标量 helper。 |
| 点类型 | `PointSource` 和 `PointTarget` 分别通过 `pcl::traits::has_xyz`、`has_normal`、`datatype`、`offset`、POD / standard-layout 和 alignment gate | traits 不完整、字段不是单个 `float`、POD 大小不匹配或 offset 不对齐时走标量。 |
| `Scalar` | 只允许 `Scalar=float` | `Scalar=double` 继续走标量。 |
| 入口形态 | 允许 full-cloud，以及 source indices + target full-cloud | source+target indices 和 correspondences overload 不分流。 |
| source-indexed 输入合同 | `indices_src.size() == cloud_tgt.size()` 后才尝试；source index stream 必须有效 | RVV 不做负数或越界过滤；非法 index 行为不是本轮 production 语义。 |
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
  -> estimateSymmetricPointNormalFullCloudStd(...)

source-indices public overload
  -> size check
  -> #if defined(__RVV10__)
       try estimateSymmetricPointNormalSourceIndicesRVV(...)
       if true: return
     #endif
  -> estimateSymmetricPointNormalSourceIndicesStd(...)

source+target indices / correspondences public overload
  -> size check when the overload has paired indices
  -> estimateSymmetricPointNormal*Std(...)
```

RVV helper 只接管逐点热点：row-source policy 先按 stride 或 gather 读取 source/target xyz 和 normal，公共后半段完成同向 normal 选择、finite mask、`(p + q).cross(n)` 和 `(q - p).dot(n)` staging，然后按压缩后的有效 lane 累加 normal equation。Eigen LDLT solve 和 4x4 matrix 构造仍在 helper 内保持标量结构。

PI3 production direct tests（生产直连测试）已新增真实公开入口测试，而不是只调用 test-rvv diagnostic wrapper：

- full-cloud `PointNormal` / `float`：RVV build 调用真实 estimator，矩阵与 std build 或同构 reference 在容差内一致。
- full-cloud `PointXYZINormal` / `float`：真实 estimator 与 `PointNormal` reference 对拍，证明 traits offset / stride / normal 字段映射成立。
- source-indices `PointNormal` / `float`：真实 estimator 与 source-indexed diagnostic candidate 对拍，证明 public overload 命中 production policy。
- source-indices `PointXYZINormal` / `float`：真实 estimator 与 `PointNormal` source-indexed reference 对拍，证明同布局 generic normal source-indexed offset 映射成立。
- source-indices mixed layout：`PointXYZINormal -> PointNormal` 真实 estimator 与 `PointNormal` source-indexed reference 对拍，证明 source gather 和 target stride 分别使用两侧 traits offset。
- source-indices invalid lane：indexed source NaN 和紧凑 target Inf 与标量 reference 一致，证明 finite mask 没有被新 row-source policy 破坏。
- source-indices `Scalar=double` fallback：真实 source-indexed overload 使用 `Matrix4d` 与标量 reference 对拍，确认新分流不越过 `Scalar=float` gate。
- full-cloud mixed layout：`PointXYZINormal -> PointNormal` 真实 estimator 与 `PointNormal` reference 对拍，证明 source/target layout gate 分别计算。
- `setEnforceSameDirectionNormals(false)`：真实 estimator 命中同一 full-cloud 分流并保持关闭 gate 的语义。
- full-cloud invalid lane：source/target xyz 或合成 normal 含 NaN/Inf 时，真实 estimator 与标量 reference 一致。
- small input fallback：`n < 64` 仍走标量并输出一致。
- indexed fallback：source+target indices 和 correspondences 真实 overload 继续输出标量结果，不因 source-indexed 分流而改变。
- `Scalar=double` fallback：编译并运行 full-cloud 与 source-indexed double case，确认不进入 RVV 分流。

PI4 production evidence rerun（生产证据重跑）已执行：

```text
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_test_compare
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls run_bench_compare
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls dump_bench_rvv
make -C test-rvv/registration/transformation_estimation_symmetric_point_to_plane_lls board_smoke
```

bench 包含 `PointNormal` 和 `PointXYZINormal` production direct case，分别调用真实 public estimator 的 full-cloud 和 source-indices overload。本轮保留 `source-indices` 与 `dual-indices` test-rvv diagnostic case，其中 dual-indices 使用独立 target index stream。反汇编摘要证明 production full-cloud helper 命中 `vlse32.v`，production source-indexed helper 命中 source `vle32.v` / `vmul.vx` / `vluxei32.v` 与 target `vlse32.v`，并标注 `vfredosum.vs` 仍来自压缩 buffer tail；dual-indices 诊断 lambda 命中双侧 gather。板卡结果已重新生成：`PointNormal` production-direct full-cloud 64K / 256K 为 `2.71x` / `2.70x`，`PointXYZINormal` full-cloud 为 `2.71x` / `2.48x`；`PointNormal` source-indices 为 `2.11x` / `1.87x`，`PointXYZINormal` source-indices 为 `2.10x` / `1.90x`；dual-indices 诊断为 `0.80x` / `0.63x`，correspondences 为 `0.77x` / `0.86x`。

PI5 结论：`production-ready/generic-normal-full-cloud-and-source-indexed`。它不是所有 symmetric LLS 入口 production-ready；`Scalar=double`、dual-indices 和 correspondences 路径保持标量，不能声称 correspondences 退化主因已经定位。

公共 trait 重构提供了 layout 判断和 32-bit byte offset helper；本轮 source-indexed 接入复用这些 gate。production 行为仍限定在 full-cloud 与 source indices + target full-cloud、`Scalar=float`、generic normal traits gate 命中时尝试 RVV；source+target indices、correspondences 和 `Scalar=double` 继续按本节 fallback 走标量。

## 13. 后续方向

本轮完成 symmetric LLS 的 generic normal full-cloud 和 source-indexed production closeout，并保留 dual-indices 与 correspondences 的 test-rvv 诊断边界。结论不是“整个 symmetric LLS 泛型入口已可生产接入”，而是“full-cloud 与 source indices + target full-cloud 的 `Scalar=float` 且 source/target 满足 generic normal traits gate 的路径已接入 production；dual-indices 和 correspondences 路径不接 production”。

后续如果要扩大范围，必须重新走候选评估：`Scalar=double` 需要重新定义 RVV double 公式和误差边界；source-indexed 仍可继续补更多真实 ICP 输入和非法 index 非覆盖声明，但当前 valid-index-only production 已闭合；dual-indices 只有在独立 target index stream 证据和板卡收益都成立后，才值得作为后续候选讨论；correspondences 的负向归因仍需消融 bench，不应把退化写成单一原因。
