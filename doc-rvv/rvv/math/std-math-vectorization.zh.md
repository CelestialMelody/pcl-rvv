# std 数学函数 RVV 向量化总则

本文记录接近 `std` / libm 数学函数的 RVV 近似实现通用规则。它面向 `expf`、`logf`、`acos`、`atan2`、`sinf/cosf` 这类 helper：既要有可复现的数学拟合，又要能被 C++ 标量链路、RVV intrinsic 链路、QEMU、反汇编、板卡和下游调用方证据共同审查。

这里的规则不是某一次实验的 backlog（待办清单），而是后续新增或评审 RVV 数学 helper 时应遵守的长期规范。具体函数的系数、阈值、板卡结果和未闭合项，应记录在对应专题文档中。

---

## 1. 适用范围

本文适用于将标量 `std::*` 或 libm 热点改写为 RVV 数学 helper 的工作，包括但不限于：

- `expf`：指数函数，通常需要 `ln2` 约化和 `2^n` 重构。
- `logf`：对数函数，通常需要 exponent / mantissa（指数/尾数）拆分和 `log1p` 多项式。
- `acos`：反余弦函数，可用 `sqrt(1-x) * Q(1-x)` 一类约化模型。
- `atan2`：二维象限角函数，需要象限约化和符号重构。
- `sinf/cosf`：周期函数，需要有限域或高精度 range reduction（范围约化）。

数学 helper 文档与 PCL 模块文档应分工清楚：

- 数学 helper 的语义合同、系数来源、特殊值矩阵、专项测试、RVV 同构链路、反汇编和板卡证据，放在 `doc-rvv/rvv/math/`。
- PCL common、filter、range_image 等模块的生产入口、数据结构、分派策略、回退条件和模块级测试，放在对应模块文档目录，例如 `doc-rvv/common/`。
- 模块文档如果依赖数学 helper，应链接到数学专题文档，并只说明调用关系、输入域是否满足合同、下游误差预算和 fallback（回退策略）。不要把系数、特殊值矩阵和脚本结果复制到模块文档里。

这条边界可以避免同一事实在多个位置分叉：数学细节由数学文档维护，调用方风险由模块文档维护。

---

## 2. 两类语义合同

RVV 数学 helper 首先要声明自己是哪一种语义合同。合同决定输入域、特殊值、fallback、测试矩阵和能否接入真实调用方。

### 2.1 Strict libm replacement

strict libm replacement（严格替代标准数学库）指 helper 试图在公开语义上替代 `std::sin`、`std::exp`、`std::log` 或对应 libm 函数。它的输入通常是任意 `float` 或较完整的有限浮点域。

这种合同必须处理：

- `NaN`、`+Inf`、`-Inf` 的分类和传播。
- signed zero（带符号零），例如 `sin(-0)` 是否必须返回 `-0`。
- overflow / underflow（上溢/下溢）和 subnormal（次正规数）。
- domain error（定义域错误），例如 `logf(x <= 0)` 或 `acos(|x| > 1)`。
- arbitrary finite float（任意有限浮点数）的范围约化，尤其是周期函数的大幅值输入。
- 与标准库不完全一致时，文档和测试必须明确差异。

严格替代标准数学库的门槛高。周期函数若要覆盖任意有限 `float`，通常不能只靠小范围 mask 分段，需要 Cody-Waite 或 Payne-Hanek 这类更高精度的范围约化方案，并配套舍入模式、常量来源和边界点测试。

### 2.2 Finite-domain fast approximation

finite-domain fast approximation（有限域快速近似）指 helper 只承诺在特定输入域内快速近似目标函数。输入域通常来自真实调用方，例如角度、归一化点积、Gaussian 参数、正 ratio 或已 clamp 的几何量。

这种合同必须写清：

