# RVV 多字段压缩 staging 输出模式

本文记录 PCL RVV 优化中“多字段稀疏输出”的通用写法：在 VL chunk 内用 mask 过滤有效 lane，对多个字段分别执行 `vcompress`，先把压缩后的 SoA 向量落到固定大小的临时数组，再用标量循环拼回 AoS staging 结构。

该模式适用于输出不是单个 index，而是多个字段组成的候选结构，例如：

```cpp
struct Candidate {
  std::uint32_t source_index;
  std::uint32_t target_index;
  float x;
  float y;
  float z;
};
```

## 1. 标量语义

典型标量代码是逐点扫描并按条件追加结构体：

```cpp
for (std::size_t i = 0; i < n; ++i) {
  if (!predicate(i))
    continue;

  out.push_back(Candidate{source_index, target_index, x, y, z});
}
```

RVV 实现需要保持：

- 只输出 predicate 为 true 的 lane；
- 每个字段来自同一个原始 lane；
- 输出顺序与标量扫描顺序一致；
- 后续 scalar tail 读取 staging 时能从同一局部状态继续执行。

单字段 index 输出可以直接 `vcompress + vse32` 到最终数组；多字段 AoS staging 还需要把多个压缩后的字段重新组合成结构体。

## 2. 基本代码形态

RVV 计算阶段通常得到分列的 SoA 向量：

```text
source_index: [s0, s1, s2, s3, ...]
target_index: [t0, t1, t2, t3, ...]
x:            [x0, x1, x2, x3, ...]
y:            [y0, y1, y2, y3, ...]
z:            [z0, z1, z2, z3, ...]
keep:          1   0   1   1  ...
```

压缩后每个字段仍保持同一 lane 顺序：

```cpp
const vuint32m2_t source_kept = __riscv_vcompress_vm_u32m2(source, keep, vl);
const vuint32m2_t target_kept = __riscv_vcompress_vm_u32m2(target, keep, vl);
const vfloat32m2_t x_kept = __riscv_vcompress_vm_f32m2(x, keep, vl);
const vfloat32m2_t y_kept = __riscv_vcompress_vm_f32m2(y, keep, vl);
const vfloat32m2_t z_kept = __riscv_vcompress_vm_f32m2(z, keep, vl);
const std::size_t count = __riscv_vcpop_m_b16(keep, vl);
```

随后把每个字段落到临时 SoA buffer：

```cpp
alignas(16) std::uint32_t source_buf[64];
alignas(16) std::uint32_t target_buf[64];
alignas(16) float x_buf[64];
alignas(16) float y_buf[64];
alignas(16) float z_buf[64];

__riscv_vse32_v_u32m2(source_buf, source_kept, count);
__riscv_vse32_v_u32m2(target_buf, target_kept, count);
__riscv_vse32_v_f32m2(x_buf, x_kept, count);
__riscv_vse32_v_f32m2(y_buf, y_kept, count);
__riscv_vse32_v_f32m2(z_buf, z_kept, count);
```

最后用短标量循环拼回 AoS staging：

```cpp
for (std::size_t lane = 0; lane < count; ++lane) {
  out[kept + lane] = Candidate{
      source_buf[lane], target_buf[lane], x_buf[lane], y_buf[lane], z_buf[lane]};
}
kept += count;
```

这段标量循环的迭代次数最多等于当前 `vl`，不是输入总规模。它只负责结构体组装，不重新执行 predicate。

## 3. 设计动机

当前写法解决的是“RVV 寄存器 SoA 表达”和“C++ staging AoS 结构”之间的布局差异。

```text
RVV register form:
  source[] target[] x[] y[] z[]

staging memory form:
  {source, target, x, y, z}
  {source, target, x, y, z}
  ...
```

直接把压缩结果写入 AoS 结构体通常需要以下条件之一：

- 有合适的 segment store，并且输出是 dense、字段布局连续、写出数量等于 `vl` 或能自然处理压缩后的低 `count` lane；
- 使用 scatter / indexed store，把每个字段写到结构体内对应 offset；
- 改变 staging 数据结构，让后续消费者接受 SoA 数组。

这些方案在 PCL production path 中需要更强的前置条件。稀疏输出的 `count` 每个 chunk 都变化，结构体可能包含混合类型，后续代码通常已经以 AoS 候选结构表达状态。临时 buffer + 标量组装能用简单代码保持标量 `push_back` 顺序，并避免在生产源码中引入复杂 scatter 地址计算。

