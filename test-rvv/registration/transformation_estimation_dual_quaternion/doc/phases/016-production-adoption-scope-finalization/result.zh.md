# Phase 016 Result：production adoption scope finalization

## 当前状态

Phase 016 已按用户授权完成 production scope（生产范围）收敛：保留
`ordered-cloud-pair`、`source-indexed-cloud-pair`、`dual-indexed-cloud-pair`
三类 production RVV path，移除 `correspondence-pair` production RVV dispatch。
correspondence 公开入口继续使用原 iterator 标量路径；test-rvv 诊断候选和历史负向 production
probe 证据保留。

本阶段结束状态是提交前检查点，不创建 commit。

## 实现结果

| action | 状态 | 事实 |
| --- | --- | --- |
| 移除 correspondence production RVV | done | production header 中只剩 ordered/source-indexed/dual-indexed 三个 RVV helper 和三个公开入口 dispatch；correspondence overload 不再进入 RVV helper。 |
| 调整 production path-hit test | done | RVV-only path-hit 保留三类 retained policy；correspondence path-hit 已删除；fallback gate 继续覆盖小规模、非 dense 和 `Scalar=double`。 |
| retained-only bench | done | `production-public-retained-row-sources` 只输出 ordered/source-indexed/dual-indexed 三类 public entry。 |
| retained-only manifest / registry | done | QEMU smoke、board summary、manifest、Evidence Doctor 和 registry 都使用 retained-only case-filter。 |

## 验证结果

| 验证 | 命令 / 路径 | 结果 | 证据边界 |
| --- | --- | --- | --- |
| correctness | `make run_test_compare` | Std `28/28`，RVV `32/32` | RVV 构建额外覆盖三类 production path-hit 和 fallback gate。 |
| QEMU smoke | `make run_qemu_smoke_evidence_doctor` | 9 comparisons；Errors=0，Warnings=9，Suggestions=0 | warnings 仅来自 `warmup_iterations=0`，本 smoke 只证明日志形状 / checksum，不证明性能。 |
| asm smoke | `make dump_bench_rvv` | asm dump generated | 作为 retained production public bench 的 RVV 指令归因输入。 |
| board retained repeated | `make run_board_bench_production_public_repeated` | 三类 retained policy 全部 positive | `Milkv-Jupiter`，5 runs，20 iterations，5 warm-up iterations。 |
| Evidence Doctor | `log/board/*production_public*_repeated/evidence_doctor.md` | 三类 retained summary 均 `0/0/0` | 支持提交前保留判断。 |

## Retained Board Evidence

| row source policy | 4K | 64K | 256K | board decision | Evidence Doctor |
| --- | ---: | ---: | ---: | --- | --- |
| `ordered-cloud-pair` | `3.232x` | `3.644x` | `3.656x` | `positive` | `0/0/0` |
| `source-indexed-cloud-pair` | `2.540x` | `2.631x` | `2.586x` | `positive` | `0/0/0` |
| `dual-indexed-cloud-pair` | `2.015x` | `1.808x` | `2.021x` | `positive` | `0/0/0` |

证据路径：

- `log/board/production_public_retained_row_sources_repeated/summary.md`
- `log/board/production_public_source_indexed_cloud_pair_repeated/summary.md`
- `log/board/production_public_dual_indexed_cloud_pair_repeated/summary.md`

Phase 015 的 `production_public_correspondence_pair_repeated` 保留为历史负向证据：
4K positive，但 64K / 256K negative，Evidence Doctor 为 `2/3/0`。该路径不再属于当前
production patch。

## EvidenceDecision

当前决策为 `ready_for_user_submit_or_cancel`：

- 建议提交当前三类 retained production RVV patch。
- correspondence production RVV 不保留；如要继续优化，应另开 correspondence 专项或等待真实 workload 分布证据。
- 若用户取消接入，需要回滚 production header、topic docs 和新增 `doc-rvv` 长期文档。

## 文档同步

已同步或本阶段要求同步的文档：

- `README.zh.md`
- `doc/testing-overview.zh.md`
- `doc/correctness-tests.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_dual_quaternion-evaluation.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc-rvv/registration/transformation_estimation_dual_quaternion-RVV.zh.md`
