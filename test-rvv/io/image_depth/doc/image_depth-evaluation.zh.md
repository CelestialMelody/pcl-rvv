# image_depth 函数级评估

## 范围和目标源码

目标源码是 `io/src/image_depth.cpp`，首轮评估 `pcl::io::DepthImage::fillDepthImage()` 和 `fillDisparityImage()`。二者从 `FrameWrapper`（帧包装器，提供 depth buffer 和尺寸元数据）读取 16-bit depth 像素，按目标尺寸写入外部 float buffer。

## 函数级结论

当前结论是 `adopted_production_behavior`：用户已确认接入有收益的 production patch（生产补丁）。`io/src/image_depth.cpp` 的真实 `DepthImage` public entry（公开入口）已经完成 Std/RVV correctness（正确性）、production-public board repeated（真实公开入口板卡重复测试）、反汇编归属和 Evidence Doctor（证据体检）。采用范围为 depth contiguous、disparity contiguous 和 disparity downsample；depth downsample 保持标量 fallback。

## 函数族评估表

| 函数 | 标量路径 | RVV 判断 | 首轮边界 |
| --- | --- | --- | --- |
| `fillDepthImageRaw` | invalid 像素写 `short quiet_NaN` 的 bit pattern，其它像素复制；全尺寸 tight row 有 memcpy 快路径 | 暂缓 | 首轮只记录，不做候选 |
| `fillDepthImage` | invalid 像素写 float NaN，其它像素写 `pixel * 0.001f`；支持整数倍下采样和 line padding | adopted for contiguous；downsample fallback | phase 050 production-public：contiguous median 1.42x；padded median 1.20x；downsample 已按生产证据回退标量，context median 0.99x。 |
| `fillDisparityImage` | invalid 像素写 0，其它像素写 `focal_length * baseline * 1000 / xStep / pixel` | adopted for contiguous and downsample | phase 050 production-public：contiguous median 1.80x；downsample median 1.34x；两者有 variance warning。 |

## 标量流程与 RVV 流程对照

标量流程先检查 upsample 和非整数 downsample，随后把 `line_step == 0` 归一成 tight row。每一行用 `xStep` 从源 depth buffer 取样，用 `ySkip` 跳到下一行，写完目标行后按 `bufferSkip` 跳过 padding。

