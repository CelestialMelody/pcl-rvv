# transformation_estimation_svd RVV 主题入口

本目录保存 `registration/transformation_estimation_svd` 的 test-rvv（RVV 测试资产）、production direct（真实生产路径证据）和阶段文档。当前 production（生产源码）已有本 topic 的窄范围 RVV 补丁，真实目标源码是：

- `registration/include/pcl/registration/transformation_estimation_svd.h`
- `registration/include/pcl/registration/impl/transformation_estimation_svd.hpp`

`tesvd` 是本 topic 的短标识，对应 `transformation_estimation_svd`。

## 当前结论

当前结论是 `production-ready / adopted for ordered-cloud-pair, source-indexed-cloud-pair, dual-indices-cloud-pair and correspondence-pair`。Phase 020 已把 Phase 010 的正向诊断落到真实 public ordered-cloud-pair（顺序点云对，source/target 按相同下标一一对应）入口；Phase 040 又把 Phase 030 的 source-indexed-cloud-pair（源索引点云对，`source[indices_src[i]]` 与 `target[i]` 配对）诊断候选接入真实 public source-indexed overload。Phase 060 继续把剩余两个 row source policy 接入真实 public dual-indices 和 correspondence overload。四条生产路径的共同 gate 是 `Scalar=float`、`use_umeyama_ == true`、source/target 分别满足 `pcl::rvv::RVVXYZAoSFloatLayout`、输入 dense 且 `n >= 16`；source-indexed 额外要求 source indices 全部合法且 source cloud 满足 32-bit byte offset gather 边界，dual-indices 额外要求 source/target 两侧 indices 等长且都落在各自 cloud 内，correspondence 额外要求 query/match 都合法。其它情况自然 fallback（回退）到原 `ConstCloudIterator` 标量路径。

ordered-cloud-pair 生产直连板卡证据来自 `production_ordered_cloud_pair_repeated`：`PointXYZ` public Umeyama Std/RVV median 为 4K `14.372x`、64K `24.471x`、256K `23.841x`，overall decision bucket 为 `positive`，Evidence Doctor（证据体检）为 Errors=0、Warnings=1、Suggestions=0。source-indexed 生产直连板卡证据来自 `production_source_indexed_cloud_pair_repeated`：public Std/RVV median 为 4K `9.634x`、64K `12.217x`、256K `11.558x`，overall decision bucket 为 `positive`，Evidence Doctor 为 Errors=0、Warnings=0、Suggestions=0。dual-indices 生产直连板卡证据来自 `production_dual_indices_cloud_pair_repeated`：public Std/RVV median 为 4K `6.805x`、64K `6.404x`、256K `5.964x`，overall decision bucket 为 `positive`，Evidence Doctor 为 Errors=0、Warnings=1、Suggestions=0。correspondence 生产直连板卡证据来自 `production_correspondence_pair_repeated`：public Std/RVV median 为 4K `8.649x`、64K `8.644x`、256K `7.872x`，overall decision bucket 为 `positive`，Evidence Doctor 为 Errors=0、Warnings=1、Suggestions=0。

`PointXYZI`、`PointXYZRGB` 与其它 gate-allowed（门控允许）xyz AoS 点型不再被排除；它们通过 production-direct correctness 证明分流和语义成立，但未逐类型上板，性能只继承代表性 `PointXYZ` 判断。`Scalar=double` 仍保持标量 fallback，不纳入当前 production 结论。

## 先读哪份文档

