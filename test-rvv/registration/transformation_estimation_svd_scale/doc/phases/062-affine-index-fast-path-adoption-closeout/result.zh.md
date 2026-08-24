# Phase 062 结果：affine index fast-path adoption closeout

## 当前结论

Phase 061 的 `affine-index-fast-path-production-probe` 已从 PI5 用户确认点收口为 `adopted-by-user` production behavior（已由用户确认采纳的生产行为）。

采纳范围保持 Phase 061 的窄边界：

- `__RVV10__` 构建；
- `Scalar=float`；
- dense source / target；
- source 和 target 都满足 traits-gated xyz AoS float layout；
- `nr_points >= 16`；
- source-indexed、dual-indexed 和 correspondence public overload；
- source / target indices 或 query / match correspondences 是 step=1 contiguous slice（连续偏移片段）。

未命中 contiguous gate 时不改变 Phase 043 / Phase 047 行为：source-indexed 和 dual-indexed 回普通 gather RVV path，correspondence 回普通 gather RVV path 或 correspondence sorted-copy branch；再失败时回父类标量路径。

## 用户确认

用户已明确表示“当前有收益的实现可以接入”。因此本阶段把 Phase 061 result、optimization matrix、roadmap、topic-local doc suite 和 production 长期主题文档里的当前状态从 pending / PI5 checkpoint 刷新为 adopted-by-user。

## 执行动作回填

| action | status | result |
| --- | --- | --- |
| A1 Phase 061 result closeout | done | `061-affine-index-fast-path-production-probe/result.zh.md` 已保留 PI5 历史边界，并把当前 decision 改为 adopted-by-user。 |
| A2 topic-local docs refresh | done | README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map 和 evaluation 已同步 Phase 061/062 adopted 状态与 23-test correctness。 |
| A3 matrix / roadmap refresh | done | `optimization-matrix.zh.md` 和 `optimization-roadmap.zh.md` 已把 `affine-index-fast-path-production-probe` 标为 adopted-by-user，并保留剩余扩展方向。 |
| A4 production long-term doc refresh | done | `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md` 已新增第四类 adopted production behavior：contiguous affine index fast path，并补 dispatch / fallback / evidence / 不覆盖范围。 |
| A5 verification | done | `py_compile`、`evidence_status` 和 `git diff --check` 已通过；路径限定 status 在最终回复列出。 |

## 证据链

| evidence layer | path / command | result | role |
| --- | --- | --- | --- |
| correctness | `make -C test-rvv/registration/transformation_estimation_svd_scale run_test_compare_recorded` | Std/RVV 各 23 tests passed。 | 证明 public production path、fallback 和新增 `AffineIndexFastPathPublicProbeMatchesReference` 与 reference 对齐。 |
| QEMU smoke | `make -C test-rvv/registration/transformation_estimation_svd_scale record_qemu_affine_index_fast_path_production_probe_state` | 6 comparisons；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | 只证明 build、public label、日志形状和 QEMU 路径；QEMU timing 不作为性能证据。 |
| board repeated | `make -C test-rvv/registration/transformation_estimation_svd_scale run_board_bench_affine_index_fast_path_production_probe_repeated` | 6/6 case positive；median B/A `10.115x` 到 `14.021x`；Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`。 | 支撑 adopted-by-user 的目标硬件性能证据。 |
| production source | `registration/include/pcl/registration/impl/transformation_estimation_svd_scale.hpp` | 已有 contiguous range 检测与 contiguous offset RVV accumulation。 | 真实生产路径，不是 test-only candidate。 |

## 生产边界

| row source | adopted fast path | fallback |
| --- | --- | --- |
| source-indexed | source indices 是 step=1 contiguous slice；source offset 来自 indices 首项，target 使用 selected target 的 0 起点顺序。 | 非 contiguous 时走 Phase 043 source-indexed gather RVV path；再失败回父类。 |
| dual-indexed | source / target indices 都是 step=1 contiguous slice 且 count 相同。 | 任一侧非 contiguous 时走 Phase 043 dual-indexed gather RVV path；再失败回父类。 |
| correspondence | query 和 match 都是 step=1 contiguous slice。 | 非 contiguous 时走普通 correspondence gather RVV path；必要时再尝试 Phase 047 sorted-copy branch；再失败回父类。 |

## 文档同步

本阶段刷新了以下当前状态文档：

- `doc-rvv/registration/transformation_estimation_svd_scale-RVV.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/README.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/testing-overview.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/correctness-tests.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/benchmark-and-evidence.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/optimization-evidence.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/optimization-roadmap.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/test-support-code-map.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/transformation_estimation_svd_scale-evaluation.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/phases/README.zh.md`
- `test-rvv/registration/transformation_estimation_svd_scale/doc/phases/optimization-matrix.zh.md`

## 仍不覆盖

以下方向没有被 Phase 061/062 关闭，继续推进时需要独立 phase plan、同边界 correctness、QEMU、ASM、board repeated 和 Evidence Doctor：

- `Scalar=double`：当前 fast path 是 f32 accumulation，需要独立数值预算和性能判断。
- stride / reverse / shuffle affine fast path：当前 adopted branch 只覆盖 step=1 contiguous slice。
- 非法 index / correspondence：仍由现有 public fallback / 父类语义负责。
- 全部 custom layout / alignment / padding 组合：已有 Phase 055-059 采样，不等于全集证明。
- 新的 row-source mitigation family：Phase 048 / 049 / 058 已关闭 staged-selected-cloud、target-sorted 和 dual-indexed source-sorted-copy 残留线索；若继续，需要新候选和新边界。

## EvidenceDecision

`adopted-by-user / affine-index-fast-path-production-probe`。

理由：真实 public production probe 已通过 correctness、QEMU smoke、board repeated 和 Evidence Doctor；用户已确认当前有收益实现可以接入；本阶段已同步长期 production 文档和 topic-local evidence 文档。该 decision 只覆盖上文列出的 contiguous row-source 边界，不外推到其它 index 分布、`Scalar=double` 或未采样布局全集。

## 下一阶段恢复

默认不把本阶段继续扩展成更宽 topic closeout。若用户继续推进优化矩阵，建议从 roadmap 中选择独立 candidate：

1. `Scalar=double` 数值与性能预算；
2. 更广 custom layout / alignment sampling；
3. 新 row-source mitigation family，尤其是 Phase 045 暴露的 shuffle locality sensitivity 后续路线。
