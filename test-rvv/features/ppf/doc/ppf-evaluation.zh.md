# PPF 函数级评估

## 范围和目标源码

目标源码是 `features/include/pcl/features/impl/ppf.hpp`，当前公开入口是
`pcl::PPFEstimation<PointInT, PointNT, PointOutT>::computeFeature`。本评估只覆盖 PPF
all-pairs descriptor（所有点对描述子）路径，不覆盖 `PPFRGB`、`CPPF` 或 shared helper
`features/src/ppf.cpp::computePPFPairFeature` 的独立生产接入。

## 函数作用速览

| 函数 / 函数族 | 作用 | 输入 / 输出状态 | 与主流程关系 | RVV 判断 |
| --- | --- | --- | --- | --- |
| `PPFEstimation::computeFeature` | 为 `indices_ × input_` 的每个点对生成 `PPFSignature`。 | 读取 input cloud（输入点云）、normal cloud（法线点云）和 indices，写 `output`。 | 公开入口 `compute()` 的实际计算体。 | 本 topic 的主评估对象。 |
| `pcl::computePairFeatures` | 计算 PFH 风格的 `f1..f4`。 | 读取两个 xyz 点和法线，输出三个角特征和距离。 | 当前 PPF production 代码实际调用它。 | 必须先作为参考链路复刻，不能误换成 `computePPFPairFeature`。 |
| `alpha_m` 构造 | 将 model reference point/normal 旋转到 x 轴附近，再用 `atan2` 生成角度。 | 每个非 identity pair 独立计算，写 `PPFSignature::alpha_m`。 | PPF 特有后段。 | Phase 020 默认先做 formula audit（公式审计），再决定是否写 RVV candidate。 |
| identity pair 分支 | `i == j` 时写五个 NaN，并把 `output.is_dense=false`。 | 不调用 pair helper。 | 输出规模仍包含 identity pair。 | correctness gate 必须覆盖。 |

## 标量流程与 RVV 候选边界

当前标量流程先把 `output` resize 到 `indices_->size() * input_->size()`，然后按 `index_i`
外层、`j` 内层顺序写 `output[index_i * input_->size() + j]`。非 identity pair 调用
`pcl::computePairFeatures` 生成 `f1..f4`，再构造 `Eigen::AngleAxisf` 和 `Eigen::Affine3f`
计算 `alpha_m`。identity pair 和 pair helper 失败都会写 NaN；helper 失败还会打印错误。

