# filters/covariance_sampling RVV 诊断说明

## 1. 函数入口作用

`pcl::CovarianceSampling<PointT, PointNT>` 是 `filters` 模块中基于协方差覆盖的采样滤波器。用户设置输入点云、法线、采样数量后调用 `filter(indices)`，公开入口最终进入 `CovarianceSampling<PointT, PointNT>::applyFilter(Indices&)`，输出被采样保留的原始点索引。

该滤波器不是简单线性筛选。它先根据输入点计算 centroid、归一化后的 scaled points 和协方差矩阵，再用 6x6 特征分解得到方向，然后为每个候选点构造 6D 向量，维护 6 个按贡献排序的 list，并迭代选择使各方向覆盖更均衡的点。当前 RVV 工作只做 bench 诊断，不改变公开 API，也不修改 `filters/include/pcl/filters/impl/covariance_sampling.hpp` 的生产分流。

## 2. 标量路径与诊断边界

上游标量路径的关键阶段是：

```text
initCompute()
  -> compute3DCentroid(input, indices)
  -> scaled_point = point - centroid
  -> average norm
  -> scaled_point /= average_norm

computeCovarianceMatrix()
  -> f[i] = [scaled_point[i] cross normal[i], normal[i]]
  -> covariance = f * f.transpose()

applyFilter(indices)
  -> Eigen 6x6 self-adjoint solver
  -> 为每个点构造 6D vector
  -> 6 个 list 按 dot(eigenvector_axis) 排序
  -> 迭代采样并更新 6 个方向累计值
```

保留候选复筛将本主题列为 `保留 / 待诊断`，诊断点是 centroid、scaled point 和 6D vector 构造。诊断边界必须覆盖 full diagnostic，因为局部 RVV 片段可能被 Eigen 6x6 solver、six-list sort 和 sampling state 稀释。

本主题的 bench-only 原型覆盖 `PointXYZ` + `Normal` 的显式 indices 场景。RVV 只尝试两个局部片段：scaled-point 构造和 normal gather 后的 6D vector 构造。solver、list sort 和 sampling state 保持标量 / Eigen。生产入口没有接入新 RVV helper。

## 3. 覆盖范围与 fallback

| 项目 | 结论 |
| --- | --- |
| 覆盖点类型 | 诊断覆盖 `pcl::PointXYZ` 和 `pcl::Normal` |
| 覆盖数据形态 | 显式 `indices` gather；identity 和 shuffled indices 均测 |
| RVV 内容 | `PointXYZ x/y/z` gather、centroid 累加、scaled point 暂存；`Normal normal_x/y/z` gather |
| 保持标量 | norm sqrt 累加、scaled point 归一化、cross product、double 6D vector 写入、Eigen solver、six-list sort、sampling update |
| fallback | 点数 `<32`、非 RVV 编译或诊断宏未开启时回退 Std helper |
| 生产入口 | 不修改公开 API，不修改 `applyFilter(Indices&)` 生产分流 |

当前 fallback 是诊断 helper 内的局部 fallback，不是生产分流。它用于证明小规模或非 RVV 构建不会误入 bench-only RVV 片段。

## 4. 详细设计

专项实现位于 `test-rvv/filters/covariance_sampling/covariance_sampling_diag.hpp`。

### 4.1 类型与布局边界

本主题是 bench 诊断，不提供模板化生产 helper。诊断入口固定为：

```cpp
computeScaledPointsRVV(const pcl::PointCloud<pcl::PointXYZ>& cloud,
                       const pcl::Indices& indices,
                       std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>& scaled_points,
                       Eigen::Vector3f& centroid,
                       double& average_norm)

buildVectorsRVV(const std::vector<Eigen::Vector3f, Eigen::aligned_allocator<Eigen::Vector3f>>& scaled_points,
                const pcl::PointCloud<pcl::Normal>& normals,
                const pcl::Indices& indices,
                std::vector<Vector6d, Eigen::aligned_allocator<Vector6d>>& vectors)
```

因此本主题没有新增生产级 `PointT` / `PointNT` traits。类型安全来自函数签名、`PointXYZ` / `Normal` 标准字段，以及公共 `rvv_point_load` gather wrapper 对 `offsetof(PointXYZ, x/y/z)` 和 `offsetof(Normal, normal_x/y/z)` 的使用。若后续重新纳入生产候选，应先定义模板入口、类型兼容检查和 sampled-index 语义约束。

### 4.2 特殊实体说明

