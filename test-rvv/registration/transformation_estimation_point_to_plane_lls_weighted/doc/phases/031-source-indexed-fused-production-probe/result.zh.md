# Phase 031 Result：source-indexed fused production probe

## 阶段摘要

本阶段按用户明确授权，把真实 source-indexed public overload 的 RVV 分流接到
`block-fused-abcd-ilp` production probe（生产探针）。这次结果没有复现 Phase 030 的
pre-production diagnostic（接入生产前诊断）负向：在真实 `production-source-indices`
公开入口上，6 个代表 case 的 5-run median 都保持正向。

当前结论是：

```text
source-indexed block-fused-abcd-ilp production probe is positive with warnings
```

这不是“全范围正式 adopted”。它支持把 probe 补丁作为 bounded production candidate
（有边界的生产候选）继续保留评审，但还不能写成 clean production pass。主要未闭合项是
source-indexed-specific asm attribution（源索引生产路径专属反汇编归因）、binary identity（二进制身份）、
taskset metadata（绑核元数据）和 262144 规模下的长尾解释。

## 计划 vs 实际

| 动作 | 状态 | 证据 / 产物 | 结论 |
| --- | --- | --- | --- |
| P1 写 production probe helper | done | `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` | 新增 source-indexed block-fused helper；public source-indexed RVV 入口先尝试 block-fused，失败后回 staged helper / scalar。 |
| P2 更新 production direct 测试 | done | `src/test_teptplw_production_direct.cpp` | 新增非法 source index 对 block-fused helper 的 pre-gather gate 断言；不扩大 public API 合同。 |
| P3 production bench target / manifest | done | `Makefile`、`script/collect_teptplw_board_compare_repeated.py`、`script/generate_teptplw_evidence_manifest.py` | 新 target 生成独立 probe summary；manifest kind 为 `production-source-indices-block-fused-probe`，不会混入 staged-gather adopted summary。 |
| P4 board repeated probe | done | `log/board/production_source_indices_block_fused_abcd_ilp_probe/summary.md` | Milkv-Jupiter 上完成 5 runs、20 iterations、5 warm-up，size 为 `65536,262144`。 |
| P5 Evidence Doctor | done | `log/board/production_source_indices_block_fused_abcd_ilp_probe/evidence_manifest.json`、`evidence_doctor.md`、`evidence_doctor.json` | Doctor 结果为 `0 Errors / 9 Warnings / 12 Suggestions`；无阻塞 Error，但 asm、binary identity 和长尾需要写入风险边界。 |
| P6 文档和交接 | done | 本 result、phase README、topic docs、evaluation、current handoff | Phase 030 负向被降级为“诊断 harness 对 production direct 不可靠的证据”，Phase 031 作为当前生产探针事实。 |

## Optimization Matrix 更新

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| block-reduction / A/B/C/N / fused-abcd-ilp | full-cloud | representative point types / `float` / contiguous weights | production full-cloud public overload | `run_test_production_direct_compare`、`run_test_candidates_compare` | `collect_board_production_dispatch_repeated` | adopted repeated board | production-default asm exists | `0E / 0W / 3S` | adopted | none |
| staged-gather / compressed-tail | source-indexed | representative point types / `float` / source indices + contiguous weights | prior production source-indexed public overload | `run_test_source_indices_compare` | `collect_board_production_source_indices_repeated` | adopted historical production summary | source-indexed-specific asm missing | `0E / 7W / 6S` | superseded by probe candidate, retained as rollback helper | none unless probe review rejects |
| block-fused-abcd-ilp | source-indexed | representative point types / `float` / valid source indices | production source-indexed public overload probe | `run_test_source_indices_compare`、`run_test_production_direct_compare` | `collect_board_production_source_indices_probe_repeated` | `production_source_indices_block_fused_abcd_ilp_probe/summary.md`：6/6 median 正向 | missing / warning | `0E / 9W / 12S` | bounded production candidate, not clean adopted | source-indexed asm / binary identity / optional extended run |
| block-fused-abcd-ilp | source-indexed | `PointNormal` / `float` / test-rvv family diagnostic | pre-production diagnostic wrapper | `run_test_source_indexed_family_compare` | `collect_board_source_indexed_family_repeated` | Phase 030 median `0.90x`，`4/5` below `1.0x` | missing | `3E / 6W / 13S` | historical diagnostic negative after Phase 031 | use only as harness-risk signal |
| staged/block/fused family | dual-indices / correspondences | `PointNormal` / `float` / index pair streams | test-rvv diagnostic only | Phase 020 tests | Phase 020 bench | diagnostic negative | missing | diagnostic doctor has Errors | no-production | none |