当前判断是 `adopted production behavior / S11 production closeout`。
Phase 000 已证明 test-only reference 能复刻当前
production 的输出顺序、identity pair、helper choice 和 `alpha_m` 标量链路。Phase 010 已证明
SoA-staged pair-feature batch RVV 候选 correctness 成立，但板卡 repeated 结果为 negative；后续不应把
该候选直接接入 production。Phase 020 已证明 `alpha_m` closed-form helper 与 Eigen reference 一致，
Phase 030 进一步证明 test-only `alpha_m` batch RVV candidate 在当前诊断边界下 5-run board
positive。Phase 040 已在 exact `PointXYZ + Normal + PPFSignature` gate 下接入真实 public
入口，并用 production-public（生产公开入口）板卡证据重新验证；Phase 050 按用户确认完成
S11 production closeout。Phase 060 继续把 production gate 扩展为 traits-gated（基于字段特征门控）
source xyz AoS + normal AoS + exact `PPFSignature`，并用接入后的 `PointXYZI + Normal`、
`PointXYZ + PointNormal` 板卡数据证明收益。当前正式长期文档为 `doc-rvv/features/ppf-RVV.zh.md`。

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `PPFEstimation::computeFeature` | production public entry | 真实 PPF 公开入口计算体。 | `PPFEstimation::compute` | `pcl::computePairFeatures`、Eigen rotation、output write | production boundary（生产边界） | `features/include/pcl/features/impl/ppf.hpp` |
| `pcl::computePPFPairFeature` | production shared helper | PPF helper，但当前 `impl/ppf.hpp` 没有调用。 | 其它 PPF-family 代码或外部用户 | 无本 topic production caller | helper-choice audit（辅助函数选择审计） | `features/src/ppf.cpp` |
| `include/ppf.h` | test support aggregator | topic-local test / bench 聚合入口。 | `src/test_ppf.cpp` | `include/impl/*` | correctness scaffold（正确性脚手架） | `test-rvv/features/ppf/include/ppf.h` |
| `include/impl/ppf_alpha_candidate.hpp` | candidate formula | Phase 020 的 `alpha_m` closed-form scalar helper 和 Phase 030 的 alpha batch RVV candidate。 | `src/test_ppf.cpp`、`src/bench_ppf.cpp` | `computeAlphaMReference` 对拍、`atan2_RVV_f32m2` | diagnostic formula / component ablation（诊断公式 / 组件消融） | `test-rvv/features/ppf/include/impl/ppf_alpha_candidate.hpp` |
| `include/impl/ppf_pair_batch_candidate.hpp` | candidate formula / staging | Phase 010 的 SoA-staged pair-feature batch RVV 候选。 | `src/test_ppf.cpp`、`src/bench_ppf.cpp` | RVV `computePairFeaturesRVV`、标量 `computeAlphaMReference` | diagnostic / component ablation（诊断 / 组件消融） | `test-rvv/features/ppf/include/impl/ppf_pair_batch_candidate.hpp` |
| `src/test_ppf.cpp` | correctness aggregate | 先建立 reference API 的失败测试，再对拍 production 输出。 | Makefile `run_test_*` | topic-local reference helper | unit / regression gate（单元 / 回归验收） | `test-rvv/features/ppf/src/test_ppf.cpp` |
| `src/bench_ppf.cpp` | bench wrapper | 输出 component reference、Phase 010 candidate、Phase 030 alpha candidate、Phase 040 exact public case 和 Phase 060 point-type expansion public case 的 timing / checksum。 | Makefile `run_bench_*`、`board_repeated` | topic-local reference / candidate、`PPFEstimation::compute` | diagnostic + production-public benchmark（诊断 + 生产公开入口性能测试） | `test-rvv/features/ppf/src/bench_ppf.cpp` |
| `script/generate_ppf_evidence_manifest.py` | analysis script | 把 repeated board summary 转成 Evidence Doctor manifest。 | Makefile `evidence_doctor_repeated` | `test-rvv/script/evidence_doctor.py` | evidence manifest（证据清单） | `test-rvv/features/ppf/script/generate_ppf_evidence_manifest.py` |
| Phase 060 board manifests / Doctor | evidence output summary | 当前 point-type expansion repeated board 结果的 Errors / Warnings / Suggestions。 | Evidence Doctor | evaluation、phase result、Handoff | summary-only evidence（仅摘要证据） | `test-rvv/features/ppf/log/board/phase060-pointxyzi-normal/repeated/evidence_manifest.json`、`test-rvv/features/ppf/log/board/phase060-pointxyz-pointnormal/repeated/evidence_manifest.json` |
| `doc/phases/000-current-state-and-gaps/plan.zh.md` | phase plan | Phase 000 修改前合同。 | worker / reviewer | result、matrix、Handoff | recovery pointer（恢复入口） | `test-rvv/features/ppf/doc/phases/000-current-state-and-gaps/plan.zh.md` |
| `doc/phases/010-pair-feature-and-output-staging-ablation/result.zh.md` | phase result | Phase 010 结果、负向证据和默认恢复动作。 | worker / reviewer | roadmap、matrix、Handoff | decision audit（决策审计） | `test-rvv/features/ppf/doc/phases/010-pair-feature-and-output-staging-ablation/result.zh.md` |
| `doc/phases/020-alpha-m-formula-audit/result.zh.md` | phase result | Phase 020 公式审计和下一阶段入口。 | worker / reviewer | roadmap、matrix、Handoff | formula audit（公式审计） | `test-rvv/features/ppf/doc/phases/020-alpha-m-formula-audit/result.zh.md` |
| `doc/phases/030-alpha-m-rvv-candidate/result.zh.md` | phase result | Phase 030 alpha batch RVV 候选、板卡 positive 证据和 PI1 授权边界。 | worker / reviewer | roadmap、matrix、Handoff | partial-production-candidate audit（局部生产候选审计） | `test-rvv/features/ppf/doc/phases/030-alpha-m-rvv-candidate/result.zh.md` |
| `doc/phases/040-production-alpha-m-rvv-integration/result.zh.md` | phase result | Phase 040 production patch、production direct correctness、asm、board repeated 和 PI5 状态。 | worker / reviewer | roadmap、matrix、Handoff、后续 `doc-rvv` | production-public evidence（生产公开入口证据） | `test-rvv/features/ppf/doc/phases/040-production-alpha-m-rvv-integration/result.zh.md` |
| `doc/phases/050-production-closeout-doc-rvv/result.zh.md` | phase result | Phase 050 用户采纳确认、S11 文档收尾、筛选状态同步和停止判断。 | worker / reviewer | roadmap、matrix、Handoff、正式 `doc-rvv` | production closeout（生产收尾） | `test-rvv/features/ppf/doc/phases/050-production-closeout-doc-rvv/result.zh.md` |
| `doc/phases/060-point-type-expansion/result.zh.md` | phase result | Phase 060 traits-gated 点类型扩展、fallback、asm、板卡 repeated 和停止判断。 | worker / reviewer | roadmap、matrix、Handoff、正式 `doc-rvv` | production point-type expansion（生产点类型扩展） | `test-rvv/features/ppf/doc/phases/060-point-type-expansion/result.zh.md` |
| `ppf-RVV.zh.md` | production topic doc | 已采纳生产行为、当前优化方式、fallback 矩阵、证据链和后续方向。 | S11 closeout | reviewer / future worker | adopted production behavior（已采用生产行为） | `doc-rvv/features/ppf-RVV.zh.md` |

