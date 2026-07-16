# 周期函数数学向量化专项规则

本参考用于 `sin` / `cos` / `tan` 等周期函数的 RVV 数学 helper 设计、拟合和验证。它是可复用规则，不是某次 `sinf`/`cosf` 实验报告；不要在本文中记录具体系数、误差结果或板卡结论。

## 1. 先定义语义合同

- 先判断目标是 strict libm replacement，还是 finite-domain fast approximation。
- strict libm replacement 必须覆盖任意有限 `float`、NaN/Inf、舍入模式、异常语义和边界输入；这通常需要完整的高精度 range reduction。
- finite-domain fast approximation 必须写清楚接受域，例如 `[-pi, pi]`、`[-pi/2, pi/2]` 或 caller 传导出的 FOV 子区间。
- 如果输入域来自真实 caller，要说明 caller 如何保证该域；如果只是独立探索，只能标记为 exploratory helper。
- 不允许把域外周期输入悄悄 clamp 成合法角度。周期函数的 clamp 会改变数学语义，不能作为 range reduction。

## 2. Caller 输入域与探索域

- 真实 caller 输入域可以降低 range reduction 难度，但不能替代数学专项测试。
- 没有真实 caller 输入域时，默认只能做有限域 scratch helper；不要声明可替换 `std::sin`、`std::cos` 或 `std::tan`。
- Caller smoke 只能验证下游误差是否可接受，不能证明 helper 本身在数学上合格。
- Caller smoke 必须是可失败 gate，至少检查 domain coverage、domain-out 和下游误差阈值；若用于 RVV 风险判断，还应覆盖 RVV path 或明确声明只覆盖 scalar approximation。
- 记录 caller 域时，要写清楚变量来源、单位、边界、是否包含端点、是否可能被 offset 或除法放大。
- 如果 caller 域依赖运行时参数，应在 smoke 中覆盖典型值、边界值和退化值。

## 3. Bounded-domain range reduction

周期函数优先选择有限域约化；不要在没有证据时尝试覆盖 arbitrary finite float。

先按输入域选择约化层级：

- 小有限域，例如 `[-pi, pi]`、`[-pi/2, pi/2]` 或已证明的 caller FOV 子区间，优先考虑 bounded mask 分段。它最容易审计，也最容易让 scalar/RVV same-chain 对齐。
- 中等有限域可评估 Cody-Waite 类约化：通常计算 `k = nearest(x * 2/pi)`，再用 `pi/2_hi` / `pi/2_lo` 拆分常量得到小余项。它比 mask 分段更通用，但必须处理舍入模式、常量来源和象限重构。
- 任意有限 `float` 的 strict libm replacement 通常需要 Payne-Hanek 级高精度约化：使用高精度 `2/pi` 表或多精度定点计算决定象限和余项。它的实现、验证和性能成本都明显更高，不应被有限域 helper 暗中承担。
- 如果 prototype 只验证有限域 helper，要明确写出“不覆盖 arbitrary finite float”，不要让 bounded reduction 被误读成 libm 级 range reduction。

常见方案有两类：

1. `k = nearest(x * 2/pi)`：
   - 目标是把 `x` 写成 `k*pi/2 + r`，并让 `r` 落入小区间，例如 `[-pi/4, pi/4]`。
   - 必须记录 `2/pi`、`pi/2_hi`、`pi/2_lo` 的来源和 float 量化方式。
   - 若 RVV 使用 `vfcvt_x_f` 得到 `k`，必须说明 FRM/FCSR 对舍入的依赖。
   - Scalar same-chain 应使用与 RVV 相同的舍入口径，例如默认 round-to-nearest-even 下的 `nearbyintf`。

2. Mask 分段：
   - 对 `[-pi, pi]` 等小域，用比较 mask 显式划分区间。
   - 每个区间都要列出余项 `r`、`sin(r)` / `cos(r)` swap 规则和符号规则。
   - 该方案可减少 FRM/FCSR 依赖，但会增加 mask/merge 成本。

无论采用哪类方案，都要测试 quadrant boundary 附近的输入。

## 4. 象限、负数和符号

