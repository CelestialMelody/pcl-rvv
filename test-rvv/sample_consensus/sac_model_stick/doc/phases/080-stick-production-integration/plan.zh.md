# Phase 080: stick production integration

## 阶段意图和边界

本阶段把 Phase 010、030 和 050 已完成的 PI1 production integration plan（生产接入计划）合并推进到 PI2-PI5。用户已经授权继续生产接入；本阶段只修改 `SampleConsensusModelStick<PointT>` 的三个距离相关公开入口：

- `countWithinDistance`
- `selectWithinDistance`
- `getDistancesToModel`

本阶段不修改 public API（公开接口）、不触碰 `optimizeModelCoefficients`、`projectPoints`、`doSamplesVerifyModel`，也不把当前证据外推到 `Scalar=double`、非 xyz AoS layout（结构数组布局）或其它 sample_consensus topic。

## validated_scope

| 维度 | 本阶段准备证明 |
| --- | --- |
| row source | `indices_` direct indexed 路径 |
| 点类型 | production gate 使用 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>`；首轮 production direct（直接生产路径证据）仍以 `PointXYZ` 测试和板卡 bench 为证据 |
| Scalar / 系数 | `Eigen::VectorXf` model coefficients；`threshold` / `radius_max_` 按现有源码转成 float 平方阈值 |
| 布局 | PCL traits 注册 x/y/z 单 float，POD standard-layout，`sizeof(PointT)==sizeof(POD)`，字段 offset 和 stride 满足 f32 AoS byte-offset helper |
| 规模 gate | `input_->size() <= pcl::rvv::rvvMaxU32ByteOffsetElements<PointT>()`，保证 32-bit indexed byte offset 可表达 |
| 目标硬件 | board repeated 5-run，默认 Milkv-Jupiter；性能结论只来自板卡 |

## unvalidated_scope

本阶段不证明 `PointXYZI`、`PointXYZRGB/RGBA`、normal 复合点型或自定义 xyz 点型的独立 production 收益；traits gate 允许它们在布局满足时命中 RVV，但首轮 closeout 必须把 `PointXYZ` 写成已测代表点型，把其它点型写入 point_type_expansion_queue（点类型扩展队列）。本阶段不证明空洞点云 / NaN 输入的 dense 边界，也不扩展到 source-indexed、dual-indexed 或 correspondence row source。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | Phase 000/020/040 是 production-shaped diagnostic；Phase 080 目标是 production-public |
| A/B boundary | 诊断阶段是 test helper vs public baseline；本阶段必须重跑 public overload（公开重载）Std/RVV |
| 当前决策问题 | RVV-vs-scalar production adoption（生产采纳） |
| diagnostic 是否可外推到 production | 只能作为 bounded production probe（有界生产探针）启动理由，不能作为采纳证据 |
| comparison-boundary / baseline mismatch 风险 | 有：旧 public 行未接 RVV，Phase 080 必须用生产补丁后 public 行重跑 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 已有 diagnostic 是 positive-stable；若生产 direct 变弱或不稳定，按 PI5 暂停 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | 当前没有已采纳 stick RVV family，用户明确认可 post-integration board 正收益即可采纳；无需 RVV-vs-RVV family selection |

## 实现和测试动作

| action | 产物 / 命令 | 完成判据 |
| --- | --- | --- |
| RED production asm gate | `script/check_stick_production_asm.py`、`make check_production_asm` | 当前未接 production 时失败，说明测试能捕捉缺失生产 RVV helper |
| PI2 production patch | `sample_consensus/include/pcl/sample_consensus/sac_model_stick.h/.hpp` | 公开入口为 model valid check -> RVV short-circuit -> Std fallback；原标量主体抽成 Std helper |
| PI3 production direct correctness | `make run_test_compare`、细分 alias | Std/RVV gtest 均通过，覆盖 count/select/getDistances 语义和 fallback |
| PI4 asm attribution | `make dump_bench_rvv && make check_production_asm` | 三个 production RVV helper 或真实内联边界可归属 RVV 指令 |
| PI4 board repeated | `SSH_AUTH_SOCK=/run/user/$(id -u)/keyring/ssh make collect_production_repeated_board_evidence` | 5-run summary 生成，Evidence Doctor Errors=0 或已解释降级 |
| PI5 EvidenceDecision | phase result、matrix、roadmap、evaluation、适用 `doc-rvv` | 若生产 public board speedup 为 positive，则按用户口径采纳并用接入后数据写正式 `doc-rvv` |

## Evidence Doctor 和 registry

本阶段新增 production manifest / doctor / registry 目标，不覆盖 Phase 000/020/040 的 diagnostic manifest。若 Evidence Doctor 出现未解决 Error，不能写 production adopted。若只有环境字段缺失或 metadata 不完整 Warning，必须在 result 和长期文档解释风险边界。

## 板卡复跑预算和决策桶

本阶段预算为 5-run repeated board；若 direction、decision bucket 或 doctor 严重级别不稳定，最多补一次同边界确认复跑。`positive` 表示三个 production public case 的 median 和 min 均大于 1.0；若某入口低于 1.0，则该入口不得采纳，保留 patch 等用户判断或按授权回滚。

## point_type_expansion_queue

| phase | 范围 | 恢复条件 | 所需证据 |
| --- | --- | --- | --- |
| `090-stick-point-type-expansion` | `PointXYZI` / RGB / RGBA / normal-like xyz AoS | Phase 080 production public `PointXYZ` 采纳后 | dedicated correctness、fallback、asm、board repeated、Evidence Doctor |

## 继续 / 停止条件

若 production direct board 为 positive，本阶段继续完成 production closeout、创建 `doc-rvv/sample_consensus/sac_model_stick-RVV.zh.md` 并同步 evaluation / roadmap / matrix / queue。若 correctness、asm、board 或 doctor 失败，停止在 PI5 user checkpoint，保留 patch 并报告可复现命令；未经用户授权不回滚。
