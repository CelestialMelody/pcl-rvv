# 030 production-patch-and-direct-evidence 计划

## 阶段意图和边界

本阶段在 020 的 PI1 production integration plan（生产接入计划）基础上继续 PI2-PI5：写最小 production patch（生产补丁），补 production direct（真实生产入口直连）测试与 benchmark（性能测试），并在板卡上重跑公开入口证据。用户已确认“板卡上的测试结果如果显示有收益即可采纳”，因此 020 的类声明头授权 blocker 解除；本阶段仍在 PI5 输出最终采纳检查点，不能把 production patch 自动视为 adopted production behavior（已采用生产行为）。

本阶段覆盖的 production 入口是 `ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices& ground)`。不改变 public API（公开接口），不修改其它 segmentation topic，不提交 commit，不提交 raw logs（原始日志）。

## 当前状态清单

| 项 | 当前状态 | 证据 |
| --- | --- | --- |
| production 源码 | `extract` 仍是原标量主体，没有 RVV dispatch（分流逻辑） | `segmentation/include/pcl/segmentation/impl/approximate_progressive_morphological_filter.hpp` |
| 诊断 correctness（正确性） | Std/RVV 5 个 component/full diagnostic 测试通过 | `make -C test-rvv/segmentation/approximate_progressive_morphological_filter run_test_compare` |
| production-shaped diagnostic（生产形态诊断） | full pipeline 5-run board median 1.31x，min 1.30x，max 1.31x | `log/board/full-pipeline-repeated/summary.md` |
| Evidence Doctor（证据体检） | full-only manifest Errors=0 / Warnings=0 / Suggestions=0 | `log/board/full-pipeline-repeated/evidence_doctor.md` |
| 020 决策 | `partial-production-candidate / PI2-blocked-on-user-authorization` | `doc/phases/020-production-integration-plan/result.zh.md` |

## validated_scope

本阶段准备证明：

- public entry：`ApproximateProgressiveMorphologicalFilter<PointT>::extract(Indices&)`。
- row source：完整 input cloud 建 grid z-min；当前 ground indices 向量做 tail threshold。
- 点类型 / layout：`PointT` 满足 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 的 xyz 单 float AoS gate；production direct correctness 至少覆盖 `PointXYZ` 与 `PointXYZI`。
- `Scalar`：当前 APMF 实现使用 `float` grid / threshold，`Scalar=double` 不适用。
- 规模：RVV stage work item count 至少 64，且 `input_->size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()`。
- 生产形态：RVV 接管 grid z-min 与 tail threshold；window open 先保留标量，避免把单独中性 / 负向 window-open RVV 组件接入 production。

## unvalidated_scope

- `PointXYZRGB/RGBA`、`PointXYZINormal` 和用户自定义 xyz 点型只由 traits gate 编译覆盖；本阶段不承诺每个点型都有独立 board bench。
- 多线程 OpenMP 与 RVV 组合不作为独立收益结论；production direct bench 使用 `setNumberOfThreads(1)` 的单线程边界。
- organized cloud 特殊语义、异常 `indices_` 内容和大于 32-bit byte offset 表达范围的输入保持标量 fallback。
- window-open RVV 不采纳；若 production direct 收益弱化，再恢复 `hybrid-full-diagnostic` / window-open A/B。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 本阶段生成 `production-public` 证据；phase 010 只作为 `production-shaped diagnostic` 前置信号。 |
| A/B boundary | public overload（公开入口）上的 Std/RVV 两个二进制对比；额外保留 production-only asm gate。 |
| 当前决策问题 | `RVV-vs-scalar`：真实 public RVV path 是否快于真实 public scalar path，并保持输出一致。 |
| diagnostic 是否可外推到 production | 只能外推“值得做 bounded production probe（有界生产探针）”；最终性能以本阶段 public entry board 结果为准。 |
| comparison-boundary / baseline mismatch 风险 | 有。诊断 helper 不含 `copyPointCloud`，且 window-open 采用 test-only RVV；本阶段改为真实 public entry bench 并保留 window-open 标量来消除主要错配。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | phase 010 full-pipeline 为 positive，因此允许；若本阶段 production direct 变成 neutral / negative，则停在 PI5 等用户确认回滚或继续 hybrid A/B。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前不是新旧 RVV family selection（实现族选择），而是 public Std/RVV 接入判断；不需要 RVV-vs-RVV A/B。若后续比较 window-open RVV 与标量 window-open hybrid，则另建 detail A/B phase。 |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| 写 production-only RED gate | `src/bench_apmf_production.cpp` 与 Makefile `check_production_rvv_asm` | production patch 前该 target 因找不到 `apmfExtractRVV` 或 RVV 指令而失败 |
| 补 public entry correctness | `src/test_apmf.cpp` 增加 public `extract` dense / non-dense / small fallback / `PointXYZI` 对拍 | Std/RVV `run_test_compare` 都通过 |
| PI2 production patch | `approximate_progressive_morphological_filter.hpp` | `extract` 变成 init/deinit + RVV short-circuit + Std fallback；非 RVV 构建保持标量 |
| PI3/PI4 本地证据 | `run_test_compare`、`check_production_rvv_asm`、必要 QEMU smoke | correctness 与路径命中闭合；QEMU timing 不写性能结论 |
| PI4 板卡证据 | 5-run production public bench，输出到 `log/board/production-public-repeated/` | summary / manifest / Evidence Doctor 可解析且 checksum 一致 |
| PI5 决策 | phase result、matrix、roadmap、evaluation、queue | production direct speedup 决定停在 `pending_user_confirmation_adopt_production` 或 `pending_user_confirmation_rollback` |

