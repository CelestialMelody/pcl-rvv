# Phase 010 结果：production-shaped compressed writer diagnostic

## 实际执行范围

本阶段按 `plan.zh.md` 验证 pack + LZF compression（LZF 压缩）生产形态诊断。
阶段仍只修改 `test-rvv/io/pcd_io_templated_writer` 下的测试资产、bench、脚本和文档；
没有修改 `io/include/pcl/io/impl/pcd_io.hpp` production（生产源码）。

validated_scope（已验证范围）：

- entry shape（入口形态）：`PCDWriter::writeBinaryCompressed<PointT>` 的 `PointCloud<PointT>`
  compressed writer 中 `point-major -> field-major -> lzfCompress` 的主要前置链路。
- point type / layout：synthetic PointXYZRGB-like 4 字节字段，覆盖 `point_step=16` 和
  `point_step=20` tail-padding（尾部填充）两种布局，以及 512 点小规模 smoke。
- evidence role（证据角色）：production-shaped diagnostic（生产形态诊断），不是
  production direct（真实生产路径证据）。

unvalidated_scope（未验证范围）：

- 真实 `PCDWriter` public entry（公开入口）分流、fallback（回退路径）和 production 符号归属。
- file lock、header 生成、raw_fallocate、mmap、write、error handling 和 `_WIN32` 分支。
- 泛型 `PointT` traits（点类型字段特征）、非 4 字节字段、`indices` overload、ASCII writer。

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| B1 写 failing shaped correctness test | done | `src/test_pcdtw.cpp` 中 `PCDTemplatedWriterCompressedDiagnostic.PackAndCompressCandidateMatchesScalarPayload`；历史 red-first 为缺少 helper 编译失败 | 测试能约束 pack+compress helper。 |
| B2 实现 test-only pack+compress helper | done | `include/impl/pcdtw_support.hpp` 的 `packAndCompressScalar` / `packAndCompressCandidate` | RVV build 在 4 字节字段布局下先命中 `PackRvv`，再交给 LZF。 |
| B3 扩展 bench case | done | `src/bench_pcdtw.cpp` 新增 `compressed_*` case；`--case-filter compressed_*` | 计时边界包含 pack + LZF，不包含 checksum、header、mmap 或 file write。 |
| B4 QEMU smoke | done | `make -C test-rvv/io/pcd_io_templated_writer run_qemu_smoke BENCH_ARGS="--case-filter compressed_* --iterations 2 --warmup-iterations 1"` | 4 个 gtest 通过；QEMU bench 只证明日志形状和可运行，不作为性能结论。 |
| B5 板卡 repeated shaped bench | done | `make -C test-rvv/io/pcd_io_templated_writer run_board_pcdtw_shaped_repeated`；summary: `log/board/production_shaped_repeat_5/summary.md` | 5 次 repeated board run，三个 compressed case 全部 positive。 |
| B6 Evidence Doctor 和 registry | done | `log/board/production_shaped_repeat_5/evidence_doctor.md`；`log/evidence_registry.json` | Doctor: Errors=0, Warnings=0, Suggestions=0；registry fresh。 |
| B7 文档回填 | done | 本文件、evaluation、roadmap、matrix、phase README | 当前结论升级为 `partial-production-candidate`，下一 phase 是 PI1 生产接入计划。 |

## 诊断证据链

correctness（正确性）：

- `make -C test-rvv/io/pcd_io_templated_writer run_test_compare` 通过。Std/RVV build 都运行
  4 个测试，其中 pack+compress payload 对拍一致。
- RVV build 的可覆盖布局命中 `PathKind::PackRvv`；非覆盖 mixed-field case 仍走
  `PackScalarFallback`。

QEMU path/log-shape（QEMU 路径 / 日志形状）：

- `run_qemu_smoke` 使用 `--case-filter compressed_* --iterations 2 --warmup-iterations 1`。
- QEMU 输出只用于证明 compressed case 可执行、日志可解析和 RVV binary 可运行，不写性能结论。

