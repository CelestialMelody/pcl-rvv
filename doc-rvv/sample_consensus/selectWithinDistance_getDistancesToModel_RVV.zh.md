# `selectWithinDistance` / `countWithinDistance` / `getDistancesToModel`：NormalPlane RVV 优化实现说明

面向 RVV 1.0（`__RVV10__`），本文记录 `pcl::SampleConsensusModelNormalPlane` 三条距离相关入口在本仓库相对上游 PCL 的差异、实现要点，以及测试与基准入口。与上游同名文件对照：[`sac_model_normal_plane.hpp`](https://github.com/PointCloudLibrary/pcl/blob/master/sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp)。

---

## 1. 背景

`SampleConsensusModelNormalPlane` 在 SAC 流水线里反复调用距离判定：按阈值筛内点（`selectWithinDistance`）、只统计内点数（`countWithinDistance`），或输出每个索引对应的标量距离（`getDistancesToModel`）。距离定义是几何项与法线角向项的加权组合：
\[
D = \left| w \cdot d_{ang} + (1-w)\cdot d_{geom} \right|,\quad
w = w_{user}\cdot(1-\text{curvature})
\]

其中 \(d_{geom} = |ax + by + cz + d|\)，\(d_{ang}\) 来自点法线与平面法线夹角并截断为锐角侧；\(w\) 随点曲率变化。

实现上的制约主要来自数据布局与访存形态：`indices_` 指向点云与法线数组，访问路径为「索引表 → `real_id` → AoS 中的坐标与法线」。该间接寻址阻断了沿 `i` 连续加载 `input_` 的假设；`selectWithinDistance` 的输出又是变长内点列表，不是与 `indices_` 等长的稠密数组。编译器自动向量化在这类路径上往往覆盖不全，因此在 RVV 上用手写 strip、`vluxei32` gather、掩码与压缩写回，与 `countWithinDistanceRVV`、平面距离/锐角等已有 `f32m2` 内核对齐，减少重复逻辑并便于后续维护。

---

## 2. 与上游实现的差异

### 2.1 RVV 运算类型

在 `#if defined(__RVV10__)` 下，公开入口先检查 source 点型是否满足 `RVVXYZAoSFloatLayout<PointT>`、normal 点型是否满足 normal/curvature 单 float 且 AoS-compatible 的本地 gate，并确认 source / normal 点云规模不超过 32-bit byte offset helper 可表达范围。条件满足时，`selectWithinDistance` 调用 `selectWithinDistanceRVV`，`getDistancesToModel` 调用 `getDistancesToModelRVV`，`countWithinDistance` 调用 `countWithinDistanceRVV`；否则回退对应 Standard helper。算术在 `float` 向量寄存器（`vfloat32m2_t` 等）中完成；接口仍向调用方提供 `std::vector<double>`（距离）与 `Indices`（内点索引），写回处通过 `vfwcvt` 将 `float` 宽化为 `double`，与 PCL 原有 API 类型保持一致。

### 2.2 数据写回

上游 `selectWithinDistance` 通常对 `inliers`、`error_sqr_dists_` 使用 `reserve` 后在命中时 `push_back`。该模式对「掩码 + 压缩」写回不匹配：`vcompress` 产出的是连续 lane 块，需要已知基址与偏移的一次性顺序写入，而不是由 `push_back` 隐式推进尾部。本仓库在入口层改为先按 `indices_->size()` `resize` 两处缓冲区，标量路径与 RVV 路径均按偏移写入，最后按实际内点数 `resize` 收缩。

上游`getDistancesToModel`按 `distances[i]` 顺序写回；本仓库主要增加 RVV 实现分支及上述 float 计算 / double 存储的拆分，不改变「第 `i` 个索引对应 `distances[i]`」的语义。`countWithinDistance` 不写出距离数组，只在 RVV 路径中用 mask popcount（掩码计数）累计小于阈值的 lane 数。

### 2.3 标量接口

标量逻辑抽成 `selectWithinDistanceStandard(..., i, current_count)`，用 `current_count` 表示下一次写入位置，与 RVV 侧 `nr_p` 的语义一致，便于与 SIMD 尾段或将来其他 ISA 扩展共用同一套「预分配 + 定址」模型。Phase 000 还补齐了 protected helper 直接调用的缓冲区合同：即使调用者绕过公开入口、传入空 `inliers` 或空 `error_sqr_dists_`，`selectWithinDistanceStandard` 与 `selectWithinDistanceRVV` 也会先按剩余索引数量补足写回空间。

---

## 3. 详细实现

### 3.1 入口：`resize`、layout gate、分流和 `resize` 收缩

预分配、`__RVV10__` 分支与最终收缩如下：

```cpp
// 66:87:sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp

  // --- 内存预分配的关键步骤 ---
  inliers.clear();
  error_sqr_dists_.clear();
  // 注意：使用 resize 而不是 reserve，以便我们可以直接通过指针写入
  inliers.resize(indices_->size());
  error_sqr_dists_.resize(indices_->size());

  std::size_t nr_p = 0; // 记录实际内点数量

#if defined (__RVV10__)
  if constexpr (pcl::detail::kNormalPlaneRVVLayoutCompatible<PointT, PointNT>)
  {
    if (input_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () &&
        normals_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> ())
      nr_p = selectWithinDistanceRVV(model_coefficients, threshold, inliers);
    else
      nr_p = selectWithinDistanceStandard(model_coefficients, threshold, inliers, 0, 0);
  }
  else
  {
    nr_p = selectWithinDistanceStandard(model_coefficients, threshold, inliers, 0, 0);
  }
#else
  // 调用标准版本
  nr_p = selectWithinDistanceStandard(model_coefficients, threshold, inliers, 0, 0);
#endif

  // --- 收缩内存 ---
  // 将容器大小调整为实际内点数量
  inliers.resize(nr_p);
  error_sqr_dists_.resize(nr_p);
```

### 3.2 标量路径：定址写入

循环内通过 `(*indices_)[i]` 取点与法线，`getAngle3D` 参与角度项；命中阈值时写入 `inliers[current_count]`、`error_sqr_dists_[current_count]`。

```cpp
  for (; i < indices_->size (); ++i)
  {
    // 双重间接寻址：
    // indices_[i] -> 获取点在原始云中的真实 ID (Real ID)
    // (*input_)[Real ID] -> 获取具体的坐标点 PointT
    // (*normals_)[Real ID] -> 获取对应的法线点 PointNT
    const PointT  &pt = (*input_)[(*indices_)[i]];
    const PointNT &nt = (*normals_)[(*indices_)[i]];

    // 计算欧氏距离 (Euclidean Distance)
    // 公式：D_geom = |ax + by + cz + d|
    // coeff.dot(p) 计算 ax + by + cz
    // model_coefficients[3] 是 d
    Eigen::Vector4f p (pt.x, pt.y, pt.z, 0.0f);
    // 注意：这里定义的 n 只是为了给 getAngle3D 传参，实际上 NormalPlane 需要更高效的计算
    Eigen::Vector4f n (nt.normal_x, nt.normal_y, nt.normal_z, 0.0f);
    double d_euclid = std::abs (coeff.dot (p) + model_coefficients[3]);

    // 计算角度差异 (Angular Distance)
    // 计算点法线 n 与平面法线 coeff 的夹角 (弧度)
    // getAngle3D 内部通常计算 acos(dot(n1, n2))
    double d_normal = std::abs (getAngle3D (n, coeff));
    // 处理钝角情况：我们只关心锐角差异。如果夹角 > 90度 (PI/2)，说明法线反向了，取补角
    // 实际上 PCL 这里写的 min(d, PI-d) 是为了把范围限制在 [0, PI/2]
    d_normal = (std::min) (d_normal, M_PI - d_normal);

    // 计算权重
    // 基于曲率 (curvature) 的权重。
    // 如果曲率小（平坦表面），权重高，更多参考法线距离；
    // 如果曲率大（噪点或边缘），权重低，更多参考欧氏距离。
    double weight = normal_distance_weight_ * (1.0 - nt.curvature);

    // 综合距离计算
    // 线性插值混合两种距离
    double distance = std::abs (weight * d_normal + (1.0 - weight) * d_euclid);

    if (distance < threshold)
    {
      // 直接写入预分配的内存
      inliers[current_count] = (*indices_)[i];
      error_sqr_dists_[current_count] = distance;
      current_count++;
    }
  }
  return current_count;
```

### 3.3 `selectWithinDistanceRVV`：VLA、`vluxei32`、掩码、`vcompress`

- `__riscv_vsetvl_e32m2` 控制每轮 `vl`；
- `vle32` 加载 `indices_` 片段，再通过 `pcl::rvv_load::byte_offsets_u32m2` 得到点类型对应的字节偏移，对坐标、法线、曲率做 indexed gather（按索引离散加载）；
- 平面距离与锐角项调用 `SampleConsensusModelPlane<PointT>::distRVV_f32m2`、`getAcuteAngle3DRVV_f32m2`，与同文件中的 `countWithinDistanceRVV` 共用 `f32m2` 内核；
- `vmflt` 比较阈值，`vcpop` 计数；`active_count > 0` 时对索引与距离做 `vcompress`，距离经 `vfwcvt` 宽化后 `vse64` 写入 `error_sqr_dists_`。

```cpp
  for (; i < total_n; ) {
    // 动态设定向量长度
    const size_t vl = __riscv_vsetvl_e32m2(total_n - i);

    // --- A. 加载索引 ---
    const vuint32m2_t v_idx = __riscv_vle32_v_u32m2((const uint32_t*)(indices_ptr + i), vl);
    const vuint32m2_t v_off_pt = pcl::rvv_load::byte_offsets_u32m2<PointT>(v_idx, vl);
    const vuint32m2_t v_off_norm = pcl::rvv_load::byte_offsets_u32m2<PointNT>(v_idx, vl);

    // --- B. 加载数据 (Gather Load) ---
    // 加载 PointT (x, y, z)
    vfloat32m2_t v_px;
    vfloat32m2_t v_py;
    vfloat32m2_t v_pz;
    pcl::rvv_load::indexed_load3_fields_f32m2<
        PointT, PointLayout::kX, PointLayout::kY, PointLayout::kZ>(
        points_base, v_off_pt, vl, v_px, v_py, v_pz);

    // 加载 PointNT (nx, ny, nz) 与 curvature。
    const vfloat32m2_t v_nx =
        pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_x>(
            normals_base, v_off_norm, vl);
    const vfloat32m2_t v_ny =
        pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_y>(
            normals_base, v_off_norm, vl);
    const vfloat32m2_t v_nz =
        pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::normal_z>(
            normals_base, v_off_norm, vl);

    const vfloat32m2_t v_curv =
        pcl::rvv_load::indexed_load_field_f32m2<PointNT, pcl::fields::curvature>(
            normals_base, v_off_norm, vl);

    // --- C. 计算距离 (全程 Float) ---
    // 广播系数
    const vfloat32m2_t v_a = __riscv_vfmv_v_f_f32m2(a, vl);
    const vfloat32m2_t v_b = __riscv_vfmv_v_f_f32m2(b, vl);
    const vfloat32m2_t v_c = __riscv_vfmv_v_f_f32m2(c, vl);
    const vfloat32m2_t v_d = __riscv_vfmv_v_f_f32m2(d, vl);

    // Math Kernels
    const vfloat32m2_t v_d_euc = pcl::SampleConsensusModelPlane<PointT>::distRVV_f32m2(v_px, v_py, v_pz, v_a, v_b, v_c, v_d, vl);
    const vfloat32m2_t v_d_norm = pcl::getAcuteAngle3DRVV_f32m2(v_nx, v_ny, v_nz, v_a, v_b, v_c, vl);

    // Weight Calculation
    const vfloat32m2_t v_w = __riscv_vfmul_vf_f32m2(
                          __riscv_vfrsub_vf_f32m2(v_curv, 1.0f, vl), w_scalar, vl);

    // Final Distance
    vfloat32m2_t v_dist = __riscv_vfmacc_vv_f32m2(
                            __riscv_vfmul_vv_f32m2(v_w, v_d_norm, vl),
                            __riscv_vfrsub_vf_f32m2(v_w, 1.0f, vl), v_d_euc, vl);
    v_dist = __riscv_vfsgnjx_vv_f32m2(v_dist, v_dist, vl); // abs(dist)

    // --- D. 筛选与存储 (Select & Store) ---

    // 1. 生成 Mask
    const vbool16_t v_mask = __riscv_vmflt_vf_f32m2_b16(v_dist, th, vl);

    // 2. 统计有效数量
    long active_count = __riscv_vcpop_m_b16(v_mask, vl);

    if (active_count > 0) {
      // 1. 存储 Inliers (uint32_t)
      // m2 类型的索引压缩后依然是 m2
      vuint32m2_t v_idx_compressed = __riscv_vcompress_vm_u32m2(v_idx, v_mask, vl);
      __riscv_vse32_v_u32m2((uint32_t*)(inliers_out_ptr + nr_p), v_idx_compressed, active_count);

      // 2. 存储 Distances (float -> double)
      // a. 在 m2 级别进行压缩
      vfloat32m2_t v_dist_compressed = __riscv_vcompress_vm_f32m2(v_dist, v_mask, vl);

      // b. 由 m2 宽化为 m4。
      // 这是 RVV 硬件定义的 SEW*2 必须对应 LMUL*2 的要求
      vfloat64m4_t v_dist_double = __riscv_vfwcvt_f_f_v_f64m4(v_dist_compressed, active_count);

      // c. 使用 vse64 直接写回 double 数组
      __riscv_vse64_v_f64m4(dists_out_ptr + nr_p, v_dist_double, active_count);

      nr_p += active_count;
  }

    i += vl;
  }
```

实现上多出的代价是：在 `resize` 至 `indices_->size()` 后，内点数量远小于 `indices_->size()` 时，峰值会短暂占用满长度缓冲区；换得的是 `vcompress` 结果可连续 `vse` 写回。单轮循环内 `v_idx` 只加载一次，点与法线与 `v_off_pt` / `v_off_norm` 对齐。gather 与 `vluxseg3ei32` 的对比见 `doc-rvv/rvv/RVV Load Store Strategy.zh.md`。

公开入口的 RVV gate（会导致分流的准入条件）当前覆盖：source 点型的 registered single-float `x/y/z`、POD / standard-layout / stride alignment 前提；normal 点型的 registered single-float `normal_x/y/z/curvature`、standard-layout 和字段 alignment 前提；以及 source / normal 点云规模的 32-bit byte offset 上界。`PointXYZI` 和 `PointXYZINormal` 已作为 source 代表点型通过公开入口 correctness 测试；`PointNormal` 和 `PointXYZINormal` 已作为 normal cloud 代表点型通过公开入口 correctness 测试；`PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉组合也已通过公开入口对 direct RVV helper 的 correctness 测试。注册了 `x/y/z` 但非 standard-layout 的 source 点型、注册 normal/curvature 但非 standard-layout 的 normal 点型，以及 curvature 非单 float 的 normal 点型会回退标量路径。

### 3.4 `getDistancesToModel` 分流与 `getDistancesToModelRVV` 写回

对外接口在 `distances.resize` 后按 `__RVV10__` 选择实现：

```cpp
  distances.resize (indices_->size ());

#if defined (__RVV10__)
  if constexpr (pcl::detail::kNormalPlaneRVVLayoutCompatible<PointT, PointNT>)
  {
    if (input_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT> () &&
        normals_->points.size () <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointNT> ())
    {
      getDistancesToModelRVV(model_coefficients, distances);
      return;
    }
  }
  getDistancesToModelStandard(model_coefficients, distances, 0);
#else
  // 标准版本
  getDistancesToModelStandard(model_coefficients, distances, 0);
#endif
```

`getDistancesToModelRVV` 无阈值与压缩；每轮 `vl` 个 `float` 距离宽化为 `double` 后顺序写入。

```cpp
    // 5. 写回：float → double
    // 遵循 SEW*2 对应 LMUL*2 的原则
    vfloat64m4_t v_final_dist_d = __riscv_vfwcvt_f_f_v_f64m4(v_final_dist_f, vl);
    __riscv_vse64_v_f64m4(dists_out_ptr + i, v_final_dist_d, vl);

    i += vl;
```

同文件 `getDistancesToModelRVV` 中保留了对 `vluxseg3ei32` 加载的注释块，便于在点类型或微架构变化时替换 gather 形态而不改动距离算术。

---

## 4. 测试与基准

### 4.1 数据与运行环境说明

测试与基准使用同一份点云数据文件 `sac_plane_test.pcd`。板卡（Milk-V Jupyter）侧的 bench 输出会打印该文件的点数（示例日志为 3283 points），并打印目标的 RVV 配置（示例日志为 `rv64gcv`，`VLEN 256-bit (zvl256b)`）。

单元测试中同时包含两类数据来源：

- **文件点云**：用于模型流程与回归（如 RANSAC/LMedS/MSAC 等用例）；
- **生成点云**：用于稳定复现 SIMD 路径差异与计时。板卡日志中 NormalPlane 的性能报告会显示“Points per cloud”，例如 `selectWithinDistance`/`getDistancesToModel` 采用 2000 点，`countWithinDistance` 采用 10000 点，并用固定次数迭代（示例日志为 1000 iterations）。

### 4.2 正确性验证

`test-rvv/sample_consensus/plane_models/src/test_sample_consensus_plane_models.cpp` 中 `SampleConsensusModelNormalPlane.SIMD_selectWithinDistance` 对比 `selectWithinDistanceStandard` 与 `selectWithinDistanceRVV`；`SIMD_countWithinDistance` 对比标准与 RVV 计数；`SIMD_getDistancesToModel` 对比 `getDistancesToModelStandard` 与 `getDistancesToModelRVV`，后者用 `EXPECT_NEAR` 约束数值误差。阈值边界附近对浮点舍入留有容差，避免把舍入差异当作实现错误。

公开入口测试覆盖以下行为：

- `PublicEntriesMatchDirectRVVForSupportedLayout`：`PointXYZ + Normal` 支持布局下，公开 `select/count/getDistances` 与直接 RVV helper 输出一致。
- `PublicEntriesMatchDirectRVVForPointXYZISource` 和 `PublicEntriesMatchDirectRVVForPointXYZINormalSource`：source 只读 `x/y/z` 时，`PointXYZI` 与 `PointXYZINormal` 代表性 AoS source 点型的公开入口输出与 direct RVV helper 一致。
- `PublicEntriesMatchDirectRVVForPointNormalNormalLayout` 和 `PublicEntriesMatchDirectRVVForPointXYZINormalNormalLayout`：normal cloud 只读 `normal_x/y/z/curvature` 时，`PointNormal` 与 `PointXYZINormal` 代表性 normal layout 的公开入口输出与 direct RVV helper 一致。
- `PublicEntriesFallbackForNonAoSRegisteredXYZSource`：注册 xyz 但非 standard-layout source 点型在 RVV build 下回退 Standard helper，不会实例化 byte-offset gather helper。
- `PublicEntriesFallbackForNonAoSRegisteredNormalLayout`：注册 normal/curvature 单 float 但非 standard-layout normal 点型在 RVV build 下回退 Standard helper。
- `PublicEntriesFallbackForUnregisteredNormalLayout`：注册但 curvature 不是 float 的 normal 类型在 RVV build 下回退到标量公开入口。
- `SelectHelperResizesEmptyOutputBuffers`：直接调用 select helper 时，空输出缓冲区会被 helper 自行扩容，避免写越界。

```bash
cd test-rvv/sample_consensus/plane_models
make run_test
```

默认可通过 QEMU（如 `qemu-riscv64 -cpu rv64,v=true,vlen=256,elen=64`）在无硬件环境下执行；板卡上跑同一目标可反映真实 RVV 行为。同目录 `Makefile` 中另有 `deploy_test` 等目标供实际设备使用。

板卡输出日志位于 `test-rvv/sample_consensus/plane_models/log/board/`，常用文件包括：

- `run_test.log`：`make run_board_test` 的完整 GTest 输出，包含 correctness 与测试内的性能报告块；
- `analyze_bench_compare.log`：`make run_board_bench_compare` 的 Std/RVV 表格化基准输出，包含上下文（设备、VLEN、数据集点数、迭代次数）与每个条目的 Avg/Total/Speedup；
- `run_bench_std.log`、`run_bench_rvv.log`：compare 脚本消费的两侧原始 bench 输出。

### 4.3 基准方法与结果

`test-rvv/sample_consensus/plane_models/src/bench_sac_normal_plane.cpp` 直接调用 `*_Standard` 与 `*_RVV`，绕过对外接口内的自动 dispatch，便于对比 `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 三条路径。

```bash
cd test-rvv/sample_consensus/plane_models
make run_bench
```

未定义 `__RVV10__` 时 bench 会跳过 RVV 分支并打印警告。

板卡侧 `analyze_bench_compare.log` 的 rerun 结果（`Iterations: 50`，`Dataset: sac_plane_test.pcd (3283 points)`）如下。该表直接来自日志中的 Avg/Speedup 列，便于与后续改动对比：

| Item | Std Avg (ms) | RVV Avg (ms) | Speedup |
|---|---:|---:|---:|
| selectWithinDistance | 0.6845 | 0.0699 | 9.79x |
| countWithinDistance | 0.6797 | 0.0590 | 11.52x |
| getDistancesToModel | 0.7702 | 0.0732 | 10.52x |

`make run_board_test` 的 GTest 输出也会打印另一组以 1000 iterations 计时的报告块。下面摘录三条 normal-plane 入口（见 `test-rvv/sample_consensus/plane_models/log/board/run_test.log`）：

```text
[ Performance Report: NormalPlane (Select Within Distance) ]
Points per cloud    : 2000
Iterations          : 1000
Standard Total Time : 397.3278 ms
RVV Total Time      : 40.5289 ms
Speedup (Std/RVV)   : 9.8036x

[ Performance Report: NormalPlane (Weighted Distance) ]
Points per cloud    : 10000
Iterations          : 1000
Standard Total Time : 1568.0555 ms
RVV Total Time      : 161.3424 ms
Speedup (Std/RVV)   : 9.7188x

[ Performance Report: NormalPlane (getDistancesToModel) ]
Points per cloud    : 2000
Iterations          : 1000
Standard Total Time : 457.6930 ms
RVV Total Time      : 39.8735 ms
Speedup (Std/RVV)   : 11.4786x
```

Phase 000 的结构化证据见：

- `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/result.zh.md`
- `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json`
- `test-rvv/sample_consensus/plane_models/doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md`

Evidence Doctor（证据体检）结果为 Errors=0、Warnings=0、Suggestions=0。需要注意，板卡 compare 的计时入口是 protected helper bench wrapper，因此它证明 hot path 性能；公开入口是否命中 RVV 与 fallback 是否保持语义由 QEMU / board unit test 证明。

phase 020 已把 manifest / doctor / registry 刷新接入 topic-local Make target：

```bash
make -C test-rvv/sample_consensus/plane_models run_board_evidence_doctor
make -C test-rvv/sample_consensus/plane_models record_board_evidence_state
make -C test-rvv/sample_consensus/plane_models evidence_status
```

当前 `evidence_status` 输出 fresh。`log/evidence_registry.json` 只登记 summary artifact（摘要证据产物）的 digest（摘要指纹）和 doc refs，不改变 raw logs 默认不提交的边界。phase 030 已新增并运行 repeated board summary target，结果写入：

```text
test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase030-repeated-board/summary.md
```

5-run repeated summary 显示 `selectWithinDistance` median 9.64x / min 9.24x、`countWithinDistance` median 12.72x / min 12.56x、`getDistancesToModel` median 12.02x / min 11.66x。Repeated Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0，`repeated_evidence_status` 输出 fresh。

Phase 050 进一步为代表性 AoS source 点型补了 protected helper hot path 的 5-run board summary：

```text
test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase050-representative-aos-source-performance/summary.md
```

`PointXYZI + Normal` 的三条 helper median speedup 分别为 8.04x、6.10x、5.41x，min 分别为
7.40x、6.05x、4.49x。`PointXYZINormal + Normal` 的三条 helper median speedup 分别为
7.37x、8.52x、8.44x，min 分别为 6.60x、7.88x、8.22x。Phase 050 Evidence Doctor
为 Errors=0、Warnings=5、Suggestions=0；Warnings 来自组内收益差异和 `PointXYZI`
`getDistancesToModel` 长尾，处理方式是逐点型、逐 helper 报告，不按组均值外推。

Phase 060 进一步补了代表性 normal layout correctness，不新增性能 summary。Phase 070 再补
`PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的
4 个代表性交叉组合 correctness，同样不新增性能 summary。`run_normal_plane_public_tests`
在 QEMU/RVV 下 13/13 通过，`run_test_compare` 在 QEMU 下 Std/RVV 各 32/32 通过，板卡
`run_board_normal_plane_public_tests` 13/13 通过。新增覆盖包括 `PointXYZ + PointNormal`、
`PointXYZ + PointXYZINormal` 的 public-vs-direct RVV 对拍、non-AoS registered normal fallback，
以及 4 个 source × normal 交叉组合 public-vs-direct RVV correctness。

---

## 5. 当前采用的优化方式

当前生产代码采用一个保守的 `f32m2` RVV helper family（RVV helper 实现族）：公开入口只在 source 点型满足 `RVVXYZAoSFloatLayout<PointT>`、normal 点型满足 `normal_x/y/z/curvature` 单 float AoS-compatible layout、且 source / normal 点云规模都不超过 32-bit byte offset 上界时进入 RVV。其它模板实例、非 RVV 构建、normal 缺失、模型无效、unsupported layout 和超出 offset 上界的输入保持 Standard fallback（标准回退路径）。

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| 公开入口 dispatch | adopted | `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 只保留短路分流，RVV 不满足条件时直接调用 Standard helper。 | `run_normal_plane_public_tests` 13/13；board public alias 13/13。 | 只覆盖当前三条 normal-plane 距离入口。 |
| source layout gate | adopted | source 侧 helper 使用 byte-offset indexed gather，因此需要 AoS、standard-layout 和 float `x/y/z` 字段前提。 | Phase 040 public correctness；non-AoS source fallback。 | 更多 source 点型需要新的 point-type expansion phase。 |
| normal layout gate | adopted | normal 侧同时读取 `normal_x/y/z` 和 `curvature`，单独使用 normal/curvature AoS-compatible gate，避免把 source gate 外推到 normal cloud。 | Phase 060 public correctness；non-AoS normal fallback。 | 更多 normal-like 点型需要新的 normal-layout phase。 |
| `selectWithinDistance` 写回 | adopted | RVV 用 mask + `vcompress` 生成连续 inlier / distance 块，因此公开入口和 helper 都保证输出缓冲区足够大，再按实际命中数收缩。 | `SelectHelperResizesEmptyOutputBuffers`；Phase 000 public / helper tests。 | 内点很少时会短暂占用 `indices_->size()` 级别缓冲区。 |
| 距离算术 | adopted with precision boundary | RVV 复用已有 `distRVV_f32m2` 和 `getAcuteAngle3DRVV_f32m2`，内部以 float 计算，写回时宽化到 double。 | Std/RVV compare 32/32；阈值附近使用容差。 | 不等价于 `Scalar=double` 实现族；需要 double helper 时另开设计。 |
| helper hot-path 性能 | adopted for current evidence boundary | protected helper bench 隔离三条热点路径，证明当前 RVV 实现足以覆盖 staging 和 gather 成本。 | Phase 030 和 Phase 050 repeated board summary。 | 公开入口完整计时不是当前性能主证据。 |
| 新 RVV family selection | deferred | 当前没有 Evidence Doctor 或 correctness 信号要求替换 `f32m2` family。 | roadmap 标记为 deferred。 | 只有出现数值或性能反转证据时再做 RVV-vs-RVV A/B。 |
| `Scalar=double` | deferred | 需要新的 double helper、误差预算、反汇编和板卡证据，已经超出当前 float production patch 完善范围。 | Phase 070 expansion queue。 | 用户明确选择后另开 phase。 |

每个 VL chunk（可变向量长度分块）内，RVV 路径先加载 `indices_`，分别计算 source 和 normal 点云的 byte offset，再用 indexed gather 读取坐标、法线和 curvature。`countWithinDistanceRVV` 只做 mask popcount；`getDistancesToModelRVV` 对每个 index 写回一个 double distance；`selectWithinDistanceRVV` 先按阈值生成 mask，再压缩索引和距离并连续写回。这个组织方式没有改变 public API，也没有改变 `indices_` 的输出顺序语义。

## 6. 正确性与高效性证据链

| 证据层 | 当前证据 | 结论 | 边界 |
| --- | --- | --- | --- |
| correctness（正确性） | `run_normal_plane_public_tests`：13/13；`run_test_compare`：Std/RVV 各 32/32。 | 当前公开入口、fallback、helper resize、代表性 source、代表性 normal layout 和 source × normal 交叉 correctness 已闭合。 | QEMU 只证明 correctness 和日志形状，不证明真实性能。 |
| production path（生产路径） | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` 的公开入口 dispatch 到 `*_RVV` / `*_Standard` helper。 | RVV 是真实 production path，不是只存在于 test-only wrapper 的诊断代码。 | helper performance 仍由 protected helper bench 隔离测量。 |
| fallback | non-AoS registered source、non-AoS registered normal、curvature 非 float normal 和非 RVV 构建均保持 Standard helper。 | 不满足 RVV layout 或编译条件时保持原标量语义。 | 非法 index 仍依赖 PCL 既有输入合同，不在当前 topic 扩展。 |
| performance（性能） | Phase 030 5-run board summary：`selectWithinDistance` 9.64x median / 9.24x min，`countWithinDistance` 12.72x median / 12.56x min，`getDistancesToModel` 12.02x median / 11.66x min。 | `PointXYZ + Normal` protected helper hot path 为 positive-stable。 | 不外推到完整公开入口计时或其它点型全集。 |
| representative source performance | Phase 050 5-run board summary：`PointXYZI + Normal` 为 8.04x / 6.10x / 5.41x median，`PointXYZINormal + Normal` 为 7.37x / 8.52x / 8.44x median。 | 代表性 source AoS stride 增大后，三条 helper 仍为 positive-stable。 | Evidence Doctor Warnings 已解释，结论按点型和 helper 独立报告。 |
| Evidence Doctor（证据体检） | Phase 000 与 Phase 030 为 Errors=0、Warnings=0、Suggestions=0；Phase 050 为 Errors=0、Warnings=5、Suggestions=0。 | 当前性能摘要没有未处理 Error；Warnings 不改变 positive-stable 桶，但限制外推。 | Phase 060 / 070 没有新增性能摘要，因此没有新增 doctor 输入。 |
| freshness（证据新鲜度） | `evidence_status`、`repeated_evidence_status`、`phase050_evidence_status` 输出 fresh。 | 文档引用、summary artifact 和 registry 一致。 | raw logs 默认不提交；提交时只保留 summary docs 和 topic-local 文档。 |

## 7. Production Closeout

| 项 | 最终状态 | 证据 |
| --- | --- | --- |
| 生产补丁范围 | 仅修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` 中 normal-plane 三条距离入口和对应 helper；不改变 public API。 | production diff；topic-local evaluation Traceability Map。 |
| 覆盖范围 | source AoS byte-offset layout、normal/curvature AoS-compatible layout、`Scalar=float` RVV 内部计算、ordered `indices_` gather。 | Phase 040 / 060 / 070 public correctness；Phase 030 / 050 board repeated summaries。 |
| 不覆盖范围 | 泛型点类型全集、更多 normal-like layout、`Scalar=double`、完整公开入口性能、新 RVV family selection。 | Phase 070 result、optimization roadmap 和 optimization matrix。 |
| fallback 矩阵 | 非 RVV build、unsupported source layout、unsupported normal layout、curvature 非 float 和 byte offset 超界均进入 Standard helper。 | public fallback tests；源码 gate。 |
| 反汇编归属 | 三个 normal-plane RVV helper 符号中可定位 RVV 指令。 | `dump_bench_rvv` 和 Phase 000 result。 |
| 文档结构 | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、roadmap、code map、phase suite、evaluation 和长期 `doc-rvv` 已分工。 | Phase 010 doc suite inventory；当前 topic artifact tracking。 |
| 提交边界 | topic-only；不提交 `build/`、`output/`、`config.mk`、raw logs 或私有板卡信息。 | README 当前可提交证据表；提交前路径限定 staging。 |

当前建议结束本 topic 的优化推进。原因是 phase 000-070 已关闭当前授权范围内的 production dispatch、fallback、helper resize、代表性 source / normal / cross correctness、代表性 source 性能、doc suite 和 evidence registry；roadmap 剩余项都需要新的范围选择，继续推进会从“完善当前 adopted patch”变成“新 point-type / performance / double helper expansion”。

## 8. 总结

本仓库在 NormalPlane 上为 RVV 做的改动可归纳为三类：

1. 输出缓冲区从动态追加改为预分配定址，以支持掩码压缩写回；
2. 算术复用既有 `f32m2` 平面距离与锐角向量函数，减少分叉；
3. 对外仍保持 `double` 距离向量与原有索引语义。

当前 adopted production behavior 的边界是 source AoS byte-offset layout + normal/curvature AoS-compatible layout + 32-bit byte offset 上界 + 既有 `f32m2` helper。`PointXYZ + Normal`、`PointXYZI + Normal` 和 `PointXYZINormal + Normal` 有 repeated board helper performance；其中新增 source 点型性能只覆盖 protected helper hot path，公开入口 dispatch / fallback 仍由 correctness 测试证明。`PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` 以及 Phase 070 的 4 个 representative source × normal 交叉组合已有公开入口 correctness；非覆盖 source 或 normal layout 会回退到标量路径。phase 010 已把测试 / bench 源码迁入 `src/`，并收敛 board harness 的远端 fixture 传参；phase 020 已补齐 topic-local Evidence registry / manifest alias 自动化；phase 030 已把 `PointXYZ + Normal` protected helper hot-path 性能证据升级为 5-run positive-stable；phase 040 已关闭 source AoS gate 和代表点型 fallback/correctness；phase 050 已关闭 `PointXYZI` / `PointXYZINormal` 代表性 source helper performance；phase 060 已关闭代表性 normal layout correctness；phase 070 已关闭代表性 source × normal 交叉 correctness。泛型点类型全集、更多 normal-like 点型、公开入口性能和 `Scalar=double` 留给后续 topic-local phase。
