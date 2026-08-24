# Phase 010 Result: production-depth-probe

## 当前结论

本阶段完成 production integration loop（生产接入闭环）的 PI1-PI5，并在用户确认“接入后板卡测试有收益即可采纳”后进入 S11 production closeout（生产收尾）。`io/src/oni_grabber.cpp` 现在保留一个窄范围 adopted production behavior（已采纳生产行为）：`ONIGrabber::convertToXYZPointCloud` 的 depth-only `PointXYZ` 填点循环被抽成 `fillXYZPointCloudStd` / `fillXYZPointCloudRVV` / `fillXYZPointCloudCandidate`。非 `__RVV10__` 构建走 Std；`__RVV10__` 构建在偶数尺寸 organized frame（有组织帧）边界下走 RVV helper。

当前 decision 是 `adopted production behavior / production-detail positive`。接入后 production-detail（生产内部边界）板卡 repeated summary（重复板卡摘要）为 median `1.18x`、min `1.16x`、max `1.22x`；Evidence Doctor（证据体检）为 Errors=0、Warnings=3、Suggestions=2。Warnings 来自 summary-only metadata（仅摘要元数据）、case role（用例角色）和 run contract（运行合同）不完整，因此结论仍不扩大为完整 ONI replay public-entry（公开入口）吞吐证据。

## 范围回填

| area | result |
| --- | --- |
| validated_scope | `convertToXYZPointCloud` / depth-only `PointXYZ` / `float` / organized contiguous depth frame / invalid depth mask / AoS cloud store |
| unvalidated_scope | RGB/RGBA/IR、尺寸不一致 resize 成本、ONI 文件 replay 调度、完整 public-entry throughput、其它点类型 |
| fallback | 非 `__RVV10__` 构建自然调用 Std helper；未改公开 API 或类声明 |
| semantic boundary | 保持原 `centerX = width >> 1`、`centerY = height >> 1` 和 `u/v` 迭代语义 |

## 实现结果

| artifact | change |
| --- | --- |
| `io/src/oni_grabber.cpp` | 新增 production-detail Std/RVV/Candidate helper、测试 hook，并让 `convertToXYZPointCloud` 调用 candidate。 |
| `src/oni_grabber_production_detail_test.cpp` | 通过 test hook 对拍 production Std 与 candidate，验证路径 hook 在 Std/RVV build 下分别记录 Scalar/Rvv。 |
| `src/bench_oni_grabber.cpp` | 新增 `prod_xyz_depth_full_640x480` case，直接调用 production-detail candidate hook。 |
| `Makefile` | 新增 production smoke/repeated/summary/Evidence Doctor/asm target。 |

## 证据

| evidence | command / path | result | boundary |
| --- | --- | --- | --- |
| correctness（正确性） | `make run_test_compare` | Std/RVV 各 5 个 gtest 通过，包含奇数尺寸回退 Scalar gate | diagnostic + production-detail helper |
| QEMU smoke（QEMU 冒烟，非性能） | `make run_bench_rvv BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` | case 可运行并输出 checksum | 日志形状，不作性能结论 |
| asm attribution（反汇编归属） | `make check_oni_grabber_production_rvv_asm` | hook 符号、`vle16.v`、`vfcvt.f.xu.v`、`vfmul`、`vmseq`、`vsse32.v` 存在 | bench RVV helper |
| board smoke（板卡冒烟） | `make run_board_oni_grabber_production_smoke` | `1.21x`，checksum 一致 | 单次 smoke |
| board repeated（重复板卡测试） | `log/board/repeated_production_depth/summary.md` | median `1.18x`，min `1.16x`，max `1.22x`，values `1.22x, 1.16x, 1.19x, 1.18x, 1.17x` | production-detail，非完整 public-entry |
| Evidence Doctor（证据体检） | `log/board/repeated_production_depth/evidence_doctor.md` | Errors=0，Warnings=3，Suggestions=2 | summary-only reviewer aid |

