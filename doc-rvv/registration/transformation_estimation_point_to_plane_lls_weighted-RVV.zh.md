# registration/transformation_estimation_point_to_plane_lls_weighted RVV 诊断说明

## 收尾摘要

`TransformationEstimationPointToPlaneLLSWeighted::estimateRigidTransformation` 本轮完成 clean topic（干净主题）诊断重建，不修改 production（生产源码）。当前产物是 test-rvv diagnostic（测试专用诊断）和 bench（性能测试）骨架：它覆盖 weighted PointNormal 的全云顺序扫描（full-cloud）和对应关系索引路径（correspondences）的字段读取、finite mask（有限值掩码）、weight load（权重加载）、`weight * normal`、`a/b/c/d` 公式、压缩 buffer tail（临时缓冲尾段）和 Eigen solve（Eigen 求解器）边界。

QEMU correctness（QEMU 正确性验证，不代表真实性能）通过，bench 输出合同可解析，反汇编证明当前 RVV helper 命中 `vlse32.v`、`vluxei32.v`、`vle32.v`、`vcompress.vm`，并暴露了 `vfredosum.vs` 自动 partial vector accumulation（部分向量累加）。板卡 `board_smoke` 已在 Milkv-Jupiter 上通过专项测试，但性能只在全云顺序扫描 64K 有弱收益 `1.18x`，全云 256K 退化到 `0.92x`，对应关系索引路径退化到 `0.50x` / `0.45x`。本主题当前结论是 bench-only/no-production（仅保留性能诊断、不接入生产）。

## 1. 函数入口作用

该入口用于 point-to-plane ICP（点到平面迭代最近点）中的 weighted LLS（带权线性最小二乘）变换估计。公开重载支持：

- 全云 source/target，对应权重来自 `weights_`。
- source indices + target 全云，对应权重来自 `weights_`。
- source indices + target indices，对应权重来自 `weights_`。
- 对应关系入口（correspondences），对应权重来自每个 `pcl::Correspondence::weight` 字段。

所有公开重载最终进入同一个 protected helper：

```text
source / target iterator + weights iterator
  -> finite check
  -> weight * normal
  -> per-pair a,b,c,d formula
  -> accumulate ATA / ATb
  -> Eigen inverse solve
  -> construct 4x4 transform
```

`constructTransformationMatrix` 每次 estimate 调用只执行一次；它不随点数增长，因此不是本轮首要 RVV 化对象。

从源码看，公开入口的输入形态不止一种，但进入 protected helper 后会被 `ConstCloudIterator`（常量点云迭代器）统一成同步前进的三元组：当前 source 点、当前 target 点和当前 weight。全云入口是一一对应扫描；indices（索引）入口通过一个或两个 index 列表间接取点；correspondences（对应关系）入口通过 `index_query/index_match` 取点，并把 `correspondence.weight` 复制成局部权重 vector。也就是说，production 源码在 helper 内看到的是统一逐点流，而不是显式的 stride 或 gather 数据流。

本轮 RVV 诊断为了让取数成本可审查，把这个统一逐点流拆成两条显式路径：全云顺序扫描路径（full-cloud data flow）和对应关系索引扫描路径（correspondences data flow）。这两条路径都映射到真实公开入口，但它们是 test-rvv 中的诊断实现形态，不是已经接入 production 的分流。

## 2. 标量路径与诊断边界

标量实现先检查 source `x/y/z`、target `x/y/z` 和 target `normal_x/y/z` 是否有限。当前 production 不检查 weight 是否有限；若 weight 是 NaN/Inf，生产路径会继续用它参与计算。测试中的 invalid lane case（无效 lane 样本）因此只注入点和 normal 的 NaN/Inf，不把 weight 有限性误写成生产语义。

有效点使用加权 normal：

```text
nx = target.normal_x * weight
ny = target.normal_y * weight
nz = target.normal_z * weight

a = nz * sy - ny * sz
b = nx * sz - nz * sx
c = ny * sx - nx * sy
d = nx * dx + ny * dy + nz * dz - nx * sx - ny * sy - nz * sz
```

随后上三角累加 21 个 `ATA` 项，累加 6 个 `ATb` 项，循环结束后补齐 `ATA` 下三角，再执行一次 `ATA.inverse() * ATb` 和一次 4x4 矩阵构造。本轮 diagnostic 不修改公开 API（应用程序接口），也不证明真实 production dispatch（分流逻辑）。