- 合法输入域是什么，是否包含端点。
- 输入域来自真实 caller（调用方），还是独立探索的实验 helper。
- domain-out（域外输入）如何处理：fallback 到标量、返回 NaN、要求 caller precondition（调用方前置条件），还是通过分派避开。
- 特殊值是否进入快速路径，还是统一合并为 NaN / fallback。
- helper 名称必须暴露有限域属性，例如包含 `finite_domain`，不能伪装成通用 `std::sin` 或 `std::log`。

有限域快速近似可以成为生产 helper，但不能被泛化为 libm 替换。只有当 caller 输入域、domain-out/fallback 合同、特殊值策略和下游 smoke 都闭合后，才能进入受限生产接入。

---

## 3. 数学模型与系数来源

每个 helper 都应先确定数学形态，再讨论 RVV intrinsic 和性能。多项式只是链路的一部分；范围约化、重构、bit construction（位构造）、mask/merge（掩码/合并）和特殊值合并同样属于数学模型。

### 3.1 常见模型

`expf` 的典型形态是：

- 使用 `x = n * ln2 + r` 做范围约化，其中 `r` 落在 `[-ln2/2, ln2/2]` 之类的小区间。
- 在小区间上拟合 `exp(r)`。
- 用 `2^n * P(r)` 重构，`2^n` 常通过浮点 exponent 位构造得到。
- 误差目标通常更关注 relative error（相对误差），因为 `exp` 的函数值跨越多个数量级。

`logf` 的典型形态是：

- 拆分输入的 exponent / mantissa。
- 把 mantissa 转成适合 `log1p(u)` 的小区间。
- 拟合 `log1p(u)`，再加回 `e * ln2`。
- 对 `x <= 0`、`NaN`、Inf、subnormal 的处理必须与合同一致。

`acos` 的一个有效形态是：

- 在调用方只需要 `[0, 1]` 或锐角场景时，用 `u = 1 - x`。
- 近似 `acos(x) ~= sqrt(u) * Q(u)`。
- 这个形态把端点附近的平方根奇异性显式拆出来，常比直接在 `x` 上做高次多项式更稳。
- 如果文档名或测试来自 `getAcuteAngle3D`，要说明它对应的是该调用场景下的 `acos` helper，而不是完整 common API 的所有逻辑。

`atan2` 的典型形态是：

- 先利用象限和对称性把输入约化到小区间，例如 `atan(t)` 的 odd kernel（奇函数核）。
- kernel（约化区间上的多项式核函数）只处理小区间，不等于完整 `atan2`。
- 完整 helper 必须包含象限重构、符号处理、零输入组合和特殊值策略。

`sinf/cosf` 的有限域形态是：

- 对小有限域使用 bounded range reduction（有界范围约化），例如 mask 分段。
- 在 `[-pi/4, pi/4]` 一类约化区间上使用 `sin` odd polynomial 和 `cos` even polynomial。
- 用 quadrant reconstruction（象限重构）完成符号和 `sin/cos` 交换。
- 过零点、象限边界和 `pi/2` 附近必须单独测试；不能只看 kernel error。

这些模型不能跨函数机械套用。`expf` 的 relative-error 目标不自动适合 `sin`；`acos` 的 `sqrt(1-x)` 形态也不自动说明任意 `acos` 输入域已经闭合。

### 3.2 系数生成方法

常见候选来源包括：

- Taylor baseline（泰勒基线）：便于闭环和 debug，但通常不是最终精度最优解。
- Remez：面向 minimax（最大误差最小化）的经典方法，可通过交换法或数值优化得到。
- LP / minimax-on-grid：把密集网格上的最大误差最小化写成线性规划，常用于可复现候选和交叉验证。
- Sollya `fpminimax`：可用于连续区间的高质量候选，但不应成为仓库第一版的必需依赖，除非文档写清运行环境和复现方式。

系数 provenance（来源记录）必须包含：

- 参数脚本路径和运行命令。
- 近似目标函数和区间。
- 多项式形式、次数、基函数和是否使用 odd/even 约束。
- 优化目标：absolute error（绝对误差）、relative error（相对误差）或其它 caller-level 目标。
- 生成方法：Taylor、Remez、LP、Sollya 或组合流程。
- float32 量化前后的系数和误差变化。
- full-chain（完整链路）仿真结果，而不仅是约化区间 kernel 误差。

