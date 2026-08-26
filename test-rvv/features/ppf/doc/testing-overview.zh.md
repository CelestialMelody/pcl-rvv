# PPF Testing Overview

本文说明 `test-rvv/features/ppf` 的测试入口、target 粒度和证据边界。这里的 QEMU（仿真器）
只用于 correctness（正确性）、路径命中和日志形状；性能结论只来自板卡 repeated benchmark
（重复板卡性能测试）。

## Target 粒度审计

| target 类别 | 当前入口 | 状态 | 证明范围 |
| --- | --- | --- | --- |
| correctness aggregate（正确性汇总入口） | `make -C test-rvv/features/ppf run_test_compare` | adopted | 顺序运行 Std/RVV 两侧 gtest。当前记录为 Std 5/5、RVV 9/9 pass。 |
| correctness aliases（正确性细分入口） | `run_test_std`、`run_test_rvv` | adopted | 分别验证标量 helper 和 RVV 构建下的 candidate / production direct tests。 |
| bench diagnostic aliases（bench 诊断入口） | `BENCH_ARGS="--case-filter <label>" run_bench_rvv` 或 board compare | adopted | `candidate_ppf_pair_feature_batch_rvv`、`candidate_ppf_alpha_m_batch_rvv` 隔离组件候选；不证明 production dispatch。 |
| QEMU smoke aliases（QEMU 小型验证入口） | `run_bench_rvv` 加单 case-filter | adopted | 只证明 bench binary、case-filter、checksum 输出形状可运行。 |
| board smoke aliases（板卡小型验证入口） | `run_board_test fetch_board_logs` | adopted | Phase 060 的 RVV board tests 9/9 pass，证明板卡 correctness gate。 |
| board repeated aliases（板卡重复采集入口） | `board_repeated evidence_doctor_repeated` | adopted | 生成 repeated summary、manifest 和 Evidence Doctor；Phase 060 两组 public case 为 current performance truth。 |
| doctor / registry aliases（证据体检和登记入口） | `evidence_manifest_repeated`、`evidence_doctor_repeated` | adopted with manual registry | topic-local manifest/Doctor 已有；`log/evidence_registry.json` 尚未接入，本 topic 用文档引用和路径扫描做 freshness check。 |
| historical probe guarded aliases（历史探针保护入口） | bench case-filter，而非默认 target | adopted | 历史 negative / diagnostic case 不作为默认 production evidence，只在明确 case-filter 下运行。 |

## 运行入口

`test-rvv/features/ppf/Makefile` 定义 topic 本地 test / bench source：

- `SRCS_TEST := src/test_ppf.cpp`
- `SRCS_BENCH := src/bench_ppf.cpp`
- `TARGET_TEST_STD/RVV := test_ppf_std/test_ppf_rvv`
- `TARGET_BENCH_STD/RVV := bench_ppf_std/bench_ppf_rvv`

共享构建、QEMU、deploy 和 board 规则来自 `test-rvv/mk/rvv-topic.mk`。板卡侧参数在
`test-rvv/features/ppf/board.mk` 中定义。

## 覆盖矩阵

| 证据层 | 当前覆盖 | 不覆盖 |
| --- | --- | --- |
| Std correctness | reference 和 production-like all-pairs 语义。 | RVV 指令和 production dispatch。 |
| RVV correctness | Phase 010/030 candidate、Phase 040/060 production direct hit、fallback gate。 | 真实硬件性能。 |
| QEMU bench smoke | case-filter 和日志形状。 | 性能结论。 |
| asm attribution（反汇编归属） | `dump_bench_rvv` 证明 production helper 实例含 RVV 指令。 | 指令快慢和目标硬件收益。 |
| board correctness | Phase 060 RVV gtest 9/9 pass。 | Std/RVV 性能差异。 |
| board repeated | Phase 040 exact case、Phase 060 两个代表性点型 public case。 | 每个自定义 traits-compatible 点型、`Scalar=double`、其它 row source。 |

## 当前停止边界

Phase 060 后的 production-public board evidence（生产公开入口板卡证据）已经稳定 positive。
Phase 070 只补文档套件，不新增计时数据。若以后要继续运行 bench，应先明确新决策问题：
证据增强、direct-AoS pair-feature revisit，或 PPFRGB / CPPF 等 caller 独立 topic。
