# NDT Correctness Tests

本文解释 `src/test_ndt.cpp` 中的 correctness（正确性）TEST。它不承担性能结论，也不证明 production direct（真实生产路径）已经存在。

## 测试文件分工

| 文件 | 角色 |
| --- | --- |
| `include/impl/ndt_fixtures.hpp` | 构造 deterministic SoA（确定性数组结构）样本，包括 `x_trans`、`c_inv`、Jacobian 和 point Hessian |
| `include/impl/ndt_references.hpp` | 标量 reference（参考链路），复刻 `updateDerivatives` 数学公式 |
| `include/impl/ndt_candidates.hpp` | test-only RVV diagnostic candidate（测试专用 RVV 诊断候选） |
| `src/test_ndt.cpp` | gtest 入口和误差验收 |

## TEST 字典

| TEST | 输入 | 断言 | 证明范围 | 不证明什么 |
| --- | --- | --- | --- | --- |
| `DenseHessianMatchesScalarReference` | 8192 个 staged sample，`compute_hessian=true` | score、6 维 gradient、6x6 hessian 接近 reference；RVV 构建命中 RVV | 大样本 hessian 数学核正确性 | 真实邻域搜索、line search、生产性能 |
| `TailHessianMatchesScalarReference` | 1031 个 sample | 非 2 的幂规模尾段一致 | RVV tail（尾段）处理 | 泛型点类型和 production fallback |
| `GradientOnlyPathKeepsHessianZero` | 2049 个 sample，`compute_hessian=false` | gradient 一致，hessian 全 0 | line search 中 gradient-only 路径 | hessian 性能收益 |
| `EmptyInputKeepsFallbackShape` | 0 个 sample | score/gradient/hessian 全 0，不声称 RVV 命中 | 空输入 defensive boundary（防御性边界） | production 空点云语义 |

## 验证命令和当前结果

```bash
make -C test-rvv/registration/ndt run_test_compare
make -C test-rvv/registration/ndt run_board_test_smoke
```

当前 QEMU Std/RVV 对拍各 4 个 TEST 通过；板卡 RVV smoke 4 个 TEST 通过。日志路径见 `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` 和 `log/board/test_smoke/run_test.log`。
