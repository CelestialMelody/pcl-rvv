# Phase 000: current-state and downsample diagnostic

## 阶段意图和边界

本阶段验证 `keypoints/src/brisk_2d.cpp` 中 `Layer::halfsample()` 与
`Layer::twothirdsample()` 的 BRISK scale-space downsample（尺度空间下采样）
是否能在 RISC-V / RVV 构建下形成可执行、可对拍、可测性能的 production-shaped
diagnostic（生产形态诊断，尽量贴近真实生产入口但仍以专项测试为主）。

本阶段证明：

- `brisk::Layer` 派生层构造在非 SSSE3 平台不再停留在空实现。
- `HALFSAMPLE` 与 `TWOTHIRDSAMPLE` 派生图像逐字节匹配 topic-local scalar reference（标量参考链路）。
- QEMU correctness（QEMU 正确性验证）和反汇编可显示 RVV path（RVV 路径）存在。
- 板卡 repeated bench（重复性能测试）可判断是否进入或采纳 production patch。

本阶段不证明：

- `getAgastPoints()`、AGAST/OAST detector（角点检测器）或 scale refinement（尺度细化）已经加速。
- features 模块的 BRISK descriptor（描述子）路径已经加速。
- 完整 `BriskKeypoint2D::compute()` 的 end-to-end（端到端）收益；若 downsample helper 正向，后续阶段再补公开入口证据。

## 当前状态清单

| area | 当前事实 | 路径 |
| --- | --- | --- |
| screening source | keypoints 队列表把 BRISK scale-space downsample 列为建议实施第 4 项。 | `doc-rvv/library-screening/keypoints/keypoints-function-evaluation-queue.zh.md` |
| production source | `halfsample` / `twothirdsample` 只有 SSSE3 分支；非 x86 fallback 当前只报 `PCL_ERROR`。 | `keypoints/src/brisk_2d.cpp` |
| template entry | `impl/brisk_2d.hpp` 只把 organized cloud 转 `image_data` 并调用 `ScaleSpace`。 | `keypoints/include/pcl/keypoints/impl/brisk_2d.hpp` |
| existing tests | 上游 BRISK test 只在 SSSE3 下运行，RISC-V 不覆盖。 | `test/features/test_brisk.cpp` |
| topic assets | 本阶段新建 `test-rvv/keypoints/brisk_2d`。 | `test-rvv/keypoints/brisk_2d/**` |

## 候选族

| candidate family | 入口 | 设计 | 风险 |
| --- | --- | --- | --- |
| scalar portable fallback | `Layer::halfsample` / `twothirdsample` | 非 SSSE3 / 非 RVV 下执行同语义标量 downsample。 | 需要与既有 SSSE3 加权/舍入边界保持文档化。 |
| RVV byte downsample | 同上 | 用 `vlse8` 按 2 或 3 像素 stride load（跨步加载），扩展到 `u16` 后加权、除法并窄化写回。 | `uint8_t` 加权整数除法、tail 宽度和 3x3->2x2 映射必须逐字节对拍。 |
| public compute smoke | `BriskKeypoint2D::compute()` | 在 production patch 后构造 organized synthetic image 验证不再报空实现。 | AGAST detector 和关键点数量可能受输入图案影响，不作为首阶段收益证据。 |

## 优化矩阵

