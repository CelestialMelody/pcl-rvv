# 正确性测试

本文件说明每个测试验证什么、为什么需要、失败意味着哪条证据断了。

| 测试 | 验证内容 | 不能证明什么 |
| --- | --- | --- |
| `CandidateMatchesScalarForFiniteAndInvalidPoints` | RVV candidate 和标量参考在 finite / invalid 点混合输入上一致 | 不能证明 production 已接入 |
| `ReferenceMatchesPublicPathWhenShadowChecksAreDisabled` | 公开入口形态和标量参考同序输出一致 | 不能证明 RVV candidate 必定更快 |
| `CandidatePreservesAdaptiveCutOrder` | adaptive cut 的对角线选择顺序保持不变 | 不能证明 shadowed face 语义已覆盖 |
