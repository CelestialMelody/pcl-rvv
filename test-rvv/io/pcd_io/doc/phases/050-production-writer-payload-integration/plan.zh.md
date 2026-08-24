# Phase 050 计划：writer payload production integration

本阶段执行 PI2-PI5 production integration loop（生产接入闭环）。用户已确认：若接入后板卡测试显示有收益即可采纳；
接入后的正式结论必须以 production direct（真实生产入口直连）板卡数据为准，若证据支持采纳再创建
`doc-rvv/io/pcd_io-RVV.zh.md`。

## 阶段意图和边界

validated_scope：

- production entry（生产入口）：`PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)`。
- candidate family（候选实现族）：4 字节有效字段从 AoS（结构数组）打包到 field-major（字段连续）buffer 时使用 RVV
  stride load/store（跨步加载 / 连续存储）；LZF compression（LZF 压缩）、header 生成和 ostream 写出保持现有语义。
- layout gate（布局验收）：过滤 `_` padding field 后，所有有效字段 size 为 4，字段 offset 和 `cloud.point_step`
  按 4 字节对齐，`cloud.width * cloud.height > 0`。
- adoption policy（采纳策略）：production direct 板卡 repeated summary 为 positive 或 weak-positive，且 Errors=0、
  fallback / asm / registry 闭合时，可进入待用户确认采纳；正式 `doc-rvv` 使用接入后的板卡数据。

unvalidated_scope：

- `PCDReader::readBodyBinary`、finite scan（有限值扫描）、file-name overload（文件名重载）中的 mmap / file lock /
  `msync` 细节、templated writer、ASCII writer、non-4-byte fields 和 generic all-PCD layout。
- `doc-rvv/io/pcd_io-RVV.zh.md` 在 PI5 证据成立前不创建；若 production direct 证据不支持采纳，停止在用户检查点。

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| P1 写 production direct correctness test | `src/test_pcd_io.cpp` | public writer 生成 payload 与 test-only scalar oracle 一致，fallback cases 不改变语义。 |
| P2 实施 production patch | `io/src/pcd_io.cpp` | 原标量主体抽成 Std helper；`__RVV10__` 下 4 字节 gate 命中 RVV pack，其它路径回退 Std。 |
| P3 production public bench | `src/bench_pcd_io.cpp`、`Makefile`、manifest script | 新增 `production_writer_*` case、board repeated target、Evidence Doctor 和 registry 记录。 |
| P4 验证 | `run_test_compare`、`run_qemu_smoke`、`dump_bench_rvv`、board repeated | correctness、QEMU 路径、asm、板卡、doctor、registry 全部回填 result。 |
| P5 文档 closeout | phase result、matrix、roadmap、evaluation、Handoff、必要时 `doc-rvv` | PI5 后按接入后证据决定采纳、暂缓或回滚请求。 |

## Fallback 矩阵

| fallback condition | expected behavior | evidence |
| --- | --- | --- |
| non-RVV build 或未定义 `__RVV10__` | 只编译 Std helper，payload 与 scalar oracle 一致。 | Std side `run_test_compare`。 |
| 4 字节字段、offset 和 point_step 对齐 | RVV build 可命中 production RVV pack；payload 与 scalar oracle 一致。 | RVV side correctness + production asm + board bench。 |
| mixed field size | 回退 Std；payload 仍与 scalar oracle 一致。 | fallback test。 |
| unaligned field offset | 回退 Std；payload 仍与 scalar oracle 一致。 | fallback test。 |
| unaligned point_step | 回退 Std；payload 仍与 scalar oracle 一致。 | fallback test。 |
| `_` padding field | padding field 被过滤；payload 与 scalar oracle 一致。 | padding test。 |
| zero points / empty data | 保持现有 empty payload 语义。 | direct test 或 existing semantic check。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-public / production-detail after PI2 |
| A/B boundary | public `std::ostream` overload；detail 是 production-local RVV pack helper。 |
| 当前决策问题 | 当前 public RVV path 是否比当前 public scalar path 更快，且 fallback 是否语义一致。 |
| diagnostic 是否可外推到 production | Phase 010 只能作为进入 probe 的依据；PI5 结论以本阶段 production direct 证据为准。 |
| comparison-boundary / baseline mismatch 风险 | 通过同一 public overload 的 Std/RVV bench 降低错配风险。 |
| weak-positive 是否允许采纳 | 用户已确认有收益即可采纳；仍要求实现小、fallback 简单、Errors=0 和文档边界完整。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前 production 之前无已采纳 RVV family；Std/RVV public A/B 足以判断是否采纳本窄 probe。 |

## 继续 / 停止条件

- 若 production direct correctness、fallback、asm 或板卡不可闭合，停止在 PI5 用户检查点，不自动回滚。
- 若板卡 repeated 为 neutral / negative / unstable，暂停并报告不建议采纳的证据；回滚需用户授权。
- 若需要扩大到 reader、file-name overload、templated writer、non-4-byte fields、public API 或跨 topic helper，立即停止并重新请求授权。

next_phase_default：执行 PI2 production patch，然后连续推进 PI3-PI5。
