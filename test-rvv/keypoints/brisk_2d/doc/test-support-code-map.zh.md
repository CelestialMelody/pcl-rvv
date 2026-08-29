# BRISK 2D 测试支撑代码地图

| 符号 / 文件 | 层级 | 作用 | 调用者 / 消费者 | 证据角色 |
| --- | --- | --- | --- | --- |
| `keypoints/src/brisk_2d.cpp` | production source | 新增 portable scalar fallback 和 RVV downsample helper 路径 | `brisk::Layer`、`ScaleSpace::constructPyramid` | production boundary（生产边界） |
| `include/brisk_2d.h` | test aggregator（测试聚合入口） | 汇总 reference 和 fixture helper | `src/test_brisk_2d.cpp`、`src/bench_brisk_2d.cpp` | reviewer navigation |
| `include/impl/brisk_2d_downsample_reference.hpp` | diagnostic reference（诊断参考链路） / fixture | 合成图像、synthetic organized cloud、标量 downsample reference、checksum | gtest / bench | correctness oracle（正确性基准） |
| `src/test_brisk_2d.cpp` | correctness test | 逐字节对拍 helper、`ScaleSpace` 触达 smoke 和 public compute smoke | `run_test_compare`、board `run_test` | correctness gate |
| `src/bench_brisk_2d.cpp` | bench wrapper | 输出 helper、construct-pyramid 和 public compute case 的 ms / checksum / iterations / warmup | board smoke、repeated board | production detail / public performance |
| `script/generate_brisk_2d_evidence_manifest.py` | topic-local analyzer | 解析 repeated run raw logs，生成 summary 和 manifest | `generate_board_evidence_manifest` / `generate_public_board_evidence_manifest` | Evidence Doctor input |
| `doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-summary.md` | summary artifact | 保存 5-run median/min/max 和 B/A 频率 | evaluation / doc-rvv / Handoff | board performance summary |
| `doc/phases/000-current-state-and-downsample-diagnostic/repeated-evidence-doctor.md` | Evidence Doctor | 保存 Error/Warning/Suggestion | evaluation / doc-rvv / Handoff | evidence validation |
| `doc/phases/010-public-compute-end-to-end/repeated-evidence-summary.md` | summary artifact | 保存 public compute 5-run median/min/max 和 B/A 频率 | evaluation / doc-rvv / Handoff | public-entry impact summary |
| `doc/phases/010-public-compute-end-to-end/repeated-evidence-doctor.md` | Evidence Doctor | 保存 public compute near-threshold suggestion | evaluation / doc-rvv / Handoff | evidence validation |
| `log/evidence_registry.json` | evidence registry | 记录 summary / manifest / doctor hash 和 freshness | `repeated_evidence_status` | freshness check |

测试支撑布局已经使用 `src/`、`include/`、`include/impl/` 和 `script/`。当前没有 legacy
`test_support/` 目录，也没有需要保留的 compatibility alias（兼容别名）。
