# Phase 013 Plan：indexed direct gather point type / layout

## 阶段意图和边界

Phase 012 已证明 `correspondence-pair` direct index stream 在 `PointXYZI` /
`PointXYZRGB` 上保持 diagnostic positive。本阶段把同一 point type / layout 维度扩展到
已有的 indexed direct gather family：

- `source-indexed-cloud-pair` direct gather
- `dual-indexed-cloud-pair` direct gather

本阶段仍只修改 `test-rvv/registration/transformation_estimation_dual_quaternion`，
不修改 production TEDQ header，不启动 PI1。

## 优化矩阵

| candidate family | row_source_policy | point type / Scalar / layout | correctness | QEMU smoke | board | decision |
| --- | --- | --- | --- | --- | --- | --- |
| direct indexed gather | source-indexed-cloud-pair | `PointXYZI` / `PointXYZRGB` / `float` / xyz AoS | planned | planned | deferred | planned |
| direct indexed gather | dual-indexed-cloud-pair | `PointXYZI` / `PointXYZRGB` / `float` / xyz AoS | planned | planned | deferred | planned |

## 动作

| action | 产物 | 验证 | 完成判据 |
| --- | --- | --- | --- |
| A1 correctness | `src/test_tedq.cpp` | `make run_test_compare` | Std/RVV 都通过，新增四个 point-type direct gather 对拍。 |
| A2 bench filter | `src/bench_tedq.cpp` | QEMU smoke | `indexed-direct-gather-point-type-layout` 输出 source/dual × PointXYZI/RGB × 4K/64K/256K。 |
| A3 QEMU evidence | QEMU manifest / doctor / registry | `make record_qemu_*` | Evidence Doctor clean；QEMU 只作为日志形状和路径证据。 |

## 停止条件

如果 correctness 或 QEMU smoke 失败，本阶段停在 diagnostic blocked，并优先修
test-support 语义。若 QEMU clean，则下一步才考虑是否为该 filter 增加 board repeated
summary script；board 仍不代表 production dispatch。