## Correctness 与 Fallback

本阶段重新运行了受影响的 QEMU correctness（QEMU 正确性）目标：

| 命令 | 结果 | 证据 |
| --- | --- | --- |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_source_indices_compare` | std `6 passed`；RVV `7 passed`。 | `log/qemu/run_test_source_indices_std.log`、`log/qemu/run_test_source_indices_rvv.log`。 |
| `make -C test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted run_test_production_direct_compare` | std `18 passed`；RVV `19 passed`。 | `log/qemu/run_test_production_direct_std.log`、`log/qemu/run_test_production_direct_rvv.log`。 |

新增测试只证明 RVV helper 在 gather 前拒绝非法 source index。它不把非法 index 的行为提升为
public API 合同；public overload 失败后仍回到原标量 iterator 边界。

Fallback 矩阵：

| 条件 | 当前行为 |
| --- | --- |
| 非 RVV 构建 | 不编译 / 不尝试 RVV helper，走标量。 |
| `Scalar != float` | `estimatePointToPlaneLLSWeightedSourceIndicesRVV` 返回 false，走标量。 |
| 小规模、VLEN miss、source byte-offset miss | `canUsePointToPlaneLLSWeightedSourceIndicesRVV` 返回 false，走标量。 |
| source / target layout miss | `RVVXYZAoSFloatLayout` 或 `RVVXYZNormalFloatLayout` false，走标量。 |
| source index 负数或越界 | block-fused helper 和 staged helper 都返回 false，公开入口回标量。 |
| dual-indices / correspondences | 未接本 helper，继续保持标量 production 边界。 |

## Board Production Probe

本阶段 board evidence（板卡证据）目录：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_block_fused_abcd_ilp_probe/
```

采集参数：

| 参数 | 值 |
| --- | --- |
| case filter | `production-source-indices` |
| size | `65536,262144` |
| runs | `5` |
| iterations | `20` |
| warm-up iterations | `5` |
| collection profile | `source-indexed-block-fused-abcd-ilp-production-probe` |

结果：

| case | median | min | max | values |
| --- | ---: | ---: | ---: | --- |
| `pointnormal 262144` | `1.64x` | `1.17x` | `1.67x` | `1.64x, 1.65x, 1.67x, 1.37x, 1.17x` |
| `pointnormal 65536` | `1.71x` | `1.66x` | `1.74x` | `1.66x, 1.74x, 1.74x, 1.71x, 1.70x` |
| `pointxyz-to-pointnormal 262144` | `1.54x` | `1.06x` | `1.71x` | `1.65x, 1.06x, 1.71x, 1.54x, 1.29x` |
| `pointxyz-to-pointnormal 65536` | `1.64x` | `1.59x` | `1.68x` | `1.62x, 1.64x, 1.68x, 1.64x, 1.59x` |
| `pointxyz-to-pointxyzinormal 262144` | `1.69x` | `1.41x` | `1.79x` | `1.49x, 1.77x, 1.79x, 1.69x, 1.41x` |
| `pointxyz-to-pointxyzinormal 65536` | `1.69x` | `1.68x` | `1.75x` | `1.69x, 1.69x, 1.75x, 1.70x, 1.68x` |

这些数据来自真实 public source-indexed overload，不是 `source-indexed-family` test-rvv wrapper。
因此 Phase 031 推翻了“Phase 030 diagnostic negative 可以直接阻止生产探针”的假设。当前解释是：
Phase 030 的 family diagnostic wrapper、sink、计时边界或代码组织不能代表 production direct 的最终表现。

