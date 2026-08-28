# sac_model_cylinder 测试支撑代码地图

## 本文职责

本文给 reviewer（审查者）和下一轮 worker（执行者）定位 test support（测试支撑）代码、bench wrapper、
script、output summary 和 production helper。性能结论见 `doc/benchmark-and-evidence.zh.md`，当前生产行为见
`doc-rvv/sample_consensus/sac_model_cylinder-RVV.zh.md`。

## 总调用图

```text
production public entries
  sample_consensus/include/pcl/sample_consensus/impl/sac_model_cylinder.hpp
        |
        | __RVV10__ gate hit                         fallback
        v                                           v
  count/select/getDistances RVV helpers      count/select/getDistances Standard helpers
        |
        v
test aggregation
  include/sac_model_cylinder.h
        |
        +--> src/test_sac_model_cylinder.cpp
        +--> src/bench_sac_model_cylinder.cpp
                 |
                 v
          board raw logs
                 |
                 v
          script/generate_cylinder_board_evidence_manifest.py
                 |
                 v
          production-repeated-evidence-manifest.json
                 |
                 v
          evidence_doctor.py -> production-repeated-evidence-doctor.md/json
                 |
                 v
          log/evidence_registry.json
```

Phase 000 的 `SampleConsensusModelCylinderDiagnostic` 仍保留为 historical production-shaped diagnostic（历史生产形态诊断）
交叉检查；当前采纳判断使用真实 public entry（公开入口）的 production direct（真实生产路径）证据。

## 代码与证据角色

| 符号 / 文件 | 层级 | 作用 | 上游 / 调用者 | 下游 / 输出 | 证据角色 |
| --- | --- | --- | --- | --- | --- |
| `SampleConsensusModelCylinder::countWithinDistance` | production public entry | 真实公开计数入口，按 RVV gate 分流。 | SAC / RANSAC caller | Standard / RVV helper | production boundary。 |
| `SampleConsensusModelCylinder::selectWithinDistance` | production public entry | 真实公开选择入口，写 `inliers` 和 `error_sqr_dists_`。 | SAC / RANSAC caller | Standard / RVV helper | production boundary / output order。 |
| `SampleConsensusModelCylinder::getDistancesToModel` | production public entry | 真实公开 dense distance vector（连续距离数组）输出入口。 | 距离消费者 | Standard / RVV helper | production boundary / dense output。 |
| `computeCylinderDistanceTermsRVV` | production RVV math helper | 在 VL chunk（可变向量长度分块）内计算 radial distance（径向距离）和 acute normal angle（锐角法线夹角）。 | 三个 RVV helper | mask / compress / store 后处理 | shared formula。 |
| `countWithinDistanceRVVCylinder` | production RVV helper | indexed xyz/normal gather、阈值 mask、`vcpop.m` 计数。 | public count | board / asm | production direct。 |
| `selectWithinDistanceRVVCylinder` | production RVV helper | 同一距离核，`vcompress.vm` 保序写 index，`vfwcvt + vse64` 写 double error。 | public select | board / asm | production direct。 |
| `getDistancesToModelRVVCylinder` | production RVV helper | 同一距离核，`vfwcvt + vse64` 写 dense double distance。 | public getDistances | board / asm | production direct。 |
| `include/sac_model_cylinder.h` | aggregator header（聚合头） | topic 稳定 include 入口。 | test / bench source | diagnostic helper 和 production header | reviewer 恢复入口。 |
| `SampleConsensusModelCylinderDiagnostic` | diagnostic helper | Phase 000 测试专用 candidate，保留历史交叉检查。 | test / bench source | diagnostic correctness / bench | historical diagnostic。 |
| `src/test_sac_model_cylinder.cpp` | correctness test | 对拍 public entry、Standard helper 和 diagnostic candidate。 | Makefile `run_test_compare` | QEMU / board gtest log | correctness gate。 |
| `src/bench_sac_model_cylinder.cpp` | bench wrapper | 生成 public production timing 和 historical diagnostic candidate timing。 | Makefile bench / board target | raw bench logs | board performance input。 |
| `script/check_cylinder_production_asm.py` | analysis script | 检查 production helper 的 RVV 指令归属。 | `make check_production_asm` | stdout / failure code | asm attribution。 |
| `script/generate_cylinder_board_evidence_manifest.py` | analysis script | 把 repeated board logs 和 asm 摘要转成 JSON manifest。 | production board logs | `production-repeated-evidence-manifest.json` | summary artifact。 |
| `log/evidence_registry.json` | evidence registry | 登记 manifest / doctor 文件和 doc refs。 | registry Make target | `production_evidence_status` | freshness check。 |

## Fixtures 与输入构造

test 和 bench 都构造 synthetic cylinder cloud：point 只读 xyz，normal 只读 normal_x/y/z。bench 默认 65536 点、
200 次迭代、5 次 warmup，`shuffled` index mode 通过相邻 pair 交换制造 direct indexed gather。near-threshold
（近阈值）样本只在 correctness 中覆盖；bench 数据刻意避开阈值附近，以减少数值噪声对性能判断的污染。

`getDistancesToModel` 的 bench checksum（校验和）只保护 dense vector 输出规模；逐项数值一致性由
`GetDistancesBenchShapedPublicEntryMatchesStandardHelper` 覆盖。

## 拆分审计

当前 topic 已采用 `src/`、`include/`、`include/impl/` 和 `script/` 分层；没有旧 `test_support/` 目录和 legacy alias。
`include/impl/sac_model_cylinder_diagnostic.hpp` 同时包含 scalar reference、RVV candidate 和布局 traits，但只服务历史
diagnostic helper。生产 helper 位于 production header 的 `pcl::detail`，便于公开入口直接短路分流和 fallback。

当前文件规模和职责划分仍可审查，不需要在本 phase 继续拆分。Phase 040 已补入代表点型 fixtures 和 manifest item
metadata；这些改动仍保持在现有 `src/` 与 `include/impl/` 分层内。若后续扩大到更多点型或真实 workload，再重新审计
是否需要把点型 fixtures、fallback assertions 或更多 manifest metadata 拆成独立 internal header。
