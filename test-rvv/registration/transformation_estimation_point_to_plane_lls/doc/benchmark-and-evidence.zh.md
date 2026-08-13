# transformation_estimation_point_to_plane_lls Benchmark And Evidence

## 本文职责

本文解释 TEPTPL bench 的 label（性能测试标签）、case-filter（用例过滤）、checksum（校验和）、
QEMU / board 边界、反汇编入口、Evidence Doctor（证据体检）和 evidence registry（证据登记表）。
当前 `src/bench_teptpl.cpp` 是薄入口，真实 bench harness 位于 `include/bench_teptpl.h` 和
`include/impl/teptpl_bench_fixtures.hpp`、`include/impl/teptpl_bench_components.hpp`、
`include/impl/teptpl_bench_cases.hpp`。性能结论只来自目标硬件或板卡 repeated summary；
QEMU timing 不作为性能结论。

## Bench CLI

默认 bench 选项：

| 参数 | 默认 | 含义 |
| --- | --- | --- |
| `--iterations` | `20` | 每个 case 的迭代次数。 |
| `--size` | `65536,262144` | 点数规模列表。 |
| `--case-filter` | 空 | 只运行 label 中包含该子串的 case。 |
| `--component-only` | `false` | 只运行组件消融 rows。 |

`case-filter` 是子串过滤，不是语义白名单。比如 `production-dispatch` 只选真实 public overload rows；
`block-fused-formula` 会选 direct helper rows，不等于 production-dispatch。

## Bench Label 字典

| label 模式 | 被测路径 | 证明点 | 边界 |
| --- | --- | --- | --- |
| `lls component full-cloud load-store-only pointnormal` | full-cloud SoA load/store component。 | load/store 基础成本。 | component-only，非端到端。 |
| `lls component dual-indices independent-stream gather-load-store-only pointnormal` | dual-index gather load/store component。 | gather / 双流索引成本线索。 | 不等同 correspondences query/match 展开。 |
| `lls component full-cloud formula-store-only pointnormal` | full-cloud formula store。 | 公式生成成本线索。 | 不含 solve。 |
| `lls component full-cloud mask-compress-only pointnormal` | finite mask / compress component。 | invalid lane 和 compress 成本线索。 | 不代表 production dispatch。 |
| `lls component full-cloud tail-only pointnormal` | scalar tail over staged formula。 | 尾段成本线索。 | 历史 staging 诊断。 |
| `lls component full-cloud no-solve pointnormal` | early full-cloud candidate no-solve。 | normal-equation 构造成本。 | 不含 Eigen solve。 |
| `lls component full-cloud fused-reduction no-solve pointnormal` | fused-reduction no-solve。 | 早期 reduction 组织归因。 | 当前未采用。 |
| `lls component full-cloud grouped-reduction no-solve pointnormal` | grouped-reduction no-solve。 | grouped candidate 归因。 | 当前未采用。 |
| `lls component full-cloud block-reduction no-solve pointnormal` | block-reduction no-solve。 | current block baseline 归因。 | direct helper，不等于 production dispatch。 |
| `lls component full-cloud block-fused-formula no-solve pointnormal` | block-fused-formula no-solve。 | fused formula direct 归因。 | direct helper，不等于 production dispatch。 |
| `lls normal-equation full-cloud pointnormal` | historical full-cloud candidate。 | 端到端 helper 诊断。 | 非 production dispatch。 |
| `lls normal-equation full-cloud trusted-dense pointnormal` | trusted-dense diagnostic。 | 跳过 finite 成本假设。 | public finite semantics 未批准。 |
| `lls normal-equation full-cloud fused-reduction pointnormal` | fused-reduction diagnostic。 | 早期 candidate 对比。 | 当前未采用。 |
| `lls normal-equation full-cloud grouped-reduction pointnormal` | grouped-reduction diagnostic。 | 早期 candidate 对比。 | 当前未采用。 |
| `lls normal-equation full-cloud block-reduction pointnormal` | block-reduction direct helper。 | current block baseline。 | direct helper。 |
| `lls normal-equation full-cloud block-fused-formula pointnormal` | fused formula direct helper。 | fused formula candidate 归因。 | performance 结论需看 board summary。 |
| `lls production-dispatch full-cloud pointnormal` | `PointNormal -> PointNormal` public full-cloud overload。 | exact PointNormal 子集真实 dispatch。 | 不外推到其它点型。 |
| `lls production-dispatch full-cloud pointxyz-to-pointnormal` | `PointXYZ -> PointNormal` public overload。 | generic source xyz gate 代表点型。 | 不外推所有 source 点型性能。 |
| `lls production-dispatch full-cloud pointxyz-to-pointxyzinormal` | `PointXYZ -> PointXYZINormal` public overload。 | generic target xyz+normal gate 代表点型。 | 不外推所有 target 点型性能。 |
| `lls public-entry-shaped full-cloud block-reduction pointnormal` | std public overload vs RVV bench-only shim。 | 公开入口形态兼容。 | 不是真实 dispatch。 |
| `lls normal-equation source-indices pointnormal` | source-indexed diagnostic helper。 | source gather row source 诊断。 | production 仍标量。 |
| `lls normal-equation source-indices trusted-dense pointnormal` | source-indexed trusted-dense diagnostic。 | 历史假设。 | 不接 production。 |
| `lls normal-equation dual-indices same-stream pointnormal` | dual-index same stream helper。 | 分布相关性诊断。 | production 仍标量。 |
| `lls normal-equation dual-indices independent-stream pointnormal` | dual-index independent streams。 | 两侧 index stream 诊断。 | production 仍标量。 |
| `lls normal-equation dual-indices independent-stream trusted-dense pointnormal` | dual-index trusted-dense。 | 历史假设。 | 不接 production。 |
| `lls normal-equation correspondences same-index pointnormal` | same-index correspondences。 | query/match 展开成本较低形态。 | 不等于 dual-indices。 |
| `lls normal-equation correspondences local-offset pointnormal` | local-offset correspondences。 | match 偏移但保持局部性。 | production 仍标量。 |
| `lls normal-equation correspondences independent-stream pointnormal` | independent-stream correspondences。 | query/match 双流展开诊断。 | production 仍标量。 |
| `lls normal-equation correspondences independent-stream trusted-dense pointnormal` | independent-stream trusted-dense。 | 历史假设。 | 不接 production。 |

