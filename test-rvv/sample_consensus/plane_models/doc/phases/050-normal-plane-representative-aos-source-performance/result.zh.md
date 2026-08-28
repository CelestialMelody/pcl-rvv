# Normal-plane 代表性 AoS Source 性能扩展 Phase Result

## 结论

本阶段已关闭，decision 为 `adopted / representative AoS source performance positive-stable`。
Phase 050 在不改变 production（生产源码）RVV 实现族和 public dispatch（公开入口分流逻辑）的前提下，
把 `bench_sac_normal_plane` 扩展为显式 `--representative-aos-sources` 模式，并在板卡上完成
`PointXYZ + Normal`、`PointXYZI + Normal`、`PointXYZINormal + Normal` 三种 source 点型的
5-run repeated board benchmark（重复板卡性能测试）。

新增性能证据显示：`PointXYZI + Normal` 和 `PointXYZINormal + Normal` 的三条 helper 均为正向，
每个 case 的 min speedup 都大于 1.0x，median speedup 都大于 1.2x。该结论只覆盖本阶段
代表性 AoS source 点型的 protected helper hot path（受保护 helper 热点路径），不外推到其它
source 点型、其它 normal layout（法线布局）、`Scalar=double` 或新的 RVV math family（数学实现族）。

## 本阶段改动

| 类型 | 路径 | 内容 |
| --- | --- | --- |
| bench | `test-rvv/sample_consensus/plane_models/src/bench_sac_normal_plane.cpp` | 默认模式保持 Phase 030 旧三项 label；新增 `--representative-aos-sources` / `--all-source-types` 显式模式，输出三种 source 点型的 9 个独立 case。 |
| Makefile | `test-rvv/sample_consensus/plane_models/Makefile` | 新增 Phase 050 repeated board、analyze、manifest、Evidence Doctor、registry 和 freshness target；把 topic-local `REMOTE_DIR` 对齐到 board 绝对路径，避免 `~` 在 quoted `test -f` 中不展开导致 compare 前重复运行。 |
| manifest wrapper | `test-rvv/sample_consensus/plane_models/script/generate_normal_plane_board_evidence_manifest.py` | 新增 `--case-set representative-aos`，让 Phase 050 manifest 解析 9 个代表性 AoS source case；默认 `existing` case set 不变，Phase 030 旧 manifest 语义不变。 |
| phase doc | `test-rvv/sample_consensus/plane_models/doc/phases/050-normal-plane-representative-aos-source-performance/plan.zh.md` | 阶段范围、复跑预算、Evidence Doctor 输入和完成条件。 |

## 计划动作回填

| action | 状态 | 命令 / 证据 | 结论 |
| --- | --- | --- | --- |
| A1 bench 参数化 | done | `make -C test-rvv/sample_consensus/plane_models run_bench_rvv BENCH_ARGS="<pcd> 1"`；`make -C test-rvv/sample_consensus/plane_models run_bench_rvv BENCH_ARGS="<pcd> 1 --representative-aos-sources"` | 默认模式仍输出 `selectWithinDistance`、`countWithinDistance`、`getDistancesToModel` 三项；显式模式输出 9 个可解析 case。QEMU 只用于日志形状，不作性能结论。 |
| A2 manifest 扩展 | done | `record_phase050_evidence_state` 生成 manifest / doctor 并登记 registry。 | Phase 050 使用 `--case-set representative-aos`；Phase 030 旧 case set 不受影响。 |
| A3 correctness smoke | done | `make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests`；`make -C test-rvv/sample_consensus/plane_models run_test_compare` | public alias RVV 6/6 passed；Std/RVV 完整 QEMU compare 各 25/25 passed。 |
| A4 asm attribution | done | `make -C test-rvv/sample_consensus/plane_models dump_bench_rvv` | RVV bench 二进制反汇编已刷新；三条 normal-plane RVV helper 继续有 RVV 指令归属。 |
| A5 board repeated performance | done | `SSH_AUTH_SOCK=<agent-socket> make -C test-rvv/sample_consensus/plane_models run_board_bench_compare_phase050`；`make -C test-rvv/sample_consensus/plane_models record_phase050_evidence_state` | 5-run board compare 完成并抓回本地；summary、manifest、doctor 和 registry 已生成。 |
| A6 文档同步 | done | 本 result、phase index、matrix、roadmap、topic-local docs、长期 `doc-rvv` 和 queue。 | 文档将 Phase 050 写成代表点型 helper performance，不升级为完整泛型 production performance。 |

## Board Repeated Summary

证据路径：

```text
test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase050-representative-aos-source-performance/summary.md
```

| Benchmark Item | runs | median | min | max | p10 | p90 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `selectWithinDistance_PointXYZ_Normal` | 5 | 9.47x | 9.31x | 9.56x | 9.33x | 9.55x |
| `countWithinDistance_PointXYZ_Normal` | 5 | 11.59x | 10.49x | 11.73x | 10.89x | 11.70x |
| `getDistancesToModel_PointXYZ_Normal` | 5 | 11.82x | 11.45x | 12.30x | 11.54x | 12.11x |
| `selectWithinDistance_PointXYZI_Normal` | 5 | 8.04x | 7.40x | 8.46x | 7.53x | 8.42x |
| `countWithinDistance_PointXYZI_Normal` | 5 | 6.10x | 6.05x | 6.13x | 6.06x | 6.13x |
| `getDistancesToModel_PointXYZI_Normal` | 5 | 5.41x | 4.49x | 6.06x | 4.64x | 5.88x |
| `selectWithinDistance_PointXYZINormal_Normal` | 5 | 7.37x | 6.60x | 7.55x | 6.77x | 7.48x |
| `countWithinDistance_PointXYZINormal_Normal` | 5 | 8.52x | 7.88x | 8.94x | 8.12x | 8.78x |
| `getDistancesToModel_PointXYZINormal_Normal` | 5 | 8.44x | 8.22x | 8.63x | 8.24x | 8.59x |

