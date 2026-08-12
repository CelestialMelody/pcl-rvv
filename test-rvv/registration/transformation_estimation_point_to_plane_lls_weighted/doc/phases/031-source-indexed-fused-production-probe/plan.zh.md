# Phase 031 Plan：source-indexed fused production probe

## 阶段意图和边界

本阶段按用户在 2026-08-12 的明确授权，跳过 Phase 030 的默认 no-production-candidate 建议，做一次受控 production probe（生产探针）：把真实 source-indexed public overload 的 RVV 分流临时接到 `block-fused-abcd-ilp` implementation family，再用 production direct correctness 和 production-shaped board bench 验证 Phase 030 的 diagnostic negative 是否会在真实生产接入后复现。

本阶段证明：

- 真实 `estimateRigidTransformation(cloud_src, indices_src, cloud_tgt, matrix)` 入口能命中 source-indexed `block-fused-abcd-ilp` 探针路径，且 correctness / fallback 仍在预算内。
- `production-source-indices` repeated board summary 在接入后的代码上反映真实 public dispatch，而不是 test-rvv family wrapper。
- 若 production bench 仍负向，Phase 030 的结论得到生产侧确认；若 production bench 正向或明显不同，Phase 030 的 diagnostic harness 需要降级为可能误导，并进入后续证据校准。

本阶段不证明：

- `block-fused-abcd-ilp` 已正式 adopted。除非 PI5 证据完整且文档 closeout 改写，当前补丁仍是 probe。
- dual-indices、correspondences、`Scalar=double`、非连续权重、非法 public index 合同或其它 row source 可以接入 RVV。
- QEMU timing 有性能含义；QEMU 只用于 correctness 和路径命中。

## 当前状态清单

| 项 | 当前状态 | 本阶段动作 |
| --- | --- | --- |
| committed baseline | 已有 3 个分类提交：production/source-indexed staged-gather、board evidence summaries、Phase 000-030 docs。 | 在干净工作树上开始 Phase 031。 |
| source-indexed production | 当前 adopted helper 是 staged-gather / compressed-tail，production-source-indices repeated summary 三类代表点型正向。 | 改为先走 `block-fused-abcd-ilp` 探针；保留 staged helper 作为 fallback / 可回滚入口。 |
| Phase 030 diagnostic | `source_indexed_family_repeated` 中 `block-fused-abcd-ilp` full estimate median `0.90x`，doctor `3E / 6W / 13S`。 | 用 production bench 验证该负向是否在真实 dispatch 中复现。 |
| evidence wrapper | `generate_teptplw_evidence_manifest.py --kind production-source-indices` 已能解析 `production-source-indices` summary。 | 复用 wrapper；输出到新的 probe evidence dir。 |

## Optimization Matrix

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| staged-gather / compressed-tail | source-indexed | representative point types / `float` / source indices + contiguous weights | production public source-indexed overload | `run_test_source_indices_compare` | `collect_board_production_source_indices_repeated` | `production_source_indices_staged_gather/summary.md` | source-indexed-specific asm missing | `0E / 7W / 6S` | adopted baseline before probe | retained as fallback / rollback baseline |
| block-fused-abcd-ilp | source-indexed | representative point types / `float` / same layout gate | production public source-indexed overload probe | `run_test_source_indices_compare`、`run_test_production_direct_compare` | `collect_board_production_source_indices_probe_repeated` 或等价 `production-source-indices` collect with output dir | pending | pending / optional | pending | planned probe | implement, test, collect board, doctor |
| block-fused-abcd-ilp | dual-indices / correspondences | `PointNormal` / `float` / index pair streams | test-rvv diagnostic only | Phase 020 tests | Phase 020 bench | diagnostic negative | missing | diagnostic doctor has Errors | no-production | none |

## 实现和测试动作

| 动作 | 产物 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| P1 写 production probe helper | `registration/include/.../transformation_estimation_point_to_plane_lls_weighted.hpp` | 新增 source-indexed block-fused helper；public source-indexed RVV 入口先走 probe helper，失败后 fallback staged / scalar。 | `git diff --check` 干净；helper 不扩大 public API。 |
| P2 更新 production direct 测试 | `src/test_teptplw_production_direct.cpp` 如需补 stats / probe path 断言 | QEMU std/RVV correctness。 | `run_test_source_indices_compare` 和 `run_test_production_direct_compare` 通过。 |
| P3 production bench target / manifest | `Makefile` 如需增加 probe output dir 和 doctor target | 能把 summary 写到 `log/board/production_source_indices_block_fused_abcd_ilp_probe/`。 | dry-run 或实际 collect 命令可复现。 |
| P4 board repeated probe | `collect_board_production_source_indices_probe_repeated` 或等价命令 | size 默认 `65536,262144`、runs `5`、iterations `20`、warmup `5`。 | summary 存在；doctor 生成 markdown/json。 |
| P5 result / docs / handoff | Phase 031 result、phase README、topic docs、current handoff | 写清 production bench 是否确认 / 推翻 Phase 030 diagnostic。 | EvidenceDecision 不超过 production probe 证据边界。 |

## Evidence Doctor 和 Registry 规则

本阶段 production probe 证据目录：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls_weighted/log/board/production_source_indices_block_fused_abcd_ilp_probe/
```

目标产物：

- `summary.md`
- `evidence_manifest.json`
- `evidence_doctor.md`
- `evidence_doctor.json`

若复用 `--kind production-source-indices` wrapper，manifest 的 source path / output dir 必须显式指向 probe summary，避免覆盖已提交的 staged-gather adopted evidence。Evidence Doctor Error 不自动判实现 bug，但 production adoption 前必须解释、重跑、降级或回滚。

## 板卡复跑预算和决策桶

| 字段 | 默认值 |
| --- | --- |
| size | `65536,262144` |
| runs | `5` |
| iterations | `20` |
| warm-up iterations | `5` |
| output | `log/board/production_source_indices_block_fused_abcd_ilp_probe/summary.md` |

决策桶：

- `positive`：所有代表点型和规模在 production-source-indices public dispatch 下稳定正向，doctor 无阻塞 Error，且 correctness / fallback 通过。
- `weak_positive`：median 正向但有 warning，需要 source-indexed-specific asm 或扩大 runs 后再考虑 adoption。
- `negative`：262144 或主要代表点型复现 Phase 030 的退化，默认回滚到 staged-gather adopted path。
- `unstable`：5-run 内跨桶摇摆或长尾过大；降级为 production probe pending，不采纳。

## 继续 / 停止条件

本阶段授权继续到 production probe patch、QEMU correctness、board repeated probe 和 Phase 031 result。若需要扩大到 public API、dual-indices、correspondences、`Scalar=double`、泛型 normal traits 重构或更大 board run-count，必须停止并另写 phase / 用户确认。
