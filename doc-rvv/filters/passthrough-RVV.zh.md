# filters/passthrough RVV 优化说明

## 背景与范围

`pcl::PassThrough<PointT>` 是常用字段区间过滤器。它的核心 indices 输出路径会线性扫描输入点云，先剔除非有限 `x/y/z`，再读取指定 `FLOAT32` 字段并按 `[filter_limit_min_, filter_limit_max_]` 判断 inlier / removed。

本轮只优化模板路径：

```text
PassThrough<PointT>::applyFilterIndices(Indices &indices)
```

这个函数对象 / 函数入口的作用是：

- `PassThrough<PointT>` 保存用户配置的过滤字段名、上下限、`negative_` 和 removed indices 开关；
- `filter(indices)` 最终调用 `applyFilterIndices(indices)`，输出满足条件的输入点下标；
- 如果用户随后调用 `filter(cloud_out)`，`FilterIndices<PointT>` 基类会先调用同一个 `applyFilterIndices(indices)`，再按这些 indices 复制点到输出点云；
- 因此本轮 RVV 优化的直接对象是“按字段区间生成 inlier/removed indices”的前置筛选阶段，点云输出路径只通过这个 indices 阶段间接受益，后续 `PointT` 拷贝仍保持原逻辑。

已覆盖：

- 标准布局 `PointT`；
- `x/y/z` 为 `float`；
- 过滤字段为 `FLOAT32`，例如 `PointXYZI::intensity`；
- 输入 indices 为 PCLBase 生成的 identity 全量索引；
- 输出为 indices，可选 removed indices；
- 大规模输入。

未覆盖路径全部回退原标量逻辑，包括显式 subset indices、字段为空、非标准点类型、`PCLPointCloud2` 特化、小规模输入等。

## 与上游差异

公开 API 不变。`passthrough.h` 只新增内部受保护 helper 声明：

- `applyFilterIndicesStd`
- `applyFilterIndicesRVV`

`impl/passthrough.hpp` 中原 `applyFilterIndices` 正文被保留为 `applyFilterIndicesStd`。公开入口 `applyFilterIndices` 只在 `__RVV10__` 且覆盖条件满足时短路调用 RVV helper，否则调用 Std。

## 实现结构

新增实体：

| 实体 | 作用 |
| --- | --- |
| `kPassThroughIndicesMinPoints` | 小规模 fallback 阈值，避免短输入承担 RVV strip-mining 和压缩开销 |
| `PassThroughScalar` | 去除 cv/ref 后判断字段类型 |
| `PassThroughXYZCompatible` / `kPassThroughXYZCompatible` | 判断 `PointT` 是否标准布局且 `x/y/z` 为 `float` |
| `applyFilterIndicesStd` | 常驻标量 helper，保留原代码主体 |
| `applyFilterIndicesRVV` | `__RVV10__` 下的 RVV helper，承载 stride load、mask 和压缩写 |

主路径 helper 位于 `pcl` 命名空间，没有额外放入 `pcl::detail`。traits / 类型检测只承担语义判断，不使用 `RVV` 后缀。

公开入口结构：

```cpp
#if defined(__RVV10__)
  if constexpr (pcl::kPassThroughXYZCompatible<PointT>)
  {
    if (!filter_field_name_.empty () &&
        fake_indices_ &&
        indices_->size () >= pcl::kPassThroughIndicesMinPoints &&
        input_)
    {
      // 校验字段存在且为 FLOAT32 后尝试 RVV。
      if (applyFilterIndicesRVV (indices, fields[distance_idx].offset))
        return;
    }
  }
#endif

  applyFilterIndicesStd (indices);
```

## RVV 数据组织

PCL 点云是 AoS。一个 VL chunk 里，RVV 用 `sizeof(PointT)` 作为 stride，从同一批点中分别读 `x/y/z/field`：

```text
内存:
  p[c]       p[c+1]     p[c+2]     p[c+3]
  x y z f    x y z f    x y z f    x y z f

RVV:
  vx = vlse32(base + offsetof(x), stride)
  vy = vlse32(base + offsetof(y), stride)
  vz = vlse32(base + offsetof(z), stride)
  vf = vlse32(base + field_offset, stride)
```

mask 组织：

```text
finite = finite(x) & finite(y) & finite(z) & finite(field)
in_range = !(field < min) & !(field > max)
keep = negative ? !in_range : in_range
keep = keep & finite
drop = !keep
```

