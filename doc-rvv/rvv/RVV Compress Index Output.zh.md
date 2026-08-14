# RVV 掩码压缩与保序索引输出模式

本文记录 PCL RVV 优化中“筛选后输出 indices”的通用写法：用 `vid` 生成当前 VL chunk 的源下标，用 `vcompress` 按 `keep` 掩码压紧有效 lane，用 `vcpop` 得到写回长度，再连续 `vse32` 写入输出数组。

该模式适用于 `clipPointCloud3D`、盒内点筛选、模型内点选择、条件过滤等“输入顺序扫描，输出保序下标列表”的路径。

## 1. 标量语义

典型标量代码如下：

```cpp
for (std::size_t i = 0; i < cloud.size (); ++i)
{
  if (predicate (cloud[i]))
    indices.push_back (static_cast<int> (i));
}
```

RVV 实现必须保持三个性质：

- 只输出 `predicate` 为 true 的元素；
- 输出下标等于原输入下标；
- 输出顺序与标量扫描顺序一致。

`vcompress` 正好匹配这个语义：它按 lane 顺序把 mask 为 true 的元素压到目标向量低位。

## 2. 基本代码形态

在当前 chunk 的起点为 `i`、当前向量长度为 `vl` 时：

```cpp
const vuint32m2_t local = __riscv_vid_v_u32m2 (vl);
const vuint32m2_t source =
    __riscv_vadd_vx_u32m2 (local, static_cast<std::uint32_t> (i), vl);
const vint32m2_t source_i32 =
    __riscv_vreinterpret_v_u32m2_i32m2 (source);

const vint32m2_t compact =
    __riscv_vcompress_vm_i32m2 (source_i32, keep, vl);
const std::size_t count = __riscv_vcpop_m_b16 (keep, vl);
__riscv_vse32_v_i32m2 (out + kept, compact, count);
kept += count;
```

各步骤含义：

- `vid` 生成 `[0, 1, 2, ...]` lane id；
- `vadd_vx` 加上 chunk 起点 `i`，得到原输入下标；
- `vcompress` 按 `keep` 把有效下标压到低 lane；
- `vcpop` 统计有效 lane 数，也是本轮写回元素个数；
- `vse32(..., count)` 只写压缩后的前 `count` 个 lane；
- `kept += count` 更新输出尾指针。

若输出类型是 `std::uint32_t` 或 bench-diagnosis 原型允许无符号下标，可直接使用 `vcompress_vm_u32m2` 与 `vse32_v_u32m2`。PCL `pcl::Indices` 通常是 `int`，生产路径应与容器元素类型保持一致。

## 3. VL chunk 的算例

假设当前 chunk 起点 `i = 40`，`vl = 6`：

```text
local lane id  = [0, 1, 2, 3, 4, 5]
source index   = [40, 41, 42, 43, 44, 45]
keep mask      = [1,  0,  0,  1,  1,  0]
```

执行 `vcompress(source, keep)`：

```text
compact low lanes = [40, 43, 44, ...]
vcpop(keep)       = 3
store count       = 3
```

写回后输出追加：

```text
out[kept + 0] = 40
out[kept + 1] = 43
out[kept + 2] = 44
```

简图：

```text
predicate mask + source indices -> vcompress -> packed low lanes -> vse32(count)
```

## 4. 保持保序

`vcompress` 按源向量 lane 从低到高扫描，把 mask 为 true 的元素依次放到目标低 lane。只要外层 strip-mining 的 `i` 单调递增，且每个 chunk 写回后执行 `kept += count`，全局输出顺序就与标量 `push_back` 一致。

这与 scatter 不同：scatter 是把每个 lane 写到各自目标地址；compress 是把命中的 lane 压成连续输出。对于 PCL 的 indices 列表，compress 更贴近标量 `push_back` 语义。

## 5. `count` 与 store VL

`vcpop_m_b16(keep, vl)` 只统计当前 `vl` 内激活 lane 的 true 数量。`vcompress` 产生的有效结果位于低 `count` 个 lane，因此 store 的 VL 应为 `count`。

在 intrinsics 中，`__riscv_vse32_v_i32m2(ptr, compact, count)` 会以 `count` 作为本次 store 的 VL。若代码需要显式控制配置，也可以先：

```cpp
const std::size_t vl_store = __riscv_vsetvl_e32m2 (count);
__riscv_vse32_v_i32m2 (out + kept, compact, vl_store);
```

当 `count == 0` 时，store VL 为 0，通常不会写内存；为了让意图更清楚，也可以在热路径成本允许时加块级跳过：

```cpp
if (count != 0)
  __riscv_vse32_v_i32m2 (out + kept, compact, count);
```

## 6. 容量和类型边界

实现前需要确认：

- 输出数组已经预留足够容量，通常可先 `resize(input_size)`，RVV 写入后再 `resize(kept)`；
- `i + vl` 不超过输入规模；
- `std::uint32_t` 到 `int32_t` 的重解释不会改变可表示下标。PCL 常规点云规模应小于 `INT_MAX`，若主题可能处理更大索引，应回退或改用更宽的输出策略；
- 输入来自子集 `indices`，而不是直接按原点云下标 `0..N-1` 顺序扫描时，`vid + i` 只得到子集内的位置，不是原点云下标。此时需要 gather 源下标后再 compress，或回退标量路径；
- 输出值类型必须与公开 API 容器一致，不能为了指令方便改变 `pcl::Indices` 的语义。

## 7. 适用和不适用场景

适合：

- 直接按原点云下标 `0..N-1` 顺序扫描，并输出保序 indices；
- 输出是 POD 标量，如 `int`、`uint32_t`、`float distance`；
- 命中率不固定，但每个元素是否命中可由 mask 表示；
- 标量路径本质是 `if (...) push_back(...)`。

不适合：

- 输出对象需要构造、析构或复杂拷贝；
- 输出位置本身由 lane 动态决定且不是压缩列表；
- 输入源下标来自任意子集但没有可靠 gather；
- 需要稳定处理超过 32-bit 的全局下标。

## 8. 性能结论

`vcompress` 的收益来自减少标量分支和逐元素 `push_back`，并把命中元素连续写回。但它不是免费操作：命中率高时写带宽增加，命中率低时 `vcpop` 和压缩本身仍有固定成本。因此专项 bench 应至少覆盖：

- 中等命中率主路径；
- 高命中率主路径；
- 小规模 fallback 或阈值边界；
- 不满足覆盖条件的 fallback case。

真实性能结论必须来自板卡结果；QEMU 只适合作为正确性、构建和指令路径证据。
