# color_coding 测试总览

本文负责说明 `test-rvv/io/color_coding` 的测试入口、target 粒度和证据边界。每个 gtest（Google Test 单元测试）的语义细节见 `doc/correctness-tests.zh.md`；bench label（性能测试标签）和 Evidence Doctor（证据体检）细节见 `doc/benchmark-and-evidence.zh.md`。

## 阅读路径

| 读者问题 | 主入口 |
| --- | --- |
| 当前 production 是否能改 | `doc/color_coding-evaluation.zh.md` |
| 测试怎么跑 | 本文和 `README.zh.md` |
| 每个 TEST 验证什么 | `doc/correctness-tests.zh.md` |
| 每个 bench case 证明什么 | `doc/benchmark-and-evidence.zh.md` |
| 每个优化候选当前状态 | `doc/optimization-evidence.zh.md` 和 `doc/phases/optimization-matrix.zh.md` |
| 下一阶段从哪里恢复 | `doc/phases/README.zh.md` 和 `tmp/rvv-work-logs/io/color_coding/current-handoff/current-handoff.zh.md` |

## 测试类型定义

| 类型 | 当前入口 | 证据角色 | 不能证明什么 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/io/color_coding run_test_compare` | Std/RVV 两条链路的 helper 语义一致性 | public compression entry（公开压缩入口）、fallback、性能 |
| diagnostic bench（诊断性能测试） | `make -C test-rvv/io/color_coding run_bench_rvv` / `run_bench_std` | component helper 或 production-shaped helper 的计时输入 | QEMU 性能、完整 octree compression |
| QEMU smoke（仿真器小型验证） | `make -C test-rvv/io/color_coding run_qemu_smoke` | 构建、correctness、日志形状 | 性能结论 |
| board smoke（板卡小型验证） | `make -C test-rvv/io/color_coding run_board_color_coding_smoke` | 板卡可运行和单次日志形状 | repeated performance（重复采集性能） |
| board repeated（板卡重复采集） | `make -C test-rvv/io/color_coding run_board_color_coding_repeated` | 5-run summary、manifest、Doctor、registry | production direct evidence |
| production repeated（生产路径重复采集） | `make -C test-rvv/io/color_coding run_board_color_coding_production_repeated` | phase 080 后只输出 skip，保护 phase 070 historical production evidence | 当前没有 production RVV path 可重复采集 |
| doctor / registry（证据体检 / 登记） | repeated target 内生成；`check_evidence_freshness` 检查 | 异常暴露和当前证据 freshness | 自动证明 production 可采纳 |

## 运行入口分类

| target / command | 来源 | 做什么 | 何时使用 |
| --- | --- | --- | --- |
| `run_test_compare` | shared `rvv-topic.mk` | 运行 Std 与 RVV gtest，可看到每边 8 个 TEST；phase 060 增加 production direct 和 fallback gate | 每次改 helper、测试、文档 closeout 前 |
| `run_qemu_smoke` | topic Makefile | 跑 `run_test_compare` 和一次 RVV bench | 检查 QEMU 构建 / 输出形状 |
| `run_bench_rvv BENCH_ARGS="--case-filter <label>"` | bench CLI | 只跑指定 RVV bench label | 快速看日志形状；不写性能结论 |
| `run_board_color_coding_smoke` | topic Makefile | 板卡单次 Std/RVV compare，`--case-filter all` | 确认可运行 |
| `run_board_color_coding_repeated` | topic Makefile | 收集 5 次板卡 compare，生成 summary / manifest / Doctor / registry | 更新性能证据 truth |
| `run_board_color_coding_production_repeated` | topic Makefile | phase 080 后显式 skip，不覆盖 phase 070 historical summary / manifest / Doctor | 仅用于确认当前没有 production RVV case |
| `check_evidence_freshness` | topic Makefile | 检查登记的 summary / manifest / doctor 与文档引用是否 fresh | 文档或 closeout 前 |
| `dump_bench_rvv` | shared `rvv-topic.mk` | 生成 RVV bench 反汇编 | 需要确认 RVV 指令归属时 |

## Target 粒度审计

| target 类别 | current shape scan | decision | next action |
| --- | --- | --- | --- |
| correctness aggregate | 有 `run_test_compare`，覆盖 Std/RVV 各 8 个 gtest。 | adopted | 保持默认入口。 |
| correctness aliases | topic Makefile 没有按 TEST 家族拆 alias；可用 gtest filter 手动筛选。 | not_applicable with evidence | PI2 后若新增 production direct / fallback TEST，再补 alias。 |
| bench diagnostic aliases | bench 支持 `--case-filter`，所有 label 可单独运行。 | adopted | label 字典见 `benchmark-and-evidence`。 |
| QEMU smoke aliases | 有 `run_qemu_smoke`；可用窄 `run_bench_rvv` 看日志形状。 | adopted | 不把 QEMU timing 写成性能。 |
| board smoke aliases | 有 `run_board_color_coding_smoke`。 | adopted | 只作为可运行 smoke。 |
| board repeated aliases | 有 `run_board_color_coding_repeated`，固定 5-run budget。 | adopted | 只有需要刷新 evidence truth 时运行。 |
| doctor / registry aliases | repeated target 生成 Doctor / registry，`check_evidence_freshness` 检查。 | adopted | 保持 summary-only 提交策略。 |
| historical probe guarded aliases | `run_board_color_coding_production_repeated` 已改为 skip guard，防止完整回滚后覆盖 phase 070 production evidence。 | adopted | 保留 historical evidence；当前 production filter 不产出 `prod_*` case。 |

## 输入数据总览

| 数据形态 | 点类型 / layout | row source（行来源） | 规模 | 证据边界 |
| --- | --- | --- | --- | --- |
| component diagnostic | `ColorPoint`，32-bit RGBA 字段 | indexed leaf gather 或 contiguous output range | leaf 31 / 257 / 1024 / 4096；default 4096 / 16384 | test helper，不代表 public entry |
| production-shaped diagnostic | `pcl::PointXYZRGBA`，真实 RGBA offset | indexed leaf gather 或 contiguous output range | leaf 257 / 4096；default 4096 | production-shaped helper，不代表 production direct |
| implementation-shape diagnostic | `pcl::PointXYZRGBA`，真实 RGBA offset + scratch `uint32_t` | staged scratch store + scalar AoS writeback | staged decode leaf 257 / 4096 | 只回答 staged-store shape 是否值得继续，不证明 production dispatch |
| production direct | `pcl::PointXYZRGBA`，真实 `ColorCoding` public method | phase 080 后全部为标量公开方法 | small / 64 点语义样本 | 证明完整回滚后 Std/RVV 构建语义一致；不证明 production RVV 性能 |

当前没有 `Scalar=double`、indices 以外的 row source、correspondence（对应关系）、mask（掩码）或 weight（权重）入口。`ColorCoding` 处理 byte-level RGBA，不是浮点几何公式。

## 覆盖矩阵

| 路径 | correctness | QEMU log shape | asm | board repeated | production direct |
| --- | --- | --- | --- | --- | --- |
| scalar reference | yes | yes | not_applicable | baseline | no |
| RVV component helper | yes | yes | yes, bench binary | yes | no |
| production-shaped helper | yes | yes | yes, same candidate binary | yes | no |
| `ColorCoding` production source | yes, scalar public-method tests after rollback | yes, production filter skip smoke | no current production RVV asm required | historical phase 070 only | yes; phase 080 says no production RVV adopted |
| `OctreePointCloudCompression` public entry | no | no | no | no | no |

## 可提交证据和默认排除项

可提交或可引用的 summary evidence（摘要证据）：

- `log/board/component_repeat_5/summary.md`
- `log/board/component_repeat_5/evidence_manifest.json`
- `log/board/component_repeat_5/evidence_doctor.md`
- `log/board/production_repeat_5/summary.md`
- `log/board/production_repeat_5/evidence_manifest.json`
- `log/board/production_repeat_5/evidence_doctor.md`
- `log/evidence_registry.json`

默认不提交：`build/`、`log/board/component_repeat_5/run*/` raw logs、私有板卡地址、本机 `config.mk` 和临时编译输出。

## 当前结论边界

phase 080 的 current truth 是 no-production closeout（不接入生产收尾）：`color_coding.h` 中 production RVV 分流已完整回滚，`encodeAverageOfPoints`、`encodePoints`、`decodePoints` 和 `setDefaultColor` 都走标量公开路径。phase 070 的 default-only production public board evidence 作为历史不采纳依据保留：median `1.0035x`、min `0.9945x`，并触发 Evidence Doctor Error。当前没有建议继续推进或保留的 production RVV candidate。
