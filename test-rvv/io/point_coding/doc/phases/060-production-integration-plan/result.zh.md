# Phase 060 Production Integration Result

## 当前状态

Phase 060 已完成 PI1-PI5，并在用户确认后进入 production closeout（生产收尾）。当前 production patch（生产补丁）保留在工作区；证据支持并已采纳
`PointCoding<PointXYZ>::decodePoints` 的 decode-only RVV path，当前状态是 adopted production behavior（已采纳生产行为）。

PI5 回填：用户确认接入后板卡测试有收益即可采纳。本阶段已创建正式 `doc-rvv/io/point_coding-RVV.zh.md`，且数据采用接入后的 Phase 060 production-direct 板卡结果。

## 实际执行范围

| dimension | actual scope |
| --- | --- |
| production entry | `io/include/pcl/compression/point_coding.h` 的 `PointCoding<PointXYZ>::decodePoints` |
| RVV gate | `__RVV10__` 且 exact `pcl::PointXYZ` |
| fallback | 非 RVV build 和非 exact `PointXYZ` 模板实例走 `decodePointsStd` |
| arithmetic | diff byte 扩展后使用 f64 vector arithmetic（双精度向量运算），最后窄化到 float，保持 production double reference 舍入语义 |
| performance boundary | production-direct bench `decode_production_direct_256/1024/4096/16384` |

## 生产 diff 摘要

| file | change |
| --- | --- |
| `io/include/pcl/compression/point_coding.h` | 增加 `__RVV10__` guarded RVV helper；把原标量循环抽成 `decodePointsStd`；`decodePoints` 对 `PointXYZ` exact gate 短路到 `decodePointsRVV`。 |
| `test-rvv/io/point_coding/src/test_point_coding.cpp` | 增加 production-direct gtest、double reference rounding 对抗样本和非 `PointXYZ` fallback 语义测试。 |
| `test-rvv/io/point_coding/src/bench_point_coding.cpp` | 增加 `decode_production_direct_*` bench case，Std/RVV 都调用真实 production `PointCoding<PointXYZ>::decodePoints`。 |
| `test-rvv/io/point_coding/script/generate_point_coding_evidence_manifest.py` | 标注 production-direct case 的 `evidence_role=production_direct`。 |
| `test-rvv/io/point_coding/Makefile` | 允许覆盖 QEMU bench smoke 参数，便于本阶段只跑 production-direct 日志形状。 |

## Correctness / QEMU / ASM

| evidence | command | result |
| --- | --- | --- |
| correctness | `make run_test_compare` | Std / RVV 各 9 个 gtest 通过。 |
| production rounding guard | `DecodePointXYZKeepsDoubleReferenceRounding` | 覆盖 f32 quick path 会出错的 double reference 舍入样本；当前 f64 RVV path 通过。 |
| fallback guard | `DecodeNonPointXYZFallsBackToScalarSemantics` | RVV build 下 `PointXYZI` 保持标量语义，`intensity` 不被改写。 |
| QEMU bench smoke | `make run_qemu_bench_smoke POINT_CODING_QEMU_BENCH_SMOKE_ARGS='--iterations 1 --warmup-iterations 0 --case-filter decode_production_direct_1024'` | RVV binary 可运行；QEMU timing 不作为性能证据。 |
| asm | `make dump_bench_rvv` + grep | RVV asm 包含 production decode 所需 `vlse8.v`、`vfwcvt.f.xu.v`、`vfncvt.f.f.w`、`vsse32.v`。 |

## Board repeated 结果

命令：

```bash
make collect_board_repeated run_board_repeated_evidence_doctor \
  POINT_CODING_REPEATED_RUNS=10 \
  POINT_CODING_REPEATED_DIR=log/board/repeated_phase060_production_decode \
  BENCH_ARGS='--iterations 20 --warmup-iterations 3 --case-filter decode_production_direct_*'
```

摘要路径：

- `log/board/repeated_phase060_production_decode/summary.md`
- `log/board/repeated_phase060_production_decode/evidence_doctor.md`
- `log/board/repeated_phase060_production_decode/evidence_manifest.json`

| case | runs | median | min | max | decision bucket |
| --- | ---: | ---: | ---: | ---: | --- |
| `decode_production_direct_256` | 10 | 1.22x | 1.22x | 1.24x | positive |
| `decode_production_direct_1024` | 10 | 1.23x | 1.14x | 1.49x | positive with long-tail warning |
| `decode_production_direct_4096` | 10 | 1.18x | 1.09x | 1.25x | weak-positive / positive |
| `decode_production_direct_16384` | 10 | 1.16x | 1.10x | 1.19x | weak-positive / positive |

所有 case 的 checksum 在 Std/RVV 间一致，且 10 轮中未出现 speedup < 1.0x。

## Evidence Doctor

Evidence Doctor 结果：`Errors=0，Warnings=1，Suggestions=0`。

唯一 warning 是 `decode_production_direct_1024` 的 `long_tail_or_variance`：min=1.14x、median=1.23x、max=1.49x。处理方式：保留 min/median/max，不剔除高侧异常；由于 min 仍大于 1.0x 且其它规模也为正向，本阶段不降级为 unstable。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | production_direct；旧 Phase 040/050/055 只作为进入本 probe 的理由。 |
| A/B boundary | Std/RVV 两侧都调用真实 `PointCoding<PointXYZ>::decodePoints` production entry。 |
| 当前决策问题 | 当前 RVV production decode path 是否快于当前 scalar production decode path。 |
| diagnostic 是否可外推到 production | 旧 diagnostic 不外推；本阶段用同 production boundary 板卡数据重新判断。 |
| comparison-boundary / baseline mismatch 风险 | 已通过 production-direct case 降低；完整 public octree end-to-end 后续已由 Phase 080 单独审计，结果为 weak / near-threshold。 |
| bounded production probe 条件 | 已满足：correctness、fallback、asm、board repeated 和 doctor 均闭合。 |
| clean adoption 是否需要 RVV-vs-RVV detail A/B | 当前 production 没有既有 RVV family；不需要同边界 RVV-vs-RVV。 |

## 阶段决策

`continue_stop_decision`: adopted_production_closeout_completed。

`stop_condition_hit`: none for exact `PointXYZ` decode closeout。PI5 用户确认已完成，production patch 进入 adopted production behavior。

`next_phase_default`:

- 若继续当前 topic：规划 `070-pointxyz-like-traits-expansion`，把 exact gate 扩到 traits-gated xyz AoS 点型前，重新补 correctness、fallback、asm、board repeated 和 Evidence Doctor。
- 若不继续扩点型：当前 exact `PointXYZ` decode production closeout 可停在 adopted 状态；encode 保持未覆盖。完整 public end-to-end 后续已由 Phase 080 审计，未形成稳定 public-positive 证据。

## 未覆盖范围

- 泛型 `PointT` RVV gate 未完成；exact `PointXYZ` 不能写成泛型结论。
- 完整 `OctreePointCloudCompression` public end-to-end timing 后续已由 Phase 080 测量，结果为 weak / near-threshold。
- encode path 仍不接 production。
- 生产长期主题文档已创建：`doc-rvv/io/point_coding-RVV.zh.md`。
