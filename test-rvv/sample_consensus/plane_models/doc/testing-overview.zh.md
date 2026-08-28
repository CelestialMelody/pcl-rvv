# plane_models normal-plane 测试总览

本文是 testing overview（测试总览）role。它说明当前 topic 的 Make target、QEMU（仿真器）/ board（板卡）边界、target 粒度审计和覆盖矩阵；每个 GTest 的语义见 `correctness-tests.zh.md`，bench 与 Evidence Doctor（证据体检）细节见 `benchmark-and-evidence.zh.md`。

## 运行入口分类

| target / 入口 | 类别 | 证明范围 | 不能证明什么 |
| --- | --- | --- | --- |
| `run_normal_plane_public_tests` | correctness alias（正确性别名） | phase 000 / 040 / 060 / 070 新增的公开入口 RVV 命中、代表性 AoS source 点型、代表性 normal layout、source × normal 交叉组合、unsupported normal/source layout fallback 和 helper 空缓冲区 resize。 | 不覆盖完整 plane_models 历史回归，也不提供性能结论。 |
| `run_test_rvv` | RVV build correctness aggregate（RVV 构建正确性汇总） | RVV 构建下完整 32 个 GTest 通过。 | 不证明 Std/RVV 同一批输入都通过，也不证明板卡性能。 |
| `run_test_std` | Std build correctness aggregate（标量构建正确性汇总） | 标量构建下完整 32 个 GTest 通过。 | 不命中 RVV helper。 |
| `run_test_compare` | QEMU correctness compare（QEMU 正确性对比） | Std/RVV 两个构建都能通过完整测试，日志形状可复核。 | QEMU timing（QEMU 计时）不是性能证据。 |
| `dump_bench_rvv` | asm attribution（反汇编归属） | `selectWithinDistanceRVV`、`countWithinDistanceRVV`、`getDistancesToModelRVV` 符号内存在 RVV 指令。 | 不证明性能，也不证明公开入口 dispatch。 |
| `run_board_test fetch_board_logs` | board correctness（板卡正确性） | 历史板卡完整 GTest 通过；phase 070 当前只复跑 public alias 13/13，默认使用板卡侧 PCD fixture。 | 测试内打印的计时块只作为辅助，不替代 board compare summary。 |
| `run_board_bench_compare fetch_board_logs` | board helper benchmark（板卡 helper 性能测试） | protected helper hot path 的 Std/RVV 同边界对比。 | 这是 production-shaped diagnostic（生产形态诊断），公开入口 dispatch 由 unit test 覆盖。 |
| `run_board_bench_compare_repeated` / `record_repeated_board_evidence_state` | board repeated summary（重复板卡摘要） | 5-run protected helper hot path 对比、repeated manifest、Evidence Doctor 和 registry freshness。 | 不证明泛型点类型或新的 RVV 实现族选择。 |
| `run_board_bench_compare_phase050` / `record_phase050_evidence_state` | representative source repeated summary（代表性 source 重复板卡摘要） | `PointXYZ`、`PointXYZI`、`PointXYZINormal` 三种 source 点型的 protected helper hot path 对比。 | 不证明其它 source 点型、其它 normal layout 或完整公开入口计时性能。 |
| `run_board_normal_plane_public_tests` | board correctness alias | 板卡上只跑 phase 000 / 040 / 060 / 070 的 13 个公开入口 / fallback / buffer contract 测试。 | 不覆盖完整测试矩阵或性能。 |
| `run_bench_load` | historical probe（历史探针） | 比较 `vluxei32` 与 `vluxseg3ei32` 的 RVV load strategy（加载策略）。 | 不作为当前 production family 选择证据，默认不跑。 |

## Target 粒度审计

