# Phase 060: production integration execution

## 阶段意图和边界

本阶段在用户明确授权后执行 Phase 050 冻结的 PI2-PI5 production integration loop（生产接入闭环）。本阶段允许修改 `sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp`，目标是在真实 public entry（公开入口）下接入三条 RVV 路径：

- `countWithinDistance`：indexed source / normal gather（按索引离散加载源点和法线）+ RVV distance + `vcpop`。
- `selectWithinDistance`：RVV distance + `vcompress` 保序写回 `inliers` 和 `error_sqr_dists_`。
- `getDistancesToModel`：RVV distance + `vfwcvt.f.f.v` + dense double store（连续 double 写回）。

本阶段只覆盖 `PointXYZ`、`PointXYZI`、`PointXYZRGB`、`PointXYZRGBA` 这类 `RVVXYZAoSFloatLayout<PointT>` source 点型，normal cloud 初始只覆盖 `pcl::Normal`。其它 normal 点型、自定义 source 点型、`Scalar=double`、非 `indices_` 入口和 public API 变化不在本阶段范围内。

## 当前状态清单

| 输入 | 状态 | 本阶段动作 |
| --- | --- | --- |
| Phase 050 PI1 plan | `PI1 plan ready`，PI2 曾等待用户授权。 | 用户已授权继续 PI2-PI5，并说明接入后板卡结果有收益即可采纳。 |
| production 源码 | 三入口仍是标量循环；没有 normal-sphere RVV helper。 | 抽出 `StandardNormalSphere` helper，新增 `RVVNormalSphere` helper，public entry 先尝试 RVV、失败回退标量。 |
| topic-local tests | 已有 public entry correctness，但不能证明 RVV production helper 被调用。 | 先新增 production detail helper direct tests（生产内部 helper 直连测试），观察 RED 后实现。 |
| bench / board | Phase 000-030 是测试专用 diagnostic；不是 production direct。 | 增加 production public bench label，跑板卡 repeated summary 和 Evidence Doctor。 |
| `doc-rvv` 长期主题文档 | 仍为 not_applicable。 | 若 PI4 production board 有收益且无 Evidence Doctor Error，本阶段创建正式文档，并使用接入后板卡数据。 |

## 优化矩阵

| candidate family | row source policy | point type / Scalar / layout | scope and entry | correctness / fallback target | bench / ablation target | board evidence | asm boundary | Evidence Doctor | decision |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| production RVV count | indexed source + indexed normal | `PointXYZ/XYZI/RGB/RGBA + pcl::Normal`, float AoS | `countWithinDistance` public entry and detail helper | production direct tests, small/fallback tests | production public count label | PI4 repeated board | `vfsqrt.v`, `vcpop.m` in production helper | production manifest | pending |
| production RVV select | indexed source + indexed normal | same | `selectWithinDistance` public entry and detail helper | order and `error_sqr_dists_` tests | production public select label | PI4 repeated board | `vcompress.vm`, `vfwcvt`, `vse64.v` | production manifest | pending |
| production RVV dense distances | indexed source + indexed normal | same | `getDistancesToModel` public entry and detail helper | dense output size/order tests | production public getDistances label | PI4 repeated board | `vfwcvt`, `vse64.v` | production manifest | pending |

## TDD 和实现动作

1. RED：新增 RVV 构建下 production detail helper direct tests，当前应因 helper 不存在或返回不成立而失败。
2. GREEN：在 production 头中新增 normal-sphere 标量 helper 和 RVV helper，并让三条 public entry 短路尝试 RVV。
3. PI3：补 fallback 测试，覆盖小规模 / 非 `pcl::Normal` normal 点型 / 非 RVV build 不误命中。
4. PI4：运行 `run_test_compare`、`dump_bench_rvv`、板卡 production bench compare、production manifest 和 Evidence Doctor。
5. PI5：若接入后板卡 evidence（板卡证据）显示收益且无 Error，按用户本轮授权视为可采纳，并创建正式 `doc-rvv/sample_consensus/sac_model_normal_sphere-RVV.zh.md`；若无收益或证据异常，保留 patch 状态并整理停止原因。

## Fallback 和停止条件

RVV helper 必须在以下情况返回 false 并回退标量：非 `__RVV10__` 构建、source layout gate 失败、normal 点型不是 `pcl::Normal`、`pcl::index_t` 不是 signed 32-bit、source / normal cloud 超过 32-bit byte offset 上限、normal cloud 小于 input cloud、小规模输入低于收益阈值。

停止条件：需要扩大 public API、修改公共 traits API、覆盖 Phase 050 未冻结点型、板卡不可达、production direct correctness 失败、asm 无法归属、Evidence Doctor 出现 Error，或接入后板卡性能没有收益。

## 文档更新清单

完成后同步本 phase result、phase index、optimization matrix、roadmap、evaluation、topic-local role 文档、保留候选复筛清单。若接入后板卡证据支持采纳，创建并填充正式 `doc-rvv` 主题文档，文档中的性能数据只采用 production direct 板卡数据，不复用 diagnostic 数值作为最终收益。
