# Phase 000 计划：当前状态和诊断 scaffold

## 阶段意图和边界

本阶段为 `registration/correspondence_rejection_poly` 建立 RVV 诊断基础。目标是重建 `CorrespondenceRejectorPoly::getRemainingCorrespondences` 的标量语义，补齐 test-rvv（RVV 测试资产）中的 correctness（正确性）、bench（性能测试）和 Evidence Doctor（证据体检）入口，并生成 evaluation（函数级评估）和长期主题文档 scaffold（脚手架）。

本阶段不修改 production（生产源码）。`registration/include/pcl/registration/impl/correspondence_rejection_poly.hpp` 和 `registration/include/pcl/registration/correspondence_rejection_poly.h` 只作为源码事实读取。阶段完成后只能输出 `diagnostic`（诊断）或 `blocked` 级结论，不能输出 production-ready（生产就绪）。

## 当前标量路径

| 步骤 | 当前源码语义 | RVV 诊断边界 |
| --- | --- | --- |
| public entry（公开入口） | `applyRejection` 调用 `getRemainingCorrespondences(*input_correspondences_, correspondences)`；直接调用也可传入 `original_correspondences`。 | 诊断测试可以直接调用公开 class；production dispatch 尚不存在。 |
| 输入 guard（验收条件） | 缺 source、缺 target、`cardinality_ < 2`、`cardinality_ >= nr_correspondences`、`similarity_threshold_` 不在 `[0, 1]` 时先把输出设为输入副本并返回。 | fallback tests 必须逐项隔离这些 guard。 |
| random sampling（随机采样） | 每轮 `getUniqueRandomIndices(nr_correspondences, cardinality_)` 用 `std::rand() % n` 做无放回抽样。 | 随机状态保持标量；测试用固定 `std::srand(seed)` 固化回归。 |
| thresholdPolygon（多边形阈值） | `cardinality_ == 2` 时检查一条边；否则检查采样多边形相邻边，任一边失败则拒绝整组。 | 局部 candidate 可以批量检查预采样 edge，但不能改变早停语义和边顺序。 |
| thresholdEdgeLength（边长相似度） | 分别计算 source 和 target 中两点的 xyz squared distance（平方距离）；`edge_sim = min(dist_src, dist_tgt) / max(dist_src, dist_tgt)`；和 `similarity_threshold_squared_` 比较。 | 需要记录零长度边 `0 / 0` 产生 NaN 并拒绝的既有语义。 |
| accept rate（接受率） | 采样命中的 correspondence 记录 `num_samples`；polygon 通过时同时记录 `num_accepted`；未被采样的接受率为 0。 | 计数数组连续，适合后续诊断局部循环，但依赖随机采样顺序。 |
| computeHistogram（直方图） | `hist_size = nr_correspondences / 2`；accept rate 在 `[0,1]` 上映射到 bin；`value == 1` clamp 到最后一格。 | histogram increment 是 data-dependent scatter（数据相关写入），首阶段保留标量。 |
| findThresholdOtsu（Otsu 阈值） | 对 histogram 做类间方差最大化；空类跳过；返回 cut bin。 | 每次调用只处理 `N/2` 个 bin，首阶段保留标量并做 correctness 文档化。 |
| 输出语义 | `cut = cut_idx / hist_size`；只保留 `accept_rate[i] > cut` 的原 correspondence，顺序保持输入顺序。 | final filter 可做诊断 candidate；必须保持 `>` 而不是 `>=`。 |

## 假设与候选族

| candidate family | idea source | 假设 | 初始状态 |
| --- | --- | --- | --- |
| `edge_length_batch` | 当前源码的 xyz squared distance 热点 | 如果已有预采样边列表，RVV gather（离散加载）可批量计算边长相似度。 | planned |
| `accept_rate_filter` | 连续 `num_samples` / `num_accepted` / `accept_rate` 数组 | RVV 可批量计算接受率和最终输出 mask，但输出 push 保序仍需标量 tail（标量尾段）。 | planned |
| `histogram_otsu_scalar` | 当前 histogram / Otsu 阶段 | histogram scatter 和 Otsu 小循环首阶段保留标量，测试只保护语义。 | planned |
| `full_entry_diagnostic` | 公开 class 调用形态 | 固定随机 seed 后可以证明 test-only scaffold 与 production class 语义一致。 | planned |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision | unblocked next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `edge_length_batch` | correspondences（对应关系路径） | `PointXYZ` / `float` / xyz AoS（结构数组） | test-only edge batch helper | `run_test_compare` | `run_bench_edge_batch_smoke`、board 同名 target | planned | `dump_bench_rvv` | planned | planned | 实现诊断 helper 和测试 |
| `accept_rate_filter` | correspondences | contiguous counters（连续计数数组） | test-only accept-rate helper | `run_test_compare` | `run_bench_acceptance_smoke`、board 同名 target | planned | `dump_bench_rvv` | planned | planned | 实现诊断 helper 和测试 |
| `full_entry_diagnostic` | correspondences | `PointXYZ` / `float` | 真实 `CorrespondenceRejectorPoly` public entry | `run_test_compare` | `run_bench_full_rejection_smoke`、board 同名 target | planned | not_applicable until RVV helper used | planned | planned | 固定 seed correctness 和 bench shape |
| `histogram_otsu_scalar` | not_applicable | `float` accept rate | scalar reference only | `run_test_compare` | included in full diagnostic | not_applicable | not_applicable | manual | planned | 记录 no-production 边界 |

