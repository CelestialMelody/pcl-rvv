# Phase 020: Average 3D Gradient Diff Buffer Diagnostic Plan

## 阶段意图和边界

本阶段验证 `initAverage3DGradientMethod()` 中 diff_x / diff_y buffer（差分缓冲区）的仅测试使用 RVV 诊断候选。目标是证明 organized image（有组织图像点云）内圈像素的三维差分：

- `diff_x(row, col) = point(row, col + 1) - point(row, col - 1)`
- `diff_y(row, col) = point(row + 1, col) - point(row - 1, col)`

可以在 `PointXYZ`-like x/y/z 字段、4-float AoS stride（数组结构跨步）布局下用 RVV strided load/store（跨步加载 / 存储）批量生成，并与标量参考逐元素一致。

本阶段不修改 production（生产源码）头文件，不证明真实 `IntegralImageNormalEstimation::initAverage3DGradientMethod()` 已经命中 RVV，也不覆盖后续 `integral_image_DX_ / integral_image_DY_` 构建、normal solver（法线求解器）、viewpoint flip（视点翻转）、indices 或泛型 `PointInT` traits（点类型字段特征）。

## 当前状态清单

| item | current state | path / evidence |
| --- | --- | --- |
| production scalar source | `initAverage3DGradientMethod()` 标量循环写 `diff_x_` / `diff_y_`，每个像素占 4 个 float，第四通道由零初始化保留为 0 | `features/include/pcl/features/impl/integral_image_normal.hpp` |
| existing diagnostic helper | 只覆盖 `computeFeature()` map-prep 的 depth-change map 和 distance map initialization | `include/impl/integral_image_normal_map_prep.hpp` |
| correctness target | `make run_test_compare` 已对 Std / RVV 两个构建运行 map-prep gtest | `src/test_integral_image_normal.cpp` |
| bench target | `bench_integral_image_normal` 当前只计 map-prep helper | `src/bench_integral_image_normal.cpp` |
| board evidence | Phase 000 map-prep 5-run repeated board 为 positive；不外推到本阶段 | `log/board/repeated-summary.md` |
| Evidence Doctor | Phase 000 doctor 为 Errors=0 / Warnings=0 / Suggestions=0；本阶段需要重新生成 manifest | `log/board/evidence_doctor.md` |

## validated_scope / unvalidated_scope

validated_scope（本阶段准备证明的范围）：

- entry shape（入口形态）：test helper boundary（测试辅助入口边界）。
- row source（行来源）：organized image contiguous rows（有组织图像连续行）。
- point type / Scalar / layout：测试专用 `XYZPadPoint`，字段 `x/y/z` 为 float，结构体 stride 为 4 个 float。
- size：常规 `320x240` 和 tail width（非 VL 整倍数）输入。
- output：完整 `width * height * 4` float buffer，边界和第四通道保持 0。

unvalidated_scope（本阶段不证明的范围）：

- production template `PointInT` 的泛型字段 offset、POD / standard-layout gate、非 4-float stride 和 `Scalar=double`。
- `integral_image_DX_ / DY_` 的 `setInput()` 行为和后续 normal output。
- `computeFeature()`、`computeFeatureFull()`、`computeFeaturePart()` 真实入口。

point_type_expansion_queue（点类型扩展队列）：

| expansion | resume condition | required evidence |
| --- | --- | --- |
| production `PointInT` traits / layout gate | 用户授权 production PI2 或单独授权生产形态诊断 | traits / offset audit、fallback tests、production direct correctness、asm、board repeated、Evidence Doctor |
| non-4-float stride diagnostic | 本阶段 positive 且需要判断泛型布局成本 | dedicated fixture、strided load correctness、board A/B、doctor metadata |

## 候选族和实现假设

| candidate family | hypothesis | risk / unknown | required evidence |
| --- | --- | --- | --- |
| diff-buffer RVV strided loads | 对 4-float stride 的 x/y/z 字段分别 `vlse32`，三分量相减后 `vsse32` 写回 diff buffer | strided store 可能增加内存流量；收益可能被 integral image 构建稀释 | TDD correctness、QEMU Std/RVV、RVV asm、board repeated、Evidence Doctor |
| scalar fallback in same helper | 非 RVV 构建使用同一 helper 的标量实现，保持测试 target 可对拍 | 小尺寸生产源码未显式 guard，本 helper 会安全保留零 buffer；需要在文档说明这是 test-only robustness | Std correctness |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| average 3D gradient diff buffer RVV | organized image | `XYZPadPoint`, float, 4-float stride | `initAverage3DGradientMethod()` diff_x / diff_y diagnostic | `run_test_compare` with new gtests | `run_board_bench_compare` with `avg3d_diff_*` cases | planned 5-run repeated | `dump_test_rvv` should show strided RVV load/store and vector subtract | planned manifest + doctor | planned |

