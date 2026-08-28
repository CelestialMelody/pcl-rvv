# Phase 050 Plan: line getDistances RVV double store shape

## 阶段意图和边界

本阶段只尝试 `SampleConsensusModelLine<PointT>::getDistancesToModelRVV` 的 store-shape（写回形态）替换：把当前 `vfsqrt` 后的 `float scratch + 标量 lane 转 double 写回`，改成 RVV `vfwcvt`（float 到 double 拓宽转换）加 `vse64`（64 位向量写回）直接写入 `std::vector<double>`。

本阶段不改变 public API（公开接口）、不改变 count/select 两个入口、不扩大点型、`Scalar`、layout 或 row source（行来源）范围。当前 production boundary（生产边界）仍是 `PointXYZ` 风格 float xyz AoS（结构数组）、direct indexed `indices_`、signed 32-bit `pcl::index_t`、u32 byte offset gate（32 位字节偏移门禁）和 RVV 构建。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| 已采纳 production family | Phase 040 已采纳 count/select/getDistances 的 indexed gather + `f32m2` family。 |
| 当前 getDistances 写回 | `getDistancesToModelRVV` 先 `vfsqrt` 得到 `vfloat32m2_t`，再 `vse32` 到 `chunk_distances`，最后标量 loop 写 `std::vector<double>`。 |
| sibling 形态 | `sac_model_normal_plane.hpp` 已使用 `vfwcvt_f_f_v_f64m4` + `vse64` 直接写 `std::vector<double>`。 |
| Phase 040 production direct 基线 | public getDistances median/min/max `3.4036x / 3.3409x / 3.5229x`，Std avg `2.227100`，RVV avg `0.651839`，Evidence Doctor 0/0/0。 |
| 当前问题 | 标量 lane 写回可能增加额外 scratch 内存流量和标量后处理；是否值得替换必须由同边界 correctness、asm 和板卡证据决定。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `getDistances-vfsqrt-vse64-store` | RVV 直接拓宽并写回 double 能减少 scratch store/load 和标量 lane loop，可能改善 getDistances production RVV 耗时。 | `vfloat64m4_t` 可能增加寄存器压力和写回带宽；收益可能很小或退化。 |
| 当前 `getDistances-vfsqrt-scratch-scalar-store` | 已有 Phase 040 证据 positive-stable，是保守回退基线。 | 保留标量 lane loop，代码形态不如 normal-plane 干净。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `getDistances-vfsqrt-scratch-scalar-store` | direct indexed `indices_` | `PointXYZ`, float xyz AoS, dense `std::vector<double>` output | Phase 040 production public `getDistancesToModel` | `run_test_compare` pass | `collect_repeated_board_production_evidence` | median `3.4036x` vs Std | `vfsqrt` + `vse32` scratch | 0/0/0 | adopted baseline before Phase 050 | compare against new store shape |
| `getDistances-vfsqrt-vse64-store` | direct indexed `indices_` | same as above | Phase 050 production public `getDistancesToModel` | `run_test_compare` | new Phase 050 production repeated evidence | pending | `vfsqrt` + `vfwcvt` + `vse64`; no scratch `vse32` requirement | pending | planned | implement and verify |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED asm gate | 更新 `check_line_production_asm.py`，要求 `getDistancesToModelRVV` 出现 `vfwcvt` 和 `vse64`，且不再要求 `vse32`。运行 `make check_production_asm`。 | 当前旧实现应失败，且失败原因是缺少拓宽转换 / 64 位写回。 |
| production patch | 修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp`。 | 删除 `chunk_distances` 和 lane 标量写回；保留原 fallback gate 和距离公式。 |
| GREEN asm gate | `make -C test-rvv/sample_consensus/sac_model_line check_production_asm`。 | 三个 production helper 归属通过，getDistances helper 命中 `vfwcvt` 和 `vse64`。 |
| correctness | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare`。 | Std/RVV 各 7 个 gtest 全部通过。 |
| board repeated | 新增 Phase 050 repeated target，使用 `SSH_AUTH_SOCK=<ssh-agent-socket>` 采集。 | 5-run 完成，public getDistances 仍 positive；同时记录 count/select 是否受同一 binary 影响。 |
| Evidence Doctor / registry | 新增 Phase 050 manifest / doctor / registry target 或等价记录。 | Errors / Warnings / Suggestions 已解释，freshness check 通过。 |

## Evidence Doctor 和 registry 规则

Phase 050 使用新的 summary evidence 路径：

- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/050-line-get-distances-vse64-store/production-repeated-evidence-doctor.json`

`log/evidence_registry.json` 只作为 local registry（本地证据登记表）使用，不默认提交。若 summary 覆盖 Phase 040 的方向、decision bucket 或 Evidence Doctor 结果，必须同步 evaluation、optimization evidence、roadmap、matrix、`doc-rvv` 和 Handoff。

## 板卡复跑预算和决策桶

- 默认运行 5-run repeated board（板卡重复测试）。
- 若 Phase 050 getDistances public RVV 相对 Std 仍明显大于 1，且 Evidence Doctor 0/0/0，则 production path 仍可保留。
- 若 Phase 050 RVV avg 明显低于 Phase 040 RVV avg，或 median/min/max 显示候选相对 Phase 040 退化，则把新写回形态标为 attempted/rejected，并回滚到 scratch+scalar store。
- 若 Phase 050 与 Phase 040 差异很小但方向不稳定，不做无限复跑；记录为 neutral / inconclusive，由 reviewer 判断是否为可读性 cleanup。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail（生产细节实现族比较）和 production-public（公开入口 Std/RVV 对比）。 |
| A/B boundary | A 是 Phase 040 已采纳 `getDistances-vfsqrt-scratch-scalar-store`；B 是 Phase 050 `getDistances-vfsqrt-vse64-store`。两者都在 `getDistancesToModel` production helper 边界内。 |
| 当前决策问题 | 新 store shape 是否比当前已采纳 family 更值得保留。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接改 production helper 并重跑 public entry。 |
| comparison-boundary / baseline mismatch 风险 | Phase 040 和 Phase 050 来自不同 binary / run batch，因此 RVV-vs-RVV 数值只能作为同 production boundary 的 detail A/B 参考；若 bucket 接近或摇摆，需要降级为 neutral。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 允许本阶段 probe，但如果退化或不稳定，回滚到 Phase 040 baseline。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；本阶段用 Phase 040 vs Phase 050 public getDistances RVV avg / median 做 detail A/B，并保留局限。 |

## 继续 / 停止条件

若新写回形态 correctness、asm、board repeated 和 Evidence Doctor 均闭合，并且 public getDistances 至少不退化，则同步文档并保留新形态。若新形态退化、asm 不稳定、QEMU correctness 失败或 Evidence Doctor Error 无法解释，则回滚 production patch 中的写回形态并把 Phase 050 写成 rejected / attempted。

本阶段完成后，仍不自动推进 `identity-index-strided-load` 或 `point-type-expansion`；它们会改变 production family 或扩大覆盖范围，需要单独 phase。