production patch 把原标量循环抽成邻近 Std helper，并在 `__RVV10__` 下对 `xStep == 1` 使用连续加载，对 `fillDisparityImage()` 的整数倍 downsample 且 `xStep > 1` 使用 `vlse16` 跨步加载。`fillDepthImage()` 的 downsample 在 production-public 复测后保持标量 fallback。非整数 downsample、upsample、非 float 对齐 `line_step` 和未覆盖入口仍走同一标量 helper，避免把当前证据外推到未验证范围。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `DepthImage::fillDepthImage` | production public entry | depth mm 到 meter float 输出 | OpenNI2 grabber、通用 depth image 使用者 | 外部调用方提供的 output buffer | production boundary | `io/src/image_depth.cpp` |
| `DepthImage::fillDisparityImage` | production public entry | depth mm 到 disparity float 输出 | depth/disparity image 使用者 | 外部 output buffer | production boundary | `io/src/image_depth.cpp` |
| production RVV helpers | production detail | contiguous depth/disparity 与 downsample disparity RVV 转换；depth downsample 标量 fallback | `DepthImage::fillDepthImage`、`fillDisparityImage` | output buffer | production-public candidate / fallback coverage | `io/src/image_depth.cpp` |
| `fillDepthMetersCandidate` | production-shaped diagnostic helper | 复刻 `fillDepthImage` 内层转换 | `src/test_image_depth.cpp`、`src/bench_image_depth.cpp` | output buffer | correctness / bench diagnostic | `test-rvv/io/image_depth/include/image_depth.h` |
| `fillDisparityCandidate` | production-shaped diagnostic helper | 复刻 `fillDisparityImage` 内层转换 | `src/test_image_depth.cpp`、`src/bench_image_depth.cpp` | output buffer | correctness / bench diagnostic | `test-rvv/io/image_depth/include/image_depth.h` |
| `run_board_depth_full` | board bench target | 板卡上运行 depth contiguous A/B | topic Makefile | board summary | board performance diagnostic | `test-rvv/io/image_depth/Makefile` |
| repeated board summary | evidence summary | 汇总 5 次板卡 Std/RVV speedup | board compare logs | Evidence Doctor、phase result | board performance diagnostic | `test-rvv/io/image_depth/log/board/repeated_contiguous/summary.md` |
| Evidence Doctor report | evidence validation | 检查 repeated summary 的异常信号和证据边界 | topic manifest | phase result、Handoff | evidence validation | `test-rvv/io/image_depth/log/board/repeated_contiguous/evidence_doctor.md` |
| downsample repeated summary | evidence summary | 汇总 5 次 downsample 板卡 Std/RVV speedup | board compare logs | Evidence Doctor、phase 020 result | board performance diagnostic | `test-rvv/io/image_depth/log/board/repeated_downsample/summary.md` |
| downsample Evidence Doctor report | evidence validation | 检查 downsample repeated summary 的异常信号和证据边界 | topic manifest | phase 020 result、Handoff | evidence validation | `test-rvv/io/image_depth/log/board/repeated_downsample/evidence_doctor.md` |
| production-public repeated summary | evidence summary | 汇总真实 `DepthImage` 入口 5 次板卡 Std/RVV speedup | board compare logs | Evidence Doctor、phase 050 result | board production-public | `test-rvv/io/image_depth/log/board/repeated_production_public/summary.md` |
| production-public Evidence Doctor report | evidence validation | 检查 production-public summary 的异常信号和证据边界 | topic manifest | phase 050 result、Handoff | evidence validation | `test-rvv/io/image_depth/log/board/repeated_production_public/evidence_doctor.md` |
| evidence registry | evidence freshness | 登记 summary / manifest / doctor 的 digest（摘要指纹）并检查文档引用 | `make record_board_repeated_evidence_state` | worker 恢复、提交前检查 | freshness guard | `test-rvv/io/image_depth/log/evidence_registry.json` |
| repeated summary generator | topic-local script | 从 5 个 repeated compare log 生成可复现 summary | `make generate_board_repeated_summary` | manifest wrapper、phase result | summary producer | `test-rvv/io/image_depth/script/generate_image_depth_repeated_summary.py` |
| PI1 plan | production planning | 冻结生产接入候选范围和暂停条件 | phase 000 result | 下一轮 production integration loop | production integration gate | `test-rvv/io/image_depth/doc/phases/010-production-integration-plan/plan.zh.md` |
| topic README | topic navigation | 给下一轮 worker / reviewer 的阅读入口和证据白名单 | phase suite | topic-local docs | doc-suite navigation | `test-rvv/io/image_depth/README.zh.md` |
| testing overview | test documentation | 说明 target 粒度、覆盖矩阵和证据边界 | Makefile、test、bench、script | reviewer / Handoff | doc-suite role | `test-rvv/io/image_depth/doc/testing-overview.zh.md` |
| benchmark and evidence doc | evidence documentation | 说明 bench label、summary、doctor、registry 和提交边界 | summary / manifest / doctor | reviewer / Handoff | doc-suite role | `test-rvv/io/image_depth/doc/benchmark-and-evidence.zh.md` |
| test-support code map | code map | 定位 test-only helper、bench、script 和 output | source / script paths | reviewer / Handoff | doc-suite role | `test-rvv/io/image_depth/doc/test-support-code-map.zh.md` |

## 测试计划和 bench 计划

