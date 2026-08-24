# ONI Grabber 函数级评估

## 当前判断

`io/src/oni_grabber.cpp` 的本 topic 当前为 `adopted production behavior / production-detail positive`。第一目标只覆盖 `ONIGrabber::convertToXYZPointCloud` 的 depth-only `PointXYZ` organized frame-to-cloud（有组织帧到点云）循环；Phase 000 的测试专用 RVV candidate 得到 median `1.17x` 的 diagnostic positive（诊断正向）结果，Phase 010 的 production-detail（生产内部边界）补丁得到 median `1.18x`、min `1.16x`、max `1.22x` 的接入后板卡 repeated summary（重复板卡摘要）。用户已确认接入后板卡测试有收益即可采纳，因此当前 production patch（生产补丁）保留为已采纳生产行为。

## 入口和标量路径

公开入口来自 ONI replay 设备回调。`depthCallback` 在 `point_cloud_signal_` 有 listener 时调用 `convertToXYZPointCloud(depth_image)`，后者构造 `PointCloud<PointXYZ>`，设置 width、height、frame id 和 `is_dense=false`，再从 OpenNI depth metadata 读取 depth map。若 depth image 尺寸与 grabber 的 depth stream 尺寸不同，源码会通过 `fillDepthImageRaw` 填充静态 buffer。

热点循环是双层 `v/u` organized traversal（有组织网格遍历）。每个 depth pixel 若为 `0`、`getNoSampleValue()` 或 `getShadowValue()`，输出 `x/y/z = NaN`；否则 `z = depth_mm * 0.001f`，`x = u * z * constant`，`y = v * z * constant`。`constant` 来自 `1.0f / device_->getDepthFocalLength(depth_width_)`。这个片段是连续 depth load 加 AoS（结构数组）`PointXYZ` stride store，适合先做 RVV 候选。

## 可 RVV 化片段和不覆盖范围

| area | 判断 | 说明 |
| --- | --- | --- |
| depth-only `PointXYZ` | 当前 phase 评估 | 公式和 invalid depth mask 清晰，可复用 OpenNI2 synthetic oracle 思路。 |
| RGB/RGBA | 暂缓 | 颜色 buffer、alpha / packed field 语义和 OpenNI2 历史弱收益使其不适合作为第一阶段。 |
| IR / `PointXYZI` | 暂缓 | intensity 合并和 depth resize 分支增加证据面；OpenNI2 IR 历史不稳定。 |
| 真实 ONI replay | 暂缓 | 文件读取和 replay reader 状态可能主导 public-entry 成本。 |
| production dispatch | 已采纳 | depth-only `PointXYZ` production-detail helper 已接入 `convertToXYZPointCloud`；非 RVV 构建和奇数尺寸 gate 回退 Std。 |

## Traceability Map（可追踪性地图）

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `ONIGrabber::depthCallback` | production public entry | 深度回调入口，决定是否生成点云 | OpenNI replay device callback | `convertToXYZPointCloud` | production boundary（生产边界） | `io/src/oni_grabber.cpp` |
| `ONIGrabber::convertToXYZPointCloud` | production scalar helper | 原始 depth-only `PointXYZ` 标量路径 | `depthCallback` | 输出 `PointCloud<PointXYZ>` | scalar reference source（标量参考来源） | `io/src/oni_grabber.cpp` |
| `fillXYZPointCloudCandidate` | production detail helper | `__RVV10__` 下走 RVV，否则走 Std | `convertToXYZPointCloud` | 写 `PointXYZ` x/y/z | bounded production candidate（有界生产候选） | `io/src/oni_grabber.cpp` |
| `fillXYZCloudScalar` | diagnostic reference | 测试专用标量参考链路 | gtest / bench | checksum 和对拍 | correctness reference（正确性参考） | `test-rvv/io/oni_grabber/include/oni_grabber.h` |
| `fillXYZCloudCandidate` | candidate helper | Std/RVV 构建分流的候选链路 | gtest / bench | correctness、asm、board bench | production-shaped diagnostic | `test-rvv/io/oni_grabber/include/oni_grabber.h` |
| `test_oni_grabber.cpp` | correctness gate | 小规模和 bitwise 对拍 | `make run_test_compare` | QEMU / board test binary | correctness gate | `test-rvv/io/oni_grabber/src/test_oni_grabber.cpp` |
| `bench_oni_grabber.cpp` | bench wrapper | 生成可解析 timing 和 checksum | `make run_bench_*` / board target | repeated summary | benchmark diagnostic | `test-rvv/io/oni_grabber/src/bench_oni_grabber.cpp` |

