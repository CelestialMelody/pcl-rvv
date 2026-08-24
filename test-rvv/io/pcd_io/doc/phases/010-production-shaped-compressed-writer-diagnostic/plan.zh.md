# Phase 010 计划：production-shaped compressed writer diagnostic

## 阶段意图和边界

本阶段要验证 `PCDWriter::writeBinaryCompressed(std::ostream&, const PCLPointCloud2&, ...)` 中
compressed payload（压缩 payload）生成的 production-shaped diagnostic（生产形态诊断）是否仍保留
Phase 000 的 pack 收益。阶段仍只修改 `test-rvv/io/pcd_io`，不修改 `io/src/pcd_io.cpp`。

validated_scope：

- writer payload：按 production 的字段过滤、`fsize` 计算、AoS 到 field-major 打包、LZF compression
  和 8 字节 compressed / uncompressed size header 生成。
- layout：4 字节 `xyzi` 连续布局和尾部 padding `xyzi` 布局。
- evidence role：production-shaped diagnostic。它比 component ablation 更接近 production，但仍不是
  production direct。

unvalidated_scope：

- `generateHeaderBinaryCompressed` 的文本 header 输出和 locale。
- file-name overload 的 `mmap` / file lock / page stretch / `msync`。
- reader shaped path、finite scan、non-4-byte fields、`impl/pcd_io.hpp` templated writer。
- production dispatch、fallback 和 `doc-rvv` production 长期主题文档。

## 当前状态清单

| 对象 | 当前状态 |
| --- | --- |
| Phase 000 | component pack positive；component unpack positive / weak-positive；doctor 无 Error / Warning。 |
| production | 未修改。 |
| tests | 现有 gtest 只覆盖 layout conversion，不覆盖 LZF payload。 |
| bench | 现有 bench 只计 layout conversion 组件。 |
| board | 用户说明板卡可用；本阶段需要 5-run repeated board summary。 |

## 假设与候选族

候选继续使用 Phase 000 的 4 字节字段 RVV stride path。A/B 两侧共享同一 payload wrapper、row source、
LZF compression、checksum policy 和 timer boundary，只把 pack helper 从 scalar reference 换成 RVV candidate。
如果 LZF 主导成本，writer payload speedup 可能从 component 的 1.12x-1.20x 降为 weak-positive 或 neutral。

## 优化矩阵

本阶段只允许更新 `production-shaped writer diagnostic` 行。即使 writer payload positive，也只能输出
`partial-production-candidate` 或继续到 PI1 的候选范围，不能直接修改 production。

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| B1 写 RED correctness test | `src/test_pcd_io.cpp` | 测试引用 payload scalar / candidate helper；实现前应编译失败。 |
| B2 实现 test-only payload helper | `include/impl/pcd_io_support.hpp` | scalar 与 candidate payload bytes 完全一致；混合字段 fallback 可见。 |
| B3 扩展 bench case | `src/bench_pcd_io.cpp` | 新增 `writer_payload_xyzi_307k` 和 `writer_payload_padded_xyzi_307k`。 |
| B4 扩展 manifest wrapper | `script/generate_pcd_io_evidence_manifest.py` | writer payload case 输出 `production_shaped_diagnostic` metadata。 |
| B5 扩展 Make / board target | `Makefile` | 新增 writer payload repeated target，生成 summary / manifest / doctor / registry。 |
| B6 验证 | `run_test_compare`、`run_qemu_smoke`、`dump_bench_rvv`、writer payload board repeated | correctness、asm、board、doctor 全部回填 result。 |
| B7 文档回填 | `result.zh.md`、evaluation、roadmap、matrix、Handoff | 写清是否进入 PI1 或暂缓。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic |
| A/B boundary | test helper / production-shaped payload wrapper，不是 public overload |
| 当前决策问题 | RVV-vs-scalar writer payload 是否值得进入 PI1 production integration plan |
| diagnostic 是否可外推到 production | partial；它包含 pack 和 LZF payload，但不包含 textual header、ostream flush、file-name mmap 和真实 public dispatch。 |
| comparison-boundary / baseline mismatch 风险 | low within this phase；A/B 两侧使用同一 wrapper 和 LZF，只替换 pack helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | weak-positive 可作为 partial-production-candidate；neutral / negative / unstable 不进入 PI1，除非后续 profile 证明 LZF 外成本不是主导。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；PI2-PI5 后仍需 production direct、fallback、asm 和 board evidence。 |

## 板卡复跑预算和决策桶

- run budget：5 次 repeated board run，`--case-filter writer_payload --iterations 20 --warmup-iterations 3`。
- positive：writer payload median speedup >= `1.08x` 且无 checksum mismatch。
- weak-positive：median speedup 在 `1.03x` 到 `1.08x`，且 helper 小、Doctor 无 Error。
- neutral：`0.98x` 到 `1.03x`。
- negative：median speedup < `0.98x`。
- unstable：5 次内 direction 或 decision bucket 摇摆，或 Doctor Error / checksum 问题无法解除。

## 继续 / 停止条件

默认继续到 B7。合法停止条件：

- 构建、QEMU、反汇编或板卡 target 失败且不能在当前 topic 内修复。
- Evidence Doctor Error 无法解除。
- writer payload neutral / negative / unstable，且 roadmap 中没有当前授权内的高优先级 writer 动作。
- writer payload positive / weak-positive 后，进入 PI1 需要修改 production 或扩大到 public dispatch；此时应输出
  partial-production-candidate Handoff，等待用户确认是否进入 production integration loop。
