# Phase 080 Result: PI2-PI5 production patch

## 结论

本阶段把 Phase 040/050 冻结的两个候选接入
`io/include/pcl/io/impl/point_cloud_image_extractors.hpp`，并补齐 production-public（公开入口生产证据）
correctness、QEMU smoke（QEMU 小型验证）、反汇编、板卡 repeated（重复板卡性能测试）和
Evidence Doctor（证据体检）。当前证据支持保留这个有界 production patch（生产补丁）；用户已确认
“板卡上的测试结果如果显示有收益即可采纳”，因此本阶段的 RGB/scaling 窄范围生产补丁进入
adopted production behavior（已采用生产行为）。长期 `doc-rvv` 主题文档使用接入后的板卡数据。

## 实际接入范围

| candidate | production entry | gate | fallback |
| --- | --- | --- | --- |
| `production_rgb_segment_store_v1` | `PointCloudImageExtractorFromRGBField<PointT>::extractImpl` | exact `PointXYZRGB` / `PointXYZRGBA`，`rgb` 或 `rgba` 字段为 `FLOAT32` / `UINT32`，字段 count 为 1，offset 和 stride 满足 32-bit 跨步加载 | 非 RVV 构建、非 exact 点型、字段 metadata 不匹配时走 `extractRgbFieldStd` |
| `production_scaling_reduction_v1` | `PointCloudImageExtractorWithScaling<PointT>::extractImpl`，由 `PointCloudImageExtractorFromIntensityField` 进入 | exact `PointXYZI`，`field_name_ == "intensity"`，`SCALING_FULL_RANGE`，字段为单个 `FLOAT32` | 非 RVV 构建、非 exact 点型、非 intensity、非 full-range 或字段 metadata 不匹配时走 `extractScalingFieldStd` |

`label_mono16_stride_v0` 仍保持诊断边界；normal field 仍为负向诊断结果。本阶段未修改
`io/include/pcl/io/point_cloud_image_extractors.h`，公开 API 不变。

## 执行动作回填

| action | status | evidence |
| --- | --- | --- |
| RED production-direct tests | done | production patch 前 `make run_test_rvv` 中 5 个新增 production direct TEST 因 hook 为 `None` 失败，证明测试能捕获缺失 dispatch。 |
| production helper split | done | RGB 和 scaling 原标量主体抽为 `detail::extractRgbFieldStd` / `detail::extractScalingFieldStd`；public `extractImpl` 只做字段查找、RVV short-circuit 和 Std fallback。 |
| RVV RGB path | done | exact RGB/RGBA 公开入口命中 `extractRgbFieldRVV`；反汇编归属含 `vlse32.v` 和 `vsseg3e8.v`。 |
| RVV scaling path | done | exact `PointXYZI::intensity` full-range 公开入口命中 `extractScalingFullRangeIntensityRVV`；反汇编归属含 `vlse32.v`、`vfredmin.vs`、`vfredmax.vs`。 |
| fallback tests | done | `PointXYZRGBL` RGB fallback、fixed-factor intensity fallback、`z` full-range fallback 均走标量 hook。 |
| production bench labels | done | `production_rgb_pointxyzrgb_640x480`、`production_rgb_pointxyzrgba_640x480`、`production_scaling_full_range_intensity_640x480` 走真实公开 extractor。 |

## Correctness 与 fallback

| command | result | note |
| --- | --- | --- |
| `make run_test_rvv` | pass, 13/13 | RVV 构建覆盖 diagnostic、PI2 gate policy、production direct path-hit 和 fallback。 |
| `make run_test_compare` | pass | Std 13/13，RVV 13/13。 |

新增 production direct TEST 证明：

- `PointXYZRGB` / `PointXYZRGBA` 的真实 RGB extractor 输出与标量参考完全一致，并在 RVV 构建命中 RVV hook。
- `PointXYZRGBL` 有 RGB 字段但不在本阶段 exact gate 内，真实入口 fallback 到标量。
- `PointXYZI` intensity full-range 输出与标量参考一致，并在 RVV 构建命中 RVV hook。
- intensity fixed-factor 和 `z` full-range 不在本阶段 gate 内，真实入口 fallback 到标量。
- base `extract` 的 NaN post-pass 仍在 RVV `extractImpl` 后生效。

## QEMU 与 ASM

| evidence | result |
| --- | --- |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter production_rgb_pointxyzrgb_640x480,production_rgb_pointxyzrgba_640x480,production_scaling_full_range_intensity_640x480'` 通过，只证明路径和日志形状。 |
| ASM dump | `make dump_bench_rvv` 通过。RGB public entry 可归属 `vlse32.v` / `vsseg3e8.v`；scaling helper 可归属 `vlse32.v` / `vfredmin.vs` / `vfredmax.vs`。 |

QEMU timing 不作为性能结论。

## Board repeated 与 Evidence Doctor

复跑预算：5 runs；每 run 使用 `--iterations 20 --warmup-iterations 3`。证据路径：

- `log/board/repeated_pi4/summary.md`
- `log/board/repeated_pi4/evidence_manifest.json`
- `log/board/repeated_pi4/evidence_doctor.md`

| production-public case | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | --- |
| `production_rgb_pointxyzrgb_640x480` | 1.54x | 1.53x | 1.57x | positive |
| `production_rgb_pointxyzrgba_640x480` | 1.55x | 1.54x | 1.57x | positive |
| `production_scaling_full_range_intensity_640x480` | 1.52x | 1.44x | 1.54x | positive |

checksum 在 Std/RVV 之间一致。Evidence Doctor 输入为 production-public manifest，输出为
`Errors=0, Warnings=0, Suggestions=0`。这支持“当前公开入口 RVV path 快于当前公开入口标量 path”
这一 RVV-vs-scalar 决策问题。

## 证据边界

本阶段不做 RVV-family-selection（RVV 实现族选择）。因此 production-public positive 不证明
`production_rgb_segment_store_v1` 或 `production_scaling_reduction_v1` 优于未来或已有其它 RVV family；
它只证明当前 public RVV path 相对当前 public scalar path 有收益。

仍未覆盖的范围：

- RGB 泛型 traits gate、`PointXYZRGBL` 等复合点型。
- `PointXYZINormal` 或其它带 intensity 字段的泛型点型。
- label mono16 production probe。
- normal field production probe。
- PNG writer、`pcd2png` 或其它端到端调用链。

## Continue / Stop decision

`continue_stop_decision`：用户确认后进入 S11 production closeout。

`stop_condition_hit`：not_applicable。PI5 用户确认已满足。

`next_phase_default`：继续当前 topic 的未阻塞候选。Phase 070 的 `label_mono16_stride_v0` 为正向诊断候选，
且用户要求仍有优化方向时继续推进；下一阶段为独立 `PI1-label-mono16-production-integration-plan`。
