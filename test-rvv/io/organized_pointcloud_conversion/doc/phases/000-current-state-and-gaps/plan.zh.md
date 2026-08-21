# Phase 000 Plan: current-state-and-gaps

## 阶段意图和边界

本阶段只验证 `OrganizedConversion<PointXYZ>::convert` 的 cloud -> disparity
路径是否值得继续做 RVV diagnostic（RVV 诊断）。本阶段覆盖：

- 入口：`pcl::io::OrganizedConversion<pcl::PointXYZ>::convert(const PointCloud&, ...)`。
- 数据流：PointXYZ AoS（结构数组）点云读取 `x/y/z`，有限值检查后写连续 `std::uint16_t` disparity。
- 点类型 / Scalar：只覆盖 `pcl::PointXYZ` 和 `float`。
- 规模：小 literal correctness、非 VL 整除长度、板卡 bench 大 organized cloud。
- 层级：test-only diagnostic，不修改 production（生产源码）。

本阶段不覆盖 colored specialization（带颜色特化）、mono/RGB 色彩转换、disparity/depth -> cloud decode、
`PointXYZRGB/RGBA`、泛型点类型、`OrganizedPointCloudCompression::encodePointCloud` PNG/LZF 后续压缩、
production dispatch（生产分流）或 production direct（真实生产路径证据）。

## 当前状态清单

