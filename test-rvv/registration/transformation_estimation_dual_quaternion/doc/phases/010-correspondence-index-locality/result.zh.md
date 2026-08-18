# Phase 010 Result：correspondence index locality ablation

## 当前结论

Phase 010 已完成。它固定 Phase 008 的 correspondence direct index stream
candidate，只改变 `query/match` 的确定性索引分布，比较 `contiguous`（连续）、
`local-window`（局部窗口）和 `strided`（跨步）三种 pattern。

结论是：direct index stream 对输入索引局部性明显敏感，但没有形成可直接进入
production 的 locality-aware RVV 分流策略。

- `contiguous` 相对 Phase 008 `strided` baseline：4K `1.171x` weak-positive，
  64K `1.243x` positive，256K `1.320x` positive。
- `local-window` 相对同一 baseline：4K `0.959x` negative，64K `0.436x`
  negative，256K `0.458x` negative；三个规模均为 5/5 退化。
- `strided` 保留为 Phase 008 direct index stream baseline。

因此保留 Phase 008 作为 correspondence test-rvv baseline，拒绝把三种输入
pattern 自动转化为 production dispatch。Phase 009 的 `vlseg3e32` 负向证据与
Phase 010 的 locality 结果共同说明：当前不应继续堆叠 correspondence ingress
指令变体。production TEDQ header、public API 和 production dispatch 均未修改。

## 计划动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 locality corpus | done | `include/impl/tedq_adapters.hpp`、`src/test_tedq.cpp`、`src/bench_tedq.cpp` | 新增连续、局部窗口、跨步三类 deterministic correspondence pattern；索引范围和点对语义保持合法。 |
| A2 correctness | done | `make run_test_compare`；`log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | Std/RVV 各 `22/22 tests passed`；三类 pattern 均与 scalar reference 在 `1e-4` 矩阵误差预算内一致；RVV 构建命中 direct index stream stats。 |
| A3 bench case | done | `make record_qemu_smoke_evidence_state BENCH_ARGS="--iterations 2 --warmup-iterations 1 --case-filter correspondence-index-locality-ablation"` | QEMU 只用于窄 smoke；三类 pattern 的日志、checksum 和 manifest 合同通过，未用于性能排序。 |
| A4 board comparison | done | `make run_board_bench_correspondence_index_locality_repeated`；`log/board/correspondence_index_locality_ablation_repeated/` | Milkv-Jupiter 上完成 5 runs、每次 20 iterations、warm-up 5；三种 pattern 分别独立汇总。 |
| A5 checksum semantics | done | `script/generate_tedq_correspondence_index_locality_summary.py`、manifest | 跨 pattern 比较 input corpus identity；output fingerprint 单独保留并结合 correctness 解释，避免受浮点规约顺序影响的 fingerprint 被误作输入一致性。 |
| A6 asm attribution | done | `make dump_bench_rvv`；RVV bench binary 反汇编 | test-support binary 中确认 `vlse32`、`vluxei32`、`vlseg3e32`；只证明诊断二进制归属，不证明 production symbol。 |
| A7 Evidence Doctor / registry | done | `make evidence_status`；本阶段 summary、manifest、doctor 已登记 | registry 刷新后只保留 local-window 高频退化 Error 和组内离群 Warning；没有 checksum 或 doc-ref 合同错误。 |
| A8 decision | done | 本 result、roadmap、optimization matrix | 保留 direct index stream baseline；停止 locality-aware production candidate 和继续堆叠 correspondence ingress 形状。 |

## 正确性与 QEMU 证据

正确性测试使用同一 `PointXYZ` / `float` / correspondence-pair 语义，三种
pattern 只改变 query/match 索引序列。Std 与 RVV 构建均通过 22 个测试，包含：

- direct index stream、segment stream、staged candidate 和 direct gather 的既有回归；
- contiguous、local-window、strided 三种 locality pattern；
- public correspondence boundary、非 identity row pairing、fallback 和矩阵误差检查。

QEMU smoke 仅证明 test-support bench binary 能运行、输出合同可解析、输入 corpus
identity 和 checksum 字段存在。QEMU 计时不进入性能结论。

## 板卡证据

板卡 repeated 的计时边界是同一个 direct index stream RVV candidate 的
correspondence row ingress、C1/C2 accumulation 和 Eigen 4x4 solve。比较定义为：

`B/A = strided Phase 008 direct index stream RVV ms / alternative pattern RVV ms`

| 规模 | contiguous median B/A | contiguous bucket | local-window median B/A | local-window bucket |
| --- | ---: | --- | ---: | --- |
| 4K | `1.171x` | `weak_positive` | `0.959x` | `negative` |
| 64K | `1.243x` | `positive` | `0.436x` | `negative` |
| 256K | `1.320x` | `positive` | `0.458x` | `negative` |

该表只说明输入分布敏感性。它不说明哪一种 pattern 在真实 PCL correspondence
工作负载中占主导，也不证明可以在 production 中安全识别、分流或维护多条路径。

## Evidence Doctor

当前 Phase 010 board doctor 为 `Errors=3 / Warnings=4 / Suggestions=0`：

- 三个 Error 都是 `strided-vs-local-window` 的高频退化，分别对应 4K、64K、
  256K，均为 5/5 低于 1。
- 四个 Warning 是 group outlier，提示 local-window 和 256K contiguous 不应被
  其它 pattern 的 median 代表。

这些异常不是 checksum 契约错误；它们是本阶段要暴露的负向性能信号。由于预算已
完成 5-run，且 local-window 的退化方向在三个规模一致，本阶段不再追加无界复跑。
结论降级为输入分布敏感的 diagnostic hypothesis，不作为 production performance
evidence。

## 命名与证据登记

新增活动对象、bench label 和文档统一使用 `ordered-cloud-pair`。它描述 source /
target 按相同下标一一对应的 row source policy；当前活动 bench filter 使用
`ordered-cloud-pair`。

Phase 010 的 summary、manifest、doctor 和 correctness / smoke registry 条目均以
topic-local 文档引用；raw board run logs 继续遵循 `summary-only` 策略，不作为默认
提交产物。

## 继续 / 停止决定

- 当前阶段：`done / rejected with evidence`（locality-aware production candidate）。
- 当前 topic 总边界：`diagnostic / no-production`；production header 保持标量。
- `doc-rvv`：`not_applicable`，因为没有 adopted production behavior 或 PI5 production
  evidence 闭环。
- `continue_stop_decision`：停止 correspondence ingress 微观变体搜索；保留 Phase
  008 direct index stream 为 test-rvv baseline。
- `next_phase_default`：若用户 / reviewer 明确授权 production integration，进入
  `PI1-production-reentry-contract`，重新验证 path-hit、fallback、production asm
  attribution、production-public board repeated 和 registry freshness；在该授权前
  不修改 production。
- 在 production 授权之外，仍可继续的窄诊断方向是 point type / layout 扩展或按真实
  workload 采样 correspondence index 分布；它们必须新建 phase plan，不能把当前
  synthetic locality 结果写成 production 结论。
