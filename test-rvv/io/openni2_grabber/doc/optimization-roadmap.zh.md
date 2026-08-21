# OpenNI2 Grabber RVV Optimization Roadmap

## 当前默认恢复队列

| order | phase | action | status | resume condition |
| --- | --- | --- | --- | --- |
| 1 | 000-current-state-and-diagnostic-scaffold | 建立 synthetic frame production-shaped diagnostic、correctness、bench smoke、asm、board repeated 和 Evidence Doctor。 | completed | 结果见 `doc/phases/000-current-state-and-diagnostic-scaffold/result.zh.md`。 |
| 2 | 010-production-integration-plan | 冻结 `convertToXYZPointCloud` / `PointXYZ` / 同尺寸 depth path 的 PI1 计划。 | completed | 用户已授权进入 PI2-PI5。 |
| 3 | 020-production-depth-connection | 接入 depth-only `PointXYZ` production detail helper，完成接入后板卡重测。 | adopted / completed | 结果 positive；用户已确认有收益即可采纳。 |
| 4 | 030-rgb-point-type-diagnostic | 回答泛型点型测试可行性，补 `PointXYZRGB` / `PointXYZRGBA` 诊断 correctness。 | completed / diagnostic-only | Std/RVV 各 8 个 gtest 通过；不扩大 production scope。 |
| 5 | legacy-openni-parity | 审计 `io/src/openni_grabber.cpp` 是否能复用同一 frame-to-cloud oracle。 | deferred | OpenNI2 depth-only 已采纳；legacy parity 应作为独立 topic 恢复。 |

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| depth_xyz_contiguous_rvv | 当前 OpenNI2 depth projection loop | `PointXYZ` 同尺寸 depth path | 连续 depth buffer 上的 VL chunk 公式化 | public-entry OpenNI2 构建未覆盖 | production detail correctness、asm、board repeated、doctor；public-entry 依赖另补 | adopted production behavior / production-detail positive | 020 |
| rgb_overlay_contiguous_rvv | RGB overlay loop | `PointXYZRGB` / `PointXYZRGBA` rgba 字段 | 字节 load 与 32-bit pack/store | alpha 语义和 RGB/RGBA union 差异 | focused ablation、board repeated、doctor | attempted / deferred；Phase 030 泛型诊断 correctness 已补 | 需要 profile 或更强候选 |
| ir_intensity_contiguous_rvv | IR + depth loop | `PointXYZI` | depth projection 与 intensity 写入同循环 | 当前没有 RVV intensity candidate，Phase 000 case 方向摇摆 | focused candidate correctness、asm、board repeated | deferred | 新 phase |
| mismatch_stride_mapping | 队列表风险项 | depth/image 分辨率不一致 organized cloud | 证明 production 的 stride mapping 是否能安全接入 | strided store 成本高，Phase 000 3/5 退化 | focused ablation 或真实 workload profile | attempted / deferred | 需要真实常见路径证据 |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | depth-only production probe | `PointXYZ` depth diagnostic 稳定 positive，且生产接入可以保持很窄。 | PI2-PI5 production detail correctness、asm、board repeated、Doctor 和用户确认已完成。 | completed |
| 000 | manifest / registry hardening | 已把 Phase 000 Doctor 从 summary-only 升级到 topic-local JSON manifest，并接入 registry freshness。 | production rerun 仍需 production direct manifest。 | high if production patch authorized |
| 020 | public-entry OpenNI2 evidence gap | production detail positive，但当前交叉依赖没有 OpenNI2 头/库。 | OpenNI2-enabled RISC-V 构建或等价 public-entry smoke。 | medium / blocked |
| 030 | RGB/RGBA generic diagnostic coverage | 用户询问泛型点型测试；源码中只有 RGB/RGBA 路径是模板入口。 | `PointXYZRGB` / `PointXYZRGBA` correctness 已通过；production 需要新的稳定 positive board 证据。 | completed for diagnostic |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| RGB/RGBA production dispatch | Phase 000 median 1.02x，接近阈值；维护成本高于当前证据收益。 | profile 显示 RGB overlay 是 bottleneck，或新 pack/store candidate 产生稳定 positive。 |
| IR production dispatch | 当前 IR candidate 没有 RVV 实现，board 方向摇摆并触发 Doctor Error。 | 先完成 focused IR candidate。 |
| mismatch production dispatch | Phase 000 median 0.98x 且 3/5 低于 1。 | 真实 workload 证明 mismatch 是关键路径，并有降低 stride store 成本的新方案。 |
| broad generic point-type expansion | `convertToXYZPointCloud` 返回具体 `PointXYZ`，当前 production patch 没有模板点型面；Phase 030 只补了 RGB/RGBA 模板诊断 correctness。 | RGB/RGBA 模板路径重新出现稳定 positive 证据后，另开 production probe；自定义点型需先补 traits/layout 审计。 |
