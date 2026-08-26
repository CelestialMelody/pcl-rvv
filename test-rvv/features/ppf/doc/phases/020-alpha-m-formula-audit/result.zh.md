# Phase 020 Result: Alpha M Formula Audit

## 阶段结论

Phase 020 已完成 `alpha_m closed-form scalar audit`。本阶段通过 test-only helper（测试专用辅助函数）
证明：在当前 `PointXYZ + Normal` / float / AoS fixture 和平行 +x normal 样本上，`alpha_m` 可以不用
Eigen `AngleAxisf` / `Affine3f` 对象构造，直接用 Rodrigues rotation formula（罗德里格旋转公式）的旋转后
y/z 分量计算，并与 `computeAlphaMReference` 保持 `1e-5` 误差预算内一致。

该结果只证明公式可行性，不证明 RVV 性能，也不修改 production。

## TDD 回填

| 步骤 | 命令 / 产物 | 结果 |
| --- | --- | --- |
| RED | `make -C test-rvv/features/ppf run_test_std` | 编译失败，错误为缺少 `ppf_test::computeAlphaMClosedForm`，符合预期。 |
| GREEN | `include/impl/ppf_alpha_candidate.hpp`、`include/ppf.h` | 新增 closed-form helper 并接入聚合头。 |
| verification | `make -C test-rvv/features/ppf run_test_compare` | Std/RVV 两侧 4 个测试通过。 |

## 公式边界

`alpha_m` 的 production 链路等价于先对 `delta = model_point - reference_point` 施加同一个旋转 `R`，
其中 `R` 把 reference normal 旋到 x 轴；后续只使用旋转后 `y` / `z` 分量。closed-form helper 直接计算：

- parallel-to-x path（平行 x 轴路径）：按 production 的 `UnitY` 旋转轴处理。
- general path（一般路径）：用 normal 的 `ny/nz` 分量构造旋转轴，不显式构造 Eigen transform。
- sign correction（符号修正）：保留 production 的 `atan2` + `sin(angle) * z` 判断。

仍未覆盖的边界是 normal 接近 `-x`、非归一化 normal、非有限输入和真实 production dispatch。Phase 030
若写 RVV candidate，必须保留这些边界为测试或 fallback 条件。

## Evidence Doctor

本阶段只运行 correctness，不生成 benchmark、board summary 或 EvidenceDecision 性能结论，因此
Evidence Doctor 不适用。Phase 030 若新增 RVV `alpha_m` candidate 和 board repeated，则必须重新生成
manifest 并运行 Evidence Doctor。

## Continue / Stop Decision

Phase 020 未命中停止条件。closed-form helper 已为 RVV lane formula（向量通道公式）提供同构参考，
因此默认继续 Phase 030：实现 test-only `alpha_m` batch RVV candidate，先跑 correctness，再按板卡可用状态
执行 component benchmark 和 Evidence Doctor。`next_phase_default=030-alpha-m-rvv-candidate`。
