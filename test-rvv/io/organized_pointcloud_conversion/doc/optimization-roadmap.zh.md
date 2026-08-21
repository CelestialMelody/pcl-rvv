# organized_pointcloud_conversion Optimization Roadmap

## 当前边界

当前 topic 已完成 production adoption。Adopted 范围包括 `OrganizedConversion<PointT>::convert(cloud, ...)` 的 uncolored / colored cloud encode overload，以及 `OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud` 的 production-detail helper；decode overload 保持 scalar-only。Conversion 生产直连证据来自 public overload Std/RVV board repeated；analyze 证据来自 protected detail helper repeated board 和 after-patch full encode-shaped repeated。

当前仍不能声称真实 `OrganizedPointCloudCompression::encodePointCloud` public class entry 已经端到端加速，因为无 OpenNI cross build 下真实类不可直接实例化。已有 encode-shaped helper 证据显示接入 analyze detail 后形态接近的完整链路仍正向。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `pointxyz_z_strided_disparity_rvv_v1_per_vl_scratch` | Phase 000 | cloud -> disparity diagnostic | 批量 z / finite / divide | diagnostic boundary | correctness、bench、board、doctor | superseded_by_production | completed |
| `rgb_or_mono_color_pack_rvv_v1_fused_pack` | Phase 020 | colored cloud -> disparity + color | 避免二次 scalar finite scan | color byte 写回仍标量 | correctness、bench、board | superseded_by_production | completed |
| `decode_disparity_backprojection_rvv_v0` | Phase 030 | disparity -> cloud | x/y/depth 公式批量计算 | staging + AoS 写回退化 | decode tests、bench、board | rejected_negative_diagnostic | reopen only with new store/staging shape |
| `encode_production_probe_pointxyz_like` | Phase 040 / 050 | cloud encode public overload | conversion 阶段真实加速 | custom point types 未覆盖；full compression 未覆盖 | production direct correctness、asm、board、doctor | adopted | closeout completed |
| `end_to_end_encode_pointcloud_bench` | adoption 反思 | encodePointCloud-shaped helper | 验证局部 conversion 收益是否传递到压缩形态链路 | 不是真实 public class evidence | correctness smoke、bench labels、board repeated、doctor | attempted_positive_production_shaped | completed |
| `analyze_cloud_reduction_rvv` | 队列 companion file + Phase 060 反思 | `analyzeOrganizedCloud` max depth / focal length | production-detail helper 1.820x-3.908x，after-patch full shaped 1.119x-1.174x | 真实 public class direct 仍受 OpenNI 构建限制 | correctness、asm、production-detail board、after-patch shaped board、doctor | adopted_production_detail | closeout completed |
| `generic_point_type_expansion` | generic point strategy | more xyz-like / custom PointT | 扩大 production claim | traits 命中不等于性能覆盖；color fields 更复杂 | point-type tests、asm、board repeated、doctor | deferred | after user/reviewer prioritizes scope expansion |
| `color_byte_vector_pack` | Phase 050 反思 | colored RGB / mono path | 减少逐 lane byte writes | 需要公共 color load/store helper 或局部 intrinsics；当前 RGB 已正向 | RVV-vs-RVV same-boundary A/B | deferred | only after end-to-end or profile shows color pack hot |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 050 | full `encodePointCloud` 端到端 bench | 用户明确要求“接入后也需要测试，看是否真的值得接入”；当前 evidence 只是 conversion public overload | end-to-end output checksum/size、board repeated、doctor | high |
| 050 | `analyzeOrganizedCloud` 先作为 component ablation，不立即改 production | queue companion file 提到该 helper，但它是否主导要由端到端 profile / bench 判断 | end-to-end + component timing | medium |
| 050 | color byte vector pack 暂缓 | 当前 RGB production direct 已有 1.35x-1.43x；继续优化需新 helper 和 RVV-vs-RVV A/B | same-boundary colored A/B | low |
| 060 | analyze component ablation | encode-shaped 端到端约 1.15x，说明 conversion-only 收益被前后段稀释 | analyze same-chain correctness、board repeated、doctor | high |
| 070 | analyze production probe | analyze diagnostic dense / mixed-invalid median 3.548x / 3.483x | production boundary plan、真实 public/direct 或 detail helper 证据 | high |
| 080 | analyze adoption closeout | production-detail median 3.908x / 3.815x / 1.820x，after-patch full shaped 仍正向 | 长期 doc、evaluation、matrix、queue 同步 | high |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| `decode_disparity_backprojection_rvv_v0` | 板卡 diagnostic 0.85x-0.91x，staging 和 AoS 写回成本过高 | 出现能避免 per-VL x/y staging 或更优 output store 的候选 |
| generic PointT beyond representative types | 当前 repeated board 只覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` | 需要扩大生产声明或 reviewer 要求覆盖更多点型 |
| real public class direct `encodePointCloud` claim | no-OpenNI cross build 不能直接实例化真实 public class | 解除 OpenNI 构建限制或补可维护 public-entry test harness |
| color byte vector pack | 当前 colored production direct 已正向，新增维护成本较高 | profile / end-to-end 显示 color pack 是主要剩余瓶颈 |

## 默认恢复队列

1. `ready_for_review`：080/090 已关闭当前高优先级 production candidate；reviewer 默认审查 production diff、证据边界和文档一致性。
2. 若后续继续优化，第一优先级是解除真实 `OrganizedPointCloudCompression::encodePointCloud` public class direct 的 OpenNI 构建限制；否则不能把 shaped helper 证据升级为 public direct。
3. 若需要扩大覆盖范围，再开 point-type expansion phase；若 profile 或同边界 A/B 显示 color byte writeback 仍是瓶颈，再开 color byte vector pack phase。
