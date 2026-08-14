# ICP transformCloud 优化路线图

本路线图记录跨 phase 候选搜索空间。阶段流水放在 `doc/phases/`，长期结论放在
`../../../doc-rvv/registration/icp-RVV.zh.md`。

| candidate family | idea source | row source / 点型 / Scalar | 预期收益 | 风险 | 证据需求 | 状态 | next phase |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `xyz-masked-store-m2` | 当前源码逐点 XYZ 变换循环 | full-cloud，`PointXYZ`，`Scalar=float`，AoS layout gate | 减少逐点 `memcpy` / Eigen 小矩阵乘法开销。 | 带掩码写回、FMA 舍入、小规模噪声。 | production direct gtest、board repeated、asm attribution、Doctor。 | adopted | review |
| `xyz-normal-two-mask-m2` | 当前源码 normals 分支 | full-cloud，`PointNormal`，`Scalar=float`，AoS layout gate | 同一轮覆盖 XYZ 和 normal 两个热点。 | XYZ 和 normal finite gate 不能误合并；normal 非有限不能回滚 XYZ。 | production direct gtest、board repeated、asm attribution、Doctor。 | adopted | review |
| production generic layout gate | PCL RVV point traits 经验 | 模板 `PointSource`，registered float XYZ 或 XYZ+normal AoS | 对 `PointXYZI` / `PointXYZINormal` 等兼容点型开放 RVV。 | 性能 repeated 代表点型仍是 `PointXYZ` / `PointNormal`；未知自定义点型只按 traits+offset gate 进入。 | generic direct tests、runtime offset fallback、`Scalar=double` fallback。 | adopted with representative performance | review |
| QEMU bench compare default guard | agent asset 反馈 | 所有 topic 的 QEMU bench compare | 避免无意义 QEMU 性能运行。 | 旧脚本若依赖默认 `run_bench_compare` 需要显式加 `ALLOW_QEMU_BENCH_COMPARE=1` 并写 `qemu_smoke_only`。 | guard smoke、skill/reference 更新。 | adopted | none |
| doc-suite parity | mature sibling quality bar + ICP 文档复盘 | README、topic-local docs、evaluation、长期 `doc-rvv` | reviewer 可从稳定文档路径恢复测试语义、bench 证据、优化方式和代码地图。 | 文档扩展不能复制成熟 sibling 的算法流水；新增 doc-suite 文件若未纳入 topic artifact tracking，README 引用会在提交后断链。 | Phase 004 audit、doc ownership、Traceability Map、包含 untracked 的 topic 路径扫描。 | adopted / pending commit boundary | review |
| `IterativeClosestPointWithNormals` path | 当前源码 override | 不走本 `transformCloud` | 不适用。 | 另一个生产函数族。 | 若要优化应另开 transforms / normals topic。 | rejected for current topic | none |

## 默认恢复动作

1. 先运行 `make -C test-rvv/registration/icp evidence_status`，确认当前 board summary、Doctor 和 docs 是 fresh。
2. 若只需复核 correctness，运行 `run_test_compare record_qemu_correctness_state` 和板卡 `run_board_test fetch_board_logs`。
3. 若 reviewer 要复核性能，只运行板卡 / target hardware repeated target；不要默认运行 QEMU `run_bench_compare`。
4. 若 reviewer 需要恢复文档结构审计，先读 `doc/phases/004-structure-parity-doc-suite/result.zh.md`。
5. 若复核文档补齐或准备提交，运行
   `git status --short --untracked-files=all -- test-rvv/registration/icp doc-rvv/registration/icp-RVV.zh.md`。
   README 引用的 doc-suite 文件若仍是 untracked，必须先进入 topic commit 边界或移除引用。
6. 若进一步扩大证据，优先考虑 20-run board confirmation 或端到端 ICP profile，而不是更多 QEMU timing。
