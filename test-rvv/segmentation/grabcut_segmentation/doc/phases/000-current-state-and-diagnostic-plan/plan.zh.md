# Phase 000: current state and organized n-link diagnostic plan

## 阶段意图和边界

本阶段只建立 GrabCut organized n-link（规则图像邻接边）和 color staging（颜色暂存）的 pre-production diagnostic（接入生产前诊断）边界。目标是证明 `computeBetaOrganized` / `computeNLinksOrganized` 的标量语义可以被 topic-local reference（主题本地参考链路）稳定复刻，并建立 component bench（组件性能测试）入口，用于回答该片段是否值得后续 RVV candidate（RVV 候选实现）。

本阶段不修改 production（生产源码），不接入 `GrabCut<PointT>` public entry（公开入口），不优化 `BoykovKolmogorov::solve` max-flow（最大流）状态机，也不把 QEMU timing（QEMU 计时）写成性能结论。

## 当前状态清单

| 对象 | 当前事实 | 路径 |
| --- | --- | --- |
| 模板入口 | `initCompute` 把 RGB/RGBA 点转成 `Image<Color>`，organized path 调 `computeBetaOrganized` 和 `computeNLinksOrganized`。 | `segmentation/include/pcl/segmentation/impl/grabcut_segmentation.hpp` |
| organized beta | 每个像素最多写 4 条邻接边，先保存 color distance 和几何距离，再做全局 beta reduction（规约）。 | 同上 |
| organized n-link | 用 `lambda_ * exp(-beta_ * color_distance) / distance` 计算真实边权。 | 同上 |
| GMM / graph 后端 | `initGraph` 计算 terminal weights 后调用 graph edge mutation；`refineOnce` 调 max-flow solver。 | `segmentation/src/grabcut_segmentation.cpp` |
| 上游测试 | 当前未发现直接 GrabCut unit test。 | `rg GrabCut test benchmarks` |
| topic 测试资产 | 新建中。 | `test-rvv/segmentation/grabcut_segmentation/` |

## 假设与候选族

| candidate family | 假设 | 风险 |
| --- | --- | --- |
| organized color distance + beta reduction | 规则 dense scan 可以用 RVV load/mask/reduction 处理，并减少标量邻接公式成本。 | reduction 顺序会改变浮点舍入，必须先用误差预算和 same-chain（同构链路）对拍界定。 |
| organized n-link weight | `exp` 权重在大 organized cloud 上可能可见。 | 当前 PCL 未确认有 strict float `exp` RVV helper；若只用近似，需要额外数学 helper 审计。 |
| color staging | RGB/RGBA 到 `Color` 的逐点转换可批量化。 | packed RGB/RGBA 字段和模板点类型 traits gate（字段特征门禁）会扩大 production 范围；本阶段只诊断。 |

## 优化矩阵

矩阵主归属：`test-rvv/segmentation/grabcut_segmentation/doc/phases/optimization-matrix.zh.md`。

## 实现和测试动作

| 动作 | 产物 | 预期证据 | 完成判据 |
| --- | --- | --- | --- |
| RED correctness | `src/test_grabcut.cpp` 先引用尚未实现的 test-only helper | 编译失败，证明测试会捕获缺失 helper | `make run_test_compare` 因缺少 helper 或断言失败而失败。 |
| GREEN reference | `include/grabcut_diagnostic.h` / `include/impl/grabcut_diagnostic_reference.hpp` | QEMU Std/RVV correctness 一致 | organized beta/n-link reference 输出与独立手写 expected case 一致。 |
| component bench scaffold | `src/bench_grabcut.cpp` | QEMU log-shape smoke + board-ready bench binary | 输出 label、checksum、time_ms，不在 QEMU 下做性能结论。 |
| asm / auto-vectorization line | `make dump_bench_rvv` 或 `generate_vec_report` | 反汇编或 missed-vectorization 线索 | 若无 RVV candidate，仅作为 compiler baseline 线索，不关闭 RVV candidate。 |

