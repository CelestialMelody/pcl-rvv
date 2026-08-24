# Phase 060 Plan: PI2 production patch and direct evidence

## 阶段意图和边界

本阶段进入 PI2 production patch（生产补丁）和 post-production direct evidence（接入后真实生产路径证据）闭环。用户已授权推进 `io/include/pcl/compression/color_coding.h`，但 PI5 采纳仍是用户检查点；本阶段不能自行把生产行为写成 adopted。

本阶段只覆盖 phase 030 冻结范围：

- `encodeAverageOfPoints`：`pcl::PointXYZRGBA` / RGBA offset 命中时用 RVV indexed gather（按索引离散加载）和 vector reduction（向量规约）求 RGB 平均值。
- `encodePoints`：只把第一遍 average pass（平均颜色求和）切到 RVV；differential byte stream（差分字节流）仍按原标量循环 `push_back`。
- `setDefaultColor`：`pcl::PointXYZRGBA` / RGBA offset 命中且规模足够时用 RVV strided store（跨步写）填充默认颜色。

显式排除：

- `decodePoints` 保持原标量路径。phase 050 的 direct decode 仍弱 / 不稳定，staged-store decode 已被 Evidence Doctor Error 拒绝。
- 完整 `OctreePointCloudCompression` public entry（公开压缩入口）、octree traversal（八叉树遍历）和 entropy coder（熵编码器）。
- 泛型 `PointT` clean adoption（干净泛型采纳）。本阶段只做 exact `pcl::PointXYZRGBA` 窄 gate；其它点类型自然 fallback（回退到标量）。

## 当前状态清单

| item | current state |
| --- | --- |
| pre-production evidence | phase 050 production-shaped labels：`ps_encode_average_leaf257` median 2.5073x，`ps_encode_average_leaf4096` median 1.9590x，`ps_encode_points_leaf257` median 1.3050x，`ps_encode_points_leaf4096` median 1.1671x，`ps_set_default_color_4096` median 1.1655x |
| decode | staged-store decode rejected；direct decode weak / unstable；本阶段不修改 |
| production source | `io/include/pcl/compression/color_coding.h` 当前纯标量 |
| tests | 只有 component / production-shaped diagnostic；缺 production direct dispatch gate |
| bench | 缺真实 `ColorCoding<pcl::PointXYZRGBA>` public method labels |
| Evidence Doctor / registry | 当前 run label `board-color-coding-component-repeat-phase050` fresh，但它不是 production direct |

## 假设与候选族

候选族为 `narrow PointXYZRGBA production RVV`：

- exact type gate（具体点型 gate）：只允许 `std::is_same_v<PointT, pcl::PointXYZRGBA>` 且 `rgba_offset_arg == offsetof(pcl::PointXYZRGBA, rgba)`。
- indexed gather 使用 32-bit byte offset；`inputCloud_arg->size() <= UINT32_MAX / sizeof(PointT)` 时才进入 RVV。
- 规模阈值采用保守 gate：indexed leaf 至少 32 个点；default color 至少 64 个点。小规模 fallback。
- 标量主体抽到 `*_Std` helper，公开入口只负责短路 dispatch（分流）和 fallback。

## 优化矩阵

| candidate family | row source | point type / layout | test | bench | board | asm | doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production encode average RVV | indexed leaf gather | exact `pcl::PointXYZRGBA` RGBA field | new production direct TEST + fallback TEST | `prod_encode_average_leaf257/4096` | repeated phase060 | production helper RVV attribution | required | pending |
| production encode points average RVV | indexed leaf gather + scalar diff push | exact `pcl::PointXYZRGBA` RGBA field | new production direct TEST | `prod_encode_points_leaf257/4096` | repeated phase060 | production helper RVV attribution | required | pending |
| production default color RVV | contiguous output range | exact `pcl::PointXYZRGBA` RGBA field | new production direct TEST + small fallback TEST | `prod_set_default_color_4096` | repeated phase060 | production helper RVV attribution | required | pending |
| decode scalar | contiguous output range | any existing scalar-supported point type | existing tests | no new production label | not_applicable | not_applicable | not_applicable | keep scalar |

## 实现和测试动作

1. RED：新增 production direct correctness test（真实生产路径正确性测试），在 RVV 构建下要求 `ColorCoding<pcl::PointXYZRGBA>` 的测试钩子显示 encode/default RVV 分流命中；当前生产头没有该钩子，预期编译失败。
2. GREEN：在 `color_coding.h` 中抽出 `encodeAverageOfPointsStd`、`encodePointsStd`、`setDefaultColorStd`，并在 `__RVV10__` 下新增窄范围 RVV helper。`decodePoints` 不拆、不改。
3. fallback tests：覆盖小规模 leaf、错误 RGBA offset、非 `PointXYZRGBA` 点型或非 RVV 构建的标量路径。
4. production direct bench：新增真实生产方法 labels，并更新 manifest 的 evidence role / case kind。
5. 验证：QEMU 只跑 correctness / log-shape smoke；性能结论只来自板卡 repeated summary。

## Evidence Doctor 和 registry 规则

本阶段会生成 phase060 run：

- summary：`test-rvv/io/color_coding/log/board/production_repeat_5/summary.md`
- manifest：`test-rvv/io/color_coding/log/board/production_repeat_5/evidence_manifest.json`
- doctor：`test-rvv/io/color_coding/log/board/production_repeat_5/evidence_doctor.md`
- registry：`test-rvv/io/color_coding/log/evidence_registry.json`

Evidence Doctor Error 阻塞 PI5 正向建议；Warning 必须解释并决定是否降级为 bounded production candidate（有界生产候选）。

## 板卡复跑预算和决策桶

- run count：5。
- iterations：20。
- warmup：3。
- budget：默认 1 组 5-run；若 production label 正好跨 positive / neutral 桶摇摆，可再追加 1 组，最多 10-run equivalent（等效十次复跑）。
- positive：median > 1.05 且 `B/A < 1` 频率不高于 1/5。
- weak-positive：median 在 1.00-1.05 或有 1/5 退化但可解释。
- neutral / negative：median <= 1.00 或多次退化。
- unstable：min/max 长尾或跨桶摇摆，且预算耗尽后仍不能稳定判断。

## 继续 / 停止条件

继续到 PI5 前必须完成 production direct tests、fallback tests、asm、board repeated、Evidence Doctor、registry 和文档刷新。若 correctness 失败或生产 patch 维护边界明显不成立，暂停并报告保留 / 回滚选择；不自行回滚。

PI5 是用户检查点：无论证据正向还是负向，保留当前 patch，报告 diff、命令、summary / Doctor 和建议，等待用户确认采纳或回滚。

## 文档更新清单

- phase 060 result。
- phase README / optimization matrix / roadmap。
- topic-local evaluation、benchmark-and-evidence、optimization-evidence、testing overview、correctness tests、code map。
- `doc-rvv/library-screening/io/io-retained-candidate-rescreen.zh.md` 状态行。
- 若 post-integration evidence 支持候选，创建 `doc-rvv/io/color_coding-RVV.zh.md`，但标记为 production-candidate pending PI5 user confirmation，不能写 adopted。
