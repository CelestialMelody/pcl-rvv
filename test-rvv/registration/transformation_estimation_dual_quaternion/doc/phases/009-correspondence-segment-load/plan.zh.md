# Phase 009 Plan：correspondence segment load

## 阶段意图和边界

Phase 008 的 direct index stream 已从 `pcl::Correspondence` AoS 记录使用两次
`vlse32` 读取 `index_query/index_match`，板卡相对 Phase 007 direct gather
保持 positive。当前 `pcl::Correspondence` 的标准布局是连续的
`index_query/index_match/distance` 三个 32-bit 字段，因此本阶段验证一个更窄的
RVV ingress code shape：

- 使用 `vlseg3e32` 一次读取三列；
- 只消费 query 和 match 两列，distance 列作为未使用字段；
- 保持双侧 `vluxei32`、C1/C2 公式、f32 到 f64 规约、Eigen 4x4 solve 和 checksum；
- 只新增 test-rvv candidate、correctness、bench、board summary 和 Evidence Doctor；
- 不修改 production TEDQ header、public API、production dispatch 或其它 row source。

本阶段比较的是 `correspondence-pair` 的 implementation family，不把
segment-load 结果外推到其它 AoS 类型或 production direct。

## 当前状态与假设

| 项目 | 当前状态 | 本阶段处理 |
| --- | --- | --- |
| Phase 008 correctness | Std/RVV 各 18 tests passed | 保持回归并新增 segment-loader 对拍 |
| Phase 008 board | direct index stream 相对 direct gather 为 positive | 作为明确 baseline |
| correspondence layout | 标准布局，query/match/distance 连续 32-bit 字段 | 验证 `vlseg3e32` 的字段提取和代码生成 |
| 主要假设 | 两次 `vlse32` 的 strided ingress 是可优化开销 | 只把板卡同边界 A/B 作为支持，不单因源码形状认定收益 |
| fallback | 当前 direct index stream gate 失败时回退 | segment gate 失败必须保留 Phase 008 语义 |

## 优化矩阵

| candidate family | row source | 点型 / Scalar / layout | correctness | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| correspondence segment index stream | correspondence-pair | `PointXYZ` / `float` / standard-layout `Correspondence` AoS + xyz AoS | 新增 segment-loader 对拍，18 项回归保持通过 | 新 filter 输出 Phase 008 direct index stream 与 segment stream | bounded 5-run，20 iterations，warm-up 5 | 检查 `vlseg3e32` / gather 归属 | 新 manifest / summary | planned |
| Phase 008 direct index stream | correspondence-pair | `PointXYZ` / `float` | 既有对拍 | 同一 RVV binary baseline | 复用 Phase 008 summary | 既有 test-support asm | Phase 008 doctor | retained baseline |

## 实现和测试动作

| action | 产物 | 验证 | 完成判据 |
| --- | --- | --- | --- |
| A1 segment loader | `tedq_candidates.hpp` loader 和 candidate | Std/RVV 编译 | 标准布局、`sizeof(Correspondence)==12`、32-bit index gate 成立时使用 segment load |
| A2 correctness | `test_tedq.cpp` 新测试和 stats gate | `make run_test_compare` | segment stream 与 scalar / Phase 008 baseline 在误差预算内一致 |
| A3 bench case | `bench_tedq.cpp` 新 label 和 filter | QEMU 窄 smoke | Phase 008 baseline 与 segment stream checksum 一致 |
| A4 board comparison | topic Makefile、summary script、manifest | 5-run board repeated | 4K / 64K / 256K 独立输出 B/A、bucket、checksum |
| A5 decision | result、roadmap、matrix、registry | Evidence Doctor | positive 才保留为后续候选；其它结果保留 Phase 008 baseline |

## 板卡预算和决策桶

- 5 runs；
- 每次 20 iterations；
- warm-up 5 iterations；
- 默认不超过一次同边界确认复跑；
- `positive`：全部 run 的 B/A 大于 1.15；
- `weak_positive`：median 不低于 1.03 且最小值不低于 0.97；
- `neutral`：全部值在 0.97 到 1.03；
- 其它方向为 `negative` 或 `unstable`；
- 64K 离群只按 correspondence size 独立解释。

## Evidence Doctor、registry 和继续条件

输出目录计划为：

```text
log/board/correspondence_segment_load_repeated/
```

summary、manifest、doctor 必须由 topic-local script 生成并由
`log/evidence_registry.json` 登记。Warning 必须解释 segment load、AoS locality、
gather、寄存器压力、cache 和 solver 稀释等候选；未有 profile 时不得单因归因。

若 segment stream 在 correctness、checksum、`vlseg3e32` 归属和 board bucket 上都
成立，下一阶段可继续做 correspondence 输入分布 / index locality 的窄消融；若无收益，
保留 Phase 008 direct index stream，停止继续扩大 correspondence ingress code shape。

## 文档更新清单

- `doc/phases/008-correspondence-direct-index-stream/result.zh.md`
- `doc/phases/009-correspondence-segment-load/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `README.zh.md`

production header 和 `doc-rvv` 不在本阶段范围内。
