# Phase 060 Plan: line select RVV compressed double store

## 阶段意图和边界

本阶段只优化 `SampleConsensusModelLine<PointT>::selectWithinDistanceRVV` 中
`error_sqr_dists_` 的压缩写回形态：当前实现先把 `vcompress` 后的 `float`
平方距离 `vse32`（32 位向量写回）到临时数组，再用标量 lane（向量通道）
循环逐项转成 `double`。本阶段候选改为 RVV `vfwcvt`（float 到 double 拓宽转换）
加 `vse64`（64 位向量写回），直接写入 `error_sqr_dists_`。

本阶段不改变 public API（公开接口）、不改变 line 距离公式、不改变
`countWithinDistance` / `getDistancesToModel` 的实现族，不扩大点型、`Scalar`、
layout（布局）或 row source（行来源）范围。当前 production boundary（生产边界）
仍是 `PointXYZ` 风格 float xyz AoS（结构数组）、direct indexed `indices_`、
signed 32-bit `pcl::index_t`、u32 byte offset gate（32 位字节偏移门禁）和 RVV 构建。

## 当前状态清单

| 项 | 当前事实 |
| --- | --- |
| 已采纳 production family | Phase 040/050 已采纳 count/select/getDistances 的 indexed gather + `f32m2` family。 |
| 当前 select 写回 | `selectWithinDistanceRVV` 对 inliers 使用 `vcompress + vse32`，对 `error_sqr_dists_` 使用 `vcompress + vse32 scratch + 标量 lane double store`。 |
| sibling 形态 | `sac_model_normal_plane.hpp` 的 `selectWithinDistanceRVV` 已使用 `vfwcvt_f_f_v_f64m4` + `vse64` 直接写压缩后的距离。 |
| Phase 050 production direct 基线 | public select median/min/max `3.1014x / 3.0318x / 3.2815x`，Std avg `2.476793`，RVV avg `0.791395`，Evidence Doctor 0/0/0。 |
| 当前问题 | 标量 lane 写回增加 scratch 内存流量和标量后处理；是否值得替换必须由 correctness（正确性）、asm（反汇编归属）和板卡证据决定。 |

## validated_scope / unvalidated_scope