## 实现和测试动作

| action | artifact | command / evidence | completion criteria |
| --- | --- | --- | --- |
| RED correctness | `src/test_integral_image_normal.cpp` | `make -C test-rvv/features/integral_image_normal run_test_compare` | 编译失败或测试失败，失败原因是缺少 diff-buffer helper API |
| GREEN helper | `include/integral_image_normal.h`, `include/impl/integral_image_normal_map_prep.hpp` | same command | Std/RVV gtest 全部通过 |
| bench extension | `src/bench_integral_image_normal.cpp` | board repeated target | 新增 `avg3d_diff_320x240` 和 tail case，checksum 可解析 |
| manifest extension | `script/generate_integral_image_normal_evidence_manifest.py` | `make run_board_evidence_doctor` | manifest 同时覆盖 map-prep 和 avg3d-diff case label |
| asm check | generated asm | `make dump_test_rvv` + `rg` | 反汇编出现本阶段 RVV 指令，归属为 test helper |
| board repeated | board logs and summary | bounded 5-run repeated, then doctor + registry | decision bucket 稳定或按规则降级 |
| docs | phase result, matrix, roadmap, evaluation, doc suite, handoff | `git diff --check` and artifact scan | 本阶段证据链和下一步状态可恢复 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic |
| A/B boundary | test helper |
| 当前决策问题 | RVV-vs-scalar for implementation-shape（实现形态） |
| diagnostic 是否可外推到 production | unknown。本阶段只验证 4-float stride 测试点型，不能替代 production `PointInT` layout / traits gate。 |
| comparison-boundary / baseline mismatch 风险 | low inside diagnostic；high for production because真实生产入口还包括对象状态、积分图构建和模板点型。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes, only if production profile shows diff-buffer cost matters and PI1 can freeze traits / fallback / dispatch scope. |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes, if production 已有或后续形成其它 adopted RVV family；本阶段不 clean-adopt。 |

## Evidence Doctor、registry 和板卡复跑规则

- Evidence Doctor 输入：`log/board/evidence_manifest.json`，由 `script/generate_integral_image_normal_evidence_manifest.py` 生成。
- 预期：Errors=0；Warnings 若来自环境字段缺失，需要在 result 中解释并降级边界为 diagnostic。
- registry：board summary / manifest / doctor 生成后运行 `record_board_evidence_state` 或等价 record，随后运行 `evidence_status`。
- board availability：当前 prompt 已说明板卡可用。本阶段若 correctness 和 asm 通过，应继续执行 5-run board repeated；只有 ssh / rsync / target 失败、复跑预算跨 bucket 摇摆或 dirty isolation 不安全才停止。
- bounded rerun budget：默认 5-run repeated；若 decision bucket 接近阈值或 Evidence Doctor 触发方向/长尾 warning，最多追加 1 个同边界确认 batch。稳定桶直接关闭，跨桶摇摆标为 `unstable`。

## 继续 / 停止条件

继续条件：

- RED 已按预期失败。
- GREEN correctness 通过后仍需要 asm、board、doctor 和 registry。
- Phase 020 positive 或 weak-positive 后，roadmap 仍有 production 授权缺口或后续 point-type expansion（点类型扩展）队列。

停止条件：

- 继续需要修改 production header，且用户仍未授权。
- 板卡不可达、工具失败或 Evidence Doctor Error 无法修复。
- diff-buffer repeated board 为 negative / unstable，且本阶段矩阵已写清该 diagnostic boundary 不能支持生产探针。

next_phase_default：完成本阶段后，如果证据为 positive，默认回到生产授权门禁：要么请求 PI2 production patch 授权，要么继续另一个未接 production 的 diagnostic 候选；若证据为 negative，更新 roadmap 后考虑 distance transform 或 full-profile 先行。

## 文档更新清单

- 新建 `result.zh.md` 回填计划动作、证据、Evidence Doctor 和 continue / stop decision。
- 更新 `doc/phases/README.zh.md`、`optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- 更新 topic-local evaluation、benchmark/evidence、optimization evidence、correctness tests 和 test-support code map。
- 更新 `doc-rvv/library-screening/features/features-function-evaluation-queue.zh.md` 的 topic 状态。
- 刷新 `tmp/rvv-work-logs/features/integral_image_normal/current-handoff/current-handoff.zh.md`。
