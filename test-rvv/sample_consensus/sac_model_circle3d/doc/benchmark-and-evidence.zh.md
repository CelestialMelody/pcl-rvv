# sac_model_circle3d Benchmark And Evidence

## Bench 入口

`bench_sac_model_circle3d` 的参数为：

```text
<size> <iterations> <warmup_iterations> [point_type]
```

默认 `BENCH_ARGS='65536 200 20'`，也可传 `PointXYZI`、`PointXYZRGB` 或 `PointXYZRGBA` 作为第四参数。日志打印 `case`、`size`、`iterations`、`warmup_iterations`、`point_type`、四个 timing、count 和 checksum（校验和），供 topic-local manifest wrapper 解析。

QEMU bench 只用于 log-shape（日志形状）smoke，不作为性能结论。当前 smoke 命令：

```bash
make -C test-rvv/sample_consensus/sac_model_circle3d run_bench_rvv BENCH_ARGS='1024 2 1' ALLOW_QEMU_BENCH_COMPARE=0
```

## 板卡和证据摘要

| target / artifact | path | role |
| --- | --- | --- |
| board repeated target | `make -C test-rvv/sample_consensus/sac_model_circle3d collect_projection_repeated_board_evidence` | 采集 5-run board logs；raw logs 默认不提交。 |
| production select board target | historical `collect_select_production_repeated_board_evidence` | 采集 `PointXYZ` 10-run production-public logs；当前已回滚，仅保留历史证据。 |
| point-type expansion board targets | historical `collect_select_xyzi_repeated_board_evidence`、`collect_select_xyzrgb_repeated_board_evidence`、`collect_select_xyzrgba_repeated_board_evidence` | 采集三种点型的 5-run production-public logs；当前已回滚，仅保留历史证据。 |
| manifest wrapper | `test-rvv/sample_consensus/sac_model_circle3d/script/generate_circle3d_board_evidence_manifest.py` | 解析当前 topic 的 board logs 和 asm metadata。 |
| manifest | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-manifest.json` | 有 warm-up 的 current repeated board summary。 |
| Evidence Doctor Markdown | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.md` | reviewer 可读异常检查。 |
| Evidence Doctor JSON | `test-rvv/sample_consensus/sac_model_circle3d/doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.json` | 可登记机器摘要。 |
| registry | `test-rvv/sample_consensus/sac_model_circle3d/log/evidence_registry.json` | 记录当前 summary artifacts 的 freshness。 |

## 当前结论边界

当前 board 证据分为 component ablation（组件消融）和 production-public（公开入口标量 / RVV）两层。Phase 000 的同一 RVV binary 内 public helper 与 test-only projection candidate 对比只支持进入生产探针；Phase 010/020 的 Std binary public overload vs RVV binary public overload 才用于接入后判断。

- `countWithinDistance`：B/A mean 0.5713，5/5 退化，Evidence Doctor 报 Error。
- `selectWithinDistance` test-only candidate：B/A mean 1.1294，5/5 正向，Evidence Doctor 无 finding。
- `selectWithinDistance` production-public `PointXYZ`：B/A mean 0.9748，median 0.9991，5/10 退化，Evidence Doctor 为 Errors=1、Warnings=1、Suggestions=0；不支持采纳，且生产 helper 已回滚。
- `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` production-public 扩展：mean 0.9117 / 0.8883 / 0.8828，三个 Evidence Doctor 均有 Error；已拒绝扩展并保留标量 fallback，当前只作为历史证据保留。

`log/board/repeated-20260828-phase000-circle3d-projection/run-*/` 是本机 raw logs，summary-only 默认策略下不进入提交候选，除非用户另行要求提交脱敏日志。
