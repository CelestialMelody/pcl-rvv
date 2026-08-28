# Normal-plane AoS 点类型 Gate 扩展 Phase Result

## 结论

本阶段已关闭，decision 为 `adopted for representative AoS source dispatch gate`。三条公开入口的 RVV dispatch（分流逻辑）已从 `RVVXYZFloatLayout<PointT>` 收紧为 `RVVXYZAoSFloatLayout<PointT>`，normal 侧继续要求 registered single-float normal + curvature，并额外要求字段 offset 和 stride 满足当前 byte-offset gather（按字节偏移离散加载）前提。

新增 correctness / fallback 证据覆盖：

- `PointXYZI + Normal` 和 `PointXYZINormal + Normal` 代表性 source 点型在公开入口下与 direct RVV helper 输出一致。
- 注册了单 float `x/y/z` 但不是 standard-layout（标准布局）的 source 点型在 RVV build 下走 Standard fallback（标量回退），不会实例化 RVV helper 的 static assertion。
- `PointXYZ + Normal` 既有公开入口、unsupported curvature fallback 和 helper buffer resize 测试继续通过。

本阶段不新增性能结论。Phase 030 的 repeated board（重复板卡）性能证据仍只覆盖 `PointXYZ + Normal` protected helper hot path（受保护 helper 热点路径），不能外推到 `PointXYZI`、`PointXYZINormal` 或完整自定义点型集合。

## 本阶段改动

| 类型 | 路径 | 内容 |
| --- | --- | --- |
| production | `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_plane.hpp` | 新增 `NormalPlaneRVVNormalAoSLayout<PointNT>` 和 `kNormalPlaneRVVLayoutCompatible<PointT, PointNT>`；三条公开入口使用 stronger AoS source gate 和 normal/curvature gate；RVV helpers 的 `PointLayout` 切到 `RVVXYZAoSFloatLayout<PointT>`；公开入口增加 source / normal 点云规模的 32-bit byte offset 上界检查。 |
| test | `test-rvv/sample_consensus/plane_models/src/test_sample_consensus_plane_models.cpp` | 新增 `NonAoSRegisteredXYZ` fallback fixture、AoS traits static asserts、`PointXYZI` / `PointXYZINormal` public-vs-direct RVV 测试和 non-AoS source fallback 测试。 |
| Makefile | `test-rvv/sample_consensus/plane_models/Makefile` | `NORMAL_PLANE_PUBLIC_FILTER` 纳入 6 个 public / fallback / buffer 测试；evidence doc refs 纳入 Phase 040 plan/result，避免 registry freshness scan 漏扫新增阶段引用。 |

## 计划动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 RED 测试 | done | 修改测试后，在旧 dispatch gate 下运行 `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests`。 | RVV build 尝试为 `NonAoSRegisteredXYZ` 实例化 `byte_offsets_u32m2<NonAoSRegisteredXYZ>` 并触发 standard-layout static assertion；RED 证明旧 `RVVXYZFloatLayout` gate 太弱。 |
| A2 production gate | done | `sac_model_normal_plane.hpp` 当前 diff。 | 公开入口 gate 与 RVV byte offset helper 前提对齐；非覆盖布局自然 fallback；`PointXYZ` 既有主路径不改变 RVV 实现族。 |
| A3 QEMU correctness | done | `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests`；`make -C test-rvv/sample_consensus/plane_models run_test_compare`。 | RVV/QEMU public alias 6/6 通过；Std/RVV 完整 compare 各 25/25 通过。QEMU timing 只作为正确性 / 日志形状输出，不作为性能证据。 |
| A4 board correctness | done | `make -C test-rvv/sample_consensus/plane_models run_board_normal_plane_public_tests`，本轮通过命令行 SSH identity override 恢复板卡登录。 | 板卡 public alias 6/6 通过。远端 make 报告 clock skew（时钟偏移）警告，但测试二进制运行和结果通过。 |
| A5 文档同步 | done | 本 result、phase index、matrix、roadmap、topic-local docs、长期 `doc-rvv` 和 queue。 | 文档把新增范围写成 representative AoS source expansion（代表性 AoS source 扩展），不写成完整泛型点型或新增性能收益。 |

## Evidence Doctor 与 Registry

本阶段没有生成新的 benchmark summary、board repeated summary、checksum summary 或 asm attribution，因此没有新增机器可读 Evidence Doctor 输入。Evidence Doctor 人工检查结果：