## 3. 覆盖范围与 fallback

当前 diagnostic 覆盖：

- `pcl::PointNormal` + `float` 权重全云路径；
- 对应关系索引路径的乱序和重复 index；
- 小规模 `n < 64` fallback（回退路径）；
- source/target 坐标和 target normal 的 NaN/Inf finite mask；
- `vlmax_e32m2 <= 64` 固定 buffer gate（固定缓冲验收条件）；
- source/target 点数量可转 32-bit byte offset（32 位字节偏移）。

当前不覆盖：

- production 泛型点类型 traits；
- `Scalar=double` 的权重转换和矩阵输出边界；
- weight NaN/Inf 的公开语义测试；
- 真实 production 入口分流；
- 真实 production 入口接入后的板卡性能。

## 4. 详细设计

| 暂存路径 | 结构 / helper | 字段来源 | 下一段消费者 / tail | gate |
| --- | --- | --- | --- | --- |
| 全云顺序扫描暂存（full-cloud weighted staging） | `accumulate_candidate_full` | source `x/y/z`、target `x/y/z`、target `normal_x/y/z`、连续 `weights[i]` | `accumulate_staged_rows` 的 buffer tail，之后 Eigen solve | `n >= 64`、`vlmax_e32m2 <= 64`、32-bit byte offset |
| 对应关系索引暂存（correspondences weighted staging） | `accumulate_candidate_correspondences` | 标量展开后的 source index、target index、weight，再 gather source/target 字段 | 同上 | 有效 correspondence 数量 `>= 64`，且 source/target byte offset 可表示 |

两条数据流的本质区别在 row（法方程的一行贡献）如何被枚举出来。全云顺序扫描路径直接按数组下标 `i` 扫描，`source[i]`、`target[i]` 和 `weights[i]` 天然组成同一行；RVV 只需要在 AoS（结构数组）布局上做 stride load（跨步加载），再连续读取权重。对应关系索引路径则以 `pcl::Correspondence` 列表为真实入口形态，row 顺序由 correspondence 列表决定：先把 `index_query`、`index_match` 和 `weight` 展开成连续数组，再把 index 转成点云内的字节偏移，用 gather（离散加载）读取 source/target 字段。

这意味着全云顺序扫描的访存模式相对规则，主要审查跨步加载、finite mask（有限值掩码）、`weight * normal`、公式 staging、`vcompress`（向量压缩）和 buffer tail（缓冲尾段）。对应关系索引路径复用后半段公式和压缩逻辑，但额外承担标量展开、offset 计算和不规则 gather 访存；它能证明 indexed row（带索引行）路径的 correctness（正确性）和日志形状，却不能把性能退化单独归因到某一个步骤。

全云顺序扫描的 VL chunk（可变向量长度分块）数据流：

```text
vsetvl
  -> vlse32.v 跨步加载 source xyz
  -> vlse32.v 跨步加载 target xyz
  -> vlse32.v 跨步加载 target normal
  -> vle32.v 连续加载 weight
  -> 基于 xyz/normal 构造 finite mask
  -> weight * normal
  -> 计算 a/b/c/d 公式
  -> vcpop + vcompress 写入 64-lane buffers
  -> buffer tail 累加
  -> Eigen solve + construct matrix
```

对应关系索引路径的 VL chunk 数据流多一个标量展开阶段：

```text
扫描 correspondences
  -> 展开 src_indices / tgt_indices / weights vectors
  -> vsetvl
  -> vle32.v 连续加载 indices 和 weight
  -> 计算 byte offset
  -> vluxei32.v gather source/target fields
  -> 复用同一套 mask/formula/compress/tail
```

反汇编显示 buffer tail 在 `-O3` 下被 GCC 自动向量化成 `vfredosum.vs` 规约。这意味着当前候选不是纯 scalar tail（标量尾段），而是“手写 staging + 自动 partial vector accumulation”。该行为保留在诊断文档中供 reviewer 审查，不作为 production 可维护承诺。

## 5. 实现选择审计

