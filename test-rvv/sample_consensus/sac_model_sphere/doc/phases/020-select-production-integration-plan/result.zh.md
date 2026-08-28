# Phase 020: selectWithinDistance production integration result

## 执行范围

本阶段按 PI1 计划进入 PI2-PI5 production integration loop（生产接入闭环）。实际修改范围只包含
`SampleConsensusModelSphere<PointT>::selectWithinDistance`：

- `selectWithinDistance` 公开入口保留原有 `isModelValid` 语义检查，然后在 `__RVV10__`、`RVVXYZFloatLayout<PointT>` 和 32-bit byte offset gate（32 位字节偏移准入条件）成立时分流到 `selectWithinDistanceRVV`。
- 原标量主体抽成 `selectWithinDistanceStandard`，作为非 RVV 构建、layout 不满足和超大 cloud 的 fallback（回退路径）。
- `selectWithinDistanceRVV` 只接管 indexed xyz gather（按 `indices_` 离散加载点坐标）、平方距离和球壳双边界判断；有序 `inliers.push_back` 与 `error_sqr_dists_` 的精确 `sqrt` 写回仍按标量顺序执行。
- `getDistancesToModel` 没有接入 production，当前测试专用 candidate 仍按负向证据处理。
- `countWithinDistance` 保持已有 production RVV 行为，只作为回归证据保留。

## 计划动作回填

| 动作 | 状态 | 证据 |
| --- | --- | --- |
| PI2 production patch | done | `sample_consensus/include/pcl/sample_consensus/sac_model_sphere.h` 新增 `selectWithinDistanceStandard` / `selectWithinDistanceRVV` protected helper；`impl/sac_model_sphere.hpp` 新增 public dispatch、Std helper 和 RVV helper。 |
| PI3 production direct correctness | done | `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare`；Std/RVV 各 4 个测试通过，新增 `ProductionSelectWithinDistanceMatchesStandardHelper`。 |
| fallback correctness | done | 同一测试在 Std 构建覆盖非 RVV fallback；RVV 构建中 direct helper 与 Standard helper 对拍。layout 不满足和超大 cloud 的 gate 通过代码分流保持 `selectWithinDistanceStandard`。 |
| asm attribution（反汇编归属） | done | `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv`；`selectWithinDistanceRVV` 符号内 RVV instruction count 为 17，`countWithinDistanceRVV` 为 19。 |
| PI4 board repeated evidence（板卡重复证据） | done | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_production_repeated_board_evidence`，5-run 均完成。 |
| Evidence Doctor（证据体检）与 registry（证据登记表） | done | `make -C test-rvv/sample_consensus/sac_model_sphere record_production_board_evidence_state` 生成 production manifest、doctor 和 registry 项。 |

## Production board 结果

板卡为 Milkv-Jupiter，bench case 为 `PointXYZ`、65536 点、direct indexed `indices_`、200 iterations、5 warmup。
speedup 使用 Std 构建耗时除以 RVV 构建耗时，性能结论只来自板卡 repeated 结果。

| case | B/A values | median | min / max | decision |
| --- | --- | --- | --- | --- |
| public `selectWithinDistance` | `1.5129, 1.4532, 1.4995, 1.5020, 1.5035` | `1.5020x` | `1.4532x / 1.5129x` | adopted for current production scope |
| public `countWithinDistance` | `3.5153, 3.5116, 3.5328, 3.5468, 3.5177` | `3.5177x` | `3.5116x / 3.5468x` | regression remains positive |
| public `getDistancesToModel` | `0.9952, 0.9900, 0.9807, 0.9849, 0.9951` | `0.9900x` | `0.9807x / 0.9952x` | scalar-only; no production RVV dispatch |
| diagnostic candidate `getDistancesToModel` | `0.7844, 0.7704, 0.7726, 0.7775, 0.7882` | `0.7775x` | `0.7704x / 0.7882x` | rejected for current candidate family |

## Evidence Doctor 处理

`production-repeated-evidence-doctor.md` 结果为 `Errors=2, Warnings=0, Suggestions=0`。两个 Error 都来自未接入的
`getDistancesToModel`：

- public `getDistancesToModel` 5/5 低于 1，且 manifest 中 `rvv_instr_count=0`，只能说明当前 public entry 在 Std/RVV 构建间接近中性或略退化；它不是本阶段 adopted 范围。
- diagnostic candidate `getDistancesToModel` 5/5 低于 1，继续支持 Phase 000 的拒绝结论；当前 scratch + 标量 `sqrt` 形态不进入 production。

`selectWithinDistance` production direct 行没有 Error / Warning，且符号级 RVV 指令归属闭合。因此上述 Error 已通过范围降级处理，不阻塞本阶段对 `selectWithinDistance` 的 adopted decision（采纳决策）。

## PI5 EvidenceDecision

本阶段 EvidenceDecision：

- `selectWithinDistance`：adopted production behavior（已采用生产行为），当前范围为 direct indexed `indices_`、`PointXYZ` board performance、`RVVXYZFloatLayout<PointT>` traits gate、`Eigen::VectorXf` coefficients、`double threshold` 输入但内部使用 float shell bounds。
- `countWithinDistance`：保持已有 adopted production RVV。
- `getDistancesToModel`：不接入 production；当前候选 family rejected，恢复条件是 RVV sqrt/helper 语义审计或新的 dense-store 消融。

## 未覆盖范围与下一阶段

当前阶段没有继续尝试 `vcompress` 写回。原因是 production direct 已经给出稳定约 `1.50x`，而当前 RVV helper 的维护成本低于 `vcompress` 双输出压缩方案。`vcompress` 仍可作为后续性能消融，但它需要 RVV-vs-RVV detail A/B（同一生产边界内两个 RVV 实现对比）、error distance 的精确 `sqrt` 语义复核和新的板卡 repeated 结果。

`point_type_expansion_queue` 保留为后续 phase：当前 correctness 覆盖 `PointXYZI`，board performance 覆盖 `PointXYZ`；若要把性能结论扩到 `PointXYZRGB`、`PointXYZRGBA` 或自定义 registered xyz 点型，需要单独补 production direct correctness、fallback、asm、board repeated 和 Evidence Doctor。

## 继续 / 停止判断

本阶段已闭合用户授权的 production scope：接入后真实 public `selectWithinDistance` 板卡 repeated 有稳定收益，因此保留 production patch 并创建正式 production `doc-rvv`。当前不继续自动做 `vcompress` 或更多点类型扩展，因为它们会引入新的实现族或扩大点类型性能结论，适合作为单独后续 phase。