| candidate | entry shape（入口形态） | data / layout | correctness | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| scalar portable fallback | `brisk::Layer` helper | contiguous `uint8_t` image | `run_test_compare` | `bench_brisk_2d_std` | required if RVV positive | not_applicable | not_applicable | pending |
| RVV byte downsample | `brisk::Layer` helper | contiguous `uint8_t` image, tail sizes | `run_test_compare` | `bench_brisk_2d_rvv` | 5-run repeated | `dump_bench_rvv` | required before EvidenceDecision | pending |
| public compute smoke | `BriskKeypoint2D::compute()` | organized `PointXYZRGBA` | phase_deferred | phase_deferred | phase_deferred | phase_deferred | phase_deferred | next phase if helper positive |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED test | `make -C test-rvv/keypoints/brisk_2d run_test_compare` | 在 production 修改前，非 SSSE3 fallback 导致测试失败。 |
| portable fallback | `keypoints/src/brisk_2d.cpp` | `USE_PCL_RVV10=0` 的 helper 对拍通过。 |
| RVV candidate | `keypoints/src/brisk_2d.cpp` | `USE_PCL_RVV10=1` 的 helper 对拍通过，反汇编含 `vlse8` / `vzext` / vector arithmetic。 |
| bench | `make ... run_bench_rvv`，板卡 `collect_repeated_board_evidence` | QEMU 只检查日志形状；板卡 repeated summary 给决策桶。 |
| docs | phase result、evaluation、roadmap、Handoff | 证据边界、继续/停止条件和文档归属可恢复。 |

## Evidence Doctor 和 registry

板卡 summary 生成后运行 `run_repeated_board_evidence_doctor`，再用
`record_repeated_board_evidence_state` 登记到 `log/evidence_registry.json`。若脚本尚未完成或 summary
格式不满足通用 doctor，本阶段 result 必须降级为人工 Evidence Doctor（证据体检）解释，不直接写 clean production。

## 板卡复跑预算和决策桶

- 默认 5 run，每 run 使用 `--iterations 100 --warmup-iterations 10`。
- `positive`：主要 helper case median speedup 大于 1.20x，且无 correctness / checksum error。
- `weak-positive`：median speedup 为 1.05x 到 1.20x，只有在实现小、fallback 简单、公开入口常用时采纳。
- `neutral`：0.95x 到 1.05x。
- `negative`：小于 0.95x。
- 5 run 内方向摇摆且 decision bucket 改变时标为 `unstable`，不无限复跑。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic；后续 production patch 后升级为 production-detail / production-public。 |
| A/B boundary | production detail helper，测试通过真实 `brisk::Layer` 构造触达。 |
| 当前决策问题 | RVV-vs-scalar；若生产接入后出现多实现族，再补 RVV-family-selection。 |
| diagnostic 是否可外推到 production | 部分可外推：helper 是真实 production helper；但完整 `BriskKeypoint2D::compute()` 还包含 AGAST 和 refinement。 |
| comparison-boundary / baseline mismatch 风险 | 有。helper bench 不等于公开入口端到端性能。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak-positive 可进入窄生产探针；neutral/negative 不采纳，只保留 fallback 修复或诊断证据。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 本阶段只有一个 RVV family，不需要 RVV-vs-RVV；接入后必须补 production direct board evidence。 |

## Phase Scope 与扩展队列

- `validated_scope`：`uint8_t` contiguous BRISK image，`HALFSAMPLE` / `TWOTHIRDSAMPLE`，常规和 tail 图像尺寸。
- `unvalidated_scope`：AGAST/OAST detector、`getValue()` 插值、完整 keypoint 输出、features BRISK descriptor。
- `point_type_expansion_queue`：不适用；downsample helper 只处理 `uint8_t` image，不依赖 PCL point type traits。
- `phase_closeout_boundary`：本阶段最多关闭 downsample helper / production detail helper，不关闭完整 BRISK topic。

## 继续 / 停止条件

若板卡 helper / production-detail 证据为 positive 或 weak-positive，且 production patch 小、fallback 清晰，
按用户本轮授权可直接采纳并进入 production doc closeout。若板卡不可访问或 repeated evidence 缺失，停在
`blocked` 并保留恢复命令。若 evidence 为 neutral/negative 且没有新的低风险候选，停止并写 no-production /
fallback-only closeout。

## 文档更新清单

- `doc/phases/000-current-state-and-downsample-diagnostic/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/brisk_2d-evaluation.zh.md`
- 若 PI5 生产证据显示收益并按本轮授权采纳：`doc-rvv/keypoints/brisk_2d-RVV.zh.md`
