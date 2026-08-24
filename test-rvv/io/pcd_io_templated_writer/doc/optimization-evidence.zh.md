# pcd_io_templated_writer 优化证据索引

## 本文职责

本文把 candidate family（候选族）映射到代码路径、测试、bench、板卡、asm、Evidence Doctor
和 decision（决策）。Roadmap 负责未来搜索空间；本文只记录已经发生或已计划到 phase 的证据状态。

## 当前结论摘要

| 状态 | candidate family |
| --- | --- |
| attempted positive | 4-byte field compressed writer RVV stride path |
| partial-production-candidate | pack+LZF production-shaped compressed writer diagnostic |
| adopted production behavior | compressed writer production public patch |
| adopted production behavior | binary tuple / segment production public patch |
| rollback/no-production | binary writer field-outer production public patch |
| rejected with evidence | ASCII writer |

## 优化方式总表

| candidate family | 代码路径 | correctness | bench / board | asm | Doctor | decision / boundary |
| --- | --- | --- | --- | --- | --- | --- |
| pack-only RVV stride path | `include/impl/pcdtw_support.hpp::packFieldsCandidate` | `run_test_compare` pass | `component_ablation_repeat_5/summary.md` positive | `vlse32.v` / `vse32.v` in bench asm | clean | attempted positive；diagnostic only。 |
| pack+LZF shaped path | `packAndCompressCandidate` + `bench_pcdtw compressed_*` | compressed payload byte-equal | `production_shaped_repeat_5/summary.md` positive | pack RVV appears；LZF scalar | clean | partial-production-candidate；supports PI1 only。 |
| compressed writer production public patch | `PCDWriter::writeBinaryCompressed<PointT>` + `bench_pcdtw production_compressed_*` | public writer payload byte-equal；mixed field fallback | `production_compressed_repeat_5/summary.md` positive：大规模 mean `1.3122x / 1.2495x` | public writer symbols contain `vlse32.v` / `vse32.v` | Errors=0 Warnings=4 Suggestions=0；warnings explained | adopted production behavior；正式文档 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。 |
| binary writer packed output | `packBinaryFieldsCandidate` + `bench_pcdtw binary_*` | binary packed bytes byte-equal；mixed size fallback | `binary_component_repeat_5/summary.md` positive；small outlier-scoped | `vlse32.v` / `vsse32.v` in bench asm | Errors=0 Warnings=1 Suggestions=0 | attempted positive；已触发 Phase 050 PI1。 |
| binary writer PI1 production plan | `doc/phases/050-binary-writer-PI1-production-integration-plan/*` | planned public entry tests | planned production bench | planned production symbol attribution | planned | completed；Phase 080 field-outer probe 后回滚，Phase 100 新实现族已采纳。 |
| binary writer field-outer production public patch | historical `PCDWriter::writeBinary<PointT>` + historical `bench_pcdtw production_binary_*`；current field-outer patch removed | 回滚前 public binary payload byte-equal；mixed-size fallback；indices overload unchanged | `production_binary_repeat_5/summary.md` negative：mean `0.9842x / 0.9727x / 0.9810x` | 回滚前 public writer symbols contained `vlse32.v` / `vsse32.v`；回滚后 field-outer helper 不存在 | Errors=3 Warnings=0 Suggestions=0 | rollback/no-production；不采纳。 |
| binary tuple / segment diagnostic | `packBinaryFieldsTupleCandidate` + `bench_pcdtw binary_tuple_*` | Std/RVV 11/11 pass；mixed fallback | `binary_tuple_segment_repeat_5/summary.md` positive：mean `9.8247x / 4.0905x / 18.0526x` | bench binary contains `vlse32.v` / `vsseg4e32.v` for padding | Errors=0 Warnings=2 Suggestions=0 | diagnostic-positive；触发 Phase 100。 |
| binary tuple / segment production public patch | `PCDWriter::writeBinary<PointT>` + `bench_pcdtw production_binary_tuple_*` | Std/RVV 15/15 pass；compact memcpy hit、padding RVV hit、mixed fallback、indices unchanged | `production_binary_tuple_repeat_5/summary.md` positive：mean `1.3046x / 1.3240x / 1.1082x` | production bench binary contains `vlse32.v` / `vsseg4e32.v` for padding | Errors=0 Warnings=1 Suggestions=0 | adopted production behavior；正式文档 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。 |
| ASCII writer | production source inspection | not_started | not_started | not_started | not_started | rejected with evidence；stream formatting / locale 风险主导。 |

## 标量路径与 RVV 路径差异

标量 reference 按 production 源码的双重循环执行：

```text
for point in cloud:
  for field in fields:
    memcpy(field_plane_ptr, point + field.offset, field_size)
```

RVV candidate 只在所有有效字段都是 4 字节且 point stride 对齐时接管 pack loop：

```text
for field in fields:
  vlse32.v 从 point-major AoS 跨步加载
  vse32.v 连续写入 field-major plane
```

LZF compression、header、file lock、mmap、write 和错误处理仍保持标量 / 既有库路径。

## Target 字典

| target / case | 隔离对象 | evidence role |
| --- | --- | --- |
| `run_test_compare` | helper-level correctness 和 fallback path hit | correctness gate |
| `run_qemu_smoke` | QEMU correctness + RVV bench log-shape | QEMU smoke |
| `dump_bench_rvv` | diagnostic bench binary RVV 指令存在性 | asm attribution |
| `run_board_pcdtw_repeated` | pack-only component ablation | board performance diagnostic |
| `run_board_pcdtw_shaped_repeated` | pack+LZF production-shaped diagnostic | board performance diagnostic |
| `run_board_pcdtw_binary_repeated` | binary writer packed output component ablation | board performance diagnostic |
| historical `run_board_pcdtw_production_binary_repeated` | binary writer production public overload before rollback | Phase 080 historical board production-public evidence |
| `run_board_pcdtw_binary_tuple_segment_repeated` | binary tuple / segment output diagnostic | Phase 090 diagnostic board evidence |
| `run_board_pcdtw_production_binary_tuple_repeated` | binary tuple / segment production public overload | Phase 100 production-public evidence |

## 结论边界

当前 compressed writer production patch 已 adopted。Phase 060 已补真实 public entry direct test、
fallback test、production asm attribution 和 production board evidence；Phase 070 已按用户确认创建
正式 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。binary writer Phase 080 production-public evidence
为负向，已回滚为 no-production；Phase 100 tuple / segment 新实现族接入后为正向，当前已采纳。
若继续探索，必须另开新范围 phase，不从 Phase 100 外推到未覆盖布局。
