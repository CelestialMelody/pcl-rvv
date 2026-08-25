# Phase 040 Result: direct AoS production probe

## 当前结论

本阶段完成 `pfh-direct-aos-production-rvv` 的 PI2-PI5 生产接入探针。生产补丁已经接入
`features/include/pcl/features/impl/pfh.hpp::computePointPFHSignature`，但只覆盖 exact
`PFHEstimation<pcl::PointNormal, pcl::PointNormal, pcl::PFHSignature125>`、`float`、AoS
xyz+normal 布局、`nr_split == 5`、`use_cache_ == false`、邻域规模不少于 4 且 32-bit byte offset
可表达的路径；其它模板实例、cache path、`PointXYZ + Normal`、`Scalar=double` 和自定义点型都保持
fallback（回退到原标量路径）。

PI5 EvidenceDecision（证据决策）：`user_confirmed_adopted_production`。接入后的板卡
production-public（真实公开入口）5-run 结果稳定正向，用户已确认采纳当前生产补丁；正式长期文档由
Phase 050 创建为 `doc-rvv/features/pfh-RVV.zh.md`。

## 执行动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| A1 production direct RED | done | `make -B -C test-rvv/features/pfh run_test_rvv` 在生产 helper 未存在时失败；接入后 `run_test_compare` Std 3/3、RVV 4/4 pass。 | RVV-only production helper 测试能证明 exact `PointNormal` helper 语义。 |
| A2 production patch | done | `features/include/pcl/features/impl/pfh.hpp` 新增 `pcl::detail::computePointPFHSignatureDirectAoSRVV`，公开入口在标量主体前短路尝试。 | 非覆盖路径自然回落标量；本阶段未扩大 public API。 |
| A3 QEMU correctness | done | `make -B -C test-rvv/features/pfh run_test_compare`。 | Std/RVV correctness 均通过；QEMU 不作为性能证据。 |
| A4 asm | done | `make -B -C test-rvv/features/pfh dump_bench_rvv`；`nm -C` 可见 `pcl::detail::computePointPFHSignatureDirectAoSRVV<pcl::PointNormal, pcl::PointNormal>`，该符号范围内有 `vluxei32.v`、`vfmacc.vv`、`vfsqrt.v`、`vfdiv.vv`。 | RVV 指令可归属到 production helper。 |
| A5 board smoke | done | `make -C test-rvv/features/pfh board_smoke BENCH_ARGS='--side 32 --k 32 --iterations 3 --warmup 1'`。 | 板端 RVV test 4/4 pass；`component_pfh_signature` `2.00x`，`public_pfh_k` `1.92x`。 |
| A6 board repeated | done | `make -C test-rvv/features/pfh board_repeated BENCH_ARGS='--side 32 --k 32 --iterations 8 --warmup 2' REPEATED_BOARD_OUTPUT_DIR=log/board/pi2-production-direct-aos/repeated`。 | 5-run decision bucket 为 `positive`，未出现 `<1x`。 |
| A7 Evidence Doctor / registry | done | `make -C test-rvv/features/pfh evidence_doctor_repeated ...pi2-production-direct-aos...`；`python3 test-rvv/script/evidence_registry.py check --registry test-rvv/features/pfh/log/evidence_registry.json ...`。 | Doctor `0E/0W/8S`；registry fresh。 |
| A8 文档刷新 | done | 本 result、phase README、optimization matrix、roadmap、features queue；Phase 050 创建 `doc-rvv/features/pfh-RVV.zh.md`。 | 长期 `doc-rvv` 使用本阶段接入后的板卡数据，不复用诊断阶段 speedup 作为采纳数字。 |

## 板卡 repeated 结果

输入：Milkv-Jupiter，synthetic pfh point-normal grid，`side=32`、`points=1024`、`k=32`，
`iterations=8`、`warmup=2`、5 runs。B/A 方向为 `Std ms / RVV ms`，大于 1 表示 RVV 更快。

| case | evidence role | 5-run B/A | mean / median | min / max | decision |
| --- | --- | --- | --- | --- | --- |
| `component_pfh_signature` | production-detail | `2.01, 1.99, 2.01, 2.01, 2.00` | `2.004x / 2.01x` | `1.99x / 2.01x` | positive |
| `public_pfh_k` | production-public | `1.92, 1.93, 1.92, 1.94, 1.92` | `1.926x / 1.92x` | `1.92x / 1.94x` | positive |
| `candidate_pfh_direct_aos_rvv` | diagnostic cross-check | `2.80, 2.78, 2.80, 2.79, 2.79` | `2.792x / 2.79x` | `2.78x / 2.80x` | historical diagnostic remains positive |
| `candidate_pfh_pair_batch_rvv` | diagnostic cross-check | `2.34, 2.33, 2.34, 2.33, 2.34` | `2.336x / 2.34x` | `2.33x / 2.34x` | historical fallback family |

