# correspondence_types 优化路线图

## 当前边界

`correspondence_types` 当前只评估 `registration/include/pcl/registration/impl/correspondence_types.hpp` 中的三个 inline helper。当前 production（生产源码）保持标量；test-rvv（RVV 测试资产）已经完成 QEMU correctness（QEMU 正确性验证）、QEMU smoke（仿真器小型验证）、反汇编 smoke、diagnostic board repeated benchmark（诊断板卡重复性能测试）、production direct probe（真实生产路径探针）和 Evidence Doctor（证据体检）检查。Phase 011 曾临时接入 `getQueryIndices` / `getMatchIndices` 生产探针，但 production direct board evidence 仍为负向，临时 patch 已回退。当前不发布 `doc-rvv`。

## 候选搜索空间

| candidate family | idea source | applies to | expected benefit | risk / unknown | required evidence | status | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| strided-index-extract | 当前源码中 `getQueryIndices` / `getMatchIndices` 是纯字段抽取循环 | `Correspondence` AoS（结构数组）中 query/match 32-bit 字段 | 用 RVV `vlse32` 跨步加载和连续 `vse32` 写出替代标量循环 | 只是内存搬运，可能受带宽限制；production inline helper 真实边界仍可能不同于 test-only candidate | QEMU correctness、asm 中 `vlse32` / `vse32`、diagnostic 5-run board repeated、production direct 5-run board repeated | `rejected_by_production_probe_negative` | none |
| distance-stats-reduction | 当前源码中距离均值 / 标准差是双 accumulator + sqrt | `distance` float 字段，输出 double mean/stddev | 减少逐元素标量循环开销 | 标量平方项是 float 乘法后转 double；RVV reduction 改变累加树；`n == 1` 既有行为敏感 | 数值预算、same-chain（同构链路）对拍、asm `vfmul.vv` / `vfw*`、board A/B | `not_run_after_index_negative_no_production` | 仅作为可选 profiling / ablation 另开阶段 |
| production-dispatch | 用户指出 diagnostic negative 不能直接禁止 production probe 后新增 | 真实 public helper | 临时用 `__RVV10__` gate 接入 `getQueryIndices` / `getMatchIndices`，再按证据决定是否保留 | inline helper 体积、非 RVV build、small input fallback、production asm attribution；若负向必须回退 | Phase 011 plan、production direct tests、board production bench、Evidence Doctor | `attempted_and_rolled_back_after_negative` | none |
| compiler-auto-vectorization-report | S2 可选诊断 | 三个标量循环 | 判断手写 RVV 是否可能被编译器已有向量化覆盖 | missed-vectorization report 不能替代 objdump，也不能改变板卡负向结果 | `make generate_vec_report` 摘要 | `not_required_after_board_negative` | none |

## 阶段反思新增路线

| phase | new idea | why now | evidence needed | priority |
| --- | --- | --- | --- | --- |
| `000-current-state-and-gaps` | 补 topic-local QEMU smoke manifest | Markdown summary doctor 只能得到 metadata 缺失；manifest 能让 Evidence Doctor 检查 case、checksum、run contract 和证据角色 | `script/generate_qemu_evidence_manifest.py`、`make run_evidence_doctor_qemu` | `done` |
| `000-current-state-and-gaps` | 先板卡验证 index extraction | QEMU correctness 和 asm 已支持路径存在，但性能主门槛仍缺目标硬件 | 5-run repeated board summary、checksum、Evidence Doctor、asm 归属 | `high` |
| `010-board-diagnostic-and-production-decision` | 不继续 distance stats board bench | 最接近 production 的 index extraction 已在板卡上稳定负向；distance stats 额外带 reduction tree 和数值风险，不能改变 no-production 主结论 | 若用户显式要求解释负向原因，可另开 profiling / ablation 小阶段 | `closed_for_default_flow` |
| `011-production-probe-boundary-check` | diagnostic negative 后补 production direct probe | 历史 weighted topic 说明 diagnostic negative 不能直接禁止 production probe；当前 topic 需要同真实 helper 边界复核 | 5-run production direct board summary、Evidence Doctor、临时 patch 回退状态 | `done_negative_no_production` |

## 暂缓 / 拒绝路线

| candidate family | reason | resume condition |
| --- | --- | --- |
| production-dispatch | Phase 011 已临时接入 production helper 并完成 production direct board probe；三条 case 仍为 `negative` bucket，临时 patch 已回退 | 只有后续另一个 code shape 在同 production boundary 至少达到 `weak_positive`，且用户明确授权再做新的 production probe |
| strided-index-extract | Phase 010 diagnostic 和 Phase 011 production direct 两个边界均未达到 `weak_positive`；Evidence Doctor 都报告退化频率 Error | 仅当后续另开 profiling / ablation 并发现可验证的新 code shape，且重新通过同边界板卡 repeated summary |
| distance-stats-reduction | 首选 candidate family 已稳定负向；distance stats 的 reduction 数值预算和 production 维护成本更高 | 用户明确要求解释或继续 reduction 候选，并接受它不会改变 index extraction no-production 结论 |
| full generic correspondence algorithm | 目标文件只包含 `pcl::Correspondences` helper，不涉及点类型 traits 或 KdTree 查询 | 另开 correspondence estimation / rejection topic |
| QEMU timing as performance evidence | QEMU 只证明可运行、正确性和日志形状；当前规则禁止把 QEMU bench compare 当性能结论 | 需要 board / target hardware repeated benchmark |

## 默认恢复队列

| order | phase | scope | status | blocker / stop condition | next action |
| ---: | --- | --- | --- | --- | --- |
| 1 | `000-current-state-and-gaps` | topic 测试资产、evaluation、QEMU correctness、asm smoke、Evidence Doctor manifest | `completed_diagnostic` | 无 | 从 `doc/phases/000-current-state-and-gaps/result.zh.md` 恢复 |
| 2 | `010-board-diagnostic-and-production-decision` | board repeated diagnostic 和 PI1 初始判断 | `completed_no_production_historical_input` | diagnostic 负向但不能单独禁止 production probe | 已由 Phase 011 复核真实 production boundary |
| 3 | `011-production-probe-boundary-check` | 真实 production helper 临时接入、production direct correctness、asm 和 board repeated | `completed_probe_rolled_back` | production direct 仍为 `negative`，不满足保留门槛 | 默认进入 reviewer 审查；保持 production 标量 |
| 4 | optional profiling / ablation | 只解释负向原因，不作为默认生产接入 | `turn_stop_deferred` | 需要用户明确要求继续花时间定位瓶颈 | 可按 `strided-index-extract` 的 load/store、`vsetvli`、内存带宽、小规模 gate 和标量 baseline 做窄消融 |