## 测试计划和 bench 计划

| 测试 / bench | 层级 | 作用 |
| --- | --- | --- |
| `PPFReference.ComputesProductionLikeAllPairsOutput` | correctness | 构造小型确定性点云，验证 test-only reference 与 production `PPFEstimation::compute` 输出一致。 |
| `PPFReference.MarksIdentityPairsAsNaNAndNotDense` | boundary/adversarial | 验证 identity pair 的五个字段为 NaN，且 `output.is_dense=false`。 |
| `PPFCandidate.PairFeatureBatchRVVComputesProductionLikeOutput` | correctness | 验证 Phase 010 candidate 与 test-only reference 对拍；失败代表 RVV `f1..f4` 或 `alpha_m` 拼接破坏输出语义。 |
| `PPFAlphaM.ClosedFormMatchesEigenReference` | correctness | 验证 Phase 020 closed-form helper 与 Eigen reference 对拍；失败代表后续 RVV `alpha_m` 公式不应继续。 |
| `PPFCandidate.AlphaMBatchRVVComputesProductionLikeOutput` | correctness | 验证 Phase 030 alpha batch RVV candidate 与 test-only reference 对拍；失败代表 `alpha_m` closed-form RVV 或 output-row staging 破坏输出语义。 |
| `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForExactTypes` | production direct correctness | RVV 构建下验证 public `PPFEstimation::compute` 与 reference 对拍，并通过 test-only trace 证明 exact type 生产分流命中。 |
| `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForPointXYZILikeSource` | production direct correctness | 验证 source 从 exact `PointXYZ` 扩到 `PointXYZI` 后，public `compute()` 真实命中 traits-gated RVV 分流并与 Std helper 对拍。 |
| `PPFProductionDirect.RVVAlphaMPathHitsPublicComputeForPointNormalLikeNormals` | production direct correctness | 验证 normal 从 exact `Normal` 扩到 `PointNormal` 后，public `compute()` 真实命中 traits-gated RVV 分流并与 Std helper 对拍。 |
| `PPFProductionDirect.RVVAlphaMRejectsUnsupportedLayouts` | fallback correctness | 验证非 `PPFSignature` output 和不满足 normal gate 的组合不会命中 RVV helper。 |
| `bench_ppf` | diagnostic / production-public benchmark | 输出 `component_ppf_reference`、`candidate_ppf_pair_feature_batch_rvv`、`candidate_ppf_alpha_m_batch_rvv`、`public_ppf_compute`、`public_ppf_compute_pointxyzi_normal` 和 `public_ppf_compute_pointxyz_pointnormal`。QEMU 只用于 log-shape smoke；三个 `public_ppf_compute*` case 是 production-public case，性能结论来自板卡 repeated。 |

