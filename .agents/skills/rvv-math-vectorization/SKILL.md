---
name: rvv-math-vectorization
description: 用于 C/C++ 高性能库中 RISC-V RVV 数学函数向量化拟合、替换、测试和文档同步；适用于 expf、logf、acos、atan2 或类似 std/libm 标量热点，覆盖语义边界、多项式候选、系数替换准入、QEMU/板卡验证、下游 smoke 和产物清理。
---

# RVV 数学函数向量化拟合

当 C/C++ 高性能库里出现 `std::*` 或 libm 数学热点，需要设计、比较或替换为 RVV 近似实现时使用本 skill。目标是让系数替换变得可审计：语义先行、候选可复现、scalar/RVV 同构对拍、板卡证据明确、文档同步干净。

本 skill 聚焦数学函数近似本体：语义边界、候选拟合、系数替换、专项测试和验收。生产路径接入、fallback 策略、RVV load/store、mask/merge、分派阈值和调用方结构，可配合 $rvv-implementation 使用。

规划新函数、评审候选系数或准备替换提交时，阅读 [references/math-workflow.md](references/math-workflow.md) 获取详细检查清单和案例模式。

新增或迁移测试资产时，阅读 [references/testing-layout.zh.md](references/testing-layout.zh.md)。数学 helper / std-libm RVV 向量化专项测试目录按 `artifact_layout.math_test_dir_template` 解析；其它项目应在 adapter 中提供等价的 RVV math 专项目录。caller smoke、benchmark、参数脚本和数学专项测试必须分层管理。

新增或迁移实现/证据文档时，RVV 数学函数专项文档按 `artifact_layout.math_doc_dir_template` 解析位置。业务模块或 common 模块函数文档按该项目 adapter 的模块文档模板定位，只记录调用关系、输入域、分派和回退边界；如果模块函数依赖数学 helper，应以链接方式引用数学专项文档，不要复制系数、误差口径或特殊值合同。

写作或评审接近 std/libm 的 RVV 数学 helper 时，先按 `artifact_layout.math_doc_dir_template` 解析并读取数学向量化总则文档。它是长期总则；具体函数的系数、阈值和板卡结果仍写在对应专题文档中。

写测试、文档或给 reviewer 汇报时，先遵循 `rvv-workflow/references/reviewability-and-language.zh.md` 的通用规则，再阅读 [references/reviewability-and-language.zh.md](references/reviewability-and-language.zh.md) 获取数学函数专用术语。英文术语首次出现时必须解释；中文主导时给中文解释，例如 `kernel（约化区间上的多项式核函数）`、`lane-level helper（只处理 RVV 向量寄存器和 vl 的单段向量 helper）`；英文主导注释中也要给 plain-English explanation（白话解释），例如 `kernel (the polynomial on the reduced interval, not the full sin/cos helper)`。后续可使用“中文 + 英文缩写/原词”的形式，避免读者只能靠英文术语猜含义；同时避免翻译腔，优先写自然工程说明。

scratch C++、Python 脚本和 Makefile target 应保留审查型注释，说明样本来源、scalar/RVV/reference 角色、gate 条件和不可泛化边界。production 代码注释应克制；配置解析出的测试资产 / prototype 代码可以更详细，长文件必须有自然的文件级阅读提示和函数级“作用/调用者/证据角色”说明，不要写成“中文执行地图”或“作用/类别”模板。

数学函数文档中的 production gate（生产接入门禁）和未闭合项必须逐条说明含义和作用，不能只列名词。例如 domain-out/fallback 合同应说明“域外输入如何处理：回退标量、返回 NaN、还是要求调用方保证输入合法”，caller 白名单应说明“哪些调用方允许使用 helper，并已证明输入域满足合同”。使用陈述句说明当前是否必须闭合。

周期函数（如 `sin` / `cos` / `tan`）还需特别注意：

- 必须先证明 range reduction、象限重构和边界点误差；没有这些证据时不得进入 production。
- 按输入域选择约化层级：小有限域优先 bounded mask 分段；中等有限域可评估 Cody-Waite；任意有限 `float` 的 strict libm 替换通常需要 Payne-Hanek 级高精度约化。
- 没有真实 caller 输入域时，只能做 exploratory helper，不能声明为可替换的 libm 实现。
- 过零函数不能以 relative error 作为主指标，必须报告 absolute error，必要时报告 ULP / bit-level 差异。
- Caller smoke 不能替代参数脚本、专项 C++ 测试和 scalar/RVV 同构对拍。
- Caller smoke 必须是可失败 gate；若用于证明 RVV helper 风险，应覆盖 RVV path，或明确声明只覆盖 scalar approximation。

## 流程

1. 先定义语义合同：
   - 判断目标是 strict libm replacement，还是 finite-domain fast approximation。
   - 明确 NaN、Inf、overflow、underflow、subnormal、signed zero、domain error 是否需要兼容。
   - 盘点下游输入域。Gaussian 权重、bilateral、norms 往往比公开 libm 有更窄的有限域。

2. 建立可比候选：
   - 先定 reduction 模型：`exp(x) = 2^n exp(r)`、log mantissa/exponent、`acos(x) ~= sqrt(1-x) Q(1-x)`、atan quadrant reduction 等。
   - 比较 current baseline、Remez 变体、LP/minimax-on-grid、可复现的 Sollya 输出。
   - 明确候选优化的是 absolute error 还是 relative error。

3. 区分指标口径：
   - 脚本核函数误差不等于 dense full-chain 仿真。
   - C++ scalar Horner 不等于 RVV intrinsic 链路，除非操作顺序和精度刻意对齐。
   - QEMU 适合验证结构和回归；性能敏感决策以板卡为准。
   - 下游业务误差预算可能比单函数数学指标更重要。

4. 使用替换准入规则：
   - 精度在接受指标上必须改善或至少不退。
   - 同系数 RVV vs scalar 的 `max |diff|` 要解释清楚；理想情况为 0。
   - 目标板卡性能不能明显退化。
   - 特殊值语义不能被偷偷改变。
   - 真正调用该函数的下游 smoke 必须通过。

5. 保持修改边界窄：
   - 一批只处理一个数学函数。
   - 系数变化时，同步实现常量、脚本 baseline/current 标签、C++ 测试标签和文档。
   - 不混入 header 拆分、CMake 安装清理、skill 整理或生成日志。
   - 目录迁移、测试拆分和 production 接入要与系数/模型变化分批处理。

6. 验证并清理：
   - 运行 `git diff --check`。
   - 运行参数脚本、专项 C++ 测试、QEMU、可用板卡测试和下游 smoke。
   - staging 前恢复 tracked logs，删除生成的 `output/`、`log/` 或临时产物。

## 何时保留 baseline

如果最佳候选只有很小的 max-error 收益，却带来 mean error 变差、下游容差风险、板卡性能无明显收益，或文档/测试口径成本过高，应保留当前实现，并明确记录“不替换”的理由。`不替换` 是工程结论，不是未完成。
