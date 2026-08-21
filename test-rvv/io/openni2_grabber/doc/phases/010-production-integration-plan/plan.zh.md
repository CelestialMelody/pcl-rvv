# Phase 010 Plan: production-integration-plan

## 阶段意图和边界

本阶段只冻结 production integration（生产接入）计划，不修改 `io/src/openni2_grabber.cpp`。Phase 000 证据支持的唯一候选是 `convertToXYZPointCloud` 中 `PointXYZ` 同尺寸 depth map 的 depth projection RVV path。本阶段不覆盖 `convertToXYZRGBPointCloud`、`convertToXYZIPointCloud`、depth/image mismatch、RGB overlay、IR intensity、legacy `io/src/openni_grabber.cpp` 或 public API 变化。

## 候选生产边界

| item | planned value |
| --- | --- |
| production entry | `pcl::io::OpenNI2Grabber::convertToXYZPointCloud(const DepthImage::Ptr&)` |
| row source | contiguous resized-or-original depth map after existing `fillDepthImageRaw` branch |
| point type | exact `pcl::PointXYZ`，非模板 |
| scalar | float xyz fields |
| layout | `std::vector<PointXYZ>` AoS，stride 为 `sizeof(PointXYZ)` |
| RVV gate | `__RVV10__` only；非 RVV 构建保留纯标量 |
| runtime fallback | helper 内或入口附近自然 fallback；小规模、异常尺寸、不能确认 contiguous 时走 Std |
| forbidden expansion | RGB/RGBA、IR、OpenNI legacy、公共 API、新公共 RVV wrapper |

## 生产实现草案

PI2 若获授权，建议在 `io/src/openni2_grabber.cpp` 中抽出邻近 helper：

| helper | role |
| --- | --- |
| `fillXYZPointCloudStd` | 保存当前标量内层循环，参数显式传入 depth map、尺寸、camera constants、invalid values 和 output points。 |
| `fillXYZPointCloudRVV` | `__RVV10__` 下的窄 RVV helper，使用 VL chunk、invalid mask、u/v lane、mm->m 和 masked strided store。 |
| public entry dispatch | 完成 header、resize buffer、camera constants 后，RVV helper 命中则返回；否则调用 Std helper。 |

生产注释只说明覆盖范围、fallback 和 AoS stride；不复制诊断文档中的长说明。

## PI3 测试计划

| target / test | requirement |
| --- | --- |
| production direct correctness | 新增测试或 production-shaped direct helper，验证真实生产 helper 与 Std bitwise 对齐。 |
| fallback correctness | 非 RVV build、resize buffer branch、小规模或 fallback gate 至少有一个覆盖点。 |
| existing diagnostic | `make run_test_compare` 继续通过，确保 test-only oracle 未漂移。 |
| QEMU smoke | 只跑 correctness 和小迭代 RVV bench smoke，不写性能结论。 |

## PI4 证据计划

| evidence | requirement |
| --- | --- |
| asm | 反汇编要能归因到 production helper 或 public entry 附近 RVV 指令，至少包含 `vle16`、`vfcvt`、`vfmul`、`vmseq`、`vsse32`。 |
| board repeated | 生产边界重新跑 5-run repeated，不能复用 Phase 000 diagnostic 数字作为最终结论。 |
| Evidence Doctor | 优先用 JSON manifest；至少补 iterations、warmup、run_count、device、boundary、checksum policy 和 binary identity。 |
| registry | 新增或等价记录 evidence freshness；没有 registry 时 Handoff 写人工检查。 |

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 production-shaped diagnostic；Phase 010 后若进入 PI2，必须升级为 production direct。 |
| A/B boundary | Phase 000 是 test helper；PI4 必须是 production helper / public-adjacent entry。 |
| 当前决策问题 | 是否允许尝试最小 production patch，而不是是否立即采纳。 |
| diagnostic 是否可外推到 production | 只能支持尝试。生产结论必须以 PI4/PI5 证据为准。 |
| comparison-boundary / baseline mismatch 风险 | 有：真实 entry 包含 device 参数、resize buffer、header、sensor orientation 和 signal path。 |
| 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段只对 positive depth path 允许；其它候选不允许。 |
| clean adoption 是否需要同一 production boundary 内 A/B | 需要 production Std/RVV repeated 和 Doctor；PI5 后还需要用户确认采纳。 |

## 暂停条件

进入 PI2 前需要用户明确授权修改 production 源码。即使 PI2-PI5 后证据正向，也必须停在 PI5 的用户检查点，展示 production diff、测试命令、板卡 / Doctor 结果和拟议下一步；未获确认前不得写 adopted，也不得创建 `doc-rvv/io/openni2_grabber-RVV.zh.md`。

## 下一步

默认下一动作是等待生产源码修改授权；若授权，按本计划进入 PI2-PI5。若未授权，本 topic 停在 `partial-production-candidate / PI1 plan ready`。
