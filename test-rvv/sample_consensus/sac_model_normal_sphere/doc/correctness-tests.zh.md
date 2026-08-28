# sac_model_normal_sphere correctness tests

## 本文职责

本文解释 gtest 的输入、断言和证明范围。它不记录性能数值，不承担 production decision（生产接入判断）。

## 测试文件分工

| 文件 | 职责 |
| --- | --- |
| `src/test_sac_model_normal_sphere.cpp` | gtest case 入口。 |
| `include/test_sac_model_normal_sphere.h` | fixture、normal 构造、公共断言 helper。 |
| `include/impl/sac_model_normal_sphere_access.hpp` | 测试专用标量参考、历史 RVV candidate helper 和 production helper direct checks。 |

## 共同输入和断言

所有 correctness case 使用 `normalSphereCoefficients()` 生成球心 `(1.0, -2.0, 0.5)` 和半径 `2.0`。source cloud 通过 `indices_` 乱序访问，normal cloud 与 source 使用同一 index。核心断言比较：

- public entry 与测试专用标量参考的 inliers、count、error 和 dense distances。
- production RVV detail helper 与同一参考链路的 inliers、count、error 和 dense distances。
- RVV candidate 与同一标量参考的 inliers、count、error 和 dense distances。
- select 输出顺序必须保持 `indices_` 顺序。

RVV candidate 的误差阈值为 `1e-4`，public 与参考链路使用更紧的 `1e-6`。

## TEST 字典

| TEST / filter | 输入 | 被测路径 | 断言 | 证明范围 | 不能证明 |
| --- | --- | --- | --- | --- | --- |
| `PublicEntriesMatchReferenceOnNormalBoundaries` | `PointXYZ + Normal`，乱序 indices，含 early continue 和 normal angle 样本 | public entries、标量参考、count/select/getDistances candidate | count、inliers、errors、dense distances 一致 | 基础 normal-sphere 公式和输出合同正确 | 其它点型、production RVV dispatch、性能 |
| `PointXYZILayoutMatchesReference` | `PointXYZI + Normal` | 同上 | 同上 | intensity 附加字段不破坏 xyz AoS layout（结构数组布局） | RGB/RGBA、自定义点型 |
| `PointXYZRGBAndRGBALayoutsMatchReference` | `PointXYZRGB + Normal`、`PointXYZRGBA + Normal` | 同上 | 同上 | RGB/RGBA 颜色字段不破坏 xyz 读取和独立 normal cloud 对拍 | 自定义点型、source 自带 normal 字段 |
| `DegenerateCenterDirectionKeepsScalarBoundary` | 人工把一个 source 点放到球心 | public entries、标量参考、candidate | 输出不扩大 inlier 语义 | `n_dir = p - center` 退化方向按标量边界处理 | 其它 NaN/Inf 或非法 index |
| `VCompressSelectCandidatePreservesOrderAndErrors` | RVV 构建下的 `PointXYZ + Normal` | scalar-writeback select 与 `vcompress` select | inlier 顺序和 error 写回一致 | `vcompress`（RVV 保序压缩）写回候选保持输出合同 | 非 RVV 构建、production dispatch、其它点型的专用 vcompress correctness |
| `ProductionRVVDetailHelpersMatchPublicReference` | RVV 构建下四种 source 点型 + `pcl::Normal`，大输入，乱序 indices | `countWithinDistanceRVVNormalSphere`、`selectWithinDistanceRVVNormalSphere`、`getDistancesToModelRVVNormalSphere` | RVV helper 返回 hit，并与 public/reference 输出一致 | 证明 Phase060 production detail helper 在当前 gate 下真实命中并保持语义 | 非 RVV 构建、其它 normal 点型、自定义点型 |
| `ProductionPublicEntriesMatchReferenceOnLargeInputs` | 四种 source 点型 + `pcl::Normal`，大输入，触发规模 gate | 三条 public entry | public 输出与参考链路一致 | RVV 构建下覆盖大输入 dispatch，Std 构建下保护 Standard helper | 不证明性能；性能来自板卡 repeated |
| `ProductionFallbacksKeepPublicReferenceSemantics` | 小输入、normal cloud 覆盖不足等 gate 失败样本 | `canUseRVVNormalSphere` 和三条 public entry | RVV gate 返回 false 时 public 输出仍与参考一致 | 证明小规模和 normal 覆盖不足不会误入 RVV 或改变语义 | 非当前点型全集、非法 index、NaN/Inf |

## 边界策略

当前 correctness 已覆盖 Phase060 冻结范围内的 production direct（真实生产路径）和关键 fallback（回退路径）：四种 source
点型、`pcl::Normal`、大输入 dispatch、小输入 gate 和 normal cloud 覆盖不足。它仍不覆盖非法 index、NaN/Inf source
或 normal、其它 normal 点型、`Scalar=double`、非 indexed entry shape（入口形态）和真实 workload（工作负载）。
这些缺口只有在用户指定扩展范围或另开 follow-up phase 时才成为当前阶段必做项。

## 验证命令

```bash
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_test_compare
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_normal_sphere_public_tests
```

board 侧公开入口 smoke 可通过：

```bash
make -C test-rvv/sample_consensus/sac_model_normal_sphere run_board_normal_sphere_public_tests
```