| scope | 内容 |
| --- | --- |
| validated_scope | `SampleConsensusModelLine<PointT>::selectWithinDistance` public entry，在 RVV 构建、`PointXYZ` 风格 float xyz AoS、direct indexed `indices_`、signed 32-bit index 和 u32 offset 安全规模下，验证压缩距离写回从 scratch+标量 lane 改成 `vfwcvt + vse64` 后仍保持语义并有收益。 |
| unvalidated_scope | 非 `PointXYZ` 风格点型、其它 `Scalar`、非 AoS float xyz layout、identity-index stride load、source-indexed / dual-indexed / correspondence row source、非 RVV 构建性能。 |
| point_type_expansion_queue | 保持 Phase 050 Handoff 中的 `point-type-expansion` separate scope；本阶段不扩大模板泛型结论。 |
| phase_closeout_boundary | 只关闭 `select-vcompress-vse64-error-store` 这一 production detail 条目；不能关闭 identity-index 或 point-type expansion。 |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| `select-vcompress-scratch-scalar-store` | 已有 Phase 050 正向生产基线，保守、语义清晰。 | 仍有 scratch store/load 和标量 lane loop，形态落后于 normal-plane。 |
| `select-vcompress-vse64-error-store` | 对压缩后的平方距离直接做 RVV 拓宽和 64 位写回，可减少 scratch 和标量后处理，可能改善 select public RVV 耗时。 | `vfloat64m4_t` 会增加寄存器压力；select 主成本可能仍是 gather / compress，收益可能很小或中性。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `select-vcompress-scratch-scalar-store` | direct indexed `indices_` | `PointXYZ`, float xyz AoS, threshold double | Phase 050 production public `selectWithinDistance` | `run_test_compare` pass | `collect_repeated_board_production_vse64_evidence` | public select median `3.1014x` vs Std | `selectWithinDistanceRVV` has `vcompress`, `vse32`, and scalar lane store in source | 0/0/0 | adopted baseline before Phase 060 | compare against new writeback shape |
| `select-vcompress-vse64-error-store` | direct indexed `indices_` | same as above | Phase 060 production public `selectWithinDistance` | `run_test_compare` | new Phase 060 production repeated evidence | pending | `selectWithinDistanceRVV` must contain `vcompress`, `vfwcvt`, `vse64`, and still keep inlier `vse32` | pending | planned | implement and verify |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| asm gate update | 更新 `test-rvv/sample_consensus/sac_model_line/script/check_line_production_asm.py`，要求 `selectWithinDistanceRVV` 出现 `vfwcvt` 和 `vse64`，同时保留 inlier index 的 `vse32` 与 `vcompress` 检查。 | 新实现的反汇编归属可自动验收；旧 `getDistancesToModelRVV` 的 Phase 050 gate 保持不变。 |
| production patch | 修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_line.hpp`。 | 删除 `compressed_sqr_distances` 临时数组和标量 lane loop；保留 `vcompress` 保序、inlier 写回和最终 resize 语义。 |
| correctness | `make -C test-rvv/sample_consensus/sac_model_line run_test_compare`。 | Std/RVV gtest 全部通过，尤其 `SelectCandidateMatchesPublicEntry...` 系列保持 public entry 对拍。 |
| asm | `make -C test-rvv/sample_consensus/sac_model_line check_production_asm`。 | 三个 production helper 归属通过；select helper 命中压缩、拓宽和 64 位写回。 |
| board repeated | `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_line collect_repeated_board_production_select_vse64_evidence`。 | 5-run 完成，public select 仍 positive；同时记录 count/getDistances 是否受同一 binary 影响。 |
| Evidence Doctor / registry | 新增 Phase 060 manifest / doctor / registry target。 | Errors / Warnings / Suggestions 已解释，freshness check 通过。 |

## Evidence Doctor 和 registry 规则

Phase 060 使用新的 summary evidence 路径：

- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-manifest.json`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.md`
- `test-rvv/sample_consensus/sac_model_line/doc/phases/060-line-select-vse64-compressed-store/production-repeated-evidence-doctor.json`

`log/evidence_registry.json` 只作为 local registry（本地证据登记表）使用，不默认提交。
若 Phase 060 改变 select 的 direction（方向）、decision bucket（决策桶）或 Evidence Doctor
数量，必须同步 evaluation、optimization evidence、roadmap、matrix、`doc-rvv` 和 Handoff。

## 板卡复跑预算和决策桶

- 默认运行 5-run repeated board（板卡重复测试），warm-up 和 iterations 继承 topic bench 当前合同。
- `positive-stable`：public select 的 5-run Std/RVV 全部大于 1，且 RVV avg 不低于 Phase 050 baseline 的同类水平。
- `weak-positive`：public select 仍大于 1，但相对 Phase 050 baseline 收益很小或波动接近噪声；可按可维护性和 normal-plane 形态一致性保留候选，但必须写清。
- `neutral / negative`：public select 相对 Phase 050 baseline 明显退化，或 Evidence Doctor 报告未解释 Error；回滚本阶段 production patch，把候选写成 attempted/rejected。
- 不无限复跑；若 5-run 预算内桶摇摆，标成 `unstable` 并暂停给 reviewer / 用户判断。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-detail（生产细节实现族比较）和 production-public（公开入口 Std/RVV 对比）。 |
| A/B boundary | A 是 Phase 050 已采纳 `select-vcompress-scratch-scalar-store`；B 是 Phase 060 `select-vcompress-vse64-error-store`。两者都在 `selectWithinDistance` production helper 边界内。 |
| 当前决策问题 | 新压缩距离写回形态是否比当前已采纳形态更值得保留。 |
| diagnostic 是否可外推到 production | 不使用 diagnostic 外推；本阶段直接修改 production helper 并重跑 public entry。 |
| comparison-boundary / baseline mismatch 风险 | Phase 050 和 Phase 060 来自不同 binary / run batch，因此 RVV-vs-RVV 数值只能作为同 production boundary 的 detail A/B 参考；若 bucket 接近或摇摆，需要降级为 neutral / weak-positive。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段 probe 已有界；若退化或不稳定，回滚到 Phase 050 baseline。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 需要；本阶段用 Phase 050 vs Phase 060 public select RVV avg / median 做 detail A/B，并保留不同 run batch 的局限。 |

## 继续 / 停止条件

若新写回形态 correctness、asm、board repeated 和 Evidence Doctor 均闭合，且 public select 仍为
positive-stable 或 weak-positive，则同步文档并按证据决定是否保留。若新形态退化、asm 不闭合、
correctness 失败或 Evidence Doctor Error 无法解释，则回滚本阶段 production patch 并把 Phase 060
写成 attempted/rejected。

本阶段完成后仍不自动推进 `identity-index-strided-load` 或 `point-type-expansion`；前者改变
RVV family（RVV 实现族）并需要 strict RVV-vs-RVV A/B（严格同边界 RVV 对 RVV 比较），后者扩大点型 /
layout 范围。
