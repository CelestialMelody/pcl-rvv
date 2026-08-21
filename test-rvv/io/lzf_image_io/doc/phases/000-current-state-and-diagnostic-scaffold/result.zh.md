# Phase 000 结果：当前状态与诊断脚手架

## 阶段结论

本阶段完成 `io/include/pcl/io/impl/lzf_image_io.hpp` 的 post-decompress conversion（解压后转换）诊断闭环。当前没有修改 production（生产源码）。诊断证据显示：

- `yuv422_planar_rgb_rvv`：板卡 repeated summary 稳定正向，作为 `partial-production-candidate` 进入下一阶段 PI1 生产接入计划。
- `depth_xyz_rvv`：板卡结果为弱正向且接近阈值，当前只保留为诊断线索，暂缓 production probe（生产探针）。
- `rgb_buffer_to_cloud_rvv`：板卡 5/5 退化，Evidence Doctor（证据体检）报 Error，当前候选拒绝，不进入 production。

`pcl::lzfDecompress`、真实文件读取、PCLZF header 解析、Bayer edge-aware debayer 本体、RGB24 reader 均不属于本阶段已证明范围。

## 动作回填

| action | 产物 | 结果 |
| --- | --- | --- |
| 建立 topic scaffold | `Makefile`、`board.mk`、`include/lzf_image_io.h`、`include/impl/lzf_image_io_support.hpp`、`src/test_lzf_image_io.cpp`、`src/bench_lzf_image_io.cpp` | 已完成。TDD red 先以缺少聚合头失败，随后补齐测试支撑并进入 green。 |
| 写标量 reference | `convertDepthToCloudScalar`、`convertPlanarYuv422ToRgbScalar`、`copyRgbBufferToCloudScalar` | 已完成，复刻当前源码的 NaN / `is_dense`、planar YUV422 公式和 RGB buffer 写回语义。 |
| 写 RVV candidate | `convertDepthToCloudRVV`、`convertPlanarYuv422ToRgbRVV`、`copyRgbBufferToCloudRVV` | 已完成，`__RVV10__` 下启用；非 RVV build 回到同一份标量 reference。 |
| QEMU correctness | `make run_test_compare` | 通过，Std/RVV 各 3 个 gtest 通过。QEMU 只作为正确性和日志形状证据。 |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"` | 通过，仅用于可运行性 smoke，不进入性能结论。 |
| 反汇编归属 | `make dump_bench_rvv`，输出 `build/asm/riscv/bench_lzf_image_io_rvv.asm` | 通过，可见 depth 的 `vle16.v` / `vzext.vf2` / `vfmul` / `vsse32.v`，YUV 的 `vlse8.v` / `vsra.vi` / `vsse8.v`，RGB copy 的 `vlseg3e8.v` / `vsse8.v`。 |
| 板卡 repeated summary | `make run_board_lzf_repeated` | 通过，Milkv-Jupiter 5-run，summary 路径见下表。 |
| Evidence Doctor 和 registry | `log/board/production_shaped_repeat_5/evidence_doctor.md`、`log/evidence_registry.json` | 已生成并登记；`make check_evidence_freshness` 通过。 |
| 板卡 correctness smoke | `make run_board_test fetch_board_logs` | 通过，board gtest 3/3。 |

## 当前证据路径

| evidence | path | role |
| --- | --- | --- |
| QEMU correctness logs | `test-rvv/io/lzf_image_io/log/qemu/run_test_std.log`、`test-rvv/io/lzf_image_io/log/qemu/run_test_rvv.log` | correctness / fallback 形状 |
| QEMU bench smoke | `test-rvv/io/lzf_image_io/log/qemu/run_bench_rvv.log` | qemu_smoke_only，不作为性能证据 |
| 反汇编 | `test-rvv/io/lzf_image_io/build/asm/riscv/bench_lzf_image_io_rvv.asm` | asm attribution（反汇编归属） |
| 板卡 repeated summary | `test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/summary.md` | production-shaped diagnostic board performance |
| Evidence Doctor | `test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/evidence_doctor.md` | EvidenceDecision 前异常检查 |
| Evidence manifest | `test-rvv/io/lzf_image_io/log/board/production_shaped_repeat_5/evidence_manifest.json` | 机器可读证据清单 |
| Evidence registry | `test-rvv/io/lzf_image_io/log/evidence_registry.json` | freshness / doc reference 状态 |

## 板卡结果和 Evidence Doctor

Milkv-Jupiter 5-run，`iterations=20`，`warmup_iterations=3`：

| candidate family | case | mean speedup | median | min | max | bucket | decision |
| --- | --- | ---: | ---: | ---: | ---: | --- | --- |
| `depth_xyz_rvv` | `depth_xyz_640x480` | 1.0389x | 1.0417x | 1.0096x | 1.0566x | `weak-positive / near-threshold` | 暂缓，仅保留诊断线索 |
| `yuv422_planar_rgb_rvv` | `yuv422_planar_rgb_640x480` | 1.1505x | 1.1490x | 1.1310x | 1.1652x | `positive` | 进入 PI1 生产接入计划 |
| `rgb_buffer_to_cloud_rvv` | `rgb_buffer_to_cloud_640x480` | 0.9902x | 0.9894x | 0.9829x | 0.9981x | `negative` | 拒绝当前 RVV copy 候选 |

