# Evidence Doctor / Evidence Validator

Evidence Doctor（证据体检）是 RVV topic（主题）证据链中的异常信号提示层。它适用于 benchmark（性能测试）、board summary（板卡摘要）、checksum summary（校验和摘要）、asm attribution（反汇编归属）和 EvidenceDecision（证据决策）前的证据复核。

它不替代 reviewer（审查者），也不自动证明实现有 bug。它的职责是把“数据看起来不对劲”的信号转成可审查的假设、检查动作和证据边界决定。worker 不能在触发 Error 或 Warning 后无解释地把表格转成性能结论。

## 适用时机

下列动作完成后、写 production performance（生产性能）或 strict A/B（严格 A/B）结论前，必须执行 Evidence Doctor 检查；如果当前 topic 尚无脚本化输入，也要按本文人工填写检查结果：

- 生成 Std/RVV benchmark compare（标量 / RVV 性能对比）表。
- 生成 repeated board（重复板卡测试）summary，尤其是 5-run、20-run 或 50-run 汇总。
- 生成 RVV-vs-RVV B/A（同一 RVV 二进制内候选相对基线收益）表。
- 生成 checksum、trace summary（跟踪摘要）、asm attribution 或 RVV instruction count（RVV 指令计数）。
- evaluation（函数级评估）、主题文档或 Handoff Packet（交接数据包）准备把数据写成 EvidenceDecision。

如果只做 quick smoke（快速冒烟）或单次诊断，Evidence Doctor 可以只输出 warning 级别的“证据不足”结果；不能因为没有 repeated board 就省略证据边界说明。

## 输入合同

Evidence Doctor 的推荐输入是机器可读 JSON manifest（证据清单）。仅有 Markdown 表格时，可以运行轻量检查，但该结果只能发现数值异常，不能证明 A/B 边界、checksum 或 asm 归属已经闭合。

manifest 字段、文件命名、topic-local wrapper（主题本地包装脚本）、Makefile target 和 alias（字段别名）规则的长期合同见 `evidence-manifest-and-naming.zh.md`。本文只规定 Evidence Doctor 为什么检查、检查哪些问题、finding 如何分级，以及发现异常后如何影响结论。

严格 A/B 或 production direct（真实生产路径证据）必须记录 baseline（基线）和 candidate（候选）两侧 metadata。如果某字段有意不同，必须在 manifest、summary 或 Handoff 中写明 `allowed_contract_mismatches` 和理由，并把证据角色降级到对应 cross-check（交叉检查）或 diagnostic（诊断），除非 reviewer 明确确认该差异不影响当前结论。

板卡性能证据还应记录环境字段，例如 device、taskset、governor、freq、temperature、VLEN 和 binary hash（或等价二进制身份）。这些字段缺失不必然推翻结果，但会削弱对长尾、方向反转和 run-to-run 波动的解释能力。若 bench 直接展示 Std/RVV timing，却没有 warmup_iterations，Evidence Doctor 应至少给出 warning，并把这类结果视为 no-warmup diagnostic，而不是默认性能结论。

## 输出分级

Evidence Doctor 输出固定分为三类：

- Errors：必须修正，否则不能作为 production evidence（生产证据）或严格性能结论。
- Warnings：可以继续分析，但结论必须说明风险、可能原因、处理方式，以及是否重跑或降级证据边界。
- Suggestions：不阻塞当前结论，但给出下一步可验证动作，例如扩大 runs、补 trace、补 asm 或拆分消融。

Warning 不是“数据错了”。Warning 的含义是：这个信号需要 agent 停下来解释，不能无声跳过。

每个 finding（发现项）应尽量包含以下字段：

```text
signal:
severity:
case:
observed_pattern:
why_suspicious:
possible_non_bug_explanations:
possible_bug_or_evidence_issues:
recommended_checks:
conclusion_policy:
```

`possible_non_bug_explanations` 很重要。Evidence Doctor 不能把所有异常都写成实现错误；它应同时提示测量噪声、环境波动、编译差异、输入分布、case 命名、metadata 缺失等非 bug 解释。

## 第一层：数据契约检查

数据契约检查回答：“这批 evidence 是否真的可比较、可复现、可解释？”

### 必须阻塞的 Error

下列情况默认是 Error：

