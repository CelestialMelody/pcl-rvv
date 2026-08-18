# Phase 008 Plan：correspondence direct index stream

## 阶段意图和边界

Phase 007 已证明三类 indexed policy 的 direct gather family 在板卡上有正向信号：
source-indexed 和 dual-indexed 为 `positive`，correspondence 为 `weak_positive`；
correspondence 64K 还触发了组内离群 warning。本阶段只针对 correspondence 的
query/match 展开成本做窄范围验证：

- 继续使用 `PointXYZ` / `float` / x,y,z AoS；
- 保持双侧 `vluxei32`、既有 C1/C2 公式、f32 到 f64 规约和 Eigen 4x4 solve；
- direct candidate 从 `pcl::Correspondences` 的 AoS 记录直接读取 query/match index，
  不先构造两个临时 `pcl::Indices`；
- 不修改 production TEDQ header，不修改 public API，不重开 production integration loop；
- source-indexed、dual-indexed 和 ordered-cloud-pair 本阶段不改实现。

本阶段的 family comparison 是 test-rvv implementation-family comparison，不是
production direct 证据。若直接读取 correspondence AoS 的布局条件不能稳定闭合，则保留
Phase 007 的双侧 index-vector direct gather，不把新候选升级为默认路径。

## 当前状态与假设

| 项目 | 当前状态 | 本阶段处理 |
| --- | --- | --- |
| Phase 007 correctness | Std/RVV 各 17 tests passed | 保持回归并新增 correspondence direct-stream 对拍 |
| Phase 007 board | source / dual `positive`；correspondence `weak_positive` | 以 64K warning 为局部优化动机 |
| query/match 展开 | direct candidate 每次构造两个临时 `Indices` | 假设直接从 `Correspondence` AoS 读取可减少展开开销 |
| data layout | `Correspondence` 为标准布局，query/match 为 `index_t` | 仅在 `sizeof(index_t)==4`、标准布局和 32-bit gather offset 可表达时命中 |
| fallback | 当前 correspondence invalid 或不满足 RVV gate 时回退 | 新 gate 失败必须保持既有 dual-indexed direct/staged 语义 |

## 优化矩阵

| candidate family | row source | 点型 / Scalar / layout | correctness | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| correspondence direct index stream | correspondence-pair | `PointXYZ` / `float` / `Correspondence` AoS + xyz AoS | 新增 direct-stream 对拍，17 项回归保持通过 | 新 filter 输出 `direct index stream` 与 Phase 007 direct gather | bounded 5-run，20 iterations，warm-up 5 | test-support bench binary RVV 指令 smoke | 新 manifest / summary | planned |
| Phase 007 dual-indexed direct gather | correspondence-pair | `PointXYZ` / `float` | 既有对拍 | 作为同一 RVV binary baseline | 复用 Phase 007 raw logs / 新 target 同边界重测 | 已有 test-support asm | Phase 007 doctor | retained baseline |

## 实现和测试动作

| action | 产物 | 验证 | 完成判据 |
| --- | --- | --- | --- |
| A1 direct correspondence ingress | `tedq_candidates.hpp` 中的 test-only helper | Std/RVV 编译 | 直接读取 query/match；不构造临时 `Indices` |
| A2 correctness | `test_tedq.cpp` 新测试和 stats gate | `make run_test_compare` | direct-stream 与 scalar / Phase 007 direct 结果在误差预算内一致 |
| A3 bench case | `bench_tedq.cpp` 新 label 和 filter | QEMU 窄 log-shape smoke | staged、Phase 007 direct、新 direct-stream 三侧 checksum 一致 |
| A4 board comparison | topic-local Makefile、summary script、manifest | 5-run board repeated | correspondence 每个 size 独立输出 B/A、bucket 和 checksum |
| A5 decision | phase result、roadmap、optimization matrix | Evidence Doctor + registry | positive/weak-positive 才保留为诊断候选；neutral/negative 保留旧 direct baseline |

## 板卡预算和决策桶

- 5 runs；
- 每次 20 iterations；
- warm-up 5 iterations；
- 默认不超过一次同边界确认复跑；
- `positive`：全部 run 的 B/A 大于 1.15；
- `weak_positive`：median 不低于 1.03 且最小值不低于 0.97；
- `neutral`：全部值在 0.97 到 1.03；
- 其它方向标为 `negative` 或 `unstable`；
- 64K 若继续偏离 4K/256K，只按 correspondence 的 size 独立解释，不向其它 policy 外推。

## Evidence Doctor、registry 和继续条件

输出目录计划为：

```text
log/board/correspondence_direct_index_stream_repeated/
```

summary、manifest、doctor 必须由 topic-local script 生成，并由
`log/evidence_registry.json` 登记。Evidence Doctor 出现 Error 时不得形成严格性能结论；
Warning 必须写明是 query/match 展开、AoS locality、gather 或测量波动的待验证假设。

若 direct index stream 在 correctness、checksum 和 board bucket 上都成立，下一阶段继续
审计 correspondence 的 index locality / packed index cache；若新候选不稳定，则保留
Phase 007 direct gather 作为明确 baseline，停止 correspondence ingress 扩展。

## 文档更新

- `doc/phases/007-indexed-direct-gather-family-expansion/result.zh.md`
- `doc/phases/008-correspondence-direct-index-stream/result.zh.md`
- `doc/phases/README.zh.md`
- `doc/optimization-roadmap.zh.md`
- `doc/phases/optimization-matrix.zh.md`
- `doc/benchmark-and-evidence.zh.md`
- `doc/optimization-evidence.zh.md`
- `README.zh.md`

production header 和 `doc-rvv` 均不在本阶段范围内。
