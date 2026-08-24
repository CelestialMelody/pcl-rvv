# Phase 090 Result: label mono16 production integration

## 结论

本阶段把 `label_mono16_stride_v0` 接入真实
`PointCloudImageExtractorFromLabelField<PointT>::extractImpl` 的 `COLORS_MONO` 分支。当前 production-public
（公开入口生产证据）板卡 repeated 为 weak-positive（弱正收益）：5-run median `1.08x`，min `1.05x`，
max `1.09x`，checksum 一致；Evidence Doctor（证据体检）为
`Errors=0, Warnings=0, Suggestions=0`。由于实现很小、fallback 清晰、只覆盖 exact `PointXYZL`，
且用户确认“有收益即可采纳”，本阶段 label mono16 窄范围生产补丁进入 adopted production behavior
（已采用生产行为）。

## 实际接入范围

| candidate | production entry | gate | fallback |
| --- | --- | --- | --- |
| `production_label_mono16_stride_v0` | `PointCloudImageExtractorFromLabelField<PointT>::extractImpl` 的 `COLORS_MONO` 分支 | exact `PointXYZL`，`label` 字段为单个 `UINT32`，offset 和 stride 满足 32-bit 跨步加载 | 非 RVV 构建、非 exact 点型、字段 metadata 不匹配、`COLORS_RGB_RANDOM` 和 `COLORS_RGB_GLASBEY` 均保持原标量分支 |

本阶段未修改 `io/include/pcl/io/point_cloud_image_extractors.h`，公开 API 不变。

## 执行动作回填

| action | status | evidence |
| --- | --- | --- |
| production gate / helper | done | 新增 `labelFieldSupportsProductionRVV`、`extractLabelMono16FieldStd`、`extractLabelMono16FieldRVV` 和 label hook。 |
| production direct correctness | done | `make run_test_compare`：Std/RVV 各 15/15。`LabelMono16HitsProductionRvvPath` 证明真实公开入口命中 RVV，`LabelRgbModesStayOnExistingScalarBranches` 保护 RGB label modes 不误命中。 |
| production bench label | done | 新增 `production_label_mono16_pointxyzl_640x480`，通过真实 label extractor 和 `COLORS_MONO`。 |
| QEMU / ASM | done | QEMU smoke 通过；`make dump_bench_rvv` 通过，反汇编可见 label 路径相关 `vlse32.v`、narrow 和 `vse16.v`。 |
| board repeated / Doctor | done | `log/board/repeated_phase090/summary.md` 和 `log/board/repeated_phase090/evidence_doctor.md`。 |
| docs | done | 本 result、matrix、roadmap、evaluation、role docs、长期 `doc-rvv` 和队列表同步到 label adopted。 |

## Correctness 与 fallback

| command | result | note |
| --- | --- | --- |
| `make run_test_compare` | pass | Std 15/15，RVV 15/15。 |

新增 production direct TEST 证明：

- exact `PointXYZL` 的真实 label extractor 在 `COLORS_MONO` 下输出与标量参考完全一致，并在 RVV 构建命中 label RVV hook。
- `COLORS_RGB_RANDOM` 和 `COLORS_RGB_GLASBEY` 不进入 mono16 RVV helper，保持既有标量状态机路径。

## QEMU 与 ASM

| evidence | result |
| --- | --- |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter production_label_mono16_pointxyzl_640x480'` 通过，只证明路径和日志形状。 |
| ASM dump | `make dump_bench_rvv` 通过；label production-public bench binary 中可见 `vlse32.v`、narrow 和 `vse16.v`。 |

QEMU timing 不作为性能结论。

## Board repeated 与 Evidence Doctor

复跑预算：5 runs；每 run 使用 `--iterations 20 --warmup-iterations 3`。证据路径：

- `log/board/repeated_phase090/summary.md`
- `log/board/repeated_phase090/evidence_manifest.json`
- `log/board/repeated_phase090/evidence_doctor.md`

| production-public case | median | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | --- | --- |
| `production_label_mono16_pointxyzl_640x480` | 1.08x | 1.05x | 1.09x | 1.07x, 1.08x, 1.05x, 1.08x, 1.09x | weak-positive adopted |

checksum 在 Std/RVV 之间一致。Evidence Doctor 输入为 production-public manifest，输出为
`Errors=0, Warnings=0, Suggestions=0`。

## 证据边界

本阶段只证明 exact `PointXYZL`、organized cloud order、`COLORS_MONO`、`label` 单个 `UINT32`
字段和 `640x480` synthetic organized cloud input。它不证明：

- generic label-like 点型。
- `COLORS_RGB_RANDOM` 或 `COLORS_RGB_GLASBEY`。
- normal field。
- PNG writer、`pcd2png` 或其它端到端调用链。

## Continue / Stop decision

`continue_stop_decision`：adopt label mono16 production path；当前 topic 的已采纳生产范围包括
RGB/RGBA exact 点型、intensity full-range exact `PointXYZI` 和 label mono16 exact `PointXYZL`。

`stop_condition_hit`：当前授权范围内没有新的高优先级未阻塞生产优化方向。normal v0 已被当前形态负向证据拒绝；
generic point type、label RGB modes、PNG writer 和 `pcd2png` 属于新的范围扩展，需要单独 phase 授权和证据。

`next_phase_default`：ready_for_review / commit decision；默认不提交，等待用户是否要求提交。