## 4. 这是常见 SIMD 处理方式

该模式属于常见 SIMD 工程折中。SIMD/RVV 擅长批量计算、批量 mask 和 SoA 风格寄存器处理；C++ 算法和 PCL 数据结构常以 AoS、`push_back`、候选结构或对象状态机组织。对于“筛选后输出多个字段”的路径，先在 vector 侧完成重计算和保序压缩，再用一个短标量尾段组装结构体，是可维护性和性能之间常用的平衡点。

它尤其适合以下条件：

- predicate 和主要数学计算已被 RVV 批量化；
- 输出数量可变，必须保持标量扫描顺序；
- 输出结构体是后续 scalar tail 的状态载体；
- 直接 scatter 或结构体向量写出会显著增加代码复杂度；
- 板卡 full case 证明临时 buffer 和标量组装没有抵消收益。

该模式不表示“输出无法继续 RVV 化”。它是一个可验证的中间边界：先把高收益、低语义风险的 predicate 前移到 RVV，再根据 profile 和语义风险决定是否继续优化写出。

## 5. 替代方案

| 方案 | 适用条件 | 风险 / 成本 |
| --- | --- | --- |
| 直接 SoA staging | 后续消费者也能改成 SoA；字段生命周期清晰 | 改动范围扩大，可能影响现有 scalar tail 和文档结构 |
| segment store 到 AoS | 字段同类型、连续布局、dense 或压缩后低 lane 可直接成组写 | 对混合类型结构体不自然；稀疏输出仍需处理可变 `count` |
| scatter 到 AoS 字段 | 需要完全向量化结构体写出，且地址计算可控 | 指令多、地址复杂、维护成本高；收益需要板卡证明 |
| prefix-sum / compact indices 后 scatter | 输出结构复杂，但必须保持全向量写出 | 实现复杂，通常需要更多临时数组和额外 pass |
| 保留标量 append | 输出构造复杂或命中率低，RVV 只负责前置筛选 | RVV 覆盖范围变小，full speedup 可能被尾段稀释 |

生产路径优先选择能保持语义清晰并有板卡收益的最小方案。只有当 append / staging 组装成为明确热点，且直接 AoS / SoA / scatter 方案能通过 correctness、反汇编和板卡收益闭环时，才继续推进输出写出 RVV 化。

### 5.1 segment store 的适用边界

Segment store 的优点是可以把多个 vector register 按交错布局直接写到内存，例如把 `x[] / y[] / z[]` 写成 `{x,y,z}` 连续结构。若输出结构满足字段同宽、同类型、无 padding、field count 与当前 LMUL 组合合法，并且写入对象的 C++ 类型语义清晰，它确实可以省掉 `source_buf[] / x_buf[] / ...` 这类临时 SoA buffer 和后续短标量组装。

该方案在 CEOP 的 projected staging 上暂不作为默认实现，原因包括：

- `ProjectedOrganizedProjectionCandidate` 是 `uint32_t, uint32_t, float, float, float` 的混合字段结构。RVV segment store intrinsic 通常按同一 EEW / 同一元素类型组织 tuple；把 float bit pattern 先 reinterpret 成整数再写入结构体，会把问题转移到对象表示、别名规则和布局 static assert 上。
- 当前 helper 使用 `e32m2`。`ProjectedOrganizedProjectionCandidate` 有 5 个 32-bit 字段，`nf=5` 的 segment store 与 `m2` 组合不满足常见 `LMUL * NFIELDS <= 8` 约束；降低 LMUL 或拆成多次 segment store 会改变当前吞吐和寄存器组织，需要重新 bench。
- 稀疏输出先经过 `vcompress`，每个 chunk 的 `keep_count` 可变。Segment store 可以写压缩后的低 `keep_count` lane，但仍需要严谨证明写入地址、对象布局、padding、field 顺序和后续 scalar tail 读取完全一致。
- CEOP 的板卡 full case 已证明临时 buffer + 标量 AoS 组装没有抵消收益。segment store 若只减少每个 chunk 至多 64 lane 的组装开销，收益需要单独 profile 证明。

因此 segment store 是可评估的后续实验方向，不是当前 production closeout 的必要改动。更适合优先尝试的场景是字段全为 `float` 的 `x/y/z` 或 `normal_x/y/z` dense store、结构体字段连续且已有公共 `rvv_point_store` primitive 可表达的路径。

## 6. 固定临时 buffer 与 VLEN gate

若使用固定长度栈 buffer，需要在 helper 入口设置 VLEN gate：

