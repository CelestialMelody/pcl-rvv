# Phase 010 Plan：correspondence index locality ablation

## 阶段意图和边界

Phase 008 的 correspondence direct index stream 相对 staged/direct gather
保持正向，Phase 009 的 `vlseg3e32` segment-load 则为 negative / unstable。
本阶段不再继续堆叠 correspondence ingress 指令形状，而是固定 Phase 008
candidate，只改变 `query/match` 索引序列的局部性，回答以下问题：

- direct index stream 的收益是否依赖当前确定性跨步索引分布；
- 连续、局部窗口和跨步索引下，双侧 `vluxei32` point gather 的方向是否稳定；
- 是否有证据支持后续 locality-aware candidate（按输入局部性选择路径），还是应保留
  Phase 008 为窄诊断 baseline 并停止 correspondence 方向扩展。

本阶段只修改
`test-rvv/registration/transformation_estimation_dual_quaternion` 的 test-support、
bench、脚本、phase 文档和证据登记；不修改 production TEDQ header、public API、
production dispatch 或其它 topic。

## 当前状态与假设

| 项目 | 当前状态 | 本阶段处理 |
| --- | --- | --- |
| Phase 008 direct index stream | retained baseline；板卡相对 staged/direct gather positive | 固定候选实现，只替换 correspondence 输入序列 |
| Phase 009 segment load | `rejected with evidence`；4K/64K negative，256K unstable | 不再作为本阶段候选 |
| 当前 correspondence pattern | query/match 使用确定性跨步索引 | 增加连续、局部窗口、跨步三类 deterministic corpus |
| 主要假设 | gather locality 可能解释部分 B/A 波动 | 只有同边界 repeated board 结果才能支持该假设 |
| production 边界 | 当前 production 保持标量 | 不进入 PI1，不修改 production header |

## 优化矩阵

| candidate family | row source | 点型 / Scalar / layout | correctness | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| direct index stream：contiguous pattern | correspondence-pair | `PointXYZ` / `float` / Correspondence AoS + xyz AoS | 三种 pattern 与 scalar reference 对拍 | `correspondence-index-locality-ablation` | bounded 5-run，20 iterations，warm-up 5 | 固定 Phase 008 helper，检查 gather 归属 | 新 manifest / summary | planned |
| direct index stream：local-window pattern | correspondence-pair | 同上 | 同上 | 同上 | 同上 | 同上 | 同上 | planned |
| direct index stream：strided pattern | correspondence-pair | 同上 | 同上 | 同上 | 同上 | 同上 | 同上 | retained baseline |
| Phase 009 segment load | correspondence-pair | `PointXYZ` / `float` | 19 tests passed | checksum same | 4K/64K negative，256K unstable | `vlseg3e32` 归属待补 | Errors=3 / Warnings=2 / Suggestions=2 | rejected with evidence |

## 实现和测试动作

| action | 产物 | 验证 | 完成判据 |
| --- | --- | --- | --- |
| A1 locality corpus | `tedq_adapters.hpp` 增加三类 deterministic correspondence pattern | Std/RVV 编译 | 三类 pattern 保持合法 query/match 范围和固定点对语义 |
| A2 correctness | `test_tedq.cpp` locality pattern 对拍 | `make run_test_compare` | Std/RVV 各通过，RVV stats 命中 Phase 008 direct index stream |
| A3 bench case | `bench_tedq.cpp` 新 filter 和 case label | QEMU 窄 smoke | 三类 pattern checksum 一致，输出合同可解析 |
| A4 board comparison | topic Makefile、summary script、manifest、doctor | 5-run board repeated | 每种 pattern 独立 median/min/max/bucket，保留退化频率 |
| A5 decision | result、roadmap、matrix、registry、活动文档 | `make evidence_status` | locality-aware 后续路线只有在同边界证据支持时保留 |
| A6 asm attribution | 更新 RVV dump 并检查 `vluxei32` | `make dump_bench_rvv` + `rg` | 只确认 test-support binary 指令存在，不外推 production |

## 板卡预算和决策桶

- 5 runs；
- 每次 20 iterations；
- warm-up 5 iterations；
- 每种 pattern 最多一次同边界确认复跑；
- `positive`：全部 run 的 B/A 大于 1.15；
- `weak_positive`：median 不低于 1.03 且最小值不低于 0.97；
- `neutral`：全部值在 0.97 到 1.03；
- 出现高频退化时标记 `negative` 或 `unstable`，不只看 median。

比较对象固定为同一 Phase 008 direct index stream RVV candidate 的三种输入
pattern；本阶段不把 Std/RVV speedup 写成 locality 结论，也不把 QEMU timing
用于性能排序。

## Evidence Doctor、registry 和继续条件

输出目录为：

```text
log/board/correspondence_index_locality_ablation_repeated/
```

summary、manifest 和 doctor 必须由 topic-local script 生成并登记到
`log/evidence_registry.json`。Evidence Doctor 的 Error 必须先修复或降级证据；
Warning / Suggestion 必须保留在 result 和下一步解释中。

若三种 pattern 的方向均接近 neutral 或不稳定，保留 Phase 008 direct index stream
作为 correspondence baseline，拒绝 locality-aware 生产候选。若某种 pattern 稳定
positive 而其它 pattern 明显退化，只保留“输入分布敏感的诊断假设”，下一阶段先做
更窄的分布复核，不直接修改 production。

## 文档更新清单

- `doc/phases/README.zh.md`
- `doc/phases/009-correspondence-segment-load/result.zh.md`
- `doc/phases/010-correspondence-index-locality/plan.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `doc/transformation_estimation_dual_quaternion-evaluation.zh.md`
- `README.zh.md`

production header 和 `doc-rvv` 不在本阶段范围内。