## Evidence Doctor 和 registry 规则

本阶段若只运行 QEMU correctness，不需要 Evidence Doctor。若生成 board repeated summary 或 checksum / benchmark summary，必须先生成 topic-local manifest（证据清单）或人工记录 `metadata_incomplete`，再运行 `test-rvv/script/evidence_doctor.py`。当前 `evidence_registry.json` 尚未建立；phase result 必须写 `evidence_registry_status=not_available` 并列出人工检查路径。

## 阶段完成条件

| 条目 | 完成条件 |
| --- | --- |
| reference correctness | `make run_test_compare` Std/RVV 均通过，且测试覆盖 interior、right/bottom boundary 和 beta denominator。 |
| bench scaffold | `make run_bench_rvv ALLOW_QEMU_BENCH_COMPARE=0` 可运行单边 log-shape，板卡 target 可部署或写明具体工具阻塞。 |
| production decision | 只能写 `diagnostic` 或 `deferred`；没有 board repeated positive 前不能写 production-ready。 |

## 板卡复跑预算和决策桶

如果 Phase 000 进入 board component bench，预算为 5 runs，每 run 使用 `--iterations 8 --warmup 2`。decision bucket（决策桶）暂定：`positive >= 1.10x`，`weak-positive 1.03x..1.10x`，`neutral 0.97x..1.03x`，`negative < 0.97x`，若 5-run 内方向摇摆且不能解释则标为 `unstable`。用户已说明板卡可用，因此需要真实性能结论时必须继续到板卡步骤，除非 ssh/rsync/make target 失败。

## 继续 / 停止条件

默认继续到 Phase 000 correctness + bench scaffold 闭合。只有下列条件允许本轮停止：测试资产 dirty isolation 不安全、交叉工具链 / 依赖不可用、板卡工具不可达、Evidence Doctor Error 无法处理，或 Phase 000 证明该片段成本明显不值得继续。

下一阶段默认入口：若 component bench 有正向信号，创建 Phase 010 RVV organized n-link candidate；若负向或 solver 稀释明显，更新 evaluation 为 no-production diagnostic closeout。

## 文档更新清单

本阶段需要维护 README、evaluation、optimization roadmap、optimization matrix、phase result 和 Handoff。`doc-rvv/segmentation/grabcut_segmentation-RVV.zh.md` 当前不适用，因为没有 adopted production behavior（已采用生产行为）。

## diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | pre-production diagnostic / component ablation（组件消融）。 |
| A/B boundary | test-only helper and component bench，不是 production public entry。 |
| 当前决策问题 | RVV-vs-scalar feasibility（可行性）和 production-value screening（生产价值筛选）。 |
| diagnostic 是否可外推到 production | unknown。organized scan 形态相似，但不包含 `initGraph` edge mutation、GMM learning 和 max-flow solver。 |
| comparison-boundary / baseline mismatch 风险 | yes。component bench 可能高估公开入口收益，因为真实 `extract/refineOnce` 会包含 graph solve。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 仅当 profile 或 component split 证明 production direct 仍有可见 n-link 成本时允许；否则不进入 production patch。 |
| clean adoption 是否需要同一 production boundary 内的 RVV-vs-RVV detail A/B | yes。若后续已有其它 RVV family 或 compiler auto-vectorized path，必须同边界比较。 |

## phase scope 与扩展队列

validated_scope：organized `PointXYZRGB` synthetic cloud，float color，dense width × height layout，test-only helper。

unvalidated_scope：`PointXYZRGBA`、custom RGB/RGBA point type、non-organized KNN path、GMM probability、public `extract/refineOnce`、真实 production dispatch、`Scalar=double` 不适用。

point_type_expansion_queue：Phase 010 后再判断是否需要从 `PointXYZRGB` 扩到 `PointXYZRGBA` 和 traits-based RGB/RGBA gate；每个扩展都需要 correctness、fallback、bench、asm、board 和 Evidence Doctor。