证据应放在函数专题文档和参数脚本输出中。不要只把常量写进头文件而不保留生成路径；没有 provenance 的系数难以评审，也难以在后续硬件或编译器变化后复核。

---

## 4. 误差指标

数学 helper 的误差报告需要同时区分“衡量什么”和“在哪条链路上衡量”。单一指标很容易误导接入判断。

### 4.1 常用指标

absolute error（绝对误差）适合函数值可穿过零点或下游看实际数值偏差的场景。`sin`、`cos`、`atan2`、角度误差和 xyz 下游误差通常都需要绝对误差口径。

relative error（相对误差）适合函数值远离零且比例误差有意义的场景。`expf` 常用 relative error，因为输出跨数量级，绝对误差会被大值区间支配。

ULP / bit-level（以浮点单位或位级比较）适合检查舍入和位级一致性，但不能单独作为所有函数的主指标。`sin` 在过零点附近真实值接近 `0`，很小的绝对误差也可能对应巨大的 ULP 诊断值；这不一定意味着下游失败。

mean / worst-case（平均值 / 最坏值）要一起看。只改善 worst-case 但显著恶化 mean，可能会增加真实 caller 的累计误差；只改善 mean 而放大边界点 worst-case，也可能破坏特殊输入。

caller-level budget（调用方误差预算）是下游可接受误差，例如角度、距离、权重、分类一致性或 pipeline 容差。它不能替代数学专项误差，但能帮助判断有限域 helper 是否值得生产接入。

### 4.2 链路层次

报告误差时至少区分：

- kernel error：只看约化区间上的多项式核函数。
- dense-chain simulation：在脚本里模拟范围约化、float32 Horner、重构、clamp 或 bit construction。
- C++ scalar same-chain：same-chain（同构链路）指标量代码刻意复刻 RVV 操作顺序，用来与 RVV intrinsic 对拍。
- RVV same-chain：同一系数和同一操作链在 RVV intrinsic 下的结果，理想情况下与标量同构链路 `max abs diff = 0`。
- QEMU correctness：验证 RISC-V 构建、RVV intrinsic 路径和功能 gate，不作为性能证据。
- board microbench：板卡微基准，是性能信号；它仍不能替代生产调用方测试。
- caller smoke：caller smoke（调用方冒烟测试）验证真实或近似真实输入形态下的下游误差和域覆盖。

不能只凭 kernel error 替换生产系数。范围约化、float32 FMA、重构、mask merge 和特殊值合并都可能改变最终误差。

---

## 5. 测试与证据矩阵

新增数学 helper 时，建议按下面矩阵收集证据。每类证据证明的事情不同，不能互相替代。

| 证据 | 放在哪里 | 证明什么 | 不证明什么 |
| --- | --- | --- | --- |
| 参数脚本 | `test-rvv/rvv/math/<function>/script/` | 系数生成可复现，kernel / dense-chain / float32 量化指标可审查 | 不证明 C++、RVV 或板卡路径正确 |
| C++ scalar same-chain | `<function>_test.cpp` | 标量同构链路与预期合同一致，可作为 RVV 对拍参考 | 不证明 RVV intrinsic 没有路径差异 |
| RVV same-chain | `<function>_test.cpp` | RVV intrinsic 链路与标量同构链路一致或差异可解释 | 不证明 strict libm 语义完整 |
| dense grid | `<function>_test.cpp` 或参数脚本 | 常规输入域上没有大面积误差退化 | 不覆盖所有边界和特殊值 |
| adversarial points | `<function>_test.cpp` | 端点、象限边界、过零点、约化切换点等高风险点通过 | 不代表真实 caller 分布 |
| special values matrix | `<function>_test.cpp` 与文档 | NaN、Inf、signed zero、subnormal、domain-out 等合同可验收 | 不自动说明生产 fallback 已设计好 |
| QEMU correctness | `make -C test-rvv/rvv/math run_<function>_test` | RISC-V 构建和 RVV 功能路径通过 | 不提供可信性能结论 |
| disassembly | 专题文档记录摘要 | 快速路径含预期 RVV 指令，未意外调用 libm | 不证明数学误差或下游正确性 |
| board microbench | `board.mk` / 专题文档 | 目标板卡上的性能信号 | 不等于生产端到端收益 |
| caller smoke | `run_<function>_<caller>_smoke` | 调用方输入域、下游误差和 RVV 路径风险 | 不能替代数学专项测试 |
| 文档证据 | `doc-rvv/rvv/math/` | 合同、模型、证据、限制和生产门禁可审查 | 不替代可运行测试 |

