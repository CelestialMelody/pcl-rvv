# 诊断入口形态

## 类式算法

如果目标函数依赖公开 setter、基类准备流程、缓存、indices/mask/input 生命周期或输出顺序，优先在专项测试中建立 test-only 派生诊断类。

默认优先级：

1. 继承目标上游类，做 test-only 子类。证据强度最高，适合复用公开调用面和 protected 状态。
2. 继承共同父类，做 test-only 兄弟类。适用于目标类封装阻止子类表达诊断入口。必须记录与目标类的差异。
3. 组合 / wrapper。适合轻量对拍或无法继承的对象。证据强度较低，需要说明未覆盖的对象状态。

full correctness 和 full bench 默认调用最接近上游公开 API 的入口层。低层 helper 的通过不能替代入口层证据。

## 入口面覆盖清单

类式诊断入口应尽量覆盖真实生产调用面。按目标入口能力检查：

- 公开 setter：`setInput*()`、`setTarget*()`、`setIndices()`、`setSearchMethod()`、阈值、半径、字段名、negative、keep organized 等。
- 默认参数：用户未显式设置 indices、mask、target、search object、字段或阈值时的默认行为。
- 准备流程：`initCompute()`、`deinitCompute()`、缓存刷新、field metadata 构建、search object 初始化、fake indices 展开。
- 生命周期：input、target、indices、removed indices、mask、缓存、临时 buffer 和输出容器何时创建、复用或清理。
- 输出语义：append 顺序、重复 index、tie 选择、organized 输出、removed indices、对象可见状态和副作用。
- warning / error 行为：上游入口在空输入、非法参数、字段缺失或 search 失败时是否 warning、return false、抛异常或保留旧状态。

full diagnostic 如果无法覆盖其中某项，文档必须标为未覆盖生产面，不得把该入口写成 production-shaped evidence。

## 继承形态差异清单

使用共同父类兄弟类、组合或 wrapper 时，文档至少列出与目标上游类的差异：

- 公开 setter 和默认值是否完全复刻。
- private/protected 成员是否缺失，是否用可见状态替代。
- 缓存、懒加载、fake indices、field metadata 或 search object 生命周期是否相同。
- warning、error、early return 和异常路径是否相同。
- 输出顺序、副作用、removed indices、状态缓存更新是否相同。
- 是否复制了目标入口主体；复制段是否需要随上游源码同步。

差异越多，证据强度越低。兄弟类复刻不完整时，应标为 production-like prototype，而不是 production-shaped diagnostic。

## 非类式函数

free function 诊断入口应保持同名或同形参数。可以增加 `DiagMode`、命名空间或 test-only wrapper，但入口、参数、输出和副作用要能与上游源码对应。

公共数学 helper 或局部内核可以建立 local fragment bench。文档必须标明它不是生产入口收益，除非后续补齐 entrance/full evidence。

## 命名建议

- 入口层名称表达上游目标入口。
- 低层 helper 名称表达替换原函数哪一段。
- 标量 tail helper 名称表达从 staging 回到哪段原标量语义。

避免使用 `stage1`、`stage2` 这类只描述顺序、不描述领域含义的名称。
