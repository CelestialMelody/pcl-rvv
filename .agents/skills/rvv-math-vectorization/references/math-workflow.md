# RVV 数学函数拟合工作流参考

本参考用于 C/C++ 库把 scalar `std::*` 或 libm 数学热点替换为 RVV intrinsic 近似实现时使用。示例来自一次 PCL 清理，但这里抽象为通用工作流。

测试目录、target 拆分、caller smoke gate 和迁移规则见 [RVV 数学测试目录与证据分层规则](testing-layout.zh.md)。新增数学函数时优先按 `artifact_layout.math_test_dir_template` 解析出的目录组织资产；其它项目使用 adapter 提供的等价 RVV math 专项目录。

实现/证据文档的默认落点按 `artifact_layout.math_doc_dir_template` 解析。业务模块或 common 模块函数文档按项目 adapter 的模块文档模板定位，用于说明入口、调用关系、分派和回退边界。common 文档如果依赖数学 helper，应链接到数学专项文档；不要在 common 文档里复制数学系数、拟合脚本结果或特殊值合同，避免同一事实在多个位置分叉。

开始新的 std/libm 风格数学 helper 前，先按 `artifact_layout.math_doc_dir_template` 解析并读取数学向量化总则文档。该文档记录跨函数通用规则；本 workflow 负责把这些规则落到具体候选、测试和收尾步骤。

文档、reviewer 汇报、配置解析出的测试资产和 prototype 注释还应遵循 `rvv-workflow/references/reviewability-and-language.zh.md` 的通用规则，以及 [RVV 数学函数原型的术语补充](reviewability-and-language.zh.md)：英文术语首次出现时必须解释；中文主导时给中文解释，英文主导时也要给 plain-English explanation（白话解释）。长测试文件应提供自然的阅读提示，非平凡函数说明作用、调用者和证据角色。

## 1. 语义合同

改系数前先写清楚：

- 公开角色：strict libm replacement、内部 helper，还是 finite-domain fast approximation。
- 输入域：任意公开 float、归一化点积、正 ratio、非正 Gaussian 参数等。
- 特殊值要求：NaN、`+Inf`、`-Inf`、overflow、underflow、subnormal、`+0`、`-0`、负数定义域。
- 调用方期望：精确类别、近似数值、单调性、非负权重，还是仅满足误差预算。

除非本批明确处理特殊值，否则不要在系数替换批次里顺手“完善”特殊值语义。

## 2. 候选模型检查

先选数学形态，再比较数值：

- `expf`：约化 `x = n ln2 + r`，在 `[-ln2/2, ln2/2]` 拟合 `exp(r)`，再重构 `2^n * P(r)`。
- `logf`：拆 exponent 和 mantissa，拟合 `log1p(u)`，再加 `e * ln2`。
- `acos`：若接受域是 `[0, 1]`，可用 `u = 1 - x` 和 `sqrt(u) * Q(u)`。
- `atan2`：按象限和对称性约化，再拟合小区间 odd kernel。
- `sin` / `cos` / `tan`：先阅读 [周期函数数学向量化专项规则](periodic-functions.zh.md)。必须先确认是 bounded-domain 近似，还是 arbitrary finite float 的 libm 替换；前者需要写清 caller 输入域，后者需要更完整的 range reduction 设计。

### 周期函数路标

遇到 `sin` / `cos` / `tan` 这类周期函数时，不要直接从多项式开始。先确认 range reduction、象限重构和过零点指标：

- 明确 bounded-domain 与 arbitrary finite float 的边界；没有真实 caller 输入域时，只能做 exploratory helper。
- 按输入域选择 range reduction 层级：小有限域优先 bounded mask 分段；中等有限域可评估 Cody-Waite；arbitrary finite float / strict libm replacement 通常需要 Payne-Hanek 级高精度约化。
- 若使用 `nearest(x * 2/pi)` 或 RVV `vfcvt_x_f`，记录 FRM/FCSR 依赖，并让 scalar same-chain 使用同一舍入口径。
- 若使用 mask 分段，显式列出每段的余项、sign/swap 规则，避免负 quadrant 和 C/C++ `%` 负数语义坑。
- 域外周期输入不能悄悄 clamp 成合法角度；应 fallback、NaN merge，或声明为非合同输入。
- Caller smoke 是后续下游证据，不能替代参数脚本、专项 C++ 测试和 scalar/RVV 同构对拍。

