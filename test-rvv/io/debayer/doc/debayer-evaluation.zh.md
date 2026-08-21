# debayer function evaluation

## 函数级评估

`io/src/debayer.cpp` 提供 `pcl::io::DeBayer` 的三个公开入口：`debayerBilinear`、`debayerEdgeAware` 和 `debayerEdgeAwareWeighted`。调用方包括 OpenNI Bayer GRBG image wrapper 和 PCLZF Bayer reader。输入是 Bayer pattern（拜耳阵列）字节图像，输出是 RGB24 byte buffer（每像素 3 字节）。

标量路径把边界行列手写成特殊 case，中间主体按两列、两行推进。`debayerBilinear` 的内区主体只做固定邻域平均：水平 / 垂直 `AVG`，对角 / 十字 `AVG4`，并写回 interleaved RGB（交错 RGB）。这段是当前最清楚的 RVV 候选。当前 production 指针推进更接近 contiguous Bayer（连续 Bayer 输入）假设；Bayer 输入 padding 单独列入未验证范围。`debayerEdgeAware` 和 weighted 分支在同一主体结构上增加 `abs` 梯度比较和加权平均，本阶段暂缓。

## 当前生产接入判断

当前阶段是 production-shaped diagnostic（生产形态诊断），不修改 production（生产源码）。`debayerBilinear` 内区的两种测试专用 RVV 形态已经验证正确，但板卡上稳定慢于标量：strided-store 约 0.94x / 0.95x，segmented-store 约 0.93x。因此当前结论是不进入 production integration plan（生产接入计划），也不修改 `io/src/debayer.cpp`。

## 诊断证据链

| evidence role | 当前状态 | 能证明 | 不能证明 |
| --- | --- | --- | --- |
| correctness | pass | 测试专用内区 reference / candidate 与 production `debayerBilinear` 内区一致 | 真实 production dispatch 已接入 |
| QEMU path | pass | RVV build 能运行、路径命中 gate 生效 | 性能 |
| asm | pass | bench binary 中出现候选相关 RVV 指令 | 指令来自 production |
| board performance | negative | 目标硬件上当前 RVV candidate 慢于标量；不支持 production probe | 所有未来 debayer 实现族都没有价值 |
| Evidence Doctor | warning | checksum 一致；negative speedup；metadata incomplete | 替代 production direct 决策 |

## 文档归属

长期 `doc-rvv/io/debayer-RVV.zh.md` 当前不适用，因为还没有 adopted production behavior（已采用生产行为）或 PI5 生产证据闭环。诊断阶段事实保留在本 evaluation、phase plan/result、optimization matrix 和 Handoff Packet。

| 信息类型 | 主归属 | 当前路径 |
| --- | --- | --- |
| 函数入口、标量路径、no-production 诊断证据链 | evaluation | `test-rvv/io/debayer/doc/debayer-evaluation.zh.md` |
| 阶段计划、负向尝试、继续 / 停止条件 | phase plan/result | `test-rvv/io/debayer/doc/phases/000-current-state-and-bilinear-interior/` |
| 候选搜索空间和恢复条件 | roadmap | `test-rvv/io/debayer/doc/optimization-roadmap.zh.md` |
| 候选矩阵和证据状态 | optimization matrix | `test-rvv/io/debayer/doc/phases/optimization-matrix.zh.md` |
| Evidence Doctor（证据体检）摘要 | topic-local evidence summary | `test-rvv/io/debayer/log/board/evidence_doctor.md` |
| 模块队列状态 | screening queue | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `pcl::io::DeBayer::debayerBilinear` | production public entry | 真实 Bayer 到 RGB 双线性转换入口 | OpenNI Bayer wrapper、PCLZF Bayer reader | production 内区和边界手写循环 | production boundary（生产边界），本阶段只读取不修改 | `io/src/debayer.cpp` |
| `debayerBilinearInteriorScalar` | diagnostic reference | 复刻 production 内区公式，供 candidate 对拍 | `src/test_debayer.cpp`、`src/bench_debayer.cpp` | correctness assertion、Std bench | correctness gate（正确性验收） | `test-rvv/io/debayer/include/debayer.h` |
| `debayerBilinearInteriorCandidate` | candidate wrapper | 在 `__RVV10__` 下尝试 RVV，否则 fallback（回退路径）到标量 reference | gtest、bench | RVV 内区实现或标量 fallback | path gate（路径命中验收）和 diagnostic candidate | `test-rvv/io/debayer/include/debayer.h` |
| `storeRgbBlock6` / `debayerBilinearInteriorRVV` | candidate formula / store | 用 RVV load、average 和 segmented store（分段写回）实现内区候选 | candidate wrapper | bench binary 内联热区 | asm attribution（反汇编归属）和 board A/B | `test-rvv/io/debayer/include/debayer.h` |
| `test_debayer.cpp` | correctness test | 验证 production 内区对拍和 RVV path gate | `make run_test_compare` | QEMU run logs | QEMU correctness（QEMU 正确性） | `test-rvv/io/debayer/src/test_debayer.cpp` |
| `bench_debayer.cpp` | bench wrapper | 生成 synthetic Bayer 输入并输出 checksum / timing | `make run_bench_rvv`、board target | analyze script、Evidence Doctor | board performance（板卡性能）输入 | `test-rvv/io/debayer/src/bench_debayer.cpp` |
| `evidence_doctor.md` | evidence output summary | 记录 checksum、negative speedup 和 metadata warning | phase result | evaluation / Handoff | Evidence Doctor 摘要 | `test-rvv/io/debayer/log/board/evidence_doctor.md` |

## 当前 no-production 结论

当前不建议把 `debayerBilinear` 内区 RVV candidate 接入 production。这个 no-production（不接入生产）结论只覆盖 phase 000 的同边界测试专用 candidate family；`debayerEdgeAware` / weighted 分支若要继续，需要先有 profile（性能剖析）或新候选证明它们的额外分支 / 加权计算能改变收益结构。

## 默认恢复动作

默认恢复动作是保持 `debayerBilinear` bilinear inner family（双线性内区实现族）的 no-production 结论，不继续同族微调。若用户明确要求继续 debayer topic，应先创建 `010-edge-aware-profile-or-branch-distribution` 或等价 phase，目标是获取 edge-aware 分支分布、load-count ablation（加载数量消融）或其它新候选证据；不能从当前 negative diagnostic 直接进入 production integration loop（生产接入闭环）。
