# 030 production-patch-and-direct-evidence 结果

## 当前结论

本阶段完成 PI2-PI5 production integration loop（生产接入闭环）的候选补丁、真实公开入口测试、production-only asm（只看生产二进制的反汇编）和 5-run 板卡 repeated bench（重复性能测试）。生产公开入口 `ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices&)` 在 `__RVV10__` 构建中先尝试 RVV helper，失败时回到 `extractStd` 标量路径；非 RVV 构建只保留标量路径。

PI5 EvidenceDecision（生产证据决策）已更新为 `adopted production behavior`：板卡 production public（真实公开入口）证据为 positive，用户确认“板卡上的测试结果如果显示有收益即可采纳”，因此当前 production patch（生产补丁）按已采用生产行为保留。提交 commit 仍需单独授权。

## 计划动作回填

| action | status | 证据 | 说明 |
| --- | --- | --- | --- |
| 写 production-only RED gate | done | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter check_production_rvv_asm` 在补丁前因缺少 `apmfExtractRVV` 失败 | 证明 asm gate 能捕捉生产 RVV helper 缺失。 |
| 补 public entry correctness | done | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | Std / RVV 共 9 个 gtest 通过，覆盖 dense、non-dense、小规模 fallback 和 `PointXYZI` 公开入口对拍。 |
| PI2 production patch | done | `segmentation/include/pcl/segmentation/approximate_progressive_morphological_filter.h`、`segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` | `extract` 拆成 init/deinit + RVV short-circuit + `extractStd` fallback；RVV helper 只接管 grid z-min 和 tail threshold，window-open 保持标量。 |
| PI3/PI4 本地证据 | done | `check_production_rvv_asm`、显式实例化编译、QEMU 小规模 smoke | 证明生产 RVV symbol 和关键 RVV 指令存在；QEMU 只作为 correctness / log-shape（日志形状）证据。 |
| PI4 板卡证据 | done | `log/board/production-public-repeated-v2/summary.md` | `PointXYZ` dense median 1.60x，non-dense median 1.35x，均为 positive。 |
| PI5 决策 | adopted | `log/board/production-public-repeated-v2/evidence_doctor.md` | Evidence Doctor：Errors=0 / Warnings=0 / Suggestions=0；用户已确认采纳。 |

## 当前生产补丁范围

| 维度 | 当前状态 | 证据 / 边界 |
| --- | --- | --- |
| public entry（公开入口） | `extract(Indices&)` 真实命中 RVV short-circuit | production bench 使用 `bench_apmf_production.cpp` 直接构造 filter 并调用 `extract`。 |
| RVV 覆盖片段 | grid z-min 和每轮 height threshold tail | `apmfExtractRVV` 调用 `computeGridZMinRVV` 与 `thresholdGroundRVV`。 |
| 保留标量片段 | window min/max open、window size / threshold 计算、非 RVV fallback | window-open 单独组件板卡证据曾为中性 / 负向，本阶段不接入。 |
| 点类型 / layout | `RVVXYZAoSFloatLayout<PointT>` traits gate（字段特征和 AoS 布局准入） | 030 correctness 覆盖 `PointXYZ` 与 `PointXYZI`；板卡性能只覆盖 `PointXYZ`。040 已启动 `PointXYZI/RGB/RGBA` 扩展。 |
| 规模 gate | `input_->size() >= 64`，且 32-bit byte offset 可表达 | 小规模公开入口测试覆盖 fallback；超界路径按源码 gate 回到标量。 |
| dense / non-dense | dense 不做有限值过滤；non-dense grid z-min 和初始 ground 过滤无效点 | public test 和 board bench 均覆盖 dense / non-dense。 |

## 证据链

| 证据类别 | 命令 / 路径 | 结果 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness（正确性） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_rvv` | RVV 构建 9 个测试通过；040 后扩展为 14 个测试通过 | 不证明非 RVV 构建；不证明所有 PCL_XYZ_POINT_TYPES 的板卡性能。 |
| Std/RVV 对拍 | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` | 030 时 Std / RVV 两个构建均通过 9 个测试；040 后 Std / RVV 均通过 14 个测试 | 不证明生产性能。 |
| QEMU smoke | `ALLOW_QEMU_BENCH_COMPARE=1 BENCH_ARGS='--size 4096 --half 2 --iterations 2 --warmup 1' make -C ... run_bench_compare SRCS_BENCH=src/bench_apmf_production.cpp ...` | checksum 一致，production public label 可解析 | QEMU timing 不作为性能结论。 |
| asm attribution（反汇编归属） | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter check_production_rvv_asm` | production-only 二进制包含 `apmfExtractRVV` 和 `vlsseg3e32.v`、`vfcvt.rtz.x.f.v`、`vcompress.vm`、`vcpop.m`、`vluxseg3ei32.v`、`vlse32.v` | 不说明每个点类型都有同等收益。 |
| board performance（板卡性能） | `log/board/production-public-repeated-v2/summary.md` | dense：median 1.60x，min 1.59x，max 1.67x；non-dense：median 1.35x，min 1.35x，max 1.39x | 只覆盖单线程 `PointXYZ` synthetic production public case。 |
| Evidence Doctor（证据体检） | `log/board/production-public-repeated-v2/evidence_doctor.md` | Errors=0 / Warnings=0 / Suggestions=0 | 环境字段 governor/freq/temperature/VLEN 未记录；当前脚本未把该缺失升为 finding。 |

