# RVV 筛选标准

本文只定义“什么样的文件、函数、loop 或 helper 值得进入 RVV 筛选”。阶段职责、队列命名和输出分组见 [stage-and-queue-policy.md](stage-and-queue-policy.md)；证据记录和执行边界见 [evidence-boundaries.md](evidence-boundaries.md)。

## 筛选对象与判定单位

先按函数、loop、helper 或公开入口路径判定，再回填文件级分类。不要只因为文件属于高性能模块、函数名像数学函数、或存在局部循环，就直接写成“建议 RVV 优化”。

每个候选至少定位到：

- 公开入口或主要调用路径。
- 第一批可能 RVV 化的函数、loop 或 helper。
- trip count 来源，例如点数、像素数、correspondence 数、邻域数、bin 数、indices 数或文件块大小。
- 主成本覆盖类型：`direct-main-path`、`partial-preprocess`、`tail-compress`、`diagnostic`、`non-standalone`。
- 为什么需要手写 RVV / RVV intrinsics，而不是仅依赖普通 `-O3` 自动向量化、已有 Eigen/BLAS/SIMD 后端或简单标量清理。

## RVV 候选判定矩阵

用下表做筛选主准则。一个候选不需要每项都是强信号，但不能带着未解释的硬伤进入建议队列。

| 维度 | 强信号 | 降级或暂缓信号 |
| --- | --- | --- |
| 生产价值 | 覆盖公开入口主循环、常用数据路径或已知热点；复杂度随输入规模线性或近线性增长 | 只在初始化、错误路径、小样本路径、debug/log、罕见模板实例或测试辅助中执行 |
| 并行合法性 | 循环迭代大体独立；loop-carried state 可拆成规约、prefix、mask 或局部临时；输出顺序可保持或无关 | 强顺序依赖、早停状态机、随机采样、异常/longjmp、外部可见副作用、跨迭代别名写入无法证明安全 |
| 算术密度 | 点坐标变换、距离/dot/cross、min/max/sum、阈值比较、权重、统计、几何谓词、定长小公式在大循环中重复 | 纯搬运、字段复制、薄 wrapper、虚调用调度、字符串/IO/序列化、容器管理或分配释放主导 |
| RVV 访存匹配 | 连续数组、固定 stride、SoA、AoS 固定字段、organized 行列、可批量处理的 indices gather；segment load/store 或 mask 能自然表达 | 指针追逐、tree/map/hash/heap、随机 scatter、冲突 histogram、cache miss 主导 search、无法界定的自定义布局 |
| 控制流 | 简单 `if` 可转 mask；尾部可用 `vl`/mask/remainder 处理；分支代价低于批量算术收益 | 深分支、递归、多出口循环、复杂 goto、每 lane 调用重函数、分支结果驱动下一轮地址或控制状态 |
| 数值与语义 | 整数/布尔语义可精确保持；浮点误差容忍、NaN/Inf、阈值边界和规约顺序可测试；fallback case 清楚 | 输出顺序、稳定排序、最近邻 tie-break、非结合浮点规约、异常标志、舍入/饱和或 NaN 传播不可改变且难以验证 |
| 证据可构造性 | 有上游 test/bench、可建立 scalar/RVV 对拍、边界 case 明确、QEMU 可确认 RVV 指令、板卡可测 | 无可代表输入、无 oracle、收益只能靠猜、需要大规模生产系统才能观察 |
| 维护成本 | RVV 分支局部、fallback 清晰、模板膨胀可控、可复用已完成主题模式 | 需要重写公共 API、改变数据结构所有权、复制大量复杂标量逻辑、引入不可维护的特化组合 |

## 常见正向信号

- 大批量点云、图像、correspondence、index 或 voxel/bin 循环，单次调用处理元素数随输入规模增长。
- 每个元素执行同构算术或比较，例如 affine transform、距离平方、normal/curvature 局部公式、min/max、sum、dot/cross、阈值 mask、权重和统计。
- AoS 中字段固定且可用 segment load/store 或稳定 offset 访问；SoA、临时数组或 organized 行列访问更优先。
- indices gather 能批量化且每 lane 后续算术足够多，或者 gather 只是进入连续 staging 的一步并可做 component ablation。
- 输出选择可用 mask/compress 表达，且顺序、计数、indices 映射和边界行为可对拍。
- 已有同模块主题证明相似入口、相似数据布局或相似 fallback 形态在目标硬件上成立。

## 常见反向信号

- 文件主要是头声明、显式实例化、类型适配、参数检查、调度 wrapper、构建胶水或测试 fixture。
- 主循环是 kd-tree/octree/search、sort、map/hash、priority queue、RANSAC 随机采样、外部 solver 或 Eigen 分解；除非目标只是明确的 component ablation。
- loop trip count 很小且固定，例如只做一次 2x2/3x3/4x4 矩阵代数；若它被超大外层循环调用，候选应落在外层路径而不是单独小矩阵文件。
- 只优化内存 copy、字段搬运、尾段压缩或阈值后处理，且入口主成本明显在其它阶段。
- 需要改变公开数据结构、点类型 ABI、输出稳定顺序或历史容忍边界才能获得收益。
- 标量路径已由成熟库、编译器自动向量化、平台 SIMD 后端或硬件库覆盖，且没有证据说明 RVV 分支会改善目标硬件结果。

## RVV 特有判断

- 优先选择能写成 vector-length agnostic strip-mining 的 loop；不要假设固定 VLEN、固定 lane 数或固定 tail 大小。
- unit-stride 通常最优；固定 stride、segment load/store、indexed gather/scatter 和 mask 是 RVV 可表达能力，但不是自动收益保证。遇到非连续访存时必须写明 gather/scatter 成本和后续算术是否足够摊薄。
- mask 适合简单条件和阈值；如果 mask 之后只是少量 scalar side effect，通常降级为 diagnostic 或暂缓。
- 规约是重要候选，但浮点规约必须说明顺序、舍入、NaN/Inf、阈值比较和误差容忍；整数 min/max/sum 也要检查溢出和类型扩宽。
- 混合宽度、narrow/widen、定点舍入/饱和和跨字段计算要提前估计 SEW/LMUL、寄存器压力和转换成本。
- intrinsics 候选应能通过 `riscv_vector.h`、feature guard、scalar fallback、QEMU 反汇编和板卡 bench 形成证据闭环；筛选阶段只记录可行性，不承诺接入。

## 外部依据锚点

这些链接用于校准筛选口径；实际筛选结论仍必须来自当前 repo 源码和本项目证据。

- RISC-V Vector Extension specification: https://github.com/riscvarchive/riscv-v-spec/blob/master/v-spec.adoc
- RISC-V Vector Intrinsic Document: https://github.com/riscv-non-isa/riscv-rvv-intrinsic-doc
- LLVM Auto-Vectorization: https://llvm.org/docs/Vectorizers.html
- GCC Tree SSA auto-vectorization: https://gcc.gnu.org/projects/tree-ssa/vectorization.html
- OpenMP SIMD Directives: https://www.openmp.org/spec-html/5.1/openmpsu49.html
- Google Highway: https://github.com/google/highway
- xsimd documentation: https://xsimd.readthedocs.io/
- Maleki et al., An Evaluation of Vectorizing Compilers: https://doi.org/10.1109/PACT.2011.68
