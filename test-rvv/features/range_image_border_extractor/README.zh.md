# range_image_border_extractor RVV topic

## 当前结论

当前 topic 暂停，结论为 `attempted-neutral / no-production`。Phase 000 单图 score-update 和 Phase 010 四方向 score-update 在板卡上分别有 `2.560x`、`2.210x` 的局部收益，但 Phase 020/030 引入真实 `RangeImage` score generation 后，production-shaped diagnostic（生产形态诊断）median 均为 `1.000x`。因此不建议把当前 score-update / neighbor-score 方向接入 production。

## 阅读路径

| 目的 | 文档 |
| --- | --- |
| 函数级评估和诊断证据链 | `doc/range_image_border_extractor-evaluation.zh.md` |
| 测试入口和 target 粒度 | `doc/testing-overview.zh.md` |
| gtest 覆盖 | `doc/correctness-tests.zh.md` |
| bench / board / Evidence Doctor | `doc/benchmark-and-evidence.zh.md` |
| 候选取舍 | `doc/optimization-evidence.zh.md` |
| 代码地图 | `doc/test-support-code-map.zh.md` |
| phase 恢复入口 | `doc/phases/README.zh.md` |

## 常用命令

| 命令 | 证据角色 |
| --- | --- |
| `make run_test_compare` | QEMU correctness（正确性） |
| `make dump_test_rvv dump_bench_rvv` | 反汇编归属 |
| `make run_board_test fetch_board_logs` | 板卡 correctness smoke |
| `make board_repeated` | 默认 Phase 010 four-score-image 5 次板卡 repeated benchmark；其它 phase 用变量覆盖复现 |
| `make evidence_doctor_repeated` | summary / manifest / Evidence Doctor |

`log/` 和 `build/` 下产物默认 local-only，不随 topic 提交。当前没有 adopted production behavior，不创建 `doc-rvv/features/range_image_border_extractor-RVV.zh.md`。

复现 Phase 000 单图 score-update repeated 时使用：

```bash
make board_repeated \
  REPEATED_BOARD_TAG=phase000_score_update \
  REPEATED_BOARD_REMOTE_TAG=phase000_score_update \
  REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase000_score_update_repeated \
  REPEATED_BOARD_CASE_NAME=score_update_641x481_tail \
  RIBE_REPEATED_BENCH_ARGS="--case-filter score_update_641x481_tail --repeat 32 --iterations 10 --warmup 2"
```

复现 Phase 030 score-generation component split repeated 时使用：

```bash
make board_repeated \
  REPEATED_BOARD_TAG=phase030_score_generation_component_split \
  REPEATED_BOARD_REMOTE_TAG=phase030_score_generation_component_split \
  REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase030_score_generation_component_split_repeated \
  REPEATED_BOARD_CASE_NAME= \
  RIBE_REPEATED_BENCH_ARGS="--case-filter range_image_local_surface_160x120,range_image_border_scores_after_surface_160x120 --repeat 1 --iterations 10 --warmup 2"
make evidence_doctor_repeated \
  REPEATED_BOARD_TAG=phase030_score_generation_component_split \
  REPEATED_BOARD_RUN_LABEL=range_image_border_extractor_phase030_score_generation_component_split_repeated \
  REPEATED_BOARD_CASE_NAME= \
  REPEATED_BOARD_TITLE="RangeImageBorderExtractor score-generation component split production-shaped diagnostic repeated board summary"
```