| 实体 | 类型 | 输入 / 输出 | 作用与边界 |
| --- | --- | --- | --- |
| `computeScaledPointsStd` | 标量 helper | cloud + indices -> centroid / scaled points / average norm | 作为 RVV helper 的对拍基准 |
| `computeScaledPointsRVV` | RVV 诊断 helper | `PointXYZ` cloud + indices -> scaled points | RVV gather 和坐标累加；norm 和归一化仍通过标量尾段完成 |
| `buildVectorsStd` | 标量 helper | scaled points + normals + indices -> 6D vectors | 作为 RVV normal gather 的对拍基准 |
| `buildVectorsRVV` | RVV 诊断 helper | scaled points + normals + indices -> 6D vectors | RVV gather normals；cross product 和 double vector 写入仍在标量尾段完成 |
| `computeCovarianceFromVectors` | 标量 / Eigen helper | 6D vectors -> 6x6 covariance | 保留 Eigen 边界，用于观察 solver 相关成本 |
| `sampleFromVectors` | 标量 helper | vectors + eigenvectors -> sampled indices | 保留 six-list sort 和 sampling state 语义 |
| `runDiagnostic` | full diagnostic helper | cloud + normals + indices + sample count -> result | 串联 RVV 局部片段与标量 solver / sort / sampling，判断局部收益是否穿透完整入口 |

### 4.3 scaled-point 片段

`computeScaledPointsRVV` 对应 bench 中的 `covariance_sampling scaled-points ...` case。每个 VL chunk：

1. `vle32` 读取 `indices`；
2. `byte_offsets_u32m2<PointXYZ>` 把 source index 转成 AoS byte offset；
3. `indexed_load3_f32m2<PointXYZ, offsetof(...x/y/z)>` gather `x/y/z`；
4. 向量累加 x/y/z 得到 centroid 分子；
5. 第二轮 gather 同一批点，减去 centroid；
6. 计算平方和并写回临时数组，标量端执行 `sqrt` 累加 average norm；
7. 标量端按 `1 / average_norm` 归一化 `scaled_points`。

这个片段没有处理 NaN / Inf mask，也没有覆盖泛型 `PointT`。它只回答“给定 `PointXYZ` 和 indices 时，centroid / scaled point 准备是否有足够局部收益”。

### 4.4 6D-vector 片段

`buildVectorsRVV` 对应 bench 中的 `covariance_sampling 6D-vector ...` case。每个 VL chunk：

1. `vle32` 读取 `indices`；
2. 对 `Normal` 执行 AoS gather，取得 `normal_x/y/z`；
3. 将 normal lane 写回临时数组；
4. 标量端读取 `scaled_points[i]`，计算 `scaled_points[i].cross(normal)`；
5. 写入 `Vector6d` 前 3 维和 normal 后 3 维。

该片段刻意没有把 cross product 和 double 写入完全 RVV 化，因为本轮诊断的核心是判断 normal gather 和 6D vector 构造是否值得继续，而不是提前承担生产级 double / Eigen 数据布局复杂度。

### 4.5 full diagnostic：局部 RVV + 标量 solver / sort / sampling

`runDiagnostic(..., use_rvv_fragments=true)` 对应 full diagnostic：

```text
computeScaledPointsRVV or Std fallback
-> buildVectorsRVV or Std fallback
-> computeCovarianceFromVectors
-> Eigen::SelfAdjointEigenSolver<Matrix6d>
-> sampleFromVectors
```

这条路径是当前主题的生产价值判断依据。它保留了 covariance solver、six-list sort 和 sampling update，因此能观察局部片段收益是否被真实主成本稀释。

## 5. 数值算例与 VL chunk 图示

设一个 VL chunk 中有 4 个 source index，centroid 已由第一轮规约得到：

```text
indices lanes:  [p3, p7, p8, p11]
gather xyz:     [x3..x11], [y3..y11], [z3..z11]
subtract mean:  dx=x-centroid.x, dy=y-centroid.y, dz=z-centroid.z
norm staging:   sqrt(dx*dx + dy*dy + dz*dz) 由标量尾段累加
scaled point:   [dx,dy,dz] / average_norm
```

随后 6D vector 构造使用同一 indices 顺序：

```text
normal gather:  [nx3..nx11], [ny3..ny11], [nz3..nz11]
cross:          scaled_point[i] cross normal[index[i]]
vector6:        [cross.x, cross.y, cross.z, nx, ny, nz]
```

这个图示强调两个语义边界：

- `scaled_points[i]` 的局部顺序必须与 `indices[i]` 对齐；
- normal gather 使用原始 source index，而不是 scaled point 的局部编号。

当前 RVV/Std full diagnostic checksum 不一致，说明仅比较 condition number 和 sampled count 不足以证明 sampled-index 序列完全一致。该问题是本主题不接生产的主要语义原因之一。

## 6. 测试、QEMU、反汇编和板卡证据

专项测试：

- `make -C test-rvv/filters/covariance_sampling run_test_compare` 通过；
- Std/RVV 两套构建均通过 4 个 gtest；
- 测试覆盖 scaled-point helper 对拍、identity / shuffled indices 的 full diagnostic condition number 和 sampled count、以及小规模 fallback。

