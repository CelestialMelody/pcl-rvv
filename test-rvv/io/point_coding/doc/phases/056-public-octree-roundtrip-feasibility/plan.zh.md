# Phase 056：public octree roundtrip feasibility 计划

## 阶段意图和边界

本阶段只在 `test-rvv/io/point_coding/**` 内验证 public octree roundtrip（公开压缩/解压往返）诊断入口是否可构造，不修改 production 源码。这里的 public roundtrip 指真实 `pcl::io::OctreePointCloudCompression<PointXYZ>` 公开 `encodePointCloud` / `decodePointCloud` 调用链，包含 tree traversal（树遍历）、entropy coding/decoding（熵编码/解码）和 stream state（流状态）。

本阶段不尝试把 Phase 055 的 RVV helper 接入 production，也不把 public roundtrip 的 Std/RVV timing 写成 RVV 收益。原因是 Phase 056 当时尚未进入 production dispatch（生产分流）接入；Std/RVV 两个 build 走的是同一条 production 标量路径。Phase 056 只回答：能否在 test-rvv 中安全构造一个后续可扩展的 public roundtrip smoke（公开往返冒烟测试），以及它能不能作为后续 PI1 或更完整诊断的输入。

## 当前状态清单

| area | 当前状态 |
| --- | --- |
| production | `io/include/pcl/compression/point_coding.h` 未修改；`OctreePointCloudCompression` 仍通过 production 标量 `PointCoding` 完成 point coder encode/decode。 |
| Phase 055 | `decode_multileaf_*` 10-run median 全部大于 1，Evidence Doctor 为 `Errors=0，Warnings=3`；仍是 production-shaped diagnostic / no-production。 |
| 测试资产 | 当前 correctness 有 5 个 gtest，尚未覆盖 public roundtrip。 |
| 生产授权 | 未授权修改 production；PI1 仍需要用户明确授权。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness target | bench / board target | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| public octree roundtrip feasibility | public octree encode/decode stream | `PointXYZ` / production profile / AoS | production public API smoke in test-rvv | `make run_test_compare` 新增 roundtrip smoke | optional QEMU bench smoke only；不写性能结论 | not_applicable for RVV candidate | not_applicable unless repeated board is later added | planned feasibility |

## 实现和测试动作

1. 在测试支撑中增加 public roundtrip helper：构造 synthetic `PointXYZ` cloud，调用 `OctreePointCloudCompression<PointXYZ>` 的 `encodePointCloud` / `decodePointCloud`，返回输出点数和 checksum。helper 注释必须说明它不命中 RVV candidate。
2. 在 `src/test_point_coding.cpp` 增加 gtest：使用不做 voxel grid downsampling 的 profile，确认 public roundtrip 输出点数、height 和基本有限值状态可用。
3. 可选增加极小 QEMU bench smoke case `octree_roundtrip_public_256`，只证明日志形状和入口可运行，不参与 board performance 结论。
4. 更新 topic-local docs、roadmap 和 matrix，明确 Phase 056 是 feasibility，不改变 no-production 决策。

## Evidence Doctor 和 registry 规则

本阶段默认不跑 repeated board，也不生成 Evidence Doctor 性能结论。若后续人为对 public roundtrip 跑板卡 repeated summary，必须先把 evidence role 标为 `production-public no-rvv-dispatch-baseline` 或等价降级角色，避免误写成 RVV 收益。

## 诊断到生产错配审计

| question | answer |
| --- | --- |
| evidence role | feasibility smoke；不是 RVV performance evidence。 |
| A/B boundary | public API smoke；Std/RVV build 均走 production 标量路径。 |
| 当前决策问题 | public roundtrip 是否可作为后续完整 context 诊断入口。 |
| diagnostic 是否可外推到 production | no。它只证明入口可构造，不证明 RVV 接入或收益。 |
| comparison-boundary / baseline mismatch 风险 | yes。如果拿 Std/RVV timing 做收益，会把同一路径编译差异误写成 RVV 收益。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不由本阶段决定；仍需用户授权 PI1。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes。当前无 production patch。 |

## 继续 / 停止条件

如果 public roundtrip smoke 不能稳定构造，本阶段停在 no-production diagnostic，并记录阻塞原因。若 smoke 通过，下一步仍不能直接 PI2；默认动作是停在 `public roundtrip feasible / production authorization required for PI1`，或在用户明确授权时写 `060-production-integration-plan`。
