# RVV Eigen 表达式语义对齐

本文记录 PCL RVV 优化中手工展开 Eigen 小矩阵、小向量、投影、距离和阈值表达式时的通用规则。目标是让 RVV intrinsic 的机器级求值结构与当前生产标量路径保持一致，尤其是在输出数量、顺序、像素坐标、阈值谓词和 checksum 对浮点末位敏感的函数中。

相关专题：

- [RVV 反汇编诊断操作手册](RVV%20Disassembly%20Diagnostic.zh.md)
- [RVV 多字段压缩 staging 输出模式](RVV%20Multi-Field%20Compress%20Staging.zh.md)
- [RVV 掩码压缩与保序索引输出模式](RVV%20Compress%20Index%20Output.zh.md)
- [RVV Float-to-Int Rounding and FRM](RVV%20Float-to-Int%20Rounding%20and%20FRM.zh.md)

## 1. 适用范围

本规则适用于 RVV helper 手工展开下列标量表达式：

- `Eigen::Matrix3f` / `Matrix4f` 与 `Vector3f` / `Vector4f` 的小矩阵乘法；
- `getVector3fMap()`、`getVector4fMap()` 读取 PCL 点字段后参与几何计算；
- `Eigen::Vector3f::norm()`、点差平方和、欧氏距离阈值；
- `fx*x + cx*z`、平面距离、cell id、voxel id、像素投影等含乘加和后续转换的表达式；
- 结果会进入 `static_cast<int>`、floor、阈值比较、`vcompress` 输出、排序 key、state update 或 append 顺序的表达式。

源码公式相同不足以证明语义一致。标量编译器、Eigen expression template、目标 ISA 和优化选项可能把 `a*b + c*d` 收缩为 FMA，也可能把小矩阵点积拆成多个部分和。RVV intrinsic 如果用不同的 `vfmul`、`vfadd`、`vfmacc` 顺序，会改变中间舍入点。

## 2. 基本原则

### 2.1 先对齐求值结构，再谈数学等价

对 Eigen 小表达式做 RVV 化时，应先确认当前生产构建下标量 lowering 的求值结构：

```text
标量源码表达式 -> 编译后 fmadd/fmul/fadd/fsqrt/fcvt 序列 -> RVV intrinsic 形态
```

常见对齐方式：

| 标量 lowering                | RVV 形态                                        | 说明                                          |
| ---------------------------- | ----------------------------------------------- | --------------------------------------------- |
| `fmadd.s`                  | `vfmacc.vf` / `vfmacc.vv`                   | 保留乘加单次舍入。                            |
| `fmul.s` 后 `fadd.s`     | `vfmul` 后 `vfadd`                          | 标量确实分离舍入时才采用。                    |
| 两个 FMA 部分和后 `fadd.s` | 两组 `vfmacc` 后 `vfadd.vv`                 | 常见于 Eigen 4x4 row-dot。                    |
| `fsqrt.s` 后 `fcvt.d.s`  | `vfsqrt.v` 后按 float 结果做 double predicate | 常见于 `Vector3f::norm()` 赋给 `double`。 |
| `fcvt.w.s rtz` 或等价截断  | `vfcvt.rtz.x.f.v`                             | 对应 `static_cast<int>` 的向零截断。        |

如果 RVV 路径无法稳定复刻标量求值结构，应缩小 RVV 覆盖范围，或对边界 lane 回退标量。不要把这类差异简单写成“浮点误差”。

### 2.2 FMA contraction 是 correctness 边界

FMA contraction 不只是性能优化。对包含整数截断或阈值比较的表达式，1 ulp 差异可以改变输出：

```text
uv0 = fx*x + cx*z
u   = static_cast<int>(uv0 / z)
```

在像素边界附近，`vfmul + vfadd` 与标量 `fmadd.s` 的结果可能落在整数边界两侧。CEOP 的 projection-pixel staging 因此采用：

