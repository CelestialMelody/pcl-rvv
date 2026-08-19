# Phase 010 Result

## 当前结论

本阶段完成 production integration probe（生产接入探针），但 production public path（生产公开入口）
性能不支持采纳当前 RVV patch。正确性通过，性能退化来自真实公开入口中的额外实现成本和较小的
RVV 可覆盖比例。

## 实际生产 diff

- 在 `surface/include/pcl/surface/impl/organized_fast_mesh.hpp` 增加 `__RVV10__` 内部 helper。
- gate 收窄到 `PointInT == pcl::PointXYZ`、row/column step 均为 1、`store_shadowed_faces_ == true`。
- 先尝试所有 mesh type，后收窄为 adaptive-cut-only。

## 证据链

| evidence | result |
| --- | --- |
| QEMU correctness | `run_test_compare` Std/RVV 均 6/6 通过 |
| board correctness | `board_smoke` test 通过 |
| first production public board smoke | quad `0.78x`、right-cut `0.93x`、left-cut `0.95x`、adaptive-cut `1.21x` |
| adaptive-only production public board smoke | quad `1.00x`、right-cut `0.99x`、left-cut `0.99x`、adaptive-cut `0.89x` |
| Evidence Doctor | adaptive-only public run 为 Errors=3、Warnings=8、Suggestions=5 |

## 负向原因

诊断候选曾经正向，是因为它比较的是 test-only candidate 与 test-only reference；production probe 比较的是
真实 `OrganizedFastMesh::reconstruct` 公开入口。公开入口中 polygon 输出、分支和对象状态都计入耗时。

当前 RVV 片段只覆盖 finite mask 和 adaptive z 差值。polygon append、cell 分支、输出顺序保持仍是标量主成本。
第一版 production helper 还使用 `push_back`，而原标量实现使用预分配 `resize` + `idx` 原地写回，
这放大了输出容器成本。该问题已移交 Phase 020 做同构输出复测。

## EvidenceDecision

当前 production patch 判为 `rejected for adoption`，并已按用户确认回滚生产源码。
topic-local 测试、bench、证据和文档保留，用于记录 no-production 结论和后续复查依据。
