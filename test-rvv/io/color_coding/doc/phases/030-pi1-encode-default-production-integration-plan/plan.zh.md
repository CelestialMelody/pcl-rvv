# Phase 030 Plan: PI1 encode/default production integration plan

## 阶段意图和边界

本阶段是 PI1 production integration plan（生产接入计划），不是 production patch（生产补丁）。它冻结一个窄范围候选，等待用户确认后才能进入 PI2 修改 `io/include/pcl/compression/color_coding.h`。

候选范围只包含：

- `encodeAverageOfPoints` 的 indexed leaf RGBA gather + vector reduction。
- `encodePoints` 的第一遍 average gather；differential byte stream 仍保留标量 push。
- `setDefaultColor` 的 contiguous RGBA fill。

显式排除：

- `decodePoints`。phase 020 的 `ps_decode_points_leaf4096` 触发 Evidence Doctor Error。
- full `OctreePointCloudCompression` public entry（公开压缩入口）和 entropy coder（熵编码器）。
- 泛型 `PointT` clean adoption。第一步只允许 `pcl::PointXYZRGBA` / RGBA-layout gate 或等价有证据的窄范围。

## 生产补丁前 Gate

| gate | required decision |
| --- | --- |
| public API | 不新增 / 不改变公开 API |
| fallback | 非 `__RVV10__`、非覆盖点类型、RGBA offset 不安全、规模太小或 32-bit indexed byte offset 溢出时走原标量 |
| point type | phase-local exact / traits gate 只覆盖已验证 RGBA layout；不能写成完整泛型 |
| size | encode points large case 有 1/5 退化 warning，PI2 需要保守规模 gate 或单独 bench |
| decode | 保持标量 |
| evidence | PI2 后必须补 production direct correctness、fallback test、asm attribution、board repeated 和 Evidence Doctor |
| user checkpoint | 没有用户确认前不进入 PI2 |

## 拟议实现形态

1. 把原标量主体拆为 `encodeAverageOfPointsStd` / `encodePointsStd` / `setDefaultColorStd` 或等价私有 helper，保持当前格式风格。
2. 在 `__RVV10__` 下新增窄范围 `*_RVV` helper，使用 current `PointT` 的 RGBA offset 和 `sizeof(PointT)` 构造 32-bit byte offsets。
3. `encodePoints` 的 RVV helper 只替代平均颜色求和；diff byte push 仍按当前标量语义执行。
4. `setDefaultColor` 只在 contiguous output range 且点类型 / offset gate 成立时用 RVV strided store；否则 fallback。
5. `decodePoints` 不接入 RVV。

## 需要新增或重跑的证据

| evidence | target / path | completion |
| --- | --- | --- |
| production direct correctness | new tests under `test-rvv/io/color_coding` or production-shaped public helper tests | Std/RVV byte stream and fallback path match |
| fallback tests | non-RVV build, unsupported point type / layout, small size gate | each gate isolated |
| asm | `dump_bench_rvv` or production direct binary dump | expected RVV instructions attributed to production helper if patch is applied |
| board repeated | board target with production-direct labels | positive / weak / neutral split by helper |
| Evidence Doctor | `log/board/.../evidence_doctor.md` | Errors=0 required for production candidate |

## 继续 / 停止条件

Stop now unless the user explicitly confirms production patch work. If confirmed, enter PI2 with the scope above and keep decode scalar. If not confirmed, preserve the diagnostic assets and handoff as partial-production-candidate evidence.
