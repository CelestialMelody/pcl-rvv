# extract_polygonal_prism_data Optimization Roadmap

## 当前边界

当前 topic 已完成 Phase 070 size threshold tuning（规模阈值调优）。Phase 040 已把 full-scan single polygon RVV 路径接入真实 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)`，Phase 045 根据用户确认把该 production patch（生产补丁）记录为 adopted production behavior（已采用生产行为），Phase 050 把合法 multi polygon XOR 纳入同一 RVV production path，Phase 060 又把 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 纳入当前已采纳点型范围。Phase 070 把 production RVV work item count 阈值从 64 降到 32。production public（公开入口生产证据）single polygon dense / indexed repeated board（重复板卡性能测试）为 positive bucket：两组 median 均为 1.75x；nested polygon dense median 2.18x；nested polygon indexed median 2.13x；post-gate point-type repeated board 中 `PointXYZI` median 1.85x、`PointXYZRGB` median 1.83x、`PointXYZRGBA` median 1.84x；32 点 threshold confirmation（阈值确认）median 1.19x、min 1.18x、Evidence Doctor（证据体检）0 / 0 / 0。`PointXYZINormal` 当前明确回退标量。

当前 production patch（生产补丁）覆盖 `RVVXYZAoSFloatLayout<PointT>`、`sizeof(PointT) <= 32`、合法 dense ordered indices 或 indexed gather、`indices_->size() >= 32`、合法 single / nested polygon。plane setup 和 `projectPoints` 保持标量但计入 production bench。`PointXYZINormal`、更宽 AoS stride、非 float xyz layout、`indices_->size() < 32` 和退化 / 非法 polygon 保持标量。

## 默认恢复队列

| 顺位 | next phase / action | 状态 | 恢复条件 |
| --- | --- | --- | --- |
| 1 | S11 production closeout | done | `doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md` 已创建，筛选队列已刷新 |
| 2 | concave hull XOR vectorization | done | Phase 050 nested dense / indexed production public 证据 positive，已接入 |
| 3 | point type expansion | done | Phase 060 已采纳 `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`；`PointXYZINormal` 已回退 |
| 4 | size threshold tuning | done | Phase 070 已采纳 `indices_->size() >= 32`，confirm5 median 1.19x、min 1.18x、doctor 0 / 0 / 0 |
| 5 | closeout submit audit | done | Phase 080 已补齐 doc-suite parity（文档套件对齐）审计和提交流程边界 |
| 6 | current topic auto-continue | turn_stop_deferred with stop_condition_hit | 当前 topic 内剩余方向会扩大到 wide-stride 专项、`projectPoints` 组件或自定义点型 / `Scalar` 边界；默认进入 commit flow |

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| RVV full-scan edge-parity production | Phase 010 / 030 diagnostic、Phase 040 production public、Phase 045 closeout、Phase 050 multi polygon | full-scan polygon scan after projection setup，dense / indexed row source，single / nested polygon | single polygon dense / indexed median 1.75x；nested dense median 2.18x；nested indexed median 2.13x | 只覆盖有界 gate，不覆盖全部点型性能 | 已有 correctness、asm、board、doctor；正式 `doc-rvv` 已刷新 | adopted | none |
| arbitrary indices gather | production `indices_` 不保证 dense ordered | indexed scan | diagnostic median 2.20x，production public median 1.75x | gather 局部收益被完整 public entry 稀释 | 已有 correctness、asm、board、doctor；已写入长期文档 | adopted within production scope | none |
| concave hull XOR vectorization | 队列要求覆盖 `test_concave_prism` two-rings XOR；Phase 050 production probe | multiple polygons | nested dense median 2.18x；nested indexed median 2.13x | 只覆盖合法 polygon；退化 polygon fallback | red/green correctness、bench、asm、board、Evidence Doctor 已完成 | adopted | none |
| point type expansion | 泛型点类型 gate 策略 | `PointXYZI`、`PointXYZRGB`、`PointXYZRGBA`；`PointXYZINormal` fallback | 扩大 production RVV 覆盖面 | 宽 stride 点型当前不稳定 | post-gate representative point types、QEMU、board、doctor | adopted partial / wide-stride rejected | none |
| size threshold tuning | Phase 040 使用 `indices_->size() >= 64` | small / medium clouds | 32 点 confirm5 仍 positive，扩大小规模可命中范围 | 阈值依赖 VLEN、polygon 边数、projection 前置成本；其它点型小规模未单独重复 | board sweep、doctor、fallback correctness | adopted | none |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| 000 | 先做 full-segment profiling / component audit，再决定是否接 production | post-projection scan 很快，但真实入口还包含 plane fitting 和 `projectPoints` | 完整 `segment` shaped bench 或 scoped production-shaped wrapper | done by Phase 040 |
| 010 | 生产补丁应优先接扫描段 helper，而不是重写 `projectPoints` | full-scan diagnostic 已覆盖平面距离和投影坐标选择，但仍不含投影生成成本 | PI1 fallback / dispatch plan；production direct bench | done by Phase 040 |
| 020 | clean split 是推荐生产形态 | production 入口标量主体较长，直接在 `segment` 中插 RVV 会让 fallback 难审 | 用户授权修改类声明头和 impl 头；PI2-PI5 生产证据 | done by Phase 040 |
| 030 | indexed gather 不应阻塞 dense production probe | indexed diagnostic 在板卡恢复后 positive | production public indexed repeated summary | done by Phase 040 |
| 040 | PI5 后先等用户确认，再写正式长期文档 | production public 证据支持采纳，但 workflow 把 PI5 定义为用户检查点 | 用户确认采纳或要求回滚 | done by Phase 045 |
| 045 | concave hull XOR 是下一个更像“优化”的扩展方向 | 当前 adopted 路径仍让真实多 polygon 输入 fallback；diagnostic helper 已有多 polygon 内部结构可借鉴，但 production gate 未放开 | production direct correctness、bench、asm、board、doctor | high |
| 050 | point type expansion 成为默认下一阶段 | 多 polygon XOR 已接入且证据正向；剩余最大覆盖缺口是代表性 PointXYZ-like AoS float 点型还没有独立性能证据 | `PointXYZRGB` / `PointXYZRGBA` / `PointXYZINormal` production direct correctness、asm、board、doctor | medium |
| 060 | size threshold tuning 成为默认下一阶段 | 三个 <=32-byte 点型已正向采纳，宽 stride 已回退；剩余可在当前 topic 内推进的是规模阈值是否能降低 | 32 / 48 / 64 / 96 / 128 / 256 等规模 production board sweep、doctor、必要时阈值候选生产补丁 | medium |
| 070 | 暂停当前 topic 的自动优化循环 | 32 阈值已用 production public confirm5 采纳；剩余方向不是本扫描段内的低风险增量 | wide-stride 要 dedicated implementation family；`projectPoints` 要另开 component ablation；自定义点型 / `Scalar=double` 要新 layout / 数值证据 | stop |
| 080 | 结束当前 topic 并进入提交流程 | 文档套件、生产分发和 artifact tracking 已完成提交前审计 | final verification、topic commit、summary evidence commit | stop |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| 立即创建正式 `doc-rvv` 长期主题文档 | 已完成，不再暂缓 | not_applicable |
| 继续扩大 production patch 的规模阈值 | Phase 070 已把阈值降到 32；继续强行低于 32 缺少当前证据且小规模收益更易被开销吞掉 | 只有真实用户 workload 大量落在 17-31 点且板卡 evidence positive 时再重开 |
| 泛型点类型全部直接采纳 | `PointXYZINormal` 的宽 stride 证据不稳定，当前 gate 显式回退；用户自定义点型仍无专门证据 | 若将来要覆盖宽 stride 或用户点型，需要新 wide-stride / custom-point phase |
| `projectPoints` RVV 化 | 当前 topic 的 adopted 路径只接管扫描段；`projectPoints` 属于 `SampleConsensusModelPlane` 组件，影响范围越过本文件扫描段 | 另开 component ablation topic，先证明 `projectPoints` 是当前 workload 的主成本 |
