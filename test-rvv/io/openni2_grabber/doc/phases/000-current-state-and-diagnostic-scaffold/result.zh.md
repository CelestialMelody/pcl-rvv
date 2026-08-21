# Phase 000 Result: current-state-and-diagnostic-scaffold

## 结论

本阶段完成 `io/src/openni2_grabber.cpp` 的 OpenNI2 frame-to-cloud（帧到点云）production-shaped diagnostic（生产形态诊断）闭环。证据支持 `depth_xyz_contiguous_rvv` 作为窄范围 bounded production probe（有界生产探针）候选：同尺寸 `PointXYZ` 深度反投影在 Milkv-Jupiter 5 次 repeated board（板卡重复采集）中 median 1.21x，min 1.15x，max 1.22x，checksum 对齐。

本阶段不修改 production（生产源码），因此不能声明 production-ready（生产可采纳）。当前 EvidenceDecision（证据决策）为 `partial-production-candidate`：只建议进入 `010-production-integration-plan`，冻结 `convertToXYZPointCloud` / `PointXYZ` / 同尺寸 depth map 的生产接入边界；不建议把 RGB overlay（颜色覆盖）、depth/image mismatch（分辨率不一致映射）或 IR intensity（红外强度）纳入下一轮生产补丁。

## 计划执行回填

