# Phase 060 Result: production-candidate-review-and-row-source-boundaries

## 当前结论

本阶段完成了对 Phase 050 production candidate（生产候选）和 Phase 030 row-source
diagnostic（行来源诊断）的恢复审阅。当前 production patch 保留，不回滚、不覆盖、不提交，
也不把当前 exact `PointXYZ -> PointXYZ` 结论升级成泛型点类型结论。

当前状态：

```text
production-candidate-supported / user-review-pending
```

本阶段结束后，按用户指定的 phase loop 进入新的
`070-generic-xyz-point-type-expansion`。该阶段先在 test-rvv 里验证 PCL traits
泛型 gate 和代表性点型证据；在证据闭合前不修改 production dispatch。source-indexed、
dual-indexed 和 correspondence 仍保持独立 row-source 阶段，不继承本阶段的
ordered-cloud-pair 结论。

## 计划动作回填

| id | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| A1 | done | 当前 production diff；`registration/include/pcl/registration/impl/transformation_estimation_2D.hpp` | RVV helper 只在 `__RVV10__` 下存在；公开 ordered-cloud-pair overload 先检查数量，再尝试 helper；helper 只接受 exact `PointXYZ -> PointXYZ`、`Scalar=float`、两侧 dense、所有点 finite、点数不少于 16；失败后继续既有 iterator 标量路径。source-indexed、dual-indexed、correspondence overload 没有接入该 dispatch。 |
| A2 | done | `log/board/ordered_cloud_pair_public_repeated/evidence_manifest.json`、`summary.md`、`evidence_doctor.md` | provenance（证据来源）闭合到 `Milkv-Jupiter`、5 runs、每 run 20 iterations、5 warmup、Std/RVV binary hash、相同 public timer boundary 和 `post-production public dispatch` 证据角色。taskset、governor、freq、temperature 仍是未记录环境字段，保留为解释长尾时的风险；本次 Doctor 没有因此产生 finding。 |
| A3 | done / diagnostic boundary retained | `log/board/row_source_fused_repeated/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` | source-indexed 只物化 source row，64K median `1.023x`、无低于 1 的 run 但接近阈值；dual-indexed 双侧物化，64K 出现 `0.956x` 长尾和 `1/5` 退化；correspondence 需要展开 query/match，64K 出现 `2/5` 退化。当前证据支持“物化、缓存局部性、分配/复制和调度噪声是待验证假设”，不支持单因归因，也不支持 production gather。 |
| A4 | done | `doc/optimization-roadmap.zh.md`、`doc/phases/optimization-matrix.zh.md`、Phase 070 plan | ordered-cloud-pair production candidate 继续保持 review-pending；generic xyz point type 作为新的 test-only evidence phase；三个 indexed/correspondence policy 继续独立 deferred，不进入本阶段的泛型结论。 |
| A5 | done | `make ... evidence_status`、`python3 -m py_compile`、YAML parse、`git diff --check` | registry fresh；topic-local scripts 可编译；当前 Handoff YAML 可解析；production target 和 topic-local 文档无 whitespace error。 |

## Production diff 审阅

当前生产改动集中在：

```text
registration/include/pcl/registration/impl/transformation_estimation_2D.hpp
```

改动保留了原公开 API 和原 iterator 标量路径。RVV helper 的两遍结构是：

1. 顺序跨步读取 source / target 的 `x/y/z`，求 x/y 质心；
2. 再次读取两侧点，中心化后用 RVV 累加 2x2 correlation；
3. `atan2`、`cos/sin`、平移和 4x4 矩阵写回仍为标量；
4. 任一 gate 失败时返回 `false`，公开入口继续既有标量估计。

当前实现使用 `offsetof(PointSource, x/y/z)` 和 `offsetof(PointTarget, x/y/z)`，
但 compile-time gate 仍是 exact `PointXYZ` 匹配。因此它不是 PointXYZ-like 泛型实现；
即使 `PointXYZI`、`PointNormal` 或 `PointXYZINormal` 的内存形态可能适合相同的
stride load，也不能从当前 patch 自动推出泛型安全性。

## 证据分层

### 正确性和路径证据

