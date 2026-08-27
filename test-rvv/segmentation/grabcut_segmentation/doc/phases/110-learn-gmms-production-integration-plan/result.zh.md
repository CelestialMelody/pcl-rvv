# Phase 110 learnGMMs production integration 结果

## 当前结论

本阶段完成 `learnGMMs()` production integration loop（生产接入闭环）中的 PI2-PI5。真实
`pcl::segmentation::grabcut::learnGMMs()` 已在 `__RVV10__` 构建中接入 RVV assignment
（RVV 分量归属选择）路径：每个 foreground / background 分组按 VL chunk（可变向量长度分块）
批量计算 K=5 Gaussian probability（高斯概率），选择最大概率 component，随后继续使用原标量
GaussianFitter relearn（高斯拟合器重新训练）。非 RVV 构建、小输入和 helper 不命中时继续走
`learnGMMsStd()`。

接入后两层板卡证据均为 positive：

| evidence role | case | board result |
| --- | --- | --- |
| production-detail（生产细节证据） | `production_learn_gmms` | Milkv-Jupiter clean 96x72 5-run B/A 为 `2.676933, 2.755010, 2.782530, 2.743741, 2.838160`，median `2.755010x`；Std median `4.887261 ms`，RVV median `1.774198 ms`。 |
| production-public（公开生产入口证据） | `public_extract` | Milkv-Jupiter clean 96x72 5-run B/A 为 `1.197422, 1.218683, 1.191227, 1.215751, 1.207907`，median `1.207907x`；Std median `1532.368265 ms`，RVV median `1265.073598 ms`。 |

两层 checksum 均一致，Evidence Doctor（证据体检）均为 `Errors=0, Warnings=0, Suggestions=0`。
这说明 `learnGMMs()` 接入后仍值得保留，并且完整 `extract()` 公开入口收益从 Phase 070 的
`1.112866x` 提升到本阶段的 `1.207907x`。

PI5 用户检查点已经闭合：用户明确确认接入后板卡结果为 positive 即可采纳。本阶段因此升级为
adopted production behavior（已采纳生产行为），并进入 S11 production closeout（生产收尾）。长期
`doc-rvv` 使用本阶段接入后的 production-detail 和 production-public 板卡数据。

## 执行范围

| item | result |
| --- | --- |
| phase plan | `doc/phases/110-learn-gmms-production-integration-plan/plan.zh.md` 已在生产源码和测试资产修改前存在。 |
| production scope | 只修改 `segmentation/src/grabcut_segmentation.cpp` 中 `learnGMMs()` free function 附近的内部 helper 和 dispatch；不改 public API。 |
| RVV coverage | Step 4 component assignment；Step 5 GaussianFitter accumulation 和 `fit()` 保持标量。 |
| validated scope | `Image<Color>`、ordered `Indices`、float `Color`、K=5 GMM、foreground/background hard segmentation mask、96x72。 |
| unvalidated scope | `Scalar=double`、其它 color layout、n-link edge mutation、max-flow solver、color staging、non-organized KNN 和新的 RVV family 选择。 |

## 计划动作回填

