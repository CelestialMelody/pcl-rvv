# extract_polygonal_prism_data 阶段索引

## 当前默认恢复入口

默认恢复到 Phase 080 closeout submit audit（收尾提交流程审计）之后。当前 production patch（生产补丁）已经作为 adopted production behavior（已采用生产行为）保留：真实 `ExtractPolygonalPrismData<PointT>::segment(PointIndices&)` 在 `__RVV10__` 构建下接入 full-scan polygon RVV 路径，覆盖合法 single / nested polygon、dense ordered indices 和 indexed gather。production public（公开入口生产证据）在 Milkv-Jupiter repeated board（重复板卡性能测试）上为 positive bucket：single polygon dense / indexed median 均为 1.75x；nested polygon dense median 2.18x，indexed median 2.13x；`PointXYZI` median 1.85x，`PointXYZRGB` median 1.83x，`PointXYZRGBA` median 1.84x。Phase 070 已把规模阈值从 `indices_->size() >= 64` 降到 `>= 32`，32 点 confirm5（5-run 确认）median 1.19x、min 1.18x、Evidence Doctor（证据体检）0 / 0 / 0。`PointXYZINormal` 因接入前 20-run 显示退化频率和长尾，当前通过 `sizeof(PointT) > 32` gate 显式回退标量；post-gate fallback confirmation（回退确认）median 1.00x，不写作 RVV 收益。

正式 production 长期主题文档已刷新：`doc-rvv/segmentation/extract_polygonal_prism_data-RVV.zh.md`。当前 `next_worker_action` 是进入 commit flow（提交流程）；`projectPoints`、wide-stride 点型或自定义点型扩展都需要新 phase 或新 topic。

## 阶段列表

| phase | status | plan | result | next action |
| --- | --- | --- | --- | --- |
| `000-current-state-and-scaffold` | complete | `doc/phases/000-current-state-and-scaffold/plan.zh.md` | `doc/phases/000-current-state-and-scaffold/result.zh.md` | 已升级到 Phase 010 |
| `010-production-scope-audit` | complete | `doc/phases/010-production-scope-audit/plan.zh.md` | `doc/phases/010-production-scope-audit/result.zh.md` | dense diagnostic 已转为 Phase 040 生产证据输入 |
| `020-production-integration-plan` | complete | `doc/phases/020-production-integration-plan/plan.zh.md` | `doc/phases/020-production-integration-plan/result.zh.md` | 用户已授权进入 PI2-PI5 clean split |
| `030-arbitrary-indices-gather-diagnostic` | complete / diagnostic-positive | `doc/phases/030-arbitrary-indices-gather-diagnostic/plan.zh.md` | `doc/phases/030-arbitrary-indices-gather-diagnostic/result.zh.md` | indexed diagnostic 已转为 Phase 040 生产证据输入 |
| `040-production-integration-loop` | complete / PI5 positive | `doc/phases/040-production-integration-loop/plan.zh.md` | `doc/phases/040-production-integration-loop/result.zh.md` | 用户已确认正收益可采纳，进入 Phase 045 |
| `045-production-closeout-after-adoption` | complete / adopted production | `doc/phases/045-production-closeout-after-adoption/plan.zh.md` | `doc/phases/045-production-closeout-after-adoption/result.zh.md` | 默认下一 phase：`050-concave-hull-xor` |
| `050-concave-hull-xor` | complete / adopted production extension | `doc/phases/050-concave-hull-xor/plan.zh.md` | `doc/phases/050-concave-hull-xor/result.zh.md` | 默认下一 phase：`060-point-type-expansion` |
| `060-point-type-expansion` | complete / adopted partial point type expansion | `doc/phases/060-point-type-expansion/plan.zh.md` | `doc/phases/060-point-type-expansion/result.zh.md` | 默认下一 phase：`070-size-threshold-tuning` |
| `070-size-threshold-tuning` | complete / adopted threshold 32 | `doc/phases/070-size-threshold-tuning/plan.zh.md` | `doc/phases/070-size-threshold-tuning/result.zh.md` | 已升级到 Phase 080 收尾审计 |
| `080-closeout-submit-audit` | complete / ready for commit flow | `doc/phases/080-closeout-submit-audit/plan.zh.md` | `doc/phases/080-closeout-submit-audit/result.zh.md` | 当前 topic 可结束并进入提交流程 |

## 文档归属

phase plan/result 保存阶段探索、证据和继续 / 停止判断；`doc/optimization-roadmap.zh.md` 保存跨阶段候选搜索空间；`doc/testing-overview.zh.md`、`doc/correctness-tests.zh.md`、`doc/benchmark-and-evidence.zh.md`、`doc/optimization-evidence.zh.md` 和 `doc/test-support-code-map.zh.md` 保存测试、bench、证据和代码地图职责；`doc/extract_polygonal_prism_data-evaluation.zh.md` 保存函数级评估、production direct（真实生产路径证据）审计和 adopted 判断。正式 `doc-rvv` 长期主题文档只保存当前 adopted production 行为、fallback、证据链和后续扩展边界。