决策桶：所有 case 均为 `positive-stable`。`PointXYZI getDistancesToModel` 的 min/max 比例较大，
但 min 仍为 4.49x，未接近退化阈值；因此记录为需解释的长尾 Warning，不触发追加复跑。

## Evidence Doctor 与 Registry

Evidence Doctor 输入与输出：

| artifact | path |
| --- | --- |
| manifest | `test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase050-representative-aos-source-performance/evidence-manifest.json` |
| doctor markdown | `test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase050-representative-aos-source-performance/evidence-doctor.md` |
| doctor json | `test-rvv/sample_consensus/plane_models/log/board/normal-plane-phase050-representative-aos-source-performance/evidence-doctor.json` |

结果为 `Errors=0, Warnings=5, Suggestions=0`。

| Warning | 解释 | 处理 |
| --- | --- | --- |
| `long_tail_or_variance` on `getDistancesToModel_PointXYZI_Normal` | 5-run 内 speedup 为 4.49x 到 6.06x，说明单一均值不足以描述该 case。 | 保留 min/median/max/p10/p90；由于 min 仍远高于 1.0x，当前桶不降级。若后续 production-public 计时方向反转，再扩展到 20-run 或补 per-iteration trace。 |
| `group_outlier` on several cases | 三种 source 点型和三条 helper 的 stride、写回和 mask / reduction 行为不同，不能把同组 median 直接外推到每个 case。 | 按 point type 和 helper 独立报告，不使用组均值作结论；矩阵逐 case 保留。 |

registry 状态：

```text
make -C test-rvv/sample_consensus/plane_models phase050_evidence_status
```

输出为 `evidence registry check: fresh`。本阶段没有提交 raw logs；summary-only（只提交摘要证据）策略保持不变。

## diagnostic-to-production mismatch audit 回填

| question | answer |
| --- | --- |
| evidence role | `production-shaped diagnostic performance`。 |
| A/B boundary | `SampleConsensusModelNormalPlaneBench` protected helper hot path；Std build vs RVV build。 |
| 当前决策问题 | representative source point type 下 RVV-vs-scalar performance。 |
| diagnostic 是否可外推到 production | 只能说明相同 source / normal layout gate 命中时 helper hot path 有强正向性能；公开入口 dispatch / fallback 仍由 Phase 040 correctness 证明。 |
| comparison-boundary / baseline mismatch 风险 | Std/RVV 使用同一 wrapper、同一输入 PCD、同一 `indices_` 和同一 normal cloud；风险主要是 helper 计时不是完整 public overload 计时。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本轮未出现弱 / 负 / 中性 / 不稳定。若后续出现，只降级对应点型性能边界，不自动回滚 source AoS correctness gate。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 不需要；本阶段没有选择新 RVV family。 |

## Phase Scope 与扩展队列

| 范围 | 状态 | 说明 |
| --- | --- | --- |
| `PointXYZ + Normal` helper performance | adopted / positive-stable | Phase 030 和 Phase 050 均保持强正向；Phase 050 默认模式未改变旧 case label。 |
| `PointXYZI + Normal` helper performance | adopted / positive-stable | 5-run median 为 8.04x、6.10x、5.41x；只覆盖 helper hot path。 |
| `PointXYZINormal + Normal` helper performance | adopted / positive-stable | 5-run median 为 7.37x、8.52x、8.44x；source 侧 extra normal / intensity 字段不参与算法输出语义。 |
| 更多 registered xyz AoS source 点型 | deferred | 可继续补 `PointXYZRGB` / `PointXYZRGBA` correctness 和性能；当前阶段未覆盖。 |
| 其它 `PointNT` normal-like layout | deferred | 当前仍只验证 `Normal` 与 double-curvature fallback。 |
| `Scalar=double` 或新 math family | deferred / not_applicable | 当前 production RVV helper 仍是 `f32m2`。 |

## 继续 / 停止决策

`continue_stop_decision = phase_closed / no high-priority unblocked performance action inside current authorized representative source scope`

`stop_condition_hit = remaining actions require expanding scope to more source point types, other normal layouts, Scalar=double, or a new implementation family`

默认恢复动作：

```bash
make -C test-rvv/sample_consensus/plane_models run_normal_plane_public_tests
make -C test-rvv/sample_consensus/plane_models run_test_compare
make -C test-rvv/sample_consensus/plane_models phase050_evidence_status
make -C test-rvv/sample_consensus/plane_models evidence_status
make -C test-rvv/sample_consensus/plane_models repeated_evidence_status
```

若继续扩大范围，下一 phase 建议为 `060-normal-plane-normal-layout-expansion`，先补其它
normal-like `PointNT` 的 traits / layout correctness 和 fallback；更宽 source 点型性能扩展可作为
低优先级 follow-up，因为 Phase 050 已覆盖两个常见代表点型且均强正向。
