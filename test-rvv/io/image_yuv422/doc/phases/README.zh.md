# image_yuv422 RVV 阶段索引

## 当前恢复入口

| phase | 状态 | 默认恢复动作 | 说明 |
| --- | --- | --- | --- |
| `000-current-state-and-gaps` | done / attempted | 见 `000-current-state-and-gaps/result.zh.md`。 | 建立 production-shaped diagnostic（生产形态诊断）测试、bench（性能测试）和首个 6×`vsse8` RGB candidate；灰度 RVV rejected，downsample deferred。 |
| `010-rgb-segment-store-probe` | done / partial-production-candidate | 等待用户确认是否进入 PI1 production integration plan（生产接入计划）。 | `vssseg3e8` 写回在 RGB full-size focused board smoke 中约 `1.46x`，但仍不是 production direct（真实生产路径证据）。 |
| `020-production-integration-plan` | adopted production behavior | 进入 S11 closeout，并继续审计未覆盖候选。 | 已接入并采纳 `ImageYUV422::fillRGB` full-size even-width RVV；production public 5-run mean `2.0297x`，Evidence Doctor `Errors=0, Warnings=0, Suggestions=0`。 |
| `030-downsample-rvv-probe` | done / partial-production-candidate | 进入 `040-downsample-production-integration`。 | RGB downsample test helper 5-run mean `1.1821x`，median `1.1477x`，Doctor `Errors=0, Warnings=1`；需要 production direct 验证。 |
| `040-downsample-production-integration` | adopted production behavior | S11 closeout 已同步，继续审计剩余候选。 | RGB downsample production public 5-run mean `3.8150x`，median `3.9294x`，Doctor `Errors=0, Warnings=1`；用户已确认采纳。 |
| `050-prod-rgb-full-padded-evidence` | done / stop-for-review | 整理 Handoff / ready-for-review。 | `prod_rgb_full_padded_640x480` production public 5-run mean `2.1030x`，median `2.1012x`，Doctor `Errors=0, Warnings=0`；当前文件内无新的高价值未阻塞优化。 |

## 文档归属

| 文档 | 主归属 |
| --- | --- |
| `doc/image_yuv422-evaluation.zh.md` | S2 函数级评估、Traceability Map（可追踪性地图）和 production 接入前置条件。 |
| `doc/phases/000-current-state-and-gaps/plan.zh.md` | 当前阶段修改前的范围、测试计划、板卡预算和停止条件。 |
| `doc/phases/000-current-state-and-gaps/result.zh.md` | Phase 000 的实际证据、负向候选和进入 Phase 010 的理由。 |
| `doc/phases/010-rgb-segment-store-probe/plan.zh.md` | Segment-store 写回探针的范围、证据计划和停止条件。 |
| `doc/phases/010-rgb-segment-store-probe/result.zh.md` | Segment-store 写回探针的 EvidenceDecision 和生产接入前置条件。 |
| `doc/phases/020-production-integration-plan/plan.zh.md` | 生产接入闭环 PI1-PI5 的范围、fallback 和证据计划。 |
| `doc/phases/020-production-integration-plan/result.zh.md` | 生产补丁、production direct 证据、PI5 判断和用户确认边界。 |
| `doc/phases/030-downsample-rvv-probe/plan.zh.md` | RGB downsample RVV 诊断探针的范围、证据计划和停止条件。 |
| `doc/phases/030-downsample-rvv-probe/result.zh.md` | downsample 候选的 correctness、asm、board、Doctor 和生产探针判断。 |
| `doc/phases/040-downsample-production-integration/plan.zh.md` | RGB downsample 生产接入闭环 PI1-PI5 的范围、fallback 和证据计划。 |
| `doc/phases/040-downsample-production-integration/result.zh.md` | downsample production patch 的生产证据、采纳判断和后续边界。 |
| `doc/phases/050-prod-rgb-full-padded-evidence/plan.zh.md` | full-size padded output production repeated 证据补强计划。 |
| `doc/phases/050-prod-rgb-full-padded-evidence/result.zh.md` | full-size padded output production repeated 结果和剩余候选 stop decision。 |
| `doc/phases/optimization-matrix.zh.md` | 跨阶段候选、证据状态和下一动作矩阵。 |
| `doc/optimization-roadmap.zh.md` | 后续候选搜索空间和默认恢复队列。 |
