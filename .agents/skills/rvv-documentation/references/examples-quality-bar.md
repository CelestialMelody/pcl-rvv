# 样例质量门槛

本 reference 从当前 PCL 项目中质量较满意的文档抽取标准。其它库迁移时可替换样例路径，但质量要求保留。

## `grid_minimum` 型：bench 诊断与不接生产

质量点：

- 开头直接说明公开入口、算法职责、当前只做 bench 诊断、不改生产入口。
- 标量路径写到公式和后续 sort/min-z 主成本，避免只写函数名。
- 明确 local cell-id 片段、full diagnostic、production unchanged case 的区别。
- 把局部 speedup 与 full diagnostic speedup 分开，说明局部收益不能直接支撑生产接入。
- 给特殊实体表：staging、数值 helper、mask helper、full diagnostic helper。
- FRM/FCSR 处理写清：使用无 FRM 副作用的转换方案，未修改调用者浮点环境。
- 关键片段覆盖 gather、floor、mask、`vcompress`、staging 写回和标量 tail 衔接。
- 结论写“不接生产”的证据，避免只写“后续优化”。

适用场景：局部片段正确且较快，但 full 入口被 sort/search/map/状态机等主成本稀释。

必检字段：

- 当前状态：是否只做 `bench-only / diagnostic` 证据路径、是否修改生产入口。
- 公开入口：用户如何进入目标函数，输入/输出和算法职责是什么。
- 标量主链路：局部公式之后还有哪些 sort/search/map/状态机或输出阶段。
- 证据分层：local fragment、full diagnostic、production case 分别对应哪些 case。
- 特殊实体：staging、数值 helper、mask helper、full diagnostic helper 的输入、输出和原标量语义。
- 数值边界：floor/RTZ、FRM/FCSR、NaN/Inf、indices 和保序压缩。
- 结论：为什么局部收益不能支撑生产接入，重新评估需要什么 full evidence。

## `norms` 型：函数族与容差口径

质量点：

- 开头概括函数族、公开 API 不变、连续 `float` gate、小规模 fallback、公共 math helper。
- 与上游差异用表格列出每类函数族。
- 数值一致性单独说明：规约顺序、`logf` 逼近、容差来源和不做 bit-level 等价的原因。
- 详细实现按函数族解释，不把所有 helper 混成一段。
- 测试说明区分功能单测、同进程对拍、两套 binary bench 和板卡日志。
- 性能表只摘关键维度，并解释个别退化或低收益 case 不外推。

适用场景：一个文件内有多个相似函数族、规约、近似 math helper 或容差策略。

必检字段：

- 函数族总览：哪些公开 API 或 helper 被覆盖，哪些保持标量。
- Gate：连续 `float`、维度阈值、非连续容器、非 `float` 或小规模 fallback。
- 数值口径：规约顺序、近似 math helper、容差公式和不能做 bit-level 等价的原因。
- 共享 helper：公共 math helper 或公共实现文件的依赖和同步要求。
- 测试层级：功能单测、同进程对拍、两套 binary 或双构建 bench、目标硬件日志。
- 性能解释：低收益或退化 case 是否只作为当前数据点，不外推成全局结论。

## `transforms` 型：生产路径与多平台证据

质量点：

- 明确公开入口、覆盖范围和保持原路径的范围。
- 与上游差异表说明 dense、non-indexed、float、layout、normal、in-place 等边界。
- 数据访问组织解释 AoS stride、公共 load/store 封装和 in-place 安全。
- 详细说明第一版退化原因和 fused 条带为何改善。
- 测试章节包含专项测试、上游原始测试、QEMU 口径、板卡 bench、x86 SIMD 对照、反汇编。
- x86 只做同平台对照，不与 RISC-V 目标硬件绝对耗时比较。

适用场景：生产路径已接入，需要证明公开 API 未破坏，并有上游测试和目标硬件收益。

必检字段：

- 生产覆盖范围：dense、non-indexed、`float`、字段 layout、normal、in-place、小规模 gate。
- 保持原路径：double、non-dense、indices、特殊入口或单点 helper。
- 访存组织：AoS stride、公共 load/store 封装、segment/field 选择和 in-place load-before-store。
- 设计迭代：若曾退化，说明退化原因、修正后的 fused 条带或新组织为什么成立。
- 证据矩阵：专项测试、上游原始测试、QEMU、反汇编、目标硬件、x86 SIMD 同平台对照。
- 平台边界：x86 不与 RISC-V 目标硬件绝对耗时比较，QEMU 不写成性能结论。

## `gaussian` 型：数值算例与双库边界

质量点：

- 开头说明实现位于共享库，bench 可执行宏不能改变已编译库内路径。
- 分流条件写清：是否由 TU 编译选项决定，是否有运行期回退。
- 行/列卷积分别给出标量公式、RVV 组织和关键片段。
- 数值算例非常具体：输入布局、核下标、VL chunk、每个 lane 如何对应标量输出。
- 测试章节区分单测、本地 QEMU、目标硬件双库对比、无 PCL 链接的算法对拍。
- 性能数据来源说明清楚；若日志采集条件不一致，不把它写成结论。

适用场景：热点在共享库内、需要双库或双构建对拍、数值流程适合手算展示。

必检字段：

- 路径决定点：实际热点在共享库、对象文件还是 header TU；bench 可执行宏是否影响该路径。
- 双库/双构建：Std/RVV 库如何构建、部署、切换和配对日志。
- 分流条件：编译期和运行期是否有 gate，未覆盖函数为何保持标量。
- 行/列或多方向算法：每个方向的标量公式、RVV 访存组织、关键片段和边界写回。
- 数值算例：输入布局、核/参数、VL chunk、lane 对应关系和 tail/边界处理。
- 证据区分：单测、本地 QEMU、目标硬件双库 bench、无库算法对拍各自证明什么。
- 日志采信：日志数据集、iterations 或构建条件不一致时，不把该表写成性能结论。

## 通用质量检查

文档完成前逐项自查：

- 是否说明入口职责，而不只是函数名。
- 是否摘录或概括真实标量路径。
- 是否区分 local fragment、full diagnostic、production case。
- 是否列出 gate 和 fallback。
- 是否解释关键 helper、traits、mask、staging、metadata 或特殊实体。
- 是否有可手算算例或说明为什么不适合。
- 是否说明每个 bench case 的证明点。
- 是否把 QEMU 和目标硬件结论分开。
- 是否记录未接生产、生产回退或弱收益接入的理由。
- 是否避免个人路径、私有地址、设备内部绝对路径和对话归因。
