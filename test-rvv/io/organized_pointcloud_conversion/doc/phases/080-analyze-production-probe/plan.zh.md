# Phase 080: analyze production probe plan

## 阶段意图和边界

070 证明 `analyzeOrganizedCloud` 组件诊断在板卡上约 3.5x 正向。本阶段的目标是审计是否可以把该候选推进到 bounded production probe（有界生产探针），而不是直接把 test-only helper 复制进 production。

本阶段会触碰 `io/include/pcl/compression/impl/organized_pointcloud_compression.hpp` 或相关 production detail helper；用户当前 goal 已明确要求在未命中停止条件时持续推进，所以本轮把 080 作为 bounded production probe（有界生产探针）执行。证据角色限定为 production-detail（生产 detail 边界），不把 no-OpenNI build 下的 detail helper 证据外推为完整 public entry（公开入口）证据。

## 当前状态清单

| area | 当前事实 |
| --- | --- |
| conversion production | adopted，production direct repeated 9 cases positive |
| encode-shaped end-to-end | 060 production-shaped median 1.147x-1.157x，Doctor clean |
| analyze diagnostic | 070 diagnostic median 3.483x-3.548x，Doctor clean |
| public class build | 当前 no-OpenNI cross build 无法直接实例化 `OrganizedPointCloudCompression`，因为 header 成员依赖 `openni_wrapper::ShiftToDepthConverter` |
| production boundary | 需要新增或修改 `organized_pointcloud_compression.hpp` 或邻近 production detail helper；本轮只允许 analyze scan，不扩大到 decode、OpenNI shift-to-depth 或 public API |

## 候选生产形态

| candidate | 说明 | 优点 | 风险 |
| --- | --- | --- | --- |
| protected helper 内直接分流 | 在 `analyzeOrganizedCloud` 内抽 Std/RVV helper | 最接近真实 runtime | 需要改 companion production header |
| detail free helper + protected wrapper | 把 analyze scan 做成可测试 detail helper，protected method 调用它 | 可做 production detail test / asm | public entry direct 仍受 OpenNI 构建限制 |
| test-only 保留，不接 production | 只把 070 作为诊断结论 | 零生产风险 | 放弃 3.5x 组件收益 |

## 必要证据

| evidence | target / path | 完成判据 |
| --- | --- | --- |
| correctness | 新增 production detail 或 public-shaped test | max depth / focal length 与 Std bitwise 一致 |
| fallback | 非 RVV、非 xyz AoS、small input、unorganized input | 保持当前 assert / fallback 语义 |
| asm | 新增 analyze production probe asm target | RVV 指令可归因到 analyze helper |
| board component | analyze production detail repeated | 与 070 同边界或明确降级 |
| encode-shaped context | full encode-shaped repeated after analyze patch | 证明端到端形态收益没有被新增 dispatch / helper 抵消 |
| Evidence Doctor | manifest + doctor | Errors=0；Warnings 解释后才能进入 PI5 |

## Diagnostic-to-production mismatch audit

| question | answer |
| --- | --- |
| evidence role | 当前 070 是 diagnostic；080 目标是 production-detail 或 production-shaped detail probe |
| A/B boundary | 待定：production detail helper 或 public class entry |
| 当前决策问题 | 是否将 analyze RVV 候选接入 production |
| diagnostic 是否可外推到 production | 不能直接外推；必须补 production boundary 证据 |
| comparison-boundary / baseline mismatch 风险 | public class direct 受 OpenNI 构建限制；detail helper 可能不是完整 public entry |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 070 已强正向；允许 probe，但不能越过用户授权生产文件边界 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-scalar | 需要；若只得到 detail evidence，最多停在 retained candidate |

## 实现顺序和 TDD gate

本阶段按 test-driven development（测试驱动开发）执行：先新增 production-detail 对拍测试并确认 RED（失败原因是 production detail helper 尚不存在或未接入），再实现 helper 和 protected method dispatch。

计划中的最小修改：

1. 新增或引入可独立 include 的 production detail helper，使 no-OpenNI cross build 能测试 analyze scan，而不实例化 `OrganizedPointCloudCompression` public class。
2. `analyzeOrganizedCloud` protected method 只保留 assert 和 dispatch 调用；公开 API 不变。
3. 新增 correctness（正确性）测试，比较 production detail Std 和 dispatch 的 `maxDepth / focalLength` checksum。
4. 新增或扩展 asm / bench label，保持证据角色为 production-detail。

## 继续 / 停止条件

继续条件：

- 当前 goal 授权把 production probe 扩大到 `organized_pointcloud_compression.hpp` 或邻近 production detail helper，但只限 analyze scan。
- 能设计出不依赖真实 OpenNI runtime 的 production detail correctness / asm / board 证据。

停止条件：

- 后续需要扩大到 OpenNI runtime、decode、public API 或非 analyze scan 的生产路径。
- 无法在当前 no-OpenNI cross build 中形成可审查的 production detail 或 public-shaped evidence。
- encode-shaped context 重跑显示 analyze 接入不能改善端到端形态，或 Evidence Doctor 出现未解 Error。

## 默认下一动作

先新增 production-detail RED 测试，再实现 “detail free helper + protected wrapper”。
