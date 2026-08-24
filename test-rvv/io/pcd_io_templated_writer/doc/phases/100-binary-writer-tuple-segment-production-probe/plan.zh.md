# Phase 100: binary writer tuple / segment production probe 计划

## 阶段意图和边界

本阶段进入有界 production integration probe（生产接入探针）。目标是把 Phase 090 的 tuple / segment output family 接入真实 `PCDWriter::writeBinary<PointT>(file_name, cloud)` public overload（公开入口），然后用接入后的 production-public（真实公开入口）板卡数据判断是否值得保留。

本阶段不扩大到 indexed overload（索引入口）`writeBinary(file_name, cloud, indices)`，也不扩大到任意字段数量、非 4 字节字段或 offset 不连续布局。若生产证据为负或 Evidence Doctor 出现阻断 Error，按用户授权可以回滚该 binary writer production probe；compressed writer 已采纳路径不回滚。

## 当前状态清单

| area | current state |
| --- | --- |
| compressed writer | adopted production behavior；保留 `writeBinaryCompressed<PointT>` 的 4-byte field RVV pack path。 |
| binary writer Phase 080 | field-outer production patch 已负收益回滚；历史 board mean `0.9842x / 0.9727x / 0.9810x`，Doctor Errors=3。 |
| binary writer Phase 090 | diagnostic positive；compact mean `9.8247x`，padding mean `4.0905x`，Doctor `Errors=0 Warnings=2 Suggestions=0`。 |
| current binary production | `writeBinary<PointT>(file_name, cloud)` 仍是原 point × field `memcpy` 标量循环。 |

## 候选实现族

| candidate | production gate | expected benefit | fallback |
| --- | --- | --- | --- |
| compact memcpy fast path | `fields.size()==4`，每个有效字段 size=4，offset 为 `0/4/8/12`，`sizeof(PointT)==16`，输入和输出 4-byte aligned | 将 packed binary payload 降成一趟连续 `memcpy` | gate 不满足时调用 `packBinaryFieldsStd` |
| padding segment RVV path | 同上，但 `sizeof(PointT)>16` 且 stride 为 4-byte multiple | 用 `vlse32.v` 读取 4 个字段，再用 `vsseg4e32.v` 连续写 packed output | gate 不满足时调用 `packBinaryFieldsStd` |

## 优化矩阵

| candidate family | row source | point type / Scalar / layout | correctness / fallback | bench / board | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| binary tuple / segment production probe | contiguous rows | registered 4-field 4-byte layout；compact 16B / padding 20B | production direct byte-equal；hook 命中 memcpy / RVV；mixed fallback；indices unchanged | 新增 `production_binary_tuple_*` QEMU smoke 和 board repeated | compact 可无 RVV；padding 需 public writer 符号附近 `vlse32.v` + `vsseg4e32.v` | production-public Doctor `Errors=0` 才能进入 PI5 positive | pending |

## 实现和测试动作

1. 在 `io/include/pcl/io/impl/pcd_io.hpp` 中新增 binary writer data-copy helper：
   - `packBinaryFieldsStd` 保存原 point × field `memcpy` 语义；
   - `packBinaryFieldsTupleRVV` 只在 `__RVV10__ && __riscv_vector` 下存在；
   - test hook（测试钩子）记录 scalar / memcpy / RVV 路径，用于 production direct gtest。
2. 修改 `writeBinary<PointT>(file_name, cloud)` 的 data copy 段：RVV build 先尝试 tuple helper，失败自然落回 `packBinaryFieldsStd`。
3. 在 `src/test_pcdtw.cpp` 增加真实 public entry binary payload 对拍、mixed fallback 和 indices unchanged 测试。
4. 在 `src/bench_pcdtw.cpp` 增加 `production_binary_tuple_*` public writer bench case，计时边界包含 header、mmap/write 和文件 checksum 之外的 timed writer 调用。
5. 在 Makefile / manifest generator 中新增 production binary tuple repeated target、manifest role 和 registry doc refs。
6. 执行：
   - `make -C test-rvv/io/pcd_io_templated_writer run_test_compare`
   - `make -C test-rvv/io/pcd_io_templated_writer run_bench_rvv BENCH_ARGS="--case-filter production_binary_tuple_* --iterations 2 --warmup-iterations 1"`
   - `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv`
   - `make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_production_binary_tuple_repeated`

## Evidence Doctor 和 registry

新增 board summary 必须放在 `log/board/production_binary_tuple_repeat_5/`，manifest role 为 `production_public`，case kind / wrapper / timer boundary 必须显示它是真实 `PCDWriter::writeBinary<PointT>` public overload。Doctor 若出现 checksum mismatch、strict A/B 缺口或 degradation frequency Error，禁止采纳并回滚或暂停。

## 板卡复跑预算和决策桶

- run count：5。
- iterations：20。
- warmup：3。
- positive：大规模 compact 和 padding mean / median 均高于 `1.05x`，且 Doctor 无阻断 Error。
- weak-positive：任一大规模 case 在 `1.00x` 到 `1.05x`，或只有 small positive；进入用户检查点但不建议 clean adoption。
- negative：任一大规模 mean 或 median 低于 1，或 Doctor Error 阻止采纳；可回滚 binary probe。
- unstable：预算用完后方向仍摇摆；降级为人工判断，不自动采纳。

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 100 将生成 `production-public` evidence；Phase 090 只作为前置信号。 |
| A/B boundary | public overload：`PCDWriter::writeBinary<PointT>(file_name, cloud)`。 |
| 当前决策问题 | 当前 binary writer tuple / segment production probe 是否快于当前 scalar public writer。 |
| diagnostic 是否可外推到 production | 否；必须以 Phase 100 接入后的 board summary 为准。 |
| comparison-boundary / baseline mismatch 风险 | 通过同一 case 的 Std / RVV 两个 build、相同 wrapper 和 checksum policy 降低风险。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段已经是 bounded probe；若 production-public 证据不成立，则回滚或停在 PI5 用户检查点。 |
| clean adoption 是否需要同一 production boundary 证据 | 需要，并且 PI5 后仍需用户确认保留 / 采纳。 |

## 文档更新清单

本阶段完成后更新 `result.zh.md`、optimization matrix、roadmap、evaluation、benchmark/evidence、optimization evidence、correctness tests、testing overview、test-support code map、current Handoff。只有 PI5 后用户确认采纳，才用接入后的 production-public 板卡数据刷新正式 `doc-rvv/io/pcd_io_templated_writer-RVV.zh.md` 中的 binary writer adopted 状态；若证据不支持则记录 rollback/no-production。
