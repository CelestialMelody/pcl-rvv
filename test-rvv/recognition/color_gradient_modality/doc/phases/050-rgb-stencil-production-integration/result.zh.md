# Phase 050: RGB Sobel stencil production integration result

## 当前状态

本阶段完成 Phase 040 `full-sobel-rgb-stencil-rvv` 的 production integration loop
（生产接入闭环）。`recognition/include/pcl/recognition/color_gradient_modality.h`
中的 `computeColorGradientPipelineRVV()` 已改为在 RVV 分块内直接从 Gaussian 后的
`pcl::RGB` buffer 做 RGB Sobel stencil（邻域模板）计算，并继续写回 production
所需的 `GradientXY`、`quantized_color_gradients_` 和
`filtered_quantized_color_gradients_`。

接入后 production direct（真实生产路径）板卡重复测试为 positive；按用户确认的
“板卡上的测试结果如果显示有收益即可采纳”，当前决策为 `adopted production behavior`
（已采纳生产行为）。

## 实际修改范围

| 文件 | 修改 | 证据角色 |
| --- | --- | --- |
| `recognition/include/pcl/recognition/color_gradient_modality.h` | 去掉整图 `selected_dx/selected_dy/selected_sqr_mag` float staging；在 `__RVV10__` helper 内用 `vlse8` 跨步读取 `pcl::RGB` 的 B/G/R 字节，widen 到 int32 后计算三通道 Sobel、最大通道选择、sqrt / atan2 / quantize 和 dominant filter | production patch |
| `test-rvv/recognition/color_gradient_modality/Makefile` | 收紧 `check_cgm_production_rvv_asm`，要求 production helper 反汇编出现 `vlse8`、widen / integer Sobel、`vfsqrt`、`atan2_RVV` 和 byte filter 指令 | asm gate |
| `test-rvv/recognition/color_gradient_modality/script/generate_cgm_evidence_manifest.py` | 让 `production_process_*` summary 根据 run label 区分 Phase 030 / Phase 050，并为 Phase 050 写 RGB stencil production reduction 描述 | evidence metadata |

## Correctness / QEMU / ASM

| evidence | result | boundary |
| --- | --- | --- |
| correctness | `make -C test-rvv/recognition/color_gradient_modality run_test_compare` 通过，Std/RVV 均 8/8 | QEMU correctness；含 diagnostic、public entry path-hit、forced scalar/RVV 同进程对拍 |
| QEMU smoke | `make -C test-rvv/recognition/color_gradient_modality run_bench_rvv BENCH_ARGS="--case-filter production_process_320x240,production_process_641x481_tail --iterations 1 --warmup-iterations 1"` 通过 | 日志形状和入口可运行；QEMU timing 不作性能结论 |
| production asm | `make -C test-rvv/recognition/color_gradient_modality check_cgm_production_rvv_asm` 通过 | production helper 命中 RGB stencil RVV 指令 |
| stencil asm | `make -C test-rvv/recognition/color_gradient_modality check_cgm_stencil_rvv_asm` 通过 | test helper stencil gate 仍成立 |

## Board repeated 结果

复现命令：

```bash
make -C test-rvv/recognition/color_gradient_modality board_repeated record_evidence_state_repeated \
  REPEATED_BOARD_TAG=phase050_rgb_stencil_production_direct \
  REPEATED_BOARD_REMOTE_TAG=phase050_rgb_stencil_production_direct \
  REPEATED_BOARD_TITLE="CGM RGB stencil production direct repeated board summary" \
  REPEATED_BOARD_RUN_LABEL=cgm_phase050_rgb_stencil_production_direct_repeated \
  EVIDENCE_ROLE_REPEATED=production_direct \
  EVIDENCE_DOC_REF_PRIMARY=doc/phases/050-rgb-stencil-production-integration/result.zh.md \
  EVIDENCE_DOC_REF_SECONDARY=doc/color_gradient_modality-evaluation.zh.md \
  CGM_REPEATED_BENCH_ARGS="--case-filter production_process_320x240,production_process_641x481_tail --iterations 20 --warmup-iterations 3"
```