`public_pfh_k` 的正向只证明当前真实公开入口 RVV path 快于当前标量 path；因为 PFH 此前没有 adopted RVV
family，本阶段无需同一 production boundary 内的 RVV-vs-RVV family A/B 才能采纳首个 RVV-vs-scalar
production path。用户确认后，adopted 状态由 Phase 050 收口。

## Evidence Doctor 与 registry

- Manifest：`test-rvv/features/pfh/log/board/pi2-production-direct-aos/repeated/evidence_manifest.json`
- Doctor：`test-rvv/features/pfh/log/board/pi2-production-direct-aos/repeated/evidence_doctor.md`
- JSON：`test-rvv/features/pfh/log/board/pi2-production-direct-aos/repeated/evidence_doctor.json`
- Registry：`test-rvv/features/pfh/log/evidence_registry.json`
- Result：`Errors=0, Warnings=0, Suggestions=8`

8 个 suggestion 均为 metadata 建议：缺少 `taskset`、`governor`、`freq`、`temperature` 和 binary hash
等环境 / 二进制身份字段。当前 5-run 方向稳定、checksum 匹配、无 Error/Warning，因此不阻塞 PI5
positive bucket；若后续扩大点型或规模，应在新的 repeated summary 中补这些字段。

Registry freshness：当前 `phase040-production-direct-aos-side32-k32-it8-warm2-runs5` 三个摘要产物已登记。
旧 `log/board/repeated` 诊断摘要被重登记为 historical evidence，避免旧 hash 干扰当前 production 结论。

## Diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `component_pfh_signature` 为 production-detail；`public_pfh_k` 为 production-public；两个 candidate case 只作为 historical diagnostic cross-check。 |
| A/B boundary | Std build 原标量 production path vs RVV build 中同一 public/helper boundary 下的 RVV dispatch。 |
| 当前决策问题 | `RVV-vs-scalar`：当前 production patch 是否值得保留。 |
| diagnostic 是否可外推到 production | Phase 030 diagnostic 只决定首个生产探针 family；PI5 采纳建议来自本阶段 production boundary 内数据。 |
| comparison-boundary / baseline mismatch 风险 | aggregate bench 同时打印 production 和 diagnostic case；本 result 已分开 evidence role，采纳判断只看 production-detail / production-public。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 不适用为拒绝条件；本阶段 production repeated 为 positive。若后续新点型出现弱/负/不稳，应独立进入 PI5。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前无既有 PFH adopted RVV family；不需要用于首个 RVV-vs-scalar 保留建议。若未来比较 staged/direct 等多个 production family，则需要补同边界 RVV-vs-RVV A/B。 |

## 范围与点类型扩展队列

已验证范围：

- `__RVV10__` RVV 构建。
- `pcl::PointNormal -> pcl::PointNormal`，`pcl::PFHSignature125`，`float`，AoS xyz+normal 布局。
- `use_cache_ == false`、`nr_split == 5`、`indices.size() >= 4`。
- synthetic dense finite cloud，KSearch public bench `side=32,k=32`。

未验证范围：

- 常见 `PointXYZ + Normal` 组合。
- PointXYZ-like / PointNormal-like traits 泛型集合、自定义点类型、`PointXYZINormal`。
- `use_cache_ == true` cache path。
- `Scalar=double` 或非 float 字段布局。
- indices / normals 非同索引容器语义、其它 PFH family caller、OMP path。

`point_type_expansion_queue`：

| candidate | status | resume condition |
| --- | --- | --- |
| `pfh-pointxyz-normal-production-expansion` | phase_deferred + requires new PI loop | 需要读取 generic point type strategy，新增 source xyz + normal cloud 两端 traits / layout gate、fallback tests、production direct bench、asm 和 repeated board。 |
| `pfh-pointnormal-like-traits-gate` | deferred | 在 exact `PointNormal` 采纳后，再把 layout-gated traits 与 offset/POD/standard-layout 证据补齐；不能用本阶段数据外推。 |
| `pfh-cache-path-rvv` | not_recommended_now | cache path 语义和 key 访问成本不同，且 public evidence 已在 `use_cache_ == false` 下正向；除非 profile 显示 cache path 热，否则不建议同 topic 立即扩大。 |

## 继续 / 停止决策

`continue_stop_decision`: Phase 040 的 PI5 user checkpoint 已由用户确认，进入 Phase 050 production closeout。

Phase 040 停止条件已解除：生产接入闭环 PI2-PI5 已完成，接入后板卡 repeated 为 positive，用户确认
采纳当前 production patch。后续状态和正式 `doc-rvv` 由 Phase 050 负责。

`next_phase_default`: `050-production-closeout-doc-rvv`，随后若仍有未阻塞优化动作，进入
`060-pointxyz-normal-production-expansion`。

Phase 050 必须使用本阶段 `pi2-production-direct-aos/repeated` 的接入后板卡数据，并保持
`PointXYZ + Normal` / 泛型 traits 扩展为新的独立 phase。
