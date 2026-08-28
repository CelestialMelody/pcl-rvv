# sac_model_normal_sphere 优化路线图

## 当前边界

本 topic 已完成 `SampleConsensusModelNormalSphere<PointT, PointNT>` 三条公开距离入口的 production RVV
（生产 RVV）接入：`selectWithinDistance`、`countWithinDistance` 和 `getDistancesToModel`。当前采纳范围是
direct indexed `indices_`、`PointXYZ` / `PointXYZI` / `PointXYZRGB` / `PointXYZRGBA` source 点型、
独立 `pcl::Normal` normal cloud、float xyz / normal AoS（结构数组）布局、signed 32-bit `pcl::index_t`、
u32 byte offset（32 位字节偏移）规模 gate 和 `indices_.size() >= 16`。

Phase060 生产证据已经闭合：四种 source 点型 × 三入口的接入后 5-run board repeated（重复板卡测试）
全部 positive，Evidence Doctor（证据体检）为 0/0/0。正式长期文档为
`doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| production RVV count early-mask | sphere shell + normal-plane angle | `countWithinDistance`，四种 source 点型 + `pcl::Normal` | RVV 同时承担 xyz/normal gather、sqrt、angle 和 mask count。 | `n_dir` 每点归一化成本高，需生产边界复核。 | production direct correctness、asm、board repeated、Doctor | adopted | 060-production-integration-execution |
| production RVV select `vcompress` writeback | sphere adopted family + Phase010 ablation | `selectWithinDistance` | 压缩写回 inliers 和 distance，降低标量 lane 输出成本。 | 输出顺序和 error 写回必须保持。 | select order correctness、asm、board repeated、Doctor | adopted | 060-production-integration-execution |
| production RVV getDistances dense double store | line/circle/stick full-RVV store | `getDistancesToModel` | `vfsqrt`、normal angle 和 double store 留在 RVV chunk 内。 | 无 early continue，成本需生产证据证明。 | dense output correctness、asm、board repeated、Doctor | adopted | 060-production-integration-execution |
| topic-local doc suite | workflow quality gate | README、testing overview、correctness、bench/evidence、optimization evidence、code map、phase suite、`doc-rvv` | 让 reviewer 和下一轮短 prompt 不依赖聊天上下文恢复证据边界。 | 文档引用必须和 registry / artifact tracking 对齐。 | role docs、Phase060 result、evidence_status、git status scan | adopted | 040 / 060 |
| other normal point types | future user scope | non-`pcl::Normal` normal-like point types | 扩大模板覆盖。 | normal field traits、layout、fallback 和板卡收益均未证明。 | dedicated correctness、fallback、asm、board repeated、Doctor | deferred outside current scope | user-specified expansion phase |
| custom registered source point types | future user scope | custom PointXYZ-like source layouts | 扩大泛型实用范围。 | 字段 offset、POD/standard-layout、alignment 和 workload 价值未知。 | traits/layout audit、fallback、board repeated | deferred outside current scope | user-specified follow-up |
| identity-index specialized path | future profile scope | workloads where `indices_` mostly identity | 可能降低 gather 成本。 | 多个 sibling 中 identity specialization 经常弱或负；当前 shuffled gather 已强正向。 | production RVV-vs-RVV A/B、profile、Doctor | not_now | profile-triggered follow-up |
| `Scalar=double` or non-float fields | future API / numeric scope | double coefficients or non-float point fields | 可能服务不同 API。 | 当前入口和 point fields 是 float，需新数值策略。 | generic strategy、numeric budget、correctness、board | not_applicable now | upstream/API change only |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | 独立 normal-sphere topic scaffold | `quadric_models` 只覆盖 RANSAC smoke，不能承载候选 bench 和证据边界。 | README、evaluation、phase plan/result、matrix、QEMU/board 输出。 | completed |
| 010 | `vcompress` select 写回消融 | Phase000 select scalar writeback positive，但输出仍经标量 lane append。 | TDD correctness、同 RVV build bench label、`vcompress.vm` 反汇编、board summary、Doctor。 | completed |
| 020 | `getDistancesToModel` dense-store 审计 | Phase000 附带结果显示 dense-store 可能值得独立评估。 | correctness、asm、board smoke、Doctor。 | completed |
| 030 | RGB/RGBA source layout 扩展 | production gate 不能只靠 `PointXYZ/PointXYZI` 外推。 | layout correctness、board smoke、Doctor。 | completed |
| 040 | topic-local doc suite | 多阶段证据、board summary 和 Doctor 超过单 README 可承载范围。 | role docs、artifact tracking、evidence_status。 | completed |
| 050 | PI1 计划冻结生产边界 | diagnostic 支持 bounded production probe（有界生产探针），但不能直接采纳。 | production scope、fallback、PI2-PI5 命令和暂停条件。 | completed |
| 060 | production integration execution | 用户偏好允许接入后板卡有收益即可采纳。 | production patch、correctness、asm、5-run board repeated、Doctor、`doc-rvv` closeout。 | completed |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| 从基础 sphere 外推 select/count | normal angle 和 `n_dir` 归一化是 normal-sphere 的新增成本与误差边界。当前已经用本 topic 生产证据替代外推。 | 不需要恢复；只保留为历史筛选理由。 |
| 其它 normal-like 点型泛化 | 当前 production gate 明确收窄到 `pcl::Normal`，没有证明其它 normal 点型字段布局、fallback 和板卡收益。 | 用户指定 normal 点型或真实 workload 后，另开 point-type expansion phase。 |
| 自定义 registered point type 泛化 | 当前只验证 PCL 内置 `PointXYZ*` source layout。自定义点型的字段注册、offset、POD / standard-layout 和收益均未知。 | 用户指定自定义点型范围，或 profile 显示真实 workload 需要。 |
| identity-index 专门路径 | 当前 shuffled indexed gather 已有 2.9x-4.5x 生产收益，没有证据显示 identity specialization 能进一步改进。 | 真实 workload/profile 显示 identity indices 主导后，做同一 production boundary 的 RVV-vs-RVV A/B。 |
| 继续优化 `computeNormalSphereDistanceRVV` 的 ILP / LMUL | 当前 m2 路径已经在三入口中稳定正向；盲目调整 LMUL、unroll 或 approximation 会增加寄存器压力和数值风险。 | 有 profile、asm spill 信号或特定板卡架构需求后再做 narrow A/B。 |

## 默认恢复动作

当前默认恢复动作是 reviewer / 用户检查 Phase060 closeout，而不是继续追加优化。理由是：已授权 scope 内的
production patch、correctness、asm、board repeated、Evidence Doctor、registry、topic-local docs、matrix、roadmap
和正式 `doc-rvv` 已闭合；剩余方向都会扩大到新点型、新 workload、新硬件或新实现族比较。

若用户明确要求继续当前 topic，推荐先选择一个新 scope：

- normal 点型扩展：从一个明确 normal-like 点型开始，补 traits / layout、fallback、correctness、asm、board repeated 和 Doctor。
- 自定义 source 点型扩展：用户提供点型或 workload 后再评估。
- identity-index 专门路径：只有 profile 显示 identity indices 主导时才做 RVV-vs-RVV A/B。
- ILP / LMUL 微调：只有 profile 或 spill 证据指向当前 kernel 仍是瓶颈时再做。