## fallback 矩阵

| gate | fallback 行为 | 证据 |
| --- | --- | --- |
| 非 `__RVV10__` 构建 | `extractStd` 完整标量路径 | `run_test_std` |
| traits / layout 不满足 | `apmfExtractRVV` 返回 false，入口调用 `extractStd` | 编译审计 + `PointXYZI` traits positive；不满足点型保留编译 fallback |
| work item count < 64 | RVV helper 返回 false 或 tail 单轮回退标量 | small public test |
| 32-bit byte offset 超界 | RVV helper 返回 false | 源码 gate 审计 |
| non-dense / invalid 点 | grid z-min RVV 使用 finite mask；初始 ground 仍过滤 invalid | non-dense public correctness test |
| window-open RVV | 本阶段不接入；继续使用标量窗口开运算 | production diff + board public bench |

## Evidence Doctor 和 registry 规则

生产板卡 repeated summary 生成后，使用 topic-local manifest wrapper：

```bash
make -C test-rvv/segmentation/approximate_progressive_morphological_filter \
  OUTPUT_DIR_BOARD=log/board/production-public-repeated \
  APMF_REPEATED_DIR=log/board/production-public-repeated \
  APMF_REPEATED_SUMMARY=log/board/production-public-repeated/summary.md \
  APMF_EVIDENCE_MANIFEST=log/board/production-public-repeated/evidence_manifest.json \
  APMF_EVIDENCE_DOCTOR_MD=log/board/production-public-repeated/evidence_doctor.md \
  APMF_EVIDENCE_INCLUDE_REGEX='production public' \
  run_board_evidence_doctor
```

预期 Errors=0。若出现 checksum mismatch、public boundary metadata 缺失或 asm 归属不闭合，当前 production evidence 降级，不进入采纳判断。Warnings 必须在 result 中解释或转成下一阶段动作。

## 板卡复跑预算和决策桶

- 初始预算：5 run，参数 `--size 262144 --half 4 --iterations 8 --warmup 2`，单线程 public entry。
- 最大自动加跑：如果 5-run bucket 摇摆，只追加 5 run；若仍摇摆，标为 `unstable` 并停在 PI5。
- decision bucket：median >= 1.20x 为 positive；1.05x 到 1.20x 为 weak-positive；0.95x 到 1.05x 为 neutral；< 0.95x 为 negative。
- 采纳建议：positive 可建议保留；weak-positive 只有在 diff 小、fallback 简单、doctor clean 时建议保留；neutral/negative 建议回滚或恢复 hybrid A/B。

## 文档更新清单

- 本阶段完成后更新 `doc/phases/030-production-patch-and-direct-evidence/result.zh.md`。
- 更新 `doc/phases/README.zh.md`、`doc/phases/optimization-matrix.zh.md`、`doc/optimization-roadmap.zh.md`。
- 更新 `doc/approximate_progressive_morphological_filter-evaluation.zh.md` 和 segmentation queue。
- 若 PI5 production public 证据 positive / weak-positive 且用户检查确认采纳，创建 `doc-rvv/segmentation/approximate_progressive_morphological_filter-RVV.zh.md`；文档数据必须使用本阶段接入后的板卡证据。

## continue / stop 条件

本阶段默认持续推进到 PI5。只有以下情况允许提前停止：

- production direct correctness 失败且无法在本阶段修复。
- production-only asm gate 无法归属到 RVV helper。
- 板卡不可达或工具链失败。
- Evidence Doctor Error 无法消除。
- production direct 结果与诊断证据矛盾，需要用户在“回滚 / 继续 hybrid A/B / 接受风险”之间判断。

## next_phase_default

默认进入 PI2：先写 production-only RED gate，再实现 clean split production patch。