| 读者问题 | 首选入口 | 说明 |
| --- | --- | --- |
| 为什么先做诊断而不是直接改 production | `doc/transformation_estimation_svd-evaluation.zh.md` | 这里记录标量路径、Traceability Map（可追踪性地图）、候选边界和初始生产接入判断。 |
| 当前 phase 做了什么 | `doc/phases/060-dual-indices-correspondences-production-integration/result.zh.md` | dual-indices / correspondence 的生产补丁、production direct tests、反汇编、板卡和 EvidenceDecision；ordered-cloud-pair 与 source-indexed 见 Phase 020/040 result。 |
| 测试和 bench 分别证明什么 | `doc/testing-overview.zh.md` | 测试层级、QEMU / board 边界和覆盖矩阵。 |
| 每个 gtest 的语义 | `doc/correctness-tests.zh.md` | correctness（正确性）测试字典。 |
| bench 输出和证据边界 | `doc/benchmark-and-evidence.zh.md` | case-filter、计时边界、checksum（校验和）和 Evidence Doctor（证据体检）计划。 |
| helper 在证据链里的位置 | `doc/test-support-code-map.zh.md` | 聚合入口、internal helper、src 和 production 对照。 |
| 继续当前 topic 从哪里恢复 | `doc/optimization-roadmap.zh.md`、`doc/phases/README.zh.md` | 默认恢复动作和候选搜索空间。 |
| 长期 production 行为是什么 | `../../doc-rvv/registration/transformation_estimation_svd-RVV.zh.md` | 只记录已采用生产行为、fallback 矩阵、证据链和长期风险。 |

## 目录分工

| 目录 / 文件 | 主职责 | 提交边界 |
| --- | --- | --- |
| `include/tesvd.h` | 测试支撑聚合入口 | topic-local test asset，审查后可纳入 topic commit。 |
| `include/impl/tesvd_support.hpp` | fixture、统计结构和样本构造 helper | topic-local test asset；用于对拍 production direct。 |
| `include/impl/tesvd_candidates.hpp` | 标量 reference、test-only RVV candidate 和 checksum helper | topic-local test asset；用于对拍 production direct。 |
| `src/test_tesvd.cpp` | QEMU / board correctness gtest | topic-local test asset。 |
| `src/bench_tesvd.cpp` | QEMU bench smoke 和 board bench wrapper | topic-local test asset；QEMU timing 不作为性能结论。 |
| `doc/` | evaluation、测试说明、bench 证据、优化证据和代码地图 | topic-local documentation，review-required。 |
| `doc/phases/` | phase plan/result、optimization matrix 和恢复入口 | phase_docs，review-required。 |
| `../../registration/include/pcl/registration/impl/transformation_estimation_svd.hpp` | 已接入的 ordered-cloud-pair / source-indexed-cloud-pair RVV production helper 和 public dispatch | production patch，review-required。 |
| `../../doc-rvv/registration/transformation_estimation_svd-RVV.zh.md` | 长期 production 主题文档 | production topic doc，PI5 通过后适用。 |
| `log/evidence_registry.json` | evidence registry（证据登记表） | 当前被 `.gitignore` 保持为 ignored-local；提交阶段若要保留需单独 allowlist 或强制加入并审查。 |
| `log/qemu/`、`log/board/`、`build/` | 可再生成证据和构建产物 | 默认 local-only，除非被文档引用且用户授权。 |

## 常用命令

