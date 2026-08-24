# Phase 010 结果：production integration PI1-PI5

## EvidenceDecision

`production_patch_positive_pending_user_confirmation`。

本阶段已完成有界 production patch（生产补丁）验证：`direct-fused-scale-accum` 接入 `TransformationEstimationSVDScale` 的 ordered public overload（公开顺序点云对入口），只覆盖 RVV 构建、dense、`Scalar=float`、layout-gated xyz AoS、等长、`nr_points >= 16`。source-indexed、dual-indexed、correspondence、`Scalar=double`、小规模、非 dense、退化 source variance 和非 RVV 构建都保持 fallback。

PI5 停止点：证据支持保留当前生产补丁，但还不能写成 adopted production behavior；需要用户检查确认是否采纳。

## 源码变更

| 文件 | 变更 | 边界 |
| --- | --- | --- |
| `registration/include/pcl/registration/transformation_estimation_svd_scale.h` | 新增 ordered cloud-pair public overload override；显式 `using` 父类 overload set，避免隐藏 indexed / correspondence 公开入口。 | 不新增 public API 形状，只覆盖已有 override。 |
| `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | 新增 RVV 条件编译的 scale accumulation / solve helper；override 先尝试 RVV，失败则显式调用父类 ordered overload。 | `__RVV10__` 内才引用 intrinsic；fallback 语义保持。 |
| `test-rvv/registration/transformation_estimation_svd_scale/src/test_tesvd_scale.cpp` | 扩展 production-facing correctness：public ordered 对拍、小规模 / 非 dense fallback、indexed / double 未覆盖入口保持正确。 | 仍保留 Phase 000 test-only candidate 对照。 |
| `Makefile` / scripts | 新增 production public-scale board repeated target；QEMU / board manifest 能区分 production public evidence 和 diagnostic evidence。 | raw logs 默认 local-only。 |

## Gate 结果

| gate | 结果 | 证据 |
| --- | --- | --- |
| PI2 compile | pass | Std/RVV `run_test_compare` 均完成编译。 |
| PI3 QEMU correctness | pass | Std/RVV 各 6 个 gtest 全通过。 |
| PI3 QEMU public smoke | pass | `public-scale` 4K/64K/256K log shape 正常；QEMU Doctor `Errors=0`、`Warnings=0`。 |
| PI4 ASM attribution | pass | production ordered overload 符号内可见 `vlsseg3e32.v`、`vfmacc.vv`、`vfredosum.vs`、`vsetvli`，并调用 `solveTransformationEstimationSVDScaleF32`。 |
| PI4 board production repeated | positive | 5 runs、20 iterations、5 warmup；4K/64K/256K median B/A = `26.123x` / `33.860x` / `33.066x`。 |
| PI4 Evidence Doctor | acceptable | board production Doctor `Errors=0`、`Warnings=1`；warning 为 4K 与组内 median 偏离，但 4K 自身 median 仍 `26.123x`、min `25.902x`。 |
| registry freshness | pass | `make ... evidence_status` 为 fresh。 |

## production 范围矩阵

| 入口 / 条件 | 当前行为 |
| --- | --- |
| ordered cloud-pair, RVV, `Scalar=float`, dense, layout-gated xyz AoS, `nr_points >= 16` | 命中 scale fused RVV production path。 |
| ordered cloud-pair, 非 RVV 构建 | 父类 scale path。 |
| ordered cloud-pair, `Scalar=double` | 父类 scale path。 |
| ordered cloud-pair, size mismatch | 父类错误语义。 |
| ordered cloud-pair, `nr_points < 16` | 父类 scale path。 |
| ordered cloud-pair, source / target 非 dense | 父类 scale path。 |
| ordered cloud-pair, source variance 退化 | 尝试 RVV solve 后 fallback 到父类 scale path。 |
| source-indexed / dual-indexed / correspondence | 未 override，保持父类路径。 |
| 泛型点型 | production code 使用 layout gate；本阶段直接证据只覆盖 `PointXYZ -> PointXYZ`，泛型点型仍是扩展队列。 |

## 证据摘要

| evidence | result |
| --- | --- |
| QEMU correctness | 6 tests：public ordered、fallback boundaries、uncovered entries、test-only candidate、小规模 candidate fallback、退化 candidate fallback。 |
| QEMU public-scale smoke | 4K/64K/256K smoke B/A = `3.80x` / `4.52x` / `4.54x`；仅 log-shape，不作真实性能结论。 |
| board production repeated | 4K B/A values `26.714, 26.123, 25.902, 25.952, 26.977`；64K `34.638, 33.838, 33.757, 33.912, 33.860`；256K `33.203, 33.066, 32.821, 32.883, 33.128`。 |
| checksum | Std/RVV 1e-6 量化 checksum mismatch；这是 RVV reduction tree 预期差异。correctness 由 gtest 和 fallback 对拍承担。 |

## PI5 结论

建议用户采纳当前 production patch，范围限定为本 result 的 production 范围矩阵。若采纳，下一步应创建或更新 `artifact_layout.topic_doc_template` 解析出的 production 长期主题文档，并把本状态从 pending user confirmation 改为 adopted。若不采纳，当前 patch 可回滚，保留 Phase 000/010 证据作为 diagnostic 和 production probe 记录。

## 后续优化矩阵

若当前 production patch 被采纳，后续可继续按独立 phase 推进：

- `generic-point-type-expansion`：对 `PointXYZI` / `PointXYZRGB` 等 layout-gated xyz AoS 点型补 correctness 和代表性 board evidence。
- `row-source-expansion`：source-indexed、dual-indexed、correspondence 逐 row source 独立验证。
- `matrix-local-scale-simplification`：作为非 direct fused 的局部 fallback / scalar-side 优化候选。
