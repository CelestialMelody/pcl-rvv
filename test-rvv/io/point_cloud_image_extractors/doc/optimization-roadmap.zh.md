# Optimization Roadmap

## 当前边界

本 topic 当前已在 `io/include/pcl/io/impl/point_cloud_image_extractors.hpp` 采纳三个窄范围
production path（生产路径）：RGB/RGBA exact 点型、intensity full-range exact `PointXYZI`、
label mono16 exact `PointXYZL`。公开 API 不变，未覆盖点型和未覆盖模式保持标量 fallback。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `rgb_u32_stride_unpack_v0` | 队列建议 + 当前源码 | `PointXYZRGB` / `PointXYZRGBA` RGB/RGBA unpack | 批量 field load 和 bit unpack，减少每点 `getFieldValue` 开销 | 当前逐 lane 写三字节，弱于 segment-store v1 | correctness、asm、board compare、Doctor | positive fallback implementation family; superseded by production v1 | closed |
| `scaling_float_stride_v0` | 队列建议 + 当前源码 | `PointXYZI::intensity` no/fixed/full-range | 批量 float load、乘法和转换 | full-range 仍需规约；float-to-uint16 语义和 FRM 需审计 | correctness、asm、board compare、Doctor | full-range rejected; fixed-factor weak-positive/deferred | phase 000 result |
| `scaling_reduction_v1` | Phase 000 负向归因 | `PointXYZI::intensity` full-range | 用 RVV min/max 规约减少 first-pass scratch store 和 lane scan | 泛型 intensity-like 点型未证明 | TDD correctness、`vfred*` asm、repeated board、Doctor | promoted to adopted production by Phase 080 | closed |
| `rgb_segment_store_v1` | image_yuv422 sibling 的 store 经验 | RGB/RGBA unpack 输出三通道 | 使用 vector store / segment store 降低逐 lane 写回 | 泛型 RGB-like 点型未证明 | 同边界 A/B、asm、board | promoted to adopted production by Phase 080 | closed |
| `normal_xyz_to_rgb` | 当前源码 | `PointNormal` normal_x/y/z -> rgb8 | 三个 float stride load + scale 到 byte | scratch 回写 + 逐 lane 转 `uint8_t` 可能抵消收益 | correctness、bench、asm、board | rejected within diagnostic boundary for v0 | 仅在新 normal 实现形态或单独 PI1 授权时恢复 |
| `label_mono16_stride_v0` | 当前源码 | `PointXYZL` label -> mono16 | `uint32` stride load + narrow store | random / Glasbey map 不覆盖；泛型 label-like 点型未证明 | correctness、bench、asm、board | promoted to adopted production by Phase 090 | closed |
| `production_probe` | Phase 010 / 020 / 040 / 050 / 080 / 090 证据 | 真实 extractor public entry | 已建立窄范围生产分流，公开入口板卡证据正向 | header-only 模板、field metadata、fallback 和 point type gate；只覆盖 exact 点型 | production-public repeated + Doctor | adopted for RGB/scaling/label mono16 | ready_for_review / commit decision |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | `rgb_segment_store_v1` | QEMU smoke 显示 v0 有逐 lane store，若板卡也负向需拆 store 成本 | 同边界 A/B + asm | high if v0 negative |
| 000 | `scaling_reduction_v1` | repeated board 显示 full-range v0 5/5 负向，且 first pass 仍有 scratch store + scalar lane scan | correctness、`vfred*` asm、repeated board、Doctor | high |
| 010 | `PI1 production_integration_plan` | reduction v1 和 RGB v0 都已有 positive diagnostic，下一步会触碰 production header | public entry scope、fallback、point type gate、production direct correctness/board/Doctor | requires user confirmation |
| 020 | `PI1 production_integration_plan` | RGB segment-store v1 比 v0 更强，scaling reduction v1 也稳定正向 | public entry scope、fallback、point type gate、production direct correctness/board/Doctor | requires user confirmation |
| 030 | doc-suite parity closeout | topic 已有多 candidate、多 bench label、repeated board 和 Evidence Doctor，下一轮 production probe 需要稳定阅读路径 | README、evaluation、role docs、phase result 和 artifact tracking scan | done |
| 040 | PI1 production integration plan | 需要先冻结 production patch 范围，避免把 diagnostic positive 直接写成 adopted | exact point type gate、fallback matrix、PI2-PI5 证据计划、用户确认边界 | done; closed by Phase 080/090 |
| 050 | PI2 gate policy test support | RED test 暴露 Phase 040 gate helper 缺口，先在测试边界固化窄范围策略 | `ProductionProbeGatePolicyMatchesPi1Plan`、Std/RVV correctness、文档边界同步 | done; closed by Phase 080/090 |
| 060 | `label_mono16` | normal v0 已在板卡 repeated 中稳定负向，当前 topic-local 仍有 label mono16 未阻塞候选 | label correctness、QEMU smoke、asm、board repeated、Doctor；保持 random / Glasbey 不覆盖 | done in Phase 070 |
| 070 | PI2 or label PI1 user gate | label mono16 诊断正向，但不在 Phase 040/050 已冻结 PI2 scope 内 | 用户确认是否只按原 PI2 接入 RGB/scaling，或扩大/另开 label production plan | done by Phase 090 |
| 080 | S11 production closeout | RGB/scaling production-public repeated board 均为 positive，Doctor 无 finding | 用户已确认有收益即可采纳；长期 `doc-rvv` 和筛选队列刷新 | done |
| 090 | label production closeout | label mono16 production-public weak-positive，Doctor 无 finding | 采纳 exact `PointXYZL` `COLORS_MONO`；RGB random / Glasbey 和 generic label-like 点型保持后续范围 | done; ready_for_review / commit decision |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| label random / Glasbey | `std::map` / `std::set` 状态和 LUT 分配不是首阶段主成本 | label mono 或 production profile 指向 label 路径 |
| production patch adoption | Phase 080 和 Phase 090 production-public evidence 已正向，用户确认有收益即可采纳 | 当前已采纳；若未来目标硬件或真实输入 profile 显示退化，再另开回归 phase |
| `scaling_float_stride_v0` full-range | repeated board median 0.91x，Evidence Doctor Error 指向 5/5 退化 | 仅在 profile 指向 production full-range 且有不同实现形态时恢复 |
| `normal_float_stride_v0` | Phase 060 repeated board median 0.61x，5/5 退化，Evidence Doctor 对 normal label 报 `Errors=1, Warnings=1` | 仅在出现不依赖 scratch 逐 lane 转换的新实现形态，或用户明确授权 normal production probe 时恢复 |
| label random / Glasbey | map/set、随机状态和 LUT 分配不是 mono16 诊断范围 | production profile 指向 RGB label color modes，且先写单独 phase plan |
