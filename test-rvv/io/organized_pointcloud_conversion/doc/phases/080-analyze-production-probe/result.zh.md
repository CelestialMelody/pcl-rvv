# Phase 080: analyze production probe result

## 执行范围

本阶段把 `analyzeOrganizedCloud` 从 070 的 diagnostic（诊断）候选推进到 production-detail（生产 detail 边界）探针。实现没有改变 public API（公开接口），但新增了 production detail helper，并让 `OrganizedPointCloudCompression::analyzeOrganizedCloud` 委托到该 helper。

由于当前 no-OpenNI cross build 无法实例化 `OrganizedPointCloudCompression` public class，本阶段证据不能写成完整 public entry direct。它证明的是 `analyzeOrganizedCloud` runtime detail boundary 的 Std/RVV 对比，以及 encodePointCloud-shaped helper 中包含该 detail dispatch 后仍保持正向。

## 代码和测试动作

| action | 状态 | 产物 / 命令 | 结论 |
| --- | --- | --- | --- |
| RED test | completed | `src/test_organized_pointcloud_conversion.cpp` 先 include 缺失 helper，`make run_test_compare` 编译失败 | 失败原因命中缺少 production detail helper |
| production helper | completed | `io/include/pcl/compression/impl/organized_pointcloud_compression_analysis.hpp` | `Std` helper 保留原标量 loop；`__RVV10__` 下 traits-gated RVV helper，否则 fallback |
| protected wrapper | completed | `organized_pointcloud_compression.hpp` | `analyzeOrganizedCloud` 委托到 detail helper，public API 不变 |
| correctness | completed | `make run_test_compare` | Std/RVV 各 14 个 TEST pass |
| asm | completed | `make check_production_rvv_asm` | production asm probe 包含 conversion 和 analyze detail RVV 指令 |
| QEMU bench smoke | completed | `make run_bench_std/run_bench_rvv BENCH_ARGS='--iterations 1 --warmup-iterations 0 --case-filter production_analyze_detail_*'` | 新 label 可运行，checksum 匹配；QEMU timing 不作性能结论 |

## Board evidence

Production-detail repeated（5-run，30 iterations，5 warmup）：

| case | min | median | max | 证据 |
| --- | ---: | ---: | ---: | --- |
| `production_analyze_detail_pointxyz_dense_307k` | 3.783x | 3.908x | 3.947x | `log/board/analyze_production_detail_repeated/summary.md` |
| `production_analyze_detail_pointxyz_mixed_invalid_307k` | 3.739x | 3.815x | 3.846x | 同上 |
| `production_analyze_detail_pointxyzi_mixed_invalid_307k` | 1.798x | 1.820x | 1.840x | 同上 |

Evidence Doctor：`log/board/analyze_production_detail_repeated/evidence_doctor.md`，`Errors=0, Warnings=1, Suggestions=0`。

Warning 是 `PointXYZI` group outlier。解释：`PointXYZ` organized fixture 的 depth 单调递增，标量路径几乎每个 finite 点都更新 `maxDepth/focalLength`；`PointXYZI` fixture 的 depth 周期重复，标量更新次数少，因此组件 speedup 低于 `PointXYZ`。该 warning 不阻塞 `PointXYZI` 自身 1.82x 正向结论，但禁止把 `PointXYZ` 的 3.8x 外推到 `PointXYZI` 或其它点型。

After-patch full encode-shaped repeated（5-run）：

| case | min | median | max | 证据 |
| --- | ---: | ---: | ---: | --- |
| `production_full_pointxyz_encode_dense_307k` | 1.096x | 1.119x | 1.140x | `log/board/full_encode_after_analyze_detail_repeated/summary.md` |
| `production_full_pointxyzrgb_encode_rgb_dense_307k` | 1.137x | 1.147x | 1.165x | 同上 |
| `production_full_pointxyzrgb_encode_mono_dense_307k` | 1.168x | 1.174x | 1.196x | 同上 |

Evidence Doctor：`log/board/full_encode_after_analyze_detail_repeated/evidence_doctor.md`，`Errors=0, Warnings=0, Suggestions=0`。

## Diagnostic-to-production mismatch audit

| question | result |
| --- | --- |
| evidence role | `production_detail` + `production_shaped_diagnostic` |
| A/B boundary | production detail helper；encodePointCloud-shaped helper |
| 当前决策问题 | RVV-vs-scalar production detail 是否值得保留 |
| 是否可外推到 public entry | 不可直接外推；no-OpenNI cross build 仍无法实例化真实 public class |
| baseline mismatch 风险 | detail helper 不覆盖 PNG / stream 外围；full encode-shaped 已补上下文但仍不是真实 public class |
| clean adoption 是否需要更多证据 | 需要用户确认；若要称完整 public entry direct，还需解决 OpenNI class build 或上游 public-entry test |

## EvidenceDecision

当前证据支持保留 analyze production-detail patch 作为 production candidate（生产候选）：correctness 通过、asm 通过、board repeated 正向、Doctor 无 Error，full encode-shaped context 仍正向。

本阶段完成时命中 PI5 user checkpoint（生产证据决策后的用户确认点）：按照 workflow，当时不能自行把新 analyze patch 写成 adopted production behavior，也不能自行回滚。后续当前 goal 已确认“正收益即可接入”，采纳收口见 `../090-analyze-adoption-closeout/result.zh.md`。

## 下一步

默认下一动作已由 `090-analyze-adoption-closeout` 完成：刷新长期 `doc-rvv` 的 adopted 状态、queue row、evaluation 和 topic-local doc suite。若将来要声称真实 public class direct，还需另开 phase 解决 OpenNI / public-entry test。
