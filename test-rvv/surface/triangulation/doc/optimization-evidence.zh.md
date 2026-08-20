# optimization evidence

## `param_grid_rvv_store`

实现位置：`include/triangulation.h::createParamGridCandidate`。

RVV build 在 `__RVV10__` 下使用 `vid.v` 生成 lane index，`vfcvt.f.xu.v` 转 float，`vfmacc.vf` 生成规则参数，再用三条 `vsse32.v` 对 `PointXYZ` 的 x/y/z 字段做 AoS stride store。非 RVV build 回到 scalar reference。

证据摘要：

| gate | evidence |
| --- | --- |
| correctness | `make run_test_compare`，Std / RVV 均 2 tests passed。 |
| asm attribution | `make dump_bench_rvv`，`bench_triangulation_rvv.asm` 中存在 `vsetvli e32,m2`、`vid.v`、`vfcvt.f.xu.v`、`vfmacc.vf`、`vsse32.v`。 |
| board smoke | `tri_param_grid_512` 为 `0.998x`，方向为 negative / neutral 边界。 |
| repeated board | `tri_param_grid_512` median `0.978x`，5-run 中 3/5 退化，Evidence Doctor 为 `Errors=1`。 |
| full-path smoke | `tri_surface_eval_256` 为 `1.005x`，方向为 neutral。 |
| full-path repeated board | `tri_surface_eval_256` median `0.999x`，5-run 中 3/5 退化，Evidence Doctor 为 `Errors=1`。 |

EvidenceDecision（证据决策）：不进入 production。当前 candidate 在参数网格局部没有稳定正向信号，在测试专用 full-path 中也被后续求值成本稀释。用户已说明忽略 on_nurbs 依赖相关方向后，当前 roadmap 中没有仍值得推进的 RVV next phase（下一阶段）。