| 命令 | 证据角色 | 输出 |
| --- | --- | --- |
| `make -C test-rvv/registration/transformation_estimation_svd run_test_compare` | QEMU correctness（QEMU 正确性验证） | `log/qemu/run_test_std.log`、`log/qemu/run_test_rvv.log` |
| `make -C test-rvv/registration/transformation_estimation_svd dump_bench_rvv` | asm attribution（反汇编归因）输入 | `build/asm/riscv/bench_transformation_estimation_svd_rvv.asm` |
| `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd run_bench_compare BENCH_ARGS="--case-filter ordered-cloud-pair --iterations 3 --warmup-iterations 1"` | QEMU bench smoke（日志形状，不是性能结论） | `log/qemu/analyze_bench_compare.log` |
| `ALLOW_QEMU_BENCH_COMPARE=1 make -C test-rvv/registration/transformation_estimation_svd run_bench_compare BENCH_ARGS="--case-filter source-indexed-cloud-pair --iterations 3 --warmup-iterations 1"` | QEMU bench smoke（source-indexed 日志形状，不是性能结论） | `log/qemu/analyze_bench_compare.log` |
| `make -C test-rvv/registration/transformation_estimation_svd evidence_status` | evidence registry（证据登记表）新鲜度检查 | fresh / stale / unregistered 输出 |
| `make -C test-rvv/registration/transformation_estimation_svd check_board_ssh` | board availability（板卡可用性）检查 | Phase 010 已通过。 |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_test_smoke` | board correctness smoke（板卡正确性小型验证） | `log/board/test_smoke/run_test.log` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_ordered_cloud_pair_repeated` | board repeated diagnostic（板卡重复诊断） | `log/board/fused_full_cloud_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_production_ordered_cloud_pair_repeated` | production direct board performance（生产直连板卡性能） | `log/board/production_ordered_cloud_pair_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_source_indexed_cloud_pair_repeated` | source-indexed board diagnostic（源索引板卡诊断） | `log/board/source_indexed_cloud_pair_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_production_source_indexed_cloud_pair_repeated` | source-indexed production direct board performance（源索引生产直连板卡性能） | `log/board/production_source_indexed_cloud_pair_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_dual_indices_cloud_pair_repeated` | dual-indices board diagnostic（双索引板卡诊断） | `log/board/dual_indices_cloud_pair_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_production_dual_indices_cloud_pair_repeated` | dual-indices production direct board performance（双索引生产直连板卡性能） | `log/board/production_dual_indices_cloud_pair_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_correspondence_pair_repeated` | correspondence board diagnostic（对应关系板卡诊断） | `log/board/correspondence_pair_repeated/summary.md`、`evidence_doctor.md` |
| `make -C test-rvv/registration/transformation_estimation_svd run_board_bench_production_correspondence_pair_repeated` | correspondence production direct board performance（对应关系生产直连板卡性能） | `log/board/production_correspondence_pair_repeated/summary.md`、`evidence_doctor.md` |

`fused-full-cloud` case-filter、`fused_full_cloud_repeated` 目录和相关 Make target 是 Phase 000/010 的 legacy label（历史标签），为保持已有 summary、manifest 和 registry 可复核而保留；当前常用入口改用 `ordered-cloud-pair` alias，row-source policy（行来源策略）仍然是 ordered-cloud-pair。

## 当前可审查证据

以下 summary-only（只提交摘要）证据已登记。当前 `.gitignore` 只自然暴露部分 summary / doctor；registry、manifest 和 raw logs 作为 ignored-local 证据路径，提交阶段若要保留需单独 allowlist / force-add 并审查。没有用户明确授权时不提交 raw board logs。

