# lzf_image_io RVV Topic

## 当前结论

本 topic 已完成 S11 production closeout（生产收尾）。Phase 020 已把 `yuv422_planar_rgb_rvv`
接入 `LZFYUV422ImageReader::read/readOMP` 的有界 production helper，并在板卡重跑生产证据：
Milkv-Jupiter 5-run mean `1.0864x`、median `1.0832x`，Evidence Doctor 为 `0/0/0`。
用户已确认“有收益即可采纳”，当前 patch 视为 adopted production behavior（已采纳生产行为）。

正式 production 长期文档已创建：`doc-rvv/io/lzf_image_io-RVV.zh.md`。该文档使用 phase 020
接入后的生产板卡数据，不使用 phase 000 诊断数据作为最终性能结论。

## 阅读路径

| 读者问题 | 入口 |
| --- | --- |
| 当前函数级结论是什么 | `doc/lzf_image_io-evaluation.zh.md` |
| 阶段计划和结果在哪里 | `doc/phases/README.zh.md` |
| 后续还能尝试什么 | `doc/optimization-roadmap.zh.md` |
| 每个候选证据状态是什么 | `doc/phases/optimization-matrix.zh.md` |
| production closeout 采用了什么 | `../../../doc-rvv/io/lzf_image_io-RVV.zh.md` |
| Phase 020 如何完成 PI5 | `doc/phases/020-yuv-planar-production-probe/result.zh.md` |
| 怎么跑测试和 bench | `doc/testing-overview.zh.md` |
| 每个 gtest 证明什么 | `doc/correctness-tests.zh.md` |
| 板卡 summary、manifest、Doctor 怎么定位 | `doc/benchmark-and-evidence.zh.md` |
| 哪些候选采用、暂缓或拒绝 | `doc/optimization-evidence.zh.md` |
| 测试支撑代码怎么分层 | `doc/test-support-code-map.zh.md` |

## 常用命令

```bash
cd test-rvv/io/lzf_image_io
make run_test_compare
make run_bench_rvv BENCH_ARGS="--case-filter all --iterations 2 --warmup-iterations 1"
make dump_bench_rvv
make check_board_ssh
make run_board_lzf_repeated
make run_board_lzf_yuv422_production_repeated
make check_evidence_freshness
make check_production_evidence_freshness
```

QEMU 只用于 correctness（正确性）、构建和日志形状；性能结论只看板卡或目标硬件。

## 证据白名单

当前文档引用的可提交摘要证据为：

- `log/board/production_shaped_repeat_5/summary.md`
- `log/board/production_shaped_repeat_5/evidence_manifest.json`
- `log/board/production_shaped_repeat_5/evidence_doctor.md`
- `log/board/production_yuv422_repeat_5/summary.md`
- `log/board/production_yuv422_repeat_5/evidence_manifest.json`
- `log/board/production_yuv422_repeat_5/evidence_doctor.md`
- `log/evidence_registry.json`

raw logs 和 `build/` 默认只留本机，不进入提交边界，除非用户明确要求并完成脱敏 / 提交策略确认。
