# Phase 040 Production Alpha-M RVV Integration Result

## 当前结论

本阶段已完成 production integration loop（生产接入闭环）的 PI1-PI5。当前生产补丁把
`PPFEstimation::computeFeature` 改成短分流入口：`__RVV10__` 构建且模板实参精确为
`PointXYZ + Normal + PPFSignature` 时走 `alpha_m` RVV helper，其它类型和非 RVV 构建走
`computePPFFeatureStd` 标量 helper。`f1..f4` 仍调用 production 当前使用的
`pcl::computePairFeatures`，没有替换为 `computePPFPairFeature`。

PI5 decision 当时是 `pending_user_confirmation_adopt_or_rollback`。production-public 板卡证据为
positive。Phase 050 已收到用户采纳确认，并把当前补丁写成 adopted production behavior，同时发布
正式 `doc-rvv/features/ppf-RVV.zh.md`。

## 计划执行回填

| action | status | evidence / command | result |
| --- | --- | --- | --- |
| 新增 production-direct RED test | done | `make -C test-rvv/features/ppf run_test_rvv` | RED 失败点为 `pcl_rvv_ppf_alpha_m_trace_hits == 0`，证明测试能捕获公开入口未命中 RVV 分流。 |
| 新增 test-only trace 宏 | done | `PCL_RVV_PPF_ENABLE_TEST_TRACE` 仅由 `src/test_ppf.cpp` 在包含 production 头前定义 | trace 不进入普通 public API，也不进入 bench 二进制；production-public board 数据来自无 trace bench。 |
| 抽出标量 helper | done | `features/include/pcl/features/impl/ppf.hpp` | 原标量主体迁入 `pcl::detail::computePPFFeatureStd`，公开入口只做 RVV try + Std fallback。 |
| 接入 production RVV helper | done | `pcl::detail::computePPFFeatureAlphaMRVV` | exact type gate 下先标量计算 `f1..f4`，再批量 RVV 计算成功点对的 `alpha_m`。 |
| 更新 manifest metadata | done | `script/generate_ppf_evidence_manifest.py` | `public_ppf_compute` 改为 `production-public` evidence role；public-only repeated summary 顶层 role 也为 `production-public`。 |
| correctness | done | `make -C test-rvv/features/ppf run_test_compare` | Std 5 tests passed；RVV 6 tests passed，新增 production-direct test 命中 trace 并与 reference 对拍。 |
| QEMU smoke | done | `make -C test-rvv/features/ppf BENCH_ARGS="--side 8 --index-count 4 --repeat 1 --iterations 1 --warmup 0 --case-filter public_ppf_compute" run_bench_rvv` | public case 可运行并输出 checksum；QEMU 不作为性能证据。 |
| asm attribution | done | `make -C test-rvv/features/ppf dump_bench_rvv` + symbol scan | `computePPFFeatureAlphaMRVV<PointXYZ, Normal, PPFSignature>` 符号范围含 `vsetvli`、`vle32`、`vfdiv`、`vfsqrt`、`vmerge`、`vfnmsac`、`vfmacc`、`vse32`。 |
| board repeated | done | `make -C test-rvv/features/ppf REPEATED_BOARD_RUNS=5 BENCH_ARGS="--side 28 --index-count 64 --repeat 8 --iterations 8 --warmup 2 --case-filter public_ppf_compute" board_repeated evidence_doctor_repeated` | public-only 5-run speedup 为 `1.35, 1.35, 1.37, 1.39, 1.38`；mean Std `390.0784 ms`，mean RVV `284.9594 ms`。 |
| Evidence Doctor | done | `log/board/repeated/evidence_doctor.md` | `Errors=0, Warnings=0, Suggestions=2`；suggestions 为环境 metadata 和 binary identity 缺失，不阻塞当前 positive bucket。 |

## Production Direct Evidence

| evidence | result | 结论边界 |
| --- | --- | --- |
| RED / GREEN correctness | RED 时只有新增 production-direct trace 断言失败；GREEN 后 Std/RVV correctness 全通过。 | 证明 RVV 构建下 public `compute()` 真实命中生产分流，并保持当前样本输出语义。 |
| QEMU public smoke | public case 小规模运行成功，checksum `213486`。 | 证明 RVV bench 二进制和 case-filter 可运行；不证明真实性能。 |
| asm attribution | public compute 调用 `computePPFFeatureAlphaMRVV`，helper 范围含 RVV 指令。 | 证明生产 helper 不是死代码，且 hot helper 有手写 RVV 指令。 |
| board production-public | 5-run mean speedup 约 `1.35x`，checksum 均显示 `6.80558e+11`。 | 证明当前 exact-type public RVV path 在 Milkv-Jupiter、本阶段规模下快于 public scalar path。 |
| Evidence Doctor | 无 Error / Warning；2 个 Suggestion。 | 支持 `positive / pending user confirmation`，不支持直接越过 PI5。 |

## Diagnostic-to-Production Mismatch Audit

| question | result |
| --- | --- |
| evidence role | Phase 040 使用 production-public 证据；不再用 Phase 030 diagnostic 数字替代生产判断。 |
| A/B boundary | A/B 是同一 public overload 的 Std build vs RVV build。 |
| 当前决策问题 | 当前 public RVV path 是否快于当前 public scalar path，并是否值得让用户确认保留补丁。 |
| diagnostic 是否可外推到 production | Phase 030 只作为进入 probe 的理由；Phase 040 生产数据已重新采集。 |
| comparison-boundary / baseline mismatch 风险 | 当前生产判断使用 public boundary，已消除 test-helper 与 production public 的主要错配；仍不能证明其它点型 / row source。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前没有既有 adopted RVV family；但 PI5 仍是用户检查点，不能自行 clean-adopt。 |

## Evidence Doctor 处理

`log/board/repeated/evidence_doctor.md` 报告：

- Errors：0。
- Warnings：0。
- Suggestions：2。

`environment_metadata_missing` 和 `binary_identity_missing` 不阻塞当前结论，因为 5-run 方向稳定、
checksum 一致、且 correctness / asm 均闭合。它们作为后续证据增强项保留：若未来出现方向反转、
长尾或 reviewer 要求严格归档，应补 taskset / governor / freq / temperature 和 binary hash。
板卡远端 make 输出还包含 clock skew（时钟偏移）警告；本阶段每轮都强制重新构建、上传并 fetch
对应 run 目录日志，且结果方向稳定，因此该环境提示不改变当前 decision bucket。

## Optimization Matrix 更新

`production alpha_m batch RVV` 在本阶段 exact scope 下状态更新为
`positive production-public`；Phase 050 已完成采纳 closeout。未覆盖矩阵仍保持 deferred：

- `PointXYZ-like + Normal-like` traits gate 未验证。
- `PointNormal + PointNormal` 和其它模板实例只证明 fallback 意图，未形成独立 production-direct
  correctness / board 证据。
- source-indexed、dual-indexed、correspondence row source 不属于本阶段入口。

## Continue / Stop Decision

`continue_stop_decision`：本阶段历史记录为 stop at PI5 user checkpoint；Phase 050 已解除该检查点并完成采纳收尾。

`stop_condition_hit`：workflow PI5 要求生产证据闭环后暂停，保留当前 production patch，报告
production diff、测试命令、板卡 / Evidence Doctor 结果和拟议下一步，等待用户确认采纳或回滚。

默认下一步：见 Phase 050。当前 exact production boundary 已 adopted；若继续扩大覆盖范围，
需新建 point-type expansion、evidence hardening 或 direct-AoS pair-feature revisit phase。