候选来源：

- 当前库 baseline。
- Remez1 / 初始交换点版本。
- Remez2 / refined 或 Powell-polished 版本。
- LP 或 minimax-on-grid 版本。
- Sollya/fpminimax：只有生成路径可用或明确标注为非默认候选时才作为主依据。

每个候选都要标明优化目标是 absolute error 还是 relative error。

## 3. 指标口径

报告和注释里必须区分：

- 脚本 kernel error：只看 reduced interval 上的多项式。
- 脚本 dense-chain simulation：包含 reduction、float Horner、reconstruction、clamp、bit construction。
- C++ scalar path：用于对拍的精确标量链路。
- RVV path：intrinsic 操作顺序、mask、merge、conversion 行为。
- QEMU：编译、结构和功能回归信号。
- 板卡：性能敏感决策的最终依据。
- 下游 caller budget：filter、norm、registration、app pipeline 的端到端容差。

不要只凭脚本 report 替换系数。C++ 链路和目标板卡结果必须支持这个决定。

## 4. 替换准入规则

满足以下条件再替换：

- 接受指标上精度改善，或至少不退。
- mean error、absolute error、relative error、worst-case category 对下游都有合理解释。
- 同系数 scalar/RVV 链路 `max |diff| = 0`，或只有可解释的小 ULP 差异。
- 目标板卡耗时没有明显退化。
- 特殊值语义不变，除非本批专门修改特殊值。
- 真正调用该函数的下游 smoke 通过。

如果候选在一个指标上更好、但 caller-relevant 指标更差，应保留 baseline 并写清理由。

### Production gate 未闭合项模板

数学函数文档中记录未闭合项时，不要只写名词清单。每项按下面口径写成陈述句：

- `输入域未闭合`：说明哪个 caller（调用方）的输入可能超出 helper 合同，为什么会超出，完成这项后能证明哪些调用方可以安全使用 helper。
- `domain-out/fallback 合同未定`：说明域外输入当前如何处理，production 中需要选择 fallback（回退标量）、NaN merge（合并为 NaN）、caller precondition（调用方前置条件）或分流策略，完成这项后能保证公开语义不会被静默改变。
- `caller 白名单未定`：说明白名单是允许使用该 helper 的真实调用方集合，完成这项后能避免把有限域 helper 误用于任意 libm 输入。
- `正式 helper 命名/包装策略未定`：说明是否提供 paired helper（同时算 sin/cos）和 single wrapper（单独 sin/cos 包装），完成这项后能让调用方式接近普通 `std` 数学函数，同时保留内部共享约化链路。
- `production 文档与代码评审未完成`：说明当前证据仍是 prototype 或 scratch evidence，完成这项后才能把实现、测试、fallback 和 caller 范围作为可维护生产事实。

每项最后说明当前阶段是否必须完成。scratch prototype 阶段可以不闭合真实 caller 白名单；production 接入前必须闭合输入域、fallback、命名和文档评审。

## 5. 最小修改边界

一个数学函数批次通常只包含：

- 对应 math helper 的实现常量或模型。
- C++ 专项测试标签和候选表。
- 系数脚本中的 baseline/current 标签。
- 文档里的系数来源、语义、精度和板卡证据。

不要混入：

- 头文件拆分或文件移动。
- CMake install-list 清理。
- 生成 output/log 更新。
- 无关下游重写。
- 多个数学函数，除非用户明确要求合批。

