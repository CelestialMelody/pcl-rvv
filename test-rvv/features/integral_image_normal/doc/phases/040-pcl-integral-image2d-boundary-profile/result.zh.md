# Phase 040: PCL IntegralImage2D Boundary Profile Result

## 执行范围

本阶段按计划新增 exact PCL `IntegralImage2D<float, 3>` boundary profile（真实 PCL 积分图边界剖析）。
production（生产源码）没有修改，`features/include/pcl/features/impl/integral_image_normal.hpp` 的 diff 为空。

新增 bench label：

- `pcl_iin_diff_*`：同一 wrapper 内的 diff_x / diff_y buffer component（组件）。
- `pcl_iin_setinput_dxdy_*`：真实 PCL `IntegralImage2D<float,3>::setInput()`，分别对 dx / dy 调用。
- `pcl_iin_query_*`：真实 PCL `getFiniteElementsCount()` + `getFirstOrderSum()` query loop（查询循环）。
- `pcl_avg3d_profile_*`：diff-buffer + PCL setInput + PCL query 的 total diagnostic（总诊断）。

## 计划动作回填

| action | 状态 | 证据 |
| --- | --- | --- |
| A1 bench 新增 exact PCL profile helper | done | `src/bench_integral_image_normal.cpp` |
| A2 manifest case metadata | done | `script/generate_integral_image_normal_evidence_manifest.py`，`python3 -m py_compile` 通过 |
| A3 Makefile evidence refs | done | `Makefile` 新增 Phase 040 run label / doc refs / case-filter |
| A4 correctness / QEMU / asm / board / doctor / registry | done | `run_test_compare`、QEMU bench smoke、`dump_bench_rvv`、`run_board_integral_image_normal_repeated`、`evidence_status` |
| A5 文档同步 | done | 本 result、README、evaluation、benchmark/evidence、roadmap、matrix、queue row、Handoff |

## 验证命令

```bash
python3 -m py_compile test-rvv/features/integral_image_normal/script/generate_integral_image_normal_evidence_manifest.py
make -C test-rvv/features/integral_image_normal run_test_compare
make -C test-rvv/features/integral_image_normal USE_PCL_RVV10=1 TARGET_BENCH=bench_integral_image_normal_rvv build/riscv/bench_integral_image_normal_rvv
make -C test-rvv/features/integral_image_normal run_bench_std BENCH_ARGS=1
make -C test-rvv/features/integral_image_normal run_bench_rvv BENCH_ARGS=1
make -C test-rvv/features/integral_image_normal dump_bench_rvv
make -C test-rvv/features/integral_image_normal run_board_integral_image_normal_repeated
make -C test-rvv/features/integral_image_normal evidence_status
```

结果：

- correctness：Std / RVV 两侧各 4 个 gtest 通过。
- QEMU bench smoke（QEMU 小型验证）：`BENCH_ARGS=1` 只验证 label / checksum 输出形状，不作为性能证据。
- asm attribution（反汇编归属）：bench RVV asm 包含 `vlse32.v`、`vfsub.vv`、`vsetvli` 等 diff helper 指令；PCL `setInput` / query 边界按计划仍是标量边界。
- board evidence（板卡证据）：5-run repeated 完成。
- evidence registry：`fresh`。

## 板卡结果

当前 summary 路径：

- `log/board/repeated-summary.md`
- `log/board/evidence_manifest.json`
- `log/board/evidence_doctor.md`
- `log/evidence_registry.json`

Phase 040 exact PCL cases：

| case | mean | median | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `pcl_iin_diff_320x240` | 1.02x | 1.01x | 1.01x | 1.04x | 0/5 | weak_positive diagnostic |
| `pcl_iin_diff_641x481_tail` | 1.12x | 1.12x | 1.06x | 1.16x | 0/5 | positive diagnostic for diff-only component |
| `pcl_iin_setinput_dxdy_320x240` | 1.01x | 1.01x | 1.00x | 1.02x | 1/5 | neutral / near-threshold |
| `pcl_iin_setinput_dxdy_641x481_tail` | 0.99x | 0.95x | 0.54x | 1.45x | 3/5 | unstable / negative component |
| `pcl_iin_query_320x240` | 0.90x | 0.95x | 0.76x | 0.99x | 5/5 | negative component |
| `pcl_iin_query_641x481_tail` | 1.13x | 1.13x | 0.99x | 1.31x | 1/5 | weak_positive with warning |
| `pcl_avg3d_profile_320x240` | 1.01x | 1.01x | 1.00x | 1.02x | 1/5 | neutral / weak |
| `pcl_avg3d_profile_641x481_tail` | 1.04x | 1.04x | 0.98x | 1.12x | 2/5 | weak / unstable |