asm attribution（反汇编归属）：

- `make -C test-rvv/io/pcd_io_templated_writer dump_bench_rvv` 生成
  `build/asm/riscv/bench_pcdtw_rvv.asm`。
- 该文件可见 `vlse32.v` / `vse32.v`，归属到 bench 中内联的 `packFieldsCandidate` hot path。
  LZF 本身仍是标量库调用。

board performance（板卡性能）：

`log/board/production_shaped_repeat_5/summary.md`：

| case | mean speedup | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `compressed_pointxyzrgb_4f_262k` | `1.3904x` | `1.3928x` | `1.3727x` | `1.4054x` | positive |
| `compressed_pointxyzrgb_4f_padding_262k` | `1.3526x` | `1.3532x` | `1.3383x` | `1.3639x` | positive |
| `compressed_pointxyzrgb_4f_small_512` | `1.3493x` | `1.3488x` | `1.3024x` | `1.3857x` | positive |

rerun budget（复跑预算）：

- 计划预算为 5 次 repeated board run，`--iterations 20 --warmup-iterations 3`。
- 三个 case 的 median 都大于 `1.08x` positive 阈值，且没有 checksum mismatch 或 Doctor finding。
- decision bucket 稳定，不需要追加复跑。

Evidence Doctor（证据体检）：

- `log/board/production_shaped_repeat_5/evidence_doctor.md`：
  Errors=0，Warnings=0，Suggestions=0。

evidence registry（证据登记表）：

- `log/evidence_registry.json` 已登记 `run_board_pcdtw_shaped_repeated`，case filter 为
  `compressed_*`，evidence_role 为 `production_shaped_diagnostic`，freshness_state 为 `fresh`。

## Diagnostic 到 production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | production-shaped diagnostic（生产形态诊断） |
| A/B boundary | test helper / bench wrapper，包含 pack + LZF，不含 public file write |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 bounded production probe（有界生产探针） |
| diagnostic 是否可外推到 production | partial yes：可支持 PI1 计划；不能替代 production direct。 |
| comparison-boundary / baseline mismatch 风险 | yes：没有真实 file lock、header、mmap、write 和 error path；LZF 输入来自 test-only synthetic cloud。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮不是弱 / 负 / 中性 / 不稳定；若后续 production direct 退化，应停止在 PI5 用户检查点。 |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | yes：本阶段没有 production boundary 内 RVV-vs-RVV detail A/B，也没有真实 public entry 证据。 |

## EvidenceDecision

当前阶段结论是 `partial-production-candidate`：

- pack-only Phase 000 与 pack+LZF Phase 010 都是 board positive，说明 4 字节字段布局转换的 RVV
  收益没有被 LZF 完全稀释。
- 该结论只覆盖 synthetic PointXYZRGB-like 4 字节字段、连续 `PointCloud<PointT>` rows 和
  compressed writer 前置链路。
- 当前没有修改 production；也不能写成 clean adopted 或 production-ready。

production decision（生产判断）：

- 建议进入 `020-PI1-production-integration-plan`，冻结窄范围生产接入计划。
- PI1 之前和 PI1 期间不得直接改 production；PI1 gate 必须写清 helper shape、fallback 矩阵、
  public entry direct test、asm 和 board production evidence 计划。

## 阶段反思和新增路线

- `compressed_*` 三个 case 的收益稳定在 `1.35x` 左右，说明 LZF 不是唯一主成本；前置布局转换仍足够重。
- 小规模 512 点 case 也为 positive，但真实 production 是否需要小规模 RVV gate 仍要在 PI1 中保守处理，
  避免 public entry 为微小输入增加维护风险。
- 下一阶段应优先解决 production helper 形态，而不是继续扩大 diagnostic case 数量。

## Continue / Stop Decision

continue_stop_decision：继续到 PI1 计划，但不修改 production。

stop_condition_hit：none for topic-local docs/tests；production edit 需要 PI1 gate 和用户明确生产接入授权。

next_phase_default：`020-PI1-production-integration-plan`。
