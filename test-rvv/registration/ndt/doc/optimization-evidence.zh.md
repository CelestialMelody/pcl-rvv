# NDT Optimization Evidence

本文索引本 topic 已尝试、拒绝或暂缓的优化方式。它不承担跨 phase 搜索空间；搜索空间见 `doc/optimization-roadmap.zh.md`。

## 当前候选总表

| candidate family | 代码路径 | correctness | asm | board evidence | Evidence Doctor | decision | 边界 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| staged derivative accumulation RVV | `include/impl/ndt_candidates.hpp` | `run_test_compare` 通过 | `build/asm/riscv/bench_ndt_rvv.asm` 有 RVV 指令 | hessian 0.320x，gradient 0.401x | board Errors=2，Warnings=2 | rejected for production | test helper，不接 production |
| public-entry align profile | `src/bench_ndt.cpp` | `run_bench_public_smoke` 通过 | `gprof.txt` 有 NDT 符号 | median 1.003x；1/3 below 1 | Errors=1，Warnings=2，Suggestions=1 | hotspot evidence only | public overload cross-build profile |
| topic-local double exp RVV prototype | `include/impl/ndt_candidates.hpp` | `run_test_compare` 通过 | QEMU filtered asm 已刷新 | hessian 0.321x，gradient 0.405x | Errors=2，Warnings=3，Suggestions=1 | attempted/rejected for NDT production | test helper，不接 common / production |

## 标量路径与 RVV 路径差异

标量 reference 按 sample 顺序直接计算 `c_inv * jacobian.col(i)`、`x_trans.dot(...)` 和 hessian 公式。RVV candidate 先按 SoA（数组结构）跨样本计算二次型和中间 `xdot_cov_j`，再对 gradient / hessian 做 vector reduction（向量规约）。

这个设计为了跨样本向量化引入了额外 staged buffer（暂存缓冲）、多遍读取和 36 个 hessian 元素的重复规约。板卡负向结果说明当前形态不值得接入。当前证据约束下的归因假设是：标量 `exp` 预处理、SoA 暂存写回、多遍 RVV reduction 和寄存器/内存流量成本超过了乘加收益；其中任何单项都尚未通过 profile（性能剖析）或 ablation（消融对照）证明为唯一根因。

## 数学 helper 审计

`registration/include/pcl/registration/impl/ndt.hpp` 中有三类数学函数：

- `computeTransformation` 里的 `std::log` 和 `std::exp(-0.5)` 用于初始化 Gaussian 常量，通常每次 alignment（配准求解）只执行一次，不是当前优先 RVV 点。
- `computeAngleDerivatives` 里的 `std::sin` / `std::cos` 每次 derivative pass（导数计算轮次）只计算 3 个角；除非 profile 显示异常占比，否则不建议优先向量化。
- `updateDerivatives` / `updateHessian` 里的 double `std::exp` 按 point-neighbor sample（点-邻域样本）执行，是数学函数热点候选。

当前 `common/include/pcl/common/impl/rvv_math.hpp` 提供的是 `expf_RVV_f32m2`、`logf_RVV_f32m2`、`sincos_finite_domain_RVV_f32m2`、`acos_RVV_f32m2` 和 `atan2_RVV_f32m2` 等 float helper。NDT 公式使用 `Eigen::Vector3d` / `Matrix3d` 和 double `std::exp`，现有 helper 不能直接替换。phase 020 已做 topic-local double `exp` finite-domain prototype：correctness 通过，但 board repeated 仍是 negative。因此 NDT 当前不建议继续公共 double `exp` helper；若未来其它 caller 也需要，应另走数学专项。

## 暂缓路线

| candidate | status | 恢复条件 |
| --- | --- | --- |
| public-entry profile（公开入口 profile） | deferred | 先确认真实 NDT runtime 中 neighbor search、point derivative、updateDerivatives、line search 和 Eigen SVD 的耗时占比 |
| fused per-sample formula（融合逐样本公式） | deferred | 需要 profile 证明 updateDerivatives 是热点，并设计减少 staged buffer 的 RVV-vs-RVV A/B |
| double exp RVV math helper audit | attempted/rejected for NDT | phase 020 已完成 topic-local prototype；收益不足，不建议抽公共 helper |
| compiler auto-vectorization report（编译器自动向量化报告） | deferred | 若后续仍考虑 NDT 局部公式优化，再用 `generate_vec_report` 判断编译器已覆盖的部分 |

当前默认建议仍是停在 no-production，不进入 production integration loop（生产接入闭环）。若继续，只建议尝试 fused formula / 减少 staging 的新实现形态。
