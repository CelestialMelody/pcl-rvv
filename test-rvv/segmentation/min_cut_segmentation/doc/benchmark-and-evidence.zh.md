# min_cut_segmentation Benchmark 与证据

## 本文职责

本文说明 bench 输出、case-filter、计时边界、summary / manifest / Evidence Doctor（证据体检）和提交边界。性能结论只来自板卡 repeated benchmark（重复性能采集）；QEMU（仿真器）只用于 correctness、构建和日志形状。

## Bench 输出格式

`src/bench_min_cut_segmentation.cpp` 输出：

- `Dataset:`：点数、foreground 点数、neighbours 和 case-filter。
- `Iterations:` / `Warmup Iterations:`：计时迭代和预热次数。
- `<case>: <ms> ms / iter`：单 case 平均耗时。
- `Checksum:`：加权浮点 checksum（校验和），用于防止路径被优化掉；正确性仍以 gtest 为准。

## CLI 参数和 case-filter 字典

| 参数 / case | 默认值 | 证明什么 | 不能证明 |
| --- | --- | --- | --- |
| `--size` | Phase 000: `65536`；Phase 010: `8192` | 输入点数规模 | 泛型点类型或 organized cloud |
| `--foreground` | `19` | unary foreground min-distance 成本 | foreground 很小或很大的分布 |
| `--neighbours` | `14` | Phase 010 KNN 出边规模 | production 所有参数组合 |
| `--iterations` / `--warmup` | phase target 指定 | 重复计时预算 | 稳定频率或温度控制 |
| `unary_min_distance` | case-filter | unary component | graph / max-flow |
| `binary_exp_weight` | case-filter | binary component | double `std::exp` 严格替换 |
| `buildgraph_potential_batch` | case-filter | KNN + Boost graph 边界内的 potential batch | public `extract()` 和 max-flow |

## 推荐 target

| target | 证据角色 |
| --- | --- |
| `make run_test_compare` | correctness |
| `make dump_bench_rvv` | asm attribution（反汇编归属） |
| `make run_board_min_cut_repeated` | Phase 000 component repeated board |
| `make run_board_min_cut_buildgraph_repeated` | Phase 010 production-shaped repeated board |
| `make run_board_evidence_doctor` | Phase 000 manifest / doctor |
| `make run_board_buildgraph_evidence_doctor` | Phase 010 manifest / doctor |

## 计时边界

| case | 计入 | 排除 |
| --- | --- | --- |
| `unary_min_distance` | foreground loop、distance min reduction、sqrt、checksum | KNN、Boost graph、max-flow |
| `binary_exp_weight` | source/target gather staging、distance formula、exp helper、checksum | KNN、Boost graph、max-flow |
| `buildgraph_potential_batch` | KNN、edge collection、Boost graph mutation、capacity / reverse map、duplicate marker、potential weights | public object lifecycle、`extract()`、max-flow solver |

## Checksum 来源

component checksum 来自 helper 的加权 sum；buildGraph-shaped checksum 来自插入 graph 的 capacity 权重和 edge count。由于 RVV binary path 使用 float `expf_RVV_f32m2`，checksum 只在 correctness 文档声明的预算内对拍，不能声称 strict double equivalence（严格 double 等价）。

## 当前 QEMU 证据

`make -C test-rvv/segmentation/min_cut_segmentation run_test_compare` 在 QEMU 上通过 Std / RVV 各 3 个 tests。QEMU 结果证明构建、路径和 correctness，不证明性能。

## 当前 Board 证据

| phase | target | summary | decision |
| --- | --- | --- | --- |
| 000 | `run_board_min_cut_repeated` | `log/board/repeated/summary.md` | unary `2.08x` positive；binary `2.44x` positive |
| 010 | `run_board_min_cut_buildgraph_repeated` | `log/board/buildgraph-repeated/summary.md` | `buildgraph_potential_batch` median `1.02x`，min `0.98x`，neutral |

## Evidence Doctor / Manifest 边界

| phase | manifest / doctor | 结果 | 处理 |
| --- | --- | --- | --- |
| 000 | `log/board/repeated/evidence_manifest.json`、`log/board/repeated/evidence_doctor.md` | `Errors=0，Warnings=0，Suggestions=0` | 支持进入 Phase 010 |
| 010 | `log/board/buildgraph-repeated/evidence_manifest.json`、`log/board/buildgraph-repeated/evidence_doctor.md` | `Errors=1，Warnings=0，Suggestions=1` | 降级为 neutral；不进入 production patch |

## ASM Attribution 口径

`make dump_bench_rvv` 生成 `build/asm/riscv/bench_min_cut_segmentation_rvv.full.asm` 和 `.asm`。当前 RVV bench binary 中可见 `vle32.v`、`vfmacc.vv`、`vfsqrt.v`、`vse32.v`、`vsetvli`，且 buildGraph-shaped 路径调用 unary / binary RVV helper。这只证明 diagnostic binary 的 RVV 指令存在，不证明 production public entry 命中。

## 复现命令和提交边界

默认提交策略为 `topic-only`：提交源码、脚本和文档，不提交 `build/`、raw logs 或完整 `log/` 树。若后续需要 evidence commit，可只用 `git add -f` 精确加入 `summary.md`、`evidence_manifest.json`、`evidence_doctor.md`，并先运行日志脱敏检查。
