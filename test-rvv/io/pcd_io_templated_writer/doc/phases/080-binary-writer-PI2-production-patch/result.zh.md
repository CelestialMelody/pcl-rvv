# Phase 080: binary writer PI2 production patch 结果

## 结论

本阶段完成了 `PCDWriter::writeBinary<PointT>(file_name, cloud)` 的 PI2-PI5 production integration loop
（生产接入闭环）验证。production direct（真实生产入口直连）正确性、fallback（回退路径）和 asm attribution
（反汇编归属）均闭合，但 Milkv-Jupiter 板卡 repeated summary 显示当前 field-outer RVV path
（按字段外层循环、`vlse32.v` 跨步加载、`vsse32.v` 跨步写 packed output）没有生产收益：

- `production_binary_pointxyzrgb_4f_compact_262k` mean `0.9842x`，median `0.9827x`，4/5 run 低于 1。
- `production_binary_pointxyzrgb_4f_padding_262k` mean `0.9727x`，median `0.9759x`，5/5 run 低于 1。
- `production_binary_pointxyzrgb_4f_compact_small_512` mean `0.9810x`，4/5 run 低于 1，只能作为 smoke。
- Evidence Doctor（证据体检）结果为 Errors=3，Warnings=0，Suggestions=0；三个 Error 都是
  `ba_degradation_frequency`，说明当前 production-public（真实公开入口）证据不能支撑采纳。

PI5 EvidenceDecision：`rollback/no-production`。用户已确认负收益结果可回滚；当前
binary writer production patch、只服务该 patch 的 production-direct tests 和
`production_binary_*` bench / board target 已移除。Phase 080 的 board summary、manifest、Doctor
和本 result 作为历史 production-public negative evidence（真实公开入口负向证据）保留，用于说明为什么
不采纳该实现。compressed writer adopted production behavior 不受影响。

## 实际执行范围

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| binary production patch | rolled back | `io/include/pcl/io/impl/pcd_io.hpp` | 已移除 `packBinaryFieldsStd`、`binaryFieldsSupportRvv4BytePack`、`packBinaryFieldsRVV`、binary hook 和 public overload 短路分流；`writeBinary<PointT>(file_name, cloud)` 回到原 point × field `memcpy` 标量循环。 |
| production direct tests | historical / removed after rollback | `make -C test-rvv/io/pcd_io_templated_writer run_test_compare` | 回滚前 Std/RVV 各 11 个 gtest 通过；回滚后当前测试集为 Std/RVV 各 8 个 gtest，保留 compressed public direct 和 binary component ablation，不再包含 binary production-direct hook 测试。 |
| QEMU smoke | historical / removed after rollback | `make -C test-rvv/io/pcd_io_templated_writer run_bench_rvv BENCH_ARGS="--case-filter production_binary_* --iterations 2 --warmup-iterations 1"` | 回滚前用于验证三个 `production_binary_pointxyzrgb_*` case 可运行；回滚后该 case-filter 不再作为当前 bench 入口。 |
| asm attribution | historical / removed after rollback | `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv` | 回滚前 `writeBinary<PCDTWPointXYZRGBField>` 和 `writeBinary<PCDTWPointXYZRGBFieldPadding>` 符号内可见 `vlse32.v` / `vsse32.v`；回滚后当前 production binary RVV 符号不存在。 |
| board repeated | done | `make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_binary_repeated` | 5-run 预算已用完；decision bucket 为 negative / no-adoption-recommended。 |
| Evidence Doctor / registry | done | `log/board/production_binary_repeat_5/{summary.md,evidence_manifest.json,evidence_doctor.md}`；`log/evidence_registry.json` | registry 已登记 Phase 080；Doctor Errors=3 阻止 production adoption。 |

## RED / GREEN

RED：production patch 前 `make -C test-rvv/io/pcd_io_templated_writer run_test_rvv` 因 binary writer hook symbols
缺失而失败。

GREEN（回滚前历史状态）：`make -C test-rvv/io/pcd_io_templated_writer run_test_compare` 通过 Std/RVV
各 11 个 gtest。回滚后当前状态由后续 rollback verification 覆盖：Std/RVV 各 8 个 gtest 通过。
被移除的 binary production-direct 测试曾覆盖：

- `BinaryPublicWriterMatchesScalarPackAndHitsRvvPath`：真实 `writeBinary<PointT>` 写临时 PCD；payload 与
  scalar pack byte-equal；RVV build hook 为 Rvv。
- `BinaryPublicWriterFallsBackForMixedFieldSizes`：mixed-size 点型输出与 scalar pack 一致；RVV build hook 为 Scalar。
- `BinaryIndicesOverloadKeepsScalarBehavior`：indices overload 输出只含指定 indices，保持原标量行为。

## Board Evidence

summary path：`test-rvv/io/pcd_io_templated_writer/log/board/production_binary_repeat_5/summary.md`

