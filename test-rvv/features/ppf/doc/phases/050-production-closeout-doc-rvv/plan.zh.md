# Phase 050 Production Closeout Doc-RVV Plan

## 阶段意图和边界

本阶段处理 Phase 040 PI5 后的用户采纳确认。用户已确认“板卡上的测试结果如果显示有收益即可采纳”，因此本阶段进入
S11 production closeout（生产收尾）：把当前 exact `PointXYZ + Normal + PPFSignature`
production patch 视为 adopted production behavior（已采用生产行为），创建正式
`doc-rvv/features/ppf-RVV.zh.md`，并同步 topic-local evaluation、roadmap、matrix、phase index、
README 和 retained candidate rescreen（保留候选复筛）状态。

本阶段不扩大生产实现范围，不新增 RVV helper，不把 exact gate 外推成泛型 traits 结论，也不重跑完整板卡矩阵。
性能数据只使用 Phase 040 接入后的 production-public board repeated 数据。

## validated_scope

| dimension | scope |
| --- | --- |
| production entry | `PPFEstimation::computeFeature` / public `compute()` |
| row source | ordered `indices_ x input_` all-pairs |
| point type / output | exact `pcl::PointXYZ + pcl::Normal + pcl::PPFSignature` |
| Scalar / layout | float / AoS |
| RVV stage | `alpha_m` 后段公式；`f1..f4` 保持 `pcl::computePairFeatures` |
| target hardware | Milkv-Jupiter board |
| evidence role | production-public Std/RVV A/B after production patch |

## unvalidated_scope

- `PointXYZ-like + Normal-like` traits gate 与其它模板点型。
- `Scalar=double`。
- source-indexed、dual-indexed、correspondence row source。
- Phase 010 pair-feature batch RVV，已由板卡负向证据拒绝。
- Direct-AoS 或 staging buffer 轻量化，需另开 phase 且不得在采纳收尾中暗改生产行为。

## 当前状态清单

| item | current state |
| --- | --- |
| Phase 040 production patch | 已存在，exact gate 下接入 `computePPFFeatureAlphaMRVV`。 |
| Phase 040 tests | `run_test_compare` fresh 通过：Std 5/5，RVV 6/6。 |
| Phase 040 board | `public_ppf_compute` 5-run speedup `1.35, 1.35, 1.37, 1.39, 1.38`。 |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=2`；suggestions 为 environment metadata 与 binary identity。 |
| doc-rvv | `doc-rvv/features/ppf-RVV.zh.md` 尚未创建，本阶段创建。 |
| screening status | `features-retained-candidate-rescreen.zh.md` 仍写 PPF 未启动，本阶段刷新。 |

## 实现和文档动作

1. 创建 `doc-rvv/features/ppf-RVV.zh.md`，只写用户确认采纳后的 production 长期事实：函数语义、标量路径、当前采用方式、fallback 矩阵、Traceability Map、数值算例、bench 与证据、正确性与高效性证据链和后续方向。
2. 更新 `test-rvv/features/ppf/doc/ppf-evaluation.zh.md`，把 PI5 pending 状态改成 adopted production behavior，并引用正式 `doc-rvv`。
3. 更新 `test-rvv/features/ppf/doc/optimization-roadmap.zh.md` 和 `doc/phases/optimization-matrix.zh.md`，把 production alpha_m batch RVV 改成 adopted；把后续泛型 traits、row source 和 buffer 轻量化列为 separate future phase / not current unblocked action。
4. 更新 `test-rvv/features/ppf/doc/phases/README.zh.md` 和 `README.zh.md`，默认恢复动作改为 production closeout 后 ready for review；删除 PI5 pending 文案。
5. 更新 `doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md` 的 PPF 行和执行清单状态，避免后续 worker 重复启动本 topic。
6. 写本阶段 `result.zh.md`，记录采纳确认、文档归属、doc-suite closeout gate、验证命令和 stop decision。
7. 更新本地 current Handoff，指向 S11 closeout 后状态和可选后续 phase。

## Doc Suite Closeout Gate

| area | required content | planned action |
| --- | --- | --- |
| production_topic_doc | adopted production 行为、当前优化方式、fallback、证据链、后续方向 | 新建 `doc-rvv/features/ppf-RVV.zh.md` |
| evaluation | S2 / S11 决策审计和候选取舍 | 更新 `test-rvv/features/ppf/doc/ppf-evaluation.zh.md` |
| optimization roadmap | 搜索空间与下一阶段条件 | 更新当前 production 行为为 adopted |
| optimization matrix | candidate / evidence 状态 | production 行改成 adopted，后续扩展标为 future separate phase |
| phase suite | Phase 050 plan/result | 新建本阶段 result |
| topic navigation | 阅读路径、命令和证据边界 | 更新 README |
| screening queue | topic 状态 | 更新 retained candidate rescreen 表 |

## 继续 / 停止条件

本阶段完成后，如果 current exact-gated production boundary 下没有无需扩大范围的高优先级优化动作，
则停止在 `ready_for_review / adopted production closeout complete`。后续泛型点类型、其它 row source、
`Scalar=double` 或 evidence metadata hardening 均需要新 phase 或用户选择，不作为本阶段继续动作。

## 验证计划

- `git diff --check -- features/include/pcl/features/impl/ppf.hpp test-rvv/features/ppf doc-rvv/features/ppf-RVV.zh.md doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md`
- `make -C test-rvv/features/ppf run_test_compare`
- `git status --short --untracked-files=all -- features/include/pcl/features/impl/ppf.hpp test-rvv/features/ppf doc-rvv/features/ppf-RVV.zh.md doc-rvv/library-screening/features/features-retained-candidate-rescreen.zh.md`
