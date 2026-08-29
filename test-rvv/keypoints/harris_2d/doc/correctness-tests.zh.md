# harris_2d Correctness Tests

## gtest 说明

| test | 输入 | 被测路径 | 断言 | 证明范围 |
| --- | --- | --- | --- | --- |
| `ScalarReferenceCoversAllResponseMethods` | 17x13 synthetic organized image | `computeResponsesScalar()` | 四类 method 都生成非零 checksum | scalar reference 覆盖 Harris/Noble/Lowe/Tomasi 公式 |
| `NonFiniteInputPointKeepsZeroResponse` | 含 NaN x 的 11x9 image | scalar reference | 非有限点 response 为 0 | 复刻 production `isXYZFinite` gate |
| `ResponseChecksumUsesToleranceBucket` | 3 个 response intensity 值 | checksum helper | 微小漂移保持同 checksum，可见漂移改变 checksum | checksum 只用于语义桶，不代表 bitwise equality（逐 bit 相等） |
| `CandidateMatchesScalarReference` | 41x29 image + 一个非有限点 | candidate vs scalar | intensity 逐点 near；RVV build 必须命中 RVV path | response map diagnostic correctness |
| `CandidateHandlesTailWidth` | 65x31 image，5x5 configured window | candidate vs scalar | tail case 对拍；RVV build 有 vector chunk | RVV tail 与窗口边界 |
| `PublicComputeMatchesScalarReference` | 39x27 public cloud | 真实 `HarrisKeypoint2D::compute()` | public output 与 scalar reference near；RVV build 返回 RVV path | production direct correctness smoke |
| `PublicComputeDoesNotReusePreviousImageSize` | 同进程先跑 23x19，再跑 65x31 | 真实 public compute + `computeSecondMomentMatrix()` | 第二个尺寸仍与 scalar reference near | 防止 `computeSecondMomentMatrix()` 复用旧 width / height |

## 当前不覆盖什么

这些测试不证明 NMS sort、occupancy map（占用图）、OpenMP critical 输出顺序、真实 `IntensityT` accessor 的泛型点类型字段读取，也不证明 `Scalar=double` 或非 organized input。上述范围保持标量或需要下一 phase 单独验证。