| case | mean speedup | median | min | max | decision |
| --- | ---: | ---: | ---: | ---: | --- |
| `production_binary_pointxyzrgb_4f_compact_262k` | `0.9842x` | `0.9827x` | `0.9552x` | `1.0136x` | negative：4/5 低于 1。 |
| `production_binary_pointxyzrgb_4f_padding_262k` | `0.9727x` | `0.9759x` | `0.9540x` | `0.9819x` | negative：5/5 低于 1。 |
| `production_binary_pointxyzrgb_4f_compact_small_512` | `0.9810x` | `0.9945x` | `0.9409x` | `1.0154x` | smoke-only negative。 |

board rerun budget（板卡复跑预算）：5 runs，20 iterations，3 warmup，预算已用完。decision bucket 稳定为
negative，不需要继续自动复跑。

## Evidence Doctor

Doctor path：`test-rvv/io/pcd_io_templated_writer/log/board/production_binary_repeat_5/evidence_doctor.md`

| severity | count | handling |
| --- | ---: | --- |
| Errors | 3 | 三个 case 都触发 `ba_degradation_frequency`；当前证据禁止写 production adoption。 |
| Warnings | 0 | none |
| Suggestions | 0 | none |

这些 Error 不说明一定有 correctness bug；它们说明当前 production-public A/B 的退化频率过高。由于 checksum
一致、production direct tests 通过、asm 归属闭合，较可能的解释是当前 field-outer RVV 实现把 binary writer
本来按点连续写出的 hot loop 改成多字段跨步写出，增加了 store / cache 成本，真实 public writer 边界无法从
Phase 040 的 component ablation 继承收益。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 040 是 `binary_component_ablation`；本阶段是 `production_public`。 |
| A/B boundary | `PCDWriter::writeBinary<PointT>(file_name, cloud)` public overload。 |
| 当前决策问题 | 当前 public RVV binary writer path 是否快于当前 public scalar path。 |
| diagnostic 是否可外推到 production | 否。Phase 080 证明 component positive 不能外推到真实 mmap / public writer 边界。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 production-public A/B 消除主要边界错配；结果反而显示真实入口退化。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe；负向后进入 PI5 用户检查点，不自动回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有既有 binary RVV family；但 production-public Std/RVV 已为 negative，因此无需补 family A/B 来拒绝采纳。 |

## Rollback closeout

用户授权回滚后，已完成以下清理：

- production source：移除 binary writer RVV helper、fallback gate、test hook 和公开入口短路分流；保留
  compressed writer adopted production patch。
- topic tests：移除只服务 binary production patch 的 `BinaryPublicWriter*` 和
  `BinaryIndicesOverloadKeepsScalarBehavior` 测试；保留 Phase 040 binary component ablation 测试作为历史 /
  诊断边界。
- bench / Makefile：移除当前 `production_binary_*` bench cases 和
  `run_board_pcdtw_production_binary_repeated` 相关 wrapper；Phase 080 的 summary、manifest、Doctor 和 registry
  登记继续作为历史负向证据。

回滚后验证：

- `make -C test-rvv/io/pcd_io_templated_writer run_test_compare`：Std/RVV 各 8 个 gtest 通过。
- `make -C test-rvv/io/pcd_io_templated_writer run_bench_rvv BENCH_ARGS="--case-filter production_compressed_* --iterations 2 --warmup-iterations 1"`：QEMU smoke 可运行；只证明日志形状。
- `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv`：压缩 writer production bench 仍可生成 RVV asm dump。
- production-binary current-code scan：`packBinaryFieldsRVV`、binary hook、`production_binary_*` target 均无当前源码 /
  测试入口残留。

## PI5 Decision

`pi5_evidence_decision = rollback_completed / no-production`

建议：

- 不采纳当前 binary writer field-outer RVV production patch。
- 当前 binary writer 生产入口保持原标量实现。
- 如果继续当前 topic，可另开 Phase 090，尝试 tuple / segment output path 或 compact memcpy fast path：
  对 4 个连续 4 字节字段按一趟 point-major tuple 写出，避免当前 `vsse32` 跨步输出。这个方向需要新的
  test-first、asm、board repeated 和 Doctor，不能覆盖 Phase 080 的 PI5 rollback 结论。

## 继续 / 停止决定

stop_condition_resolved：`production_rollback_requires_user_authorization` 已由用户授权解除，回滚已完成。
当前 Evidence Doctor Errors=3 且 board bucket 为 negative，因此该实现族关闭为
`rollback/no-production`。

next_phase_default：默认暂停当前 topic 并交给 reviewer / 用户判断。若用户希望继续探索 binary writer，
可以创建 `090-binary-writer-tuple-output-probe` 或等价 phase，但它必须作为新实现族重新建 plan，
不能修改 Phase 080 历史结论。