caller smoke 必须有可失败的验收条件，例如 `domain_out == 0`、`in_contract == samples`、下游误差阈值、RVV 与标量同构链路差异阈值。只打印统计不能作为 gate。

QEMU 只能说明 correctness。性能敏感结论必须以真实板卡为准。板卡 microbench 也只是性能信号；如果真实调用方有不同的数据布局、缓存行为或分派成本，还需要 caller-level 证据。

---

## 6. 目录与文档归属

测试资产默认放在：

```text
test-rvv/rvv/math/<function>/
```

推荐结构：

```text
test-rvv/rvv/math/
  Makefile
  board.mk
  README.zh.md
  script/
    lp_minimax.py
    sollya_utils.py
  <function>/
    <function>_test.cpp
    <function>_<caller>_smoke.cpp
    script/parms_<function>.py
```

数学专项文档默认放在：

```text
doc-rvv/rvv/math/
```

PCL common 模块函数文档仍放在：

```text
doc-rvv/common/
```

归属规则如下：

- 如果文档主角是数学函数本体、系数、误差、特殊值或 RVV math helper，放 `doc-rvv/rvv/math/`。
- 如果文档主角是 PCL common API 的完整向量化、数据布局、分派和模块级行为，放 `doc-rvv/common/`。
- 如果 common 函数依赖数学 helper，common 文档只链接数学专题文档，并描述调用点和输入域，不复制数学模型细节。
- 如果某个文档既涉及数学 helper 又涉及调用场景，应看“主要证据归属”。例如 `getAcuteAngle3D` 的核心是 `acos` helper 证据时，放数学目录，并在文档中说明对应调用场景。

目录迁移和数学行为修改应分批进行。迁移文档或测试目录时，应保持行为和结论不变，并同步修正旧路径引用。

---

## 7. Production gate

production gate（生产接入门禁）是进入真实生产路径前必须闭合的验收条件。门禁未闭合时，helper 可以保留为 scratch prototype（实验原型）或 test-only integration prototype（仅测试使用的接入原型），但不能宣称已可生产接入。

输入域闭合说明所有允许调用 helper 的生产路径都满足 helper 合同。它需要追踪输入变量从调用方参数、offset、归一化、投影或 clamp 到 helper 的传导过程。完成这项后，可以降低有限域 helper 被误用于域外输入的风险；生产接入前必须完成。

domain-out/fallback 合同说明域外输入如何处理：fallback（回退策略）到标量 libm、返回 NaN、使用 caller precondition，还是通过分派避开。完成这项后，可以保证公开语义不会被静默改变；有限域 helper 进入生产前必须完成。

caller 白名单说明哪些真实调用方允许使用该 helper，并且这些调用方的输入域和下游误差已被证据覆盖。完成这项后，可以避免一个为 RangeImageSpherical 设计的有限域 `sincos` helper 被误用于 base RangeImage 或任意 `std::sin` 热点；生产接入前必须完成。

helper 命名与 wrapper 策略说明 helper 是 paired helper、single wrapper，还是 strict libm 替换。完成这项后，调用形式可以接近普通 `std` 数学函数，同时保留有限域边界；生产接入前必须完成。有限域 helper 的名称应显式暴露 `finite_domain` 或等价含义。