| 测试 / target | 层级 | 作用 |
| --- | --- | --- |
| `run_test_compare` | correctness（正确性） | Std / RVV build 都要通过 scalar-vs-candidate 对拍 |
| `run_bench_rvv` | QEMU smoke（QEMU 小型验证） | 只检查 RVV build 可运行和日志形状，不产生性能结论 |
| `dump_bench_rvv` | asm attribution（反汇编归属） | 确认候选 helper 中出现 RVV 指令 |
| `run_board_depth_full` | board performance（板卡性能） | contiguous depth meters case 的板卡性能诊断 |
| `run_board_disparity_full` | board performance（板卡性能） | contiguous disparity case 的板卡性能诊断 |
| `collect_board_downsample_repeated` | board performance（板卡性能） | 采集 downsample depth / disparity 的 5-run production-shaped diagnostic |
| `run_board_downsample_evidence_doctor` | Evidence Doctor（证据体检） | 生成 downsample summary / manifest 并检查异常信号 |
| `collect_board_production_repeated` | board performance（板卡性能） | 采集 production-public `prod_*` case 的 5-run Std/RVV 对比 |
| `run_board_production_evidence_doctor` | Evidence Doctor（证据体检） | 生成 production-public summary / manifest 并检查异常信号 |

## 当前诊断证据链

| 证据 | 结果 | 边界 |
| --- | --- | --- |
| QEMU correctness | `make run_test_compare` | Std/RVV 各 9 个 TEST 通过；RVV build 覆盖 4 个 production-public path hit。 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` 可运行 | 只证明日志形状和 RVV build 可运行，不提供性能结论。 |
| asm | `make dump_bench_rvv` 生成 `build/asm/riscv/bench_image_depth_rvv.asm` | 可见 `vle16`、`vfcvt`、`vfmul`、`vfrdiv`、mask 和 masked `vse32`。 |
| board repeated | `log/board/repeated_contiguous/summary.md` | `depth_full_640x480` median 1.38x，`depth_full_padded_640x480` median 1.21x，`disparity_full_640x480` median 1.93x。 |
| Evidence Doctor | `log/board/repeated_contiguous/evidence_doctor.md` | Errors=0，Warnings=1，Suggestions=0；warning 是 disparity 长尾，后续 production direct 需保留解释。 |
| downsample board repeated | `log/board/repeated_downsample/summary.md` | `depth_downsample_640x480_to_320x240` median 1.17x，`disparity_downsample_640x480_to_320x240` median 1.38x。 |
| downsample Evidence Doctor | `log/board/repeated_downsample/evidence_doctor.md` | Errors=0，Warnings=2，Suggestions=0；两个 warning 均为长尾，当前只支持 production-shaped diagnostic 正向。 |
| production-public board repeated | `log/board/repeated_production_public/summary.md` | `prod_depth_full_640x480` median 1.42x；`prod_depth_full_padded_640x480` median 1.20x；`prod_depth_downsample_640x480_to_320x240` fallback context median 0.99x；`prod_disparity_full_640x480` median 1.80x；`prod_disparity_downsample_640x480_to_320x240` median 1.34x。 |
| production-public Evidence Doctor | `log/board/repeated_production_public/evidence_doctor.md` | Errors=0，Warnings=2，Suggestions=0；两个 warning 都来自 disparity 长尾。depth downsample 在 manifest 中是 `production-fallback-coverage`，不作为 RVV 收益候选。 |
| evidence registry | `make evidence_status` | 当前 repeated summary、manifest 和 doctor 的 digest 已登记，registry check 为 fresh。 |

## 生产接入判断

用户已确认采纳当前缩窄 patch：contiguous depth、contiguous disparity 和 disparity downsample 作为 adopted production behavior；depth downsample 不作为当前生产 RVV 路径继续接入，保持标量 fallback。raw path 因 full-size tight row 常见入口已有 `memcpy` 快路径且缺少 profile 证据，保持 profile-gated deferred；OpenNI legacy 入口涉及另一个 production file，建议后续作为独立 parity topic 复核。

## 文档归属

production 长期主题文档是 `doc-rvv/io/image_depth-RVV.zh.md`，只记录 adopted production behavior、dispatch / fallback、production-public evidence 和长期维护风险。topic-local README、testing overview、correctness tests、benchmark/evidence、optimization evidence、test-support code map、phase suite、roadmap 和本 evaluation 继续承担候选取舍、测试细节、bench 字典和恢复状态说明。
