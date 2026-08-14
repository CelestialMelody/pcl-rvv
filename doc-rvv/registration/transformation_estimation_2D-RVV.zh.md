# transformation_estimation_2D RVV 窄范围生产候选

## 当前状态

`transformation_estimation_2D` 当前有一个窄范围 RVV production candidate，状态为：

```text
production-candidate-supported / user-review-pending
```

它不是泛型点云的通用 RVV 实现，也不覆盖 indexed / correspondence overload。

## 生产入口和 gate

当前 production dispatch 位于：

```text
registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
```

只有以下条件同时满足时才尝试 RVV：

- ordered-cloud-pair public overload；
- `PointSource == pcl::PointXYZ`；
- `PointTarget == pcl::PointXYZ`；
- `Scalar == float`；
- source / target 点数相等且不少于 16；
- 两侧 `is_dense == true`；
- 两侧所有点的 x/y/z 都是 finite。

不满足条件时继续使用原有 `ConstCloudIterator` 标量路径。indices、dual indices、correspondences、其它点型、`Scalar=double`、小输入、非 dense 或非有限输入都不在当前生产范围内。

## 实现形态

RVV helper 分两遍处理：

1. 用跨步分段加载读取 source / target 的 x、y、z，并求 x/y 质心。
2. 直接对中心化后的 x/y 累加 `H00/H01/H10/H11`。

`atan2`、`cos/sin`、平移计算和 4x4 矩阵写回保持标量。RVV helper 返回失败时，公开入口自然回退到原标量 iterator helper。

## 证据链

- correctness：Std / RVV 各 16/16。
- QEMU production-public smoke：Evidence Doctor `0/0/0`；只用于正确性、路径和日志形状。
- asm：public boundary 可归属 `vlsseg3e32.v`、`vfmacc`、`vfredosum`、`vfsub`、`vfadd`。
- 真实板卡：`Milkv-Jupiter`，5 runs，每 run 20 iterations、5 warmup。
  - 4K：`4.222x`，`positive`。
  - 64K：`5.310x`，`positive`。
  - 256K：`4.947x`，`positive`。
  - board Evidence Doctor：`0/0/0`。

板卡摘要路径：

```text
test-rvv/registration/transformation_estimation_2D/log/board/ordered_cloud_pair_public_repeated/summary.md
```

QEMU 计时不参与性能结论。

## 当前不覆盖的方向

Phase 030 的 source-indexed、dual-indexed、correspondence materialize-to-ordered candidate 已完成板卡诊断，但 row-source Evidence Doctor 为 `Errors=1、Warnings=2、Suggestions=6`，64K 有退化/长尾；它们仍是 test-only 诊断，不是生产批准。

## 采用边界

当前 patch 尚未自动提交。用户审阅前不得自动回滚、删除、覆盖或扩大 production patch。若要接入新的 row source，必须先建立新的实现族、正确性/回退合同、asm 归属和目标板卡证据，并重新执行 PI1-PI5。
