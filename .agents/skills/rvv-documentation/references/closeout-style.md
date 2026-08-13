# Closeout 与写作风格

## Current-state-first

主题进入 closeout 后，文档不应继续保持开发流水账结构。先写当前生产状态、已 RVV 化阶段、仍保留的标量边界、主要性能结论和是否建议继续扩展。

历史过程只保留能解释当前设计的内容，例如 FMA contraction、阈值谓词、`vcompress` 保序、fallback tail 或 traits gate。

production closeout 后的 `doc-rvv` 主题文档应单独说明“当前采用的优化方式”。这段说明解释当前实际使用的 RVV 组织方式、采用原因、VL chunk 内部流程、分组职责、fallback 边界和暂缓方案。它不是历史实验清单，也不替代后面的“正确性与高效性证据链”。no-production closeout 没有 adopted production behavior 时，不创建 `doc-rvv`；候选尝试、拒绝理由和“诊断证据链”写入 topic-local evaluation / phase closeout。

production closeout 后的主题文档应回答：

- 原标量循环中哪几段已经由 production RVV 或 diagnostic RVV 接管。
- 哪些阶段仍是标量，为什么不继续 RVV 化。
- 当前采用的优化方式如何工作，例如 load / gather、mask、staging、reduction、store、scalar tail 或 block group 分工。
- 哪些 gate 会触发 fallback，fallback 后语义如何保持。
- 哪些测试、反汇编和目标硬件证据证明边界成立。
- 收益是否足以覆盖 staging、buffer、分流和维护成本。

## 测试保留策略

production 升级后先做测试 inventory，不要因为已有真实生产入口测试就机械删除诊断测试。保留规则：

- `production direct` 测试是生产接入后的主证据，应覆盖真实入口、真实 gate、真实 fallback、checksum 或输出顺序。
- RVV-only adversarial 测试如果保护已知语义风险，应继续保留，例如 bit-level、threshold boundary、rounding、mask / `vcompress` ordering、traits、fallback 或同进程先 RVV 后 fallback 的顺序测试。
- `production-shaped diagnostic` 可继续作为阶段归因、边界复现或 fallback 验证工具保留；名称和注释要说明用途，不再写成未来接入的单独证据。
- 只有测试的入口形态、输入构造、参数、断言和覆盖边界都被其它测试完全包含时，才适合删除或合并。

如果测试被删除、合并或改名，评估文档和适用的 production 长期主题文档必须记录：

- 旧测试名。
- 删除或合并原因。
- 与哪些保留测试重复。
- 剩余风险由哪些测试覆盖。
- 若仍有风险，写清风险由哪个保留测试、上游测试、diagnostic case 或 production direct case 覆盖。

删除理由要基于输入、入口、参数、断言和 adversarial 条件，不能只写“重复”。

## 写作风格

通用规则见 [writing-style.md](writing-style.md)。

closeout 文档还应遵循：

- 先说明实际结论、覆盖范围和证据，再列限制。
- 命令、日志位置和状态使用清单、表格或明确字段。
- 技术文档、函数级评估、模块筛选报告和工作日志以技术事实、复核目标和证据链为主语。
- 讨论触发的问题可以写成“为复核某问题”“本轮补充验证”“实验显示”。

## 数值算例

数值算例按“约定 -> 标量公式 -> RVV chunk 逐步对齐 -> 结论”组织。

至少说明：

- 输入/输出数据结构、内存布局、坐标命名、kernel、transform 或 threshold。
- 若干输出位置或 source lane 的标量计算式。
- 一个小 `vl` 下 load/gather、mask、FMA、`vcompress`、store 或 staging 写回。
- lane 与标量结果如何对应。

如果主题完全不适合数值算例，需要在文档中说明原因。

## 跨文档同步

closeout 时至少检查：

- 函数级评估与适用的 production 长期主题文档的生产接入结论是否一致；no-production 时是否明确 `doc-rvv` 为 `not_applicable`。
- 模块状态表是否反映当前主题完成、暂缓、bench 诊断或生产回退状态。
- 工作日志是否记录测试、bench、QEMU、反汇编、目标硬件和文档同步状态。
- 如果改动暴露通用规则，是否同步到 `.agents/skills/` 或项目知识文档。