## Phase 010 诊断证据链

| evidence | result | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/features/ppf run_test_compare` 通过 Std/RVV 3 个测试。 | 证明 reference 和 Phase 010 candidate 在当前 `PointXYZ + Normal` / float / AoS 样本上满足误差预算。 |
| QEMU smoke | `make -C test-rvv/features/ppf run_bench_rvv` 小规模运行可输出 bench 形状。 | 只证明构建和日志形状，不证明性能。 |
| asm attribution | `make -C test-rvv/features/ppf dump_bench_rvv` 生成 `bench_ppf_rvv.asm`。 | 证明候选符号和 RVV 指令存在；不是性能证据。 |
| board performance | `board_repeated evidence_doctor_repeated` 5-run，candidate speedup 为 `0.80, 0.79, 0.79, 0.79, 0.81`。 | 证明该 diagnostic 边界下 SoA-staged pair-feature RVV 不值得接入 production。 |
| Evidence Doctor | `Errors=1, Warnings=1, Suggestions=8`，Error 为 candidate 5/5 退化。 | Error 阻止把该候选写成 production-ready；环境和 binary metadata 建议不改变 negative 桶。 |

bench raw log 中 checksum 仍是默认 6 位科学计数法显示，candidate RVV 侧显示为 `6.80557e+11`，
reference / public 侧显示为 `6.80558e+11`。这需要作为显示精度风险保留；当前 correctness 结论
来自逐字段 gtest，而不是该 checksum 字符串。

## Phase 030 诊断证据链

| evidence | result | 结论边界 |
| --- | --- | --- |
| correctness | `make -C test-rvv/features/ppf run_test_compare` 通过 Std/RVV 5 个测试。 | 证明 Phase 030 candidate 在当前 `PointXYZ + Normal` / float / AoS 样本上满足 `2e-3` 误差预算。 |
| QEMU smoke | `run_bench_rvv` 小规模单 case `candidate_ppf_alpha_m_batch_rvv` 运行成功并输出 checksum。 | 只证明构建、case-filter 和日志形状，不证明性能。 |
| asm attribution | `make -C test-rvv/features/ppf dump_bench_rvv` 后，`computePPFAlphaMBatchRVV` 符号范围包含 RVV load / arithmetic / merge / store 指令。 | 证明本阶段 helper 命中 RVV 指令；不是 production dispatch 证据。 |
| board performance | `board_repeated evidence_doctor_repeated` 5-run，alpha candidate speedup 为 `1.57, 1.57, 1.56, 1.57, 1.57`。 | 证明该 diagnostic 边界下 alpha 后段 RVV candidate 值得进入有界生产探针设计。 |
| Evidence Doctor | `Errors=1, Warnings=1, Suggestions=10`；Error 仍属于 Phase 010 pair-feature 候选，alpha candidate 只有环境 metadata / binary identity suggestions。 | 允许把 alpha candidate 写成 `partial-production-candidate`；不允许写成 adopted production behavior。 |

## Phase 040 生产接入证据链

| evidence | result | 结论边界 |
| --- | --- | --- |
| production patch | `features/include/pcl/features/impl/ppf.hpp` 新增 `computePPFFeatureStd` 和 `computePPFFeatureAlphaMRVV`，公开入口在 `__RVV10__` 下先尝试 exact type RVV helper，否则落回 Std。 | 真实 production patch；不改变 public API，不接入 Phase 010 pair-feature RVV。 |
| RED / GREEN correctness | RED：`run_test_rvv` 只有 production-direct trace 断言失败；GREEN：`run_test_compare` Std 5 tests、RVV 6 tests 通过。 | 证明 public `compute()` 在 RVV 构建下真实命中分流并保持当前样本输出语义。 |
| QEMU smoke | `run_bench_rvv` 使用 `--case-filter public_ppf_compute` 小规模运行成功。 | 只证明 public case 可运行和日志形状，不证明性能。 |
| asm attribution | `dump_bench_rvv` 后，生产 helper `computePPFFeatureAlphaMRVV<PointXYZ, Normal, PPFSignature>` 符号范围包含 `vsetvli`、`vle32`、`vfdiv`、`vfsqrt`、`vmerge`、`vfnmsac`、`vfmacc`、`vse32`。 | 证明生产 helper 归属范围内存在 RVV 指令。 |
| board performance | public-only 5-run Milkv-Jupiter：speedup `1.35, 1.35, 1.37, 1.39, 1.38`；mean Std `390.0784 ms`，mean RVV `284.9594 ms`。 | 证明当前 exact gate、当前规模和板卡下 public RVV path 快于 public scalar path。 |
| Evidence Doctor | `Errors=0, Warnings=0, Suggestions=2`，suggestions 为 environment metadata 和 binary identity 缺失。 | 不阻塞 positive bucket；正式归档时可补环境和二进制身份字段。 |

## Phase 060 点类型扩展证据链

| evidence | result | 结论边界 |
| --- | --- | --- |
| production patch | `features/include/pcl/features/impl/ppf.hpp` 的 RVV gate 从 exact `PointXYZ + Normal` 扩为 `RVVXYZAoSFloatLayout<PointInT>` + PPF normal AoS + exact `PPFSignature`，并使用当前模板点型 offset 读取 staging 输入。 | 真实 production patch；公开 API 不变；`f1..f4` 仍使用标量 `computePairFeatures`。 |
| RED / GREEN correctness | RED：新点型 trace tests 在生产修改前命中数为 0；GREEN：`run_test_compare` Std 5/5、RVV 9/9 pass。 | 证明新增代表点型 public `compute()` 真实命中 RVV 分流，并保持当前样本输出语义。 |
| fallback correctness | `PPFProductionDirect.RVVAlphaMRejectsUnsupportedLayouts` pass。 | 非 `PPFSignature` 输出和不满足 normal AoS gate 的组合不会误入 RVV helper。 |
| board correctness | `run_board_test fetch_board_logs` 后 RVV board tests 9/9 pass。 | 证明板卡上 correctness gate 通过；clock-skew 提示只作为环境时间戳风险记录。 |
| QEMU smoke | 两个新 public case 的小规模 QEMU smoke checksum 均为 `213486`。 | 只证明 case-filter 和日志形状，不证明性能。 |
| asm attribution | production helper instances for `PointXYZ/Normal`、`PointXYZI/Normal`、`PointXYZ/PointNormal` all contain RVV instructions。 | 证明扩展后的模板实例可归属到手写 RVV 指令范围。 |
| board performance | `PointXYZI + Normal` speedup `1.40, 1.43, 1.33, 1.41, 1.35`，mean Std `429.5662 ms`、mean RVV `310.3166 ms`；`PointXYZ + PointNormal` speedup `1.33, 1.33, 1.31, 1.33, 1.35`，mean Std `412.4258 ms`、mean RVV `310.2756 ms`。 | 证明两个代表性 traits-gated production-public case 在目标板卡上稳定快于 scalar path。 |
| Evidence Doctor | 两组 repeated report 均为 `Errors=0, Warnings=0, Suggestions=2`；suggestions 为 environment metadata 和 binary identity 缺失。 | 不阻塞 positive bucket；严格归档时可另开 evidence hardening。 |

## 生产接入判断

当前 production decision（生产接入判断）是 `adopted production behavior`。production-public
证据支持保留当前 traits-gated `alpha_m` RVV 生产补丁；Phase 060 已按用户“接入后有收益即可采纳”的
规则完成点类型扩展采纳，并刷新 `doc-rvv/features/ppf-RVV.zh.md`。正式文档中的性能数据使用
Phase 060 接入后的 board repeated 数据；Phase 030 diagnostic helper 数据只保留为生产探针来源。

当前 PPF topic 内没有值得继续推进的高优先级未阻塞优化动作。后续如果要继续，只建议按独立 phase /
topic 处理：严格证据归档的 evidence hardening、profile-driven direct-AoS pair-feature revisit，或
PPFRGB / CPPF 等 caller 的单独评估。
