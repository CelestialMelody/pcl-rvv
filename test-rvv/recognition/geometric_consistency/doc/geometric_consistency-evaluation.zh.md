# recognition/geometric_consistency 函数级评估

## S2 函数级评估

目标源码是 `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp`。
公开入口 `GeometricConsistencyGrouping<PointModelT, PointSceneT>::recognize()` 进入
`clusterCorrespondences()`，其中排序后的 correspondences 会按 consensus set 做
pairwise distance consistency（成对距离一致性）检查，再经过 RANSAC rejector 和
transformation 输出。

当前阶段最初把 RVV 机会收窄为 component ablation：先验证固定 consensus set 下的
pairwise consistency predicate（谓词）是否能被 RVV 组织成稳定诊断信号。现在这段
谓词已经采纳进 production `clusterCorrespondences()`，而排序、RANSAC、
cluster growth 外层顺序和 transformation 输出仍保持原样。

## 初步判断

当前判断已经从 diagnostic（诊断）前进到窄范围 production behavior。原因是：

- production direct correctness 通过 `run_upstream_test_compare` 复核，真实
  `GeometricConsistencyGrouping<PointType, PointType>::recognize()` 在 Std/RVV 构建下都通过。
- 5-run repeated board 的诊断证据显示同一 pairwise kernel / cluster growth 形状有稳定收益。
- phase 020 又补了独立的 growth probe，median `2.51x`、min `2.50x`、max `2.56x`，
  `B/A < 1 = 0/5`，checksum 稳定。它说明 outer growth helper 也保持正向，
  但仍是 diagnostic / production-shaped diagnostic，不是新的 production boundary。
- 当前 production patch 只接入 `clusterCorrespondences()` 的 pairwise predicate；
  `std::sort`、`taken_corresps`、RANSAC rejector 和输出聚类顺序仍然按原实现保留。

## 本阶段证据

- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/summary.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/evidence_doctor.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase000_pairwise_consistency_diagnostic/evidence_manifest.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/summary.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_doctor.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase010_cluster_growth_diagnostic/evidence_manifest.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/summary.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_manifest.json`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.md`
- `test-rvv/recognition/geometric_consistency/log/board/repeated_phase020_cluster_growth_production_probe/evidence_doctor.json`

## 函数族作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `recognize()` | 公开入口 | 读 model / scene / correspondences，写 transformations | public entry | 已通过 upstream test compare |
| `clusterCorrespondences()` | 聚类主流程 | 排序、consensus growth、RANSAC、输出 cluster | 当前 production 母体 | 只接入 pairwise predicate |
| pairwise consistency predicate | 给定 `j` 和 consensus set，判断是否继续收进 cluster | 读 packed correspondence coordinates，写 bool | 当前 production patch 主目标 | 已采纳进 production |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `GeometricConsistencyGrouping::recognize` | production public entry | 公开入口 | 上游调用者 | `clusterCorrespondences()` | production boundary | `recognition/include/pcl/recognition/impl/cg/geometric_consistency.hpp` |
| `clusterCorrespondences` | production helper | 读 sorted correspondences，做 consensus growth | `recognize()` | RANSAC rejector / transformations | current production helper | 同上 |
| `pairwiseConsistencyScalarReference` | diagnostic reference | 复刻局部 pairwise predicate | test / bench | correctness oracle | correctness gate | `test-rvv/recognition/geometric_consistency/include/impl/gc_candidates.hpp` |
| `pairwiseConsistencyCandidate` | diagnostic candidate | RVV 诊断候选 | test / bench | output bool / path | component ablation | 同上 |
| `countConsistentCandidatesCandidate` | bench helper | 固定 consensus set 上的候选计数 | bench wrapper | summary / manifest | diagnostic bench | 同上 |
| `test_recognition_cg.cpp` | upstream correctness source | 真实 `recognize()` 和 `GeometricConsistencyGrouping` 回归 | `run_upstream_test_compare` | QEMU correctness | production direct correctness | `test/recognition/test_recognition_cg.cpp` |

## 实现方式审计

| 实现维度 | 当前状态 | 证据 | 边界 / 恢复条件 |
| --- | --- | --- | --- |
| dispatch / fallback | adopted narrow production | production helper 通过 `__RVV10__` 分流，非 RVV 构建自然回退标量 | 仍保留原 `clusterCorrespondences()` 语义 |
| layout / traits gate | adopted narrow production | `RVVXYZAoSFloatLayout` / `PointXYZ-like` 生产 gate | 其它不满足 gate 的模板实例保持标量 |
| staging / reduction | adopted narrow production | pairwise predicate 在 VL chunk 内做 gather、sqrt 和 compare | 外层 growth 和输出顺序暂不改 |
| production scope | adopted narrow production | upstream test compare 通过，board proxy 正向 | 如要扩大到 growth 外层，需新 phase |

## 测试计划

| 测试 | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | diagnostic correctness | 标量参考与 RVV 候选对拍 |
| `run_qemu_smoke` | QEMU smoke | 只证明可构建、可运行和日志形状 |
| `run_upstream_test_compare` | production direct correctness | 真实 `recognize()` 入口 Std/RVV 对拍 |
| `check_gc_rvv_asm` | asm gate | 证明候选路径命中预期 RVV 指令 |

## Bench 计划

`bench_gc` 仍只计时 pairwise consistency predicate 的 batch 版本，不包含 production
`clusterCorrespondences()`、`std::sort`、RANSAC rejector 或 transformation 输出。
它现在作为 production board proxy：说明同一局部 kernel 值不值得继续推进，
但不替代 `run_upstream_test_compare` 的 correctness 证据。

## 生产接入判断

当前 `doc-rvv` 已适用。若后续还要继续扩展，下一轮应新开 production-direct growth
phase，而不是把 Phase 010 的 diagnostic 结果外推成更宽的模板泛型结论。
