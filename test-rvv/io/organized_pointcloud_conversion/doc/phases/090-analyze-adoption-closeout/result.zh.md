# Phase 090: analyze adoption closeout result

## 执行范围

本阶段把 080 `analyzeOrganizedCloud` production-detail patch 从 `pending_user_confirmation_adopt_production` 推进到 adopted production behavior（已采纳生产行为）。当前 goal 已给出采纳条件：板卡测试显示有收益即可接入，并以接入后的板卡测试数据更新正式文档。

生产源码没有新增扩大范围；本阶段只同步文档和状态。被采纳的生产行为是：

- 新增 `io/include/pcl/compression/impl/organized_pointcloud_compression_analysis.hpp`。
- `OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud` 委托到 `pcl::io::organized_compression_detail::analyzeOrganizedCloud`。
- RVV 构建下对 traits-compatible `PointT` 尝试 `analyzeOrganizedCloudRVV`，非 RVV、布局不满足或小范围不闭合时回退 `analyzeOrganizedCloudStd`。

## 证据回填

| evidence | result | 结论边界 |
| --- | --- | --- |
| correctness | `make run_test_compare`，本阶段 fresh rerun Std/RVV 各 14 个 TEST pass | 覆盖 conversion、decode diagnostic、encode-shaped smoke 和 analyze production-detail helper 对拍 |
| asm | `make check_production_rvv_asm`，本阶段 fresh rerun pass | production asm probe 同时命中 conversion 和 analyze detail RVV 指令 |
| production-detail board | `log/board/analyze_production_detail_repeated/summary.md` | `PointXYZ` dense 3.908x、`PointXYZ` mixed 3.815x、`PointXYZI` mixed 1.820x median |
| production-detail Doctor | `log/board/analyze_production_detail_repeated/evidence_doctor.md` | `Errors=0, Warnings=1, Suggestions=0`；warning 只禁止按组外推 |
| after-patch encode-shaped board | `log/board/full_encode_after_analyze_detail_repeated/summary.md` | 接入 analyze detail 后 full shaped median 1.119x / 1.147x / 1.174x，仍正向 |
| after-patch shaped Doctor | `log/board/full_encode_after_analyze_detail_repeated/evidence_doctor.md` | `Errors=0, Warnings=0, Suggestions=0` |

`PointXYZI` warning 的处理：`PointXYZ` fixture 的 z 单调递增，标量路径更频繁更新 max depth；`PointXYZI` fixture 的 z 周期重复，标量更新次数少，因此 speedup 较低。该 warning 不阻塞 `PointXYZI` 自身 positive，但禁止把 `PointXYZ` 的约 3.8x 外推到其它点型。

## Diagnostic-to-production mismatch audit

| question | result |
| --- | --- |
| evidence role | `production_detail` + `production_shaped_diagnostic` |
| A/B boundary | production detail helper；encodePointCloud-shaped helper |
| 当前决策问题 | `analyzeOrganizedCloud` detail RVV-vs-scalar 是否值得保留 |
| 是否可外推到 public entry | 不可直接外推；no-OpenNI cross build 仍不能实例化真实 public class |
| baseline mismatch 风险 | detail helper 不含 PNG / stream 外围；after-patch encode-shaped helper 已补上下文但仍不是 public class direct |
| clean adoption 是否需要更多证据 | 当前采纳范围是 production detail helper；若要声明完整 public class direct，需要解决 OpenNI / public-entry test |

## Doc suite closeout

| area | current shape scan | decision | evidence / next action |
| --- | --- | --- | --- |
| README navigation | 已更新当前结论、证据路径和默认恢复入口 | adopted | `README.zh.md` |
| testing overview | 已增加 analyze production-detail row 和证据白名单 | adopted | `doc/testing-overview.zh.md` |
| correctness tests | 已修正为 Std/RVV 各 14 个 TEST，新增 detail helper tests | adopted | `doc/correctness-tests.zh.md` |
| benchmark / evidence | 已增加 production-detail 和 after-patch shaped evidence | adopted | `doc/benchmark-and-evidence.zh.md` |
| optimization evidence | 已把 analyze detail production patch 写成 adopted | adopted | `doc/optimization-evidence.zh.md` |
| roadmap | 已关闭 080，记录剩余可选方向和恢复条件 | adopted | `doc/optimization-roadmap.zh.md` |
| test support code map | 已加入 production detail helper / asm probe / evidence role | adopted | `doc/test-support-code-map.zh.md` |
| phase index / matrix | 已增加 090 并更新 analyze matrix row | adopted | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` |
| evaluation | 已更新 EvidenceDecision、Traceability Map 和未覆盖范围 | adopted | `doc/organized_pointcloud_conversion-evaluation.zh.md` |
| production topic doc | 已更新长期 adopted production behavior 和证据链 | adopted | `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` |
| io queue | 已更新 module queue row | adopted | `doc-rvv/library-screening/io/io-function-evaluation-queue.zh.md` |

## Continue / stop decision

`continue_stop_decision=ready_for_review`。当前 topic 已采纳两个生产层级：

1. `OrganizedConversion<PointT>::convert(cloud, ...)` 的 encode conversion RVV path。
2. `OrganizedPointCloudCompression<PointT>::analyzeOrganizedCloud` 的 production-detail RVV path。

当前没有必须继续推进的高优先级、未阻塞性能候选。真实 `encodePointCloud` public class direct 证据受 no-OpenNI cross build 限制；color byte vector pack 需要 profile 或 same-boundary A/B 证明其仍是主瓶颈；更多 traits-compatible 点型扩展是范围扩展，需 dedicated point-type phase 后再声称覆盖。

下一轮默认动作是 reviewer 审查当前 diff 和证据边界。若用户要求继续拓展，优先顺序为：public class direct build unblock 审计、generic point type expansion、colored byte pack same-boundary A/B。

## Fresh verification

收口后已重新运行：

- `make run_test_compare`：Std build 14/14 pass，RVV build 14/14 pass。
- `make check_production_rvv_asm`：pass，production encode probe contains RVV instructions。
- `make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/analyze_production_detail_repeated`：comparisons=3，Doctor `Errors=0, Warnings=1, Suggestions=0`。
- `make run_production_repeated_evidence_doctor PRODUCTION_REPEATED_DIR=log/board/full_encode_after_analyze_detail_repeated`：comparisons=3，Doctor `Errors=0, Warnings=0, Suggestions=0`。
- `git diff --check -- <topic paths>`：无输出，exit 0。
