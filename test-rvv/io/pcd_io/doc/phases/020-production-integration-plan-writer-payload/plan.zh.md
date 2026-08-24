# Phase 020 计划：writer payload 生产接入计划

本阶段只关闭 PI1 production integration plan（生产接入计划）。目标是把 Phase 010 的
production-shaped diagnostic（生产形态诊断）候选转换成可审查的生产补丁合同。production
（生产源码）仍不在本阶段修改。

## 阶段意图和边界

validated_scope：

- production entry（生产入口）：`PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)`。
- candidate family（候选实现族）：4 字节字段 AoS（结构数组）到 field-major（字段连续）打包使用 RVV stride load/store（跨步加载 / 连续存储），随后继续使用现有 `pcl::lzfCompress`。
- data shape（数据形态）：`PCLPointCloud2`，字段过滤后每个有效 field 的 `field.count * getFieldSize(field.datatype) == 4`，字段 offset 和 `cloud.point_step` 按 4 字节对齐。
- evidence basis（证据依据）：Phase 000 component ablation 和 Phase 010 writer payload production-shaped diagnostic。

unvalidated_scope：

- `PCDReader::readBodyBinary`、finite scan（有限值扫描）、file-name overload（文件名重载）、mmap / file lock / `msync`。
- `io/include/pcl/io/impl/pcd_io.hpp` templated writer（模板 writer）和 ASCII writer。
- non-4-byte fields、field count 生成的 8 字节或混合字段、datatype switch（数据类型分流）、generic all-PCD layout。
- public API（公开接口）变更、跨 topic 公共 helper 变更、`doc-rvv/io/pcd_io-RVV.zh.md` 长期生产文档。

phase_closeout_boundary：

- 本阶段完成后只能声明 PI1 范围、fallback（回退路径）、测试计划和暂停条件已冻结。
- 本阶段不能声明 production-ready（可接入生产）、adopted production behavior（已采纳生产行为）或生产性能成立。

## 当前状态清单

| area | current evidence | path |
| --- | --- | --- |
| component ablation | pack positive，unpack positive / weak-positive；Errors=0，Warnings=0 | `log/board/component_ablation_repeat_5/summary.md`、`evidence_doctor.md` |
| writer payload shaped | `writer_payload_xyzi_307k` median 1.18x；`writer_payload_padded_xyzi_307k` median 1.12x；Errors=0，Warnings=0 | `log/board/writer_payload_repeat_5/summary.md`、`evidence_doctor.md` |
| correctness | Std/RVV 各 4 个 gtest 通过，payload bytes 和 scalar 一致 | `make -C test-rvv/io/pcd_io run_test_compare` |
| QEMU smoke（仿真器小型验证） | correctness 和日志形状可运行；QEMU timing 不作为性能证据 | `make -C test-rvv/io/pcd_io run_qemu_smoke` |
| asm attribution（反汇编归属） | bench binary 可见 `vlse32.v` / `vse32.v`，归属到 test helper 内联路径 | `make -C test-rvv/io/pcd_io dump_bench_rvv` |
| production source | 未修改 | `io/src/pcd_io.cpp` |
| production topic doc | not_applicable | 无 `doc-rvv/io/pcd_io-RVV.zh.md` |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / production-shaped payload wrapper |
| 当前决策问题 | implementation-shape and bounded production probe（有界生产探针）是否可控 |
| diagnostic 是否可外推到 production | partial yes。它覆盖 pack + LZF + 8 字节 payload header；不覆盖文本 header、ostream flush、mmap 和 file-name overload。 |
| comparison-boundary / baseline mismatch 风险 | yes。Phase 010 不是 public overload，也没有 production dispatch。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | yes，但只允许 `std::ostream` overload 的 4 字节字段 pack；production direct 若低于阈值或证据矛盾，停在 PI5 用户检查点。 |
| clean adoption 是否需要同一 production boundary 内 A/B | yes。PI2-PI5 必须补真实 public overload Std/RVV、fallback、production asm 和 board production repeated evidence。 |

## PI1 候选结构

建议 PI2 采用 production-local helper（生产文件局部 helper）而不是公开 API：

| helper / gate | shape | reason |
| --- | --- | --- |
| `writeBinaryCompressedStd` 或邻近 internal free helper | 保留现有标量主体，负责所有 fallback 和非 RVV 构建 | public entry（公开入口）维持“语义检查 -> RVV 短路 -> Std fallback”形状。 |
| `tryWriteBinaryCompressedRVV` 或等价 helper | 只在 `__RVV10__` 下编译，内部完成 layout gate、4 字节字段 pack、LZF 和 payload 写出 | 避免非 RVV build 常驻无意义 stub。 |
| field layout gate | 运行期检查字段过滤后的 size、offset、point_step、data_size 和空输入 | 保证不支持 layout 自然回退现有标量路径。 |

production comments（生产注释）只解释覆盖范围、fallback 和数据布局边界。

## Fallback 矩阵