如果本批目标是迁移测试目录或拆分 scratch 文件，应只做结构迁移，保持数学行为等价。不要在同一批同时换系数、改 production helper 或改变特殊值语义。

### 测试资产边界

数学 helper / std-libm RVV 向量化专项资产应优先放在 `artifact_layout.math_test_dir_template` 解析出的目录；其它项目使用 adapter 提供的等价 RVV math 专项目录：

- 参数脚本和 Sollya / LP 工具。
- C++ 数学专项测试。
- RVV same-chain 对拍。
- 板卡 microbench。
- caller-shaped smoke。

Module 目录只放 module production caller 或 common.hpp 本体测试。迁移后不要保留旧数学 helper target 的双入口。

## 6. 验证矩阵模板

命令名按项目替换，但形状保持一致。命令中的 `<math-test-root>` 来自
`artifact_layout.math_test_root_template`；函数专属文件和脚本仍放在
`artifact_layout.math_test_dir_template` 解析出的目录。

```bash
git diff --check
make -C <math-test-root> parms_<function>
make -C <math-test-root> run_<function>_test
make -C <math-test-root> deploy_<function>_test
ssh <board> 'cd <deployed-dir> && make run_<function>_test'
```

然后跑下游调用方：

- `expf`：Gaussian、bilateral、convolution、image/kernel smoke。
- `logf`：norms、divergence、KL、entropy-like caller。
- `acos`：angle helper、sample consensus、normal-plane 或 geometry smoke。
- `atan2`：edge orientation、2D gradient、angle-map caller。

Caller smoke 必须有失败条件，不能只打印统计。至少报告 domain coverage、domain-out 数量、下游误差阈值和阈值来源。若 smoke 用于 RVV helper 风险判断，应覆盖 RVV path 或明确声明只覆盖 scalar approximation。

测试后：

- 恢复 tracked output/log 文件。
- 删除 untracked 生成日志。
- 确认 `git status --short` 只剩预期源码/文档文件。

## 7. 案例模式

### acos

决策模式：如果 reduced model 同时更简单、更准，可以替换旧多项式。

示例结果：

- 旧 baseline：PCL 风格八常数 sqrt 形式，约 `7.7e-4 rad`。
- 替换：`acos(x) ~= sqrt(1-x) * Q(1-x)`，degree-5 remez2。
- 专项结果：约 `1.3e-6 rad`；同链路 RVV vs scalar diff 为 `0`。
- 理由：精度显著改善，实现成本接近，下游 angle helper 已覆盖。

### expf

决策模式：只改善有限域系数，不偷偷改变 strict `std::expf` 语义。

示例结果：

- 替换：reduced interval 上的 relative-error Remez 系数。
- 合同：finite-domain fast approximation，输入 clamp，不完整模拟 NaN/Inf/overflow/underflow。
- 专项结果：接受的 C++ 测试网格上 max relative error 约 `2.2e-7`。
- 理由：网格精度改善，板卡性能无明显退化。

### logf

决策模式：候选只改善一个指标但提高 caller 风险时，保留 baseline。

示例结果：

- current baseline 保留。
- 某候选略微改善 max relative error，但 mean absolute error 和 Div/KL 下游容差风险不占优。
- norms QEMU/板卡测试在当前 baseline 下通过。
- 理由：收益不足以抵消验证和文档口径成本。

### atan2

决策模式：已有成熟核且测试结果好时，不为了“整理”而替换。

示例结果：

- 保留现有 odd kernel。
- 候选探索有记录，但不作为优先替换项。

## 8. 结构重构单独处理

把数学 helper 移到 `impl/rvv_math.hpp`、补 CMake 安装头、创建 workflow skill 都是有价值的清理，但不属于数学拟合本体。等数学行为验证完成后，单独提交这些结构改动。

测试目录迁移也属于结构重构。迁移时优先建立 `artifact_layout.math_test_dir_template` 解析出的专项目录，按函数分桶移动参数脚本、测试和 smoke；迁移完成后再继续数学候选探索或 production 接入。
