# plane_models normal-plane 测试支撑代码地图

本文是 test support code map（测试支撑代码地图）role。它只提供代码定位、调用关系和证据角色，不承担性能结论。

## 总调用图

```text
SAC public tests / bench wrappers
  -> SampleConsensusModelNormalPlane<PointXYZ, Normal>
     -> public select/count/getDistances
        -> RVV helper or Standard helper
  -> phase docs / manifest / Evidence Doctor / registry
```

公开入口测试通过 test-only proxy（测试专用代理）直接访问 protected helper；bench wrapper 也使用同类 proxy 隔离 helper hot path。该设计用于证据分层：公开入口是否命中 RVV 由 GTest 证明，helper 性能由 board bench 证明。

## 稳定入口

| 入口 | 路径 | 角色 |
| --- | --- | --- |
| test source | `src/test_sample_consensus_plane_models.cpp` | GTest correctness、fallback 和历史 plane_models 回归。 |
| bench source | `src/bench_sac_normal_plane.cpp` | protected helper Std/RVV board compare。 |
| load probe source | `src/bench_sac_normal_plane_load_compare.cpp` | 历史 RVV load strategy 探针。 |
| Makefile | `Makefile` | 本地/QEMU/board target 聚合入口。 |
| board fragment | `board.mk` | 板卡侧二进制名、fixture 路径和 load probe target。 |
| phase docs | `doc/phases/` | phase plan/result、matrix、manifest 和 doctor。 |
| topic-local script | `script/generate_normal_plane_board_evidence_manifest.py` | normal-plane board logs 到 Evidence Doctor manifest 的主题本地转换入口。 |

## Fixtures 与输入构造

| 对象 | 来源 | 用途 |
| --- | --- | --- |
| `pcd/sac_plane_test.pcd` | topic-local fixture | RANSAC 回归、board test、helper bench。 |
| 随机 `PointXYZ + Normal` cloud | `src/test_sample_consensus_plane_models.cpp` | SIMD helper 对拍和容差验证。 |
| `PointXYZI` / `PointXYZINormal` source cloud | `src/test_sample_consensus_plane_models.cpp`、`src/bench_sac_normal_plane.cpp` | Phase 040 代表性 AoS source public-vs-direct RVV 测试；Phase 050 代表性 source helper performance bench。 |
| synthetic ordered indices | public/fallback tests | 确认公开入口保持输入顺序和输出写回语义。 |
| `NormalWithDoubleCurvature` | 测试源码中注册点类型 | 验证 curvature 非 float layout 走 fallback。 |
| `NonAoSRegisteredXYZ` | 测试源码中注册点类型 | 验证注册 xyz 但非 standard-layout 的 source 点型走 Standard fallback。 |

## Reference、Candidate 和 Bench Harness

| 符号 / 文件 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `SampleConsensusModelNormalPlaneTest` | test-only proxy | 暴露 protected helper，避免改 public API。 | helper correctness。 |
| `SampleConsensusModelNormalPlaneBench` | bench wrapper | 直接调用 Standard/RVV helper，隔离三条 hot path。 | production-shaped diagnostic performance。 |
| `makeSourceCloud<PointT>` | bench fixture helper | 从同一 `PointXYZ` PCD 数据构造 `PointXYZI` / `PointXYZINormal` source 点云，保证 Phase 050 只改变 source layout / stride，不改变几何输入。 | representative source performance fixture。 |
| `selectWithinDistanceStandard` | production Std helper | 标量 fallback 和 helper direct path。 | correctness baseline。 |
| `selectWithinDistanceRVV` | production RVV helper | mask + `vcompress` 写回 inliers 和 distances。 | RVV candidate / asm attribution。 |
| `countWithinDistanceRVV` | production RVV helper | mask popcount 计数。 | RVV candidate / asm attribution。 |
| `getDistancesToModelRVV` | production RVV helper | dense distance write。 | RVV candidate / asm attribution。 |
| `kNormalPlaneRVVLayoutCompatible` | production dispatch gate | 把 source AoS layout、normal/curvature field layout 和 byte-offset 上界组合成公开入口准入条件。 | production boundary / fallback coverage。 |
| Evidence manifest | phase output | 把 board/qemu/asm 元数据转成 doctor 输入。 | summary evidence。 |
| `generate_normal_plane_board_evidence_manifest.py` | topic-local script | 解析当前 topic 的 board Std/RVV bench logs、case label 和 asm 符号。 | manifest wrapper。 |

