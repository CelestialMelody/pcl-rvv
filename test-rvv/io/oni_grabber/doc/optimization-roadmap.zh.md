# ONI Grabber Optimization Roadmap

## 当前默认恢复动作

| phase | scope | status | blocker | next action |
| --- | --- | --- | --- | --- |
| `000-current-state-and-diagnostic-scaffold` | depth-only `PointXYZ` production-shaped diagnostic | `complete / positive` | none | 已推进到 Phase 010。 |
| `010-production-depth-probe` | depth-only `PointXYZ` production integration loop | `adopted production behavior / production-detail positive` | none for current scope | 当前无值得继续同轮推进的未阻塞优化方向；后续只在新 profile、ONI replay 场景或新候选出现时恢复。 |

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| depth-projection-rvv | `openni2_grabber` depth-only 同构经验 + 当前源码 | `convertToXYZPointCloud` depth-only `PointXYZ` | 板卡 diagnostic median `1.17x` | ONI replay reader 可能吞掉 public-entry 收益；Evidence Doctor 只有 summary-only metadata | correctness、QEMU smoke、asm、board repeated、Evidence Doctor | `superseded by production-probe` | Phase 010 已闭合 production-detail 证据 |
| rgb-rgba-ir-extension | 当前源码存在 RGB/RGBA/IR 类似循环 | RGB/RGBA/IR | 未知 | OpenNI2 RGB overlay median 仅 `1.02x`，IR median `1.01x` 且有低于 1 的 run；ONI 源码还要处理 packed color 和 `PointXYZI::data_c` 语义 | 新 profile 或 focused candidate 后再做 correctness、asm、board repeated、Doctor | `not_recommended_now` | 不作为当前下一 phase |
| production-probe | phase 000 positive + user adoption confirmation | production detail helper | production-detail board repeated median `1.18x` | summary-only metadata 不完整；缺完整 ONI replay public-entry 吞吐 | production-detail correctness、asm、board repeated、Evidence Doctor 已完成 | `adopted production-detail positive` | 已进入 production closeout，创建 `doc-rvv/io/oni_grabber-RVV.zh.md` |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| full ONI replay public-entry bench | 真实 ONI 文件读取和 replay 调度可能主导，且当前没有文件场景输入 | 用户提供 ONI replay 场景或 profile 证明 conversion 是主成本 |
| RGB/RGBA production dispatch | 同构 OpenNI2 RGB overlay repeated median `1.02x`，收益近阈值；ONI legacy 还需维护 RGB/RGBA packed field 语义。 | profile 显示 RGB overlay 是瓶颈，或新 pack/store candidate 产生稳定 positive。 |
| IR production dispatch | 同构 OpenNI2 IR repeated median `1.01x`，且有低于 1 的 run；当前没有 focused IR RVV candidate。 | 先完成 focused IR candidate，并单独跑 correctness、asm、board repeated 和 Doctor。 |
