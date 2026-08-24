# Phase 060 Result: normal-field diagnostic

## 实际执行范围

本阶段只评估 `PointCloudImageExtractorFromNormalField<PointT>::extractImpl` 的
test-only diagnostic（测试专用诊断）候选 `normal_float_stride_v0`。production header
没有修改；`io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 仍保持标量实现。

候选只覆盖 organized cloud order（有组织点云顺序）下的 exact `PointNormal`，读取
`normal_x`、`normal_y`、`normal_z` 三个 float 字段，执行 `(value + 1.0) * 127`
后写入 `rgb8` 字节。它不覆盖泛型 `PointT` normal traits、真实 `PCLImage` metadata
写入、公开入口 dispatch（分流逻辑）或 NaN post-pass。

## TDD 回填

| step | status | 证据 |
| --- | --- | --- |
| RED | done | 新增 `NormalFieldCandidateMatchesScalarReference` 后，`make run_test_rvv` 编译失败于缺少 `makeNormalCloud`、`extractNormalScalar` 和 `extractNormalCandidate`。 |
| GREEN | done | 实现 test-only fixture、标量参考和 RVV candidate 后，`make run_test_compare` 通过，Std/RVV 各 7 个 gtest pass。 |
| mutation check | done | 若字段顺序、tail 或 float 到 `uint8_t` 截断语义错误，新测试会与真实 normal extractor 输出不一致。 |

## 执行命令和证据路径

| 证据层 | 命令 | 路径 / 结果 |
| --- | --- | --- |
| correctness | `make run_test_compare` | `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log`; Std/RVV 各 7 tests pass |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter normal_field_pointnormal_640x480'` | label 可运行并输出 checksum；QEMU timing 不作性能结论 |
| asm | `make dump_bench_rvv` 后检查 `vlse32.v`、`vfadd.vf`、`vfmul.vf` | normal candidate 指令存在于 diagnostic bench binary |
| board repeated | `make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_phase060 PCIE_REPEATED_RUNS=5 BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter normal_field_pointnormal_640x480'` | `log/board/repeated_phase060/summary.md` |
| Evidence Doctor | `make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase060` | `log/board/repeated_phase060/evidence_doctor.md`; `Errors=1, Warnings=1, Suggestions=0` |

## 板卡结果

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `normal_field_pointnormal_640x480` | 5 | 0.61x | 0.46x | 0.67x | negative |

Std/RVV checksum 均为 `14002970499957503227`。5-run 全部低于 1.0，decision bucket
稳定为 negative（负向）；复跑预算已经按计划用完，不继续无限复跑。

## Evidence Doctor 解释

Evidence Doctor 报告 `Errors=1, Warnings=1, Suggestions=0`：

- Error `ba_degradation_frequency` 指向 `normal_field_pointnormal_640x480`，5/5 低于 1.0。
  处理动作是把 `normal_float_stride_v0` 降级为当前诊断边界下不支持继续接入的候选。
- Warning `long_tail_or_variance` 指出 min 0.46x、median 0.61x、max 0.67x。该长尾没有改变
  decision bucket；它只要求保留 min/median/max，并说明可能有板卡频率、缓存或调度因素。

该 Error 不代表 correctness bug，因为 checksum 一致；它表示当前 test-helper A/B 边界不能支撑
normal v0 的 production performance（生产性能）结论。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`；Std build 用真实 normal extractor 生成参考，RVV build 用测试专用 candidate 直接写 `std::vector<uint8_t>`。 |
| 当前决策问题 | `RVV-vs-scalar` 的诊断筛选。 |
| diagnostic 是否可外推到 production | 不能直接外推。它只说明当前 `PointNormal` / organized / test-helper 形态下，三路 float stride load 加 scratch 写回不值得纳入当前生产探针范围。 |
| comparison-boundary / baseline mismatch 风险 | 存在。真实 production 还包含 extractor 对象、`PCLImage` metadata、公开入口 fallback 和 base `extract` post-pass。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 规则上不能用本负向 diagnostic 直接拒绝 bounded production probe；但当前 Phase 040/050 PI2 scope 已冻结为 RGB segment-store 与 full-range scaling reduction，normal v0 不纳入当前 PI2。若用户后续单独授权 normal production probe，需要重新做 PI1 范围冻结。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。当前没有 production patch，也没有 production direct 证据。 |

## Matrix 更新

- `normal_xyz_to_rgb` / `normal_float_stride_v0`：`deferred -> attempted / rejected within diagnostic boundary`。
- 当前 PI2 production probe 范围不变：仍只包含 `rgb_segment_store_v1` 和 `scaling_reduction_v1`，
  且 PI2 生产补丁需要用户明确确认。
- `label_mono16` 仍是 topic-local、未阻塞的下一个诊断候选。

## Evidence Freshness 和 Registry

`log/board/repeated_phase060/summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`
由本阶段命令生成。此前曾用环境变量前缀方式误跑 `collect_board_repeated`，因为 Makefile
变量覆盖顺序写入了默认 `log/board/repeated_phase000`；该批输出只作为历史卫生问题记录，不作为
Phase 060 证据。当前结论只依赖命令行变量方式生成的 `repeated_phase060`。

topic-local registry 不是本阶段 gate；提交前仍需要路径限定 status scan 和 `git diff --check`。

## Continue / Stop Decision

`continue_stop_decision=continue`。

本阶段已经关闭 normal v0 诊断条目，但没有命中可停止条件。production patch 仍需要用户确认；
在当前 topic-local 授权范围内，`label_mono16` 仍是未阻塞候选。

`next_phase_default=070-label-mono16-diagnostic`。