| area | 当前状态 | 路径 / 证据 |
| --- | --- | --- |
| 队列来源 | 建议队列第 1 项，状态为未启动 | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` |
| production 源码 | 只有标量模板实现，没有 `__RVV10__` 分流 | `io/include/pcl/compression/organized_pointcloud_conversion.h` |
| topic 测试资产 | 新建 topic，尚无历史证据 | `test-rvv/io/organized_pointcloud_conversion/` |
| board availability（板卡可用性） | 当前会话说明板卡可用；本机 `test-rvv/config.mk` 有 board override，但私有值不写入文档 | `test-rvv/mk/rvv-env.mk` |
| evidence registry | 本阶段先用人工 freshness 记录；registry 文件待候选和 summary 稳定后补齐 | `log/evidence_registry.json` planned |

## 假设与候选族

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `pointxyz_z_strided_disparity_rvv` | 当前源码和队列建议 | PointXYZ cloud -> disparity | AoS stride load 只取 `x/y/z`，用 RVV 批量完成 finite mask 和 `focal/(scale*z)+shift/scale` | `pcl::PointXYZ` 布局、finite mask、float->uint16 cast 和临时 buffer 成本可能抵消收益 | literal correctness、PCL scalar 对拍、QEMU、asm、board repeated、Evidence Doctor | planned | 当前 phase |
| `rgb_or_mono_color_pack_rvv` | 当前源码 | PointXYZRGB/RGBA cloud -> disparity + RGB/mono | RGB copy / grayscale 可连续写，可能与 disparity 同步处理 | color interleave、invalid color zero 和 grayscale 取整需单独对拍 | colored correctness、bench、asm、board | deferred | 后续 colored phase |
| `decode_backprojection_rvv` | 当前源码 | disparity/depth -> cloud | x/y 坐标和 depth 可批量公式化 | `push_back`、PointT 写回、invalid 0/0x7FF 和 color fallback 更复杂 | decode correctness、bench、asm、board | deferred | 后续 decode phase |
| `production_integration_probe` | phase 证据后续 | 真实 public compression entry | 若 diagnostic 强正向，可进入 PI1 | 需要 fallback、模板点类型策略和 PNG/LZF 稀释审计 | production direct tests、fallback matrix、board production bench | deferred | 证据支持后另建 phase |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `pointxyz_z_strided_disparity_rvv` | organized cloud order | `PointXYZ` / `float` / AoS stride | cloud -> disparity diagnostic | `run_test_compare` | `board_smoke` with `--case-filter pointxyz_disparity_dense,pointxyz_disparity_mixed_invalid` or `all` | planned, run budget 3 repeated-equivalent summaries if bucket unstable | `dump_bench_rvv` should show RVV instructions in diagnostic helper or inlined loop | planned via global `evidence_doctor.py` or manual summary if manifest incomplete | planned | implement candidate, run QEMU correctness, asm, board bench |
| `rgb_or_mono_color_pack_rvv` | organized cloud order | `PointXYZRGB/RGBA` / `float` / AoS + RGB bytes | colored cloud -> disparity/RGB/mono | planned later | planned later | not_started | not_started | not_started | deferred | colored phase |
| `decode_backprojection_rvv` | image scan order | `PointXYZ`, `PointXYZRGB/RGBA` / `float` | disparity/depth -> cloud | planned later | planned later | not_started | not_started | not_started | deferred | decode phase |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 | 状态 |
| --- | --- | --- | --- |
| 写 failing test（失败测试） | `src/test_organized_pointcloud_conversion.cpp`; `make run_test_rvv` | 首次运行因 planned aggregator / helper 缺失而失败，证明测试会捕获缺失候选层 | planned |
| 实现 test-only diagnostic helper | `include/organized_pointcloud_conversion.h`, `include/impl/opc_*` | `run_test_std` 和 `run_test_rvv` 通过，candidate 与 PCL scalar path 对拍 | planned |
| 建立 bench | `src/bench_organized_pointcloud_conversion.cpp` | 输出 `Dataset:`、`Iterations:`、case avg、`Total Time` 和 checksum，可被 compare 脚本解析 | planned |
| QEMU correctness | `make run_test_compare` | 只作为 correctness / log-shape 证据，不写性能结论 | planned |
| 反汇编归属 | `make dump_bench_rvv` | 记录 RVV 指令是否可归属到 diagnostic helper 或 inlined path | planned |
| 板卡 bench | `make board_smoke` | 目标硬件 std/RVV bench compare 生成 summary；若结果摇摆，按预算复跑 | planned |
| Evidence Doctor | `test-rvv/script/evidence_doctor.py` 或人工 doctor 表 | Errors / Warnings / Suggestions 被解释、降级或进入下一动作 | planned |
| 文档和队列同步 | phase result、evaluation、roadmap、matrix、queue 状态 | 结论不超过 PointXYZ diagnostic 边界；production doc 为 not_applicable | planned |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `diagnostic`，测试专用 helper 与 PCL scalar path 对拍 |
| A/B boundary | `test helper`，不是 production public overload |
| 当前决策问题 | `RVV-vs-scalar` 的首个局部诊断，不是 clean adoption |
| diagnostic 是否可外推到 production | unknown；如果收益明显，只能支持 bounded production probe（有界生产探针），还要审计 `encodePointCloud` 的 PNG/LZF 稀释、fallback 和模板点类型 |
| comparison-boundary / baseline mismatch 风险 | yes；candidate 可能使用 resize / 临时 buffer，而 production 当前使用 `push_back` |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只在实现很小、fallback 清楚且能证明 production direct 边界时允许；否则保留为 diagnostic/no-production |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采用 RVV family；若后续出现多个 candidate family，需要同一边界 A/B |

## Phase scope 与扩展队列

`validated_scope`：PointXYZ / float / organized cloud order / cloud -> disparity diagnostic。

`unvalidated_scope`：PointXYZRGB、PointXYZRGBA、mono/RGB、disparity/depth decode、generic PointT、`Scalar=double`、
production `encodePointCloud`、PNG/LZF 压缩后端、0x7FF decode invalid、indices 或其它 row source。

`point_type_expansion_queue`：

| phase | scope | resume condition | required evidence |
| --- | --- | --- | --- |
| colored-cloud-to-disparity | `PointXYZRGB/RGBA` RGB 与 mono | 当前 PointXYZ phase 有 correctness 且没有结构 blocker | colored correctness、bench、asm、board、doctor |
| decode-backprojection | disparity/depth -> cloud | encode-side证据说明继续有价值，或 queue 要求 decode 覆盖 | decode correctness、invalid 0/0x7FF、color fallback、bench、board |
| production-integration-plan | production public entry | diagnostic board bucket positive 且维护 / fallback 可控 | PI1 plan、fallback matrix、production direct tests、board production bench |

## 板卡复跑预算和决策桶

- 初始预算：`board_smoke` 1 次；如果 std/RVV 方向与 checksum 稳定，关闭本阶段板卡动作。
- 复跑预算：若 speedup 桶在 positive / weak-positive / neutral / negative 间摇摆，最多再跑 2 次。
- 决策桶：`positive >= 1.20x`，`weak-positive 1.05x-1.20x`，`neutral 0.95x-1.05x`，`negative < 0.95x`，checksum 或 doctor Error 为 blocked/invalid。
- 预算耗尽仍摇摆：标为 `unstable`，不进入 production integration。

## Structure parity 和 doc-suite 审计

| area | current shape scan | config / quality bar / optional calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| test/bench source layout | 新 topic 使用 `src/` | `artifact_layout.source_subdir=src` | adopted | `Makefile` 指向 `src/test_*` 和 `src/bench_*` | none |
| aggregator and internal helpers | planned aggregator + `include/impl` | `test_support.aggregator_directory=include`, `internal_directory=include/impl` | adopted | 本阶段将新增职责拆分 | none |
| topic-local docs | phase README、roadmap、evaluation planned | doc-suite quality bar requires navigation/evaluation/phase docs for complex topic | phase_deferred + unblocked | 首阶段先建立 evaluation/roadmap/phase；完整 README/testing docs 可在证据稳定后补齐 | doc-suite phase if current phase closes |
| production topic doc | no adopted production behavior | `doc-rvv` only after adopted / PI5 confirmation | not_applicable with evidence | 本阶段不改 production | keep not_applicable |
| evidence freshness | no prior evidence | registry planned after summary shape exists | phase_deferred + unblocked | 首次证据尚未生成 | add registry after QEMU/board summary |
| legacy compatibility | 新 topic 无 legacy pointer | compatibility alias default remove | not_applicable with evidence | 无旧路径 | none |

## 继续 / 停止条件

默认继续到 QEMU correctness、反汇编、板卡 bench 和 phase result。只有以下条件命中才停止：

- test/bench 无法在本机交叉工具链构建，且错误不是当前 topic 可修复。
- 板卡 SSH / rsync / 远端运行失败。
- Evidence Doctor Error 或 checksum 不一致无法在本阶段修复。
- 继续需要修改 production header 或 public API；这需要 S10 后进入 production integration loop。
- dirty isolation 显示当前 topic 路径与用户其它改动冲突。

`next_phase_default`：完成本阶段证据后，根据 result 进入 colored-cloud-to-disparity、decode-backprojection、
production-integration-plan 或 no-production closeout。
