# bfgs 正确性测试说明

## 本文职责

本文记录 Phase 010 已实现的 correctness（正确性）测试族。测试源码位于 `src/test_bfgs.cpp`，通过 `include/bfgs.h` 聚合入口使用 test-only fixtures（测试专用输入样本）、reference（参考链路）和 candidate（候选 helper）。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `include/bfgs.h` | 稳定聚合入口，test / bench 只 include 这里。 |
| `include/impl/bfgs_fixtures.hpp` | 固定二次函数、固定输入向量和 cache 覆盖样本。 |
| `include/impl/bfgs_references.hpp` | Eigen baseline reference（参考链路）和只测试使用的同构状态更新 helper。 |
| `include/impl/bfgs_candidates.hpp` | test-only diagnostic candidate（测试专用诊断候选），不进入 production。 |
| `src/test_bfgs.cpp` | gtest correctness、boundary 和 caller-shaped smoke。 |

## 共同输入和断言

输入使用固定样本：

- `Vector6d`：匹配 GICP caller 的真实 BFGS 维度。
- 128 维连续 double 向量：只用于证明 VL chunk（可变向量长度分块）循环。
- `dxdg == 0`、空输入和固定 `alpha`：只用于边界和 fallback shape。

共同断言覆盖：

- `x_alpha`、`g_alpha`、`p`、`fp0` 等状态在 baseline 和 candidate 间保持一致。
- `dxdg == 0` 时不除零。
- 空输入不声称 RVV 命中。
- `BFGS<QuadraticFunctor>` 的 public API 可运行且状态有限。

## TEST / 测试族字典

| TEST / 测试族 | 层级 | 输入 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `BFGSDiagnostic.DirectionUpdateVector6MatchesScalarReference` | diagnostic correctness | GICP 形态 `Vector6d` | direction update 全部中间量和最终 `p/fp0` 与标量 reference 一致 | 局部 BFGS 状态更新同构 |
| `BFGSDiagnostic.DirectionUpdateLongVectorMatchesScalarReference` | diagnostic correctness | 128 维连续 double 向量 | 同上 | RVV VL chunk 循环同构 |
| `BFGSDiagnostic.DirectionUpdateZeroDxdgBoundaryMatchesScalarReference` | boundary / regression | 构造 `dxdg == 0` | `A/B` 保持 0，结果与 reference 一致 | 除零边界 |
| `BFGSDiagnostic.DirectionUpdateEmptyInputKeepsScalarFallbackShape` | boundary | 空向量 | 结果为空且 `used_rvv == false` | 无 lane 输入的 fallback shape |
| `BFGSDiagnostic.MoveToAndSlopeMatchesScalarReference` | numerical consistency | 64 维连续 double 向量 | `x_alpha` 和 slope 与 reference 一致 | `moveTo(alpha)` + `slope()` 局部链路 |
| `BFGSPublicApiSmoke.QuadraticFunctorMinimizeOneStepRuns` | unit / public API smoke | 固定二次函数和 `Vector6d` 初值 | `minimizeInit()` / `minimizeOneStep()` 可运行，`f` 和 `x` 有限 | BFGS public API 编译和状态推进 |

## 边界和随机样本策略

当前只使用固定样本，避免随机输入让 reviewer 难以复核。随机 stress（压力样本）暂缓；如后续引入，必须固定 seed（随机种子）和误差预算。

## 验证命令

```bash
make -C test-rvv/registration/bfgs run_test_compare
make -C test-rvv/registration/bfgs run_test_candidate_direction
make -C test-rvv/registration/bfgs run_test_public_api_smoke
```

如果只在 QEMU 上运行，该命令只证明 correctness，不证明性能。
