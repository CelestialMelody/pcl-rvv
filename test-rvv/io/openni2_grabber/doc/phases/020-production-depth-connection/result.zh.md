# Phase 020 Result: production-depth-connection

## 结论

本阶段完成 `PointXYZ` depth-only production patch（生产补丁）和接入后板卡重测。`openni2_grabber.cpp` 现在有 production detail helper（生产内部 helper）`fillXYZPointCloudStd` / `fillXYZPointCloudRVV` / `fillXYZPointCloudCandidate`，`OpenNI2Grabber::convertToXYZPointCloud` 在准备好 depth buffer、相机参数和 invalid values 后调用该候选分流。

PI4 production-detail（生产内部边界）板卡 repeated 结果为 positive：`prod_xyz_depth_full_640x480` 5-run median 1.19x，min 1.15x，max 1.22x，checksum 一致，Evidence Doctor（证据体检）为 Errors=0、Warnings=0、Suggestions=0。

用户已确认“有收益即可采纳”。因此本阶段在 PI5 后进入 adopted production behavior（已采纳生产行为）收尾：补丁保留在工作区，长期 `doc-rvv` 文档和队列表均按接入后板卡数据写成 adopted；本轮仍不自动提交。

## 实际执行范围

| action | status | evidence |
| --- | --- | --- |
| RED test | done | `make run_test_rvv` 在 helper 缺席时链接失败，缺少 `pcl_rvv_openni2_grabber_*_test_hook`。 |
| PI2 production patch | done | 修改 `io/src/openni2_grabber.cpp`，只覆盖 `convertToXYZPointCloud` 的 `PointXYZ` 连续 depth projection。 |
| PI3 correctness | done | `make run_test_compare`：当时 Std/RVV 各 7 个 gtest 全通过；production-detail hook bitwise 对拍通过，RVV build path hit 为 RVV。Phase 030 后同一入口扩展为各 8 个 gtest。 |
| QEMU smoke | done | `make run_bench_rvv BENCH_ARGS="--case-filter prod_xyz_depth_full_640x480 --iterations 2 --warmup-iterations 1"` 可运行；只证明日志形状。 |
| asm | done | `make check_openni2_grabber_production_rvv_asm` 通过，看到 `vle16`、`vfcvt`、`vfmul`、`vmseq`、`vsse32`。 |
| board repeated | done | `log/board/repeated_production_depth/summary.md`。 |
| Evidence Doctor / registry | done | `log/board/repeated_production_depth/evidence_doctor.md` 为 0/0/0；`log/evidence_registry.json` 已 record。 |

## 生产边界

| item | result |
| --- | --- |
| production entry | `OpenNI2Grabber::convertToXYZPointCloud(const DepthImage::Ptr&)`。 |
| helper boundary | production detail helper in `io/src/openni2_grabber.cpp`，test/bench 通过 macro hook 调用。 |
| point type | 具体 `pcl::PointXYZ`，不是模板入口。 |
| data layout | 连续 `std::uint16_t` depth map 到 AoS `PointXYZ`。 |
| fallback | 非 RVV 构建自然调用 Std helper；RGB/RGBA、IR、mismatch、legacy OpenNI 保持原路径或未接入。 |
| public-entry evidence gap | 本地交叉依赖未启用 OpenNI2，无法构造真实 `OpenNI2Grabber` public entry test；当前生产证据是 production-detail，不是 production-public。 |

## 板卡证据

| case | runs | median | min | max | values | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `prod_xyz_depth_full_640x480` | 5 | 1.19x | 1.15x | 1.22x | 1.15x, 1.22x, 1.19x, 1.21x, 1.18x | positive |

证据路径：

- `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/summary.md`
- `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_manifest.json`
- `test-rvv/io/openni2_grabber/log/board/repeated_production_depth/evidence_doctor.md`
- `test-rvv/io/openni2_grabber/log/evidence_registry.json`

## Diagnostic 到 Production 错配审计回填

| question | result |
| --- | --- |
| evidence role | production-detail；不是完整 production-public。 |
| A/B boundary | `openni2_grabber.cpp` production detail helper via hook。 |
| 当前决策问题 | 接入后是否仍值得保留。当前 detail 边界 positive，且用户已确认有收益即可采纳。 |
| diagnostic 是否可外推到 production | Phase 000 只支持有界探针；Phase 020 生产 detail 重测已经确认接入后 helper 收益，但 public entry 仍有依赖缺口。 |
| comparison-boundary / baseline mismatch 风险 | 仍有：真实 OpenNI2 public entry 的 device、resize buffer、header/sensor metadata 未进入 bench 计时。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前不是 RVV family selection；不需要 RVV-vs-RVV A/B。 |

## 泛型测试计划回答

`convertToXYZPointCloud` 返回具体 `pcl::PointCloud<pcl::PointXYZ>`，不是模板入口，因此当前 production patch 没有泛型点型生产测试面。Phase 030 已补 `convertToXYZRGBPointCloud<PointT>` 对应的 `PointXYZRGB` / `PointXYZRGBA` 诊断 correctness 覆盖；该测试只证明 RGB/RGBA 模板诊断 helper 与标量 reference bitwise 对齐，不会把本阶段 `PointXYZ` 结果外推到模板点型，也不支持 RGB/RGBA production 接入。

## 进一步优化方向

当前值得继续的同 topic 方向已经完成 PI5 采纳：depth-only `PointXYZ` production detail 接入和板卡重测已闭合。剩余可尝试方向如下：

| direction | status | reason / resume condition |
| --- | --- | --- |
| public-entry OpenNI2 test | blocked / external dependency | 需要 RISC-V OpenNI2 头/库或完整 OpenNI2-enabled 构建环境。 |
| RGB/RGBA overlay | deferred | Phase 000 median 1.02x，收益近阈值；需要 profile 或新 pack/store candidate。 |
| mismatch stride mapping | deferred | Phase 000 median 0.98x，当前不值得接 production。 |
| IR intensity | deferred | 当前没有 RVV intensity candidate，Phase 000 方向摇摆。 |
| legacy `openni_grabber.cpp` parity | deferred | OpenNI2 depth-only 已采纳；legacy parity 应作为独立 topic，不能把当前证据直接外推。 |

## Continue / Stop Decision

`continue_stop_decision`: adopted_closeout_complete。

`stop_condition_hit`: none for current adopted scope。当前补丁和证据支持采纳，且用户已确认“有收益即可采纳”。不继续扩大到 public-entry、RGB/RGBA、IR、mismatch 或 legacy OpenNI，是因为这些方向分别命中外部依赖缺口、收益不足或独立 topic 边界。

`next_phase_default`: no unblocked same-topic optimization。后续只有在补齐 OpenNI2-enabled RISC-V public-entry 环境、出现 RGB/RGBA 或 IR 新 profile / 新候选，或用户明确开启 legacy parity topic 时恢复。
