# ISS 3D Test Support Code Map

## 源码布局

| 文件 | 职责 | 证据角色 |
| --- | --- | --- |
| `include/iss_3d.h` | 稳定聚合入口，测试和 bench 只 include 这个头。 | test support navigation |
| `include/impl/iss_3d_scatter.hpp` | 标量 reference、RVV candidate、fallback wrapper。 | diagnostic correctness / benchmark |
| `src/test_iss_3d.cpp` | gtest correctness，包括 protected production scatter probe。 | QEMU correctness |
| `src/bench_iss_3d.cpp` | diagnostic scatter 和 public compute benchmark CLI。 | board performance |
| `script/generate_iss_3d_evidence_manifest.py` | 从 repeated board run logs 生成 summary / manifest。 | Evidence Doctor input |
| `Makefile` | 本地、QEMU、asm、board repeated、doctor 和 registry target。 | reproducibility |
| `board.mk` | 板卡端运行参数和共享 board fragment。 | board execution |

## Production 对照

| production 符号 | 作用 | 测试侧对照 |
| --- | --- | --- |
| `ISSKeypoint3D::getScatterMatrix` | 搜索邻域并生成 scatter matrix。 | `ISSScatterProbe::scatterAt` 调用 protected method。 |
| `pcl::detail::computeISSScatterMatrixRVV` | RVV indexed gather + f64 reduction probe。 | `computeScatterMatrixRVV` 的 test-only 同构实现。 |
| 原标量 loop | fallback 和 non-RVV build 的语义基线。 | `computeScatterMatrixStd` 和 independent radius reference。 |

## Layout And Naming Audit

当前 topic 已采用 `src/`、`include/`、`include/impl/` 和 topic-local `script/` 布局，符合 `.agents/config/defaults.yaml` 中的 aggregator / internal helper 约定。没有旧 `test_support/` 目录和 legacy compatibility alias（兼容旧入口别名）需要清理。

`iss_3d` 是短 topic token（主题短标识），不需要额外缩写。Makefile target 粒度覆盖 correctness、asm、implementation-shape、board repeated、doctor / registry 和 freshness check。

## 可读性边界

测试 helper 当前集中在一个内部头中，职责包括 reference、candidate 和 fallback wrapper，行数未超过拆分阈值。若后续继续加入 point type expansion、更多 public case 或 profile parser，应优先拆出 fixtures、bench cases 和 assertions，避免 `iss_3d_scatter.hpp` 变成多职责大文件。