证据路径：

- summary: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/summary.md`
- manifest: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/evidence_manifest.json`
- doctor: `test-rvv/recognition/color_gradient_modality/log/board/repeated_phase050_rgb_stencil_production_direct/evidence_doctor.md`
- registry: `test-rvv/recognition/color_gradient_modality/log/evidence_registry.json`

| case | runs | median speedup | range | B/A < 1 | checksum |
| --- | ---: | ---: | --- | ---: | --- |
| `production_process_320x240` | 5 | `1.620x` | `1.610x` - `1.680x` | `0/5` | Std/RVV 一致：`5189351474172833280` |
| `production_process_641x481_tail` | 5 | `1.590x` | `1.580x` - `1.660x` | `0/5` | Std/RVV 一致：`2369217414544299322` |

与 Phase 030 production direct 基线相比，当前 public RVV path 的 median 从
`1.490x` / `1.540x` 变为 `1.620x` / `1.590x`。该比较不是严格 RVV-vs-RVV
detail A/B；它只说明 Phase 050 接入后相对 public scalar path 仍稳定有收益，并且当前
生产实现值得保留。

## Evidence Doctor

`log/board/repeated_phase050_rgb_stencil_production_direct/evidence_doctor.md`：

- Errors: `0`
- Warnings: `0`
- Suggestions: `4`

Suggestions 是两个 case 各自缺少环境字段和 binary hash。它们不阻塞当前 positive
结论；若后续出现方向反转或长尾异常，应补记录 taskset、governor、freq、temperature
和 binary hash。

## EvidenceDecision

当前决策为 `adopted production behavior`：

- correctness：Std/RVV 8/8 通过，真实 public entry 的 forced scalar/RVV 对拍一致。
- asm：production helper gate 命中 `vlse8`、widen / integer Sobel、`vfsqrt`、
  `atan2_RVV`、byte compare / merge / store。
- board：production direct 5-run repeated 稳定 positive，checksum 一致。
- boundary：计时包含 RGB copy、Gaussian、Gaussian 后 color-gradient Std/RVV 链路和
  spread，不包含 `extractFeatures()`。

## 继续 / 停止决策

- `continue_stop_decision`: pause after adoption closeout for this production family。
- `stop_condition_hit`: 当前 color-gradient preprocessing production family 已无未阻塞高价值
  接入候选；剩余 `extractFeatures()` 方向需要先证明它在完整模板生成中是热点。
- `next_phase_default`: `060-feature-extraction-profile-ablation` 仅在用户希望继续覆盖
  `extractFeatures()` 时恢复，第一步应是 profile / component ablation，不应直接写 production RVV。
- `doc_rvv_action`: refresh_now completed，长期文档数据采用 Phase 050 production direct board summary。

## 文档套件闭合审计

当前 topic 的优化工作已经结束；本节仅用于说明提交前的文档闭合状态，不引入新的优化方向。