## 推荐命令

QEMU production-dispatch log-shape：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls \
  run_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
```

QEMU component-only smoke：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls \
  run_bench_compare BENCH_ARGS="--component-only --size 65536 --case-filter no-solve"
```

RVV asm dump：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls dump_bench_rvv
```

板卡单次 production-dispatch smoke：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls \
  run_board_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls fetch_board_logs
```

repeated board summary 当前由人工多轮 fetch + `test-rvv/script/analyze_bench_repeated.py` 生成稳定 summary。
新 repeated run 改变数值、direction bucket（方向桶）或 Evidence Doctor 数量时，必须刷新 evaluation、topic doc、
phase result 和 evidence registry。

## Checksum 和输出合同

每个 bench case 输出：

```text
<case label>: <avg> ms/iter
  Total Time: <total> ms
  Checksum: <checksum>
```

checksum 来自 matrix checksum 或 normal-equation checksum 加上很小的 `accepted_points` 标记，用于 std/RVV 日志形状和输出指纹检查。checksum 一致不替代完整 correctness tests；checksum 不一致时禁止写性能结论。

## 当前 Board Summary

| summary | 角色 | 当前结论 | Evidence Doctor 边界 |
| --- | --- | --- | --- |
| `output/board/production_dispatch_generic_representative_5run_summary.md` | production-dispatch repeated board summary。 | fused formula 三类代表点型 64K/256K median 均正向，支撑当前 production candidate。 | summary-only metadata incomplete；没有 JSON manifest，doctor 只能人工/Markdown 辅助检查。 |
| `output/board/block_fused_formula_5run_summary.md` | diagnostic direct fused-formula A/B summary。 | fused formula direct helper 较 current block direct helper正向。 | diagnostic evidence，不替代 production dispatch。 |

当前稳定 evidence policy 是 summary-only：summary artifact 可以进入提交候选，raw run archive 不默认提交。

## 反汇编边界

`dump_bench_rvv` 生成：

```text
build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_rvv.full.asm
build/asm/riscv/bench_transformation_estimation_point_to_plane_lls_rvv.asm
```

已记录的归属结论：

- helper-level symbols 中可见 strided load、finite mask、`vfmsac.vv`、`vfmacc.vv`、`vcpop` 和 `vfredosum`。
- production-dispatch public overload case 的 call site 会跳到 fused helper 符号。
- 直接 grep 到的其它 RVV 指令可能来自 component/direct diagnostic、bench harness、Eigen 或编译器自动向量化；不能单独作为 production hot path 证据。

## Evidence Registry

registry 文件：

```text
test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json
```

当前登记：

| run label | 文件 | 角色 |
| --- | --- | --- |
| `production-dispatch-generic-representative-5run` | `output/board/production_dispatch_generic_representative_5run_summary.md` | post-production performance summary。 |
| `block-fused-formula-5run` | `output/board/block_fused_formula_5run_summary.md` | diagnostic performance summary。 |
| `qemu-correctness-std-rvv-phase-030` | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness logs。 |

检查命令见 `README.zh.md`。如果新增文档只引用登记文件，不改变 evidence 文件 digest；如果重跑 test 覆盖 QEMU log，必须重新 `record`。

## 日志提交边界

| 类别 | 默认策略 |
| --- | --- |
| board summary `.md` | 被文档引用且 registry fresh 时可提交。 |
| `log/evidence_registry.json` | 作为 summary-only freshness artifact 可提交。 |
| QEMU correctness logs | 只有当前文档引用且 registry fresh 时可作为 summary evidence；默认仍需 review。 |
| QEMU bench logs | 默认不作为性能证据；只有日志形状检查需要时引用。 |
| raw board run archive | local-only，不默认提交。 |
| build binaries / asm full dump | 默认不提交；asm 摘要可由文档引用。 |
