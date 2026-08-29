# Phase 020 Result: accumulator-scatter-audit

## 当前状态

这一阶段完成了 `use_interpolation=false` 的 board ablation（板卡消融测试），用于判断
`voteInt()` 的 27 邻域插值是否吞掉了 vote generation RVV 的局部收益。

## 结论

`use_interpolation=false` 时，full `houghVoting()` 3-run median 为 `1.002x`，
values 为 `1.000x, 1.049x, 1.002x`，decision bucket 仍是 `neutral`。
Evidence Doctor 报 `0 Error / 1 Warning / 1 Suggestion`：run count 只有 3，
且 median 距离 `1.0` 阈值太近。

这说明 `voteInt()` 插值是明显成本来源：默认插值开启时 full 入口约 `617 ms`，
关闭插值后约 `169 ms`。但它没有形成稳定 production 收益，不能推翻 phase 010
“当前 production patch 不采纳”的结论。

## 已登记证据

- `test-rvv/recognition/hough_3d/log/board/repeated_phase020_no_interpolation/summary.md`
- `test-rvv/recognition/hough_3d/log/board/repeated_phase020_no_interpolation/evidence_manifest.json`
- `test-rvv/recognition/hough_3d/log/board/repeated_phase020_no_interpolation/evidence_doctor.md`
- `test-rvv/recognition/hough_3d/log/board/repeated_phase020_no_interpolation/evidence_doctor.json`

## 下一步

本轮命中停止条件：默认 production direct 已经是 neutral / 退化，no-interpolation 只给出
接近阈值的弱信号；继续把 `voteInt()` / voter tracking 做 RVV 会进入复杂 production
状态写回，不适合作为当前 attempted patch 的直接采纳延伸。