| action | result | evidence |
| --- | --- | --- |
| RED: 写最小 same-chain gtest | 已完成。最初因缺少 `openni2_grabber.h` 失败，符合 RED 预期；后续 RGB pack 和 IR union 假设也被测试暴露并修正。 | `src/test_openni2_grabber.cpp` |
| GREEN: 增加 scalar reference / RVV candidate 聚合入口 | 已完成。Std/RVV 两侧 correctness 通过，包含 depth invalid mask、RGB alpha/pack、mismatch slots、IR intensity 和大帧 bitwise 对拍。 | `make run_test_compare` |
| 补 bench 入口 | 已完成。QEMU 只做 RVV bench smoke（小型验证）和日志形状，不作为性能结论。 | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` |
| 反汇编检查 | 已完成。可见并可归因到 topic helper 的 `vle16.v`、`vfcvt.f.xu.v`、`vfmul`、`vmseq`、`vsse32.v`、`vlseg3e8.v`。 | `build/asm/riscv/bench_openni2_grabber_rvv.asm` |
| 板卡 smoke / repeated diagnostic | 已完成。5-run repeated summary 生成；最终采用修复后 summary。一次修复前 checksum mismatch 降级为历史失败，不参与结论。 | `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/summary.md` |
| Evidence Doctor | 已完成 manifest-based 检查。报告有 2 个 Error，均来自 IR 和 mismatch case 的退化频率；depth case 无 Error / Warning。 | `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_doctor.md` |

## Correctness 证据

`make run_test_compare` 最终通过。测试覆盖：

| TEST | 证明范围 |
| --- | --- |
| `DepthXYZMatchesProductionFormulaAndInvalidMask` | `0`、no-sample、shadow depth 写 NaN；有效 depth 使用 `z = pixel * 0.001f` 和 production 同序乘法。 |
| `RGBOverlayUsesProductionAlphaAndPackedOrder` | RGB 输入按 PCL `RGBValue` 内存布局写入，alpha 为 255。 |
| `MismatchedDepthWidthKeepsUnmappedRGBSlotsTransparent` | `cloud_width / depth_width` 整数 step 布点，未映射 slot 保持初始化 NaN/黑色/alpha 255。 |
| `IRPointCloudClearsColorStorageAndCopiesIntensity` | `PointXYZI::data_c` 清零后写 intensity，避免把 `data_c[0]` 误判为独立颜色 lane。 |
| `CandidateMatchesScalarBitwiseOnLargeFrame` | 大帧下 `PointXYZ` 与 `PointXYZRGBA` 的 xyz / rgba bitwise 对拍。 |

## 板卡性能证据

输入是 synthetic depth/RGB/IR frame，iterations=10，warmup=2，run_count=5，设备为 Milkv-Jupiter。性能结论仅来自板卡。

| case | median | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | --- | --- |
| `xyz_depth_full_640x480` | 1.21x | 1.15x | 1.22x | 1.21x, 1.22x, 1.21x, 1.15x, 1.21x | positive |
| `xyzrgba_depth_mismatch_320_to_640` | 0.98x | 0.96x | 1.04x | 0.98x, 0.98x, 1.04x, 0.96x, 1.00x | neutral / unstable |
| `rgba_overlay_full_640x480` | 1.02x | 1.00x | 1.05x | 1.02x, 1.03x, 1.05x, 1.00x, 1.00x | neutral |
| `xyzi_depth_ir_full_640x480` | 1.01x | 0.96x | 1.03x | 0.99x, 1.03x, 0.96x, 1.02x, 1.01x | neutral / unstable |

## Evidence Doctor 处理

manifest-based Evidence Doctor 结果为 `Errors=2, Warnings=1, Suggestions=2`。处理如下：

| signal | case | 处理 |
| --- | --- | --- |
| `ba_degradation_frequency` | `xyzi_depth_ir_full_640x480` | 2/5 低于 1，且当前 RVV candidate 对 IR 仍走 scalar；不作为生产候选。 |
| `ba_degradation_frequency` | `xyzrgba_depth_mismatch_320_to_640` | 4/5 低于 1，方向摇摆；不作为生产候选。 |
| `ba_degradation_frequency` | `rgba_overlay_full_640x480` | 1/5 低于 1，且 median 仅 1.02x；不作为生产候选。 |
| `near_threshold_ba` | RGB / IR | 接近 1.0，不接入生产；恢复条件是更强候选或 focused ablation。 |

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic。 |
| A/B boundary | topic-local scalar reference vs topic-local RVV candidate。 |
| 当前决策问题 | 是否存在足够正向、低风险的候选进入 bounded production probe。 |
| diagnostic 是否可外推到 production | 部分可外推。`PointXYZ` depth loop 公式、invalid mask、AoS store 与 production 对齐，但还没经过 `OpenNI2Grabber` 对象、resize buffer、signal callback 和真实 public entry。 |
| comparison-boundary / baseline mismatch 风险 | 有。production 还包含 focal length / principal point 读取、frame id、buffer resize、sensor orientation 和 signal path。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 只允许对稳定 positive 的 `PointXYZ` 同尺寸 depth path 继续；RGB、mismatch、IR 不满足下一轮 production probe 条件。 |
| clean adoption 是否需要 production boundary 证据 | 需要。PI5 前必须完成 production direct correctness、asm、board repeated 和 Evidence Doctor；PI5 后还需要用户确认采纳。 |

## Doc Suite Role Inventory

| role | status | path / evidence |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` |
| testing_overview | standalone | `doc/testing-overview.zh.md` |
| correctness_tests | standalone | `doc/correctness-tests.zh.md` |
| benchmark_and_evidence | standalone | `doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | standalone | `doc/optimization-evidence.zh.md` |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` |
| test_support_code_map | standalone | `doc/test-support-code-map.zh.md` |
| phase_index | standalone | `doc/phases/README.zh.md` |
| evaluation_diagnostic | standalone | `doc/openni2_grabber-evaluation.zh.md` |
| production_topic_doc | not_applicable with evidence | 没有 adopted production behavior；不创建 `doc-rvv/io/openni2_grabber-RVV.zh.md`。 |

## Evidence Registry

已补 `make generate_board_openni2_grabber_repeated_evidence_manifest`、`make record_board_openni2_grabber_repeated_evidence_state` 和 `make evidence_status`。当前登记对象为 `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/summary.md`、`test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_manifest.json` 与 `test-rvv/io/openni2_grabber/log/board/repeated_diagnostic/evidence_doctor.md`。registry 只用于 freshness guard（新鲜度检查）；raw logs 和本地生成日志仍默认不提交。

## 继续 / 停止判断

Phase 000 自身完成。后续已经创建 `010-production-integration-plan` 并冻结 depth-only PI1 边界。生产源码修改尚未授权；PI2 production patch 必须等显式生产接入授权。
