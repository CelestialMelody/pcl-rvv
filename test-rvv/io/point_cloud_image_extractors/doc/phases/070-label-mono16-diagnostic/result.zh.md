# Phase 070 Result: label-mono16 diagnostic

## 实际执行范围

本阶段新增 `label_mono16_stride_v0` test-only diagnostic（测试专用诊断）候选。production
源码没有修改；`io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 仍保持标量实现。

候选只覆盖 exact `PointXYZL`、organized cloud order（有组织点云顺序）和 `COLORS_MONO`，
读取 `label` 的 `std::uint32_t` 字段，按 production 语义截断为 `unsigned short` 后写入
`mono16` 输出。`COLORS_RGB_RANDOM`、`COLORS_RGB_GLASBEY`、泛型 label-like 点型、真实
production dispatch（生产分流）和 NaN post-pass 均不在本阶段范围内。

## TDD 回填

| step | status | 证据 |
| --- | --- | --- |
| RED | done | 新增 `LabelMono16CandidateMatchesScalarReference` 后，`make run_test_rvv` 编译失败于缺少 `makeLabelCloud`、`extractLabelMono16Scalar` 和 `extractLabelMono16Candidate`。 |
| GREEN | done | 实现 test-only fixture、标量参考和 RVV candidate 后，`make run_test_compare` 通过，Std/RVV 各 8 个 gtest pass。 |
| mutation check | done | fixture 覆盖大于 65535 的 label；若 candidate 没有复刻低 16 位截断语义，新测试会与真实标量 extractor 输出不一致。 |

## 执行命令和证据路径

| 证据层 | 命令 | 路径 / 结果 |
| --- | --- | --- |
| correctness | `make run_test_compare` | `log/qemu/run_test_std.log`, `log/qemu/run_test_rvv.log`; Std/RVV 各 8 tests pass |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter label_mono16_pointxyzl_640x480'` | label 可运行，checksum 为 `7973167110771994097`；QEMU timing 不作性能结论 |
| asm | `make dump_bench_rvv` 后检查 `extractLabelMono16Rvv` 符号 | `build/asm/riscv/bench_pcie_rvv.full.asm` 中 label 符号存在；filtered asm 可见 `vlse32.v`、`vse16.v`，full asm 中可见 `vnsrl.wi` |
| board repeated | `make collect_board_repeated PCIE_REPEATED_DIR=log/board/repeated_phase070 PCIE_REPEATED_RUNS=5 BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter label_mono16_pointxyzl_640x480'` | `log/board/repeated_phase070/summary.md` |
| Evidence Doctor | `make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=log/board/repeated_phase070` | `log/board/repeated_phase070/evidence_doctor.md`; `Errors=0, Warnings=0, Suggestions=0` |

## 板卡结果

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `label_mono16_pointxyzl_640x480` | 5 | 1.21x | 1.18x | 1.27x | positive |

Std/RVV checksum 均为 `7973167110771994084`。5-run 均高于 1.0，复跑预算按计划用完，
decision bucket 稳定为 positive（正向）。性能结论只来自 Milkv-Jupiter board repeated，
不使用 QEMU timing。

## Evidence Doctor 解释

Evidence Doctor 报告 `Errors=0, Warnings=0, Suggestions=0`。manifest 显示 strict A/B
的 boundary、wrapper、row source、checksum policy、timer boundary、gate、mask 和 reduction
字段一致；环境 metadata 仍缺少 taskset、governor、freq、temperature 和 binary hash，因此该证据足以
支撑诊断边界下的正向 bucket，但不升级为 production evidence（生产证据）。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production_shaped_diagnostic` |
| A/B boundary | `test_helper`；Std build 用真实 label extractor 的 `COLORS_MONO` 生成参考，RVV build 用测试专用 candidate 直接写 `std::vector<uint16_t>`。 |
| 当前决策问题 | `RVV-vs-scalar` 的诊断筛选。 |
| diagnostic 是否可外推到 production | 只能外推为 bounded production candidate（有界生产候选）。真实 production 仍需 color-mode dispatch、fallback、`PCLImage` metadata、NaN post-pass 和 public entry direct test。 |
| comparison-boundary / baseline mismatch 风险 | 存在。candidate 绕开了真实 `PCLImage` 写入和 `COLORS_*` switch 的其它分支。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用，本阶段为 positive；若后续生产证据转弱或负向，必须停在 PI5 用户检查点，不能自动采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 需要。当前没有 production patch，也没有 production direct 证据。 |

## Matrix 更新

- `label_mono16_stride_v0`：`planned -> partial-production-candidate within diagnostic boundary`。
- 当前已冻结 PI2 production probe 范围仍不变：`rgb_segment_store_v1` 和 `scaling_reduction_v1`。
  若要把 label mono16 加入 production patch，需要用户明确扩大 PI2 范围或另开 PI1。
- `normal_float_stride_v0` 保持 rejected within diagnostic boundary。

## Evidence Freshness 和 Registry

`log/board/repeated_phase070/summary.md`、`evidence_manifest.json` 和 `evidence_doctor.md`
由本阶段命令生成。当前 topic 没有把 registry 作为 gate；提交前仍需要路径限定 status scan、
`git diff --check` 和 raw log 提交边界确认。

## Continue / Stop Decision

`continue_stop_decision=turn_stop_deferred with stop_condition_hit`。

当前 topic-local roadmap 中的 RGB、scaling、normal 和 label mono16 诊断条目都已有阶段性证据。
继续到 production patch 会修改 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，且当前
Phase 040/050 冻结的 PI2 范围不包含 label mono16；扩大或执行 PI2 都需要用户明确确认。

`next_phase_default=PI2-production-patch after explicit user confirmation`，或用户明确扩大范围后先写
`PI1-label-mono16-production-integration-plan`。