## 诊断证据链

本阶段已完成 depth-only `PointXYZ` 诊断证据链：

| evidence | command / path | result | boundary |
| --- | --- | --- | --- |
| correctness（正确性） | `make run_test_compare` | Std/RVV 各 2 个 gtest 通过 | test-only helper correctness |
| QEMU smoke | `make run_bench_rvv BENCH_ARGS="--case-filter xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | 输出 Dataset、Iterations、Total Time、checksum | 日志形状，不是性能 |
| asm attribution（反汇编归属） | `make check_oni_grabber_rvv_asm` | `vle16.v`、`vfcvt.f.xu.v`、`vfmul`、`vmseq`、`vsse32.v` 存在 | bench RVV helper |
| board performance diagnostic | `log/board/repeated_diagnostic/summary.md` | median `1.17x`，min `1.13x`，max `1.20x` | production-shaped diagnostic，不是 production direct |
| Evidence Doctor | `log/board/repeated_diagnostic/evidence_doctor.md` | Errors=0，Warnings=2，Suggestions=2 | summary-only reviewer aid |

Evidence Doctor 的两个 Warning 来自 metadata 不完整：当前 summary 没有完整 JSON manifest，也没有结构化暴露 warmup / environment / binary identity。因此当前性能结论只写成 production-shaped diagnostic positive，不能写成完整 production evidence。

## Production 接入判断

当前判断是 `adopted production behavior / production-detail positive`。Phase 010 已完成 production integration loop（生产接入闭环）的 PI1-PI5：production-detail correctness、QEMU smoke、反汇编和板卡 repeated 均闭合；Evidence Doctor 为 Errors=0、Warnings=3、Suggestions=2。Warnings 主要来自 summary-only metadata、case role 和运行合同不完整，因此证据支持已采纳 production-detail 行为，但不能证明完整 ONI replay public-entry throughput。

正式 production 文档为 `doc-rvv/io/oni_grabber-RVV.zh.md`。该长期文档只使用接入后的 Phase 010 板卡数据；Phase 000 diagnostic 数据只作为 topic-local 背景。

## 后续优化审计

| 候选 | 当前判断 | 理由 | 恢复条件 |
| --- | --- | --- | --- |
| RGB/RGBA | 不建议当前继续 | ONI 源码路径除 xyz 外还要维护 RGB buffer、`RGBValue` packed field 和 RGB/RGBA 两个输出字段；同构 OpenNI2 RGB overlay repeated median 仅 `1.02x`。 | 新 pack/store candidate 或 profile 证明 RGB overlay 是瓶颈。 |
| IR / `PointXYZI` | 不建议当前继续 | IR 路径需要保持 `data_c[0..3]` 清零和 intensity 写入；同构 OpenNI2 IR median `1.01x` 且有低于 1 的 run。 | focused IR RVV candidate 先通过 correctness、asm、board repeated 和 Doctor。 |
| 完整 ONI replay public entry | 外部输入阻塞 | 当前证据是 production-detail，不包含 ONI 文件读取、replay reader 调度和 callback/signal 开销。 | 用户提供 ONI replay 场景或 profile 证明 conversion 是主成本。 |