## Evidence Doctor 结果

运行命令：

```bash
make -C test-rvv/segmentation/approximate_progressive_morphological_filter \
  OUTPUT_DIR_BOARD=log/board/production-public-repeated-v2 \
  APMF_REPEATED_DIR=log/board/production-public-repeated-v2 \
  APMF_REPEATED_SUMMARY=log/board/production-public-repeated-v2/summary.md \
  APMF_EVIDENCE_MANIFEST=log/board/production-public-repeated-v2/evidence_manifest.json \
  APMF_EVIDENCE_DOCTOR_MD=log/board/production-public-repeated-v2/evidence_doctor.md \
  APMF_EVIDENCE_INCLUDE_REGEX='production public' \
  run_board_evidence_doctor
```

结果为 Errors=0 / Warnings=0 / Suggestions=0。manifest 的 `evidence_role` 为 `production-public`，`boundary` 为 `public_overload`，`point_type` 为 `PointXYZ`，run count 为 5，iterations 为 8，warmup iterations 为 2。governor、freq、temperature 和 VLEN 未记录；当前脚本规则未把这些字段缺失升为 warning，文档仍把环境字段缺失作为可复核边界保留。

## 决策和停止条件

当前没有性能负向、asm 归属失败或 Evidence Doctor Error。按本阶段 bucket，production public 结果为 positive，已按用户确认采纳 production patch。

本阶段不再停在 PI5 用户确认点。下一步进入 `040-point-type-production-expansion`：保持当前 production 实现，补 `PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 的 production public correctness、bench label、board repeated 和 Evidence Doctor。若 040 板卡访问不可用，应以板卡访问问题作为真实停止条件，而不是回退 030 的 adopted 判断。

## 后续优化判断

`hybrid-full-diagnostic` 当前不建议继续：production public 已正向，且生产补丁已经采用“grid z-min + tail threshold RVV、window-open 标量”的 hybrid 形态；继续做 window-open RVV A/B 不会改变当前保留判断。

仍可后续扩展的方向是 point type expansion（点类型扩展）：当前 production gate 是 traits-gated 泛型，但 030 板卡性能只覆盖 `PointXYZ`，correctness 另覆盖 `PointXYZI`。040 已创建计划，先扩 `PointXYZI`、`PointXYZRGB/RGBA` 的 production public correctness / bench / asm / Evidence Doctor；在完成前，不能把 `PointXYZ` 板卡收益外推成所有 xyz 点型的性能结论。

## 文档同步

- 阶段索引、优化矩阵、路线图和 evaluation 已改为 adopted production behavior 状态。
- segmentation function evaluation queue 已把本 topic 从“PI5 待用户确认采纳”更新为“已采纳；040 点型扩展受板卡访问阻塞”。
- `doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md` 已刷新为正式 adopted production 文档，数据采用接入后的 `production-public-repeated-v2` 板卡结果。

## doc_suite_role_inventory

| role | status | 路径 / 说明 |
| --- | --- | --- |
| topic_navigation | standalone | `README.zh.md` 保存当前状态、阅读路径、常用命令和证据白名单。 |
| testing_overview | merged | `doc/approximate_progressive_morphological_filter-evaluation.zh.md#当前证据链` 和本文“证据链”说明 correctness、QEMU、asm、board 和 doctor 边界。 |
| correctness_tests | merged | `src/test_apmf.cpp` 的 TEST 名称和 evaluation 证据表共同说明 component/full/public/fallback 覆盖。 |
| benchmark_and_evidence | merged | `src/bench_apmf.cpp`、`src/bench_apmf_production.cpp`、`script/generate_apmf_board_evidence_manifest.py`、本文 Evidence Doctor 小节和 README 证据白名单覆盖 bench / output 合同。 |
| optimization_evidence | standalone | `doc/phases/optimization-matrix.zh.md` 保存 candidate family 到 evidence / decision 的矩阵。 |
| optimization_roadmap | standalone | `doc/optimization-roadmap.zh.md` 保存后续 candidate frontier（候选前沿）和恢复条件。 |
| test_support_code_map | merged | evaluation 与 `doc-rvv` 的 Traceability Map 覆盖 production helper、test、bench、script、output 和 phase 文档定位。 |
| phase_index / phase_plan / phase_result | standalone | `doc/phases/README.zh.md`、本阶段 plan/result 和 optimization matrix。 |
| evaluation_production | standalone | `doc/approximate_progressive_morphological_filter-evaluation.zh.md` 已同步 adopted 证据链、040 本地扩展和未覆盖范围。 |
| production_topic_doc | adopted | `doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md` 已刷新为已采用生产文档。 |

当前未拆独立 `testing-overview.zh.md`、`correctness-tests.zh.md`、`benchmark-and-evidence.zh.md` 和 `test-support-code-map.zh.md`。这些职责已在 evaluation、README、phase result 和 `doc-rvv` 的 Traceability Map 中可定位；若后续要求提交前进一步提高审查颗粒度，可另开 structure-parity doc-suite phase 拆出独立 role 文档。