`keep` 用于 `vcompress(source_index, keep)` 写 `indices`，`drop` 在 `extract_removed_indices_` 为 true 时写 `removed_indices_`。由于当前只覆盖 identity indices，`source_index = c + lane`，不需要 gather 读取用户 subset。

## 数值算例

假设 `PointXYZI`，过滤字段为 `intensity`，限制为 `[0.0, 1.0]`，`negative=false`。某个 VL chunk 从 `c=8` 开始，VL=4：

| lane | source index | x | y | z | intensity | finite | in_range | keep |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- | --- |
| 0 | 8  | 1 | 2 | 3 | -0.5 | true | false | false |
| 1 | 9  | 4 | 5 | 6 | 0.0  | true | true  | true |
| 2 | 10 | 7 | 8 | 9 | 0.8  | true | true  | true |
| 3 | 11 | NaN | 1 | 2 | 0.5 | false | true | false |

标量公式逐点执行：

```text
if !finite(x/y/z) -> removed
else if !finite(field) -> removed
else if field < 0.0 || field > 1.0 -> removed
else -> indices
```

因此该 chunk 输出：

```text
indices chunk        = [9, 10]
removed_indices chunk = [8, 11]
```

RVV 对应：

```text
source_index = [8, 9, 10, 11]
keep mask    = [0, 1, 1, 0]
drop mask    = [1, 0, 0, 1]

vcompress(source_index, keep) -> [9, 10]
vcompress(source_index, drop) -> [8, 11]
```

这与标量扫描顺序一致。

## 分流与回退

RVV helper 返回 false 的情况：

- `fake_indices_ == false`；
- 点数小于 64；
- 点数超过 `int` 范围；
- 字段 offset 不满足 float 对齐；
- `input_` 无效。

公开入口还会在以下情况下直接落回 Std：

- 非 RVV 编译；
- `PointT` 不满足 `kPassThroughXYZCompatible`；
- `filter_field_name_` 为空；
- 字段不存在或不是 `FLOAT32`；
- `PCLPointCloud2` 特化。

字段不存在、字段非 `FLOAT32` 和 `rgb` 警告仍沿用原有文本和行为；未命中 RVV 时不额外改变标量路径的 warning 时机。

## 已暂缓项

`PCLPointCloud2` indices 路径可用字节 stride 继续扩展，但当前实现位于 `filters/src/passthrough.cpp`，且字段 metadata、indices 初始化和编译链接路径与模板头不同，暂缓为后续主题。

`PCLPointCloud2` cloud 输出路径暂缓，因为它还涉及：

- `keep_organized_`；
- 整点 `memcpy`；
- filtered 点 xyz 写入 `user_filter_value_`；
- `output.width/row_step/data.resize`；
- removed indices 组合。

显式 subset indices 暂缓，因为需要 gather `(*indices_)[lane]` 后再按 source index 读取 AoS 字段，收益和验证成本均高于当前 identity 路径。

## 测试与验证

专项测试：

```text
make -C test-rvv/filters/passthrough run_test_compare
```

结果：std 与 RVV 二进制均通过 6 个测试。

专项 bench 和反汇编：

```text
make -C test-rvv/filters/passthrough run_bench_compare dump_bench_rvv
```

结果：

- `output/qemu/analyze_bench_compare.log` 可解析；
- 无 `未解析`、`n/a`、`Total Time 不计算`；
- 反汇编确认 `vlse32.v`、`vcompress.vm`、`vcpop.m`、`vmflt.vf`、`vmfgt.vf`、`vmand.mm`、`vmnot.m`、`vsetvli ... e32,m2`。

QEMU bench 显示 RVV 路径在 QEMU 下慢于标量，但 QEMU 只作为构建、正确性、日志格式和指令路径证据，不作为性能结论。

上游原始测试：

```text
make -C test-rvv/filters/passthrough run_upstream_test
```

`test/filters/test_filters.cpp` 需要 `bun0.pcd` 和 `milk_cartoon_all_small_clorox.pcd` 两个运行参数。专项 Makefile 使用 `UPSTREAM_TEST_ARGS` 默认指向仓库根目录 `test/` 中已有 PCD，不复制测试数据。该目标已通过上游 filters 全量 21 个测试。

## 板卡结果

专项 Makefile 已提供：

- `deploy_board`
- `run_board_test`
- `run_board_bench_compare`
- `fetch_board_logs`
- `board_smoke`

`run_board_bench_compare` 会在板卡侧完成 std/RVV bench 和 compare 分析；`fetch_board_logs` 会拉回 `output/board`。

本轮在 Codex sandbox 中遇到两层 SSH 差异：

