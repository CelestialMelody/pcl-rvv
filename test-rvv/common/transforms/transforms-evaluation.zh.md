# `common/include/pcl/common/impl/transforms.hpp`：函数级梳理、筛选评估与 RVV 优先级

本文档记录 `impl/transforms.hpp` 的函数级筛选结论。该文件已在 `doc-rvv/common/module-evaluation.zh.md` 的 common 模块第二次筛选中列为优先处理对象；本文件对应第三次筛选，并为后续 bench、自动向量化诊断和 RVV 实现确定范围。

---

## 1. 函数 / 函数组梳理

`transforms.hpp` 主要提供点云刚体 / 仿射变换。公开入口在 `common/include/pcl/common/transforms.h`，具体循环实现在本文件。

| 函数 / 函数组 | 功能概要 | 复杂度与访存 | 当前判断 |
| --- | --- | --- | --- |
| `detail::Transformer<Scalar>::se3` | 单点 xyz 经过 4x4 矩阵变换 | 单点 3 路输入、3 路输出；无批量循环 | 作为批量 RVV helper 的标量参考 |
| `detail::Transformer<Scalar>::so3` | 单点 normal 经过 3x3 旋转 | 单点 3 路输入、3 路输出 | 作为 normal RVV helper 的标量参考 |
| `transformPointCloud(cloud, cloud_out, Matrix4, copy_all_fields)` | 整云 xyz 变换 | 对 `cloud.size()` 线性遍历；dense 路径无有效性检查 | 高优先级，第一轮 RVV 覆盖 dense、非 indexed、`float` 矩阵 |
| `transformPointCloud(cloud, indices, cloud_out, Matrix4, copy_all_fields)` | indices 子集 xyz 变换 | 对 `indices.size()` 遍历；源点为 gather | 中优先级，第一轮暂缓，后续可评估 gather RVV |
| `transformPointCloud(PointXY, Affine2f)` | 2D 点 xy 变换 | 对 `cloud.size()` 线性遍历；每点 2D affine | 中优先级，可作为后续轻量扩展 |
| `transformPointCloudWithNormals(cloud, cloud_out, Matrix4, copy_all_fields)` | 整云 xyz + normal 变换 | 每点同时处理 xyz 与 normal 两组 3 float | 高优先级，第一轮 RVV 覆盖 dense、非 indexed、`float` 矩阵 |
| `transformPointCloudWithNormals(cloud, indices, cloud_out, Matrix4, copy_all_fields)` | indices 子集 xyz + normal 变换 | gather 读源点；写连续输出 | 中优先级，第一轮暂缓 |
| `transformPointCloud(offset, rotation)` / `transformPointCloudWithNormals(offset, rotation)` | 包装入口 | 先组装 Eigen transform，再调用 Matrix4 入口 | 随底层 Matrix4 入口受益 |
| `transformPoint` / `transformPointWithNormal` | 单点变换 | 无批量循环 | 低优先级，暂不做 RVV |
| `getPrincipalTransformation` | PCA 主方向变换 | 主要调用 centroid / covariance / eigen33 | 不属于本轮 transforms RVV 范围 |

---

## 2. 筛选标准

| 维度 | 判断口径 | 对本文件的影响 |
| --- | --- | --- |
| 循环规模 | 是否随点数 `N` 增长 | 整云变换和 normals 变换是主目标 |
| 算术密度 | 每点矩阵乘加量 | xyz 路径约 9 乘加 + 平移；normals 路径另有 9 乘加 |
| 访存规整性 | 源 / 目标点字段是否可按 AoS stride 访问 | dense 非 indexed 可用 `rvv_point_load/store` 的 strided segment helper |
| 分支复杂度 | 是否存在逐点 NaN 检查、copy、indices | non-dense 与 indexed 第一轮暂缓 |
| 数值语义风险 | 标量 `Scalar=double` 与 float RVV 的差异 | 第一轮只对 `Scalar=float` 分发，double 保持标量 |
| 可测试性 | 是否已有单元测试与稳定参考 | 上游 `test/common/test_transforms.cpp` 可迁移，另补大规模随机对拍 |