特殊值合同说明 NaN、Inf、signed zero、subnormal、overflow、underflow 和定义域错误如何处理。完成这项后，可以避免 RVV 快速路径与标量参考链路在异常输入上分叉；生产接入前必须完成，除非调用方输入域已严格排除这些值且文档说明为前置条件。

反汇编和板卡复跑说明最终生产形态仍走预期 RVV 快速路径，并在目标板卡上没有明显性能退化。完成这项后，可以降低编译器优化、内联、fallback 或 libm 调用意外改变性能路径的风险；生产接入前必须完成。

文档与代码评审说明实现、测试、fallback、调用方白名单和证据矩阵已经在长期维护位置记录。完成这项后，后续 worker 可以复查、复跑和修改 helper；生产接入前必须完成。

这些门禁不要求在 scratch 阶段全部闭合。scratch 阶段可以只证明数学方案可行；production 接入前必须把语义和调用方风险闭合。

---

## 8. 从 scratch 到 production 的阶段

scratch prototype（实验原型）用于验证数学方案。这个阶段可以放在 `test-rvv/rvv/math/<function>/`，可以使用测试专用 header、实验候选和较详细注释。它能证明参数脚本、C++ 标量链路、RVV 链路、QEMU、反汇编或板卡 microbench 可行，但不能宣称已可生产接入。

test-only integration prototype（仅测试使用的接入原型）用于验证 helper 与真实调用方形态是否匹配。它可以通过测试专用头文件覆盖或原型函数模拟接入方式，但不能修改真实 production caller，不能改变 public API。它能证明调用方式和输入域有可能闭合，但不能替代正式 production 代码评审。

limited production helper（受限生产 helper）是进入 `rvv_math.hpp` 或等价生产头文件的受限 helper。它必须有明确名称、有限域合同、特殊值策略、fallback 选择、专项测试和文档。它可以不等于 strict libm replacement，但不能隐藏自己的限制。

caller integration（真实调用方接入）是把 helper 接到白名单生产 caller。这个阶段必须证明输入域闭合、RVV 分派条件、fallback、下游误差、板卡性能和文档同步。一个 caller 通过不代表所有 caller 通过；新 caller 需要单独闭合输入域和下游 smoke。

阶段之间的结论不能越级。scratch 性能好只能说明值得继续；test-only 接入通过只能说明接入形态可行；只有 production gate 闭合后，才允许把 helper 作为生产事实记录。

---

## 9. 已有案例索引

- [`sincos-RVV.zh.md`](sincos-RVV.zh.md)：代表有限域周期函数的 paired helper 经验。重点是 bounded range reduction、hi/lo 常量补偿、过零点指标、同构链路、调用方形态冒烟测试和生产门禁边界。
- [`expf-RVV.zh.md`](expf-RVV.zh.md)：代表 `ln2` 约化、relative-error 系数目标、`2^n` 重构和板卡性能复核。
- [`logf-RVV.zh.md`](logf-RVV.zh.md)：代表 exponent/mantissa 拆分、`log1p` 多项式、特殊值处理，以及候选未明显优于 baseline 时保留现实现的工程判断。
- [`atan2-RVV.zh.md`](atan2-RVV.zh.md)：代表象限约化、odd kernel、符号重构和多个候选来源的比较方式。
- [`getAcuteAngle3DRVV.zh.md`](getAcuteAngle3DRVV.zh.md)：代表 `acos` helper 在 `getAcuteAngle3D` 调用场景中的证据归属，重点是 `sqrt(1-x) * Q(1-x)` 约化模型和锐角输入域。
- [`remez-coeffs.zh.md`](remez-coeffs.zh.md)：代表 Remez、LP/minimax-on-grid、Sollya 和参数脚本的系数来源记录方式。

新增案例应优先复用本文的证据矩阵和目录归属规则，再在函数专题文档中记录该函数独有的合同、模型、误差和生产门禁状态。
