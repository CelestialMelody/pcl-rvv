# Phase 050 Result: Map-prep Production Probe

## 当前状态

Phase 050 已完成 PI2-PI5：`features/include/pcl/features/impl/integral_image_normal.hpp`
已经接入 map-prep 前缀 RVV production probe（生产探针），并完成 correctness（正确性）、
QEMU smoke（仿真器冒烟）、反汇编和板卡 5-run repeated。用户已在 PI5 明确确认采纳并保留当前
map-prep production patch；该 patch 现在是 adopted production behavior（已采用生产行为）。

## 实现结果

| action | artifact | result |
| --- | --- | --- |
| A1 production helper | `integral_image_normal.hpp` | 新增 `pcl::detail::integral_image_normal::initializeMapPrepStd` 与 `tryInitializeMapPrepRVV`；`__RVV10__` 且 `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>` 成立时尝试 RVV，否则自然 fallback 到 Std。 |
| A2 production direct correctness | `src/test_integral_image_normal.cpp` | 新增 `IntegralImageNormalProductionRVV.PublicComputeDistanceMapMatchesReference`，通过真实 `IntegralImageNormalEstimation<PointXYZ, Normal>::compute()` 对拍最终 distance map。 |
| A3 production direct bench | `src/bench_integral_image_normal.cpp` | 新增 `prod_compute_avg_depth_320x240` 与 `prod_compute_avg_depth_641x481_tail`，计时真实 public `compute()` 的 `AVERAGE_DEPTH_CHANGE` 路径。 |
| A4 manifest / doctor | topic-local script + Makefile | `prod_compute_*` 已登记为 `production_direct` / `public_compute`，Phase 050 board label 为 `integral_image_normal_phase050_map_prep_production_probe`。 |
| A5 docs | topic docs / phase docs / Handoff | 本 result、matrix、roadmap、evaluation、bench 文档、README、queue row 和 Handoff 刷新到 Phase 050 当前证据。 |

生产 patch 只覆盖 `computeFeature()` 中 depth-change map 和 distance-map initialization 前缀。
distance transform 两遍传播、normal 输出、`computeFeatureFull/Part()`、indices 路径、border policy
和 viewpoint flip（视点翻转）保持标量原逻辑。

## 验证命令

```bash
python3 -m py_compile test-rvv/features/integral_image_normal/script/generate_integral_image_normal_evidence_manifest.py
make -C test-rvv/features/integral_image_normal run_test_compare
make -C test-rvv/features/integral_image_normal USE_PCL_RVV10=1 TARGET_BENCH=bench_integral_image_normal_rvv build/riscv/bench_integral_image_normal_rvv
make -C test-rvv/features/integral_image_normal run_bench_std BENCH_ARGS=1
make -C test-rvv/features/integral_image_normal run_bench_rvv BENCH_ARGS=1
make -C test-rvv/features/integral_image_normal dump_bench_rvv
make -C test-rvv/features/integral_image_normal dump_test_rvv
make -C test-rvv/features/integral_image_normal run_board_integral_image_normal_repeated
make -C test-rvv/features/integral_image_normal evidence_status
```

`run_test_compare` 中 Std / RVV 两侧各 5 个 gtest 全部通过。QEMU bench 只用来确认 label、
checksum 和日志形状，不作为性能结论。反汇编在 RVV bench / test binary 中确认到 `vlse32`、
masked `vse8`、`vse32`、`vmseq`、`vfsub` 和 `vsetvli` 等目标指令。

## 板卡结果

证据路径：

| path | role |
| --- | --- |
| `test-rvv/features/integral_image_normal/log/board/repeated-summary.md` | 当前 5-run board repeated summary。 |
| `test-rvv/features/integral_image_normal/log/board/evidence_manifest.json` | Evidence Doctor 输入 manifest。 |
| `test-rvv/features/integral_image_normal/log/board/evidence_doctor.md` | Evidence Doctor 输出。 |
| `test-rvv/features/integral_image_normal/log/evidence_registry.json` | evidence registry，`make evidence_status` 显示 fresh。 |

关键 5-run 结果如下：