| fallback condition | expected behavior | PI3 evidence |
| --- | --- | --- |
| non-RVV build 或未定义 `__RVV10__` | 只编译和运行 Std helper | Std build production direct test |
| `cloud.fields.empty()` | 保持现有 `-1` 错误 | existing semantic test or direct unit |
| `cloud.data.empty()` | 保持现有 warning 后继续生成空 payload 行为 | empty-data direct test |
| filtered effective fields empty | 回退 Std；不走 RVV pack | fallback path test |
| 任一有效 field size 不是 4 | 回退 Std | mixed field size fallback test |
| 任一有效 field offset 不是 4 字节对齐 | 回退 Std | unaligned offset fallback test |
| `cloud.point_step` 不是 4 字节对齐 | 回退 Std | unaligned point_step fallback test |
| `data_size == 0` | 保持现有 compressed_size=0 / data_size=0 payload | zero point direct test |
| `data_size * 3 / 2 > uint32 max` | 保持现有 `-2` 错误 | overflow guard test or documented unreachable large allocation boundary |
| `_` padding field | 忽略 padding field，与现有标量语义一致 | padding field direct test |
| file-name overload | 不直接分流；仍调用 ostream overload 后执行现有 mmap/file 写出 | PI2 不修改 file-name overload；必要时只做 smoke |

## Production direct test 计划

PI3 必须新增真实入口测试，不复用 production-shaped helper 代替：

| test family | must prove |
| --- | --- |
| public ostream RVV hit | RVV build 下 4 字节字段输入命中 RVV helper，并生成与 Std 路径相同的 binary_compressed payload。 |
| non-RVV build | Std build 编译通过，输出与 RVV build fallback 语义一致。 |
| fallback gates | mixed field size、unaligned offset、empty effective fields 和 zero point 不误命中 RVV。 |
| `_` padding semantics | padding 字段被过滤，payload 与现有标量路径一致。 |

如果生产 helper 没有可观测 path marker（路径标记），PI3 需要增加 test-only seam（测试专用切入点）或通过 binary payload 和反汇编联合证明。该 seam 不得进入 public API。

## 反汇编和板卡计划

| evidence | command / target | expected boundary |
| --- | --- | --- |
| production asm | PI2 后新增 production dump target 或扩展现有 `dump_bench_rvv` | RVV 指令应归属到 production helper、RVV wrapper 或其内联 public overload。 |
| board smoke | PI2 后先跑 production direct smoke | 证明板卡可运行、checksum / payload 非空。 |
| board repeated | PI4 跑 production public Std/RVV repeated summary | 性能结论只来自板卡或目标硬件。 |
| Evidence Doctor | 对 PI4 manifest 运行 doctor | Errors 必须为 0；Warnings 必须解释或降级。 |
| registry | 记录 PI4 summary / manifest / doctor | 恢复和提交前能检查 freshness（新鲜度）。 |

rerun budget（复跑预算）：PI4 默认 5-run repeated。若 decision bucket（决策桶）与 Phase 010 方向一致且
Errors=0 / Warnings 已解释，不自动扩大预算。若跨过 positive / neutral 边界，最多追加一次同边界确认复跑。

## 优化矩阵

| candidate family | entry | layout | correctness / fallback | bench / board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| writer RVV 4-byte pack production probe | `writeBinaryCompressed(std::ostream&, ...)` | 4 字节有效字段，offset 和 point_step 4 字节对齐 | PI3 planned | PI4 planned | PI4 planned | PI4 planned | PI1 plan only |
| reader unpack RVV | `readBodyBinary` | compressed body unpack | deferred | deferred | deferred | deferred | not in PI1 |
| finite scan RVV mask | `readBodyBinary` dense scan | float/double finite fields | deferred | deferred | deferred | deferred | not in PI1 |

## 实现和测试动作

| action | artifact / command | completion criterion |
| --- | --- | --- |
| A1 写 PI1 plan | 本文件 | 范围、fallback、test、asm、board 和暂停条件明确。 |
| A2 回填 PI1 result | `result.zh.md` | 逐项说明 PI1 是否闭合和为什么不进入 PI2。 |
| A3 同步 phase index、roadmap、matrix、evaluation、README | topic-local docs | 默认恢复入口从 PI1 更新到 PI2 authorization checkpoint。 |
| A4 验证文档与证据状态 | `git diff --check -- test-rvv/io/pcd_io`、registry check | 无 whitespace error；Phase 000 / 010 registry fresh。 |

## 继续 / 停止条件

本阶段应停止在 PI1，不进入 PI2，除非用户明确确认修改 production：

- 继续到 PI2 会修改 `io/src/pcd_io.cpp`，属于 production patch（生产补丁）。
- 当前 prompt 授权继续 RVV topic 和 topic-local phase 文档；它不把 production patch 的最终采纳或回滚授权一并给出。
- 若用户确认推进 production integration loop，下一轮默认从 `PI2 production_patch` 开始，范围只按本文件冻结的 `std::ostream` overload 和 4 字节字段 pack 执行。

stop_condition_hit：`production_patch_requires_user_confirmation`。

next_phase_default：`PI2 production_patch for writer std::ostream 4-byte field payload`，等待用户确认。

## 文档更新清单

- `doc/phases/020-production-integration-plan-writer-payload/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/pcd_io-evaluation.zh.md`
- `README.zh.md`

`doc-rvv/io/pcd_io-RVV.zh.md` 仍 not_applicable。
