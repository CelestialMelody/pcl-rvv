# Phase 010 Plan: distribution-matrix-ablation

## 阶段意图和边界

本阶段验证 `ROPSEstimation::getDistributionMatrix()` 的 test-only RVV candidate。目标是把 rotated local cloud（旋转后的局部点云）到 distribution matrix（分布矩阵）的 bin index（分箱索引）计算拆出来：RVV 负责批量计算 `u_ratio/v_ratio` 和 row/col，scatter（向矩阵累加）先保持标量以避免同一 bin 冲突改变语义。

本阶段不修改 production，不覆盖 `computeFeature()` 完整 descriptor，不证明 LRF、rotateCloud 或 feature normalization 已优化。若 correctness 和 asm 成立，才补 component bench 和板卡证据。

## 当前状态清单

| area | current state | path |
| --- | --- | --- |
| Phase 000 | central moments correctness 成立，但不建议 standalone production | `doc/phases/000-current-state-and-component-ablation/result.zh.md` |
| production helper | `getDistributionMatrix()` 按 projection 选择 XY/XZ/YZ 两轴，逐点计算 row/col 并 `matrix(row,col)+=1`，最后除以 cloud size | `features/include/pcl/features/impl/rops_estimation.hpp` |
| current test support | 已有 `src/`、`include/`、`include/impl`、Makefile 和 phase docs | `test-rvv/features/rops_estimation` |

## 假设与候选族

| candidate family | hypothesis | risk | validation |
| --- | --- | --- | --- |
| distribution bin index RVV + scalar scatter | local cloud 点数通常大于 25 bins；批量 index 计算比 central moments 更可能形成收益 | scalar scatter 仍可能支配；若多点落同一 bin，不能直接 vector scatter accumulate | production oracle 对拍、QEMU、asm；通过后再决定 board component bench |

## 实现和测试动作

| action | artifact / command | expected evidence | completion criteria |
| --- | --- | --- | --- |
| A1 RED test | `src/test_rops_estimation.cpp` 调用尚不存在的 `rops::getDistributionMatrixRVV()` | `make run_test_rvv` 编译失败 | failure 来自缺少 candidate symbol |
| A2 helper implementation | `include/impl/rops_components.hpp` | 非 RVV 构建走 Std；RVV 构建批量生成 row/col staging 后标量 scatter | `run_test_compare` 通过 |
| A3 projection coverage | tests cover XY / XZ / YZ and boundary max bin clamp | QEMU Std/RVV logs | 与 production private helper oracle 逐 cell 近似一致 |
| A4 asm attribution | `make dump_test_rvv` | helper 中出现 `vle32.v`、`vfsub`、`vfdiv` 或等价指令 | result 写清归属边界 |
| A5 bench decision | result / roadmap / matrix | 若 helper 值得测，补 bench 和 board repeated；否则转向 rotate/projection | 不因 correctness 单项结束 topic |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic component |
| A/B boundary | production private helper oracle vs test-only candidate |
| 当前决策问题 | implementation-shape；distribution matrix 的 index 计算是否值得继续到 component bench |
| diagnostic 是否可外推到 production | no。它不覆盖完整 `computeFeature()`、LRF、rotateCloud、feature normalization 或 public dispatch |
| comparison-boundary / baseline mismatch 风险 | yes。candidate 把 row/col staging 显式计入 helper，production 若接入可能需要不同 buffer 生命周期 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | no for this component alone；可继续 rotate/projection 消融 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes |

## 继续 / 停止条件

默认完成 A1-A5。若 correctness 不成立，先修 helper；若 asm 没有 RVV 指令或 staging 成本显然过高，记录为 rejected/attempted 并继续 projection/rotate 消融。只有工具链不可用、源码 oracle 无法访问、dirty isolation 不安全或后续 production 接入需要用户确认时停止。