测试使用 `gtest` / `gtest_main`，不是手写 main。bench 是独立可执行文件，用于输出可解析性能日志。

QEMU bench：

- `make -C test-rvv/filters/covariance_sampling run_bench_compare` 通过；
- `output/qemu/analyze_bench_compare.log` 可解析；
- QEMU 只作为构建、日志格式和路径证据，不作为性能结论。

反汇编：

- `make -C test-rvv/filters/covariance_sampling dump_bench_rvv` 生成 `build/asm/riscv/bench_covariance_sampling_rvv.full.asm`；
- RVV 摘录中可见 `vsetvli`、`vle/vse`、`vfmul`、`vfmadd`、`vfred*` 等指令；
- 该二进制还包含 Eigen/RVV 或编译器向量化指令，不能把全部 RVV 指令归因到本主题手写 helper。

板卡验证：

- `make -C test-rvv/filters/covariance_sampling run_board_test run_board_bench_compare fetch_board_logs` 通过；
- 日志位于 `test-rvv/filters/covariance_sampling/output/board/`；
- 设备：Milkv-Jupiter；iterations：5；数据集：synthetic `PointXYZ` + `Normal` clouds，identity 和 shuffled indices。

## 7. 板卡结果与 case 解释

speedup 计算方式为 `Std avg ms/iter / RVV avg ms/iter`。full diagnostic 的 Std/RVV checksum 不一致，因此 full diagnostic 不能作为生产语义等价证据。

| case | 入口与参数 | 路径含义 | speedup | 证明点 |
| --- | --- | --- | ---: | --- |
| `covariance_sampling scaled-points identity 4K` | 4K `PointXYZ`，identity indices | RVV scaled-point helper | 1.08x | 局部片段只有弱收益 |
| `covariance_sampling scaled-points shuffled 4K` | 4K `PointXYZ`，shuffled indices | RVV gather + scaled-point helper | 1.05x | gather 形态下仍是弱收益 |
| `covariance_sampling 6D-vector identity 4K` | 4K `Normal`，identity indices | RVV normal gather + 标量 cross / double 写入 | 0.79x | 局部 helper 退化 |
| `covariance_sampling 6D-vector shuffled 4K` | 4K `Normal`，shuffled indices | RVV normal gather + 标量 cross / double 写入 | 1.12x | 局部收益不稳定 |
| `covariance_sampling covariance+solver identity 4K` | 4K vectors，Eigen 6x6 solver | 不命中本主题主要 RVV helper | 1.02x | solver 边界基本持平 |
| `covariance_sampling full apply identity 4K sample 512` | 4K identity，采样 512 | 局部 RVV + 标量 solver / sort / sampling | 0.99x | full diagnostic 不成立 |
| `covariance_sampling full apply shuffled 4K sample 512` | 4K shuffled，采样 512 | 局部 RVV + 标量 solver / sort / sampling | 1.00x | full diagnostic 持平 |
| `covariance_sampling scaled-points identity 16K` | 16K `PointXYZ`，identity indices | RVV scaled-point helper | 1.01x | 大规模仍是弱收益 |
| `covariance_sampling 6D-vector identity 16K` | 16K `Normal`，identity indices | RVV normal gather + 标量 cross / double 写入 | 0.78x | helper 退化稳定存在 |
| `covariance_sampling covariance+solver identity 16K` | 16K vectors，Eigen 6x6 solver | 不命中本主题主要 RVV helper | 1.00x | 持平 |
| `covariance_sampling full apply identity 16K sample 1024` | 16K identity，采样 1024 | 局部 RVV + 标量 solver / sort / sampling | 1.00x | full diagnostic 持平 |
| `covariance_sampling full apply shuffled 16K sample 1024` | 16K shuffled，采样 1024 | 局部 RVV + 标量 solver / sort / sampling | 1.00x | full diagnostic 持平 |

## 8. 结论

`CovarianceSampling` 的 centroid / scaled-point 和 6D-vector 构造可以形成 bench-only RVV 诊断，但当前不应接入生产主路径。板卡结果显示：scaled-point 片段仅有 `1.01x` 到 `1.08x` 弱收益，6D-vector 片段有 `0.78x` 到 `0.79x` 退化 case，full diagnostic 为 `0.99x` 到 `1.00x`，没有覆盖生产入口主成本。

更重要的是，full diagnostic 的 Std/RVV checksum 不一致，说明 sampled index 序列可能受到浮点累加顺序、Eigen solver、list sort 或 sampling update 的放大影响。即使 condition number 和 sampled count 在 gtest 容差内一致，也不能证明生产输出语义等价。

后续若重新评估生产接入，应先提出能保持 sampled-index 语义的完整数据流方案，并证明 full `applyFilter(Indices&)` 或真实生产入口在板卡上有稳定明显收益。只优化前置 centroid / scaled-point 或 normal gather 片段，不足以重新升级为生产候选。
