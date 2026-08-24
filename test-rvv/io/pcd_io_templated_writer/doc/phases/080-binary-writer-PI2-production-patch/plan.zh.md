# Phase 080: binary writer PI2 production patch 计划

## 阶段意图和边界

本阶段进入 `writeBinary<PointT>(file_name, cloud)` 的 PI2-PI5 production integration loop（生产接入闭环）。
Phase 040 证明 binary writer packed output loop（按点连续的有效字段输出循环）在 test-only component
ablation（测试专用组件消融）中大规模 positive；Phase 050 已冻结 PI1 范围。本阶段把该候选接入真实
production public entry（生产公开入口），并在接入后重跑 correctness、QEMU smoke、asm、板卡 repeated
bench、Evidence Doctor 和 registry。

本阶段不修改：

- `writeBinary(file_name, cloud, indices)` indices overload；indexed row source（索引行来源）需要独立 gather / scalar 证据。
- `writeBinaryCompressed<PointT>`，它已在 Phase 070 作为 adopted production behavior closeout。
- `writeASCII<PointT>`、PCLPointCloud2 writer / reader 和 public API。

## 当前状态清单

| item | current evidence |
| --- | --- |
| compressed writer | adopted；正式文档 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。 |
| binary component | Phase 040 result：median `1.2738x / 1.1842x / 6.8125x`，small case outlier-scoped。 |
| binary PI1 | Phase 050 plan/result 已冻结 production patch、fallback 和 evidence plan。 |
| production source | `writeBinary<PointT>(file_name, cloud)` 仍在 public entry 主体中执行 point × field `memcpy`。 |
| test hook | 现有 hook 只覆盖 compressed writer；binary writer 需要新 hook。 |
| board availability | 当前会话用户说明板卡可用；本阶段需要运行 board repeated target。 |

## Scope

validated_scope（本阶段准备证明）：

- entry：`PCDWriter::writeBinary<PointT>(file_name, cloud)` 非 indices overload。
- row source：contiguous `PointCloud<PointT>` rows。
- point type / layout：4 个 4 字节有效字段，compact 16B 和 tail-padding 20B registered point types。
- fallback：mixed-size field 走 Std；indices overload 保持现有标量路径。
- benchmark：真实 public binary writer 文件输出边界。

unvalidated_scope（本阶段不证明）：

- indices overload、non-4-byte fields、非对齐字段、任意自定义 point type、ASCII writer、PCLPointCloud2 writer。
- small 512 case 若出现退化，只作为 smoke，不作为 production adoption 主证据。

## 优化矩阵

| candidate family | row source / layout | correctness | bench / board | asm | Doctor | decision target |
| --- | --- | --- | --- | --- | --- | --- |
| binary writer production public patch | contiguous rows；4-byte aligned effective fields；compact/padding | public file payload byte-equal；RVV hit；mixed fallback；indices unchanged | `production_binary_*` public writer repeated | `vlse32.v` / `vsse32.v` near public writer/helper symbol | Errors=0 required；warnings explained | PI5-positive or rollback recommendation |
| indices overload | indexed rows | unchanged smoke / scalar hook not required | not_applicable | not_applicable | not_applicable | scalar-only |

## Production helper 计划

1. 抽出 `packBinaryFieldsStd`：复刻原 `for point -> for field -> memcpy(out, ...)`，写入 `map + data_idx`。
2. 新增 `binaryFieldsSupportRvv4BytePack` gate：点数非 0、字段非空、`sizeof(PointT)` 为 4 字节倍数、
   source / destination 基址 4 字节对齐、所有有效字段 size 和 offset 都是 4 字节对齐。
3. 在 `__RVV10__ && __riscv_vector` 下新增 `packBinaryFieldsRVV`：每个字段用 `vlse32.v` 从 point-major AoS
   跨步加载，再用 `vsse32.v` 按 packed output point stride 写回 `map + data_idx + field_offset`。
4. 新增 `PCL_RVV_PCD_WRITER_TEST_HOOK` 下的 binary writer path hook，和 compressed writer hook 并列。
5. public entry 保留 header、file open、lock、raw_fallocate、mmap、msync、munmap、close 和 error handling；
   只把 data copy loop 改成 RVV 尝试 + Std fallback。

## Test-first 计划

| test | expected RED before patch | GREEN criteria |
| --- | --- | --- |
| `BinaryPublicWriterMatchesScalarPackAndHitsRvvPath` | 编译失败：缺少 binary writer hook symbols。 | 真实 `writeBinary<PointT>` 写临时 PCD；payload 与 scalar binary pack byte-equal；RVV build hook 为 Rvv。 |
| `BinaryPublicWriterFallsBackForMixedFieldSizes` | 编译失败或 hook 缺失。 | mixed-size 点型输出与 scalar pack 一致；RVV build hook 为 Scalar。 |
| `BinaryIndicesOverloadKeepsScalarBehavior` | 可先作为 unchanged smoke。 | indices overload 写出的 payload 只含指定 indices；不依赖 RVV hook，不被 production patch 改变。 |

## Bench / asm / board 计划

- 新增 `production_binary_pointxyzrgba_4f_compact_262k`
- 新增 `production_binary_pointxyzrgba_4f_padding_262k`
- 新增 `production_binary_pointxyzrgba_4f_compact_small_512`
- QEMU smoke：`run_bench_rvv BENCH_ARGS="--case-filter production_binary_* --iterations 2 --warmup-iterations 1"`。
- asm：`dump_bench_rvv` 后确认 public writer path 附近有 `vlse32.v` / `vsse32.v`。
- board repeated：新增 `run_board_pcdtw_production_binary_repeated`，5 runs、20 iterations、3 warmup。
- manifest：`production_binary_*` 分类为 `production_public_binary_writer`，timer boundary 为
  `public_writer_header_pack_mmap_write_checksum_after_timing`。

## Evidence Doctor 和 registry 规则

- Doctor Errors 必须为 0。
- 大规模 compact / padding 若 min 和 median 稳定 positive，可进入 PI5-positive；small case 若有退化则降级 smoke-only。
- registry 必须登记 production binary summary / manifest / doctor。
- 如果 board 结果 weak / negative / unstable，本阶段不能自行回滚；PI5 保留补丁并报告是否建议回滚，等待用户确认。

## 继续 / 停止条件

本阶段继续到 PI5。合法停止条件：

- production direct correctness 无法修复；
- Evidence Doctor Error 无法修复；
- board / toolchain 不可用；
- board repeated 预算用尽但 decision bucket 仍 unstable，需要用户判断。

如果 production-public board 大规模 positive，则暂停在 PI5，建议用户采纳 / 保留 binary writer patch，并在用户确认后再创建或更新长期文档。

## 文档更新清单

- 本 phase result。
- README、evaluation、testing overview、correctness tests、benchmark/evidence、optimization evidence、roadmap、matrix、Handoff。
- 若 PI5 后用户确认采纳 binary writer，再刷新 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 040 是 component_ablation；本阶段必须产出 production-public。 |
| A/B boundary | `PCDWriter::writeBinary<PointT>(file_name, cloud)` public overload。 |
| 当前决策问题 | 当前 public RVV binary writer path 是否快于当前 public scalar path。 |
| diagnostic 是否可外推到 production | 不可直接外推；必须接入后重测。 |
| comparison-boundary / baseline mismatch 风险 | file open、lock、mmap/write、msync 和 checksum 可能稀释 component 收益。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段就是 bounded production probe；PI5 后按证据建议采纳或回滚。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；当前 binary writer 没有既有 RVV family。 |
