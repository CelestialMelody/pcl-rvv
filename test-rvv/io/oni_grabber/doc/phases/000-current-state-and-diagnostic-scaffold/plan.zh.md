# Phase 000 Plan: current-state-and-diagnostic-scaffold

## 阶段意图和边界

本阶段只回答一个窄问题：`io/src/oni_grabber.cpp` 中 `ONIGrabber::convertToXYZPointCloud` 的 depth-only `PointXYZ` 反投影循环，是否能在 production-shaped diagnostic（生产形态诊断）边界下复用 OpenNI / OpenNI2 的 synthetic depth oracle 并形成 RVV 候选收益。阶段不修改 production（生产源码），不覆盖 RGB/RGBA、IR、真实 ONI 文件读取、replay reader 调度、OpenNI runtime、public-entry throughput（公开入口吞吐）或 production dispatch（生产分流）。

`validated_scope`：depth-only `PointXYZ`，synthetic depth frame，`Scalar=float`，organized continuous depth map，`640x480` 代表规模和小规模 correctness 样本。

`unvalidated_scope`：RGB/RGBA/IR、depth/image 尺寸不一致、真实 ONI 文件、OpenNI device wrapper、public entry、production patch、其它点类型、其它 `Scalar` 和 replay 调度成本。

## 当前状态清单

| area | current state |
| --- | --- |
| 筛选来源 | `doc-rvv/library-screening/io/io-retained-candidate-rescreen.zh.md` 把 `src/oni_grabber.cpp` 升级为函数级评估，第一目标是 depth-only `PointXYZ` production-shaped diagnostic。 |
| 源码入口 | `ONIGrabber::depthCallback` 在有 point cloud listener 时调用 `convertToXYZPointCloud(depth_image)`。 |
| 标量热点 | `convertToXYZPointCloud` 分配 organized cloud，必要时 resize depth buffer，然后双层循环按 `u/v`、depth mm->m、focal constant 写 `PointXYZ`，invalid depth 写 NaN。 |
| 同类经验 | `openni2_grabber` depth-only production-detail helper 已采纳，板卡 median `1.19x`；但 ONI replay / OpenNI wrapper 边界不同，不能外推成 production 结论。 |
| 当前 topic 资产 | 本阶段新建 `test-rvv/io/oni_grabber`，尚无 result、summary、Evidence Doctor 或 registry。 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic`（生产形态诊断） |
| A/B boundary | `test helper` / `production-shaped helper`，不是真实 public overload |
| 当前决策问题 | `RVV-vs-scalar` 候选筛选；不是 production adoption（生产采纳） |
| diagnostic 是否可外推到 production | 只能外推到“depth projection helper 是否值得接入生产探针”；不能外推到真实 ONI 文件吞吐或 replay 调度。 |
| comparison-boundary / baseline mismatch 风险 | 有。OpenNI2 helper 的正向来自同构 depth path，但 ONI 的 device wrapper、buffer resize 和 replay reader 可能改变真实主成本。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 若 depth-only diagnostic 为 weak / negative / neutral / unstable，默认不进入 production patch；只有源码审计证明 production helper 复用成本极低且用户授权时，才允许窄 production probe。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 若后续接入 production，需要 production direct correctness、fallback、asm、板卡 repeated 和 PI5 用户确认；本阶段不能 clean-adopt。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| depth-projection-rvv | organized depth frame | `PointXYZ` / `float` / contiguous depth + AoS cloud | test-only helper shaped like `convertToXYZPointCloud` | `make run_test_compare` | `make run_bench_rvv` QEMU smoke; board compare only on target | `collect_board_oni_grabber_repeated` planned | `check_oni_grabber_rvv_asm` planned | summary/manual doctor planned | planned |

## 实现和测试动作

| action | artifact / command | expected evidence | done condition |
| --- | --- | --- | --- |
| 写红灯测试 | `src/test_oni_grabber.cpp` + `make run_test_compare` | 缺少 candidate helper 时链接失败或测试失败 | 已观察到预期失败 |
| 实现候选 | `include/oni_grabber.h` | Std build 走 scalar reference，RVV build 走 RVV intrinsic | `make run_test_compare` 通过 |
| bench smoke | `src/bench_oni_grabber.cpp` + `make run_bench_rvv ...` | QEMU 只证明日志形状 | 输出 Dataset、Iterations、Total Time、checksum |
| 反汇编 | `make check_oni_grabber_rvv_asm` | 关键 RVV 指令存在 | grep gate 通过 |
| 板卡 repeated | `make collect_board_oni_grabber_repeated` + summary | 目标硬件性能 | 5-run summary 形成 decision bucket |
| Evidence Doctor | 脚本或人工检查 summary | Errors / Warnings / Suggestions | 写入 result 和 Handoff |

## 板卡复跑预算和决策桶

默认运行 5-run repeated board。若 summary 或 Evidence Doctor 暴露方向接近 1、长尾明显或 `B/A < 1` 频率异常，最多追加一次同边界 5-run 确认。decision bucket（决策桶）：median speedup >= 1.10 且 min >= 1.00 为 positive；median 1.03-1.10 为 weak_positive；median 0.97-1.03 为 neutral；median < 0.97 为 negative；跨 run 方向摇摆且预算耗尽为 unstable。

## 继续 / 停止条件

继续条件：correctness、bench smoke、asm 或板卡证据仍未闭合且工具可用。用户已说明板卡可用，因此“需要板卡验证”不是停止条件。

停止条件：候选 correctness 失败且无法修复、RVV asm 不闭合、板卡工具不可达、Evidence Doctor Error 不能降级、dirty isolation 不安全，或 diagnostic 已形成明确 negative / unstable 且没有授权 production probe。

## 文档更新清单

本阶段更新 `README.zh.md`、`doc/oni_grabber-evaluation.zh.md`、`doc/phases/README.zh.md`、本 plan、后续 result、`doc/phases/optimization-matrix.zh.md` 和 `doc/optimization-roadmap.zh.md`。`doc-rvv/io/oni_grabber-RVV.zh.md` 当前不适用。
