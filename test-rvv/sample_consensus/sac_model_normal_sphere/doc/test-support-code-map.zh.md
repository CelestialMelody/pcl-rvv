# sac_model_normal_sphere 测试支撑代码地图

## 本文职责

本文帮助 reviewer 从文档跳到测试支撑代码、bench、script 和 evidence output。它不承担性能结论，也不把 test-only helper（测试专用 helper）写成 production helper。

## 总调用图

```text
production public entry
  -> sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp
    -> *RVVNormalSphere helper when gate matches
    -> *StandardNormalSphere helper as fallback
gtest / bench main
  -> include/test_sac_model_normal_sphere.h 或 include/bench_sac_model_normal_sphere.h
    -> SampleConsensusModelNormalSphereAccess
      -> production public entry（当前已有 RVV dispatch）
      -> scalar reference（测试专用同构参考）
      -> RVV candidate（测试专用历史候选，__RVV10__ 下启用）
      -> production detail helper direct checks
  -> board logs
    -> script/generate_normal_sphere_evidence_manifest.py
      -> doc/phases/*/board-evidence-summary.md
      -> doc/phases/*/board-evidence-manifest.json
      -> Evidence Doctor / evidence registry
```

## 稳定入口

| 对象 | 角色 | 位置 | 备注 |
| --- | --- | --- | --- |
| `Makefile` | build / QEMU / evidence target | `test-rvv/sample_consensus/sac_model_normal_sphere/Makefile` | 使用共享 `rvv-topic.mk`。 |
| `board.mk` | board-side 参数 | `test-rvv/sample_consensus/sac_model_normal_sphere/board.mk` | 远端值来自环境或本机配置，raw log 不提交。 |
| `src/test_sac_model_normal_sphere.cpp` | gtest source | `src/` | 只保留 TEST case。 |
| `src/bench_sac_model_normal_sphere.cpp` | bench CLI source | `src/` | 解析 `<points> <iterations> <PointT>`。 |
| `include/test_sac_model_normal_sphere.h` | correctness aggregator（正确性聚合头） | `include/` | fixture 和断言 helper。 |
| `include/bench_sac_model_normal_sphere.h` | bench aggregator（性能测试聚合头） | `include/` | bench fixture、计时和输出合同。 |
| `include/impl/sac_model_normal_sphere_access.hpp` | internal helper（内部测试支撑） | `include/impl/` | 标量参考、历史 RVV candidate 和 production helper direct checks。 |
| `impl/sac_model_normal_sphere.hpp` | production helper | `sample_consensus/include/pcl/sample_consensus/impl/` | 当前 adopted Standard / RVV helper 和 public dispatch。 |

## Fixtures 与输入构造

`makeNormalSphereCloud` 构造小规模边界样本，覆盖球壳内外、normal angle 和球心退化点。`makeBenchCloud` 构造大规模 synthetic cloud，并用相邻交换的 `indices_` 模拟 indexed AoS gather（按索引离散读取结构数组）。`PointXYZI`、`PointXYZRGB` 和 `PointXYZRGBA` 的附加字段只用于 layout 验证，不参与公式。

## 标量 Reference

`countWithinDistanceScalarReference`、`selectWithinDistanceScalarReference` 和 `getDistancesToModelScalarReference` 复刻 production 标量语义。它们是 correctness reference（正确性参考链路），不是新的 production API。

## Candidate / Diagnostic Helper

| helper | 职责 | fallback | 当前状态 |
| --- | --- | --- | --- |
| `countWithinDistanceCandidate` | `__RVV10__` 下尝试 mask count；否则走标量参考。 | 非 RVV 或 layout gate 失败。 | historical positive diagnostic。 |
| `selectWithinDistanceCandidate` | RVV 计算 distance，标量 lane 写回 inliers/errors。 | 非 RVV 或 layout gate 失败。 | historical positive diagnostic，被 production `vcompress` 路径替代。 |
| `selectWithinDistanceVCompressCandidate` | 用 `vcompress` 保序压缩 index 和 distance。 | 非 RVV 或 layout gate 失败。 | historical PI1 input；production select 已采纳同类写回。 |
| `getDistancesToModelCandidate` | RVV 计算 distance 并 dense double store。 | 非 RVV 或 layout gate 失败。 | historical PI1 input；production getDistances 已采纳 dense store。 |
| `distancesRVV` | VL chunk 内计算球壳距离和 normal angle。 | 仅由测试专用 candidate 调用。 | 历史测试专用 RVV 核心。 |
| `loadIndexedPointAndNormal` | 按 `indices_` gather source xyz 与 normal xyz。 | 仅由测试专用 candidate 调用。 | 历史点型扩展 gate。 |

## Production Helper

| helper | 职责 | fallback | 当前状态 |
| --- | --- | --- | --- |
| `countWithinDistanceStandardNormalSphere` | 保存原标量 count 语义。 | public entry 在 RVV gate 失败或非 RVV 构建下调用。 | adopted fallback。 |
| `selectWithinDistanceStandardNormalSphere` | 保存原标量 select 和 `error_sqr_dists_` 语义。 | 同上。 | adopted fallback。 |
| `getDistancesToModelStandardNormalSphere` | 保存原标量 dense double distance 语义。 | 同上。 | adopted fallback。 |
| `computeNormalSphereDistanceRVV` | RVV chunk 内计算球壳距离、normal 归一化、锐角和最终 distance。 | 仅 `__RVV10__` 下编译。 | adopted production helper。 |
| `canUseRVVNormalSphere` | 检查 layout、normal 点型、index 类型、规模和小输入。 | 返回 false 时 public entry 调用 Standard helper。 | adopted dispatch / fallback gate。 |
| `countWithinDistanceRVVNormalSphere` | RVV distance + `vcpop.m` 统计内点。 | gate 失败返回 false。 | adopted production helper。 |
| `selectWithinDistanceRVVNormalSphere` | RVV distance + `vcompress.vm` 保序压缩 index 和 distance。 | gate 失败返回 false。 | adopted production helper。 |
| `getDistancesToModelRVVNormalSphere` | RVV distance + `vfwcvt.f.f.v` + `vse64.v` 连续写回 double。 | gate 失败返回 false。 | adopted production helper。 |

## Scripts 与 Evidence Output

| 文件 | 作用 |
| --- | --- |
| `script/generate_normal_sphere_evidence_manifest.py` | 解析 board logs，生成 phase-local summary 和 JSON manifest。 |
| `test-rvv/script/evidence_doctor.py` | 检查 manifest 的证据角色、run count、checksum、asm 和异常信号。 |
| `test-rvv/script/evidence_registry.py` | 记录 summary / manifest / doctor 的 freshness。 |

## Production 与 Test Support 边界

`sample_consensus/include/pcl/sample_consensus/impl/sac_model_normal_sphere.hpp` 是当前 production 事实来源。
Phase060 后，生产 RVV helper 已接入三条 public entry；`test-rvv` 下的 candidate helper 只保留为历史诊断和 correctness
回归，不再作为当前生产性能结论来源。

## 拆分审计

当前 topic 已使用配置解析出的 `src/`、`include/` 和 `include/impl/` 布局，没有旧 `test_support/` 目录或 legacy alias。
`include/impl/sac_model_normal_sphere_access.hpp` 仍承担 reference、layout gate、历史 candidate 和 production helper direct check
多职责，但行数低于 hard limit，且当前代码地图已经把 test-only 与 production helper 边界分开。Phase060 closeout
没有未阻塞的测试支撑拆分动作。
