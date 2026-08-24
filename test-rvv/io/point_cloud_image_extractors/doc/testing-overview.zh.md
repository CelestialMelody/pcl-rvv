# Testing Overview

## 本文职责

本文说明 `point_cloud_image_extractors` topic 的测试入口、target 粒度和证据边界。
每个 TEST 的输入和断言见 `doc/correctness-tests.zh.md`；bench label、board repeated
和 Evidence Doctor（证据体检）见 `doc/benchmark-and-evidence.zh.md`。

## 阅读路径

| 文档 | 主职责 |
| --- | --- |
| `README.zh.md` | 入口导航、常用命令和当前结论。 |
| `doc/correctness-tests.zh.md` | correctness（正确性）测试字典。 |
| `doc/benchmark-and-evidence.zh.md` | bench、board、manifest 和 Doctor 边界。 |
| `doc/optimization-evidence.zh.md` | candidate family 到证据和决策的索引。 |
| `doc/test-support-code-map.zh.md` | helper、bench、script 和 output 的代码地图。 |
| `doc/point_cloud_image_extractors-evaluation.zh.md` | EvidenceDecision（证据决策）和 production 接入判断。 |

## 运行入口分类

| 类别 | 当前入口 | 证明什么 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make run_test_compare` | Std/RVV 两个构建的 helper、production direct、fallback 和 label RGB mode non-hit 输出一致。 | 不证明未冻结点型、label RGB modes 性能或 normal production。 |
| QEMU smoke（QEMU 小型验证） | `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter <label>'` | RVV binary 可运行，日志和 checksum 可输出。 | 不证明性能。 |
| asm attribution（反汇编归属） | `make dump_bench_rvv` 后 `rg` 查 `vlse32`、`vsseg3e8`、`vfred*` | 目标 RVV 指令存在于 bench binary。 | 仍不是 production hot path 归属。 |
| board smoke（板卡小型验证） | `make board_smoke` | 板卡可运行、checksum 和单次 compare。 | 不替代 repeated board。 |
| board repeated（板卡重复采集） | `make collect_board_repeated ... PCIE_REPEATED_DIR=<dir>` | 目标硬件上的稳定 performance bucket。 | 不证明 production direct。 |
| doctor / manifest | `make run_board_repeated_evidence_doctor PCIE_REPEATED_DIR=<dir>` | 检查 checksum、A/B 边界和退化频率。 | 不自动决定 production 接入。 |

## 输入数据总览

| 输入族 | 点类型 | 规模 | row source | 候选 |
| --- | --- | --- | --- | --- |
| RGB/RGBA | `PointXYZRGB`, `PointXYZRGBA` | correctness 小尺寸；bench `640x480` | organized cloud order | `rgb_u32_stride_unpack_v0`, `rgb_segment_store_v1` |
| intensity scaling | `PointXYZI` | correctness 小尺寸；bench `640x480` | organized cloud order | `scaling_float_stride_v0`, `scaling_reduction_v1` |
| normal field | `PointNormal` | correctness 小尺寸；bench `640x480` | organized cloud order | `normal_float_stride_v0` |
| label mono16 | `PointXYZL` | correctness 小尺寸；bench `640x480` | organized cloud order | `label_mono16_stride_v0` |
| NaN post-pass | `PointXYZI` | correctness 小尺寸 | organized cloud order | 标量 post-pass 保留 |
| PI2 gate policy | `PointXYZRGB` / `PointXYZRGBA` / `PointXYZRGBL` / `PointXYZI` / `PointXYZINormal` | correctness policy cases | not_applicable | test-only gate helper |

## Target 粒度审计

| target 类别 | decision | 证据 |
| --- | --- | --- |
| correctness aggregate | adopted | `make run_test_compare` 覆盖当前 15 个 TEST。 |
| correctness aliases | not_applicable with evidence | 当前 gtest 数量小，暂无单独 filter target；TEST 字典已列明边界。 |
| bench diagnostic aliases | adopted | `--case-filter` 可隔离每个 bench label。 |
| QEMU smoke aliases | adopted | 可通过 `run_bench_rvv` + `--case-filter` 运行单 label smoke。 |
| board smoke aliases | adopted | `make board_smoke` 来自共享 board harness。 |
| board repeated aliases | adopted | `collect_board_repeated` 支持 `PCIE_REPEATED_DIR` 和 `PCIE_REPEATED_RUNS`。 |
| doctor / registry aliases | adopted | manifest + Doctor 已接入；当前不提交 raw log，topic-local registry 暂不作为本阶段 gate，phase result 使用路径限定 status scan 做 freshness check。 |
| historical probe guarded aliases | not_applicable with evidence | 当前无历史 production probe target。 |

## 当前结论边界

当前测试体系支持三个 adopted production behavior（已采用生产行为）：`production_rgb_segment_store_v1`、
`production_scaling_reduction_v1` 和 `production_label_mono16_stride_v0`。`normal_float_stride_v0`
已在 Phase 060 的板卡 repeated 中负向，不纳入当前 production scope。label random / Glasbey 和泛型
label-like 点型仍需单独 phase。