```text
uv0 = z * cx
uv0 = vfmacc(uv0, fx, x)
uv1 = z * cy
uv1 = vfmacc(uv1, fy, y)
```

该形态对齐标量 contraction，再接 `vfdiv.vv` 和 `vfcvt.rtz.x.f.v`。

### 2.3 `Vector3f::norm()` 的计算域要单独确认

表达式：

```cpp
const double dist = (p_src3 - pt_tgt.getVector3fMap()).norm();
```

容易被误读成“double 距离”。在当前 CEOP 生产构建中，`p_src3 - target` 是 `Eigen::Vector3f`，`norm()` 的核心平方和与 `sqrt` 先在 `float` 域得到 `float_norm`，随后拓宽为 `double` 参与比较。

因此 RVV final predicate 应对齐：

```text
dx = src.x - tgt.x
dy = src.y - tgt.y
dz = src.z - tgt.z
sum = dx*dx
sum = vfmacc(sum, dy, dy)
sum = vfmacc(sum, dz, dz)
float_norm = vfsqrt(sum)
predicate = double(float_norm) < max_distance
```

不能直接替换成：

```text
float_norm < float(max_distance)
```

当 `float(max_distance)` 等于 `float_norm` 但原始 `double max_distance` 略大时，标量 `double(float_norm) < max_distance` 为 true；单纯 float `<` 会变为 false。CEOP production 用 `<` / `<=` 分支保留该 double threshold 语义，并用 bit / predicate 级测试保护。

### 2.4 `getVector*fMap()` 是字段视图，不是计算授权

`getVector3fMap()` / `getVector4fMap()` 提供 PCL 点字段到 Eigen 向量的视图。RVV helper 可以用 traits / offset gather 直接读取 `x/y/z`，但后续计算仍要对齐 Eigen 标量表达式的字段顺序、`w` 取值和求值结构。

典型边界：

- `getVector4fMap()` 的 `w` 通常为 `1`，4x4 transform 的平移项参与 row-dot；
- identity transform 可以直接 staging 原始 `x/y/z`，避免无意义的 `1*x + 0*y + 0*z + 0` 求值改变边界 lane；
- non-identity transform 应按标量 lowering 展开，而不是只按数学式随意重排。

## 3. 何时必须做反汇编诊断

出现以下任一条件，应把反汇编诊断作为生产接入门禁：

- RVV 改写了 Eigen 小矩阵 / 小向量表达式的求值结构；
- 表达式结果进入 `static_cast<int>`、floor、round、bucket id、pixel id、voxel id；
- 表达式结果进入 `<` / `<=` 阈值谓词，且谓词会改变输出数量或 state update；
- 输出是确定性序列，例如 correspondences、indices、sampled indices、压缩 staging；
- QEMU / board checksum、输出数量、首个差异元素或边界 lane 发生变化；
- 编译器可能自动引入 FMA contraction、tree vectorizer reduction、`fsqrt` / `fcvt` 组合或 FRM/FCSR 修改；
- helper 为了稳定语义使用 `no-tree-vectorize`、显式 rounding intrinsic 或局部标量 tail。

诊断步骤：

1. 构造最小 adversarial case。
2. 分别导出标量与 RVV 构建反汇编。
3. 搜索 `fmadd.s`、`fmsub.s`、`fsqrt.s`、`fcvt.*`、`vfmacc.*`、`vfmul.*`、`vfadd.*`、`vfsqrt.v`、`vfcvt.*`、`fsrm/frrm/frcsr`。
4. 把指令序列映射回源码表达式，确认差异来自乘加顺序、转换、sqrt、阈值、FRM/FCSR、gather 顺序还是 append 状态。
5. 修改 RVV intrinsic 形态或局部回退后，复跑 bit / predicate / production direct 测试。

QEMU 适合用于 checksum、路径、日志和反汇编证据；真实性能结论仍以板卡或目标硬件为准。

## 4. adversarial case 构造

