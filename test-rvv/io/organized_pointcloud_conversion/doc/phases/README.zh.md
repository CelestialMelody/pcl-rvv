# organized_pointcloud_conversion 阶段索引

## 当前默认恢复入口

`050-adoption-closeout` 已把用户确认采纳后的 conversion production patch 同步为 adopted production behavior。`060-end-to-end-encode-pointcloud-bench` 已证明 encode-shaped 压缩链路仍有约 1.15x 正向收益；`070-analyze-organized-cloud-component` 已证明 `analyzeOrganizedCloud` 组件诊断约 3.5x 正向；`080-analyze-production-probe` 已接入 protected production-detail helper，并由 `090-analyze-adoption-closeout` 按当前 goal 确认为 adopted production behavior。当前默认恢复入口是 `090-analyze-adoption-closeout/result.zh.md`。

Decode/backprojection v0 在 Phase 030 正确但负向，保持 rejected；除非出现新的 output store / staging 方案，否则不恢复。

## 阶段表

| phase | 状态 | 作用 | plan | result |
| --- | --- | --- | --- | --- |
| 000-current-state-and-gaps | completed | 建立当前状态、首个 PointXYZ cloud->disparity 诊断候选和证据链 | `000-current-state-and-gaps/plan.zh.md` | `000-current-state-and-gaps/result.zh.md` |
| 010-colored-cloud-to-disparity | completed | 扩展到 PointXYZRGB 的 disparity + RGB/mono diagnostic，得到 weak-positive 诊断证据 | `010-colored-cloud-to-disparity/plan.zh.md` | `010-colored-cloud-to-disparity/result.zh.md` |
| 020-fused-colored-pack | completed | 复用 RVV finite/disparity pass，把 colored RGB/mono 写出合并到一次扫描，得到正向诊断证据 | `020-fused-colored-pack/plan.zh.md` | `020-fused-colored-pack/result.zh.md` |
| 030-decode-backprojection | completed | 覆盖 disparity image -> PointXYZ cloud 的反投影诊断，v0 因 0.85x-0.91x 退化被拒绝 | `030-decode-backprojection/plan.zh.md` | `030-decode-backprojection/result.zh.md` |
| 040-encode-production-integration-plan | completed | 完成 encode-only production probe；PI5 证据支持采纳 | `040-encode-production-integration-plan/plan.zh.md` | `040-encode-production-integration-plan/result.zh.md` |
| 050-adoption-closeout | completed | 用户确认采纳后补 S11 production closeout、正式 `doc-rvv` 和 doc suite | `050-adoption-closeout/plan.zh.md` | `050-adoption-closeout/result.zh.md` |
| 060-end-to-end-encode-pointcloud-bench | completed | 验证 encode-shaped 压缩链路是否仍受益 | `060-end-to-end-encode-pointcloud-bench/plan.zh.md` | `060-end-to-end-encode-pointcloud-bench/result.zh.md` |
| 070-analyze-organized-cloud-component | completed | 单独验证 `analyzeOrganizedCloud` max-depth / focal-length 组件是否值得 production probe | `070-analyze-organized-cloud-component/plan.zh.md` | `070-analyze-organized-cloud-component/result.zh.md` |
| 080-analyze-production-probe | completed / PI5 positive | 接入 analyze production-detail helper，完成 correctness、asm、board repeated 和 Evidence Doctor | `080-analyze-production-probe/plan.zh.md` | `080-analyze-production-probe/result.zh.md` |
| 090-analyze-adoption-closeout | completed | 用户确认采纳后同步 analyze detail adopted 状态、长期 `doc-rvv`、evaluation、matrix 和队列表 | `090-analyze-adoption-closeout/plan.zh.md` | `090-analyze-adoption-closeout/result.zh.md` |

## 文档归属

| 文档 | 角色 |
| --- | --- |
| `../organized_pointcloud_conversion-evaluation.zh.md` | EvidenceDecision、Traceability Map、未覆盖范围 |
| `../optimization-roadmap.zh.md` | 跨阶段候选搜索空间和默认恢复队列 |
| `optimization-matrix.zh.md` | candidate / point type / evidence 状态矩阵 |
| `../../README.zh.md` | topic 导航、常用命令和证据白名单 |
| `doc-rvv/io/organized_pointcloud_conversion-RVV.zh.md` | adopted production behavior 的长期维护说明 |
