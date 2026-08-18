# Phase 007 Result：已完成，进入 correspondence ingress 后续阶段

## 当前结论

Phase 007 已完成 dual-indexed-cloud-pair 和 correspondence-pair 的 direct-gather
implementation-family comparison。板卡目标为 `Milkv-Jupiter`，采集预算为 5 runs、
每次 20 iterations、warm-up 5 iterations。三类 policy 使用同一 RVV binary、同一 C1/C2
公式、同一 Eigen 4x4 solve 和同一 checksum policy。

- source-indexed-cloud-pair：4K / 64K / 256K median B/A 为 `1.296x / 1.469x / 1.461x`，
  decision bucket 为 `positive`；
- dual-indexed-cloud-pair：4K / 64K / 256K median B/A 为 `1.892x / 1.219x / 1.709x`，
  decision bucket 为 `positive`；
- correspondence-pair：4K / 64K / 256K median B/A 为 `1.552x / 1.170x / 1.591x`，
  4K / 256K 为 `positive`，64K 为 `weak_positive`，policy overall 为 `weak_positive`。

三类 policy 的 staged/direct checksum 均一致。Evidence Doctor 为 `Errors=0`、
`Warnings=2`、`Suggestions=0`；两条 warning 都是 dual-indexed / correspondence 64K
相对各自 policy 组内其它 size 的离群提示，不是 checksum 或严格边界错误。

## 计划动作回填

| action | 状态 | 证据 |
| --- | --- | --- |
| A1 terminology | done | 新 bench dataset、活动文档和新增 case 使用 `ordered-cloud-pair`。 |
| A2 dual direct gather | done | `accumulateDualQuaternionRVVDualIndexedImpl` 和 `estimateDualQuaternionDualIndexedDirectCandidate`；Std/RVV direct 对拍通过，stats 证明 `used_gather=true`、`used_staging=false`。 |
| A3 correspondence direct gather | done | `estimateDualQuaternionCorrespondenceDirectCandidate` 将 query/match 映射为双侧 index stream 后调用 dual direct gather；非 identity correspondence 对拍通过。 |
| A4 family bench | done | `indexed-direct-gather-family-comparison` 输出 source / dual / correspondence 三类 staged/direct pair，9 个比较 checksum 一致。 |
| A5 board evidence | done | `log/board/indexed_direct_gather_family_comparison_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md`；registry 已登记。 |

## 证据边界和剩余风险

本阶段证明的是 test-rvv implementation family，不是 production direct。direct gather 只覆盖
标准布局、`PointXYZ` / `float`、稠密 synthetic row source；不覆盖 production dispatch、
其它点型、`Scalar=double` 或任意真实 correspondence 分布。

64K 离群 warning 不能单因归结为 gather。可能原因包括 query/match 展开、两侧 index
locality、AoS stride、gather、寄存器压力或后段 solver 稀释；当前没有 profile，因此只能
作为下一阶段的可验证假设。下一阶段已创建：
`008-correspondence-direct-index-stream`，先去掉 correspondence direct candidate 的两个
临时 `Indices` 构造，再做同边界 correctness、board 和 Evidence Doctor。

## 证据路径

- correctness：`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log`，Std/RVV 各 17 tests passed；
- board summary：`log/board/indexed_direct_gather_family_comparison_repeated/summary.md`；
- manifest：`log/board/indexed_direct_gather_family_comparison_repeated/evidence_manifest.json`；
- Evidence Doctor：`log/board/indexed_direct_gather_family_comparison_repeated/evidence_doctor.md`；
- registry：`log/evidence_registry.json`。
