# OpenNI2 Grabber Test Support Code Map

| path | role | 说明 |
| --- | --- | --- |
| `include/openni2_grabber.h` | aggregator + helper | 当前单头聚合 `CameraModel`、scalar reference、RVV candidate 和 candidate wrapper。 |
| `src/test_openni2_grabber.cpp` | correctness tests | gtest same-chain、边界语义和 bitwise 对拍。 |
| `src/openni2_grabber_test_hook.cpp` | production-detail hook | 以 `PCL_RVV_OPENNI2_GRABBER_TEST_HOOK` 包含真实 `io/src/openni2_grabber.cpp`，暴露测试专用 hook。 |
| `src/openni2_grabber_production_detail_test.cpp` | production-detail correctness tests | 对 production helper 做 bitwise 对拍和 path hit 检查。 |
| `src/bench_openni2_grabber.cpp` | bench harness / cases | CLI、synthetic input、checksum、case registry 和计时边界。 |
| `Makefile` | topic targets | QEMU、board smoke、repeated board 和 summary 生成入口。 |
| `board.mk` | board config include | topic-local board hook，具体私有连接参数不写入文档。 |
| `log/evidence_registry.json` | local generated evidence registry | 由 `make record_board_openni2_grabber_repeated_evidence_state` 生成，默认不提交。 |
| `doc/phases/*` | phase suite | phase plan/result、optimization matrix 和恢复入口。 |
| `doc/*.zh.md` | doc suite | evaluation、测试、证据、优化结论和代码地图。 |

## Helper 职责审计

当前 `include/openni2_grabber.h` 混合 core types、reference、RVV candidates 和 wrappers，职责超过拆分阈值，但文件长度仍低于 hard limit。Phase 020 没有把 test helper 搬入 production，而是在 `io/src/openni2_grabber.cpp` 旁边新增 production detail helper；测试支撑通过 hook 调用真实生产源码。后续若继续扩大 RGB/RGBA 或 IR，可再把 topic test support 拆到 `include/impl/`。

## 结构状态

`src/`、`include/`、`doc/phases/` 已采用配置解析出的 topic 结构；尚未采用 `include/impl/` 内部拆分。当前缺口不阻塞 adopted closeout（已采纳收尾）：新增 production-detail hook 和测试文件已经把生产边界与诊断 helper 分开。
