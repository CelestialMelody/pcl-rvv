# Phase 066 Result: Scalar Double Adoption Closeout

## 结论

用户已确认 Phase 065 的正向 production probe 可以接入。本阶段把 `scalar-double-production-probe` 收口为：

```text
adopted-by-user / scalar-double-production-probe / ordered-cloud-pair / PointXYZ -> PointXYZ / Scalar=double / dense / nr_points >= 16
```

这是一条有界 production behavior（生产行为），不是 `Scalar=double` 泛型结论。row-source double、泛型 xyz AoS double、自定义 layout double、非 dense、小规模和非法输入仍不继承本阶段证据。

## 采纳范围

| item | adopted scope | fallback / not covered |
| --- | --- | --- |
| public entry | ordered-cloud-pair public overload | source-indexed、dual-indexed、correspondence double 不覆盖。 |
| point type | exact `pcl::PointXYZ -> pcl::PointXYZ` | `PointXYZI`、`PointXYZRGB`、custom xyz AoS double 不覆盖。 |
| Scalar | `Scalar=double` | `Scalar=float` 继续走既有 adopted RVV family；其它 scalar 不覆盖。 |
| layout | dense `PointXYZ` AoS，float xyz 字段加载后扩宽到 f64 accumulation | 非 dense、小规模、非法输入 fallback。 |
| size | `nr_points >= 16`；board evidence 为 64K | 小规模 fallback，只证明 fallback correctness。 |

## 证据回填

| evidence | result | path |
| --- | --- | --- |
| correctness | Std/RVV 各 27/27 passed；新增 production-facing double tests。 | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| QEMU smoke | `scalar-double-production-probe` Doctor `Errors=0`、`Warnings=0`、`Suggestions=0`；QEMU timing 不作为性能结论。 | `log/qemu/scalar_double_production_probe/evidence_doctor.md` |
| ASM attribution | RVV bench asm 输入可见 `vfwcvt.f.f.v`、`vfmul.vv`、`vfredosum.vs`、`vsetvli e64,m1` 等 f64 widened accumulation 特征。 | `build/asm/riscv/bench_transformation_estimation_svd_scale_rvv.asm` |
| board repeated | 5-run B/A `33.955, 33.664, 33.781, 34.077, 33.792`，median `33.792x`，bucket `positive`，max public error `9.858780e-14`，checksum match。 | `log/board/scalar_double_production_probe_repeated/summary.md` |
| board Evidence Doctor | `Errors=0`、`Warnings=0`、`Suggestions=0`。 | `log/board/scalar_double_production_probe_repeated/evidence_doctor.md` |

Phase 065 的 board summary 仍写有 “PI5 后需用户确认” 的生成时说明；本阶段 result 是该 PI5 确认后的 adopted closeout 记录。

## 文档同步

| document area | result |
| --- | --- |
| phase index | 默认恢复入口从 Phase 064 / 065 更新为 Phase 066 adoption closeout。 |
| optimization matrix | 新增 / 更新 `scalar-double-production-probe` 行为 `adopted-by-user`。 |
| optimization roadmap | `Scalar=double production RVV` 不再是暂缓项；新增后续独立方向：row-source double、generic xyz AoS double、自定义 layout double。 |
| topic-local README / evaluation / evidence docs | 已把 ordered double production branch 写成 adopted，并保留不覆盖范围。 |
| long-term `doc-rvv` | 已加入 ordered `Scalar=double` adopted branch、fallback 矩阵和证据链。 |

## 继续 / 停止判断

- `continue_stop_decision`: `complete_adoption_closeout`
- `next_phase_default`: 若继续优化，默认从 `scalar-double-row-source-diagnostic-or-probe` 或 `scalar-double-generic-xyz-aos-expansion` 中选择一个独立 phase；二者都必须重新建立 correctness、QEMU、ASM、board 和 Evidence Doctor 边界。
- `turn_stop_deferred`: not applicable；本阶段没有新增未完成的 adoption closeout 项。
