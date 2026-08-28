# sac_model_plane 测试支撑代码地图

## 本文职责

本文把 production（生产源码）、test / bench、script（脚本）、output（输出）和文档位置对应起来。
读者可从这里定位 Phase 000/010 的证据链。

## 代码与证据地图

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 证据角色 |
| --- | --- | --- | --- | --- |
| `SampleConsensusModelPlane::selectWithinDistance` | production public entry | 公开 inlier 输出入口，命中 RVV 或 Standard fallback。 | PCL SAC callers | production boundary |
| `SampleConsensusModelPlane::countWithinDistance` | production public entry | 公开计数入口，命中 RVV 或 Standard fallback。 | PCL SAC callers | production boundary |
| `SampleConsensusModelPlane::getDistancesToModel` | production public entry | 公开 dense distance 输出入口，命中 gather-only RVV 或 Standard fallback。 | PCL SAC callers | production boundary |
| `getDistancesToModelStandard` / `selectWithinDistanceStandard` / `countWithinDistanceStandard` | production Std helper | 保留原标量语义和 fallback。 | public entry / fallback | fallback correctness |
| `selectWithinDistanceRVV` / `countWithinDistanceRVV` | production RVV helper | indexed gather；identity chunk 下可切到 strided load。 | public entry | adopted Phase 000/010 RVV implementation |
| `getDistancesToModelRVV` | production RVV helper | indexed gather + float distance + double store。 | public entry | adopted Phase 000 gather implementation |
| `sacModelPlaneRVVLoadXYZ` | production detail helper | 在 select/count 中选择 strided load 或 gather；在 getDistances 中强制 gather。 | RVV helpers | load strategy boundary |
| `src/test_sac_model_plane.cpp` | topic-local test | 公开入口和 protected helper 对拍，含 identity case。 | `run_test_compare` | correctness gate |
| `src/bench_sac_model_plane.cpp` | topic-local bench | 三入口 public Std/RVV 计时，支持 `identity` / `shuffled` mode。 | `board_smoke` / repeated board loop | production-public performance |
| `script/generate_board_evidence_manifest.py` | topic-local script | repeated logs 转 Evidence Doctor manifest；支持 `run-XX/board/` 目录。 | manifest Make targets | evidence metadata |
| `doc/phases/000-base-plane-select-distance-production/evidence-doctor.md` | evidence summary | Phase 000 可提交 doctor 摘要。 | documentation / reviewer | evidence review |
| `doc/phases/010-identity-index-strided-load/evidence-doctor.md` | evidence summary | Phase 010 可提交 doctor 摘要。 | documentation / reviewer | evidence review |

## 输出目录

| 路径 | 用途 | 提交策略 |
| --- | --- | --- |
| `log/board/repeated/` | Phase 000 5-run repeated board raw logs 和 summary。 | raw logs 默认本机保留；manifest 可作为摘要证据。 |
| `log/board/phase-010/identity-stride-select-count/repeated/` | Phase 010 identity 5-run repeated board。 | raw logs 默认本机保留；summary/manifest 被文档引用。 |
| `log/board/phase-010/shuffled-stride-select-count/repeated/` | Phase 010 shuffled 5-run repeated board。 | raw logs 默认本机保留；summary/manifest 被文档引用。 |
| `build/asm/riscv/bench_sac_model_plane_rvv.full.asm` | RVV bench 反汇编。 | build 输出默认不提交。 |

## 结构审计

当前 topic 已有 `src/`、topic-local `script/`、README、phase suite、evaluation、testing overview、
correctness tests、benchmark/evidence、optimization evidence 和 test-support code map。若后续进入
点型专用性能或 fixture（测试夹具）拆分 phase，应先判断是否需要把点型 fixture 和 bench case
（性能用例）从当前单文件中拆出到 `include/impl/`。