---

## 3. 优先级与候选总表

| 优先级 | 状态 | 函数 / 函数组 | 第一轮优化方向 | 主要风险 | 预期收益 | 回退条件 |
| --- | --- | --- | --- | --- | --- | --- |
| 高 | 待实施 | `transformPointCloud` 整云 dense、非 indexed、`Scalar=float` | AoS strided load xyz，RVV FMA 后 strided store xyz | in-place 输入输出别名；PointT 字段布局 | 点数大时中高 | bench 无收益、QEMU/板卡异常或布局不兼容 |
| 高 | 待实施 | `transformPointCloudWithNormals` 整云 dense、非 indexed、`Scalar=float` | xyz 走 SE3，normal 走 SO3；两组 strided load/store | normal 字段偏移兼容性；寄存器压力 | 点数大且含 normal 时中高 | 同上 |
| 中 | 暂缓 | indices 版 `transformPointCloud` / `transformPointCloudWithNormals` | 后续评估 indexed gather + 连续写 | gather 成本、copy_all_fields 行为 | 数据量大且 indices 连续时可能有收益 | gather 性能不稳定则保留标量 |
| 中 | 暂缓 | `PointXY` 2D affine | 可做 x/y 两路 strided load/store | 收益受点数与字段布局影响 | 中 | bench 无收益 |
| 低 | 暂缓 | 单点 `transformPoint` / `transformPointWithNormal` | 不做 RVV | 无批量并行空间 | 低 | 不适用 |
| 低 | 暂缓 | `getPrincipalTransformation` | 不在本文件直接处理 | 依赖 covariance / eigen33 | 低 | 不适用 |

---

## 4. 第一轮实现约定

第一轮只覆盖以下范围：

- `cloud_in.is_dense == true`；
- 非 indices 入口；
- `Scalar=float` 的 4x4 矩阵；
- `PointT` 满足 `pcl::rvv_load::kRVVXYZPointCompatible<PointT>` 与 `pcl::rvv_store::kRVVXYZPointCompatible<PointT>`；
- `n >= 16` 时进入 RVV，小点云回退标量；
- `cloud_in` 与 `cloud_out` 可为同一对象，RVV helper 在每个条带中先完成 load 再 store，保持 in-place 安全；
- `Scalar=double`、non-dense、indices、PointXY 第一轮保持标量。

`PointXYZRGBNormal` / `PointNormal` 的 normal 路径需要 `normal_x`、`normal_y`、`normal_z` 为标准布局中的 float 字段。实现时应独立增加 normal 字段兼容 trait，避免对不含 normal 的点类型实例化 normal RVV helper。

---

## 5. 与后续流程衔接

1. Baseline：在 `test-rvv/common/transforms` 下建立 `bench_transforms.cpp`，分别运行 `make run_bench_std` 与 `make run_bench_rvv`。
2. 单测：迁移 `test/common/test_transforms.cpp` 的核心用例，补充 `n >= 16` 的 dense 大点云用例，覆盖 `PointXYZ` 与 `PointXYZRGBNormal`。
3. 自动向量化诊断：`generate_vec_report` 聚焦 `pcl/common/include/pcl/common/impl/transforms.hpp`，用于记录标量路径中循环、模板和内存访问导致的诊断信息。
4. RVV 实现：在 `impl/transforms.hpp` 内加入 `transformPointCloudStandard` / `transformPointCloudRVV` 类似分发，保留现有公开 API。
5. 文档：完成后在 `doc-rvv/common/transforms-RVV.zh.md` 记录实现范围、回退条件、测试和 bench 结果。

---

## 6. 代码与测试索引

| 项目 | 路径 |
| --- | --- |
| 实现 | `common/include/pcl/common/impl/transforms.hpp` |
| 声明 | `common/include/pcl/common/transforms.h` |
| 上游单测参考 | `test/common/test_transforms.cpp` |
| RVV 单测 | `test-rvv/common/transforms/test_transforms.cpp` |
| RVV Bench | `test-rvv/common/transforms/bench_transforms.cpp` |
| 构建脚本 | `test-rvv/common/transforms/Makefile` |