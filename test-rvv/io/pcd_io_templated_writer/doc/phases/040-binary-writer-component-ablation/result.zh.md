# Phase 040 结果：binary writer component ablation

## 当前结论

本阶段完成 `PCDWriter::writeBinary<PointT>(file_name, cloud)` packed binary output
（按点连续的有效字段二进制输出）循环的 test-only component ablation（测试专用组件消融）。
production（生产源码）未修改；`io/include/pcl/io/impl/pcd_io.hpp` 仍保持原状。

EvidenceDecision（证据决策）：`attempted positive / binary-component-candidate`。
该结论只说明 binary writer 的 4 字节字段 packed loop 值得继续做 production-shaped diagnostic
（生产形态诊断）或 PI1 计划；它不是 production direct（真实生产路径证据），也不能 clean adopt
（干净采纳）到生产。

## 计划回填

| action | result | evidence |
| --- | --- | --- |
| E1 写 binary writer failing correctness test | done；新增 binary correctness case 后，helper 缺失时曾出现编译失败，作为 TDD RED。 | `src/test_pcdtw.cpp` |
| E2 实现 test-only binary pack helper | done；新增 scalar / RVV binary pack helper 和 `PathKind::Binary*`。 | `include/impl/pcdtw_support.hpp` |
| E3 扩展 bench case | done；新增 `binary_*` case，checksum 在计时后计算。 | `src/bench_pcdtw.cpp` |
| E4 QEMU smoke / asm | done；QEMU smoke 只作 correctness / log-shape；asm 可见 `vlse32.v` 和 `vsse32.v`。 | `run_qemu_smoke`、`dump_bench_rvv` |
| E5 板卡 repeated binary bench | done；5-run repeated summary 已生成。 | `log/board/binary_component_repeat_5/summary.md` |
| E6 Evidence Doctor / registry | done；Doctor 无 Error，有 1 个 small case outlier Warning；registry fresh。 | `log/board/binary_component_repeat_5/evidence_doctor.md`、`log/evidence_registry.json` |
| E7 文档回填 | done；phase result、roadmap、matrix、evaluation 和 topic-local doc suite 已同步。 | 本文件及相关文档 |

## 正确性与路径证据

`run_test_compare` 在 Std / RVV build 下运行 6 个 gtest。新增 binary tests 覆盖：

- tail padding layout（尾部填充布局）下，`packBinaryFieldsCandidate` 输出和标量参考链路 byte-equal。
- mixed field size（混合字段大小）下，candidate 回退到 `PathKind::BinaryScalarFallback`。

QEMU smoke 使用：

```bash
make -C test-rvv/io/pcd_io_templated_writer run_qemu_smoke BENCH_ARGS="--case-filter binary_* --iterations 2 --warmup-iterations 1"
```

它只证明 binary case 构建、运行和日志形状可解析，不提供性能结论。

`dump_bench_rvv` 的 bench binary 中可见 `vlse32.v` 和 `vsse32.v`。这证明 diagnostic bench
里存在预期 RVV stride load / stride store（跨步加载 / 跨步存储）指令；它仍不是 production
symbol attribution（生产符号归属）。

## 板卡证据

板卡 repeated target：

```bash
make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_binary_repeated
```

run budget 为 5 次，每次 `--iterations 20 --warmup-iterations 3`，device 为 Milkv-Jupiter。

| case | mean speedup | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `binary_pointxyzrgb_4f_262k` | 1.2724x | 1.2738x | 1.2479x | 1.2897x | positive |
| `binary_pointxyzrgb_4f_padding_262k` | 1.1730x | 1.1842x | 1.1446x | 1.1924x | positive |
| `binary_pointxyzrgb_4f_small_512` | 6.8500x | 6.8125x | 6.8125x | 6.9688x | positive but outlier-scoped |

大规模 no-padding 和 padding case 均超过 Phase 040 的 positive 阈值 `median >= 1.08x`。
small case 的数值也为正向，但 Evidence Doctor 将它标成 group outlier（组内离群）；因此它只作为
小规模 smoke 信号单独报告，不把 `6.8x` 外推到大规模或 production。

## Evidence Doctor 与 registry

Evidence Doctor：

- report：`log/board/binary_component_repeat_5/evidence_doctor.md`
- result：Errors=0，Warnings=1，Suggestions=0
- Warning：`binary_pointxyzrgb_4f_small_512` 为 group outlier。

处理策略：

- large / padding case 的 decision bucket 不受 small outlier 影响，继续作为 binary component positive。
- small case 单独降级为 smoke-shaped positive，不用于预测 production 收益。
- 本阶段不进入 production，因此 Doctor Warning 不阻塞 component-level 结论。

`log/evidence_registry.json` 已登记 summary、manifest 和 Doctor，run label 为
`board-pcdtw-binary-component-repeat-phase040`，freshness_state 为 `fresh`。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | `binary_component_ablation`（binary writer 组件消融） |
| A/B boundary | test helper / bench wrapper；不含 public file write、mmap、header 或 error path。 |
| 当前决策问题 | binary packed loop 是否值得继续 shaped diagnostic 或 PI1 计划。 |
| diagnostic 是否可外推到 production | unknown；positive 只能支持下一阶段，不支持直接 production adoption。 |
| comparison-boundary / baseline mismatch 风险 | yes；真实 `writeBinary` 的 file mapping、header 和 write 成本都不在计时边界内。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段为 positive；若后续 shaped 结果转弱或负，需先做 mismatch audit，不能直接拒绝 production probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段没有 production boundary 内 A/B。 |

## 下一步判断

Phase 040 不改变 Phase 010/020 的 compressed writer production 状态：
`writeBinaryCompressed<PointT>` 仍是 `partial-production-candidate / pending-production-authorization`，
PI2 修改 production 仍需要用户明确授权。

binary writer 现在从 `deferred` 升级为 `attempted positive / binary-component-candidate`。
可继续动作是新建一个窄阶段，评估 `writeBinary<PointT>` 的 production-shaped diagnostic 或 PI1 计划；
该动作仍不得自动修改 production。若用户授权 production integration loop，compressed writer 的 PI2
仍按 Phase 020 计划优先，因为它已有 pack+LZF production-shaped positive 证据。
