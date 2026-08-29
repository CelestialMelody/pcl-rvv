# Phase 020 Result: direct intensity stride store

## 阶段结论

本阶段 production patch（生产补丁）已采纳。真实公开入口 `HarrisKeypoint2D<PointXYZI, PointXYZI>::compute()` 在 RVV 构建、organized dense input（有组织密集输入）且 `nonmax=false` 时命中 `responseRVV()`，内部像素使用 RVV 计算 Harris/Noble/Lowe/Tomasi response，并通过 `vsse32` 直接跨步写回 `PointOutT::intensity`。边界像素、非 dense 输入和非 RVV 构建保持标量路径。

本阶段同时发现并修复 `computeSecondMomentMatrix()` 的尺寸状态泄漏：函数内 `static const int width/height` 会在同进程连续处理不同尺寸图像时复用第一次输入尺寸。新增回归测试 `PublicComputeDoesNotReusePreviousImageSize` 先在 Std 构建失败，再在去掉 `static` 后通过。该修复属于当前 production patch 的 correctness（正确性）部分。

## 实现回填

| action | 结果 | 证据 |
| --- | --- | --- |
| production RVV helper | adopted；`responseHarris/Noble/Lowe/Tomasi()` 在 `__RVV10__ && __riscv_vector && input_->is_dense` 下调用 `responseRVV()` | `keypoints/include/pcl/keypoints/impl/harris_2d.hpp` |
| direct intensity stride store | adopted；interior pixels 直接 `vsse32` 写回 AoS intensity 字段，避免中间 response buffer 后拷贝 | `responseRVV()` |
| dimension state fix | adopted；`computeSecondMomentMatrix()` 每次从当前 `input_` 读取 width / height | `PublicComputeDoesNotReusePreviousImageSize` |
| manifest correctness policy | adopted；`checksum` 字段改为 tolerance-based semantic fingerprint（基于误差阈值的语义指纹），原始 response checksum 保留为 `raw_response_checksum` | `script/generate_harris_2d_evidence_manifest.py` |
| registry / freshness | adopted；phase020 summary / manifest / doctor 已登记并被文档引用 | `log/evidence_registry.json` |

## 验证结果

| 证据 | 命令 / 路径 | 结果 | 说明 |
| --- | --- | --- | --- |
| QEMU correctness | `make -C test-rvv/keypoints/harris_2d run_test_compare` | Std/RVV 均 7 tests passed | QEMU 只证明正确性和路径形状，不作为性能结论。 |
| asm attribution | `make -C test-rvv/keypoints/harris_2d check_harris_2d_rvv_asm` | pass | `bench_harris_2d_rvv.asm` 中记录 883 条 RVV asm lines。 |
| board repeated | `log/board/repeated_phase020_direct_intensity_stride_store_public_entry/summary.md` | 5-run，三组 case 均 B/A < 1 = 0/5 | 性能结论来自 Milkv-Jupiter 板卡 repeated 结果。 |
| Evidence Doctor | `log/board/repeated_phase020_direct_intensity_stride_store_public_entry/evidence_doctor.md` | Errors=0，Warnings=0，Suggestions=6 | Suggestions 是环境 metadata 和 binary identity 缺失，不阻塞当前弱正向采纳。 |

## 板卡结果

| case | median speedup | min | max | B/A < 1 | decision |
| --- | ---: | ---: | ---: | ---: | --- |
| `harris2d_harris_320x240` | 1.070x | 1.050x | 1.080x | 0/5 | weak_positive |
| `harris2d_tomasi_320x240` | 1.090x | 1.080x | 1.110x | 0/5 | weak_positive |
| `harris2d_noble_tail_641x481` | 1.180x | 1.180x | 1.190x | 0/5 | weak_positive close to positive threshold |

## Evidence Doctor 处理

当前 doctor 无 Error / Warning。Suggestions 说明 repeated summary 未记录 taskset、governor、freq、temperature 和 binary hash；本轮命令、run label、summary、manifest、doctor 和 registry 已足够支撑“当前 public RVV path 快于当前 public scalar path”的弱正向采纳。若后续出现方向反转或需要 release-grade 性能报告，应补充环境 metadata 和二进制身份。

## Phase Scope 与扩展队列

| 字段 | 内容 |
| --- | --- |
| validated_scope | `PointXYZI -> PointXYZI`，`Scalar=float`，organized dense public `compute()`，NMS disabled，Harris/Noble/Tomasi board cases，Lowe 由 correctness 覆盖。 |
| unvalidated_scope | NMS enabled、indices、non-organized input、其它点类型 / `IntensityT` accessor、`Scalar=double`、非标准 intensity layout、真实应用数据分布。 |
| point_type_expansion_queue | 若要扩大到泛型点类型，先读取 `doc-rvv/rvv/RVV Generic Point Type Strategy.zh.md`，补 intensity 字段 traits / offset / stride gate、fallback tests、public direct bench、asm、board 和 Evidence Doctor。 |
| phase_closeout_boundary | 关闭当前窄范围 production direct 条目；不关闭 NMS、泛型点类型或其它输入形态。 |

## 继续 / 停止决定

当前授权范围内没有未阻塞的进一步性能优化动作。`direct-IntensityT-rvv` 和 `nms-rvv` 都需要扩大 topic scope（主题范围）：前者涉及泛型点类型和 `IntensityT` accessor 策略，后者涉及排序、occupancy map（占用图）和输出顺序。默认停止在 production adopted / ready for review 状态；后续扩展应作为新 phase 或新 topic 启动。