## 实现和测试动作

| action | 产物 | 完成判据 |
| --- | --- | --- |
| A1 | `include/correspondence_rejection_poly.h` 和 `include/impl/correspondence_rejection_poly_candidates.hpp` | 提供稳定聚合入口和 test-only reference / RVV candidate，注释说明诊断边界。 |
| A2 | `src/test_correspondence_rejection_poly.cpp` | 覆盖 guard、edge threshold、histogram、Otsu、接受率、输出顺序和固定 seed public entry。 |
| A3 | `src/bench_correspondence_rejection_poly.cpp` | 输出可解析 `Dataset:`、`Iterations:`、`Average Time`、`Total Time` 和 checksum；提供 case-filter。 |
| A4 | `Makefile`、`board.mk` | 接入公共 `rvv-topic.mk` 和 board runner，增加 QEMU smoke / board smoke / Evidence Doctor target。 |
| A5 | `script/generate_crpoly_evidence_manifest.py` | 将 bench summary 和 asm 输入转成 manifest（证据清单）；无法解析时 fail closed（失败关闭）。 |
| A6 | evaluation、topic doc、roadmap、matrix、phase result、Handoff | 回填路径、证据边界、EvidenceDecision 和下一步。 |

## Evidence Doctor 和 registry 规则

本阶段先接入 QEMU summary manifest 和 manual doctor（人工证据体检）边界。若能生成 bench summary，则执行：

```bash
make -C test-rvv/registration/correspondence_rejection_poly run_evidence_doctor_qemu
```

若环境无法运行 QEMU 或 bench，只在 result 和 Handoff 中写 `not_run`，并按 `evidence-doctor.zh.md` 人工列出 Errors / Warnings / Suggestions。`log/evidence_registry.json` 初始记录为 `not_available`，下一阶段再接入 registry record / check。

## 板卡复跑预算和决策桶

本阶段不自动跑 repeated board（重复板卡性能采集）。计划中的 board smoke 只证明目标硬件可运行和日志形状。若后续进入 repeated board，预算为 5-run 起步，最多追加 1 轮同边界确认；decision bucket（决策桶）使用 `positive / weak_positive / neutral / negative / unstable`。

## 继续 / 停止条件

继续条件：

- correctness 或 QEMU smoke 仍能在当前 topic-local 范围内补齐。
- Evidence Doctor wrapper、manifest 或 topic-local 文档仍缺低风险字段。
- 发现新的候选 family 仍限于 test-rvv 诊断资产。

停止条件：

- 需要修改 production 文件或 public API（公开接口）。
- 板卡、工具链或依赖不可用，且本地可执行证据已完成。
- Evidence Doctor Error 不能修复，或证据层之间矛盾。
- dirty isolation（脏工作区隔离）无法保证只触碰当前 topic。

## 文档更新清单

- `test-rvv/registration/correspondence_rejection_poly/doc/correspondence_rejection_poly-evaluation.zh.md`
- `doc-rvv/registration/correspondence_rejection_poly-RVV.zh.md`
- `test-rvv/registration/correspondence_rejection_poly/doc/optimization-roadmap.zh.md`
- `test-rvv/registration/correspondence_rejection_poly/doc/phases/optimization-matrix.zh.md`
- `test-rvv/registration/correspondence_rejection_poly/doc/phases/000-current-state-and-gaps/result.zh.md`
- `tmp/rvv-work-logs/registration/correspondence_rejection_poly/current-handoff/current-handoff.zh.md`

## roadmap 同步动作

本阶段结束时把 `edge_length_batch`、`accept_rate_filter`、`histogram_otsu_scalar` 和 `full_entry_diagnostic` 的状态回填到 roadmap。若 QEMU / board / asm 未运行，应记录恢复命令和阻塞条件。
