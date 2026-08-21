# Phase 020 结果：YUV planar 生产探针

## 阶段结论

PI2-PI5 已推进到用户检查点。`LZFYUV422ImageReader::read/readOMP` 已接入有界 RVV
production patch（生产补丁）：解压和 `cloud.resize()` 完成后，若 `PointT` 是 standard-layout
且 `r/g/b` 字段表达式都是 `std::uint8_t`，RVV helper 接管 planar YUV422 到 RGB 的转换；
其它点型、非 RVV 构建或奇数像素数自然回到标量 helper。

接入后的板卡 repeated summary（重复板卡摘要）显示生产 helper 仍有收益，但低于诊断阶段：
Milkv-Jupiter 5-run mean `1.0864x`、median `1.0832x`、min `1.0732x`、max `1.1135x`。
Evidence Doctor（证据体检）为 `Errors=0, Warnings=0, Suggestions=0`，registry freshness
检查通过。按 phase 计划的决策桶，这是 `weak-positive`。

建议：由于 production diff 较小、fallback gate 清晰、correctness / asm / board / Doctor 均闭合，
建议保留这次 YUV planar 接入。用户已确认“有收益即可采纳”，S11 production closeout（生产收尾）
已创建正式 `doc-rvv/io/lzf_image_io-RVV.zh.md`，该文档使用本阶段生产板卡数据。

## 执行动作回填

| action | status | evidence / result |
| --- | --- | --- |
| 先写 production helper direct red test | done | `make run_test_compare` 在 helper 不存在时编译失败，失败点是 `pcl::io::detail` 尚未声明。 |
| 生产 helper 和 dispatch | done | `io/include/pcl/io/impl/lzf_image_io.hpp` 新增 `convertPlanarYuv422ToPointCloudStd`、`StdOMP`、`RVV`，`read/readOMP` 改为 RVV 短路 + 标量 fallback。 |
| production correctness | done | `make run_test_compare`：Std/RVV 各 5 个 gtest 通过；板卡 `make run_board_test fetch_board_logs` 通过。 |
| production bench case | done | `src/bench_lzf_image_io.cpp` 新增 `yuv422_planar_rgb_production_640x480`。 |
| QEMU bench smoke | done | `make run_bench_rvv BENCH_ARGS="--case-filter yuv422_planar_rgb_production_640x480 --iterations 2 --warmup-iterations 1"` 通过；只作为日志形状证据。 |
| asm attribution | done | `make dump_bench_rvv` 生成 `build/asm/riscv/bench_lzf_image_io_rvv.asm`；可见 YUV 路径的 `vlse8.v`、`vsra.vi`、`vsse8.v`。 |
| board repeated + Doctor | done | `make run_board_lzf_yuv422_production_repeated` 生成 `log/board/production_yuv422_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}`。 |
| registry freshness | done | `make check_production_evidence_freshness`：fresh。 |

## 生产证据

| evidence | path / command | result |
| --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 各 5 个 gtest 通过。 |
| board correctness | `make run_board_test fetch_board_logs` | 板卡 RVV gtest 5/5 通过。 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter yuv422_planar_rgb_production_640x480 --iterations 2 --warmup-iterations 1"` | 生产 case 可运行；QEMU timing 不用于性能结论。 |
| asm | `build/asm/riscv/bench_lzf_image_io_rvv.asm` | YUV RVV 指令存在并归属到 bench 内联边界。 |
| board performance | `log/board/production_yuv422_repeat_5/summary.md` | mean `1.0864x`、median `1.0832x`、min `1.0732x`、max `1.1135x`。 |
| Evidence Doctor | `log/board/production_yuv422_repeat_5/evidence_doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 |
| registry | `log/evidence_registry.json` | run label `board-lzf-image-io-production-yuv422-repeat-phase020` fresh。 |

## 诊断到生产错配审计回填

| question | result |
| --- | --- |
| evidence role | 当前生产 repeated 为 `production_detail`，不是 phase 000 的 diagnostic。 |
| A/B boundary | baseline 和 candidate 都在 `production_detail_helper` 边界，case 为 `yuv422_planar_rgb_production_640x480`。 |
| 当前决策问题 | 当前 public RVV path 的生产 helper 是否快于生产标量 helper。 |
| diagnostic 是否可外推到 production | phase 000 只作为进入 PI2 的信号；PI5 结论以本阶段生产证据为准。 |
| comparison-boundary / baseline mismatch 风险 | 已降级：本阶段同一 bench case 内比较 production Std helper 与 production RVV helper，checksum 一致。 |
| weak-positive 处理 | median `1.0832x`，未达到 positive 桶；由于实现小、风险低、Doctor 无 finding，建议用户确认保留。 |
| clean adoption 条件 | 用户已确认有收益即可采纳；正式 `doc-rvv/io/lzf_image_io-RVV.zh.md` 已创建。 |

## 当前未覆盖范围

- `depth_xyz_rvv` 仍为 deferred diagnostic，没有接 production。
- `rgb_buffer_to_cloud_rvv` 仍为 rejected diagnostic，没有接 production。
- `LZFRGB24ImageReader`、Bayer debayer 本体、文件读取、PCLZF header、`decompress()` 和 ImageGrabber 调度未覆盖。
- RGB member gate 是本地窄 gate，尚未扩展为 PCL traits 级 RGB/RGBA 泛型策略。

## Continue / Stop Decision

`continue_stop_decision`：PI5 用户检查点已通过，S11 production closeout 完成。

`stop_condition_hit`：当前无继续自动扩大 production patch 的高优先级未阻塞方向。YUV path 已采纳；
depth、RGB copy、Bayer debayer、泛型 RGB traits 和 `lzfDecompress` 都需要新的 profile、
新 candidate 或独立 phase 授权。

`next_phase_default`：ready_for_review / optional commit phase。若后续继续优化，优先另开或新建
phase 审计泛型 RGB/RGBA traits、depth16 xyz 或 Bayer debayer，不从当前 weak-positive YUV patch
直接扩大范围。