- checksum 或 correctness fingerprint（正确性指纹）不一致，却准备写性能结论。
- strict A/B 缺少 baseline 或 candidate 任一侧 metadata。
- strict A/B 两侧的 boundary、row_source、solve、checksum_policy、timer_boundary、gate、mask 或 reduction 不一致，且没有明确降级为 cross-check。
- summary 没有 comparison 或 comparison 结构不可解析，却被当成证据通过。
- 表格声称 production direct 或 production-ready（可接入生产），但 public entry（公开入口）、fallback（回退路径）、asm attribution 或 board performance 缺失，且没有降级说明。

这些 Error 不表示一定有实现 bug；它们表示当前 evidence 不能支撑所声明的结论角色。

### 必须暴露的 Warning

下列情况默认是 Warning：

- summary 名称暗示 strict A/B、production direct 或 production-ready，但 metadata 只支持 diagnostic、cross-check 或 unknown role。
- std/RVV（标量 / RVV）对比不是同一个 case 的两个 build，而是不同 wrapper、row source、输入规模或数据口径。
- asm boundary（反汇编边界）缺失或目标 RVV 指令无法归因到 hot symbol（热点符号）。
- run_count、iterations、warmup_iterations、device、taskset、governor、freq、temperature 或 binary hash 缺失。
- warmup_iterations 为 0，而 summary / manifest 又在展示 Std/RVV timing。
- output summary 与 raw log、analysis script、binary 或文档引用的 run label 不能对应到同一批输入。

这些 Warning 可以继续保留，但 summary、evaluation 或 Handoff 必须说明它们如何影响结论边界。

## 第二层：异常信号检测

异常信号检测回答：“即使数据契约看起来成立，数值模式是否值得停下来想一想？”

常见 warning 信号包括：

- 某个 point type（点类型）、size（规模）或 row source policy（行来源策略）与同组其它 case 方向明显不同。
- B/A 的平均或 median（中位数）正向，但 `B/A < 1` 的频率偏高。
- median 正向，但 min/max、p10/p90 或 p95 长尾明显。
- 5-run 与 20-run / 50-run 的结论反转。
- checksum 一致，但耗时波动显著高于同组 case。
- RVV instruction count 更少，但实际 B/A 退化。
- asm 显示目标指令存在，但 hot path attribution（热点路径归属）不闭合。
- std/RVV speedup（同一 case 标量相对 RVV 的加速比）很好，但 RVV-vs-RVV B/A 退化。
- Std 侧 candidate 相对 baseline 更快，而 RVV 侧 candidate 反而更慢，或反过来。
- component no-solve 比 full estimate 更慢，或 full estimate 相比 component no-solve 的 solve delta 在同组候选中明显离群。
- warm-up 后前几轮仍明显慢。
- 不同 run batch、不同 binary 或不同环境字段给出方向相反的结果。

这些信号都不自动判错。它们要求 worker 给出可验证解释，例如：

- 当前 case 的 layout、AoS stride（结构数组跨步）、字段 offset、cache locality（缓存局部性）或 gather（离散加载）不同。
- 温度、governor、freq、taskset 或系统调度导致测量波动。
- RVV 候选减少了指令数，但增加了寄存器压力、spill/reload、load/store 或 `vsetvli` 开销。
- baseline / candidate 的边界、row source、solve 或 timer boundary 其实不同。
- std/RVV speedup 被 std 侧变化放大，不能代表 RVV-vs-RVV 候选收益。

## 第三层：诊断反馈与纠正动作

Evidence Doctor finding 不能只写“异常”。它必须把异常转成下一步动作。

常见纠正策略：

| 信号 | 默认动作 | 结论策略 |
| --- | --- | --- |
| checksum 不一致 | 先修 correctness，检查 checksum_policy、输入对象、warm-up 是否一致 | 禁止性能结论 |
| strict A/B 边界不一致 | 按同边界重跑，或改名为 mixed-boundary cross-check（混合边界交叉检查） | 降级证据角色 |
| component no-solve 比 full estimate 更慢 | 检查 sink 口径、wrapper、warm-up、频率、缓存和计时边界 | 不能直接解释成“少做更慢是正常” |
| `B/A < 1` 频率高 | 报告频率，扩大 runs，按 point type / size / row source 分组 | 不得只写 mean / median 正向 |
| 长尾明显 | 查 per-iteration trace、温度、governor、freq、taskset | 保留异常值并解释，不能先验剔除 |
| asm 不闭合 | 重新 dump asm，检查 hot symbol、inline、clone、spill/reload | 不能把“二进制有 RVV 指令”写成 hot path 证据 |
| 指标冲突 | 区分 std/RVV speedup 和 RVV-vs-RVV B/A | 生产候选优先看同一 RVV binary 内 B/A |
| output / binary 疑似不一致 | 清理旧日志，记录 binary hash，重跑 summary | 未确认前不能写稳定结论 |
| group outlier（组内离群） | 按 case 单独报告，检查 layout / gate / fallback | 不能把其它 case 收益外推到该 case |

