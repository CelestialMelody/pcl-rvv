# Phase 040 结果：production stabilization and point type expansion audit

## 实际执行范围

本阶段在用户确认保留 narrow `PointNormal` production patch 后继续推进。阶段内先完成三项接入后工作：一是补 non-`PointNormal` fallback correctness，二是重新跑 `run_test_compare` 验证 Std/RVV 两侧一致，三是把 production-direct board summary 从 2-run 刷新到 3-run。阶段执行当时第 4 轮前出现板卡 SSH timeout，因此把 5-run steady board 转入恢复动作；后续恢复已经补齐 5-run，当前 `production_direct_repeated` 是 historical narrow anchor。

## 动作回填

| action | status | evidence | result |
| --- | --- | --- | --- |
| production adoption docs | done | `doc-rvv/surface/marching_cubes-RVV.zh.md`、topic-local docs | Phase 030 已从 pending checkpoint 更新为 adopted narrow production。 |
| fallback correctness | done | `make run_test_compare` | 阶段内 Std/RVV 各 5 tests passed；后续 Phase 050 扩展为 Std/RVV 各 6 tests passed。 |
| QEMU correctness refresh | done | `make run_test_compare` | 阶段内生产直连和 fallback 语义保持一致；后续 generic 代表点型也已覆盖。 |
| asm refresh | done | `make dump_bench_rvv` | `bench_marching_cubes_rvv.asm` 仍可见 RVV 指令。 |
| production-direct board steady | done / historical anchor | `production_direct_repeated` | 后续恢复补齐 5-run：`5.330x/6.386x/8.824x`，继续作为 narrow historical anchor。 |
| Evidence Doctor | done / warning explained | `log/board/production_direct_repeated/evidence_doctor.md` | 当前 historical anchor 为 `Errors=0`、`Warnings=1`、`Suggestions=0`。 |
| generic point type audit | done in Phase 050 | `surface/include/pcl/surface/impl/marching_cubes.hpp`、`common/include/pcl/rvv_point_traits.h` | Phase 050 已把 gate 放宽为 `RVVXYZAoSFloatLayout<PointNT>` 并补齐四个代表点型 5-run repeated board。 |

## 当前 board 结果

| case | median | values | bucket | 解释 |
| --- | ---: | --- | --- | --- |
| `mc_prod_sphere_64` | `5.364x` | `5.364x, 5.443x, 5.324x` | positive | public path 仍稳定正向。 |
| `mc_prod_wave_72` | `6.388x` | `6.386x, 6.464x, 6.388x` | positive | public path 仍稳定正向。 |
| `mc_prod_sparse_sphere_80` | `8.840x` | `8.840x, 8.980x, 8.806x` | positive | sparse case 明显更快，但仍作为单 case 解释，不外推到所有输入分布。 |

## Evidence Doctor 处理

- `Errors=0`：当前没有阻塞接入的性能退化或 checksum mismatch。
- 当前 historical anchor 的 remaining warning 为 low_run_count / evidence role 边界提示；generic repeated board 已在 Phase 050 用四个代表点型无 finding 结果替代当前 gate 证据。
- 处理动作：保留 narrow production anchor 作为历史对照，不再把 Phase 040 timeout 当作当前阻塞。

## Generic point type 审计状态

阶段内生产 gate 曾是：

```cpp
if constexpr (std::is_same_v<PointNT, pcl::PointNormal>)
  reconstructSurfaceRVV (intermediate_cloud);
else
  reconstructSurfaceStd (intermediate_cloud);
```

这保证其它模板实例在 RVV build 下回退标量。后续 Phase 050 已完成 traits-gated expansion audit，并把当前生产 gate 更新为：

```cpp
if constexpr (pcl::rvv::RVVXYZAoSFloatLayout<PointNT>::value)
  reconstructSurfaceRVV (intermediate_cloud);
else
  reconstructSurfaceStd (intermediate_cloud);
```

`RVVXYZAoSFloatLayout<PointNT>` 在语义上覆盖了 `getBoundingBox()` 和 RVV prepass 所需的 xyz 输入条件；`PointXYZ` / `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` 代表点型已经补齐 correctness 和 5-run board repeated。该结论仍不外推到真实 Hoppe/RBF 输入分布或 triangle emission RVV。

## Stop / Continue 决策

阶段内曾命中外部 stop condition：板卡 SSH 连续 timeout。后续恢复已解除该阻塞，默认恢复动作是：

1. Phase 050 已完成 generic point type audit 和代表点型 repeated board。
2. 当前默认恢复动作是文档同步；若继续优化，进入 active-z tail / table-lookup compression A/B。
3. 真实 Hoppe/RBF 输入分布不由本阶段关闭。
