# lzf_image_io 函数级评估

## S2 函数级评估

`io/include/pcl/io/impl/lzf_image_io.hpp` 承载 PCLZF image reader（PCLZF 图像读取器）的模板实现。公开入口在 `io/include/pcl/io/lzf_image_io.h` 中声明，`read()` / `readOMP()` 先通过 `loadImageBlob()` 读取压缩文件，再调用 `decompress()` 得到 uncompressed buffer（解压后的缓冲区），最后把 buffer 转为 `pcl::PointCloud<PointT>`。

本 topic 的 RVV 范围只覆盖最后一步 post-decompress conversion（解压后转换）。`pcl::lzfDecompress` 是串行压缩状态机，不在本 topic 内优化。

## 标量路径重建

| reader | 标量主循环 | 当前可 RVV 化片段 | 当前不覆盖 |
| --- | --- | --- | --- |
| `LZFDepth16ImageReader::read/readOMP` | 逐像素读取 depth16；depth 为 0 时写 NaN 并把 `cloud.is_dense=false`；否则按相机内参写 x/y/z。 | depth16 load、zero mask、z/x/y 公式和 AoS cloud 写回。 | 文件读取、decompress、camera parameter XML 解析。 |
| `LZFYUV422ImageReader::read/readOMP` | PCLZF 布局为 U plane、Y plane、V plane；每个 U/V pair 生成两个 RGB 点。 | planar U/Y/V load、整数乘加、clip 和 RGB 字段写回。 | 与 `ImageYUV422` 的 interleaved YUYV production patch 不是同一布局。 |
| `LZFBayer8ImageReader::read/readOMP` | 先调用 `DeBayer::debayerEdgeAware()` 生成 RGB buffer，再逐点写入 cloud RGB 字段。 | 首阶段只诊断 RGB buffer 到 cloud 字段写回。 | edge-aware debayer stencil 本体。 |
| `LZFRGB24ImageReader::read/readOMP` | planar RGB 到 cloud RGB 字段拷贝。 | 与 Bayer 后 RGB copy 同构，可作为后续扩展。 | 队列表首轮未把 RGB24 作为主入口。 |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `LZFDepth16ImageReader::read/readOMP` | production public entry | 读取 depth PCLZF 并生成 xyz cloud。 | `ImageGrabber` PCLZF path | `decompress`、post-decompress conversion loop | production boundary（生产边界） | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `LZFYUV422ImageReader::read/readOMP` | production public entry | 读取 YUV422 PCLZF 并生成 RGB cloud。 | `ImageGrabber` PCLZF path | post-decompress planar YUV conversion | production boundary | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `LZFBayer8ImageReader::read/readOMP` | production public entry | 读取 Bayer PCLZF，经 debayer 后生成 RGB cloud。 | `ImageGrabber` PCLZF path | `DeBayer::debayerEdgeAware`、RGB copy loop | production boundary | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| `convert*Scalar` / `convert*Candidate` | diagnostic reference / candidate | 复刻解压后转换语义，供 gtest 和 bench 对拍。 | `test_lzf_image_io.cpp`、`bench_lzf_image_io.cpp` | 无 production 下游 | production-shaped diagnostic correctness / bench | `test-rvv/io/lzf_image_io/include/lzf_image_io.h` |
| `convertPlanarYuv422ToPointCloudStd/RVV` | production detail helper | `LZFYUV422ImageReader::read/readOMP` 的 YUV planar 转换 helper。 | `read/readOMP`、production bench case | cloud RGB 字段写回 | production-detail correctness / bench | `io/include/pcl/io/impl/lzf_image_io.hpp` |
| phase 000 plan | phase plan | 冻结首阶段范围、矩阵、板卡预算和继续 / 停止条件。 | worker / reviewer | result、roadmap、Handoff | recovery pointer（恢复入口） | `test-rvv/io/lzf_image_io/doc/phases/000-current-state-and-diagnostic-scaffold/plan.zh.md` |
| phase 000 result | phase result | 记录诊断候选的 correctness、QEMU、asm、board、Doctor 和 EvidenceDecision。 | worker / reviewer | roadmap、optimization matrix、PI1 plan | closeout and recovery pointer | `test-rvv/io/lzf_image_io/doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md` |
| diagnostic board repeated summary | evidence output summary | 保存 phase 000 Milkv-Jupiter 5-run 统计和 case speedup。 | `make run_board_lzf_repeated` | evaluation、phase result、Evidence Doctor | board performance summary | `test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/summary.md` |
| production board repeated summary | evidence output summary | 保存 phase 020 YUV production helper 5-run 统计。 | `make run_board_lzf_yuv422_production_repeated` | PI5 result、Evidence Doctor、Handoff | production-detail board performance | `test-rvv/io/lzf_image_io/log/board/production_yuv422_repeat_5/summary.md` |
| Evidence Doctor report | evidence validator output | 暴露 Error / Warning / Suggestion，约束 EvidenceDecision。 | `make run_board_lzf_repeated` | evaluation、phase result、Handoff | evidence validation | `test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/evidence_doctor.md` |
| PI1 plan / result | production integration plan | 冻结 YUV planar 的生产接入范围、fallback gate 和 PI5 停止条件；记录 PI2 生产源码授权停点。 | phase 000 result | 用户授权后的 PI2 | production authorization checkpoint | `test-rvv/io/lzf_image_io/doc/phases/010-production-integration-plan/plan.zh.md`、`test-rvv/io/lzf_image_io/doc/phases/010-production-integration-plan/result.zh.md` |
| topic README | topic navigation | 提供读者入口、常用命令和证据白名单。 | worker / reviewer | topic-local docs | documentation navigation | `test-rvv/io/lzf_image_io/README.zh.md` |
| testing / evidence docs | doc suite | 拆分测试总览、gtest 字典、bench 证据、优化证据和代码地图。 | worker / reviewer | Handoff、phase result | documentation recovery | `test-rvv/io/lzf_image_io/doc/*.zh.md` |

