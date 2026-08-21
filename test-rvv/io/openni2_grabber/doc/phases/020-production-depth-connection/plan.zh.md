# Phase 020 Plan: production-depth-connection

> 本文件是执行前 phase plan（阶段计划）。PI5 用户检查点已在 `result.zh.md` 中闭合：接入后板卡证据为 positive，用户已确认“有收益即可采纳”，当前状态为 adopted production behavior（已采纳生产行为）。

## 阶段意图和边界

本阶段进入 PI2-PI5 production integration loop（生产接入闭环），只接入 Phase 000 已显示稳定收益的 `PointXYZ` depth projection（深度反投影）候选。生产入口限定为 `io/src/openni2_grabber.cpp` 中的 `OpenNI2Grabber::convertToXYZPointCloud(const DepthImage::Ptr&)`，不修改 public API（公开接口），不覆盖 RGB/RGBA、IR、depth/image mismatch、legacy `openni_grabber.cpp` 或其它 `io` topic。

本地 RISC-V 依赖树当前没有 OpenNI2 头/库，且交叉 PCL 安装的 `pcl_config.h` 未启用 `HAVE_OPENNI2`。因此本阶段的 production direct（直接生产路径证据）分两层处理：测试和 bench 直接调用 `openni2_grabber.cpp` 中的 production detail helper（生产内部 helper）；真实公开入口 dispatch（分流逻辑）写入源码，但只能在启用 OpenNI2 的完整构建中生效。这个限制不阻塞窄 production patch，但 PI5 必须把 public-entry evidence gap（公开入口证据缺口）展示给用户。

## validated_scope / unvalidated_scope

| item | value |
| --- | --- |
| validated_scope | `PointXYZ`、`float x/y/z`、连续 depth buffer、AoS `PointXYZ` 输出、640x480 主 bench case、无效 depth 写 quiet NaN。 |
| unvalidated_scope | RGB/RGBA 模板入口、IR intensity、depth/image mismatch stride mapping、legacy OpenNI、真实设备回调、OpenNI2 public entry 构造路径。 |
| point_type_expansion_queue | 当前入口返回具体 `PointXYZ`，没有泛型点型生产面；RGB/RGBA 模板路径只有在新证据变 positive 后另开 point-type expansion phase。 |
| phase_closeout_boundary | 只能关闭 depth-only production detail helper 和 public-adjacent dispatch 的 `partial-production-candidate` 条目，不能写成完整 OpenNI2 public entry 已采纳。 |

## 当前状态清单

| evidence | current state |
| --- | --- |
| Phase 000 diagnostic | `xyz_depth_full_640x480` 板卡 5-run median 1.21x，depth case 无 Doctor Error/Warning。 |
| Phase 010 | PI1 plan complete；用户本轮已授权如果板卡显示收益即可接入并接入后重测。 |
| OpenNI2 dependency | 本地/交叉依赖无 OpenNI2，public entry test 只能降级为 production detail helper direct。 |
| doc-rvv | 仍不创建；只有 PI5 证据支持且用户确认采纳后才创建长期 `doc-rvv/io/openni2_grabber-RVV.zh.md`。 |

## Diagnostic 到 Production 错配审计

| question | answer |
| --- | --- |
| evidence role | Phase 000 是 production-shaped diagnostic；Phase 020 目标是 production-detail，若 OpenNI2 依赖不可用则不能声称 production-public。 |
| A/B boundary | `openni2_grabber.cpp` 内 production detail helper 的 Std/RVV A/B；公开入口 dispatch 只做源码接入。 |
| 当前决策问题 | 已有诊断收益是否能转成有界生产补丁，以及接入后是否仍值得保留。 |
| diagnostic 是否可外推到 production | 只能外推到“值得进行有界生产探针”；最终取舍看 Phase 020 的 production-detail 板卡重测。 |
| comparison-boundary / baseline mismatch 风险 | 有：public entry 还包含 device 参数、resize buffer、header/sensor metadata；本阶段 bench 只测内层 helper。 |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 本阶段只因 depth diagnostic 已 positive 才允许；其它 case 仍不允许接入。 |
| clean adoption 是否需要同一 production boundary 内 RVV-vs-RVV detail A/B | 当前是新增 RVV-vs-scalar bounded candidate，不做 RVV family selection；PI5 仍需用户确认。 |

## 实现和测试动作

| action | artifact | expected evidence |
| --- | --- | --- |
| RED test | `src/openni2_grabber_production_detail_test.cpp`、`src/openni2_grabber_test_hook.cpp`、Makefile test source | RVV build 因 hook symbol/helper 尚不存在而失败。 |
| PI2 production patch | `io/src/openni2_grabber.cpp` | 抽出 `fillXYZPointCloudStd`，新增 `__RVV10__` 下 `fillXYZPointCloudRVV`，公开入口只分流 depth-only `PointXYZ`。 |
| PI3 correctness | `make run_test_compare` | Std/RVV hook 对拍 bitwise，RVV build path hit 为 RVV。 |
| PI4 asm | 新增 production helper asm target | 反汇编归因到 `openni2_grabber.cpp` hook/production detail helper，包含 `vle16`、`vfcvt`、`vfmul`、`vmseq`、`vsse32`。 |
| PI4 board repeated | 新增 `repeated_production_depth` targets | 5-run production-detail repeated summary、manifest、Evidence Doctor 和 registry fresh。 |
| PI5 pause | phase result / Handoff | 展示 production diff、测试命令、板卡与 Doctor 结果；当前已在 result 中闭合为用户确认采纳。 |

## 板卡复跑预算和决策桶

| item | value |
| --- | --- |
| run count | 5 repeated runs |
| iterations | `--case-filter prod_xyz_depth_full_640x480 --iterations 10 --warmup-iterations 2` |
| positive | median speedup >= 1.05x 且 checksum matched，无该 case Doctor Error。 |
| weak-positive | 1.00x < median < 1.05x 或长尾明显；保留为 PI5 人工判断，不写 clean adoption。 |
| neutral/negative | median <= 1.00x 或多数 runs 低于 1；PI5 建议回滚，但不自动回滚。 |
| unstable | 方向摇摆且预算耗尽；降级证据并停在 PI5。 |

## Evidence Doctor 和 registry

生产重测使用 `log/board/repeated_production_depth/summary.md`、`evidence_manifest.json`、`evidence_doctor.md` 和 topic-local `log/evidence_registry.json`。若 Doctor 出现 Error，先修复或降级；不能用未解释 Error 关闭阶段。

## 继续 / 停止条件

本阶段默认持续推进到 PI5。只有以下情况可中途停止：OpenNI2/工具链/板卡不可用且无法完成当前 evidence、production helper 编译被 PCL layout 阻塞、Doctor Error 无法归因、或 dirty isolation 不安全。PI5 用户检查点已在 `result.zh.md` 中闭合，当前长期 `doc-rvv` 按 adopted production behavior 维护。