| case | min speedup | median speedup | mean speedup | max speedup | checksum | decision |
| --- | ---: | ---: | ---: | ---: | --- | --- |
| `map_prep_320x240` | 5.19x | 5.27x | 5.25x | 5.29x | `1.7621e+07` | positive diagnostic |
| `map_prep_641x481_tail` | 5.07x | 5.13x | 5.11x | 5.15x | `1.41569e+08` | positive diagnostic |
| `prod_compute_avg_depth_320x240` | 1.00x | 1.06x | 1.06x | 1.12x | `39192.8` | weak-positive production public |
| `prod_compute_avg_depth_641x481_tail` | 0.99x | 1.06x | 1.06x | 1.12x | `42918.4` | weak-positive production public |

production direct（真实生产入口）两项 mean / median 都为 1.06x，说明真实 public `compute()`
路径在当前板卡和当前 `PointXYZ -> Normal` / organized image / `AVERAGE_DEPTH_CHANGE` case 下有收益。
但 5-run 中两项各有 1 次低于 1x：320x240 的 B/A 序列为 `1.06x, 1.04x, 1.06x, 1.12x, 0.998x`，
641x481 tail 的 B/A 序列为 `1.06x, 1.04x, 1.06x, 1.12x, 0.991x`。因此结论不能写成无风险 clean positive。

## Evidence Doctor 处理

当前 Evidence Doctor 结果为 Errors=3 / Warnings=21 / Suggestions=8。

Errors 都来自非 production-direct 的 diagnostic / profile component case：
`pcl_iin_query_320x240`、`profile_component_diff_320x240` 和
`profile_component_query_320x240` 的退化频率。它们不指向 Phase 050 production map-prep patch
的 checksum 或 correctness 错误，但继续证明 average 3D gradient diff-buffer 不适合本轮生产化。

production-direct 两项触发 Warning 而非 Error：各有 1/5 run 低于 1。处理策略是：

- 保留完整 min / median / mean / max，不剔除第 5 轮。
- 把 `prod_compute_*` 判为 weak-positive production public，而不是 clean adopted。
- 用户已确认采纳；长期主题文档使用本阶段接入后的板卡数据。

## EvidenceDecision

| candidate family | decision | reason |
| --- | --- | --- |
| map-prep production probe | `adopted weak-positive production-public` | public `compute()` 真实入口 5-run median / mean 均 1.06x，checksum 一致；但各有 1/5 退化，采纳说明中保留风险。 |
| average 3D gradient diff-buffer | `no-production-now` | Phase 030/040 加上本轮 Evidence Doctor 继续显示完整 profile / component 不稳定，不扩大生产 patch。 |
| distance transform rewrite | `deferred` | 两遍传播有行内依赖和算法替换风险；当前收益已来自 map-prep 前缀，不建议在 PI5 前扩大范围。 |

## PI5 建议

用户已确认采纳当前 map-prep production patch。采纳理由是实现范围小、fallback 简单、正确性闭合、真实 public
入口已有板卡收益，且收益来自 Phase 000 中稳定 5x 级别的前缀优化。采纳时必须同步说明：
当前 production evidence 只覆盖 `PointInT` 满足 `RVVXYZAoSFloatLayout`、实际 board case 为
`PointXYZ -> Normal`、organized full image、`AVERAGE_DEPTH_CHANGE` 的 public `compute()` 路径；
其它点类型、indices 输出、其它 normal method 和 distance transform / normal output 不外推。

长期文档已创建在 `doc-rvv/features/integral_image_normal-RVV.zh.md`，并使用本阶段接入后的板卡数据作为
正式“正确性与高效性证据链”。

## 后续优化空间

当前 topic 内没有比 map-prep 更值得立即继续扩大 production patch 的性能方向：

- diff-buffer 已有多阶段证据，完整链路弱 / 不稳定。
- distance transform 是方向传播算法，直接 RVV 化需要算法证明和更高风险测试，不适合作为 PI5 前的连续推进。
- normal solver / output path 混合积分图查询、Eigen 解算、border/mirror policy 和 indices 语义，需要单独 profile 和生产数据流审计。

因此本阶段 closeout 后的停止条件是：当前 topic 内没有建议立即继续扩大 production patch 的方向。
