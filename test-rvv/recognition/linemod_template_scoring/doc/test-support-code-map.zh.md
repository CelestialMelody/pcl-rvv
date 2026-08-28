# LINEMOD Template Scoring Test Support Code Map

## 源码和测试支撑

| 文件 / 符号 | 层级 | 作用 | 证据角色 |
| --- | --- | --- | --- |
| `recognition/src/linemod.cpp::linearizeEnergyMapStd` | production Std helper | 保留原 `EnergyMaps -> LinearizedMaps` 标量事实来源 | fallback source of truth |
| `recognition/src/linemod.cpp::linearizeEnergyMapRVV` | production RVV helper | 用 `vlse8.v` 跨步加载 energy map，并连续写入 linearized map | adopted production RVV path |
| `recognition/src/linemod.cpp::linearizeEnergyMap` | production dispatch / fallback | `__RVV10__` 下调用 RVV helper，否则调用 Std helper | production boundary |
| `include/linemod_template_scoring.h` | aggregator（聚合入口） | 测试支撑聚合头，只包含 test-only candidate helper | test support navigation |
| `include/impl/linemod_template_scoring_candidates.hpp` | internal helper | 保存 score accumulation、scan、energy map、linearized copy 的标量 / RVV test helper | diagnostic correctness / bench |
| `include/impl/linemod_template_scoring_production_direct.hpp` | internal helper | 构造 production-direct `FixedQuantizableModality` 和 `LINEMOD` fixture | production direct correctness / bench |
| `src/test_linemod_template_scoring.cpp` | correctness source | candidate helper 对拍 | diagnostic correctness |
| `src/test_linemod_template_scoring_production_direct.cpp` | correctness source | 真实公开入口 `matchTemplates` / `detectTemplates` / `detectTemplatesSemiScaleInvariant` smoke | production direct correctness |
| `src/bench_linemod_template_scoring.cpp` | bench source | Phase 000-050 diagnostic / full-chain split bench | diagnostic board evidence |
| `src/bench_linemod_template_scoring_production_direct.cpp` | bench source | Phase 070 / 080 当前源码公开入口计时 | production-public board evidence |
| `script/generate_linemod_template_scoring_evidence_manifest.py` | analysis script | 把 repeated board logs 转成 summary / manifest | Evidence Doctor input |
| `log/evidence_registry.json` | evidence registry | 登记 summary、manifest、doctor 和 doc refs | freshness check |

## 布局审计

当前 topic 已采用 `src/`、`include/` 和 `include/impl/` 布局；没有旧 `test_support/` 目录或 legacy alias 需要迁移。测试支撑按职责拆成 candidate helper 和 production-direct fixture，bench 也按 diagnostic 与 production-public 分文件，因此当前没有未阻塞的 internal-helper-layout 或 test-source-split 动作。