```text
Bad owner or permissions on /etc/ssh/ssh_config.d/20-systemd-ssh-proxy.conf
socket: Operation not permitted
```

处理方式：

- `Makefile` 默认使用 `SSH_OPTS ?= -F $(HOME)/.ssh/config`，绕过 sandbox 中 owner 异常的系统 ssh config；
- 板卡命令在允许网络 socket 的环境中执行。

已运行：

```text
make -C test-rvv/filters/passthrough run_board_test
make -C test-rvv/filters/passthrough run_board_bench_compare fetch_board_logs
```

结果：

- 板卡专项测试 6 个用例通过；
- 板卡 compare 日志：`test-rvv/filters/passthrough/output/board/analyze_bench_compare.log`；
- 日志可解析 Dataset / Iterations / Total Time。

真实板卡性能：

| case | Std ms/iter | RVV ms/iter | speedup |
| --- | ---: | ---: | ---: |
| `passthrough indices range 64K` | 3.2691 | 1.0174 | 3.21x |
| `passthrough indices range 1M` | 52.0308 | 19.4224 | 2.68x |
| `passthrough indices negative 1M` | 53.9325 | 18.4232 | 2.93x |
| `passthrough indices removed 1M invalid` | 58.9814 | 22.6989 | 2.60x |
| `passthrough cloud-out range 1M` | 93.1835 | 57.0853 | 1.63x |
| `passthrough explicit subset fallback 1M` | 26.5917 | 26.7630 | 0.99x |

前四个 identity indices case 命中 RVV 主路径；cloud-out case 通过 `FilterIndices` 基类间接受益，点拷贝仍保持原路径；explicit subset case 是 fallback 语义检查，接近 1.0x 符合预期。

这些 case 名称的含义如下：

| case | 对应入口 | 数据 / 参数 | 是否命中 RVV | 结果含义 |
| --- | --- | --- | --- | --- |
| `passthrough indices range 64K` | `PassThrough<PointXYZI>::filter(Indices&)` | 64K 个 `PointXYZI`，identity indices，`intensity` 在 `[-0.5, 0.75]` 内保留 | 是 | 小一档规模下，RVV 对字段区间判断和 ordered indices 压缩的收益，板卡为 `3.21x` |
| `passthrough indices range 1M` | `PassThrough<PointXYZI>::filter(Indices&)` | 1M 个 `PointXYZI`，同样的正向区间过滤 | 是 | 主工作负载，板卡为 `2.68x` |
| `passthrough indices negative 1M` | `PassThrough<PointXYZI>::filter(Indices&)` + `setNegative(true)` | 1M 个点，保留区间外点 | 是 | 验证 `negative_` 反向语义也由 RVV mask 覆盖，板卡为 `2.93x` |
| `passthrough indices removed 1M invalid` | `PassThrough<PointXYZI> filter(true)` + `filter(Indices&)` | 1M 个点，含稀疏 NaN/Inf，启用 `extract_removed_indices_` | 是 | 同时压缩 inlier 和 removed indices，验证非有限点总是 removed，板卡为 `2.60x` |
| `passthrough cloud-out range 1M` | `PassThrough<PointXYZI>::filter(PointCloud&)` | 1M 个点，输出点云而非只输出 indices | 间接命中 | 基类先调用 RVV indices helper，再标量复制点；收益低于 indices-only，但仍为 `1.63x` |
| `passthrough explicit subset fallback 1M` | `PassThrough<PointXYZI>::filter(Indices&)` + `setIndices(subset)` | 1M 个点的一半显式 subset indices | 否 | 当前 RVV 不覆盖 gather/subset，回退 Std，`0.99x` 用来证明 fallback 路径接近原语义和成本 |

表中的 speedup 按 `Std ms/iter / RVV ms/iter` 计算。例如 `indices range 1M` 为 `52.0308 / 19.4224 = 2.68x`。

## 已知限制

- 当前 RVV 只优化 `PointT` identity indices 的 FLOAT32 字段过滤。
- 点云输出路径只通过基类调用 indices helper 间接受益，点拷贝仍为原路径。
- AoS stride load + mask 压缩在 Milkv-Jupiter 板卡上对 identity indices 路径有明确收益；其它板卡仍需单独验证。
- 上游 `test/filters/test_filters.cpp` 通过 `UPSTREAM_TEST_ARGS` 补齐仓库内 PCD 参数后已运行通过；若后续主题没有直接对应上游测试，可以在评估文档中说明，不需要强制新增。