## 生产接入初判

当前判断是 `adopted weak-positive production behavior`。Phase 020 已完成 `LZFYUV422ImageReader::read/readOMP` 的有界生产接入、production helper direct correctness、fallback gate、QEMU smoke、反汇编归属、板卡 repeated summary 和 Evidence Doctor。生产板卡收益为 mean `1.0864x`、median `1.0832x`，低于诊断阶段但稳定正向；用户已确认“有收益即可采纳”，因此保留当前 production patch。

`depth_xyz_rvv` 只有弱正向并接近阈值，`rgb_buffer_to_cloud_rvv` 为负向且触发 Doctor Error。二者当前都不进入 production。正式 `doc-rvv/io/lzf_image_io-RVV.zh.md` 已创建，使用 phase 020 的 production-detail board 数据。

## 诊断证据链

本轮使用 `test-rvv/io/lzf_image_io` 的 production-shaped diagnostic（生产形态诊断）资产，直接喂解压后的 depth/YUV/RGB buffer，不覆盖文件读取、PCLZF header、`decompress()` 或 ImageGrabber 调度。QEMU 只证明 correctness（正确性）和日志形状；真实性能结论只来自 Milkv-Jupiter 板卡。

关键命令和结果：

| evidence | command / path | result |
| --- | --- | --- |
| correctness | `make run_test_compare` | Phase 000 时 Std/RVV 各 3 个 gtest 通过；phase 020 后当前为 Std/RVV 各 5 个 gtest 通过。 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | 通过；不作为性能证据。 |
| asm | `make dump_bench_rvv` -> `build/asm/riscv/bench_lzf_image_io_rvv.asm` | YUV path 可见 `vlse8.v`、`vsra.vi`、`vsse8.v`；depth 和 RGB copy 也能归属到对应 RVV 指令。 |
| board repeated | `log/board/production_shaped_repeat_5/summary.md` | YUV mean 1.1505x / median 1.1490x；depth median 1.0417x；RGB copy median 0.9894x。 |
| Evidence Doctor | `log/board/production_shaped_repeat_5/evidence_doctor.md` | `Errors=1, Warnings=1, Suggestions=1`；Error 指向 RGB copy 5/5 退化，Suggestion 指向 depth near-threshold。 |
| freshness | `make check_evidence_freshness` | registry fresh。 |

## 生产证据链

Phase 020 在真实 production helper 边界重跑证据。QEMU 仍只作为 correctness（正确性）和日志形状证据；
性能结论只来自 Milkv-Jupiter 板卡。

| evidence | command / path | result |
| --- | --- | --- |
| TDD red | `make run_test_compare` | 新增 production helper direct gtest 后，因 `pcl::io::detail` helper 不存在而编译失败。 |
| correctness | `make run_test_compare` | Std/RVV 各 5 个 gtest 通过。 |
| board correctness | `make run_board_test fetch_board_logs` | 板卡 RVV gtest 5/5 通过。 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter yuv422_planar_rgb_production_640x480 --iterations 2 --warmup-iterations 1"` | 生产 case 可运行；不作为性能结论。 |
| asm | `make dump_bench_rvv` -> `build/asm/riscv/bench_lzf_image_io_rvv.asm` | 可见 `vlse8.v`、`vsra.vi`、`vsse8.v`。 |
| board repeated | `log/board/production_yuv422_repeat_5/summary.md` | mean `1.0864x`、median `1.0832x`、min `1.0732x`、max `1.1135x`。 |
| Evidence Doctor | `log/board/production_yuv422_repeat_5/evidence_doctor.md` | `Errors=0, Warnings=0, Suggestions=0`。 |
| freshness | `make check_production_evidence_freshness` | registry fresh。 |

EvidenceDecision：

| candidate family | decision | production 接入判断 |
| --- | --- | --- |
| `yuv422_planar_rgb_rvv` | `adopted weak-positive production behavior` | 生产证据支持保留；用户已确认有收益即可采纳。 |
| `depth_xyz_rvv` | `deferred-diagnostic` | 暂缓，不用弱诊断收益外推 production。 |
| `rgb_buffer_to_cloud_rvv` | `rejected-diagnostic-candidate` | 当前 RVV copy 候选拒绝，不进入 production。 |

## 文档归属

本 topic 当前已有用户确认采纳的 adopted production behavior（已采纳生产行为），正式文档为 `doc-rvv/io/lzf_image_io-RVV.zh.md`。诊断结论主归属是本 evaluation、phase 000 result、optimization roadmap 和 optimization matrix；生产 closeout 主归属是正式 `doc-rvv`、phase 020 result、production board summary 和 production Evidence Doctor；测试 / bench / 代码地图主归属是 `README.zh.md` 引导的 topic-local doc suite。

下一默认恢复入口为 `doc-rvv/io/lzf_image_io-RVV.zh.md` 和 `test-rvv/io/lzf_image_io/doc/phases/020-yuv-planar-production-probe/result.zh.md`。当前不建议继续自动扩大 production patch；depth、RGB copy、Bayer debayer 和泛型 RGB traits 扩展只有在新的 profile 或用户明确要求时再开后续 phase。
