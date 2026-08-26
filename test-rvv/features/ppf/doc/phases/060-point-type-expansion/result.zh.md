# Phase 060 Result: Point type expansion

## 阶段结论

本阶段完成 `features/include/pcl/features/impl/ppf.hpp` 的 point type expansion（点类型扩展）。
Phase 040/050 已采纳的 `alpha_m` RVV production path（生产路径）不再只限制 exact
`pcl::PointXYZ + pcl::Normal + pcl::PPFSignature`，而是扩展为：

- source 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointInT>::value`。
- normal 满足 PPF 本地 `PPFNormalAoSFloatLayout<PointNT>::value`。
- output 仍必须是 exact `pcl::PPFSignature`。
- `__RVV10__` 未启用、不满足 traits / AoS layout（字段特征 / 结构数组布局）或输出类型不匹配时，
  继续回退到 `computePPFFeatureStd`。

生产实现仍只 RVV 化 `alpha_m` 后段；`f1..f4` 继续调用当前 production 标量路径使用的
`pcl::computePairFeatures`。本阶段不改变公开 API，也不接入 Phase 010 rejected 的 pair-feature batch RVV。

EvidenceDecision（证据决策）：`adopted production behavior`。两组新增 representative point type
（代表性点类型）production-public 板卡数据均为 positive，且 Evidence Doctor（证据体检）无 Error / Warning。

## 计划执行回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 RED 测试 | done | `make -C test-rvv/features/ppf run_test_rvv` 在生产修改前仅新 trace 测试失败，`pcl_rvv_ppf_alpha_m_trace_hits == 0`。 | 证明 Phase 050 exact gate 尚未覆盖新点型。 |
| A2 production gate 扩展 | done | `features/include/pcl/features/impl/ppf.hpp` 引入 `pcl/rvv_point_traits.h` 和 PPF 本地 normal AoS gate。 | dispatch（分流逻辑）改为 source xyz AoS traits + normal AoS traits + exact `PPFSignature`。 |
| A3 staging 字段读取改造 | done | `ppfRVVReadFloatField<PointT, offset>()` 使用当前模板点型 offset 读取 xyz / normal staging 输入。 | 不再依赖 `PointXYZ` / `Normal` 的固定成员布局。 |
| A4 correctness / fallback | done | `make -C test-rvv/features/ppf run_test_compare`；板卡 `run_board_test fetch_board_logs`。 | 本地 Std 5/5、RVV 9/9 pass；板卡 RVV 9/9 pass。unsupported output / layout fallback trace 不命中。 |
| A5 bench case | done | `public_ppf_compute_pointxyzi_normal`、`public_ppf_compute_pointxyz_pointnormal`。 | 两个 case 都使用 public `PPFEstimation::compute`，计时边界是 production-public。 |
| A6 asm / board / Doctor | done | `dump_bench_rvv`、两组 5-run `board_repeated evidence_doctor_repeated`。 | 三个 production helper 实例均有 RVV 指令；两组新增点型板卡 positive；Doctor 均 `0E/0W/2S`。 |
| A7 文档回填 | done | 本 result、matrix、roadmap、evaluation、README、`doc-rvv/features/ppf-RVV.zh.md`、筛选状态表和 Handoff。 | 正式长期文档使用接入后的 Phase 060 板卡数据，不使用诊断数据外推。 |

## Production direct correctness

`make -C test-rvv/features/ppf run_test_compare`：

- Std build：5/5 pass。
- RVV build：9/9 pass。
- 新增 RVV production-direct tests：
  - `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForPointXYZILikeSource`
  - `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForPointNormalLikeNormals`
  - `PPFProductionDirect.RVVAlphaMRejectsUnsupportedLayouts`

板卡 correctness：

```bash
make -C test-rvv/features/ppf OUTPUT_DIR_BOARD=log/board/phase060-correctness REMOTE_BOARD_OUTPUT_DIR=/root/pcl-test/features/ppf/log/board/phase060-correctness run_board_test fetch_board_logs
```

结果：RVV board tests 9/9 pass。板卡输出中有 `rvv-board-run.mk` timestamp / clock-skew
warning（时间戳偏移提示）；它不改变 correctness 结果，也不改变 repeated board 的 positive 决策桶。

## QEMU 和反汇编证据

QEMU bench smoke（小型运行）只用于证明新 case-filter、checksum 输出和日志形状可运行，不作为性能结论：

- `public_ppf_compute_pointxyzi_normal` checksum：`213486`
- `public_ppf_compute_pointxyz_pointnormal` checksum：`213486`

`make -C test-rvv/features/ppf dump_bench_rvv` 后，`nm -C` 可见三个 production helper 实例：

- `computePPFFeatureAlphaMRVV<PointXYZ, Normal, PPFSignature>`
- `computePPFFeatureAlphaMRVV<PointXYZI, Normal, PPFSignature>`
- `computePPFFeatureAlphaMRVV<PointXYZ, PointNormal, PPFSignature>`

符号范围检查确认每个 helper 范围内都有 `vsetvli`、`vle32`、`vse32`、`vfmv` 和浮点向量算术指令。
反汇编只证明路径和手写 RVV 指令归属，不单独证明性能。

## Board performance

两组板卡性能均使用 production-public（生产公开入口）Std/RVV A/B，规模与 Phase 040 一致：

```text
--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2
```

### `PointXYZI + Normal -> PPFSignature`

命令：

```bash
make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 REPEATED_BOARD_OUTPUT_DIR=log/board/phase060-pointxyzi-normal/repeated BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute_pointxyzi_normal" board_repeated evidence_doctor_repeated
```

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 435.968 | 311.465 | 1.40x |
| run_02 | 439.786 | 306.714 | 1.43x |
| run_03 | 413.605 | 311.647 | 1.33x |
| run_04 | 440.339 | 311.700 | 1.41x |
| run_05 | 418.133 | 310.057 | 1.35x |
| mean | 429.5662 | 310.3166 | 约 1.384x |

Evidence Doctor：`Errors=0, Warnings=0, Suggestions=2`。

### `PointXYZ + PointNormal -> PPFSignature`

命令：

```bash
make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 REPEATED_BOARD_OUTPUT_DIR=log/board/phase060-pointxyz-pointnormal/repeated BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute_pointxyz_pointnormal" board_repeated evidence_doctor_repeated
```

| run | Std ms | RVV ms | speedup |
| --- | ---: | ---: | ---: |
| run_01 | 411.535 | 308.818 | 1.33x |
| run_02 | 411.882 | 310.287 | 1.33x |
| run_03 | 408.980 | 311.958 | 1.31x |
| run_04 | 412.259 | 309.951 | 1.33x |
| run_05 | 417.473 | 310.364 | 1.35x |
| mean | 412.4258 | 310.2756 | 约 1.33x |

Evidence Doctor：`Errors=0, Warnings=0, Suggestions=2`。

两个 Doctor suggestion 都是 `environment_metadata_missing` 和 `binary_identity_missing`。这些建议降低严格归档
可复核性，但在没有 Error / Warning、5-run 决策桶稳定 positive 的情况下不阻塞采纳。若后续要提交严格证据归档，
可另开 evidence hardening phase 补 taskset / governor / freq / temperature / binary hash。

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | production-public + production-direct correctness。 |
| A/B boundary | 同一 public overload；Std build vs RVV build。 |
| 当前决策问题 | `RVV-vs-scalar`：扩大点类型 gate 后 public RVV path 是否仍快于 public scalar path。 |
| diagnostic 是否可外推到 production | 不外推。本阶段使用接入后的 public case 和 production direct tests。 |
| comparison-boundary / baseline mismatch 风险 | 两个新增 case 使用独立 label，未复用 exact `public_ppf_compute` 数值。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不需要；本阶段 repeated board 稳定 positive。若未来某个新点型单独 negative，只能收窄该点型证据，不能推翻已验证代表点型。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有替换 RVV family，只扩大同一 `alpha_m` family 的 traits gate。 |

## 优化矩阵更新

| candidate family | row source policy | point type / Scalar / layout | correctness / fallback target | board evidence | asm | Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ppf-point-type-expansion` | ordered `indices_ x input_` | `RVVXYZAoSFloatLayout<PointInT>` + PPF normal AoS + exact `PPFSignature` / float | `run_test_compare` Std 5/5、RVV 9/9；board RVV 9/9 | `PointXYZI + Normal` mean 1.384x；`PointXYZ + PointNormal` mean 1.33x | three production helper instances contain RVV instructions | both `0E/0W/2S` | adopted production behavior |

