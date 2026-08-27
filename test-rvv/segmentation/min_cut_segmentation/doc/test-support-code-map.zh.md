# min_cut_segmentation 测试支撑代码地图

## 本文职责

本文定位 test support（测试支撑）代码和角色边界，不承担性能结论。所有代码均为 topic-local diagnostic，不被 production 包含。

## 总调用图

```text
production scalar source
  segmentation/include/pcl/segmentation/impl/min_cut_segmentation.hpp
        |
        v
topic-local semantic mirror
  include/min_cut_segmentation.h
        |
        v
  include/impl/min_cut_segmentation_components.hpp
        |                         |
        v                         v
src/test_min_cut_segmentation.cpp src/bench_min_cut_segmentation.cpp
        |                         |
        v                         v
QEMU correctness logs             board repeated summary / manifest / doctor
```

## 稳定聚合入口

| 入口 | 路径 | 职责 |
| --- | --- | --- |
| topic Makefile | `Makefile` | 构建、QEMU、asm、board repeated 和 doctor targets |
| board Makefile | `board.mk` | 远端板卡运行参数入口 |
| aggregator header（聚合头） | `include/min_cut_segmentation.h` | 给 test / bench 暴露统一 include |
| internal helper（内部 helper） | `include/impl/min_cut_segmentation_components.hpp` | 类型、reference、candidate、buildGraph-shaped helper |

## Fixtures 与输入构造

| fixture | 位置 | 语义 |
| --- | --- | --- |
| `makeCloud` | test / bench source | 生成 dense `PointXYZ` cloud |
| `makeIndices` | test / bench source | 完整或 stride indices |
| `makeForeground` | test / bench source | deterministic foreground points |
| `makeEdges` | test / bench source | Phase 000 component edge list |
| `collectBuildGraphEdges` | internal helper | Phase 010 用真实 KNN 和 duplicate marker 收集边 |

## 标量 Reference

| helper | production 映射 | 不能证明 |
| --- | --- | --- |
| `computeUnaryPotentialsStd` | `calculateUnaryPotential` 的 foreground min-distance 和 sink/source 权重 | graph 写入 |
| `computeBinaryPotentialsStd` | `calculateBinaryPotential` 的三维距离和 double `std::exp` | RVV float exp 严格等价 |
| `computeBuildGraphPotentialBatchStd` | `buildGraph()` 的 KNN / graph write 形态 | public `extract()` / max-flow |

## Candidate / Diagnostic Helper

| helper | 职责 | 状态 |
| --- | --- | --- |
| `computeUnaryPotentialsRVV` | input rows 内 RVV 化 foreground min-distance | diagnostic-positive |
| `computeBinaryPotentialsRVV` | edge rows 内 RVV 化 distance + float expf | diagnostic-positive |
| `computeBuildGraphPotentialBatchRVV` | buildGraph-shaped helper 内批量 potential 后写 graph | rejected for production；neutral |

## Bench Harness 与 Case Registry

| case-filter | helper path | timer boundary |
| --- | --- | --- |
| `unary_min_distance` | unary Std / RVV helper | component only |
| `binary_exp_weight` | binary Std / RVV helper | component only |
| `buildgraph_potential_batch` | buildGraph batch Std / RVV helper | KNN + Boost graph + duplicate marker |

## Scripts 与 Evidence Output

| 脚本 / 输出 | 作用 |
| --- | --- |
| `script/generate_min_cut_board_evidence_manifest.py` | 从 repeated run 的 compare log 生成 summary 和 manifest |
| `../../script/evidence_doctor.py` | 共享 Evidence Doctor |
| `log/board/repeated/summary.md` | Phase 000 component summary |
| `log/board/buildgraph-repeated/summary.md` | Phase 010 buildGraph-shaped summary |

## Production 与 Test Support 边界

production 文件未修改；topic helper 不参与 PCL public API，也没有 production dispatch（生产分流）。`doc-rvv` production 长期主题文档为 not_applicable with evidence，因为没有 adopted production behavior（已采用生产行为）。

## 拆分审计

当前结构符合 `artifact_layout`：`src/` 承载 test / bench source，`include/` 承载聚合头，`include/impl/` 承载内部 helper。单个 internal helper 目前低于 hard line limit，职责虽包含 types / reference / candidate / buildGraph-shaped helper，但仍可通过本 map、testing overview 和 optimization evidence 定位；不需要为 no-production closeout 继续拆文件。