直接向量规约暂缓。它可能消除 buffer 写回和 tail 成本，但会改变 `ATA/ATb` 的跨 lane 累加树。由于 6x6 solve 会放大某些 normal-equation 差异，生产接入前必须有 adversarial case（对抗边界样本）、误差预算和目标硬件收益。

部分 vector accumulation 被诊断性采用。源码是压缩 buffer 后的 C++ tail；反汇编显示编译器自动生成 `vfredosum.vs`。它能观察“压缩后再规约”的潜在方向，但语义可控性弱，编译器和 flags（编译选项）变化可能改变结果。若下一轮要推进，应手写可控 intrinsic 或明确禁用自动规约并做消融对比。

fused multiply-add（融合乘加）intrinsic 暂缓。手写公式保留 `vfmul` + `vfadd/vfsub`，让 `weight * normal` 和 `a/b/c/d` 容易审查。全二进制有 `vfmadd/vfmacc`，但多处来自 Eigen、自动向量化或 bench 周边，不能写成手写 staging 已采用 FMA。若改用 FMA，需要 same-chain（同构链路）测试和热点 asm 证明。

数学函数和矩阵构造暂缓。`constructTransformationMatrix` 和 `sin/cos` 每次 estimate 只调用一次；Eigen solve 是固定 6x6 小矩阵。除非 profile（性能剖析）显示它们成为主热点，否则手写 RVV 数学函数维护成本大于潜在收益。

这些取舍都不是永久结论。当前没有证据证明 buffer + tail 优于直接向量规约，也没有证据证明 FMA（fused multiply-add，融合乘加）会破坏业务可接受结果；它们只是本轮在没有误差预算、消融 bench 和逐符号反汇编归属前的保守边界。下一轮若要继续，应把实现选择本身当成诊断对象，而不是默认接受当前方案。

## 6. 数值算例与 VL chunk

假设一个 chunk 有 4 个 lane，lane 2 的 target normal 是 NaN：

```text
lane:      0      1      2      3
finite:   true   true   false  true
weight:   0.55   0.62   0.69   0.76
```

RVV 会先加载权重并为所有 lane 计算候选公式，但 keep mask（保留掩码）只有 `1,1,0,1`。`vcompress` 后 buffer 顺序是 lane `0,1,3`，与标量循环遇到 lane 2 时 `continue` 的可见顺序一致。区别在于当前编译器可能把 buffer 中若干项再做 `vfredosum`，这改变了尾段累加树，因此只能用专项容差和后续板卡/对抗样本继续审查。

一个单点公式例子：

```text
source = (1, 2, 3)
target = (1.1, 1.9, 3.2)
normal = (0, 0, 1)
weight = 0.5
weighted normal = (0, 0, 0.5)
a = 0.5 * 2 - 0 * 3 = 1
b = 0 * 3 - 0.5 * 1 = -0.5
c = 0 * 1 - 0 * 2 = 0
d = 0.5 * 3.2 - 0.5 * 3 = 0.1
```

该 lane 对 `ATb` 的贡献是 `[0.1, -0.05, 0, 0, 0, 0.05]`，对 `ATA` 的贡献进入 6x6 上三角。

## 7. Bench case 说明

输入在计时前构造：source 是确定性解析曲面，target 由 `transformPointCloudWithNormals` 生成；full-cloud 权重是周期序列；correspondences 是确定性 subset，包含非连续和重复 index。correspondences case 的 `index_query` 和 `index_match` 当前保持相同点号，主要用于暴露 indexed row、weight 展开和 gather 形态，不代表所有真实 correspondence 分布。

测量包含：normal-equation 构造、Eigen solve 和 `constructTransformationMatrix`。full-cloud 不包含权重生成；correspondences 包含 candidate 内部 index/weight 展开，因为这是当前 gather 方案的必要入口成本。

| case | 入口 | 规模 | 是否命中 RVV | 证明点 | 不能证明什么 |
| --- | --- | --- | --- | --- | --- |
| 全云顺序扫描 64K | `estimate_candidate_full` | 65536 | RVV 构建命中 | 中等规模 stride load、weight load、mask、compress/tail。 | 不证明 production dispatch、泛型点类型和目标硬件性能。 |
| 对应关系索引 64K | `estimate_candidate_correspondences` | 65536 输入点 subset | RVV 构建命中 | index/weight 展开 + gather 路径的 correctness 和日志形状。 | 不能单独归因 gather、展开、压缩或 tail 哪一项最慢，也不能代表任意 target pairing。 |
| 全云顺序扫描 256K | `estimate_candidate_full` | 262144 | RVV 构建命中 | 放大后检查 QEMU 日志和 checksum 稳定性。 | QEMU timing 不代表性能。 |
| 对应关系索引 256K | `estimate_candidate_correspondences` | 262144 输入点 subset | RVV 构建命中 | 大规模 indexed row 日志形状和 checksum。 | 不代表所有真实 correspondence 分布，也不能确认退化主因。 |

