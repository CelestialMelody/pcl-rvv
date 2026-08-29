# DOTMOD Template Matching RVV

## 当前状态

`recognition/src/dotmod.cpp` 中 `pcl::DOTMOD::detectTemplates()` 已采用 RVV
（RISC-V Vector，可变长度向量扩展）生产路径。RVV 构建下，窗口计分直接读取
`QuantizedMap` 原图窗口，并用 byte AND（字节按位与）、mask（掩码）和 `vcpop`
统计命中；非 RVV 构建保持原 `getSubMap()` 标量路径。当前生产实现还复用
`responses` 缓冲，避免每个滑窗位置重复构造 vector。

当前 production-public（真实公开入口）板卡证据来自
`test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/summary.md`。
路径名沿用 Phase 020 target，内容为 Phase 030 response-buffer-reuse 后的当前证据：
5-run median speedup `3.298x`，range `3.276x - 3.304x`，`B/A < 1 = 0/5`。
Evidence Doctor（证据体检）为 `Errors=0`、`Warnings=0`、`Suggestions=2`。

## 函数语义和标量路径

`DOTMOD::detectTemplates()` 先从每个 `DOTModality` 获取 dominant quantized map（主量化图），
再按 row/col 滑窗扫描。每个窗口内，函数遍历 modality 和 template，对图像窗口 byte 与模板
byte 做按位与。非零结果累加到 `responses[template_index]`。随后函数用
`1 / (template_bins_x * template_bins_y)` 缩放 score（分数），当 `response > threshold`
时按滑窗和 template 顺序追加 `DOTMODDetection`。

原标量路径在每个 modality/window 上构造 `QuantizedMap::getSubMap()` 临时子图，再对连续子图数据计数。
这个临时子图分配和拷贝位于滑窗内层，是当前 RVV 生产路径绕开的主要成本之一。

## 当前采用的优化方式

| 维度 | 当前状态 | 采用或暂缓原因 | 证据 | 边界 / 下一步 |
| --- | --- | --- | --- | --- |
| RVV dispatch（分流逻辑） | adopted | `__RVV10__ && __riscv_vector` 下启用，非 RVV 构建不包含 RVV include 或 helper | production direct correctness、asm | public API 不变 |
| direct-window score | adopted | 避免 RVV 构建下每窗口 `getSubMap()`，并向量化 byte hit count | board median `3.298x` | 覆盖 row-major `uint8_t` map 和 contiguous template features |
| response-buffer-reuse | adopted | 每个窗口只需清零已有 `responses`，无需重复分配 vector | Phase 030 correctness 和 board evidence | 不改变输出顺序或 threshold 语义 |
| threshold / detection output | scalar tail | 输出 vector append 需要保序；当前证据显示主收益来自窗口读取和计数 | production direct checksum | 真实 workload 显示输出尾段热点时再评估 |
| standalone `getSubMap()` | not_applicable | RVV 生产路径已绕开该 caller，非 RVV 构建保留原路径 | source diff | 其它 caller 需要时另开 support-kernel topic |

VL chunk（可变向量长度分块）内部流程是：按窗口行计算 image row 和 template row 指针，
用 `vle8` 连续加载 byte，`vand` 得到命中 byte，`vmsne` 生成非零 mask，`vcpop` 统计当前
chunk 命中数。每行 tail 由 `vsetvl` 自然处理。检测输出仍由原标量 tail 完成。

## 范围决策表

| 范围 | 状态 | 证据 | 下一步 |
| --- | --- | --- | --- |
| `DOTMOD::detectTemplates()` production-public byte map | adopted | production direct gtest、asm、board summary、Evidence Doctor | reviewer 审查 |
| 非 RVV 构建 | scalar fallback | source guard 保留原 `getSubMap()` 标量路径 | 无需新增运行时 gate |
| 点类型 / `Scalar` 扩展 | not_applicable | DOTMOD matching 使用量化 byte map，不是点类型模板入口 | none |
| 真实 RGB-D workload 分布 | deferred | 当前 bench 使用 fixed modality synthetic maps | 需要 profile 或真实数据集后重启 |
| DOTMOD modality preprocessing | separate topic | `color_gradient_dot_modality` 已有独立 topic；本文件只处理 matching | 不并入当前 closeout |
| `QuantizedMap::getSubMap()` standalone | separate support kernel | 当前 adopted path 不再依赖 RVV 构建下的该调用 | 其它 caller 需要时单独评估 |

## Traceability Map

