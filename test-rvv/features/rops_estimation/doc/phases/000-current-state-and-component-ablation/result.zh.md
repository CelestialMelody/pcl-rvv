# Phase 000 Result: current-state-and-component-ablation

## 当前结论

Phase 000 已建立 ROPS central moments（中心矩）component 的 test-rvv scaffold（测试资产脚手架）。`computeCentralMomentsRVV()` 与 production 私有 `computeCentralMoments()` oracle（参考真值）在 QEMU Std/RVV 两种构建下通过 3 个 correctness case；反汇编能把 RVV 指令归属到 test-only helper。

本阶段不进入 production（生产源码）。原因是 central moments 默认只处理 5x5 小矩阵，当前 RVV helper 需要 row/col/mass staging（暂存），entropy（熵）仍调用标量 `logf`。这个 evidence role（证据角色）只足以说明“组件语义可复刻”，不能证明完整 `ROPSEstimation::computeFeature()` 受益。

## 计划动作回填

| action | status | evidence | notes |
| --- | --- | --- | --- |
| A1 RED test | done | `make -C test-rvv/features/rops_estimation run_test_rvv` first failed with missing `rops::computeCentralMomentsRVV` | failure 是预期缺少 candidate symbol |
| A2 Std/RVV helper | done | `include/impl/rops_components.hpp` | 非 RVV 构建走 `computeCentralMomentsStd()`；RVV 构建用 VLA 分块规约 mean 和 4 个 moments |
| A3 correctness cases | done | `src/test_rops_estimation.cpp` | 覆盖 dense 5x5、zero 5x5、non-default 7x7 |
| A4 asm attribution | done | `build/asm/riscv/test_rops_estimation_rvv.full.asm` symbol `computeCentralMomentsRVV` contains `vsetvli` / `vle32.v` / `vfmul.vv` / `vfredusum.vs` | asm 文件是本地生成证据，默认不提交 raw / full asm |
| A5 bench decision | done | 本 result、roadmap、matrix | 不为 central moments standalone 建 board bench；继续 distribution matrix |

## 验证结果

| command | result | evidence path | role |
| --- | --- | --- | --- |
| `make -C test-rvv/features/rops_estimation run_test_compare` | pass：Std 3/3，RVV 3/3 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness（正确性）和日志形状 |
| `make -C test-rvv/features/rops_estimation dump_test_rvv` | pass：生成 filtered/full asm | `build/asm/riscv/test_rops_estimation_rvv.asm`、`build/asm/riscv/test_rops_estimation_rvv.full.asm` | asm attribution（反汇编归属） |

## Evidence Doctor 和 registry

Evidence Doctor（证据体检）未运行，原因是 Phase 000 未生成 bench summary、board summary、checksum summary 或 EvidenceDecision 性能结论。`log/evidence_registry.json` 尚未创建；当前只有 QEMU correctness log 和 asm dump，均为本地生成证据，默认不提交。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic correctness；test helper component boundary |
| A/B boundary | production private helper oracle vs test-only candidate |
| 当前决策问题 | implementation-shape；是否把 central moments 作为后续 component 候选保留 |
| diagnostic 是否可外推到 production | no。完整 production 还包含 mesh local surface、LRF、rotateCloud、distribution matrix scatter、normalization 和 public `compute()` |
| comparison-boundary / baseline mismatch 风险 | yes。当前 helper-only correctness 没有计入完整 descriptor 调用频率、allocation、Eigen 和 KdTree 成本 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for central moments alone |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段没有 production patch |

## Phase scope 与未验证范围

`validated_scope`：`pcl::PointXYZ` / `pcl::Histogram<135>` production oracle 实例，`Eigen::MatrixXf` 5x5 与 7x7 central moments component，float moments，QEMU Std/RVV correctness。

`unvalidated_scope`：完整 ROPS descriptor、distribution matrix、rotate/projection、LRF、feature normalization、其它点型、真实 production dispatch、board performance。

`point_type_expansion_queue`：本阶段不适用；若后续进入 production probe，再为 `PointXYZI`、`PointXYZRGBA`、`PointNormal` 和 PointXYZ-like traits 建独立 phase。

## 阶段反思和下一步

central moments helper 证明了 production oracle 可被拆成 test-only same-chain test，这是后续组件消融的基础。但它不是优先性能候选：默认 5x5 矩阵太小，RVV staging 和标量 entropy 会稀释收益。下一阶段默认进入 `010-distribution-matrix-ablation`，验证 rotated local cloud 到 bin matrix 的 index 计算是否有更大工作量和更合理的 RVV 边界。

## Artifact tracking

本阶段新增的 topic-local 文档和源码位于 `test-rvv/features/rops_estimation/**`。`build/` 和 `log/` 为生成证据，默认不提交。
