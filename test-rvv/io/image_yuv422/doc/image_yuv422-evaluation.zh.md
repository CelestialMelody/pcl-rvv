# image_yuv422 函数级评估

## S2 函数级评估

`io/src/image_yuv422.cpp` 提供 `pcl::io::ImageYUV422::fillRGB` 和 `fillGrayscale`。公开入口从 `FrameWrapper` 读取 YUV422 数据，YUV422 在当前源码中按 `U Y1 V Y2` 排列，两个相邻像素共享 `U` / `V`。`fillRGB` 的全尺寸路径是本 topic 的主热点：每两个像素执行两组整数乘加、右移和 `CLIP_CHAR` 饱和写回。`fillGrayscale` 只复制 Y 分量，适合作为低风险内存路径候选。

当前判断：full-size RGB 与 RGB downsample 偶数 ratio gate 均为 `adopted_production_behavior`。Phase 020 已把 segment-store（段存储）RGB full-size 候选接入真实 `ImageYUV422::fillRGB` public entry 并获用户确认采纳。Phase 040 已把 RGB downsample 候选接入当前 production path，并完成 production direct（真实生产路径）correctness、asm、5-run board 和 Evidence Doctor；用户已确认采纳当前 production patch。

## 可 RVV 化片段

| 片段 | 计划 | 当前理由 |
| --- | --- | --- |
| `fillRGB` full-size | adopted production behavior | Phase 020 production public 5-run mean `2.0297x`，median `2.0247x`，min `2.0102x`，max `2.0576x`；Evidence Doctor `Errors=0, Warnings=0, Suggestions=0`。 |
| `fillRGB` full-size padded | adopted evidence strengthened | Phase 050 production public 5-run mean `2.1030x`，median `2.1012x`，min `2.0889x`，max `2.1169x`；Evidence Doctor `Errors=0, Warnings=0, Suggestions=0`。 |
| `fillRGB` downsample | adopted production behavior | Phase 040 production public 5-run mean `3.8150x`，median `3.9294x`，min `3.3750x`，max `3.9692x`；Evidence Doctor `Errors=0, Warnings=1, Suggestions=0`，warning 为 long-tail / variance。 |
| `fillGrayscale` full-size | rejected / scalar fallback | 灰度只复制 Y 分量，RVV byte-copy 诊断收益不足；当前 candidate 保持标量 reference。 |
| `fillGrayscale` downsample | rejected / scalar fallback | 灰度 downsample 仍只复制采样 Y 分量；当前不保留 active RVV candidate。 |
| OpenNI legacy YUV422 | 暂缓 | `io/src/openni_camera/openni_image_yuv_422.cpp` 公式同构，但依赖 OpenNI 条件编译和旧 wrapper；本 topic 先验证通用 `ImageYUV422`。 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ImageYUV422::fillRGB` | production public entry | YUV422 到 RGB24 转换 | OpenNI2 / ONI image wrappers | RGB 输出 buffer | adopted production RVV path | `io/src/image_yuv422.cpp` |
| `ImageYUV422::fillGrayscale` | production public entry | YUV422 到灰度输出 | image wrapper users | gray 输出 buffer | production boundary，当前未修改 | `io/src/image_yuv422.cpp` |
| `image_yuv422.h` | diagnostic reference / candidate | 测试专用标量参考链路和 RVV 候选 | gtest / bench | QEMU、asm、board evidence | production-shaped diagnostic | `test-rvv/io/image_yuv422/include/image_yuv422.h` |
| `test_image_yuv422.cpp` | correctness gate（正确性验收） | 验证饱和、两像素顺序、downsample 和 padding | `make run_test_compare` | QEMU logs | correctness diagnostic | `test-rvv/io/image_yuv422/src/test_image_yuv422.cpp` |
| `bench_image_yuv422.cpp` | bench wrapper（性能测试包装层） | 输出 case timing 和 checksum | board / QEMU smoke targets | bench summary / Evidence Doctor | performance diagnostic | `test-rvv/io/image_yuv422/src/bench_image_yuv422.cpp` |
| `generate_image_yuv422_evidence_manifest.py` | analysis script（分析脚本） | 生成 Evidence Doctor manifest（证据清单） | `make run_evidence_doctor` | `log/board/evidence_manifest.json` / `evidence_doctor.md` | evidence validation（证据校验） | `test-rvv/io/image_yuv422/script/generate_image_yuv422_evidence_manifest.py` |
| `010-rgb-segment-store-probe/result.zh.md` | phase result（阶段结果） | 记录 segment-store 候选、命令、证据和 EvidenceDecision | worker / reviewer | roadmap、matrix、Handoff | recovery pointer（恢复入口） | `test-rvv/io/image_yuv422/doc/phases/010-rgb-segment-store-probe/result.zh.md` |
| `020-production-integration-plan/result.zh.md` | phase result | 记录 production patch、fallback、production direct 证据和 PI5 用户确认边界 | worker / reviewer | roadmap、matrix、`doc-rvv` 草案 | production evidence pointer | `test-rvv/io/image_yuv422/doc/phases/020-production-integration-plan/result.zh.md` |
| `040-downsample-production-integration/result.zh.md` | phase result | 记录 downsample production probe、fallback、production direct 证据和 PI5 用户确认边界 | worker / reviewer | roadmap、matrix、`doc-rvv` 草案 | production evidence pointer | `test-rvv/io/image_yuv422/doc/phases/040-downsample-production-integration/result.zh.md` |
| `050-prod-rgb-full-padded-evidence/result.zh.md` | phase result | 记录 full-size padded output production-public repeated board 证据和剩余候选 stop decision | worker / reviewer | roadmap、matrix、`doc-rvv` | production evidence pointer | `test-rvv/io/image_yuv422/doc/phases/050-prod-rgb-full-padded-evidence/result.zh.md` |

## 诊断证据链

| 证据类别 | 当前结果 | 能证明 | 不能证明 |
| --- | --- | --- | --- |
| correctness | `run_test_compare` Std / RVV 各 3 个 gtest 通过 | 测试专用 reference 与 RVV candidate 的 RGB、downsample、灰度输出一致。 | 真实 production dispatch 尚未接入。 |
| QEMU smoke | `run_bench_rvv --case-filter rgb_full_640x480 --iterations 1 --warmup-iterations 1` 可运行 | bench 输出和 checksum 形状可解析。 | QEMU timing 不能证明性能。 |
| asm | `dump_bench_rvv` 可见 `vssseg3e8.v`、`vlse8.v`、整数乘法和饱和相关指令 | Segment-store RGB 写回命中 RVV 指令。 | 还不能归属到 production symbol。 |
| board | `run_board_yuv422_rgb_full fetch_board_logs`：`Std 2.3034 ms`、`RVV 1.5785 ms`，约 `1.46x` | 在 Milkv-Jupiter 上，test helper 边界 RGB full-size 有 positive 诊断信号。 | run count 低，不能替代 repeated production board。 |
| Evidence Doctor | `Errors=0, Warnings=1, Suggestions=0` | 当前 focused RGB manifest 没有 checksum 或边界 Error。 | `low_run_count` warning 要求生产接入阶段补 repeated board。 |

## 正确性与高效性证据链

| 证据类别 | 当前结果 | 能证明 | 不能证明 |
| --- | --- | --- | --- |
| production direct correctness | `make -C test-rvv/io/image_yuv422 run_test_compare`：Std/RVV 各 6 个 gtest 通过 | 真实 `ImageYUV422::fillRGB` full-size 与 RGB downsample 已采纳路径均和参考链路一致；grayscale fallback 仍保持标量语义。 | 不证明 OpenNI legacy 入口或其它 resize ratio。 |
| board correctness | `make -C test-rvv/io/image_yuv422 run_board_test fetch_board_logs`：板卡 RVV test 6 个 gtest 通过 | 目标硬件上 correctness gate 可运行。 | 不单独证明性能。 |
| QEMU smoke | `run_bench_rvv --case-filter prod_rgb_full_640x480 --iterations 1 --warmup-iterations 1` | production bench label 可运行并输出 checksum。 | QEMU timing 不进入性能结论。 |
| asm | `dump_bench_rvv` | `pcl::io::ImageYUV422::fillRGB` 符号内可见 `vlse8.v`、`vmul.vx`、`vsra.vi`、`vssseg3e8.v`。 | 不证明其它入口已向量化。 |
| board performance | `log/board/prod_rgb_full_640x480_repeat_5/summary.md` | production public strict A/B 5-run mean `2.0297x`，median `2.0247x`，min `2.0102x`，max `2.0576x`。 | 只覆盖 640x480 full-size RGB，不外推 downsample、灰度或 OpenNI legacy。 |
| padded board performance | `log/board/prod_rgb_full_padded_640x480_repeat_5/summary.md` | production public strict A/B 5-run mean `2.1030x`，median `2.1012x`，min `2.0889x`，max `2.1169x`。 | 补齐 full-size adopted path 的 padded output stride 证据。 |
| Evidence Doctor | `log/board/prod_rgb_full_640x480_repeat_5/evidence_doctor.md`、`log/board/prod_rgb_full_padded_640x480_repeat_5/evidence_doctor.md`、`log/board/prod_rgb_downsample_640x480_to_320x240_repeat_5/evidence_doctor.md` | full-size / padded production repeated 为 `Errors=0, Warnings=0, Suggestions=0`；downsample production repeated 为 `Errors=0, Warnings=1, Suggestions=0`。 | 三者均支持已采纳边界；downsample warning 需保留 min/median/max 解释。 |
| downsample board performance | `log/board/prod_rgb_downsample_640x480_to_320x240_repeat_5/summary.md` | production public strict A/B 5-run mean `3.8150x`，median `3.9294x`，min `3.3750x`，max `3.9692x`。 | 只覆盖 640x480 -> 320x240 RGB downsample 偶数 ratio gate。 |

## Production 接入判断

当前 Phase 020 和 Phase 040 production patch（生产补丁）均已采纳为 adopted production behavior（已采纳生产行为），覆盖 `fillRGB` full-size even-width RGB 和 RGB downsample 偶数 ratio gate；Phase 050 又补齐 full-size padded output 的 production-public 5-run 证据。非 RVV 构建、灰度、其它 resize ratio 和 OpenNI legacy 均不扩大。当前文件内剩余候选审计后，不建议继续扩大 production patch：灰度收益不足，其他 ratio 不适用，进一步 tuning 需要新的 profile 证明热点仍值得投入；OpenNI legacy 属于当前文件外入口，需另开或获得明确扩展授权。