| target 类别 | 当前状态 | 证据 / 缺口 | 下一步 |
| --- | --- | --- | --- |
| correctness aggregate | adopted | `run_test_compare` 覆盖 Std/RVV 各 32 tests。 | 保持。 |
| correctness aliases | adopted | `run_normal_plane_public_tests` 和 `run_board_normal_plane_public_tests` 隔离 phase 000 / 040 / 060 / 070 的 13 个 public / fallback / buffer 用例。 | 若继续扩大点型全集，再按新 phase 补别名。 |
| bench diagnostic aliases | adopted | `run_board_bench_compare` 对三条 helper 输出统一表。 | 若新增 RVV-vs-RVV A/B，再补独立 case-filter。 |
| QEMU smoke aliases | adopted | `run_test_compare` 用于 correctness；共享规则阻止 QEMU bench compare 被误用为性能证据。 | 保持。 |
| board smoke aliases | adopted | `board_smoke` 串联 board test、bench compare 和 fetch logs；phase 010 修正 remote PCD 参数。 | 保持。 |
| board repeated aliases | adopted | `run_board_bench_compare_repeated`、`analyze_board_bench_compare_repeated`、`run_repeated_board_evidence_doctor` 已接入并完成 5-run summary；Phase 050 另有 `run_board_bench_compare_phase050`。 | 提交或复跑前运行 `repeated_evidence_status` 和 `phase050_evidence_status`。 |
| doctor / registry aliases | adopted | `generate_board_evidence_manifest`、`run_board_evidence_doctor`、`record_board_evidence_state`、`evidence_status`、`record_repeated_board_evidence_state`、`repeated_evidence_status`、`record_phase050_evidence_state` 和 `phase050_evidence_status` 已接入；registry check 输出 fresh。 | 保持。 |
| historical probe guarded aliases | adopted with narrow scope | `run_bench_load` 需要显式调用，且文档标成历史加载策略探针。 | 不纳入默认 closeout 命令。 |

## 测试流程

推荐验证顺序：

1. `run_normal_plane_public_tests`：先确认新增公开入口和 fallback 用例。
2. `run_test_compare`：确认 Std/RVV 完整 correctness。
3. `dump_bench_rvv`：确认目标 RVV helper 有反汇编归属。
4. `run_board_test fetch_board_logs`：确认板卡完整 GTest。
5. `run_board_bench_compare fetch_board_logs`：刷新 helper hot-path board summary。
6. `run_board_bench_compare_repeated`：采集 5-run repeated summary 输入。
7. `record_repeated_board_evidence_state`：生成 repeated summary、manifest、doctor 并登记 summary artifact（摘要证据产物）。
8. `repeated_evidence_status`：检查 repeated summary、manifest、doctor 和 doc refs 是否 fresh。
9. `run_board_bench_compare_phase050`：显式采集代表性 AoS source 点型性能矩阵。
10. `record_phase050_evidence_state` 和 `phase050_evidence_status`：生成 Phase 050 manifest / Evidence Doctor 并检查 freshness。
11. Phase 060 / 070 以后若只改 correctness / fallback 测试，不新增性能 summary；继续运行三组 freshness target 以确认 Phase 000/030/050 证据未过期。

## 输入数据和覆盖矩阵

| 维度 | 当前覆盖 | 未覆盖范围 |
| --- | --- | --- |
| point type | `PointXYZ + Normal`；`PointXYZI + Normal`、`PointXYZINormal + Normal` 代表性 source correctness 和 protected helper performance；`PointXYZ + PointNormal`、`PointXYZ + PointXYZINormal` 代表性 normal layout correctness；`PointXYZI` / `PointXYZINormal` source 与 `PointNormal` / `PointXYZINormal` normal cloud 的 4 个代表性交叉 correctness；`NormalWithDoubleCurvature`、non-AoS registered xyz source 和 non-AoS registered normal fallback case。 | 更多 PCL xyz AoS source 点型、其它 `PointNT` normal-like layout、自定义 float curvature layout、泛型 normal traits。 |
| row source | `indices_` 顺序索引表，bench 使用全量 ordered indices。 | 非法索引、跨对象 correspondences（对应关系）不属于该模型。 |
| Scalar / 算术 | API 使用 `double threshold` 和 `std::vector<double>` 输出；RVV 内部用 float 计算后写回 double。 | `Scalar=double` RVV 计算族未实现。 |
| layout | source 侧 `RVVXYZAoSFloatLayout<PointT>`；normal 侧 registered single-float normal + curvature 且字段 offset / stride 满足 byte-offset helper 前提。 | 不满足 source AoS、normal/curvature field 或 32-bit byte offset 上界时回退标量。 |
| production direct | 公开入口测试覆盖 dispatch / fallback 行为。 | board 性能仍来自 protected helper bench，不是完整公开入口计时。 |

当前 correctness、单轮性能证据、Phase 030 `PointXYZ + Normal` repeated summary 和 Phase 050 代表性 source repeated summary 均支持保留已有 production patch；registry automation（证据登记自动化）已在 phase 020 / 030 / 050 关闭。Phase 070 已关闭代表性 source × normal 交叉 correctness；泛型点类型全集、更多 normal-like 点型、公开入口性能和 `Scalar=double` 仍需后续 phase 独立关闭。
