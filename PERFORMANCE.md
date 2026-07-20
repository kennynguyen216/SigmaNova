# SigmaNova Performance

## Optimization Summary

SigmaNova's fullscreen volumetric renderer evaluates camera rays, procedural density, emission, absorption, transmittance, and layered noise for every covered pixel. The initial optimization pass reduced the red-supergiant frame time from approximately `24.0 ms` to `6.84 ms`, a `3.5x` improvement.

The main techniques were:

- early density rejection outside the possible gas radius
- tighter analytic march bounds
- adaptive step sizes in low-detail outer gas
- normalized FBM so different octave counts retain comparable output ranges
- tiered noise quality for primary, secondary, and silhouette detail
- transmittance-based early ray termination

A later renderer pass added delayed GPU timestamp queries, a dynamically scaled HDR volume target, half-resolution bloom, phase-specialized material paths, and a batched HUD.

## Final Remnant Benchmark - 2026-07-20

This benchmark records the approved Version 1 remnant after adding larger analytic ejecta bounds, persistent low-frequency lobes, a compact pulsar, and distance-faded pulsar illumination.

### Test Conditions

- Build: MSVC Release
- Window: `800x600`
- Camera and event timeline: identical automated benchmark sequence
- VSync: disabled by `--benchmark`
- Bloom scale: `50%`
- Values: phase averages from delayed GPU timestamp queries

Commands:

```powershell
cmake --build --preset windows-msvc-release
build/windows-msvc/Release/sigma_nova.exe --benchmark
build/windows-msvc/Release/sigma_nova.exe --benchmark --full-resolution
```

### Results

| Phase | GPU total at 75% | GPU total at 100% | 75% reduction | Approx. GPU FPS at 75% |
|---|---:|---:|---:|---:|
| Idle star | 5.131 ms | 8.755 ms | 41.4% | 194.9 |
| Pre-flash | 3.787 ms | 4.728 ms | 19.9% | 264.1 |
| Flash and reveal | 9.005 ms | 12.602 ms | 28.5% | 111.1 |
| Settled remnant | 25.428 ms | 44.881 ms | 43.3% | 39.3 |

The settled remnant is the bottleneck. At the default `75%` volume scale, `24.612 ms` of its `25.428 ms` total GPU time is spent in the volume pass. Bloom remains inexpensive at `0.278 ms`.

The approved remnant is more expensive than the earlier compact shell because its analytic march bound and directional lobes cover substantially more screen area and retain density farther from the center. The visual gain is deliberate: the remnant no longer reads as a circular procedural cloud.

The final `75%` result remains under the project's `30 ms` GPU threshold and is `43.3%` faster than the matched full-resolution remnant. Version 1 accepts this tradeoff. Future optimization should reduce empty raymarch coverage or replace repeated procedural noise with texture-backed density rather than weakening the approved art direction.
