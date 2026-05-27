# MODULE_NAME 函数级筛查清单（阶段 A）

## 1. 函数/函数族梳理

- 覆盖范围：`MODULE_NAME` 模块下全部候选源码文件（`.h/.hpp/.c/.cc/.cpp/.cu`）。
- 粗筛口径：先做全文件函数登记，再按函数族归并。
- 统计项：
  - 源码文件数：`FILE_COUNT`
  - 识别函数签名约：`SIGNATURE_COUNT`
  - 归并函数族数：`FAMILY_COUNT`

| file | function_or_family | complexity_shape | access_pattern | branch_level | notes |
| --- | --- | --- | --- | --- | --- |
| FILE_PATH | FUNCTION_FAMILY | COMPLEXITY | ACCESS_PATTERN | BRANCH_LEVEL | NOTE |

## 2. 函数级筛选评估标准说明

- 循环规模：优先关注随点数/像素数/邻域大小线性或更高增长的循环。
- 算术密度：优先关注乘加、规约、距离/统计计算密集路径。
- 访存规整性：优先连续或固定步长访存；随机访存标记为风险项。
- 分支复杂度：分支过深或状态机式路径降级为中/低优先。
- 数值语义风险：涉及非结合规约、阈值敏感、迭代收敛的路径标记风险。
- 可测试性：能构建标量对照与回退条件的路径优先。

## 3. 文件级评估汇总表

| file_path | file_priority | 是否候选 | 判断依据 |
| --- | --- | --- | --- |
| FILE_PATH | FILE_PRIORITY | CANDIDATE_FLAG | JUDGEMENT_BASIS |

## 4. 优先级与向量化候选总表

| function_or_family | priority | status | optimization_direction | major_risk | expected_benefit | rollback_condition |
| --- | --- | --- | --- | --- | --- | --- |
| FUNCTION_FAMILY | PRIORITY | STATUS | OPT_DIRECTION | MAJOR_RISK | EXPECTED_BENEFIT | ROLLBACK_CONDITION |
