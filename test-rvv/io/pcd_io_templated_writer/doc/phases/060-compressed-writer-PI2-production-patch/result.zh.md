# Phase 060: compressed writer PI2 production patch 结果

## 结果摘要

本阶段已完成 `writeBinaryCompressed<PointT>(file_name, cloud)` 的 production patch（生产补丁）、
production direct（真实生产入口直连）正确性测试、fallback（回退路径）测试、QEMU smoke
（QEMU 冒烟，只证明可运行和日志形状）、反汇编归属和 Milkv-Jupiter 板卡 production-public
（真实公开入口）5 次 repeated bench。

PI5 EvidenceDecision：`production-public positive / pending user adoption confirmation`。补丁建议保留，
但根据 production integration loop 规则，本阶段在 PI5 暂停，等待用户确认是否采纳；未确认前不创建
`doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`，也不把补丁写成 adopted production behavior。

## 实际生产补丁

修改文件：`io/include/pcl/io/impl/pcd_io.hpp`。

- 抽出 `pcl::io::detail::packBinaryCompressedFieldsStd`，复刻原本的 point-major 到 field-major 标量循环。
- 新增 `packBinaryCompressedFieldsRVV`，仅在 `__RVV10__ && __riscv_vector` 下编译。
- 新增 `compressedFieldsSupportRvv4BytePack` gate：点数非 0、字段非空、`sizeof(PointT)` 为 4 字节倍数、
  source / destination 基址 4 字节对齐、所有有效字段大小为 4 字节且 offset 4 字节对齐。
- 公开入口保持原 header、LZF、file lock、mmap/write、empty cloud 和错误路径；只把 pack 阶段改成
  “RVV 尝试，失败走 Std fallback”。
- 测试 hook 仅在 `PCL_RVV_PCD_WRITER_TEST_HOOK` 下启用，不改变公开 API。

未触碰路径：

- `writeBinary<PointT>`、ASCII writer、indices overload、PCLPointCloud2 writer / reader、`io/src/pcd_io.cpp`。
- 非 4 字节字段、非对齐字段、empty cloud、非 RVV 构建都走标量路径。

## 计划动作回填

| action | status | command / artifact | evidence |
| --- | --- | --- | --- |
| A1 production-direct RED test | done | `make -C test-rvv/io/pcd_io_templated_writer run_test_rvv` | 生产补丁前编译失败，缺少 `pcl_rvv_pcd_writer_compressed_*_test_hook`，RED 指向生产路径未接入。 |
| A2 fallback RED / GREEN test | done | `CompressedPublicWriterFallsBackForMixedFieldSizes` | mixed-size 自定义点型解压后 payload 等于标量 pack，hook 为 Scalar。 |
| A3 production patch | done | `io/include/pcl/io/impl/pcd_io.hpp` | public entry 只分流 pack helper；LZF / mmap / header 路径保持原状。 |
| A4 correctness | done | `make -C test-rvv/io/pcd_io_templated_writer run_test_compare` | Std/RVV 各 8 个 gtest 通过；RVV build hit case 命中 Rvv，fallback case 命中 Scalar。 |
| A5 production-public bench harness | done | `bench_pcdtw production_compressed_*` | QEMU smoke 可运行，case label 与 `point_step=16/20/16` 对齐。 |
| A6 asm attribution | done | `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv` | `writeBinaryCompressed<PCDTWPointXYZRGBPadding>` 和 `<PCDTWPointXYZRGBCompact>` 符号附近可见 `vlse32.v` / `vse32.v`。 |
| A7 board repeated production evidence | done | `make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_compressed_repeated` | 5-run summary / manifest / Doctor / registry 已生成并登记。 |
| A8 PI5 report and docs | done | 本 result、matrix、roadmap、evaluation、handoff、screening | PI5 暂停等待用户确认。 |

## Correctness 和 fallback 证据

`make -C test-rvv/io/pcd_io_templated_writer run_test_compare`：

- Std build：8/8 gtest 通过。
- RVV build：8/8 gtest 通过。
- 新增 production-direct tests：
  - `CompressedPublicWriterMatchesScalarPackAndHitsRvvPath`：真实 `PCDWriter::writeBinaryCompressed<PointT>`
    写临时文件，解压 compressed payload 后与标量 pack byte-equal；RVV build hook 为 Rvv，Std build hook 为 Scalar。
  - `CompressedPublicWriterFallsBackForMixedFieldSizes`：含 2 字节字段的自定义点型仍能写出与标量 pack 一致的 payload，
    RVV build hook 为 Scalar。