| 符号 / 文件 | 层级 | 作用 | 调用者 / 上游入口 | 被调用者 / 下游消费者 | 证据角色 | 位置 |
| --- | --- | --- | --- | --- | --- | --- |
| `pcl::DOTMOD::detectTemplates()` | production public entry | 真实匹配入口 | DOTMOD callers | RVV helper 或标量 `getSubMap()` 路径 | production boundary | `recognition/src/dotmod.cpp` |
| `dotmodScoreWindowDirectRVV()` | production RVV helper | 一个窗口、一个模板的 RVV byte hit count | `detectTemplates()` | RVV intrinsic | asm attribution | `recognition/src/dotmod.cpp` |
| `run_production_direct_test_compare` | correctness target | 对拍真实入口 Std/RVV 输出 | topic Makefile | QEMU/run command | production direct correctness | `test-rvv/recognition/dotmod_template_matching/Makefile` |
| `bench_dotmod_template_matching_production_direct.cpp` | bench wrapper | 计时 `detectTemplates()` total | board repeated target | summary script | board performance | `test-rvv/recognition/dotmod_template_matching/src/bench_dotmod_template_matching_production_direct.cpp` |
| `generate_dotmod_template_matching_evidence_manifest.py` | analysis script | 生成 summary / manifest | evidence targets | Evidence Doctor / registry | evidence manifest | `test-rvv/recognition/dotmod_template_matching/script/generate_dotmod_template_matching_evidence_manifest.py` |
| `summary.md` | evidence output summary | 保存 5-run production-public speedup | board collect target | evaluation / 本文 / Handoff | performance evidence | `test-rvv/recognition/dotmod_template_matching/log/board/repeated_phase020_production_direct/summary.md` |
| `dotmod_template_matching-evaluation.zh.md` | evaluation | 保存候选取舍、phase 链和证据边界 | reviewer / worker | 本文和 phase docs | decision audit | `test-rvv/recognition/dotmod_template_matching/doc/dotmod_template_matching-evaluation.zh.md` |

## 数值算例

假设窗口一行有 8 个 byte，图像行为 `[1, 2, 4, 8, 16, 32, 64, 128]`，
模板行为 `[1, 1, 4, 1, 16, 1, 64, 1]`。按位与结果为
`[1, 0, 4, 0, 16, 0, 64, 0]`，非零 mask 有 4 个 lane（向量通道）。
RVV helper 对这一行贡献 `4` 到 score。多行 score 相加后，再乘以原标量路径的
`scaling_factor`，因此 threshold 和 detection 输出语义保持一致。

## Bench 与证据

| evidence | command / path | result | boundary |
| --- | --- | --- | --- |
| production direct correctness | `make -C test-rvv/recognition/dotmod_template_matching run_production_direct_test_compare` | Std/RVV 各 2 个 gtest 通过 | 证明真实入口输出和 strict threshold 语义 |
| asm attribution | `make -C test-rvv/recognition/dotmod_template_matching dump_production_direct_bench_rvv` | `dotmodScoreWindowDirectRVV`、`vle8`、`vand`、`vmsne`、`vcpop` present | 证明路径和指令归属 |
| board performance | `log/board/repeated_phase020_production_direct/summary.md` | median `3.298x`，`0/5` 退化 | 仅该板卡和该 synthetic production-public case |
| Evidence Doctor | `log/board/repeated_phase020_production_direct/evidence_doctor.md` | `Errors=0`、`Warnings=0`、`Suggestions=2` | metadata suggestion 不阻塞当前结论 |
| registry | `log/evidence_registry.json` | summary / manifest / doctor 已登记 | 支撑 freshness check（新鲜度检查） |

QEMU（仿真器）只用于 correctness、构建、路径和日志形状。本文不使用 QEMU timing（QEMU 计时）
作为性能结论。

## Fallback 矩阵

| 条件 | 行为 | 证据 / 理由 |
| --- | --- | --- |
| 未定义 `__RVV10__` 或未定义 `__riscv_vector` | 编译时不包含 RVV helper，走原 `getSubMap()` 标量路径 | source guard |
| RVV 构建且进入 `detectTemplates()` | 使用 direct-window RVV score helper | asm 和 board evidence |
| threshold / detection 输出 | 保持标量 tail | correctness gtest 和 checksum |
| 点类型 / `Scalar=double` | not applicable | DOTMOD matching 输入是 `uint8_t` map |
| 其它 DOTMOD modality 或 `getSubMap()` caller | 不由本 patch 接管 | topic scope |

## 正确性与高效性证据链

Correctness（正确性）：production direct tests 通过真实 `createAndAddTemplate()` 写入模板状态，
再调用真实 `detectTemplates()`。测试覆盖稳定 detection 输出、字段范围、score 超阈值和严格 `>` 阈值边界。

Path / asm（路径 / 反汇编）：生产直连 bench binary 中 `dotmodScoreWindowDirectRVV()` 可见，目标 RVV
指令存在。该证据用于归属，不用于性能排序。

Performance（性能）：性能结论来自板卡 repeated summary。当前 `dotmod_production_detecttemplates_total`
5-run median `3.298x`，range `3.276x - 3.304x`，所有 run 均正向。

Boundary（边界）：EvidenceDecision 只覆盖本 topic 的真实公开入口、row-major byte map、
contiguous template features 和当前 case-filter `256 192 24 16 100 5 8 2 0.9 1`。
真实 RGB-D 输入分布、其它尺寸和其它 caller 需要重新取证。

Risk（风险）：Evidence Doctor 建议补环境字段和 binary hash。当前 positive bucket 稳定，
这两个 suggestion 不阻塞采纳；它们是后续异常复核的优先补项。

## 结论与后续方向

当前建议保留 production patch，并进入 reviewer 审查或提交准备。继续优化当前 topic 需要新的输入：
真实 workload profile、更多尺寸矩阵或输出尾段热点证据。`QuantizedMap::getSubMap()` standalone、
DOTMOD modality preprocessing 和真实数据集 profile 都应作为独立后续范围处理。
