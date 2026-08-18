# Phase 009 Result：correspondence segment load（拒绝）

## 当前结论

本阶段验证 `pcl::Correspondence` 的三列连续 32-bit AoS 布局是否适合用
`vlseg3e32` 替换 Phase 008 的两次 `vlse32` query/match 读取。segment loader、
Std/RVV correctness、QEMU 窄 smoke 和板卡 repeated 均已完成。

production TEDQ header、public API 和 production dispatch 均未修改。

## 计划动作回填

| action | 状态 | 证据和结论 |
| --- | --- | --- |
| A1 segment loader | done | 新增 `CorrespondenceSegmentIndexStreamLoader` 和 `estimateDualQuaternionCorrespondenceSegmentIndexStreamCandidate`；gate 要求标准布局、32-bit `index_t`、`sizeof(Correspondence)==12` 和 32-bit point byte offset。 |
| A2 correctness | done | `make run_test_compare`；Std/RVV 各 `19/19 tests passed`。新增 segment-loader 对拍通过，RVV stats 命中 `used_correspondence_segment_stream`，Std 保持 fallback。 |
| A3 bench case | done | `correspondence-segment-load-comparison` 的 QEMU 窄 smoke 已完成，4K/64K/256K checksum 全一致。 |
| A4 board comparison | done | `log/board/correspondence_segment_load_repeated/` 已完成 5 runs、每次 20 iterations、warm-up 5 的 repeated。 |
| A5 decision | done | manifest、summary、Evidence Doctor 和 registry 已刷新；segment-load 标记为 `rejected with evidence`，Phase 008 direct index stream 保留为 baseline。 |

## 证据边界

候选只覆盖 `PointXYZ` / `float`、标准布局 `pcl::Correspondence`、连续
`query/match/distance` 三个 32-bit 字段和已有双侧 `vluxei32` point gather。
distance 列只为满足 segment load 读取而载入，不参与 TEDQ 公式。该候选是
test-rvv implementation-family diagnostic，不能写成 production-ready。

## QEMU 与正确性

QEMU 只用于正确性和日志形状，不用于性能排序。Std/RVV 各 `19/19 tests passed`；
segment stream 的 query/match 提取、row pairing、RVV stats 和 fallback 合同均通过。
QEMU bench smoke 中 Phase 008 direct index stream 与 segment stream 的 4K/64K/256K
checksum 全一致。

## 板卡结果与 Evidence Doctor

Milkv-Jupiter 上使用 5-run、每次 20 iterations、warm-up 5 的同边界 RVV-vs-RVV
比较，B/A 定义为 `Phase 008 direct index stream RVV ms / segment-load RVV ms`：

| 规模 | median B/A | min | max | 决策桶 | checksum |
| --- | ---: | ---: | ---: | --- | --- |
| 4K | `0.979x` | `0.872x` | `1.049x` | `negative` | same |
| 64K | `1.001x` | `0.925x` | `1.017x` | `negative` | same |
| 256K | `1.012x` | `0.989x` | `1.043x` | `unstable` | same |

Evidence Doctor：`Errors=3, Warnings=2, Suggestions=2`。Errors 暴露三个规模
均有较高退化频率；4K 还有长尾和组内离群，64K/256K 收益接近阈值。当前证据
不支持 segment-load 的稳定收益，因此不能保留为默认 correspondence ingress。
完整异常说明见：

- `log/board/correspondence_segment_load_repeated/summary.md`
- `log/board/correspondence_segment_load_repeated/evidence_manifest.json`
- `log/board/correspondence_segment_load_repeated/evidence_doctor.md`

## 继续 / 停止决定

- 当前阶段：`done / rejected with evidence`。
- `continue_stop_decision`：停止继续堆叠 correspondence ingress 指令形状，进入
  Phase 010 的 correspondence index locality 窄消融。
- `next_phase_default`：
  `010-correspondence-index-locality`，固定 Phase 008 direct index stream，
  只改变 query/match 索引的连续、局部窗口和跨步分布。
- Phase 008 direct index stream 是当前 correspondence baseline；本阶段 negative /
  unstable 结果不改变其它 row source policy 的独立状态。
