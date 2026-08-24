# Phase 000 计划：current state and component ablation

## 阶段意图和边界

本阶段要证明 `io/include/pcl/io/impl/pcd_io.hpp` 中
`PCDWriter::writeBinaryCompressed<PointT>` 的 compressed write（压缩写入）前置布局转换是否值得继续
RVV 优化。阶段只新增 test-rvv（测试资产）和 topic-local 文档，不修改 production（生产源码）。

validated_scope（本阶段验证范围）：

- writer：templated `writeBinaryCompressed<PointT>` 中 PointCloud AoS 到 field-major 的压缩前置打包。
- layout：4 字节字段为主，覆盖顺序 offset、多字段、尾部 padding、小规模 fallback 和大规模 RVV 候选。
- point type（点类型）：只用 synthetic PointXYZ / PointXYZRGB-like byte layout 代表，不关闭泛型模板入口。
- evidence role（证据角色）：component ablation（组件消融），不是 production direct（真实生产路径）。

unvalidated_scope（本阶段不验证范围）：

- LZF compression / file mapping / stream I/O / header 生成。
- `writeBinary<PointT>`、`writeASCII<PointT>` 和 indices overload。
- `io/src/pcd_io.cpp` 的 PCLPointCloud2 reader / writer；该路径属于独立 topic。
- 所有 datatype / count / offset / point traits（点类型字段特征）组合的泛型生产结论。

## 当前状态清单

| 对象 | 当前状态 |
| --- | --- |
| production | `io/include/pcl/io/impl/pcd_io.hpp` 仍为原始标量 pack 双重循环。 |
| topic docs | 本阶段新建 README、evaluation、roadmap、phase index 和 optimization matrix。 |
| tests | 本阶段新建 `test-rvv/io/pcd_io_templated_writer` 测试。 |
| board | 用户已说明板卡可用；本阶段需要有界 repeated board bench。 |
| Evidence Doctor | 尚未有 pcd_io_templated_writer manifest / doctor 输出。 |

## 假设与候选族

第一候选是 4 字节字段 RVV stride path（跨步路径）：使用 strided load（跨步加载）从
PointCloud AoS 读取字段并 contiguous store（连续写入）到 field-major buffer。非 RVV 构建、非 4 字节字段、
非对齐 offset 或小规模输入回到标量参考链路。

## 优化矩阵

详见 `../optimization-matrix.zh.md`。本 phase 只允许关闭第一行的 component ablation 证据，不能关闭
binary writer、ASCII writer、production-shaped diagnostic 或 production integration（生产接入）条目。

## 实现和测试动作

| action | artifact / command | completion criteria |
| --- | --- | --- |
| A1 写 failing correctness test | `src/test_pcdtw.cpp`，`make run_test_compare` | RVV build 在实现前不能命中 `PackRvv`，证明测试能捕捉缺失候选。 |
| A2 实现 test-only scalar / RVV candidate | `include/pcdtw.h`、`include/impl/pcdtw_support.hpp` | `run_test_compare` 通过，非 RVV 构建走 scalar。 |
| A3 写 component bench | `src/bench_pcdtw.cpp` | QEMU 只跑 `run_bench_rvv` 做 log-shape smoke，不做性能结论。 |
| A4 反汇编归属 | `make dump_bench_rvv` | RVV build 的 bench asm 中能看到与候选相关的 RVV load/store 指令，或记录未闭合原因。 |
| A5 板卡 repeated bench | `make run_board_pcdtw_repeated` | 5 次 repeated summary，decision bucket 稳定或标记 unstable。 |
| A6 Evidence Doctor 和 registry | topic-local manifest + `../../script/evidence_doctor.py` | Errors / Warnings / Suggestions 写入 result，并说明是否降级。 |
| A7 文档回填 | `result.zh.md`、evaluation、roadmap、matrix、Handoff | 每个 action 回填 done / partial / deferred / blocked。 |

## Diagnostic 到 production mismatch audit

| question | answer |
| --- | --- |
| evidence role | component ablation（组件消融） |
| A/B boundary | test helper / bench wrapper，不是 public overload |
| 当前决策问题 | RVV-vs-scalar 是否值得进入 production-shaped diagnostic |
| diagnostic 是否可外推到 production | unknown；需要后续 shaped context 证明 LZF / file boundary 没有吞掉收益 |
| comparison-boundary / baseline mismatch 风险 | yes；组件计时不含 compression、mmap、stream 和 header |
| diagnostic 弱 / 负 / 中性 / 不稳定时是否允许 bounded production probe | 弱正向可进入 shaped diagnostic；负向或不稳定先不建议 production probe |
| clean adoption 是否需要 production boundary 内 RVV-vs-RVV detail A/B | yes；当前阶段不能 clean adopt |

## 板卡复跑预算和决策桶

- run budget（复跑预算）：5 次 repeated board run，`--iterations 20 --warmup-iterations 3`。
- positive：median speedup >= `1.08x` 且无 checksum mismatch。
- weak-positive：median speedup 在 `1.03x` 到 `1.08x`，且实现小、Doctor 无 Error。
- neutral：`0.98x` 到 `1.03x`。
- negative：median speedup < `0.98x`。
- unstable：5 次内 decision bucket 摇摆，或 Doctor Error / checksum 问题无法解除。

## 继续 / 停止条件

默认继续到 A7。合法停止条件只有：构建工具或板卡不可达、Evidence Doctor Error 无法解除、dirty isolation
显示当前 topic 文件无法与无关修改隔离，或组件证据 negative / unstable 后 roadmap 中没有当前授权内的高优先级动作。
如果组件证据 positive / weak-positive，下一 phase 默认是 production-shaped compressed writer diagnostic。
