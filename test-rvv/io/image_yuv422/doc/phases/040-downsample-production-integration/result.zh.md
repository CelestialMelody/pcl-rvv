# Phase 040: downsample-production-integration Result

## 当前结论

Phase 040 已完成 RGB downsample production integration loop（生产接入闭环）PI1-PI5。当前工作区中的
`io/src/image_yuv422.cpp` 已包含 downsample production probe：`ImageYUV422::fillRGB` 在
`__RVV10__` 且 downsample ratio 为偶数时可走 `fillRGBDownsampleRVV`；full-size 已采纳路径保持不变，
灰度和 OpenNI legacy 仍不修改。

EvidenceDecision：`adopted_production_behavior`。production direct correctness、QEMU smoke、
asm attribution（反汇编归属）、5-run board 和 Evidence Doctor 均支持保留 downsample 增量；
用户已确认采纳当前 production patch，因此 RGB downsample 现在写入 adopted production behavior。

## PI 动作回填

| PI | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| PI1 plan | done | `040-downsample-production-integration/plan.zh.md` | 范围冻结为 `ImageYUV422::fillRGB` RGB downsample，比例 gate 为水平/垂直偶数 ratio；灰度和 OpenNI legacy 不扩大。 |
| PI2 production patch | done | `io/src/image_yuv422.cpp` | 新增 `canUseRGBDownsampleRVV`、`fillRGBDownsampleRVV` 和 contiguous RGB segment store；public entry full-size gate 优先，downsample gate 次之。 |
| PI3 production direct correctness | done | `make -C test-rvv/io/image_yuv422 run_test_compare` | Std/RVV 各 6 个 gtest 通过；production downsample 真实入口与标量 reference 对拍并覆盖 padding。 |
| PI4 QEMU smoke | done | `make -C test-rvv/io/image_yuv422 run_bench_rvv BENCH_ARGS="--case-filter prod_rgb_downsample_640x480_to_320x240 --iterations 1 --warmup-iterations 1"` | production downsample bench label 可运行且 checksum 非空；QEMU timing 不作为性能结论。 |
| PI4 asm | done | `make -C test-rvv/io/image_yuv422 dump_bench_rvv` | `ImageYUV422::fillRGB` 符号内可见 RVV 计算和 segmented store；downsample 路径命中生产符号。 |
| PI4 board performance | done | `log/board/prod_rgb_downsample_640x480_to_320x240_repeat_5/summary.md` | production public 5-run：mean `3.8150x`、median `3.9294x`、min `3.3750x`、max `3.9692x`；mean Std `2.2022 ms`，mean RVV `0.5794 ms`。 |
| PI4 Evidence Doctor | done | `log/board/prod_rgb_downsample_640x480_to_320x240_repeat_5/evidence_manifest.json`、`log/board/prod_rgb_downsample_640x480_to_320x240_repeat_5/evidence_doctor.md` | `production_public`、`strict_ab=true`、`Errors=0, Warnings=1, Suggestions=0`；warning 为 long-tail / variance。 |
| PI5 user checkpoint | adopted | 本 result、evaluation、matrix、roadmap、`doc-rvv/io/image_yuv422-RVV.zh.md` | 用户已确认采纳当前 downsample production 增量；正式文档和队列表同步为 adopted。 |

## Evidence Doctor 解释

Doctor warning：`long_tail_or_variance`。5-run speedup 为 `3.9673x`、`3.8343x`、`3.9294x`、
`3.9692x`、`3.3750x`，max/min 为 `1.18`。第五次 run 低于前四次，但仍为强正向；
min `3.3750x` 明显高于 positive 桶阈值，方向稳定。结论保留 min / median / max，不剔除任何 run。

## Fallback 矩阵

| 条件 | 当前行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | `fillRGBStd` 标量路径 | `run_test_compare` Std 侧 6 个 gtest 通过。 |
| full-size even-width RGB | 已采纳 RVV path | Phase 020 production evidence。 |
| RGB downsample，水平/垂直 ratio 为偶数 | 当前工作区命中 `fillRGBDownsampleRVV` | Phase 040 production direct correctness、asm、board repeated。 |
| 其它 resize 形态 | 标量 fallback | gate 未覆盖，不外推。 |
| `fillGrayscale` | 标量路径 | production direct grayscale gtest 通过。 |
| OpenNI legacy YUV422 | 不触碰 | 本 phase scope 排除。 |

## Continue / Stop Decision

用户已确认采纳当前 production patch 后，本阶段完成 S11 production closeout 同步。当前 adopted
范围为 full-size even-width RGB 和 RGB downsample 偶数 ratio gate；灰度和 OpenNI legacy 仍不扩大。
后续若继续优化，必须从 roadmap 中选择新的未阻塞候选，且不能把 full-size / downsample 证据外推到
灰度或 OpenNI legacy。
