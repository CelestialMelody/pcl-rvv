# Trajkovic 2D Response Grid Phase Index

| phase | status | 默认恢复入口 | 说明 |
| --- | --- | --- | --- |
| `000-current-state-and-response-grid-production-probe` | complete / adopted narrow scope | `result.zh.md` | `PointXYZI -> PointXYZI`、`window_size == 3`、EIGHT_CORNERS production response grid RVV 已采纳；FOUR_CORNERS 和未覆盖范围保持标量 fallback。 |

当前默认恢复动作：本 topic 当前冻结范围已完成，恢复时先读 `result.zh.md`、`optimization-matrix.zh.md`、
`doc/optimization-roadmap.zh.md` 和 `../../../doc-rvv/keypoints/trajkovic_2d_response_grid-RVV.zh.md`。
若后续要扩大点类型、accessor、window 或 NMS 范围，应新建独立 phase，并重新闭合 correctness
（正确性）、asm attribution（反汇编归属）、board repeated（板卡重复性能测试）和 Evidence Doctor
（证据体检）。
