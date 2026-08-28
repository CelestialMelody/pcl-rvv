# Phase 060: PointXYZRGB / PointXYZRGBA 点型扩展结果

## 当前结论

本阶段未修改 production 源码，只扩展测试 / bench / manifest / evidence target。`PointXYZRGB` 和 `PointXYZRGBA` 都能命中当前 `RVVXYZFloatLayout<PointT>` production gate，并在接入后的板卡 repeated evidence（重复板卡证据）中显示 `selectWithinDistance` public entry median 正向。

EvidenceDecision：

- `PointXYZRGB`：adopted for tested point type，5/5 public select run 正向，median `1.6225x`。
- `PointXYZRGBA`：adopted with stability warning，public select median `1.5528x`，但 1/5 run 为 `0.9184x`，不能写成 clean all-run stable。
- `getDistancesToModel`：仍保持标量；RGB/RGBA 的测试专用 getDistances candidate 继续 5/5 退化，不恢复 production probe。

## 实际改动

| 文件 | 改动 |
| --- | --- |
| `include/test_sac_model_sphere.h` | RGB/RGBA fixture 填充颜色字段，保持 x/y/z 输入分布一致。 |
| `src/test_sac_model_sphere.cpp` | 新增 `PointXYZRGBAndRGBALayoutsMatchReference` correctness case。 |
| `include/bench_sac_model_sphere.h` | bench 输入支持 RGB/RGBA 颜色字段。 |
| `src/bench_sac_model_sphere.cpp` | CLI 第三个参数新增 `PointXYZRGB` / `PointXYZRGBA`。 |
| `script/generate_sphere_board_evidence_manifest.py` | Dataset 点型解析新增 RGB/RGBA，并绑定 manifest 的 `point_type` / `gate` / asm count。 |
| `Makefile` | 新增 RGB/RGBA 独立 board repeated、manifest、Doctor、registry 和 freshness status target。 |

## 验证

| 命令 | 结果 | 说明 |
| --- | --- | --- |
| `make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'` | RED: failed before implementation | 运行返回 unsupported point type，证明当前缺口存在。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere run_test_compare` | passed | Std/RVV 两个构建各 6 个 gtest 通过。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGB'` | passed | QEMU 日志形状包含 `PointXYZRGB` Dataset 行。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere run_bench_rvv BENCH_ARGS='128 1 PointXYZRGBA'` | passed | QEMU 日志形状包含 `PointXYZRGBA` Dataset 行。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere clean_bench_rvv dump_bench_rvv` | passed | RVV bench 可重建并生成反汇编。 |
| `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere check_board_ssh` | passed | 板卡可达。 |
| `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_rgb_point_type_repeated_board_evidence` | passed | RGB 5-run board repeated 完成；每轮板卡 gtest 6/6 passed。 |
| `SSH_AUTH_SOCK=<ssh-agent-socket> make -C test-rvv/sample_consensus/sac_model_sphere collect_rgba_point_type_repeated_board_evidence` | passed | RGBA 5-run board repeated 完成；每轮板卡 gtest 6/6 passed。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere record_rgb_point_type_board_evidence_state` | passed | 生成 RGB manifest / Doctor / JSON 并登记 registry。 |
| `make -C test-rvv/sample_consensus/sac_model_sphere record_rgba_point_type_board_evidence_state` | passed | 生成 RGBA manifest / Doctor / JSON 并登记 registry。 |

板卡运行仍出现远端 clock skew warning（时钟偏移警告），但测试、bench 和日志抓取均完成；该环境警告不改变本阶段数值归属。

## Board Evidence

| point type | public `selectWithinDistance` B/A values | median | min / max | asm | Doctor |
| --- | --- | ---: | ---: | --- | --- |
| `PointXYZRGB` | `1.6331, 1.6181, 1.6308, 1.6203, 1.6225` | `1.6225x` | `1.6181x / 1.6331x` | `selectWithinDistanceRVV` count `27` | `Errors=1, Warnings=1, Suggestions=1`; select row clean，Error/Warning 属于 getDistances rows。 |
| `PointXYZRGBA` | `1.6122, 0.9184, 1.3738, 1.5528, 1.6354` | `1.5528x` | `0.9184x / 1.6354x` | `selectWithinDistanceRVV` count `27` | `Errors=1, Warnings=3, Suggestions=1`; select row has degradation and long-tail Warnings. |

Regression rows:

| point type | public `countWithinDistance` median | public `getDistancesToModel` median | diagnostic getDistances candidate median |
| --- | ---: | ---: | ---: |
| `PointXYZRGB` | `2.0566x` | `1.0104x` | `0.7291x` |
| `PointXYZRGBA` | `2.0094x` | `1.0035x` | `0.7222x` |

Doctor Errors 均属于测试专用 `diagnostic candidate getDistancesToModel`，不属于 `selectWithinDistance` production row。RGB 的 public select row clean；RGBA 的 public select Warning 表示该点型不能写成全 run 稳定正向。

## 文档边界

Phase 060 会更新长期 `doc-rvv` 中的点型覆盖表，但措辞按 evidence 分层：

- `PointXYZRGB` 写成当前测试边界下 clean positive。
- `PointXYZRGBA` 写成 median positive with 1/5 degradation and long-tail warning。
- RGB/RGBA 均不外推到自定义 registered xyz 点型、其它规模、其它 row source 或 `getDistancesToModel`。

## Continue / Stop Decision

未命中板卡访问或生产收益停止条件。Phase 060 关闭 RGB/RGBA 点型 evidence 缺口；当前未阻塞的生产收益扩展主要剩余自定义 registered xyz 点型和 `getDistancesToModel` 新实现族。自定义点型需要先定义代表性类型和测试边界，`getDistancesToModel` 只有出现 RVV sqrt/helper 或 dense-store 消融新证据时恢复；因此本阶段后默认暂停在整理 / review-ready 状态，不继续凭空扩大到未知自定义点型。