Evidence Doctor 结果为 `Errors=1, Warnings=1, Suggestions=1`。Error 和 Warning 均指向 `rgb_buffer_to_cloud_640x480` 的退化频率和组内离群；因此 RGB copy 不能作为 production candidate。Suggestion 指向 `depth_xyz_640x480` 接近阈值；因此 depth 当前不应只凭弱收益接入 production。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`。测试直接喂解压后 buffer，不走真实 `loadImageBlob()`、PCLZF header 和 `decompress()`。 |
| A/B boundary | `test helper`：Std build 使用标量 reference，RVV build 使用测试专用 candidate。 |
| 计时边界 | 只计 post-decompress conversion 和 checksum；不计文件读取、解压、cloud resize、reader 状态和上游 ImageGrabber 调度。 |
| 当前决策问题 | 是否存在值得进入 production integration loop 的 candidate family。 |
| diagnostic 是否可外推到 production | 只能作为升级信号，不能替代 production direct 证据。YUV planar 的转换循环与 production 源码局部匹配度最高，因此允许有界生产计划；depth 和 RGB copy 不外推。 |
| comparison-boundary / baseline mismatch 风险 | 有。诊断 helper 使用测试 POD `PointXYZRGB`，production 入口是模板 `PointT`；真实 reader 还包含文件读取、解压和 resize。YUV PCLZF 是 planar U/Y/V，不等同于 `ImageYUV422` 的 interleaved YUYV。 |
| 弱 / 负 / 中性 / 不稳定时 bounded production probe 条件 | 只有实现范围小、fallback 明确、生产直连测试可写、板卡可复跑且 Evidence Doctor 无阻塞错误时才允许。当前只有 YUV planar 满足进入 PI1 的条件。 |
| clean adoption 是否需要 production boundary 证据 | 需要。PI 后必须补 production direct correctness、asm attribution、board repeated summary 和 Evidence Doctor；PI5 仍需用户确认是否采纳或回滚。 |

## 文档套件和结构审计

| area | current shape scan | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- |
| README navigation | 已补 `README.zh.md` | `adopted` | 入口导航、常用命令和证据白名单已独立承载 | 后续 PI 更新当前结论即可 |
| testing overview / correctness tests / benchmark evidence | 已补 `doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md` | `adopted` | target 粒度、gtest 字典、case-filter、summary / Doctor / registry 路径已可恢复 | PI 阶段补 production direct target 后同步 |
| test support code map | 已补 `doc/test-support-code-map.zh.md` | `adopted` | helper 348 行，低于 hard limit；职责地图说明当前合并理由 | 若后续扩展 depth/Bayer，再按职责拆分 |
| optimization roadmap / matrix | 已存在 | `adopted` | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md` | 本阶段同步刷新 |
| production topic doc | 不适用 | `not_applicable with evidence` | 尚无 adopted production behavior，也无 PI5 用户确认采纳 | PI5 通过并确认后才创建 `doc-rvv/io/lzf_image_io-RVV.zh.md` |
| artifact tracking | 新增 topic-local 文档和测试资产均在 `test-rvv/io/lzf_image_io` 下 | `partial` | 当前未提交；raw logs 默认 local-only，summary/manifest/doctor 被文档引用 | 提交前用路径限定扫描和 evidence policy 再确认 |

## doc_suite_role_inventory

| role | status |
| --- | --- |
| topic_navigation | `standalone:test-rvv/io/lzf_image_io/README.zh.md` |
| testing_overview | `standalone:test-rvv/io/lzf_image_io/doc/testing-overview.zh.md` |
| correctness_tests | `standalone:test-rvv/io/lzf_image_io/doc/correctness-tests.zh.md` |
| benchmark_and_evidence | `standalone:test-rvv/io/lzf_image_io/doc/benchmark-and-evidence.zh.md` |
| optimization_evidence | `standalone:test-rvv/io/lzf_image_io/doc/optimization-evidence.zh.md` |
| optimization_roadmap | `standalone:test-rvv/io/lzf_image_io/doc/optimization-roadmap.zh.md` |
| test_support_code_map | `standalone:test-rvv/io/lzf_image_io/doc/test-support-code-map.zh.md` |
| phase_index / phase_plan / phase_result / optimization_matrix | `standalone:test-rvv/io/lzf_image_io/doc/phases/*` |
| evaluation_diagnostic | `standalone:test-rvv/io/lzf_image_io/doc/lzf_image_io-evaluation.zh.md` |
| evaluation_production | `not_applicable with evidence`，尚未进入 PI2 production patch |
| production_topic_doc | `not_applicable with evidence`，尚无 adopted production behavior |

## EvidenceDecision

| candidate family | EvidenceDecision | 原因 |
| --- | --- | --- |
| `yuv422_planar_rgb_rvv` | `partial-production-candidate` | correctness / QEMU smoke / asm / Milkv-Jupiter 5-run 均闭合，median 1.1490x，且未触发阻塞 Doctor finding。仍需 production direct 证据。 |
| `depth_xyz_rvv` | `deferred-diagnostic` | median 1.0417x，低于本阶段 positive 阈值且触发 near-threshold suggestion；invalid depth 还包含标量 lane 修正和 `is_dense` 语义。 |
| `rgb_buffer_to_cloud_rvv` | `rejected-diagnostic-candidate` | median 0.9894x，5/5 低于 1，Evidence Doctor Error 阻止其作为 production candidate。 |

## 下一阶段

默认下一阶段为 `010-production-integration-plan`，只评估 `LZFYUV422ImageReader::read/readOMP` 的 planar U/Y/V 到 `PointT::r/g/b` 写回。PI1 只写生产接入计划、production direct test/bench 需求、fallback gate 和用户检查点；生产源码改动需显式授权后再进入 PI2。

当前合法停止条件：production source modification（生产源码修改）需要用户确认。板卡可用，不是阻塞项。
