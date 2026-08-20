# transformation_estimation_2D RVV 主题入口

本目录是 `registration/transformation_estimation_2D` 的 RVV topic（主题）入口。测试资产里的
topic token（主题短标识）为 `te2d`；production 符号仍使用
`transformation_estimation_2D`。

目标 production 文件：

```text
registration/include/pcl/registration/transformation_estimation_2D.h
registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
```

## 当前结论

当前 production patch 保留四条 RVV 路径。Phase 114 只整理测试支撑结构，不修改 production
source（生产源码）或扩大既有 gate：

| row source | 当前状态 | 生产 gate / fallback |
| --- | --- | --- |
| ordered-cloud-pair | adopted / retained | `Scalar=float`；source/target 分别满足 `RVVXYZAoSFloatLayout<PointT>`；dense、finite、size >= 16；失败回退现有 iterator scalar path。 |
| source-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`；`Scalar=float`；source indices 有效；dense、finite、size >= 16；失败回退 source-indexed iterator scalar path。 |
| dual-indexed-cloud-pair | adopted / retained | exact `PointXYZ -> PointXYZ`；`Scalar=float`；source/target indices 有效；dense、finite、size >= 16；失败回退 dual-indexed iterator scalar path。 |
| correspondence-pair | not adopted / rolled back | Phase 107 试接入后 family A/B 为 negative；当前 header 没有 correspondence RVV production dispatch。 |

仍不能写成 adopted 的范围：

- source-indexed generic PointXYZ-like widening：Phase 103/104 仍是 guarded probe；Phase 106 独立
  20-run public variance 为 negative，不能 clean-adopt。
- dual-indexed generic 和 correspondence generic：Phase 100 / 101 代表性点型诊断为 negative。
- `Scalar=double`、未逐类型上板的自定义点型、RGB/RGBA 语义和其它 row source 不继承上述结论。

## 接入后证据

接入后测试是 production integration loop（生产接入闭环）的必跑项，不依赖用户提醒。本轮已完成：

| 验证 | 当前结果 |
| --- | --- |
| correctness | Std `84/84` pass；RVV `84/84` pass。 |
| ordered generic board | Phase 107 `generic_xyz_point_types_public_phase107_repeated`：16 cases 全部 positive，`PointXYZ->PointXYZ` 4K/64K/256K 为 `4.400x / 5.556x / 5.184x`；Doctor `0/4/0`。 |
| source-indexed exact board | Phase 107 `source_indexed_public_phase107_repeated`：4K/64K/256K 为 `4.157x / 4.814x / 4.615x`；Doctor `0/0/0`。 |
| dual-indexed exact board | Phase 107 `dual_indexed_family_ab_phase107_repeated`：direct/materialize B/A 4K/64K/256K 为 `1.085x / 1.691x / 1.678x`；4K 有 `1/5` below-1 caveat；Doctor `0/3/0`。 |
| correspondence exact board | Phase 107 `correspondence_family_ab_phase107_repeated`：4K/64K/256K 为 `1.091x / 1.645x / 1.385x`，但 256K `4/20` below-1；overall negative，dispatch 已退回；Doctor `0/4/0`。 |
| source-indexed generic variance | Phase 106 独立 20-run：12 positive、1 weak_positive、3 negative；`PointNormal->PointNormal 256K` 为 `7/20` below-1；Doctor `1/27/0`。 |

QEMU（仿真器）只作为 correctness、路径命中、日志形状和反汇编归属证据；性能结论只引用 board /
target hardware repeated benchmark（板卡或目标硬件重复性能测试）。

## 先读哪份文档

| 问题 | 文档 |
| --- | --- |
| 函数做什么、标量路径、生产接入判断和 Traceability Map（可追踪性地图） | `doc/transformation_estimation_2D-evaluation.zh.md` |
| 当前 production 行为、fallback 矩阵和长期证据链 | `../../../doc-rvv/registration/transformation_estimation_2D-RVV.zh.md` |
| 测试入口和覆盖矩阵 | `doc/testing-overview.zh.md` |
| 每个 gtest 的输入、断言和证明范围 | `doc/correctness-tests.zh.md` |
| bench label、QEMU / board / Evidence Doctor 边界 | `doc/benchmark-and-evidence.zh.md` |
| adopted / attempted / rejected / deferred 优化证据 | `doc/optimization-evidence.zh.md` |
| 候选族和后续恢复动作 | `doc/optimization-roadmap.zh.md` |
| 候选 × row source × 证据状态 | `doc/phases/optimization-matrix.zh.md` |
| 阶段当前入口 | `doc/phases/README.zh.md` |
| 历史 phase 为什么这么多 | `doc/phases/history.zh.md` |
| 测试支撑代码地图 | `doc/test-support-code-map.zh.md` |

## 目录分工

| 路径 | 作用 |
| --- | --- |
| `Makefile`、`board.mk` | topic-local build、QEMU、board、Evidence Doctor 和 evidence registry 入口。 |
| `src/test_te2d.cpp` | public semantics、fallback、row source 和 production direct correctness。 |
| `src/bench_te2d.cpp` | QEMU smoke、board repeated、family A/B 和 historical guarded probe bench 入口。 |
| `include/te2d.h` | 稳定聚合入口；测试和 bench 只 include 这个文件。 |
| `include/impl/te2d_core_types.hpp` | 共享 include、`CandidateStats` 和 `Fused2DAccumulation`。 |
| `include/impl/te2d_fixtures.hpp` | fixtures、代表性点型样本和二维刚体变换构造。 |
| `include/impl/te2d_layout_helpers.hpp` | layout gate、finite/dense 检查和 stats 填充。 |
| `include/impl/te2d_row_sources.hpp` | source-indexed、dual-indexed、correspondence 的索引统计、合法性检查和物化。 |
| `include/impl/te2d_public_wrappers.hpp` | 真实 public overload 的 test-only wrapper。 |
| `include/impl/te2d_family_ab.hpp` | production-detail family A/B 对照 wrapper。 |
| `include/impl/te2d_ordered_candidates.hpp` | ordered-cloud-pair 标量 / RVV fused candidate。 |
| `include/impl/te2d_source_indexed_candidates.hpp` | source-indexed materialize 和 direct-gather test candidates。 |
| `include/impl/te2d_dual_indexed_candidates.hpp` | dual-indexed materialize 和 direct-gather test candidates。 |
| `include/impl/te2d_correspondence_candidates.hpp` | correspondence direct-gather、chunked staging 和 materialize candidates。 |
| `include/impl/te2d_checksums.hpp` | matrix diff 和 checksum helper。 |
| `script/**` | topic-local summary、manifest、asm attribution、registry 和 board repeated 解析脚本。 |
| `doc/*.zh.md` | topic-local doc suite（主题本地文档套件）。 |
| `doc/phases/README.zh.md` | 当前 phase loop 恢复入口和提交前导航。 |
| `doc/phases/history.zh.md` | Phase 000-107 压缩历史索引；替代 root README 的长 phase 目录表。 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段候选、row source、点类型、证据和决策矩阵。 |
| `doc/phases/106-*`、`107-*`、`108-*` | 当前提交前最关键的 source-indexed generic variance、beneficial adoption loop 和 doc compaction 记录。 |
| `log/**` | evidence summary / manifest / Doctor 产物；raw logs 默认 local-only。 |
| `build/**` | 二进制和 asm dump，默认 local-only。 |

## 常用命令

```bash
make -C test-rvv/registration/transformation_estimation_2D run_test_compare
make -C test-rvv/registration/transformation_estimation_2D record_qemu_correctness_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_production_public_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_source_indexed_public_state
make -C test-rvv/registration/transformation_estimation_2D record_qemu_dual_indexed_family_ab_state
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_generic_xyz_point_types_public_repeated TE2D_BOARD_GENERIC_PUBLIC_RUN_LABEL=generic_xyz_point_types_public_phase107_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_source_indexed_public_repeated TE2D_BOARD_SOURCE_INDEXED_PUBLIC_RUN_LABEL=source_indexed_public_phase107_repeated
make -C test-rvv/registration/transformation_estimation_2D run_board_bench_dual_indexed_family_ab_repeated TE2D_BOARD_DUAL_INDEXED_FAMILY_AB_RUN_LABEL=dual_indexed_family_ab_phase107_repeated
make -C test-rvv/registration/transformation_estimation_2D evidence_status
```

历史 guarded probe、generic point-type 诊断和 correspondence profile target 仍保留在 `Makefile` /
`board.mk` 中；默认阅读入口放在 `doc/testing-overview.zh.md` 和
`doc/benchmark-and-evidence.zh.md`，不再在本 README 重复完整 target 清单。

## 本次提交边界

Phase 114 提交采用 topic-only（仅主题）策略：

| 产物 | 提交边界 |
| --- | --- |
| production patch | 不提交 production source；Phase 114 没有生产源码变更。 |
| topic test assets | 提交 `include/te2d.h`、新增 `include/impl/te2d_*.hpp` 和历史单一实现入口删除。 |
| topic docs | 提交 `README.zh.md`、`doc/*.zh.md`、`doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`、Phase 114 plan/result。 |
| long-term doc | 只在需要同步测试支撑地图或旧引用时更新；不改变 production 结论。 |
| evidence summaries | 本次不提交 `log/**`；`run_test_compare` 和 QEMU smoke 只刷新本地 generated logs。 |
| local recovery | `tmp/rvv-work-logs/**` 默认 local-only；如本次需要保留交接包，可作为独立审查对象，不和 raw logs 混在一起。 |

默认不提交：`log/**`、`build/`、raw QEMU logs、raw board logs、本机 `config.mk`、私有部署路径、聊天记录、
未被文档引用的临时日志，以及无关 dirty files。

## 当前恢复动作

Phase 114 当前负责测试支撑职责拆分和文档同步：历史单一实现入口已删除，`include/te2d.h`
作为稳定聚合入口保留。完成后默认恢复动作是 reviewer 检查 Commit B；不创建新的 Normal、
correspondence 或 generic widening 优化 phase。
