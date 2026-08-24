# point_cloud_image_extractors RVV topic

本目录是 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 的 RVV
专项测试资产。当前已采纳三个窄范围 production patch（生产补丁）：RGB/RGBA exact 点型、
intensity full-range exact `PointXYZI` 和 label mono16 exact `PointXYZL` 均已有 production direct
correctness（真实生产路径正确性）和 production-public board evidence（公开入口板卡证据）。

## 先读什么

| 文档 | 作用 |
| --- | --- |
| `doc/point_cloud_image_extractors-evaluation.zh.md` | 函数级评估、证据边界和生产接入判断。 |
| `doc/testing-overview.zh.md` | 测试入口分类、target 粒度审计和 QEMU / board 证据边界。 |
| `doc/correctness-tests.zh.md` | gtest 输入、被测路径、断言和不能证明的范围。 |
| `doc/benchmark-and-evidence.zh.md` | bench label、case-filter、板卡 repeated、manifest 和 Evidence Doctor。 |
| `doc/optimization-evidence.zh.md` | candidate family 到 correctness、bench、asm、board 和 decision 的索引。 |
| `doc/test-support-code-map.zh.md` | `src/`、`include/`、`script/` 和 evidence output 的代码地图。 |
| `doc/optimization-roadmap.zh.md` | 后续 RGB store、scaling、production probe 搜索空间。 |
| `doc/phases/000-current-state-and-diagnostic-scaffold/plan.zh.md` | 当前阶段计划、范围和停止条件。 |
| `doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md` | 首阶段 correctness、asm、板卡 repeated 和 Evidence Doctor 结果。 |
| `doc/phases/010-scaling-full-range-reduction-ab/plan.zh.md` | full-range scaling RVV 规约候选计划。 |
| `doc/phases/010-scaling-full-range-reduction-ab/result.zh.md` | full-range scaling RVV 规约候选结果。 |
| `doc/phases/020-rgb-segment-store-ab/plan.zh.md` | RGB segment-store 候选计划。 |
| `doc/phases/020-rgb-segment-store-ab/result.zh.md` | RGB segment-store 候选结果。 |
| `doc/phases/030-doc-suite-parity/result.zh.md` | topic-local doc suite 职责审计结果。 |
| `doc/phases/040-pi1-production-integration-plan/plan.zh.md` | PI1 生产接入计划、fallback gate 和 PI2-PI5 证据计划。 |
| `doc/phases/040-pi1-production-integration-plan/result.zh.md` | PI1 计划回填和用户确认门槛。 |
| `doc/phases/050-pi2-gate-policy-test-support/result.zh.md` | PI2 gate policy 的测试专用验收结果。 |
| `doc/phases/060-normal-field-diagnostic/result.zh.md` | normal field 诊断候选的负向板卡结果和 Evidence Doctor 解释。 |
| `doc/phases/070-label-mono16-diagnostic/result.zh.md` | label mono16 诊断候选的正向板卡结果和 Evidence Doctor 解释。 |
| `doc/phases/080-pi2-production-patch/result.zh.md` | 当前 production patch 的 PI2-PI5 结果和 PI5 用户检查点。 |
| `doc/phases/090-label-mono16-production-plan/result.zh.md` | label mono16 production-public 弱正收益结果和采纳边界。 |
| `doc/phases/optimization-matrix.zh.md` | candidate、点类型、证据和决策矩阵。 |

## 常用命令

| 命令 | 证据角色 |
| --- | --- |
| `make run_test_compare` | QEMU correctness（QEMU 正确性），Std/RVV 对拍。 |
| `make run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 1 --case-filter all'` | QEMU bench smoke（日志形状，不作为性能结论）。 |
| `make dump_bench_rvv` | 反汇编检查，确认候选 binary 中有 RVV 指令。 |
| `make board_smoke` | 板卡 correctness + bench compare，性能结论只从这里或 repeated board 产生。 |
| `make collect_board_repeated BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter all' PCIE_REPEATED_RUNS=5` | 5-run repeated board，生成可给 Evidence Doctor 使用的板卡证据。 |

## 当前边界

- 已覆盖：`PointXYZRGB` / `PointXYZRGBA` 的 RGB/RGBA unpack diagnostic，
  `PointXYZI::intensity` 的 no-scaling / fixed-factor / full-range scaling diagnostic，
  `PointNormal` normal_x/y/z 到 `rgb8` 的 diagnostic，`PointXYZL` label 到 `mono16` 的
  diagnostic，`PointXYZI` NaN 涂黑 post-pass。
- 当前结果：RGB/RGBA unpack diagnostic 稳定正向；full-range scaling v0 负向；full-range
  `scaling_reduction_v1` 正向；`rgb_segment_store_v1` 明显强于 RGB v0；fixed-factor scaling 弱正向；
  `normal_float_stride_v0` 在 Phase 060 诊断边界下 5/5 退化，median 0.61x；
  `label_mono16_stride_v0` 在 Phase 090 生产边界下 weak-positive，median 1.08x 且 Doctor 无 finding。
- 未覆盖：label random / Glasbey `std::map` 路径、normal field production、
  PNG writer 入口、`pcd2png` 工具端到端路径和泛型字段 offset production gate。
- Phase 080 production-public 结果：`production_rgb_pointxyzrgb_640x480` median 1.54x、
  `production_rgb_pointxyzrgba_640x480` median 1.55x、
  `production_scaling_full_range_intensity_640x480` median 1.52x，Evidence Doctor 无 finding。
- Phase 090 production-public 结果：`production_label_mono16_pointxyzl_640x480` median 1.08x、
  min 1.05x、max 1.09x，Evidence Doctor 无 finding。
- 长期 `doc-rvv/io/point_cloud_image_extractors-RVV.zh.md` 已适用，记录当前 adopted production behavior。
- 当前默认恢复入口：ready_for_review / commit decision。默认不提交 raw board logs。
