# Phase 070 Plan: production-integration-loop

## 阶段意图和边界

本阶段执行 Phase 060 之后的 PI2-PI5 production integration loop（生产接入闭环）。用户已授权：若接入后的板卡测试显示收益且证据链可接受，则可以采纳该生产补丁，并创建/更新正式 `doc-rvv` 主题文档；正式文档中的性能数据必须采用接入后的板卡测试数据。

候选范围仍保持 Phase 060 冻结边界：只覆盖默认宏配置下 `matchTemplates` 与 `detectTemplates` 的 `EnergyMaps -> LinearizedMaps` copy loop。`detectTemplatesSemiScaleInvariant`、`LINEMOD_USE_SEPARATE_ENERGY_MAPS`、NMS、averaged detection、score accumulation 和 threshold scan 不在本阶段生产改动范围内。

## Validated / Unvalidated Scope

| scope type | content |
| --- | --- |
| validated_scope | 默认宏下单个 `EnergyMaps` bin 到 `LinearizedMaps` 的 64 个 offset maps；`step_size=8`；`unsigned char` map 数据；`matchTemplates` 和 `detectTemplates` 两个公开入口的默认 copy loop。 |
| unvalidated_scope | semi-scale scaled feature offset、separate-energy 四套 map、非默认 step size、NMS / averaged detection 输出排序、score accumulation / scan RVV、真实 modality 分布的更宽 profile。 |
| point_type_expansion_queue | not applicable；该优化段只处理 LINEMOD 字节图，不涉及 PCL 点类型或 `Scalar`。 |
| phase_closeout_boundary | 若 PI4 production evidence positive 且 Evidence Doctor 没有未处理 Error，可采纳本阶段生产补丁；不能把 semi-scale 或 separate-energy 写成已覆盖。 |

## 实现计划

| PI step | action | expected evidence |
| --- | --- | --- |
| PI2 RED | 先在 topic-local test/bench 增加 production-direct canary（生产直连哨兵）或 production-linked bench label，要求 RVV 构建能命中生产 `linemod.cpp` 中的 linearized copy RVV helper；补丁前应失败或缺少预期符号/指令归属。 | RED 日志记录在 phase result；失败原因应是生产 RVV helper 尚未存在或未命中。 |
| PI2 GREEN | 在 `recognition/src/linemod.cpp` 文件局部新增 `linearizeEnergyMapStd` / `linearizeEnergyMapRVV` / dispatch helper；默认宏下 `matchTemplates` 和 `detectTemplates` 的 copy loop 调用该 helper。 | 公开 API 不变，非 RVV 构建走标量 helper，RVV 构建在 `__RVV10__` 下使用 `vlse8.v` + `vse8.v`。 |
| PI3 tests | 运行 topic correctness；新增/扩展 production-direct 测试验证 copy helper 语义、tail 和 fallback。 | Std/RVV correctness 均通过；fallback 不改变标量输出。 |
| PI4 asm | dump production-linked RVV binary，确认 `vlse8.v` / `vse8.v` / `vsetvli` 可归属到生产 helper 或其 inline clone。 | asm 摘要写入 production evidence manifest。 |
| PI4 board repeated | 在板卡上执行 production-direct repeated benchmark，5-run、warm-up 5、iterations 200。 | 以接入后 production-public / production-detail 数据判断收益；QEMU timing 不作为性能结论。 |
| PI4 doctor / registry | 生成 production evidence manifest、summary、Evidence Doctor 和 registry 记录。 | Errors 必须为 0；Warnings 若存在须解释和降级。 |
| PI5 decision | 若接入后板卡收益成立且 doctor 可接受，按用户授权采纳并进入 S11 production closeout；否则保留 diff 并暂停请求回滚/下一步。 | result、matrix、roadmap、evaluation、doc-rvv、队列表和 Handoff 同步。 |

## Diagnostic To Production Mismatch Audit

| question | answer |
| --- | --- |
| evidence role | Phase 050 是 `production_shaped_diagnostic`；本阶段 PI4 必须生成 `production-public` 或 `production-detail` 证据。 |
| A/B boundary | 从 `test_helper` 升级为 production-linked helper / public overload 边界。 |
| 当前决策问题 | production RVV-vs-scalar 是否值得接入。 |
| diagnostic 是否可外推到 production | Phase 050 只支持进入本阶段探针；最终采纳只看 PI4 接入后板卡数据。 |
| comparison-boundary / baseline mismatch 风险 | 存在；本阶段用生产源码 helper、生产库链接和 production-direct bench 降低错配。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已授权 bounded production probe；若接入后转弱/负/不稳定，不自动采纳。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前只有一个生产 RVV family，不做 family selection；后续若新增 fused/multi-bin family 再补 A/B。 |

## 板卡预算与决策桶

- bounded rerun budget：默认 5-run，warm-up 5，iterations 200。
- positive：median >= 1.05 且 0/5 degradation，可采纳。
- weak_positive：1.0 <= median < 1.05 且无退化，需要结合实现复杂度；本阶段实现很小，可记录为可采纳但带风险。
- unstable / negative：存在退化或 median < 1.0 时不采纳，除非用户另行判断。

## 文档更新清单

- 更新本阶段 `result.zh.md`、optimization matrix、roadmap、topic-local evaluation、README / phase index。
- 若 PI4 接入后收益成立，创建/更新 `doc-rvv/recognition/linemod_template_scoring-RVV.zh.md`，只写 adopted production behavior 和接入后板卡数据。
- 更新 `doc-rvv/library-screening/recognition/recognition-function-evaluation-queue.zh.md` 的当前状态。
- 更新 current Handoff Markdown/YAML。
