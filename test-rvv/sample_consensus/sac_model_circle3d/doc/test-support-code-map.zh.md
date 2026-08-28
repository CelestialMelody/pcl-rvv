# sac_model_circle3d Test Support Code Map

| 符号 / 文件 | 层级 | 作用 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- |
| `sac_model_circle3d.h` | aggregator header（聚合入口） | 让 test / bench 使用稳定 include。 | reviewability（可审查性） | `include/sac_model_circle3d.h` |
| `SampleConsensusModelCircle3DAccess` | test-only diagnostic | 暴露 candidate count/select helper。 | component ablation | `include/impl/sac_model_circle3d_candidates.hpp` |
| `test_sac_model_circle3d.cpp` | correctness source | 对拍 public path 和 candidate。 | correctness gate | `src/test_sac_model_circle3d.cpp` |
| `bench_sac_model_circle3d.cpp` | bench source | 承载 public/candidate timing、warm-up、count 和 checksum 输出。 | board performance input | `src/bench_sac_model_circle3d.cpp` |
| `check_circle3d_projection_asm.py` | analysis script | 检查 candidate 符号内的 gather、FMA、fnmsac、sqrt 和 select compress。 | asm attribution | `script/check_circle3d_projection_asm.py` |
| `generate_circle3d_board_evidence_manifest.py` | analysis script | 把 board repeated raw logs 转成 Evidence Doctor manifest。 | evidence manifest wrapper | `script/generate_circle3d_board_evidence_manifest.py` |
| `projection-repeated-evidence-manifest.json` | evidence output summary | 保存有 warm-up 的 board repeated B/A 和 metadata。 | component ablation summary | `doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-manifest.json` |
| `projection-repeated-evidence-doctor.md` | evidence output summary | 保存 Evidence Doctor Errors / Warnings / Suggestions。 | EvidenceDecision gate | `doc/phases/000-circle3d-projection-component-ablation/projection-repeated-evidence-doctor.md` |
| `select-production-repeated-evidence-manifest.json` | evidence output summary | 保存 production-public `PointXYZ` 10-run B/A 和 metadata。 | production adoption gate | `doc/phases/010-selectwithin-production-probe/select-production-repeated-evidence-manifest.json` |
| `select-*-repeated-evidence-manifest.json` | evidence output summary | 保存 Phase 020 三个点型扩展的 production-public 5-run B/A。 | point-type expansion gate | `doc/phases/020-select-point-type-expansion/` |
