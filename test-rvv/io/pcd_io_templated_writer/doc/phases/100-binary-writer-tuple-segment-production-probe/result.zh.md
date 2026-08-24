# Phase 100: binary writer tuple / segment production probe 结果

## 当前结论

本阶段把 Phase 090 的 tuple / segment output family 接入真实
`PCDWriter::writeBinary<PointT>(file_name, cloud)` public overload（公开入口），并完成接入后的
production-public（真实公开入口）correctness（正确性）、QEMU smoke（QEMU 冒烟）、asm attribution
（反汇编归属）、5-run board repeated benchmark（板卡重复性能测试）和 Evidence Doctor（证据体检）。

决策为 `adopted production behavior`：接入后大规模 compact 和 padding case 均稳定正向，Doctor
无阻断 Error。用户已明确“板卡上的测试结果如果显示有收益即可采纳”，因此本阶段不再停在 PI5
待确认检查点，而是进入文档 closeout。small 512 case 只作为 smoke，不作为生产收益主证据。

## 实际执行范围

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| production helper | done | `io/include/pcl/io/impl/pcd_io.hpp` | 新增 `packBinaryFieldsStd`、`binaryFieldsSupportTuple4BytePack`、`packBinaryFieldsTupleRVV` 和 binary path test hook。 |
| public entry dispatch | done | `writeBinary<PointT>(file_name, cloud)` data-copy 段 | RVV build 下先尝试 tuple helper；失败自然落回原标量 helper。 |
| production direct gtest | done | `make -C test-rvv/io/pcd_io_templated_writer run_test_compare` | Std/RVV 各 15 个 gtest 通过；compact 命中 memcpy、padding 命中 RVV segment、mixed fallback、indices unchanged 均闭合。 |
| QEMU smoke | done | `make -C test-rvv/io/pcd_io_templated_writer run_bench_rvv BENCH_ARGS="--case-filter production_binary_tuple_* --iterations 2 --warmup-iterations 1"` | case 可运行，checksum 稳定；QEMU 不作为性能结论。 |
| asm attribution | done | `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv` | padding production writer 路径所在 bench binary 可见 `vlse32.v` 和 `vsseg4e32.v`。 |
| board repeated | done | `log/board/production_binary_tuple_repeat_5/summary.md` | 5-run 预算用完，compact / padding 大规模 bucket 为 positive。 |
| Evidence Doctor / registry | done | `log/board/production_binary_tuple_repeat_5/evidence_doctor.md`；`log/evidence_registry.json` | Doctor `Errors=0 Warnings=1 Suggestions=0`；registry fresh。 |

## 板卡结果

summary path：`test-rvv/io/pcd_io_templated_writer/log/board/production_binary_tuple_repeat_5/summary.md`

| case | mean speedup | median | min | max | mean Std ms | mean RVV ms | 解释 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `production_binary_tuple_pointxyzrgba_4f_compact_262k` | `1.3046x` | `1.3016x` | `1.2835x` | `1.3408x` | `32.0421` | `24.5642` | compact 16B 在真实 public writer 边界仍稳定正向；该路径主要收益来自整点 `memcpy` fast path。 |
| `production_binary_tuple_pointxyzrgba_4f_padding_262k` | `1.3240x` | `1.3168x` | `1.3068x` | `1.3494x` | `32.4192` | `24.4881` | padding 20B 在真实 public writer 边界仍稳定正向；该路径使用 `vlse32.v` + `vsseg4e32.v`。 |
| `production_binary_tuple_pointxyzrgba_4f_compact_small_512` | `1.1082x` | `1.1036x` | `1.0209x` | `1.1963x` | `0.3924` | `0.3544` | small case 有长尾 warning，只作为 smoke。 |

## Evidence Doctor 解释

Doctor path：`test-rvv/io/pcd_io_templated_writer/log/board/production_binary_tuple_repeat_5/evidence_doctor.md`

- `Errors=0`：没有 checksum mismatch、A/B 边界缺失或 degradation frequency 阻断。
- `Warnings=1`：`production_binary_tuple_pointxyzrgba_4f_compact_small_512` 触发
  `long_tail_or_variance`，min `1.0209x`、median `1.1036x`、max `1.1963x`。处理方式是保留
  min / median / max，并把 small case 降级为 smoke-only；它不影响大规模 compact / padding 的采纳判断。
- `Suggestions=0`。

## Diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | `production_public`。Phase 090 只作为前置信号；本阶段用真实 public overload 重新验证。 |
| A/B boundary | `PCDWriter::writeBinary<PointT>(file_name, cloud)` public overload；计时边界包含 header、file lock、mmap/write 和 writer 调用。 |
| 当前决策问题 | 当前 binary tuple / segment production path 是否快于当前 scalar public writer。 |
| diagnostic 是否可外推到 production | 不外推；Phase 100 接入后的 board summary 是本决策依据。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 使用同一 case-filter、wrapper、checksum policy 和 repeated budget；manifest 记录为 `public_overload`。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用；本阶段已完成 bounded probe 且 bucket positive。 |
| clean adoption 是否需要同一 production boundary 证据 | 已满足 production-public Std/RVV 证据；不是 RVV-family-selection 对已有 RVV family 的替换。 |

## Production gate 和 fallback

| 条件 | 行为 |
| --- | --- |
| 非 RVV 构建 | 不编译 `packBinaryFieldsTupleRVV`，直接使用 `packBinaryFieldsStd`。 |
| `cloud.size()==0` | RVV helper 返回 false，保持标量 empty-cloud 行为。 |
| 有效字段数量不是 4 | fallback 到标量。 |
| 任一有效字段 size 不是 4 | fallback 到标量。 |
| offset 不是 `0/4/8/12` | fallback 到标量。 |
| `sizeof(PointT)` 不是 4 字节倍数，或小于 16 | fallback 到标量。 |
| compact `sizeof(PointT)==16` | 直接 `memcpy` 有效 payload。 |
| padding `sizeof(PointT)>16` | 通过 `vlse32.v` 读取四个字段，再用 `vsseg4e32.v` 写连续 payload；mmap 输出未对齐时使用 staging buffer 后复制回 output。 |
| `writeBinary(file_name, cloud, indices)` | 不接入 RVV；测试验证 hook 保持 `None` 且 payload 顺序不变。 |

## 矩阵更新

| candidate family | row source | point type / layout | correctness | board | asm | Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| binary tuple / segment production path | contiguous rows | 4 个连续 4-byte fields；compact 16B / padding 20B | Std/RVV 15/15 gtest pass；mixed fallback pass；indices unchanged pass | compact mean `1.3046x`；padding mean `1.3240x`；small smoke mean `1.1082x` | production bench binary contains `vlse32.v` + `vsseg4e32.v` for padding path | `Errors=0 Warnings=1 Suggestions=0` | adopted production behavior | S11 doc closeout and formal `doc-rvv` refresh。 |

## Continue / stop decision

本阶段完成后，`writeBinaryCompressed<PointT>` 和 `writeBinary<PointT>` 的当前已授权生产优化均已闭合。
剩余可想象方向包括 arbitrary compact payload memcpy（任意字段连续覆盖时整点复制）、non-4-byte field
packing、更多 point type / layout 扩展和 ASCII writer profile，但这些要么不是 RVV 主路径，要么需要扩大
点类型 / datatype 范围或新的 profile 证据。当前 phase loop 默认停止在 closeout：不建议继续自动修改
production；后续若要扩展，应另开新 phase 并先冻结范围和证据计划。