## Evidence Doctor / Manifest

新 probe evidence 已生成：

- `log/board/production_source_indices_block_fused_abcd_ilp_probe/evidence_manifest.json`
- `log/board/production_source_indices_block_fused_abcd_ilp_probe/evidence_doctor.md`
- `log/board/production_source_indices_block_fused_abcd_ilp_probe/evidence_doctor.json`

Doctor 结果：

```text
Errors=0, Warnings=9, Suggestions=12
```

处理结论：

| finding | 处理动作 | 对结论影响 |
| --- | --- | --- |
| `asm_boundary_missing`，6 个 case 都触发 | 保留为未闭合项；需要 source-indexed block-fused production helper 的 objdump / attribution。 | 不阻塞“production probe 正向”，但阻止写成 clean adopted。 |
| `long_tail_or_variance`，3 个 262144 case 触发 | summary 保留 min/median/max 和全部 values；不剔除异常值。 | 允许写 `positive_with_warnings`；若要正式 adoption，可扩大到 20-run / 50-run。 |
| `environment_metadata_missing` / `binary_identity_missing` suggestions | 当前 summary-only 策略不提交 raw run / collection manifest；下一轮可补 taskset、binary hash。 | 不单独阻塞，但方向反转或长尾排查时优先补齐。 |

## Evidence Freshness

Phase 030 的 `source_indexed_family_repeated` summary 和 result 保留为 historical diagnostic（历史诊断）：

- Phase 030 证明的是 test-rvv implementation-family wrapper 在 `PointNormal / float / 262144` 下的负向。
- Phase 031 证明的是真实 production source-indexed public overload 在接入 block-fused probe 后，代表点型和两个规模上均正向。

因此旧 Phase 030 结论不能再写成“阻止任何 production probe”。它现在的角色是提醒 reviewer：
pre-production family harness 对真实 production dispatch 的预测可能失真，后续需要 source-indexed-specific asm
和更细的同边界消融来解释原因。

## Evidence Registry 状态

本 topic 仍没有统一 `log/evidence_registry.json` 登记表。本阶段按人工 freshness 检查处理：

| 路径 | 状态 |
| --- | --- |
| `production_source_indices_block_fused_abcd_ilp_probe/summary.md` | 新增 summary-only 证据；已被 Phase 031 result、evaluation 和 topic docs 引用。 |
| `production_source_indices_block_fused_abcd_ilp_probe/evidence_manifest.json` | 新增 manifest；已区分 probe kind。 |
| `production_source_indices_block_fused_abcd_ilp_probe/evidence_doctor.md` / `.json` | 新增 Doctor 输出；已记录 `0E / 9W / 12S`。 |
| raw compare logs | 默认临时清理；不进入提交边界。 |

## Current Decision

当前 EvidenceDecision：

```text
bounded-production-candidate/source-indexed-block-fused-abcd-ilp-f32-aos-valid-index-positive-with-warnings
```

保留范围：

- source-indexed public overload。
- `Scalar=float`。
- 连续 `weights_`。
- valid source index stream。
- source xyz f32 AoS layout、target xyz+normal f32 AoS layout。
- `65536,262144` 两个规模上的三类代表点型 board summary。

不扩大范围：

- dual-indices。
- correspondences。
- `Scalar=double`。
- 非连续权重。
- 未验收 source / target layout。
- 非法 index 的 public API 语义。

## Continue / Stop Decision

当前 Phase 031 已闭合到 production probe + repeated board + Doctor + 文档 closeout。默认停止在评审边界，
不自动扩大到 dual-indices、correspondences、`Scalar=double` 或更大 run-count。

默认下一步是 reviewer 审查是否接受 bounded production candidate。若要把 probe 升级为 clean adopted，
下一 phase 应先补：

- source-indexed block-fused helper 的 asm attribution。
- 记录 binary hash / taskset 的 repeated board manifest。
- 可选 20-run / 50-run source-indexed production probe，保留 `--raw-dir` 供长尾排查。
- 若需要解释 Phase 030/031 分歧，再做同边界 source-indexed family vs production direct 消融。