| action | status | evidence | conclusion |
| --- | --- | --- | --- |
| PI2 production patch | done | `segmentation/src/grabcut_segmentation.cpp` 新增 `learnGMMsStd()`、`learnGMMsRVV()`、`assignGMMComponentsForGroupRVV()` 和 `relearnGMMsFromComponentsStd()`。 | 原标量主体已抽成 Std fallback helper；public free function 只做 RVV 短路后 fallback。 |
| production direct correctness | done | `make -C test-rvv/segmentation/grabcut_segmentation run_test_compare` 通过。 | Std 8/8、RVV 10/10；真实 production `learnGMMs()` 输出 components 和 GMM 参数与参考链路一致。 |
| fallback coverage | done | `GrabCutProductionDirect.LearnGMMsSmallInputFallbackMatchesReference`；Std 构建也通过。 | 小输入触发 RVV helper false 后自然 fallback；非 RVV 构建不依赖 RVV 符号。 |
| bench case | done | `src/bench_grabcut.cpp` 新增 `production_learn_gmms`。 | case 直接调用真实 production `learnGMMs()`，不再通过 test-only candidate。 |
| QEMU smoke | done | Std/RVV `--width 32 --height 24 --iterations 1 --warmup 0 --case production_learn_gmms` checksum 均为 `11269213739271905300`。 | QEMU 只证明可运行、路径和日志形状；不作为性能结论。 |
| asm attribution | done | `make -C test-rvv/segmentation/grabcut_segmentation dump_bench_rvv` 后，`bench_grabcut_rvv.full.asm` 中存在 `learnGMMsRVV` / `assignGMMComponentsForGroupRVV`，并有 `vle32.v`、`vse32.v`、`vfmacc.vf`、`vfmacc.vv`。 | RVV 指令归属到本阶段生产 helper。 |
| production-detail board repeated | done | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-manifest.json`。 | 5-run bucket 为 positive；median `2.755010x`。 |
| production-public board repeated | done | `doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-manifest.json`。 | 完整 public `extract()` 仍为 positive；median `1.207907x`。 |
| Evidence Doctor / registry | done | `repeated-evidence-doctor.md`、`public-repeated-evidence-doctor.md` 和 `log/evidence_registry.json`。 | 两层 Doctor 均 `0/0/0`；summary artifact 已登记。 |
| docs / Handoff | done | 本 result、matrix、roadmap、evaluation、doc-rvv、Handoff。 | 文档按用户确认刷新为 adopted production behavior；后续优化需另开 phase。 |

## Board evidence

### Production-detail

| field | value |
| --- | --- |
| command | `make -C test-rvv/segmentation/grabcut_segmentation collect_production_learn_gmms_repeated_board PRODUCTION_LEARN_GMMS_REPEATED_DIR=doc/phases/110-learn-gmms-production-integration-plan/repeated-board-20260827-clean-96x72 SSH_OPTS='-F /home/zoomin/.ssh/config -i /home/zoomin/.ssh/id_milkv_jupyter -o IdentitiesOnly=yes'` |
| bench args | `--width 96 --height 72 --iterations 8 --warmup 2 --case production_learn_gmms` |
| manifest | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-manifest.json` |
| doctor | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.md` |
| B/A values | `2.676933, 2.755010, 2.782530, 2.743741, 2.838160` |
| median B/A | `2.755010x` |
| Std median | `4.887261 ms` |
| RVV median | `1.774198 ms` |
| checksum | Std/RVV both `6156654766199696906` |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=0` |

### Production-public

