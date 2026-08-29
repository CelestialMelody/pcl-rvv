# Benchmark and Evidence

## bench 边界

bench 先测 vote generation helper，再补 production direct repeated board。

## 证据边界

- QEMU：只看构建和日志形状。
- board：`log/board/repeated_phase000_vote_generation_diagnostic/summary.md` 和
  `log/board/repeated_phase010_production_direct/summary.md`，以及
  `log/board/repeated_phase020_no_interpolation/summary.md`、
  `log/board/repeated_phase030_default_distance_weight/summary.md`。
- Evidence Doctor：`log/board/repeated_phase000_vote_generation_diagnostic/evidence_doctor.md`
  和 `log/board/repeated_phase010_production_direct/evidence_doctor.md`，
  `log/board/repeated_phase020_no_interpolation/evidence_doctor.md`、
  `log/board/repeated_phase030_default_distance_weight/evidence_doctor.md`。

## 当前判断

- phase 000：vote generation diagnostic 5/5 正向，median `1.076x`。
- phase 010：production direct 5/5 都低于 `1.0`，median `0.995x`，Evidence Doctor
  报 `1 Error`。
- phase 020：`use_interpolation=false` 的消融 3-run median `1.002x`，Evidence Doctor
  报 `1 Warning / 1 Suggestion`，只能说明 `voteInt()` 插值成本很高，不能支撑采纳。
- phase 030：默认 `use_distance_weight=false` 的 production direct 5-run median
  `1.005x`，`1/5` 低于 `1.0`，Evidence Doctor 报 `1 Warning / 1 Suggestion`，
  仍不能支撑采纳。
