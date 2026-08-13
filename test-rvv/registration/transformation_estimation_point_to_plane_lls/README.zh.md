# transformation_estimation_point_to_plane_lls 总览

本目录是 `registration/transformation_estimation_point_to_plane_lls` 的 RVV 专项测试工程。它包含 gtest、
bench、phase loop（阶段循环）、topic-local docs（主题本地文档）和 summary-only evidence（只提交摘要的证据）。

production 文件：

```text
registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls.hpp
```

当前 production candidate（生产候选）只覆盖 full-cloud（全云顺序扫描，source 和 target 按相同下标配对）
公开 overload：`Scalar=float`、source xyz f32 AoS layout gate（float 字段结构数组布局门控）、
target xyz+normal f32 AoS layout gate、规模/VLEN/byte-offset gate 均满足时，RVV path 使用
fused-formula block-reduction（融合公式分块规约）构造 point-to-plane normal equation（法方程）。
source-indexed、dual-indices、correspondences、weighted LLS、`Scalar=double` 和 layout miss 路径保持标量。

## 先读哪份文档

| 问题 | 文档 |
| --- | --- |
| 有哪些测试类型，`run_test` / `run_bench` 属于什么证据 | `doc/testing-overview.zh.md` |
| 每个 gtest 名称是什么意思，输入和断言是什么 | `doc/correctness-tests.zh.md` |
| bench label、case-filter、checksum、asm、registry 和日志提交边界怎么解释 | `doc/benchmark-and-evidence.zh.md` |
| 每种 RVV 优化方式对应哪些代码、target 和证据 | `doc/optimization-evidence.zh.md` |
| `include/`、`include/impl/` 和 `src/` 的函数族怎么组织 | `doc/test-support-code-map.zh.md` |
| 为什么采用当前 production candidate，历史候选如何取舍 | `doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md` |
| 当前 phase loop 如何恢复，哪些动作已闭合 | `doc/phases/README.zh.md` |
| 后续候选搜索空间和恢复条件 | `doc/optimization-roadmap.zh.md` |
| production 实现长期说明 | `../../../doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md` |

## 目录分工

| 路径 | 作用 |
| --- | --- |
| `include/teptpl.h` | test/bench 共用聚合入口。TEPTPL 是本 topic 在测试资产里的短标识。 |
| `include/test_teptpl.h` | gtest 专用聚合入口，加入 fixtures、assertions 和 production helper bridge。 |
| `include/bench_teptpl.h` | bench 专用聚合入口，加入 bench fixture、component helper 和 case registry。 |
| `include/impl/teptpl_*.hpp` | 标量 reference、RVV math、row source policy、reduction candidate、estimate candidate、gtest helper 和 bench helper 的内部实现。 |
| `src/test_teptpl_public_semantics.cpp` | gtest 源码，覆盖 public estimator 与 test-only reference 的语义对拍。 |
| `src/test_teptpl_candidates.cpp` | gtest 源码，覆盖 full-cloud candidate、block reduction、fused formula 和数值压力。 |
| `src/test_teptpl_production_direct.cpp` | gtest 源码，覆盖 production direct、fallback、generic point type 和 fused production path。 |
| `src/test_teptpl_row_sources.cpp` | gtest 源码，覆盖 source-indexed、dual-indices、correspondences 和 isolated fallback diagnostic。 |
| `src/bench_teptpl.cpp` | bench 薄入口，只转发到 `include/bench_teptpl.h` 中的 harness；label 和 case-filter 合同不变。 |
| `doc/` | topic-local 测试、证据、代码地图、评估和 phase 文档。 |
| `output/board/` | 可提交 summary-only board evidence；raw run 目录不在默认提交边界。 |
| `log/qemu/` | 当前登记的 QEMU correctness logs。 |
| `log/evidence_registry.json` | evidence registry（证据登记表），记录 summary / log 的 digest 和文档引用。 |

## 常用命令

QEMU correctness（正确性）对拍：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_compare
```

单独运行 std / RVV gtest：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_std
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls run_test_rvv
```

QEMU bench 日志形状检查（不作为性能结论）：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls \
  run_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
```

RVV bench 反汇编：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls dump_bench_rvv
```

板卡单次 smoke：

```bash
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls \
  run_board_bench_compare BENCH_ARGS="--size 65536,262144 --case-filter production-dispatch"
make -C test-rvv/registration/transformation_estimation_point_to_plane_lls fetch_board_logs
```

evidence registry freshness check：

```bash
python3 test-rvv/script/evidence_registry.py check \
  --registry test-rvv/registration/transformation_estimation_point_to_plane_lls/log/evidence_registry.json \
  --scan-glob 'test-rvv/registration/transformation_estimation_point_to_plane_lls/output/board/**/*.md' \
  --scan-glob 'test-rvv/registration/transformation_estimation_point_to_plane_lls/log/qemu/*.log' \
  --doc doc-rvv/registration/transformation_estimation_point_to_plane_lls-RVV.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/README.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/transformation_estimation_point_to_plane_lls-evaluation.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/benchmark-and-evidence.zh.md \
  --doc test-rvv/registration/transformation_estimation_point_to_plane_lls/doc/phases/README.zh.md \
  --require-doc-ref \
  --fail-on any
```

## 当前可提交证据

| 证据 | 角色 | 边界 |
| --- | --- | --- |
| `output/board/production_dispatch_generic_representative_5run_summary.md` | production-dispatch repeated board summary。 | 支撑三类代表点型 full-cloud production candidate；不外推到全部 gate-allowed 点型或其它 row source。 |
| `output/board/block_fused_formula_5run_summary.md` | diagnostic direct fused-formula A/B summary。 | 只解释 candidate 归因；不替代真实 production dispatch。 |
| `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` | QEMU correctness logs。 | 证明测试通过和路径形状，不提供性能结论。 |
| `log/evidence_registry.json` | freshness guard。 | 记录 digest 和 doc refs；不替代 Evidence Doctor 或性能结论。 |

`build/`、raw board archive、未被文档引用的 generated logs、本机 `config.mk` 和临时聊天记录不在默认提交边界。

## 当前不覆盖

- source-indexed、dual-indices 和 correspondences production RVV。
- weighted point-to-plane LLS。
- `Scalar=double` RVV。
- 不满足 source xyz / target xyz+normal f32 AoS layout gate 的点型组合。
- 满足 gate 但未逐类型上板的特殊点型性能结论。