| field | value |
| --- | --- |
| command | `make -C test-rvv/segmentation/grabcut_segmentation collect_production_learn_gmms_public_repeated_board PRODUCTION_LEARN_GMMS_PUBLIC_REPEATED_DIR=doc/phases/110-learn-gmms-production-integration-plan/public-repeated-board-20260827-clean-96x72 SSH_OPTS='-F /home/zoomin/.ssh/config -i /home/zoomin/.ssh/id_milkv_jupyter -o IdentitiesOnly=yes'` |
| bench args | `--width 96 --height 72 --iterations 3 --warmup 1 --case public_extract` |
| manifest | `doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-manifest.json` |
| doctor | `doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-doctor.md` |
| B/A values | `1.197422, 1.218683, 1.191227, 1.215751, 1.207907` |
| median B/A | `1.207907x` |
| Std median | `1532.368265 ms` |
| RVV median | `1265.073598 ms` |
| checksum | Std/RVV both `9089139176994405943` |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=0` |

`B/A = Std_ms / RVV_ms`，`>1` 表示 RVV path 更快。本阶段使用计划内一次 5-run 预算。两层证据
方向稳定，checksum 一致，没有触发追加复跑。板卡日志中的 clock skew warning（远端 Makefile 时间戳
偏未来）没有进入 Evidence Doctor finding；当前作为环境噪声记录，不影响 checksum 或 B/A 结论。

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 100 是 production-shaped diagnostic；Phase 110 已补 production-detail 和 production-public。 |
| A/B boundary | baseline 是真实 Std 构建 `learnGMMs()` / `extract()`；candidate 是真实 RVV 构建命中 `learnGMMsRVV()` 和已采纳 `initGraphTerminalWeightsRVV()` 的路径。 |
| 当前决策问题 | RVV-vs-scalar：接入后的真实生产路径是否快于同入口标量路径。 |
| diagnostic 是否可外推到 production | Phase 100 只能 partial 外推；Phase 110 已用真实 production dispatch 和 public wall-time 闭合主要错配。 |
| comparison-boundary / baseline mismatch 风险 | production-detail 已消除 test-support helper mismatch；production-public 仍不能把收益单独归因到某个 helper，但可证明整体公开入口 positive。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段实际为 positive；若后续重跑转弱 / 负 / 不稳定，应在 PI5 等待用户决定保留、回滚或追加复跑。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前决策是 Std/RVV 接入，不是多个 RVV family 选择；不需要新增 RVV-vs-RVV A/B。若后续尝试 LMUL / ILP / GaussianFitter accumulation，则必须补同边界 A/B。 |

## Evidence Doctor 和 registry

| evidence | path | result |
| --- | --- | --- |
| production-detail manifest | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-manifest.json` | registered |
| production-detail Doctor | `doc/phases/110-learn-gmms-production-integration-plan/repeated-evidence-doctor.md/.json` | `Errors=0, Warnings=0, Suggestions=0` |
| production-public manifest | `doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-manifest.json` | registered |
| production-public Doctor | `doc/phases/110-learn-gmms-production-integration-plan/public-repeated-evidence-doctor.md/.json` | `Errors=0, Warnings=0, Suggestions=0` |
| registry | `log/evidence_registry.json` | Phase 110 summary artifacts 已登记；最终验证需 `make evidence_status`。 |

## 优化矩阵更新

| candidate family | result | decision | next action |
| --- | --- | --- | --- |
| production `learnGMMs` RVV assignment + scalar relearn | correctness、fallback、QEMU smoke、asm、production-detail board、production-public board 和 Doctor 均闭合；用户确认采纳。 | adopted production behavior | 当前生产补丁保留。 |
| post-adoption public component profile | Phase 080 是接入 `learnGMMs()` 前的 profile；接入后还没有重新拆分 public wall-time。 | phase_deferred + unblocked | 创建下一 phase，重跑 `public_extract_profile`，确认剩余热点和是否仍值得尝试 GaussianFitter accumulation 或 LMUL / ILP A/B。 |
| GaussianFitter accumulation RVV | 当前 production-public 已有 `1.207907x`，Step 5 仍为标量。 | deferred pending profile | 只有接入后 profile 显示 relearn 仍有可见占比，才开独立 accumulation 消融和数值预算 phase。 |
| LMUL / ILP variants | 本阶段复用 Phase 100 形状，未做 RVV-vs-RVV A/B。 | deferred pending profile | 只有接入后 profile 或 reviewer 指向当前 helper 仍是主要成本时，再做同边界 RVV-vs-RVV A/B。 |

## Continue / stop decision

`continue_stop_decision`: `continue`。

停止条件已解除：用户确认按接入后板卡 positive 采纳当前 production patch。Phase 110 的生产接入闭环
和 S11 closeout 已完成到文档状态；由于 roadmap 中仍有当前授权范围内的未阻塞诊断动作，默认继续进入
接入后 public component profile（公开入口组件剖析）阶段。该阶段不再直接修改 production 源码，而是先验证
剩余热点是否足以支撑新的 RVV family。

`next_phase_default`: 创建并执行 `120-post-learn-gmms-adoption-profile`。