非 PCL profile 历史 cases 在本次复跑中也刷新为当前 summary：

- `map_prep_320x240` mean 5.52x；`map_prep_641x481_tail` mean 5.27x，仍稳定 positive。
- `avg3d_diff_641x481_tail` mean 1.35x，仍只支持 diff-only diagnostic。
- `avg3d_profile_641x481_tail` mean 1.02x，2/5 run 退化，仍是 needs_review / unstable。

## Evidence Doctor

Evidence Doctor 输出 `Errors=8 / Warnings=22 / Suggestions=8`。Error 不是 checksum 或脚本失败；
它们是退化频率信号，要求降级证据边界：

- `pcl_avg3d_profile_641x481_tail`：2/5 run 退化。
- `pcl_iin_query_320x240`：5/5 run 退化。
- `pcl_iin_setinput_dxdy_641x481_tail`：3/5 run 退化，并有 `0.54x` 到 `1.45x` 长尾。
- 旧 `profile_component_integral/query_*` 仍有多项退化频率 Error。

处理动作：

- 不扩大复跑预算；5-run 已足以把 Phase 040 归入 weak / unstable diagnostic。
- 不把 exact PCL total 的 mean 正向写成 production-ready。
- diff-buffer production patch 继续保持 `no-production-now`。

## Diagnostic-to-Production Mismatch Audit 回填

| question | result |
| --- | --- |
| evidence role | `production-shaped diagnostic`，比 Phase 030 更接近真实 PCL 积分图，但仍是 test helper |
| A/B boundary | `test helper`，Std/RVV 共享同一 wrapper；RVV 构建只在 diff-buffer helper 上有手写 RVV |
| 当前决策问题 | `implementation-shape`：diff-buffer 局部收益是否能穿过 PCL setInput/query 总链路 |
| 是否可外推 production | 不能外推为 production direct；只能说明 exact PCL boundary 下仍弱 / 不稳定 |
| comparison-boundary risk | 有。输入仍是合成 `XYZPadPoint`，不覆盖真实 `PointInT` traits、object state、fallback 或 public dispatch |
| weak / negative 时是否允许 bounded production probe | 不允许直接 diff-buffer production patch；map-prep PI1/PI2 gate 不受本阶段取消 |
| clean adoption 是否需要 production detail A/B | 需要；本阶段没有同 production boundary RVV-vs-RVV detail A/B |

## EvidenceDecision

Phase 040 结论是：

`weak/unstable exact-PCL production-shaped diagnostic / no-production-now` for average 3D gradient diff-buffer productionization。

这比 Phase 030 的测试专用积分图 profile 更接近真实 PCL `IntegralImage2D`，但没有反转结论：

- total profile 只有 near-threshold mean positive。
- 大图 tail 有 2/5 退化。
- component 里 `pcl_iin_query_320x240` 明确负向，`pcl_iin_setinput_dxdy_641x481_tail` 明显不稳定。

因此，本阶段不建议从 diff-buffer 进入 production integration loop（生产接入闭环）。
默认恢复入口仍是 Phase 010 / map-prep PI2 用户授权门禁；修改 production header 仍需用户明确确认。

## Continue / Stop Decision

本阶段闭合了 handoff 中的 exact PCL `IntegralImage2D` open question。继续当前 topic 的未阻塞默认动作仍是：

- 如果用户授权 production patch：回到 `010-pi1-production-integration-plan`，推进 map-prep PI2。
- 如果不授权 production patch：当前不建议继续围绕 diff-buffer 做 production 探针；可另开 follow-up 做更细的 PCL integral-image / query 性能归因，但它不是 `integral_image_normal.hpp` 的直接 production patch。

`stop_condition_hit`：继续到 production source 会扩大到未授权 PI2，因此当前生产路径停在用户确认门槛。