测试数据应围绕“一个 lane 的结果刚好决定分支”的位置构造，而不是只随机采样。

| 风险点               | 数据构造                                                                           |
| -------------------- | ---------------------------------------------------------------------------------- |
| FMA contraction      | 选择 `fx*x + cx*z` 接近整数边界，使 `static_cast<int>` 对 1 ulp 敏感。         |
| 4x4 transform        | 选择大/小量混合、平移项和旋转项接近抵消的点，覆盖 identity 与 non-identity。       |
| `Vector3f::norm()` | 构造平方和 sqrt 后的 `float_norm` 等于或紧邻 `float(max_distance)`。           |
| double threshold     | 选择 `max_distance` 介于 `double(float_norm)` 和相邻 double / float 表示之间。 |
| `vcompress` 顺序   | 构造 keep mask 为 `[1,0,1,1,0...]`，检查输出顺序和多字段 lane 对齐。             |
| gather / layout      | 用 `setIndices()` 的乱序、重复、非连续 subset 和不同点类型组合。                 |
| fallback tail        | 构造前一阶段 staging 成功、后一阶段因规模或 gate 失败的 case。                     |

测试断言应匹配风险等级：

- bit-level：用于验证 RVV helper 与 Eigen lowering 的 float bit pattern；
- predicate-level：用于验证 `<` / `<=`、`double(float_norm) < max_distance` 等布尔结果；
- output-level：用于验证 correspondence / indices / candidate 序列的数量、顺序和 checksum；
- tolerance-level：仅用于规约、近似 math 或业务允许误差的路径。

## 5. 何时允许容差，何时必须等价

### 5.1 可以使用容差的路径

以下路径通常允许工程容差，但需要在主题文档说明误差来源和预算：

- `norms` 这类长向量范数和规约，RVV 使用不同的条带累加和 `vfredosum`；
- `centroid` / covariance 等统计量，RVV 改变累加顺序但最终业务只要求数值近似；
- `bilateral` 中使用 common `expf_RVV_f32m2` 替换 libm `std::exp`，并保留尾段标量累加顺序隔离误差来源；
- Gaussian 卷积等按 tap 顺序对齐但仍可能受工具链和 FMA 策略影响的数值路径，依据测试预算判断。

容差不是默认许可。文档必须说明：

- 标量与 RVV 的差异来自何处；
- 误差预算如何设置；
- 是否会改变输出集合、排序、bucket、state 或 public API 可见行为；
- QEMU / board / 上游测试是否覆盖该风险。

### 5.2 必须 bit / predicate / output 等价的路径

以下路径应按等价处理：

- correspondence、indices、sampled indices 等确定性输出序列；
- `vcompress` 后的保序 staging；
- 像素、voxel、bucket、cell id；
- 阈值边界会改变输出数量、后续元素位置或 checksum 的 predicate；
- stored field bit pattern 被下游测试或 API 观察到的输出。

CEOP 属于该类。一个 lane 的 distance predicate 改变会改变 correspondence 数量、后续元素位置和 checksum。因此 final distance predicate 需要证明 `double(float_norm) < max_distance` 与标量完全一致；stored distance 写出仍用标量 Eigen `norm()` 重算，以保持 `pcl::Correspondence::distance` bit pattern 绑定当前 production 标量表达式。

## 6. 证据分工

| 证据                         | 作用                                                  | 不适合承担的结论                        |
| ---------------------------- | ----------------------------------------------------- | --------------------------------------- |
| 专项 helper 测试             | 证明局部表达式、bit、predicate 或 staging 语义        | 不能替代公开入口测试。                  |
| production-shaped diagnostic | 证明对象状态、fake indices、subset 和阶段归因         | production 接入后不能替代真实入口证据。 |
| production direct 测试       | 证明真实上游入口、真实 gate、fallback 和输出          | 不能单独解释局部差异来源。              |
| QEMU run_test / run_bench    | checksum、日志格式、路径命中、可复现性                | 不作为真实性能结论。                    |
| 反汇编                       | 指令结构、FMA、sqrt、convert、FRM/FCSR、`vcompress` | 不证明业务性能。                        |
| board bench                  | 真实性能和 full case 成本                             | 不替代 bit / predicate correctness。    |

