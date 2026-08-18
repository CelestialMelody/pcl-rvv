# Phase 002 Plan：production-integration-plan

## 目标

把 Phase 001 已验证为正向的 ordered-cloud-pair C1/C2 RVV accumulation candidate（RVV 累加候选）收窄接入 production public entry（生产公开入口）。本阶段只覆盖无 indices / correspondences 的顺序全云点对入口，不扩大到 source-indexed、dual-indexed 或 correspondence-pair。

## 范围冻结

| 维度 | 本阶段范围 | 不证明 / 不触碰 |
| --- | --- | --- |
| production 文件 | `registration/include/pcl/registration/impl/transformation_estimation_dual_quaternion.hpp` | 不修改公开 API，不修改其它 registration topic |
| public entry | `estimateRigidTransformation(cloud_src, cloud_tgt, Matrix4&)` | indexed、dual-indexed、correspondences 保持当前 iterator 标量路径 |
| `Scalar` | 仅 `float` 输出矩阵进入 RVV | `Scalar=double` 自然 fallback |
| 点型 / layout | `PointSource` / `PointTarget` 分别满足 `RVVXYZAoSFloatLayout`，即 PCL traits 注册的单 float x/y/z AoS layout | 不声称完整泛型 PointT 输出语义；不覆盖缺 xyz 或非 float 字段 |
| 输入属性 | `cloud_src.size()==cloud_tgt.size()`、两侧 `is_dense`、点数不少于 32 | non-dense、NaN/Inf、极小规模保持标量 |
| RVV 覆盖 | C1/C2 前置累加；Eigen 4x4 self-adjoint solve 和矩阵构造保留标量 | 不向量化 Eigen solve |

## Experience Migration Audit

| 经验维度 | sibling / 现有机制 | 当前采用 | 状态 | 证据 / 理由 | 下一步 |
| --- | --- | --- | --- | --- | --- |
| ordered-cloud-pair production gate | `transformation_estimation_svd.hpp` 使用 `RVVXYZAoSFloatLayout` 分别 gate source/target | 采用同类 gate，不复制 SVD 数学 | `adopted` | TEDQ 同样只读 x/y/z，且 board diagnostic 已覆盖 `PointXYZ` / layout test 覆盖 `PointXYZI` | production helper 使用 source/target 分别 gate |
| 公共 load wrapper | `pcl::rvv_load::strided_load3_f32m2` 支持 m2；TEDQ 证据使用 f32mf2 -> f64m1 | 不直接采用 m2 wrapper | `rejected with evidence` | C1/C2 需要 16 个 f64 accumulator；m2->m4 会放大寄存器压力并偏离 Phase 001 证据形态 | 本阶段局部保留 mf2 strided load，并用 traits offset gate 约束 |
| row source policy | 只在 ordered-cloud-pair public entry 分流，其它 overload fallback | 采用 | `adopted` | indexed / correspondence 未有 board 证据 | 本阶段只改 ordered-cloud-pair public overload |
| evidence model | SVD topic 有 production direct board repeated 经验 | 采用 | `adopted` | Phase 001 只有 test-support helper evidence，需要生产直连补证据 | 新增 production public board repeated target / manifest |

## 实现动作

| action | 内容 | 产物 | 完成判据 |
| --- | --- | --- | --- |
| A1 production helper | 在 TEDQ impl 中新增 `detail` RVV accumulation helper、solve wrapper 和 ordered-cloud-pair dispatch | production header | QEMU Std/RVV gtest 通过 |
| A2 production tests | 增加 public ordered-cloud-pair `PointXYZI`、`Scalar=double` fallback compile / correctness、indexed identity 边界测试 | `src/test_tedq.cpp` | Std/RVV 测试通过 |
| A3 production evidence target | 扩展 board repeated summary script / Make target，采集 `public-dual-quaternion` production direct | Makefile、summary script、`.gitignore` allowlist | summary、manifest、doctor、registry fresh |
| A4 asm / board evidence | 运行 asm dump、QEMU correctness、board production repeated、Evidence Doctor | `log/board/production_public_full_cloud_repeated/*` | Evidence Doctor Errors=0；checksum 一致 |
| A5 docs / matrix | 更新 evaluation、README、benchmark/evidence、optimization evidence、phase result、doc-rvv 长期主题文档和模块筛选表 | topic docs / doc-rvv | production evidence 边界可恢复 |

## 测试和证据计划

```bash
make run_test_compare
make dump_bench_rvv
make run_qemu_smoke_evidence_doctor BENCH_ARGS="--iterations 2 --warmup-iterations 1 --case-filter public-dual-quaternion"
make collect_board_bench_production_public_repeated
make record_board_production_public_state
make evidence_status
```

QEMU timing（QEMU 计时）仍只用于日志形状，不进入性能结论。production direct（真实生产路径证据）只来自 board repeated。

## Board 复跑预算和决策桶

- repeated runs：`5`
- iterations：`20`
- warm-up iterations：`5`
- case-filter：`public-dual-quaternion`
- B/A：Std public entry ms / RVV public entry ms
- bucket：沿用 Phase 001 的 `positive` / `weak_positive` / `neutral` / `negative` / `unstable` 规则；64K / 256K 是主要判断规模。

## 继续 / 停止条件

本阶段通过后，EvidenceDecision 可升级为 `production_candidate_positive` 或 `production_ready`，具体取决于 production direct tests、board repeated、asm 和 Evidence Doctor 是否全部闭合。若 checksum mismatch、Evidence Doctor Error、production public entry 性能 negative 或 dirty isolation 不安全，则停止并回滚或降级 production 接入。