- Std / RVV correctness：各 16/16。
- production-public QEMU smoke：Evidence Doctor `Errors=0, Warnings=0, Suggestions=0`。
- production-public asm：关键 RVV 指令可归属到 `runPublicCase` 内联边界或当前 production
  public boundary；该证据只证明路径和指令归属，不证明板卡性能。

### 真实板卡性能证据

`Milkv-Jupiter` production-public repeated：

| case | median B/A | B/A<1 | bucket |
| --- | ---: | ---: | --- |
| public ordered-cloud-pair 4K | `4.222x` | `0/5` | positive |
| public ordered-cloud-pair 64K | `5.310x` | `0/5` | positive |
| public ordered-cloud-pair 256K | `4.947x` | `0/5` | positive |

Board Evidence Doctor 为 `0/0/0`。这只支持保留当前窄范围 production patch，不支持泛型、
indexed、dual-indexed 或 correspondence production。

### Row-source 诊断证据

Phase 030 的 materialize-to-ordered candidate 仍是 test-only：

| policy | 64K 结果 | Doctor / 边界 |
| --- | --- | --- |
| source-indexed-cloud-pair | median `1.023x`，`0/5` 退化 | 接近阈值，不能写成稳定收益 |
| dual-indexed-cloud-pair | median `1.014x`，`0.956x` 长尾，`1/5` 退化 | Error/Warning 未闭合为 production evidence |
| correspondence-pair | median `1.013x`，`2/5` 退化 | Doctor Error，不能用 median 掩盖退化频率 |

Row-source board Doctor 为 `Errors=1, Warnings=2, Suggestions=6`。异常没有被删除；
当前 decision 继续降级为 diagnostic-only。下一次 row-source 阶段必须比较真实 gather /
staging 与 materialize-to-ordered 的边界，并继续单独维护 policy-specific correctness、
asm、board 和 Doctor。

## Phase loop 决策

| 项目 | 状态 |
| --- | --- |
| `phase_plan_written_before_edits` | pass；本阶段已有 plan，Phase 070 也先创建 plan |
| `optimization_roadmap_ready` | pass；roadmap 已加入 generic point-type candidate |
| `roadmap_default_recovery_queue_ready` | pass；默认队列现在先进入 Phase 070，再回到 row-source layout/stability |
| `phase_completion_matrix_ready` | pass；本 result 逐项回填 A1-A5 |
| `optimization_matrix_ready` | pass；generic point type、三类 row source 和 production narrow gate 分行 |
| `micro_stop_guard` | pass；没有因为“审阅完成”而结束 topic，继续进入授权内的泛型证据阶段 |
| `ready_for_review_validity_check` | stale stop decision；Phase 070 是当前授权内、未阻塞的下一阶段，因此不能把 topic 写成 ready_for_review |

## Evidence Doctor 和 registry

- production-public board manifest：`Errors=0, Warnings=0, Suggestions=0`，可继续作为窄范围
  production candidate 的板卡证据。
- row-source board manifest：`Errors=1, Warnings=2, Suggestions=6`，继续作为 diagnostic-only；
  不能升级为 production evidence。
- QEMU manifest：只作为 correctness、路径、日志形状和 asm 输入证据。
- `make -C test-rvv/registration/transformation_estimation_2D evidence_status`：`fresh`。
- raw logs、build、registry 和 Handoff 继续按 `summary-only / local-only` 边界处理。

## 下一阶段

```text
070-generic-xyz-point-type-expansion
```

下一阶段只允许先修改 test-rvv 资产和 topic-local 文档，核心动作是：

- 用 `pcl::rvv::RVVXYZAoSFloatLayout<PointT>` 设计 PointXYZ-like gate；
- 对 `PointSource` 和 `PointTarget` 分别取 traits、POD、sizeof、offset 和 stride；
- 覆盖 `PointXYZ`、`PointXYZI`、`PointNormal`、`PointXYZINormal` 以及必要 mixed pair；
- 把额外字段“不参与本算法”的语义写成 correctness 断言；
- 补 generic candidate 的 fallback、QEMU、asm、代表性板卡 bench 和 Evidence Doctor；
- 证据闭合后，才重新写 PI1，决定是否修改 production dispatch。

本阶段没有命中回滚、提交或 production adoption 的授权条件；当前 patch 必须保留。