| 类别 | 数量 | 处理 |
| --- | ---: | --- |
| Errors | 0 | 新增证据是 correctness / fallback；没有把 QEMU timing 或 board public test 写成性能结论。 |
| Warnings | 0 | 新增点型只关闭 representative source correctness；性能仍引用 Phase 030 `PointXYZ + Normal` repeated summary。 |
| Suggestions | 0 | 若后续要给 `PointXYZI` / `PointXYZINormal` 写性能结论，需要单独建立 board performance phase。 |

registry 状态：本阶段没有覆盖 Phase 030 repeated summary。提交或复跑前仍需运行：

```bash
make -C test-rvv/sample_consensus/plane_models evidence_status
make -C test-rvv/sample_consensus/plane_models repeated_evidence_status
```

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-public correctness / fallback`；Phase 030 仍是 `production-shaped diagnostic performance`。 |
| A/B boundary | supported AoS types 使用 public overload vs direct RVV helper；non-AoS source 使用 public overload vs Standard helper。 |
| 当前决策问题 | fallback correctness and implementation-shape（实现形态）。 |
| diagnostic 是否可外推到 production | correctness 直接来自 public entry；性能只沿用 `PointXYZ + Normal` protected helper hot path，不外推到新增 source 点型性能。 |
| comparison-boundary / baseline mismatch 风险 | 新增测试没有性能 A/B；只证明 dispatch、direct RVV 输出一致、fallback 可编译可运行。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | not_applicable；本阶段不做新性能 probe。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；没有选择新 RVV family。 |

## Phase Scope 与扩展队列

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| `PointXYZ + Normal` | adopted | 既有 public dispatch、fallback、QEMU、asm、board repeated performance 仍成立。 |
| `PointXYZI + Normal` | adopted for correctness / dispatch | public overload 与 direct RVV helper 输出一致；不含 dedicated board performance。 |
| `PointXYZINormal + Normal` | adopted for correctness / dispatch | source 只读 `x/y/z`，source 侧 normal / intensity 字段不参与输出语义；不含 dedicated board performance。 |
| non-AoS registered xyz source + `Normal` | adopted fallback | public overload 回退 Standard helper，避免 helper static assertion。 |
| `NormalWithDoubleCurvature` | adopted fallback | 继续覆盖 curvature 非单 float normal layout fallback。 |
| 更多 registered xyz AoS source 点型 | deferred | 可继续补 `PointXYZRGB`、`PointXYZRGBA` 等代表点型 correctness 和可选 board performance。 |
| 其它 `PointNT` normal-like layout | deferred | 当前只证明 `Normal` 和 double-curvature fallback；其它 normal 点型需要单独 normal/curvature gate 测试。 |
| `Scalar=double` 或新 math family | deferred / not_applicable for this phase | 当前 RVV helper 仍是 float `f32m2` 实现；本阶段不改变数值实现族。 |

## 继续 / 停止决策

`continue_stop_decision = phase_closed / current worker turn can stop after final verification`

`stop_condition_hit = current roadmap has no high-priority unblocked action inside this phase without expanding scope`

默认恢复动作：

```bash
make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests
make -C test-rvv/sample_consensus/plane_models run_test_compare
make -C test-rvv/sample_consensus/plane_models evidence_status
make -C test-rvv/sample_consensus/plane_models repeated_evidence_status
```

若继续扩大范围，下一 phase 应是 `050-normal-plane-representative-aos-source-performance` 或 `050-normal-plane-normal-layout-expansion`，二者都需要新的 phase plan，并明确是否要新增 board performance / asm / Evidence Doctor。`Scalar=double` 不建议作为当前默认下一步，除非用户明确选择数值实现族扩展。

## ready_for_review_validity_check

| area | 状态 | 证据 | 结论 |
| --- | --- | --- | --- |
| production boundary | closed for current scope | `kNormalPlaneRVVLayoutCompatible` + public tests。 | source AoS gate、normal/curvature gate 和 byte-offset 上界一致。 |
| point type expansion | closed for representative source correctness | `PointXYZI`、`PointXYZINormal` public-vs-direct RVV tests 通过。 | 只能写代表点型 correctness，不写完整泛型性能。 |
| fallback | closed | `NonAoSRegisteredXYZ` 和 `NormalWithDoubleCurvature` fallback tests。 | 非覆盖 source / normal layout 不误入 RVV。 |
| board correctness | closed | `run_board_normal_plane_public_tests` 6/6 通过。 | 板卡证明 public alias 在目标硬件可运行。 |
| board performance | unchanged | Phase 030 repeated summary。 | 性能证据仍限定 `PointXYZ + Normal` helper hot path。 |
| roadmap / matrix | closed for current phase | 本 result、roadmap 和 matrix 已同步。 | 后续扩展均需新 phase，不属于本阶段未完成动作。 |