| 路径 | 证据角色 |
| --- | --- |
| `test-rvv/registration/transformation_estimation_svd/log/qemu/run_test_std.log` | QEMU Std correctness。 |
| `test-rvv/registration/transformation_estimation_svd/log/qemu/run_test_rvv.log` | QEMU RVV correctness。 |
| `test-rvv/registration/transformation_estimation_svd/log/qemu/run_bench_std.log` | QEMU Std bench smoke raw log；只证明日志形状。 |
| `test-rvv/registration/transformation_estimation_svd/log/qemu/run_bench_rvv.log` | QEMU RVV bench smoke raw log；只证明日志形状。 |
| `test-rvv/registration/transformation_estimation_svd/log/qemu/analyze_bench_compare.log` | QEMU bench smoke summary；不作性能结论。 |
| `test-rvv/registration/transformation_estimation_svd/log/qemu/evidence_manifest.json` | Evidence Doctor manifest（证据体检输入）。 |
| `test-rvv/registration/transformation_estimation_svd/log/qemu/evidence_doctor.md` | Evidence Doctor 报告，Errors=0、Warnings=0、Suggestions=0。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/summary.md` | Phase 010 board repeated summary；same-boundary fused Std/RVV median 为 `3.081x` / `3.179x` / `3.157x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_manifest.json` | Phase 010 board manifest；默认 ignored-local，供 Evidence Doctor 和 registry 复核。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/fused_full_cloud_repeated/evidence_doctor.md` | Phase 010 board Evidence Doctor，Errors=0、Warnings=4、Suggestions=0；Warning 已在 phase result 中解释。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/summary.md` | Phase 020 production direct board summary；public Std/RVV median 为 4K `14.372x`、64K `24.471x`、256K `23.841x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/evidence_manifest.json` | Phase 020 production direct manifest；默认 ignored-local，供 Evidence Doctor 和 registry 复核。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_ordered_cloud_pair_repeated/evidence_doctor.md` | Phase 020 production direct Evidence Doctor，Errors=0、Warnings=1、Suggestions=0。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/source_indexed_cloud_pair_repeated/summary.md` | Phase 030 source-indexed board diagnostic；same-boundary median 为 `1.917x` / `1.843x` / `1.785x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/source_indexed_cloud_pair_repeated/evidence_manifest.json` | Phase 030 source-indexed manifest；默认 ignored-local，供 Evidence Doctor 和 registry 复核。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/source_indexed_cloud_pair_repeated/evidence_doctor.md` | Phase 030 source-indexed Evidence Doctor，Errors=0、Warnings=4、Suggestions=0。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/summary.md` | Phase 040 source-indexed production direct board summary；public Std/RVV median 为 4K `9.634x`、64K `12.217x`、256K `11.558x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/evidence_manifest.json` | Phase 040 source-indexed production direct manifest；默认 ignored-local，供 Evidence Doctor 和 registry 复核。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_source_indexed_cloud_pair_repeated/evidence_doctor.md` | Phase 040 source-indexed production direct Evidence Doctor，Errors=0、Warnings=0、Suggestions=0。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/dual_indices_cloud_pair_repeated/summary.md` | Phase 050 dual-indices board diagnostic summary；same-boundary fused Std/RVV median 为 `1.801x` / `1.683x` / `1.425x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/correspondence_pair_repeated/summary.md` | Phase 050 correspondence board diagnostic summary；same-boundary fused Std/RVV median 为 `2.174x` / `1.905x` / `1.772x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/summary.md` | Phase 060 dual-indices production direct board summary；public Std/RVV median 为 4K `6.805x`、64K `6.404x`、256K `5.964x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/summary.md` | Phase 060 correspondence production direct board summary；public Std/RVV median 为 4K `8.649x`、64K `8.644x`、256K `7.872x`。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/evidence_manifest.json` | Phase 060 dual-indices production direct manifest；默认 ignored-local，供 Evidence Doctor 和 registry 复核。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/evidence_manifest.json` | Phase 060 correspondence production direct manifest；默认 ignored-local，供 Evidence Doctor 和 registry 复核。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_dual_indices_cloud_pair_repeated/evidence_doctor.md` | Phase 060 dual-indices production direct Evidence Doctor，Errors=0、Warnings=1、Suggestions=0。 |
| `test-rvv/registration/transformation_estimation_svd/log/board/production_correspondence_pair_repeated/evidence_doctor.md` | Phase 060 correspondence production direct Evidence Doctor，Errors=0、Warnings=1、Suggestions=0。 |
| `test-rvv/registration/transformation_estimation_svd/log/evidence_registry.json` | evidence registry（证据登记表）。 |

## 默认不提交的生成产物

`build/`、未被文档引用的 `log/qemu/*.log`、raw board logs、本机 `config.mk`、私有板卡地址和临时命令输出默认不提交。

## doc-rvv 适用性

`doc-rvv/registration/transformation_estimation_svd-RVV.zh.md` 当前适用，因为已有 production patch、production direct tests（真实生产路径测试）和 PI5 production evidence（生产证据闭环）。长期文档只承载已采用的 ordered-cloud-pair、source-indexed-cloud-pair、dual-indices-cloud-pair 与 correspondence-pair 生产行为；逐点型性能和 `Scalar=double` 仍保持代表性边界和 fallback 说明。