| area | current shape scan | quality bar / supplemental calibration | decision | blocker / evidence | next action |
| --- | --- | --- | --- | --- | --- |
| topic_navigation | `test-rvv/recognition/color_gradient_modality/README.zh.md` 已说明当前 phase、恢复入口、常用命令和 production topic doc 位置 | README 作为入口导航，保留当前状态、证据白名单和 reopen 条件即可 | adopted | `README.zh.md` | none |
| testing_overview | 运行入口和边界已由 `README.zh.md` 的命令清单与 `test-rvv/.../Makefile` 的 target 分类覆盖 | 当前 topic 只有一条主 production entry 和一组 narrow bench / board target，合并在 README + Makefile 足以让 reviewer 定位 | merged: `README.zh.md#常用命令` / `Makefile` targets | `run_test_compare`、`run_bench_rvv`、`check_cgm_*_asm`、`board_repeated`、`record_evidence_state_repeated` | none |
| correctness_tests | `doc/color_gradient_modality-evaluation.zh.md` 已逐阶段记录 `run_test_compare`、forced scalar/RVV 对拍和 feature 输出一致性 | correctness 证据主归属仍在 evaluation；本 topic 无需再拆独立 correctness role 文件 | merged: `doc/color_gradient_modality-evaluation.zh.md#Traceability Map` | `src/test_cgm.cpp`、`run_test_compare` | none |
| benchmark_and_evidence | `doc/color_gradient_modality-evaluation.zh.md` 与 Phase 050 result 已写清 benchmark 边界、board summary、Evidence Doctor 和 registry | board repeated summary 是当前性能主证据；QEMU 仅做日志形状与 correctness，不承载性能 | merged: `doc/color_gradient_modality-evaluation.zh.md#Phase 050 production direct evidence` / `src/bench_cgm.cpp` | Phase 050 summary、manifest、doctor、registry | none |
| optimization_evidence | evaluation、roadmap 和 phase suite 已形成候选取舍、采用理由和暂停理由 | 当前只剩 `extractFeatures()` 的条件性恢复项，不存在新的未阻塞优化族 | merged: `doc/color_gradient_modality-evaluation.zh.md` / `doc/optimization-roadmap.zh.md` / `doc/phases/*` | Phase 000-050 结果、roadmap、matrix | none |
| optimization_roadmap | `test-rvv/recognition/color_gradient_modality/doc/optimization-roadmap.zh.md` 已保留 candidate frontier、恢复条件和拒绝路线 | roadmap 只保留 `extractFeatures()` 的条件性恢复，不再列未闭合的 RVV 主题候选 | adopted | roadmap 文件本身 | none |
| test_support_code_map | `doc/color_gradient_modality-evaluation.zh.md` 的 Traceability Map 已能从 production helper 跳到 test helper、bench wrapper、script 和 board summary | 当前 topic 只有一个 production helper 和一组 test/bench 资产，单独拆 code map 会重复导航信息 | merged: `doc/color_gradient_modality-evaluation.zh.md#Traceability Map` | `src/test_cgm.cpp`、`src/bench_cgm.cpp`、`script/generate_cgm_evidence_manifest.py` | none |
| phase_index / phase_plan / phase_result / optimization_matrix | `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md` 与 Phase 000-050 目录已闭合 | 阶段循环已停在 Phase 050 adopted production behavior；不再产生新的未阻塞 phase | adopted | phase suite 目录 | none |
| evaluation_production | `doc/color_gradient_modality-evaluation.zh.md` 已承载 production 接入判断、证据链和 Traceability Map | 生产长期事实已单独落在 `doc-rvv`；evaluation 只保留决策审计 | adopted | evaluation 文档 | none |
| production_topic_doc | `doc-rvv/recognition/color_gradient_modality-RVV.zh.md` 已刷新为 Phase 050 数据 | adopted production behavior 已有长期维护文档，且数据来源明确 | adopted | production topic doc | none |

### Artifact tracking

当前提交候选只覆盖本 topic 的既有路径：`recognition/include/pcl/recognition/color_gradient_modality.h`、
`test-rvv/recognition/color_gradient_modality/**`、
`doc-rvv/recognition/color_gradient_modality-RVV.zh.md`、
`doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` 和
`tmp/rvv-work-logs/recognition/color_gradient_modality/**`。这些路径没有外溢到其它 topic；板卡 summary、
manifest 和 Evidence Doctor 仍按 summary-only 证据策略保留在 topic-local log 路径下，未把 raw log 当作新长期文档入口。

### 结束判断

当前 topic 没有值得继续推进的未阻塞 RVV 方向；`extractFeatures()` 仍可作为未来 reopen 的独立 profile / ablation
主题，但不属于当前 topic 的继续优化范围。此处可以结束当前优化工作，进入提交准备。