## 覆盖范围和未覆盖范围

已验证并采纳：

- public `PPFEstimation::compute` / `computeFeature`。
- ordered `indices_ x input_` all-pairs row source。
- source xyz AoS traits gate（以 `PointXYZI + Normal` 为代表性 production-public board case）。
- normal AoS traits gate（以 `PointXYZ + PointNormal` 为代表性 production-public board case）。
- exact `pcl::PPFSignature` 输出、float AoS、Milkv-Jupiter board repeated。

仍不覆盖：

- `Scalar=double` 或非 float output 语义。
- 非 `PPFSignature` 输出。
- source-indexed、dual-indexed、correspondence 或 PPFRGB / CPPF 等其它 row source / caller。
- 对每一个用户自定义 traits-compatible 点型逐一板卡覆盖。本阶段证明 gate 语义和两个代表性点型，
  其它满足 traits 的 AoS 点型属于 gate-allowed but not individually board-covered。
- Phase 010 pair-feature batch RVV；该候选仍按负向证据拒绝。

## Continue / Stop Decision

`continue_stop_decision`: stop / ready for review。

`stop_condition_hit`: 当前 phase plan 的完成矩阵已闭合；roadmap 和 optimization matrix 中没有仍值得在当前
PPF topic 内继续推进的 high-priority unblocked optimization action（高优先级未阻塞优化动作）。

不建议继续同一 topic 的原因：

- 继续改 `alpha_m` staging buffer 预期收益小，必须新增同边界 RVV-vs-RVV A/B 和板卡证据；当前没有 profile
  指向它是值得优先做的剩余瓶颈。
- Phase 010 已用板卡 negative 证据拒绝 SoA-staged pair-feature batch RVV；direct-AoS pair-feature revisit
  需要新的 profile 或同边界设计假设，不应在缺少瓶颈证据时继续试。
- evidence hardening 只增强归档质量，不是新的优化方式；可以提交前或严格归档时另开窄 phase。
- PPFRGB / CPPF / 其它 caller 属于独立 topic，不能在当前 PPF closeout 中外推。

`next_phase_default`: ready for review。若用户后续要求继续扩展，首选不是继续微调当前 `alpha_m`，而是另开
独立 phase / topic：evidence hardening、profile-driven direct-AoS pair-feature revisit，或 PPFRGB 跟随评估。
