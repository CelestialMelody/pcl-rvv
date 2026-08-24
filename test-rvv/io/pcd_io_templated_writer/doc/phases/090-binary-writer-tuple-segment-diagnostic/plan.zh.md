# Phase 090: binary writer tuple / segment diagnostic 计划

## 阶段意图和边界

本阶段只做 test-rvv 诊断，不修改 production（生产源码）。目标是验证 `writeBinary<PointT>` 的
新 packed output（按点连续有效字段输出）实现族是否值得进入后续 production integration loop
（生产接入闭环）。

Phase 080 的 field-outer RVV path 已在真实 public writer boundary（公开入口边界）退化并回滚。本阶段不复用
Phase 080 的 production patch；它只回答一个更窄的问题：如果 compact 16B 点型直接整块 `memcpy`，
padding 20B 点型用 RVV segment store（分段存储，按 `x/y/z/rgb` 交错写回）避免 `vsse32.v` 跨步写出，
component / diagnostic 边界是否出现足够强的收益信号。

## 当前状态清单

| area | current state |
| --- | --- |
| compressed writer | adopted production behavior；正式文档为 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。 |
| binary writer field-outer | Phase 080 production-public negative；board mean `0.9842x / 0.9727x / 0.9810x`，Doctor Errors=3；已回滚。 |
| current binary production source | `PCDWriter::writeBinary<PointT>(file_name, cloud)` 回到原 point × field `memcpy` 标量循环。 |
| current tests | `run_test_compare` 回滚后 Std/RVV 各 8 个 gtest 通过。 |
| current bench | `binary_*` component cases 仍可用于诊断；`production_binary_*` 已移除，只保留历史证据。 |

## 假设与候选族

| candidate family | idea | expected benefit | risk |
| --- | --- | --- | --- |
| compact memcpy fast path | 当有效字段覆盖 `offset=0/4/8/12` 且 `point_step=16` 时，packed binary output 等于原点数组前 16 字节，可一趟 `memcpy`。 | 避免逐字段小 `memcpy` 和 RVV store 开销。 | 不是 RVV intrinsic；只能作为 compact layout 的生产快路径候选。 |
| RVV segment output | 对 tail-padding 20B 点型，分别 `vlse32.v` 读取 4 个字段，再 `vsseg4e32.v` 连续交错写 packed output。 | 避免 Phase 080 的 `vsse32.v` 跨步写出。 | 仍有 4 次 stride load、tuple 组装和 segment store 开销；真实 mmap/write 边界可能继续吞掉收益。 |

## 优化矩阵

| candidate family | row source | point type / layout | correctness | bench / board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| binary tuple / segment diagnostic | contiguous rows | 4 个 4-byte fields；compact 16B / padding 20B / small 512 | 新增 tuple/segment candidate 与 scalar byte-equal；fallback 保持 scalar | `binary_tuple_*` QEMU smoke；board repeated 若本地验证通过 | compact 可无 RVV 指令；padding 需 `vlse32.v` + `vsseg4e32.v` | repeated 后必须跑 Doctor | diagnostic positive / weak / negative |

## 实现和测试动作

1. 在 `include/impl/pcdtw_support.hpp` 新增 test-only `packBinaryFieldsTupleCandidate`：
   - RVV build 下 compact 16B 走 memcpy fast path；
   - RVV build 下 4-field padding layout 走 `vlse32.v` + `vsseg4e32.v`；
   - 其它字段数量、字段大小、offset 或 point_step 不满足时回退到 `packBinaryFieldsScalar`。
2. 在 `src/test_pcdtw.cpp` 增加 tuple/segment correctness 和 fallback gtest。
3. 在 `src/bench_pcdtw.cpp` 增加 `binary_tuple_*` case，不改变现有 `binary_*` 历史 component case。
4. 运行 `run_test_compare`、窄 QEMU smoke、`dump_bench_rvv`。
5. 若本地正确性和 asm 通过，板卡可用时运行 bounded board repeated，生成 summary / manifest / Doctor / registry。

## Evidence Doctor 和 registry

本阶段如果生成 board summary，必须复用 topic-local manifest generator，并把 `binary_tuple_*` 标为新的
`binary_tuple_segment_diagnostic` evidence role。Doctor 中若出现 degradation frequency、metadata 缺口或
长尾，必须在 result 中降级，不进入 production。

## 阶段完成条件

- `adopted` 不适用于本阶段；本阶段最多给出 `diagnostic_positive` 或 `production_probe_candidate`。
- 只有 `binary_tuple_*` 大规模 compact 和 padding 在 board repeated 下稳定 positive，且 Doctor 无阻断 Error，
  才建议后续另开 production integration phase。
- 若 compact positive 但 padding neutral / negative，只能建议 compact memcpy fast path 的窄生产 probe；
  padding segment output 不进入生产。
- 若 board 为 weak / negative / unstable，当前 topic 默认暂停，不建议继续自动生产接入。

## 板卡复跑预算和决策桶

- run count：5。
- iterations：20。
- warmup：3。
- decision bucket：
  - positive：大规模 mean / median 均高于 `1.05x`，且 degradation frequency 不触发 Doctor Error。
  - weak-positive：`1.00x` 到 `1.05x`，或 small-only positive。
  - negative：mean 或 median 低于 1，或 Doctor Error 阻止采纳。
  - unstable：方向不一致且预算耗尽。

## 继续 / 停止条件

当前板卡可用，若本地验证通过则同轮推进 board repeated。若 tuple / segment diagnostic negative，本 topic
默认暂停；若 positive，再创建后续 production integration plan，而不是直接改 production。

## 文档更新清单

本阶段结束后更新：

- 本 phase `result.zh.md`。
- `doc/phases/optimization-matrix.zh.md`。
- `doc/optimization-roadmap.zh.md`。
- `doc/pcd_io_templated_writer-evaluation.zh.md`。
- `doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md` 和 current Handoff。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | diagnostic / component-shaped binary writer output。 |
| A/B boundary | test helper / bench wrapper，不是真实 `PCDWriter::writeBinary` public overload。 |
| 当前决策问题 | 新实现族是否值得进入 bounded production probe。 |
| diagnostic 是否可外推到 production | 否。Phase 080 已证明 binary component positive 不能直接外推到 public writer。 |
| comparison-boundary / baseline mismatch 风险 | 有。bench 不包含 header、mmap、sync、file lock 和真实 public writer error path。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 默认不允许；除非 compact 大规模强正向且用户明确要求窄生产 probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-scalar 证据 | 需要。即使本阶段 positive，也只能进入后续 PI1-PI5。 |
