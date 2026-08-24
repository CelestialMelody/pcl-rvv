# Phase 040 计划：binary writer component ablation

## 阶段意图和边界

本阶段评估 `PCDWriter::writeBinary<PointT>(file_name, cloud)` 的 packed binary output（压缩外二进制输出）
是否存在值得继续的 RVV（RISC-V Vector，可变长度向量）组件收益。它仍是 test-only component ablation
（测试专用组件消融），不修改 production（生产源码）。

本阶段只复刻 binary writer 中“过滤 `_` 字段后，把每个 point 的有效字段按 point-major packed
格式写入输出 buffer”的循环：

```text
for point in cloud:
  for field in fields:
    memcpy(out, point + field.offset, field_size)
    out += field_size
```

不覆盖 header、file lock、raw_fallocate、mmap、write、msync、error handling、`indices` overload、
ASCII writer 或 PCLPointCloud2 path。

## Phase Scope

validated_scope（本阶段准备验证）：

- entry shape（入口形态）：`writeBinary<PointT>(file_name, cloud)` 的 contiguous PointCloud rows。
- point type / layout：synthetic PointXYZRGB-like 4 字节字段，覆盖 no-padding 与 tail-padding。
- evidence role：component_ablation（组件消融），不是 production direct（真实生产路径证据）。

unvalidated_scope（本阶段不验证）：

- `writeBinary(file_name, cloud, indices)` indexed row source（索引行来源）。
- 非 4 字节字段、多元素字段、真实 PCL registered point type（注册点类型）。
- 真实 file mapping / write。

## 候选假设

binary writer 的输出仍是 point-major packed layout（按点连续的有效字段），不同于 compressed writer 的
field-major layout（按字段连续）。4 字节字段下可尝试：

- RVV stride load（跨步加载）从 source point cloud 读取同一字段。
- RVV stride store（跨步存储）写入 packed output 中每个 point 的对应字段位置。

风险：stride store 可能抵消收益；no-padding 场景也可能被简单连续 copy 或编译器优化覆盖。因此本阶段只先做组件消融和板卡证据，不进入 production。

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| E1 写 binary writer failing correctness test | `src/test_pcdtw.cpp` | 新测试在 helper 缺失时编译失败或不能通过。 |
| E2 实现 test-only binary pack helper | `include/impl/pcdtw_support.hpp` | Std/RVV build 输出一致；RVV build 命中 binary candidate。 |
| E3 扩展 bench case | `src/bench_pcdtw.cpp` | 增加 `binary_*` case，计时只包含 binary pack，不包含 checksum。 |
| E4 QEMU smoke / asm | `run_qemu_smoke`、`dump_bench_rvv` | correctness / log-shape 通过；asm 可见 RVV load/store。 |
| E5 板卡 repeated binary bench | `run_board_pcdtw_binary_repeated` | 5 次 repeated summary，decision bucket 稳定或标记 unstable。 |
| E6 Evidence Doctor / registry | `log/board/binary_component_repeat_5/*` | Doctor Error 必须解除或降级；registry fresh。 |
| E7 文档回填 | result、roadmap、matrix、evaluation | 写清 binary writer 是否值得继续到 shaped / PI1。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component_ablation（组件消融） |
| A/B boundary | test helper / bench wrapper；不含 public file write |
| 当前决策问题 | binary writer packed output 是否值得继续 production-shaped diagnostic |
| diagnostic 是否可外推到 production | unknown；positive 只能支持下一阶段 shaped diagnostic 或 PI1 计划。 |
| comparison-boundary / baseline mismatch 风险 | yes；真实 mmap/file write 和 header 不在计时边界内。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 默认不允许；需先补 production-shaped diagnostic 或 profile。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | yes；本阶段不能 clean adopt。 |

## 板卡复跑预算和决策桶

- run budget：5 次 repeated board run，`--iterations 20 --warmup-iterations 3`。
- positive：median speedup >= `1.08x` 且无 checksum mismatch。
- weak-positive：median speedup 在 `1.03x` 到 `1.08x`，且 Doctor 无 Error。
- neutral：`0.98x` 到 `1.03x`。
- negative：median speedup < `0.98x`。
- unstable：5 次内 decision bucket 摇摆，或 Doctor Error / checksum 问题无法解除。

## Continue / Stop Conditions

若 binary component ablation positive / weak-positive 且 Doctor clean，下一 phase 可创建 binary writer
production-shaped diagnostic 或 PI1 计划；若 neutral / negative，写 rejected / deferred，并保留 compressed
writer 的 pending-production-authorization 状态不变。
