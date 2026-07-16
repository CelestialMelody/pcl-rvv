# Staging 与证据层级

## Staging 说明

当 RVV 路径把逐点流式标量流程改成“两阶段 staging + 标量 tail”时，函数级评估和主题文档必须说明：

- staging 字段来自原函数哪个局部变量或对象成员。
- invalid lane 如何被移除。
- `vcompress` 后顺序如何对应标量扫描顺序。
- 哪些后续状态继续由标量 tail 处理。
- 小规模、类型不匹配、尺寸越界或非 RVV 构建如何 fallback。

多个 staging 阶段应给出对照表：

```text
| staging 名称 | 结构体 | 生成 helper | 入口调用点 | 字段来源 | 下一段消费者 / tail | gate |
```

## 临时缓冲与 segment store

多字段 `vcompress` staging 常用固定长度临时缓冲。必须把缓冲容量、SEW、LMUL 和 `vlmax` gate 绑定说明。这个 gate 限制单次 chunk lane 数，不限制总输入规模。

固定缓冲检查：

- 记录每个缓冲的元素类型、容量和对应向量 SEW。
- 记录所用 LMUL、`vsetvl` 形态和最大 `vlmax`。
- 证明 `vlmax <= buffer_capacity`，或改用动态缓冲、分段处理、逐字段 spill 后再组装。
- 区分“单次 chunk 最大 lane 数”与“总输入规模”。总输入应由 strip-mined loop 分 chunk 处理，不应被固定缓冲容量限制。
- 若不同编译选项、不同 VLEN 或不同 LMUL 会改变 `vlmax`，gate 必须绑定到实际向量类型，而不是绑定到某次板卡观测值。

segment store 适用条件：

- 多个字段类型相同，SEW 一致。
- AoS 字段顺序、对齐和 stride 与 segment intrinsic 自然匹配。
- `LMUL * NFIELDS` 满足目标 intrinsic 约束，且不会迫使降到吞吐更差的 LMUL。
- 所有字段共享同一个 keep mask，且压缩后的字段一一对应。

segment store 不适用或需谨慎的条件：

- 字段混合类型，例如 float 与 integer、index、flag 混合。
- 字段数较多，segment intrinsic 限制导致 LMUL 降低或寄存器压力过高。
- 需要先跨字段分别压缩，或某些字段来自后续标量计算。
- staging 最终是 C++ 结构体，后续还要按对象状态、source index 或 z 值标量组装。
- segment 写出会掩盖字段来源和 tail 消费关系，使文档和测试难以审计。

字段混合、字段数多、需要跨字段分别压缩或 segment intrinsic 降低吞吐时，SoA 临时缓冲加标量组装通常更可控。文档应记录选择原因，避免后续维护者把临时 buffer 误认为偶然实现细节。

## 测试矩阵

production-shaped diagnostic 至少按目标入口能力考虑：

- 标量公式对拍。
- RVV candidate 对拍。
- 小规模 fallback。
- NaN / Inf / invalid lane。
- 默认输入范围、fake indices、mask 或同类隐式选择集。
- 显式 subset / mask，包含非连续、乱序、重复 index。
- 点类型 traits 矩阵。
- 阈值或投影边界 adversarial case。
- 增量失败路径固定为诊断测试。

测试矩阵应按规则来源分层，避免把某个 case study 的特例升级成所有 RVV 主题的硬规则：

- `generic-rvv`：`vcompress` 保序、多字段同 mask、`vcpop` count、VLEN buffer gate、小规模 fallback、QEMU checksum、反汇编和板卡 full bench。
- `pcl-adapter`：点类型 traits 矩阵、POD / standard-layout / alignment 前提、source/target 不同点类型组合、fake indices、显式 subset / mask、`initCompute()` 或 PCLBase 生命周期。
- `case-specific`：算法自己的投影、距离、阈值、transform、输出对象 bit pattern、状态机或公开入口转调。

fake indices、`setIndices()` subset 和 `initCompute()` 生命周期属于 PCL adapter 入口证据；例如 CEOP 的 organized target、投影公式、identity/FMA 和 depth/distance 则属于 case-specific 证据。

测试数量增多时要做 inventory。只有入口形态、输入构造、参数、断言和覆盖边界都被其它测试完全包含时，才适合删除或合并。删除或合并理由要写入评估和主题文档。

## Bench 命名

- `*-helper` 或 `*-staging`：局部片段，用于潜力和归因。
- `*-diagnostic` 或 `*-candidate`：test-only 同形入口。
- `production-*`：生产源码已接入后，直接测真实入口。
- `*-experimental`：增量诊断路径；正确性失败时不得作为默认 candidate。
