# GICP Benchmark 与证据

## Bench 合同

`src/bench_gicp.cpp` 输出 `Dataset`、`Iterations`、`Warmup Iterations`、case 平均耗时、checksum 和 `Total Time`。case-filter 包含：

| case-filter | case | 计时边界 | 不能证明 |
| --- | --- | --- | --- |
| `residual` | `residual-mahalanobis-dense`、`residual-indexed-gather` | residual / Mahalanobis dense-row 与 production-like indexed-gather 组件 | GICP public entry end-to-end |
| `covariance` | `covariance-post-knn-default-k` | nearestKSearch 之后的 mean / covariance loop | KdTree search 和 3x3 SVD |
| `dfddf` | `dfddf-loop-dense` | `dfddf()` per-correspondence accumulator loop | production indexed gather 和完整 public entry |
| `production` | `production-public-align-pointxyz` | 每次迭代构造 GICP 对象、设置 source/target 并调用 public `align()`；点云构造不计时 | 泛型点类型、其它 source/target 组合和其它 `Scalar` |
| `all` | residual、indexed residual、`dfddf`、covariance 组件 | 宽口径诊断 smoke | production-ready |

## 板卡复跑预算

默认 repeated board（重复板卡测试）预算是 5 runs、80 iterations、10 warmup。decision bucket（决策桶）规则：

| 条件 | bucket |
| --- | --- |
| median speedup >= 1.20 且没有 B/A < 1 | `positive` |
| median speedup >= 1.05 且 B/A < 1 不超过 1/5 | `weak_positive` |
| median 在 [0.95, 1.05) | `neutral` |
| median < 0.95 | `negative` |
| 跨桶摇摆或 Evidence Doctor Error 未解决 | `unstable` / `blocked` |

QEMU bench smoke 只用于 build / log-shape（日志形状）。性能结论只引用 `log/board/component_diagnostic_repeated/summary.md` 和对应 Evidence Doctor。

## 已登记证据

| evidence | path | role | result |
| --- | --- | --- | --- |
| QEMU Std correctness | `test-rvv/registration/gicp/log/qemu/run_test_std.log` | correctness | current aggregate 10 tests pass |
| QEMU RVV correctness | `test-rvv/registration/gicp/log/qemu/run_test_rvv.log` | correctness | current aggregate 10 tests pass |
| QEMU RVV bench smoke | `test-rvv/registration/gicp/log/qemu/run_bench_all_rvv.log` | smoke only | build/log shape ok |
| QEMU asm dump | `test-rvv/registration/gicp/build/asm/riscv/bench_gicp_rvv.asm` | asm smoke | RVV instructions present in bench binary |
| QEMU Evidence Doctor | `test-rvv/registration/gicp/log/qemu/evidence_doctor.md` | reviewer aid | `missing_comparisons` expected for raw smoke log |
| Board repeated summary | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/summary.md` | diagnostic performance | dense residual `1.347x` positive；indexed residual `1.255x` positive；covariance `1.151x` weak_positive |
| Board Hessian loop summary | `test-rvv/registration/gicp/log/board/dfddf_loop_dense_repeated/summary.md` | diagnostic performance | `dfddf-loop-dense` median `1.410x` positive |
| Board production public summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_repeated/summary.md` | production-public performance | `production-public-align-pointxyz` median `1.019x` neutral |
| Board production public 4096 quick summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_4096_repeated/summary.md` | production-public expansion | cost-only median `1.011x` neutral；3-run only |
| Board dfddf production public summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_repeated/summary.md` | production-public performance | `dfddfLoopRVV` path median `1.068x` weak_positive |
| Board dfddf production public 4096 summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_4096_repeated/summary.md` | production-public expansion | median `1.070x` weak_positive；3-run only |
| Board dfddf clean production public summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_repeated/summary.md` | production-public performance | clean adoption 1024 median `1.080x` weak_positive |
| Board dfddf clean production public 4096 summary | `test-rvv/registration/gicp/log/board/production_public_align_pointxyz_dfddf_clean_4096_repeated/summary.md` | production-public expansion | clean adoption 4096 median `1.058x` weak_positive；3-run only |
| Board manifest | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_manifest.json` | evidence metadata | 3 comparisons |
| Board Evidence Doctor | `test-rvv/registration/gicp/log/board/component_diagnostic_repeated/evidence_doctor.md` | diagnostic quality gate | Errors=0、Warnings=5 |

`test-rvv/registration/gicp/log/board/test_smoke/run_test.log` 记录板卡 smoke correctness，但不在 registry summary-only 白名单内作为提交证据。`test-rvv/registration/gicp/log/evidence_registry.json` 记录证据 freshness（新鲜度）状态。

## 实现迭代记录

上一版 residual 使用 `f64m1` 时出现严重退化；本轮将 residual dense-row 改为 `f64m4`，降低 per-chunk reduction 次数后转为 `positive`。随后补充 `residual-indexed-gather`，用 RVV indexed load 模拟 `tmp_idx_src_` / `tmp_idx_tgt_` 的生产式索引访存，仍保持 `positive`。covariance post-KNN 尝试 `f64m4` 会退化，因此当前组合保留 covariance `f64m1`。bench checksum 现在混入 iteration salt，避免偶数次重复 XOR 让 case checksum 归零。

Phase 001 生产公开入口 cost-only 探针为 `neutral`，不建议单独采纳。Phase 003 清理 cost-only
helper 后，`dfddfLoopRVV()` 生产补丁重新复测，public GICP `PointXYZ -> PointXYZ` 在 1024 点
5-run repeated 中 median `1.080x`，4096 点 3-run 扩展中 median `1.058x`，均为
`weak_positive`。Phase 004 的 gather width 微调没有改善。最终按用户确认回退生产源码，当前
summary 作为 no-production 决策证据保留，不再表示当前生产路径。