如果人工决定继续接入，即使存在 Warning，也必须写清：为什么可以接受、风险边界是什么、后续如何复核。不要只写“人工判断可接受”。

## 脚本接口

跨 topic 可复用脚本放在：

```text
test-rvv/script/evidence_doctor.py
```

该脚本适合检查通用 JSON manifest 和 Markdown summary 表中的数值分布、A/B metadata、checksum、asm boundary、环境字段和异常模式。

生成模板：

```bash
python3 test-rvv/script/evidence_doctor.py --write-template /tmp/evidence_manifest.example.json
```

检查 JSON manifest：

```bash
python3 test-rvv/script/evidence_doctor.py \
  --manifest <topic-output>/evidence_manifest.json \
  --output <topic-output>/evidence_doctor.md
```

对旧 Markdown summary 做轻量检查：

```bash
python3 test-rvv/script/evidence_doctor.py \
  --summary-md <topic-output>/summary.md \
  --output <topic-output>/evidence_doctor.md \
  --fail-on never
```

Markdown summary 模式会输出 `summary_only_metadata_missing` warning。它只能作为 reviewer aid（审查辅助），不能写成完整 Evidence Doctor 通过。

如果检查逻辑依赖某个 topic 的 case label、函数名、helper 名、字段布局、反汇编符号或目录结构，应写 topic-local wrapper（主题本地包装脚本），例如：

```text
test-rvv/<module>/<topic>/script/generate_<topic>_evidence_manifest.py
```

该 wrapper 负责把 topic-specific raw log（当前主题特定原始日志）转成通用 manifest，再调用全局 `test-rvv/script/evidence_doctor.py`。

## 字段名稳定性与兼容层

字段名稳定性由 `evidence-manifest-and-naming.zh.md` 维护。Evidence Doctor 可以为旧 summary 或早期 wrapper 兼容少量 alias（字段别名），但 alias 只用于迁移；新 wrapper 必须输出规范字段。若字段名拼错且不在 alias 表中，strict A/B 下应触发缺字段 Error 或运行合同 warning，从而阻止严格结论。

## Summary、文档和 Handoff 接入

Evidence Doctor 结果必须进入离开当前对话后仍能审查的位置：

- output summary 可以新增 `Evidence Doctor` 小节，列出 Errors / Warnings / Suggestions 摘要。
- topic 目录可以保留 `evidence_doctor.md` 作为 summary artifact（摘要证据产物）。
- evaluation 或主题文档引用 doctor report 路径和关键 finding，不复制长 raw log。
- Handoff Packet 必须输出 `evidence_doctor_result`，说明是否运行脚本、输入 manifest / summary、结果分级、未解决 warning、处理动作，以及是否因此重跑、降级证据边界、修改结论或保留风险。

如果本轮没有运行脚本，Handoff 中也要写 `not_run` 和原因，并按本文人工检查 Errors / Warnings / Suggestions。不能因为脚本尚未接入当前 topic 就省略 Evidence Doctor。

## 常见误用

- 不要把 Warning 当成失败，也不要把 Warning 隐藏在 summary 外。
- 不要用 std/RVV speedup 替代 RVV-vs-RVV B/A。
- 不要因为 checksum 一致就跳过数值波动、asm 归属和环境检查；checksum 只能说明当前 checksum_policy 下的输出一致。
- 不要用 QEMU timing（QEMU 计时）支撑性能结论；QEMU 只支持 correctness、路径和日志形状。
- 不要把一个历史 mixed-boundary 案例写成机制中心。mixed-boundary 只是数据契约检查的一类；Evidence Doctor 的通用目标是捕捉各种证据声明与数据模式不匹配。