生产接入判断应把这些证据组合起来，而不是用单一 bench speedup 替代语义证明。

## 7. 已有主题经验归纳

| 主题                                                                          | 可复用经验                                                                                                                                        |
| ----------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------- |
| [CEOP](../registration/correspondence_estimation_organized_projection-RVV.zh.md) | Eigen 4x4 transform、projection-pixel FMA、`Vector3f::norm()` predicate、`vcompress` 保序和 append 标量边界都需要按 production 输出序列处理。 |
| [transforms](../common/transforms-RVV.zh.md)                                     | dense 点云 4x4 transform 适合按 VL chunk 批量化；in-place 安全要求先完成输入 load 再 store。小矩阵 row-dot 应记录 FMA 组织。                      |
| [norms](../common/norms-RVV.zh.md)                                               | 长向量规约允许容差，但要说明 `vfredosum` 与标量累加顺序不同；含 `logf_RVV` 的路径要单独设误差预算。                                           |
| [centroid](../common/centroid.zh.md)                                             | centroid / covariance 属于统计规约，重点是累加顺序、finite / dense gate 和 Eigen 写回语义，而不是 bit 等价。                                      |
| [gaussian](../common/gaussian.zh.md)                                             | 卷积按 tap 顺序对齐，数值算例应展示标量 tap 与 RVV chunk 的一一对应。                                                                             |
| [frustum_culling](../filters/frustum_culling-RVV.zh.md)                          | 多个几何谓词共享同一 VL chunk，可用命名小谓词组织 FMA / compare / mask 收敛；输出 indices 必须保序。                                              |
| [bilateral](../filters/bilateral-RVV.zh.md)                                      | 近似 math 与尾段累加顺序要拆开归因；若怀疑编译器自动向量化影响尾段，应按反汇编手册单独复核，避免把未验证的 reduction 顺序变化写成既定事实。       |
| [covariance_sampling](../filters/covariance_sampling-RVV.zh.md)                  | 局部 RVV 片段收益和近似结果不足以证明生产可接入；sampled-index 序列受 Eigen solver、sort 和 state update 放大时应停留在 bench-only。              |

## 8. 文档与源码注释要求

涉及 Eigen / FMA / norm 语义对齐的 RVV 工作，应在源码、测试、bench 和主题文档中留下短注释，说明保护的语义边界：

- RVV helper 注释：说明某段 `vfmacc` / `vfadd` 对齐哪个 Eigen lowering 或标量表达式；
- 测试注释：说明 adversarial 数据保护哪个边界 lane、bit pattern 或 predicate；
- bench 注释：说明 case 是 helper、production-shaped diagnostic 还是 production direct；
- 主题文档：记录标量源码、RVV intrinsic 形态、反汇编线索、修复方式和保留的标量边界；
- closeout：把当前状态写在前面，历史失败只保留能解释当前设计的部分。

## 9. 生产接入决策

手工展开 Eigen 表达式后，按以下顺序判断：

1. 标量表达式的当前 lowering 已通过反汇编确认。
2. RVV intrinsic 形态能复刻关键舍入点。
3. adversarial bit / predicate / output 测试覆盖边界。
4. production-shaped 或 production direct 入口覆盖对象状态、indices、traits 和 fallback。
5. QEMU checksum 与路径证据通过。
6. 板卡 full case 证明 staging、buffer 和 tail 成本没有抵消收益。
7. 文档和 skill 已沉淀可复用规则。

若第 2 或第 3 点无法成立，应保留标量 tail、局部回退或 bench-only 诊断。不要为了“全 RVV 化”牺牲确定性输出序列和可维护性。
