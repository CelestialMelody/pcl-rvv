# Phase 020: production-integration-plan Result

## 当前结论

Phase 020 已完成 PI1-PI5 的 production integration loop（生产接入闭环）。PI5 用户检查点之后，
用户已明确确认采纳当前 production patch。
生产补丁已接入 `pcl::io::ImageYUV422::fillRGB` 的 full-size even-width RGB 路径：
RVV build（启用 `__RVV10__`）命中 `vlse8` + `vssseg3e8` 的 YUYV 到 RGB 转换；
非 RVV 构建、RGB downsample、奇数宽度和 `fillGrayscale` 仍走标量 fallback（回退路径）。

EvidenceDecision：`adopted_production_behavior`。生产 direct correctness、
asm attribution（反汇编归属）、板卡 repeated bench 和 Evidence Doctor 均支持保留当前补丁；
用户确认后，当前补丁成为已采纳生产行为。未覆盖范围仍不得从 full-size RGB 外推。

## PI 动作回填

| PI | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| PI1 plan | done | `020-production-integration-plan/plan.zh.md` | 范围冻结为 `ImageYUV422::fillRGB` full-size even-width RGB；downsample、灰度和 OpenNI legacy 不扩大。 |
| PI2 production patch | done | `io/src/image_yuv422.cpp` | 抽出 `fillRGBStd`，新增 `fillRGBFullSizeRVV`，public entry 在 `__RVV10__` 且 full-size even-width 时短路 RVV。 |
| PI3 production direct correctness | done | `make -C test-rvv/io/image_yuv422 run_test_compare` | QEMU Std/RVV 各 6 个 gtest 通过；新增 3 个 production direct 测试覆盖 RGB full-size、RGB downsample fallback、grayscale fallback。 |
| PI4 QEMU smoke | done | `make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter prod_rgb_full_640x480 --iterations 1 --warmup-iterations 1"` | production bench label 可运行且 checksum 非空；QEMU timing 不作为性能结论。 |
| PI4 asm | done | `make -C test-rvv/io/image_yuv422 dump_bench_rvv` | `pcl::io::ImageYUV422::fillRGB` 符号内可见 `vlse8.v`、`vmul.vx`、`vsra.vi`、`vssseg3e8.v`。 |
| PI4 board correctness | done | `make -C test-rvv/io/image_yuv422 run_board_test fetch_board_logs` | 板卡 RVV test 6 个 gtest 通过。 |
| PI4 board performance | done | `prod_rgb_full_640x480_repeat_5` run set | 5-run production public strict A/B：mean `2.0297x`、median `2.0247x`、min `2.0102x`、max `2.0576x`；mean Std `3.2082 ms`，mean RVV `1.5807 ms`。 |
| PI4 Evidence Doctor | done | `log/board/prod_rgb_full_640x480_repeat_5/evidence_manifest.json`、`log/board/prod_rgb_full_640x480_repeat_5/evidence_doctor.md` | `production_public`、`strict_ab=true`、`Errors=0, Warnings=0, Suggestions=0`。 |
| PI5 user checkpoint | confirmed adopted | 本 result、evaluation、matrix、roadmap、`doc-rvv/io/image_yuv422-RVV.zh.md` | 用户已确认采纳 / 保留当前 production patch；状态升级为 adopted production behavior。 |

## Production patch 边界

| 路径 | 状态 |
| --- | --- |
| `io/src/image_yuv422.cpp` | 修改；新增文件局部标量 helper 和 RVV helper，公开 API 不变。 |
| `io/include/pcl/io/image_yuv422.h` | 未修改。 |
| `io/src/openni_camera/openni_image_yuv_422.cpp` | 未修改；legacy parity 暂缓。 |
| `test-rvv/io/image_yuv422/**` | 新增 production direct test、bench label、manifest wrapper repeated 支持和阶段文档。 |

## Fallback 矩阵

| 条件 | 当前行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | `fillRGBStd` 标量路径 | `run_test_compare` Std 侧 6 个 gtest 通过。 |
| full-size even-width RGB | RVV build 命中 `fillRGBFullSizeRVV` | production asm 和 board repeated evidence。 |
| RGB downsample | 标量 fallback | `ImageYUV422ProductionDirect.RGBDownsampleKeepsScalarFallbackSemantics`。 |
| `fillGrayscale` full/downsample | 原标量路径 | `ImageYUV422ProductionDirect.GrayscaleStillMatchesScalarReference`。 |
| OpenNI legacy YUV422 | 不触碰 | 本 phase scope 排除；后续需单独 parity phase。 |

## Evidence Doctor 解释

当前 production manifest 的 `run_count=5`，`evidence_role=production_public`，`strict_ab=true`。
两侧 boundary 均为 `public_overload`，wrapper 均为 `pcl::io::ImageYUV422`，checksum policy
均为 `output_bytes_fnv1a`。Evidence Doctor 报告 `Errors=0, Warnings=0, Suggestions=0`。

## Continue / Stop Decision

本阶段在 PI5 后已获得用户采纳确认，production patch 状态升级为 adopted production behavior
（已采纳生产行为）。默认下一步是进入 S11 production closeout（生产收尾）并继续审计
roadmap 中仍在当前 topic 范围内的未阻塞候选。当前未覆盖范围包括 RGB downsample、灰度和
OpenNI legacy；这些范围必须作为独立 phase 重新验证，不能从 full-size RGB 证据外推。
