# RVV Buffer 与 Staging 策略

本文记录 PCL RVV 优化里 buffer 的通用判断方法。这里的 buffer 指 RVV 计算结果和后续消费者之间的临时内存承接点。它可以是连续工作区、固定栈数组、压缩后 staging 数组，也可以是为了保序累加而保留的短数组。

## 1. 先看 buffer 在链路里的角色

RVV 路径里的 buffer 只做一件事：把某个 VL chunk 的中间态交给下一段代码。下一段代码可能是向量归约，可能是标量 tail，也可能是 AoS 结构体组装。

角色一旦确定，后续判断就可以按消费者类型展开。

| 形态 | 作用 | 保留条件 | 常见替代 | 例子 |
| --- | --- | --- | --- | --- |
| 连续工作区 | 承接连续 `float` 结果，供归约、归一化、二次写回使用 | 后续仍要读同一批连续值 | 直接写回、重算、两阶段写回 | `2d/include/pcl/2d/impl/kernel.hpp` |
| 参数 staging | 先把每个 lane 的参数算成标量数组，再用 `vle32` 装回向量寄存器 | 生成公式更适合先用标量推进，或者 helper 接口已经按数组组织 | 用 `vid` 直接生成 lane 坐标 | `2d/include/pcl/2d/impl/kernel.hpp` |
| 压缩后 staging | `vcompress` 后把多个字段分别落到临时数组，再拼回结构体 | 后续消费者要 AoS 结构，且字段要保序一致 | segment store、scatter、改成 SoA 消费者 | `registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` |
| 保序累加缓冲 | 先存一段 lane 值，再按原顺序做标量累加 | 累加顺序要和标量路径一致 | 向量规约、block reduction | `filters/include/pcl/filters/impl/bilateral.hpp` |
| 固定行缓冲 | 保存压缩后的局部行项，再做标量法方程累加 | 目标仍是逐行消费，且 buffer 有明确 VLEN 上界 | 分块规约、`vfredosum` | `registration/include/pcl/registration/impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` |

## 2. 什么时候该保留 buffer

保留 buffer 的条件，优先看后续消费者。

1. 后续代码要消费具体结构体。
   例如 `vcompress` 之后仍要拼成 `Candidate`、`ProjectedOrganizedProjectionCandidate`、`AcceptedOrganizedProjectionCandidate` 这类 AoS 记录。

2. 后续代码要保持标量顺序。
   例如 `radiusSearch` 邻居顺序、`push_back` 顺序、或者某个标量 tail 里的状态机顺序。

3. 后续代码仍是标量 tail。
   这种情况里，buffer 负责把 lane 状态留给 tail，tail 继续做对象构造、距离重算、append 或 solver 状态更新。

4. 临时数组有明确上界。
   这类数组必须和 `__riscv_vsetvlmax_*()` 的 gate 配套。若上界写死，就要先证明最坏 `keep_count` 不会超过数组长度。

## 3. 什么时候可以去掉 buffer

如果 buffer 只服务于固定维度的归约，通常可以改成分块规约。

1. 只做 normal-equation 累加时，可以把每组项拆成 block reduction。
   `registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` 当前实现是这类形态。它把 27 个项拆成 A/B/C/N 四组，先在寄存器里做局部累加，再用 `vfredosum` 汇总。

2. 当 lane 参数能由 `vid`、常量和向量算术生成时，可以去掉参数 staging。
   `2d/include/pcl/2d/impl/kernel.hpp` 里的 `tmp[MAX_SAFE_VL]` 属于这一类候选。当前实现先用标量推进二维坐标，再装回向量寄存器；如果后续把 lane 公式直接改成向量生成，`tmp` 这一层就可以少掉。

3. 当 buffer 只保存 block reduction 可以直接消费的局部项时，可以撤掉。
   如果下一段只看固定数量的局部和，继续保留行缓冲会增加一层写读。

## 4. 代码里的几种状态

`registration/include/pcl/registration/impl/correspondence_estimation_organized_projection.hpp` 里的 buffer 是典型的压缩后 staging。
它先用 `vcompress` 保留有效 lane，再把 `source_index`、`target_index`、`x/y/z` 分别落到临时数组，最后由短标量循环组装成结构体。
这种写法对应多字段、保序、AoS 结构。

`registration/include/pcl/registration/impl/transformation_estimation_symmetric_point_to_plane_lls.hpp` 仍保留 `a_buf`、`b_buf`、`c_buf`、`d_buf`、`nx_buf`、`ny_buf`、`nz_buf`。
这里的 buffer 负责把压缩后的行项交给标量累加。
它适合保序消费，但也说明这条路径还没改成纯 block reduction。

`filters/include/pcl/filters/impl/bilateral.hpp` 里保留 `weights[256]` 和 `contribs[256]`。
这两个数组的作用是把每个邻居的权重和贡献按 radiusSearch 的原始顺序送到后续标量循环。
代码里还通过 GCC pragma 禁止自动向量化，避免这段顺序累加被改写。

`2d/include/pcl/2d/impl/kernel.hpp` 里同时有两类 buffer。
`std::vector<float> buf` 是连续工作区。
`tmp[MAX_SAFE_VL]`、`tmp_arg[MAX_SAFE_VL]`、`tmp_one_minus[MAX_SAFE_VL]` 是参数 staging。
前者支撑归约和归一化，后者支撑 `expf_RVV` 的 lane 参数装载。

`registration/include/pcl/registration/impl/transformation_estimation_point_to_plane_lls_weighted.hpp` 当前已经不保留旧的行缓冲。
它把工作拆成 A/B/C/N 四组 block reduction，直接把可合并项累到寄存器，再汇总到 `ata` / `atb`。
这类代码适合用作去掉 buffer 的样例。

## 5. 固定 buffer 的门槛

只要临时数组长度写死，就要把 VLEN gate 写在 helper 入口。

- 若数组上界是 64，就先检查 `__riscv_vsetvlmax_e32m2() <= 64`。
- 若数组上界是 128，就先检查对应 `vlmax` 是否落在 128 内。
- 这个 gate 约束的是单个 VL chunk 的最坏保留 lane 数，不是输入总规模。

当 gate 不成立时，优先回退标量路径。若该路径值得继续推进，再考虑动态 scratch、分块写出、降低 LMUL，或者改写成不需要 buffer 的组织方式。

## 6. 复核时要问的几件事

1. 这个 buffer 的后继消费者是谁。
2. 这个 buffer 是否只保存了后续必需的信息。
3. 输出顺序是否必须和标量路径一致。
4. 临时数组是否有明确的 `vlmax` 上界。
5. 这层 buffer 是否只保存 block reduction 可以直接消费的局部项。
6. 这条路径的收益是否已经被板卡结果证明。

## 7. 例子如何补充

这篇文档只记录通用判断。具体函数的细节应留在各自主题文档里。

- `doc-rvv/2d/kernel.zh.md` 说明连续 `buf` 和参数 staging 的用法。
- `doc-rvv/registration/correspondence_estimation_organized_projection-RVV.zh.md` 说明 CEOP 的多字段压缩 staging。
- `doc-rvv/registration/transformation_estimation_dataflows-RVV.zh.md` 说明 registration 数据流里 buffer、tail 和 reduction 的关系。
- `doc-rvv/rvv/RVV Multi-Field Compress Staging.zh.md` 记录了多字段压缩 staging 的单独写法。

后续如果再确认新的 RVV buffer 位置，可以按同一模板补一条：角色、输入、输出、保留原因、可替代方案、gate 和证据位置。