## Scripts 与 Evidence Output

当前 topic 有一个 topic-local manifest wrapper，并复用共享 Evidence Doctor / registry 脚本：

```text
test-rvv/sample_consensus/plane_models/script/generate_normal_plane_board_evidence_manifest.py
test-rvv/script/evidence_doctor.py
test-rvv/script/evidence_registry.py
```

当前 evidence output（证据输出）分层：

| 路径 | 状态 | 提交边界 |
| --- | --- | --- |
| `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-manifest.json` | summary artifact | 可提交候选。 |
| `doc/phases/000-normal-plane-current-state-and-public-entry-boundary/evidence-doctor.md` | summary artifact | 可提交候选。 |
| `log/board/normal-plane-phase030-repeated-board/summary.md` | summary artifact | 可提交候选；Phase 030 `PointXYZ + Normal` repeated helper performance。 |
| `log/board/normal-plane-phase050-representative-aos-source-performance/summary.md` | summary artifact | 可提交候选；Phase 050 代表性 source repeated helper performance。 |
| `log/evidence_registry.json` | freshness registry | 默认本机生成；如需提交需 `git add -f` 精确选择并保留文档引用。 |
| `log/board/*.log`、`log/qemu/*.log` | raw log | 默认不提交。 |
| `build/asm/riscv/*.asm` | generated asm | 默认不提交；phase docs 只引用路径和统计。 |

## Production 与 Test Support 边界

production 源码只在 `sac_model_normal_plane.hpp` 中保留 Std/RVV helper 和 public dispatch。测试专用 proxy、bench wrapper、load strategy probe、manifest 和 doctor report 都属于 `test-rvv/sample_consensus/plane_models`，不能写成 PCL public API。

## 拆分审计

| area | current shape | decision | reason / next action |
| --- | --- | --- | --- |
| `src/` source layout | 三份 topic-owned `.cpp` 已迁入 `src/`，Makefile 已更新。 | adopted | phase 010 已验证本地/QEMU/board target。 |
| `include/` aggregator | 当前无共享 test helper header；proxy 和 fixtures 仍在单一 test source 内。 | not_applicable with evidence | 没有被多文件复用的 helper。若后续新增 point-type expansion，可创建聚合头。 |
| `include/impl/` internal helpers | 当前无旧 `test_support/` 或大 header 需要迁移。 | not_applicable with evidence | 本阶段只关闭 source layout；内部职责拆分不应制造空目录。 |
| topic-local script | 已新增 normal-plane manifest wrapper，Makefile 已接入 doctor / registry alias，并支持 Phase 050 `--case-set representative-aos`。 | adopted | phase 020 已验证基础 alias；phase 050 已验证 `record_phase050_evidence_state` 和 `phase050_evidence_status`。 |
| legacy root `.cpp` paths | 旧 root source 已删除，文档引用已刷新到 `src/`。 | adopted | 不保留 compatibility alias（兼容别名）。 |
| Phase 040 test additions | 仍在 `src/test_sample_consensus_plane_models.cpp` 内，因为新增代码只服务 public/fallback 测试族。 | adopted | 新增 helper 未被 bench 或多文件复用，暂不创建空 `include/impl`。 |
| Phase 050 bench additions | `src/bench_sac_normal_plane.cpp` 为 265 行，新增 source 点云构造和 bench wrapper 仍只服务该 bench 文件；`src/test_sample_consensus_plane_models.cpp` 虽为历史大文件，但本阶段没有新增跨文件共享 helper。 | adopted / no split | 当前没有旧 `test_support/` 目录、单个大 header 或多文件复用职责。若后续 normal layout expansion 同时需要测试和 bench 复用 fixture，再创建 `include/impl`。 |
