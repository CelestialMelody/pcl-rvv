# Phase 090: binary writer tuple / segment diagnostic 结果

## 当前结论

本阶段完成 `binary_tuple_*` test-only diagnostic（测试专用诊断）闭环。它不修改 production（生产源码），只证明新的 packed output 组织方式值得进入后续有界 production probe（生产探针）：compact 16B 布局走整点 `memcpy` fast path，tail-padding 20B 布局走 `vlse32.v` + `vsseg4e32.v` segment store（分段存储）。

Phase 090 的 decision bucket 为 `diagnostic_positive / production_probe_candidate`。它不能直接替代真实 `PCDWriter::writeBinary<PointT>` public overload（公开入口）证据；后续必须在接入 production 后重跑 production-public（真实公开入口）正确性、反汇编、板卡 repeated benchmark（重复性能测试）和 Evidence Doctor（证据体检）。

## 实际执行范围

| 计划动作 | 状态 | 证据 / 路径 | 结论 |
| --- | --- | --- | --- |
| tuple / segment test helper | done | `include/impl/pcdtw_support.hpp` | 新增 `packBinaryFieldsTupleCandidate`；RVV build 下 compact 16B 走 `BinaryTupleMemcpy`，padding 20B 走 `BinaryTupleSegmentRvv`，其它字段布局回退标量。 |
| correctness / fallback gtest | done | `src/test_pcdtw.cpp`；`make -C test-rvv/io/pcd_io_templated_writer run_test_compare` | Std/RVV 11 个 gtest 均通过；mixed field size fallback 保持 scalar。 |
| QEMU bench smoke | done | `make -C test-rvv/io/pcd_io_templated_writer run_bench_rvv BENCH_ARGS="--case-filter binary_tuple_* --iterations 2 --warmup-iterations 1"` | QEMU 只证明 bench case 可运行和日志可解析，不作为性能结论。 |
| asm attribution | done | `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv` | tuple / segment path 可见 `vlse32.v` 和 `vsseg4e32.v`；compact memcpy path 不要求 RVV 指令。 |
| board repeated | done | `log/board/binary_tuple_segment_repeat_5/summary.md` | 5-run 预算用完，decision bucket 稳定 positive。 |
| Evidence Doctor / registry | done | `log/board/binary_tuple_segment_repeat_5/evidence_doctor.md`；`log/evidence_registry.json` | Doctor `Errors=0 Warnings=2 Suggestions=0`；registry 已登记 `board-pcdtw-binary-tuple-segment-repeat-phase090`。 |

## 板卡结果

summary path：`test-rvv/io/pcd_io_templated_writer/log/board/binary_tuple_segment_repeat_5/summary.md`

| case | mean speedup | median | min | max | mean Std ms | mean RVV ms | 解释 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `binary_tuple_pointxyzrgb_4f_262k` | `9.8247x` | `9.7862x` | `9.7649x` | `9.9095x` | `13.5465` | `1.3789` | compact 16B fast path 强正向，但它是 memcpy fast path，不是 RVV intrinsic 收益。 |
| `binary_tuple_pointxyzrgb_4f_padding_262k` | `4.0905x` | `4.1229x` | `3.9890x` | `4.1516x` | `13.3132` | `3.2553` | padding 20B segment-store path 稳定正向；不能继承 compact case 的 9.8x。 |
| `binary_tuple_pointxyzrgb_4f_small_512` | `18.0526x` | `18.2500x` | `16.8462x` | `18.6667x` | `0.0220` | `0.0012` | small smoke（小规模冒烟）结果只说明路径开销很低，不作为生产采用主依据。 |

## Evidence Doctor 解释

Doctor path：`test-rvv/io/pcd_io_templated_writer/log/board/binary_tuple_segment_repeat_5/evidence_doctor.md`

- `Errors=0`：没有 checksum、A/B 边界或 degradation frequency（退化频率）阻塞。
- `Warnings=2`：
  - `binary_tuple_pointxyzrgb_4f_padding_262k` 是 group outlier（组内离群）：padding median `4.1229x` 明显低于 compact median `9.7862x`。处理方式是按 layout 单独报告，production gate 不能把 compact 收益外推到 padding。
  - `binary_tuple_pointxyzrgb_4f_small_512` 是 group outlier：small median `18.2500x` 明显高于大规模 case。处理方式是仅作为 smoke，不把 small case 当作生产吞吐主结论。
- `Suggestions=0`。

## Diagnostic-to-production mismatch audit 回填

| question | result |
| --- | --- |
| evidence role | `binary_tuple_segment_diagnostic`，不是 production-public。 |
| A/B boundary | `test_helper` / bench wrapper；不包含真实 header、mmap/write、file lock、error path 和 public overload 状态。 |
| 当前决策问题 | 新实现族是否值得进入 bounded production probe，而不是是否已采纳。 |
| diagnostic 是否可外推到 production | 只能作为强信号，不能直接外推。Phase 080 已证明 binary component positive 可能在 public boundary 下消失。 |
| comparison-boundary / baseline mismatch 风险 | 有。Phase 100 必须重建 production-public A/B。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮不是弱 / 负 / 中性 / 不稳定；大规模 compact 和 padding 均强正向，允许进入窄 production probe。 |
| clean adoption 是否需要同一 production boundary 证据 | 需要。即使 production-public 为正，也必须在 PI5 停下供用户检查确认最终采纳或回滚。 |

## 矩阵更新

| candidate family | row source | point type / layout | correctness | board | asm | Doctor | decision | next action |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| binary tuple / segment diagnostic | contiguous rows | 4 个连续 4-byte fields；compact 16B / padding 20B | Std/RVV 11/11 gtest pass；mixed fallback pass | compact mean `9.8247x`；padding mean `4.0905x`；small smoke mean `18.0526x` | padding path 有 `vlse32.v` + `vsseg4e32.v` | `Errors=0 Warnings=2 Suggestions=0`，warnings 已分 layout 解释 | diagnostic_positive / production_probe_candidate | 创建 Phase 100，接入窄 production probe 并重跑 public evidence。 |

## Continue / stop decision

本阶段没有命中停止条件。由于板卡可用、diagnostic 大规模结果强正向、Doctor 无阻断 Error，默认继续到 Phase 100：`binary-writer-tuple-segment-production-probe`。

Phase 100 必须保持窄范围：只覆盖 `writeBinary<PointT>(file_name, cloud)` contiguous rows（连续点云行）、4 个连续 4-byte 有效字段、compact 16B 和 tail-padding 20B 两种布局；`writeBinary(file_name, cloud, indices)`、非 4 字节字段、字段数量不为 4、offset 不连续、非对齐或其它泛型布局全部保持标量。
