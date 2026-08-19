# NDT public-entry board repeated summary

- run_label: `public_entry_profile_repeated`
- expected_runs: `3`
- collected_runs: `3`
- evidence_role: `production_public_profile`
- A/B boundary: `public overload cross-build profile`
- timer_boundary: public align; target voxel grid setup is outside timed loop, per-align derivative, line search and solver are included.

| case | runs | median speedup | min | max | B/A < 1 | decision bucket |
| --- | ---: | ---: | ---: | ---: | ---: | --- |
| `production-public-align-pointxyz` | 3 | 1.003x | 0.997x | 1.024x | 1/3 | `neutral` |

## Evidence boundary

This is pre-production diagnostic evidence. It supports only a bounded
production probe discussion after staging, fallback and public-entry costs are audited.