## QEMU smoke 和反汇编

- `make -C test-rvv/io/pcd_io_templated_writer run_bench_rvv BENCH_ARGS="--case-filter production_compressed_* --iterations 2 --warmup-iterations 1"`：
  QEMU 下 production bench 可运行，日志形状可被 summary 脚本解析；QEMU 计时不作为性能结论。
- `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv`：
  `bench_pcdtw_rvv.full.asm` 中的 public writer 实例化符号附近存在 `vlse32.v` / `vse32.v`。

## 板卡 production-public 证据

证据路径：

- summary：`log/board/production_compressed_repeat_5/summary.md`
- manifest：`log/board/production_compressed_repeat_5/evidence_manifest.json`
- Doctor：`log/board/production_compressed_repeat_5/evidence_doctor.md`
- registry：`log/evidence_registry.json`

5 次 repeated summary：

| case | mean speedup | median | min | max | mean Std ms | mean RVV ms | decision |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| `production_compressed_pointxyzrgba_4f_compact_262k` | `1.3122x` | `1.2969x` | `1.2192x` | `1.4410x` | `59.7364` | `45.5381` | positive |
| `production_compressed_pointxyzrgba_4f_padding_262k` | `1.2495x` | `1.2261x` | `1.1937x` | `1.3856x` | `58.1228` | `46.5272` | positive |
| `production_compressed_pointxyzrgba_4f_compact_small_512` | `1.1239x` | `1.0915x` | `0.9463x` | `1.3146x` | `0.5015` | `0.4459` | smoke-only weak / unstable |

大规模 compact 和 padding case 的 min 都大于 `1.19x`，decision bucket 稳定为 positive。small case 有 1/5
低于 1x，按计划只作为 smoke，不用于决定 production adoption。

## Evidence Doctor

Doctor 结果：Errors=0，Warnings=4，Suggestions=0。

Warnings 处理：

- 大规模 compact 和 padding case 都有 long tail / variance（长尾 / 方差）警告。处理方式：不剔除异常，
  在结论中保留 min / median / max；由于 min 仍显著高于 `1.05x`，不改变 positive bucket。
- small case 同时有退化频率和长尾警告。处理方式：降级为 smoke-only，不把 mean / median 正向外推到生产收益。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | 本阶段是 production-public；Phase 000/010 仍只作为历史 diagnostic / production-shaped diagnostic。 |
| A/B boundary | `PCDWriter::writeBinaryCompressed<PointT>` public overload，计时边界含 header、pack、LZF、mmap/write 和文件 checksum。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path。 |
| diagnostic 是否可外推到 production | 已不再外推；本阶段用真实公开入口重新测量。 |
| comparison-boundary / baseline mismatch 风险 | 仍有真实文件 I/O 和 LZF 稀释风险；板卡 production-public 结果显示大规模仍 positive。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段大规模不弱；small 不稳定已降级。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 不需要；当前 production 无既有 RVV family。 |

## 优化矩阵更新

| candidate family | evidence | decision | next action |
| --- | --- | --- | --- |
| compressed writer 4-byte RVV production pack | production-direct correctness、fallback、asm、board production-public positive、Doctor no Errors | bounded production candidate / PI5 positive | 暂停等待用户确认采纳；确认后创建正式 `doc-rvv` 文档并进入 production closeout。 |
| binary writer packed output | Phase 040/050 positive + PI1 complete | still pending-production-authorization | 当前不自动进入 binary PI2；用户确认压缩 writer 后可再选是否继续。 |

## Continue / Stop Decision

`continue_stop_decision`：停止在 PI5 user checkpoint。

`stop_condition_hit`：production integration loop 的 PI5 对称检查点。证据支持保留补丁，但 worker 不能自行把它写成
adopted production behavior，也不能创建正式 `doc-rvv` 文档或提交。下一步需要用户确认：

1. 采纳 / 保留当前 production patch：随后创建 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md`，用本阶段 production-public 板卡数据作为正式证据，并同步 closeout。
2. 不采纳 / 回滚当前 production patch：需要用户明确授权回滚。
3. 继续当前 topic 的其它方向：binary writer PI2 仍有 Phase 050 计划，但这是另一个 production patch 边界。
