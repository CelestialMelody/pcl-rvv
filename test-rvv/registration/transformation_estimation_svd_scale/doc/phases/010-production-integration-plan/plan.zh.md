# Phase 010 计划：production integration PI1

## 阶段意图和边界

本阶段进入 production integration loop（生产接入闭环）的 PI1。目标是把 Phase 000 中已经 positive 的 `direct-fused-scale-accum` 收窄成可审查的 production patch 计划：先冻结 public dispatch（公开入口分流）、fallback（回退路径）、测试、bench、ASM 和 board gate，再进入 PI2 源码修改。

PI1 本身只承诺范围和证据计划；PI2 才允许修改 production 源码。PI5 仍必须停到用户检查点，不能自动把 patch 写成 adopted production behavior。

## 可接入范围冻结

| 维度 | PI1 冻结值 |
| --- | --- |
| production entry | `TransformationEstimationSVDScale<PointSource, PointTarget, Scalar>::estimateRigidTransformation(const pcl::PointCloud<PointSource>&, const pcl::PointCloud<PointTarget>&, Matrix4&) const`。 |
| row source | ordered-cloud-pair，仅 source / target 按相同下标配对的 public overload。 |
| point type / layout | RVV compile 时使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointSource>` 和 `PointTarget` gate；本轮直接证据只覆盖 `PointXYZ -> PointXYZ`。 |
| Scalar | `float` only；`double` 必须 fallback。 |
| input shape | `cloud_src.size() == cloud_tgt.size()`、dense、`nr_points >= 16`。 |
| math path | 直接累加 source sum、target sum、source-target cross sum 和 source square sum；保留 Eigen 3x3 SVD 后段；scale 使用 `trace(R * H) / source_variance`。 |
| production file | 目标为 `registration/include/pcl/registration/transformation_estimation_svd_scale.h` 和 `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp`。 |

## fallback 矩阵

| 条件 | 处理 |
| --- | --- |
| 非 RVV 构建 | 走既有父类 ordered public overload。 |
| `Scalar != float` | 走既有父类 ordered public overload。 |
| source / target layout 不满足 `RVVXYZAoSFloatLayout` | 走既有父类 ordered public overload。 |
| size 不等 | 保持既有错误语义，不进入 RVV helper。 |
| `nr_points < 16` | fallback，避免小规模 RVV setup cost。 |
| source 或 target 非 dense | fallback；NaN / Inf 不由本轮 RVV path 接管。 |
| degenerate source variance | fallback，保持既有 production scale path 语义。 |
| source-indexed / dual-indexed / correspondence overload | 本轮不 override，保持父类路径。 |

## production 设计

1. 在 scale subclass（子类）声明并定义 ordered cloud-pair public overload override。
2. 在 `impl/transformation_estimation_svd_scale.hpp` 的 RVV 条件编译 detail namespace 中新增 scale 专用 accumulation / solve helper。
3. override 先做 size mismatch 的既有错误语义，再尝试 RVV helper；失败时显式调用父类同名 ordered overload。
4. 生产注释只写 dispatch / fallback / numeric boundary，不复制 diagnostic 文档。

## PI2-PI5 gate

| gate | 目标 |
| --- | --- |
| PI2 compile | Std / RVV 都能编译；非 RVV 构建不引用 RVV intrinsic。 |
| PI3 correctness | 新增或扩展 gtest，验证 public ordered overload 与 reference 一致，小规模 / 非 dense / double / indexed fallback 不被误写成当前覆盖。 |
| PI3 benchmark | bench 新增 production public ordered path 对比，避免继续只测 test-only helper。 |
| PI4 evidence | QEMU correctness + QEMU smoke + ASM attribution + board production repeated + Evidence Doctor。 |
| PI5 closeout | 更新 evaluation、README、benchmark/evidence、optimization matrix / roadmap、phase result；停到用户检查点。 |

## 继续 / 停止条件

若 override 方案能在不改变公共 API 语义的前提下通过 Std/RVV 编译和 correctness gate，则进入 PI2。若发现必须扩大到 indexed / correspondence、泛型点类型策略或父类接口重构，停止并写 `production_integration_blocked`，不接源码。