- 不要依赖负数 `% 4` 的隐式语义来决定象限；C/C++ 负数取模容易让实现和数学意图脱节。
- 若使用整数 `k`，优先用显式比较处理 `k == -2, -1, 0, 1, 2` 这类 bounded-domain 情况。
- 若需要归一化象限编号，必须在 scalar 和 RVV 测试里复刻完全相同的规则。
- `sin` 是奇函数，`cos` 是偶函数；符号和 swap 错误通常会在 `+-pi/2`、`+-pi` 附近放大。
- 对 `-0` 要单独记录合同：`sin(-0)` 是否保留负零，`cos(-0)` 是否返回 `+1`。

## 5. 误差指标

- 周期函数过零点附近 relative error 会失效，不能作为主指标。
- 主指标应至少包含 absolute error；对接近 libm 的 helper，还应报告 ULP 或 bit-level 差异。
- 分别报告 kernel error 和 dense full-chain simulation。kernel error 只看约化区间上的多项式，full-chain 要包含 range reduction、象限重构、float Horner/FMA、mask/merge。
- Scalar same-chain vs RVV 的 `max |diff|` 必须单独报告。理想值为 0；若非 0，需要解释 FMA、FRM/FCSR、常数量化或 tail/mask 行为。
- Mean error 不能掩盖 worst-case；必须列出最差点位置，尤其是零点、极值点和象限边界。

## 6. Special values matrix

为每个周期函数 helper 写出特殊值矩阵，至少包含：

- `NaN`
- `+Inf` / `-Inf`
- `+0` / `-0`
- 正负 subnormal
- 大幅值 finite 输入
- 有限但超出合同域的输入

每一类都要说明策略：保留、生成 NaN、fallback、mask merge，还是非合同输入。非法 lane 不应进入未定义的 polynomial 主路径。除非本批专门修改特殊值语义，否则不要在系数或模型替换中顺手改变特殊值行为。

## 7. 系数来源与可复现性

- 记录候选来源：Taylor、Remez、LP/minimax-on-grid、Sollya/fpminimax 或历史 baseline。
- 记录脚本命令、目标函数、区间、degree、basis、误差目标和依赖工具。
- 记录系数量化到 `float` 后的复核结果；不要只报告 double 精度拟合误差。
- `sin(r)` 常用 odd polynomial，`cos(r)` 常用 even polynomial；如果 paired helper 共享 range reduction，要把两条多项式和重构规则一起验证。
- 文档和测试标签要能追踪到同一组系数，避免实现、脚本和报告中出现不同的 baseline/current 口径。

## 8. 验证矩阵

周期函数至少覆盖：

- 参数脚本和 dense-chain 仿真。
- Dense grid：合同域全区间和 caller 子区间。
- Adversarial points：`k*pi/2 +/- ulp`、`0 +/- ulp`、`+-pi +/- ulp`、分段边界。
- Special values matrix。
- C++ scalar same-chain vs RVV intrinsic path。
- ULP / bit-level comparator。
- QEMU correctness。
- 反汇编确认没有意外 libm call 混入 fast path。
- 板卡 microbench。性能敏感结论以板卡为准。

Caller smoke 是后续证据。它可以说明真实调用方的端到端误差和性能是否可接受，但不能替代参数脚本、专项 C++ 测试或 scalar/RVV 同构对拍。目录和 target 拆分规则见 [RVV 数学测试目录与证据分层规则](testing-layout.zh.md)。

Caller smoke 建议独立 target 或独立文件；当数学专项测试已经包含 dense/adversarial/special、benchmark 和 caller smoke 时，应拆分，避免单个 scratch 文件同时承担所有证据。

## 9. Production gate 禁止项

出现以下情况时，不进入 production：

- 没有明确语义合同。
- 没有证明 range reduction 和象限重构。
- 没有覆盖过零点、极值点和象限边界。
- 域外输入被悄悄 clamp 成合法角度。
- 只报告 relative error，未报告 absolute error 或必要的 ULP。
- 只有 caller smoke，没有数学专项测试。
- Scalar/RVV same-chain 差异无法解释。
- 没有 QEMU/板卡证据，却准备替换已有实现。
