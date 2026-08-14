# Phase 003 result：production integration

## 结论

Phase 003 已完成，EvidenceDecision 为 `production_direct_positive`。RVV path 已接入
`IterativeClosestPoint::transformCloud`。入口现在只做 matrix cast、RVV 短路和
`pcl::registration::detail::transformCloudStandard` fallback 调用，并通过 production direct correctness、
板卡 correctness、production direct repeated benchmark 和反汇编归因。

## 生产接入边界

| 条件 | 决策 |
| --- | --- |
| `__RVV10__` | 只有 RVV build 编译 RVV helper。 |
| `Scalar=float` | 进入 RVV；`Scalar=double` 回退标量。 |
| `input.size() >= 32` | 进入 RVV；小规模回退标量。 |
| `PointSource` layout | 通过 `pcl::rvv::kRVVXYZAoSPointCompatible` 或 `kRVVXYZNormalPointCompatible`。 |
| runtime offsets | 必须等于 traits layout offset，否则回退标量。 |
| `source_has_normals_` | false 走 XYZ path；true 走 XYZ+normal path。 |
| Std fallback | 调用 `pcl::registration::detail::transformCloudStandard`，保留上游标量语义。 |

## 验证结果

| 证据 | 结果 | 路径 |
| --- | --- | --- |
| QEMU correctness | Std/RVV 各 12 tests passed。 | `test-rvv/registration/icp/log/qemu/run_test_std.log` / `test-rvv/registration/icp/log/qemu/run_test_rvv.log` |
| board correctness | RVV 12 tests passed。 | `test-rvv/registration/icp/log/board/run_test.log` |
| production board repeated | 5-run median：5.68x / 5.30x / 3.76x / 3.93x。 | `test-rvv/registration/icp/log/board/transform_cloud_repeated/summary.md` |
| Evidence Doctor | Errors=0，Warnings=2，Suggestions=0。 | `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_doctor.md` |
| manifest | `evidence_role=production_direct`，含 production boundary 和 asm boundary。 | `test-rvv/registration/icp/log/board/transform_cloud_repeated/evidence_manifest.json` |
| asm attribution | RVV 指令簇位于 production `transformCloud` 符号；Std fallback 有独立 `transformCloudStandard` 符号。 | `test-rvv/registration/icp/doc/asm-attribution.zh.md` |

## Doctor warning 处理

`PointXYZ 64K` 的 repeated speedup min/median/max 为 5.29x/5.68x/6.50x，Doctor 报
`long_tail_or_variance` 和 `group_outlier`。该 case 所有 run 都明显正向，且其它 case 没有跨方向或接近阈值，
因此本 phase 不扩大预算；结论保持 positive，并在 evaluation / topic doc 中保留这些 warnings。

## 后续状态

Phase 004 已补齐 doc-suite parity closeout，当前 topic 可进入 review。若 reviewer 希望进一步降低性能风险，
可追加 20-run board confirmation 或把 collector 改成一次部署、多次远端运行；这不是当前 production
adoption 的阻塞项。