```cpp
const std::size_t vlmax = __riscv_vsetvlmax_e32m2();
if (vlmax > 64)
  return false;
```

然后使用与 gate 一致的临时数组：

```cpp
alignas(16) float x_buf[64];
```

这个 gate 与 RVV 向量变量类型直接相关。当前 CEOP helper 使用的是 `vfloat32m2_t`、`vuint32m2_t`、`vint32m2_t`，对应 `e32,m2`：元素宽度是 32 bit，LMUL 是 2。`__riscv_vsetvlmax_e32m2()` 返回当前硬件 VLEN 下 `e32,m2` 一次最多能处理的 lane 数，近似关系是：

```text
vlmax_e32m2 = VLEN_bits * 2 / 32
```

示例：

| VLEN | `e32,m2` 最大 lane 数 |
| --- | --- |
| 128 bit | 8 |
| 256 bit | 16 |
| 512 bit | 32 |
| 1024 bit | 64 |
| 2048 bit | 128 |

`vcompress` 后最坏情况下所有 lane 都保留，`keep_count` 可以等于当前 `vl`，而 `vl` 最大可以到 `vlmax_e32m2`。因此固定 `source_buf[64]`、`x_buf[64]` 这类栈 buffer 的安全条件是：

```text
vlmax_e32m2 <= 64
```

如果未来某个 RVV 实现的 VLEN 更大，例如 `e32,m2` 的最大 lane 数达到 128，继续使用 `[64]` buffer 就可能在 `vse32(..., keep_count)` 时越界。当前生产实现选择在这种目标上回退标量，而不是引入动态 scratch 或更复杂的分块写出。

这种写法的边界清晰：当前生产实现只覆盖 `e32m2` 最大 VL 不超过 64 的目标；更大 VLEN 自动回退标量，避免栈 buffer 越界。若后续需要支持更大 VLEN，可以改成动态 scratch、分块固定 VL、降低 LMUL 或直接 SoA staging，但需要重新验证性能和代码复杂度。

`alignas(16)` 给临时数组一个稳定对齐，避免栈对齐不足导致低质量 store 代码。对齐值不应被写成语义条件；真正的安全边界是 `vlmax <= buffer_length`。

## 7. CEOP 示例

`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` 中的 `projectOrganizedProjectionPixelsRVV()` 使用该模式。

该 helper 消费：

```cpp
OrganizedProjectionCandidate{source_index, x, y, z}
```

其中 `x/y/z` 已是 source 点经 identity fast path 或 4x4 transform 后的 target camera 坐标。helper 执行：

```text
uv0 = z * cx; uv0 = vfmacc(uv0, fx, x)
uv1 = z * cy; uv1 = vfmacc(uv1, fy, y)
u/v = vfdiv + vfcvt.rtz
keep = u/v in bounds
target_index = v * width + u
vcompress(source_index, target_index, x, y, z)
```

压缩后的多个字段先写到：

```cpp
source_buf[64], target_buf[64], x_buf[64], y_buf[64], z_buf[64]
```

再组装为：

```cpp
ProjectedOrganizedProjectionCandidate{source_index, target_index, x, y, z}
```

后续 `acceptProjectedOrganizedProjectionCandidatesRVV()` 继续消费该 staging，执行 target gather、target finite、depth mask 和 final distance predicate。最终 `pcl::Correspondence` append 仍保留标量，避免把可变数量结构体写出和 stored distance bit pattern 绑定到复杂 scatter 实现。

## 8. 使用准则

采用该模式前应确认：

- `vcompress` 的字段集合完整覆盖后续 scalar tail 所需状态；
- 每个字段使用同一个 `keep` mask，避免跨字段 lane 错配；
- `vcpop` 的 `count` 同时用于所有字段 store 和 AoS 组装循环；
- staging 容器先按输入上界 `resize`，结束后再 `resize(kept)`；
- 固定临时 buffer 有明确 `vlmax` gate；
- 32-bit indexed load/store 的 byte offset 有边界检查；
- 文档说明该 staging 对应原标量代码的哪一段，以及后续哪些状态仍由 scalar tail 负责；
- QEMU 用于 correctness、checksum 和指令路径，真实性能以板卡 full case 为准。

该模式对后续 PCL RVV 优化有指导价值：凡是遇到“批量 predicate + 多字段候选 + 保序 append”的函数，可以先用它建立 production-shaped diagnostic，再决定是否继续推进更激进的 AoS/SoA 输出改写。