Evidence Doctor 的 Warning 已降级解释：当前输入是 Markdown summary，缺少完整 metadata、运行合同和二进制身份；case 名含 `prod_`，但 summary 没有结构化 evidence role。因此本阶段只写成 production-detail positive，不写成完整 production public evidence。

## EvidenceDecision

`decision_bucket = weak-positive / adopted production-detail`。收益处于约 `1.16x-1.22x`，实现小、fallback 简单、语义边界清晰，且用户已确认接入后板卡结果有收益即可采纳，因此当前补丁保留为已采纳生产行为。缺少真实 ONI replay public-entry 吞吐、环境 metadata 和 manifest，所以不写成 production-public clean adoption。

## Production closeout

已创建长期主题文档 `doc-rvv/io/oni_grabber-RVV.zh.md`，并同步 topic README、evaluation、roadmap、optimization matrix 和保留候选复筛状态表。本轮不创建 commit，也不提交 raw logs（原始日志）。

## 后续优化审计

| candidate | decision | reason | resume condition |
| --- | --- | --- | --- |
| RGB/RGBA extension | `not_recommended_now` | ONI 源码路径除 xyz 外还涉及 RGB buffer、packed color union（打包颜色联合体）和 RGB/RGBA 分支；同构 OpenNI2 topic 的 RGB overlay repeated median 仅 `1.02x`，且收益接近阈值。 | 新 pack/store candidate 或 profile（性能剖析）证明 RGB overlay 是实际瓶颈。 |
| IR intensity | `not_recommended_now` | ONI IR 路径需要同时保持 `PointXYZI::data_c` 清零和 intensity 写入；OpenNI2 同构诊断为 median `1.01x` 且 2/5 低于 1。 | 先有 focused IR RVV candidate，并通过 correctness、asm、board repeated 和 Doctor。 |
| depth/image mismatch / full ONI replay public entry | `blocked/external_input` | 当前 production-detail bench 不包含真实 ONI 文件读取、replay reader 调度或完整 callback/signal 成本。 | 用户提供 ONI replay 场景或 profile 证明 conversion 仍是主成本。 |

`continue_stop_decision = turn_stop_deferred with stop_condition_hit`。停止条件不是板卡不可用，而是当前授权范围内没有值得继续同轮推进的未阻塞优化方向：depth-only 已采纳；RGB/RGBA/IR 在同构证据下接近 1.0 或方向摇摆；public-entry 需要外部 ONI replay 场景或 profile。若后续提供新 profile 或新候选，再从 roadmap 的对应 resume condition 恢复。

## Doc suite parity closeout gate

| area | current shape scan | decision | evidence / next action |
| --- | --- | --- | --- |
| README navigation | `README.zh.md` 列出默认阅读路径、命令和提交边界。 | `adopted` | 已改为 adopted 状态并链接长期 `doc-rvv`。 |
| testing overview / correctness / benchmark evidence | 小 topic 角色合并在 `README.zh.md`、`doc/oni_grabber-evaluation.zh.md` 和本 result。 | `adopted` | gtest、QEMU smoke、asm、board repeated 和 Doctor 均在证据表中列明。 |
| optimization evidence / roadmap | `doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md` 承载候选状态。 | `adopted` | RGB/RGBA/IR 与 public-entry 恢复条件已更新。 |
| test-support code map | `doc/oni_grabber-evaluation.zh.md` Traceability Map 覆盖 production helper、test hook、gtest 和 bench wrapper。 | `adopted` | 当前 helper 文件短小，未触发必须拆分到 `include/impl` 的 blocker。 |
| long-term `doc-rvv` | `doc-rvv/io/oni_grabber-RVV.zh.md` 只写已采纳 production-detail 行为、证据链和边界。 | `adopted` | Phase 000 diagnostic 数据不作为正式性能数字来源。 |
| artifact tracking | 当前 topic 文件仍处于未提交工作区，后续 commit 应按 topic-only 策略精确加入。 | `adopted` | `build/`、raw logs、私有板卡配置和 `config.mk` 默认不提交。 |