## 8. 测试、QEMU、反汇编和板卡证据

- QEMU test：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_compare`，std/RVV 各 6 个测试通过。
- QEMU bench：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_bench_compare`，日志可解析，checksum 基本对齐；QEMU 不作为性能结论。
- 反汇编：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted dump_bench_rvv`，helper 区域命中 stride load、contiguous weight load、gather、`vcompress` 和自动 `vfredosum`。
- 板卡：`make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted board_smoke` 已通过专项测试和 bench compare，真实性能日志在 `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/board/analyze_bench_compare.log`。

板卡结果如下：

| case | Std ms/iter | RVV ms/iter | speedup | 结论 |
| --- | ---: | ---: | ---: | --- |
| 全云顺序扫描 64K | 7.1234 | 6.0440 | 1.18x | 弱收益，只覆盖中等规模连续输入。 |
| 对应关系索引 64K | 5.2553 | 10.4137 | 0.50x | index/weight 展开、gather、compress/tail 组合明显退化；主因尚未拆分定位。 |
| 全云顺序扫描 256K | 28.0058 | 30.3425 | 0.92x | 放大规模后退化，64K 弱收益不稳定。 |
| 对应关系索引 256K | 20.9287 | 46.2499 | 0.45x | 大规模 indexed row 路径退化更明显；仍不能单独归因 gather 或 buffer。 |

证据日志保留在 `test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/output/`、`build/asm/` 和 `log/`，不作为默认提交内容。

## 9. 反汇编热点归属

`accumulate_candidate_full` 是当前 helper 热点符号：`vlse32.v` 对应 `PointNormal` 字段 stride load，`vle32.v` 对应连续 weight load，`vfmul/vfadd/vfsub` 对应 weighted formula staging，`vcpop.m` 和 `vcompress.vm` 对应有效 lane 压缩。

`vfredosum.vs` 位于 helper 符号内，对应压缩 buffer tail 的自动向量化规约。它不是手写 direct raw-lane reduction，也不是 Eigen solve 的指令。

全二进制中大量 `vfmadd/vfmacc` 不能直接归因到当前手写 staging；其中部分来自 Eigen kernel、自动向量化、bench harness 或其它库代码。下一轮 reviewer 应要求按符号范围生成 FMA/reduction/gather/vcompress 摘要，避免把无关库代码当成当前 helper 证据。

当前反汇编和板卡证据的组合只能支持“当前 candidate 整体不适合生产接入”。它不能支持“correspondences 慢的主因就是 gather”或“buffer + tail 一定是错误方案”这类单因归因。若要把负向归因闭合，需要把 gather、index/weight 展开、`vcompress`/buffer 写回、自动 `vfredosum` tail 分别拆成消融 bench，并在板卡上比较。

## 10. 生产接入评估

不接 production。理由是板卡真实性能不支持生产接入：全云顺序扫描 64K 的 `1.18x` 属于弱收益，且全云 256K 已退化；对应关系索引路径两个规模都明显退化。反汇编还暴露出自动规约这一未显式控制的数值边界。即使 QEMU correctness 和板卡专项测试通过，也只能证明测试矩阵下的功能一致和路径命中，不能覆盖 production 性能、泛型维护成本和所有入口形态。

当前按 bench-only/no-production closeout（仅保留性能诊断、不接生产收尾）整理，不进入 production integration loop。

## 11. 后续方向

下一轮若继续投入，不应直接接入 production，而应做消融诊断：分离 index/weight 展开成本、`vluxei32.v` gather 成本、`vcompress` 写回成本和自动 `vfredosum` tail 成本。若只保留全云顺序扫描受限分流，也必须证明 256K 退化原因已消除，并补公开入口 gate、fallback 和泛型点类型证据。
