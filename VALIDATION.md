# Current-goal completion audit

Date: 2026-09-06. Scope: SOOTHE_CLONE_GOAL.md sections 36–37, Windows x64 Debug.

Final command: `& VSTProject/SootheClone/Build.ps1` from workspace root. Exit code 0; six CTest suites passed (51.64 s). Detailed stdout is preserved in Reports/DebugTests.log.

| Requirement | Evidence |
|---|---|
| Project compiles | Final CMake/MSBuild Debug build completed successfully |
| VST3 target builds | SootheClone_VST3 produced bundle and moduleinfo.json; actual binary loaded by SootheVST3 |
| STFT reconstruction | Deterministic noise including initial padding and flushed tail; maximum absolute error 2.98023e-7 |
| Arbitrary host blocks | Core sizes 1/17/64/127/511/1024/2049; VST3 mono/stereo neutral tests and active changing 0/1/17/127/511/2049/64 sizes; active partition null exactly 0 |
| Relative Soft prominence | Soft/Engine compare -6/-12/-24/-36 dB; broadband-plus-tone scale null 4.47035e-8 |
| Monotonic Depth | Final Depth 0/2.5/5/7.5/10 gives approximately 0/5.14/10.27/15.41/20.55 dB at 1 kHz |
| Monotonic Detail resolution | Three-tone half-height width decreases 270/38/17/8/6 bins at Detail 0/2.5/5/7.5/10; central reduction increases 3.34/6.67/10.01/13.35/16.68 dB |
| Multiple resonances | 500/1000/2000 Hz peaks detected; valley/peak approximately 0.879 at Detail 0, 0.000804 at Detail 5, 0 at Detail 10; no hard-coded peak count in DSP |
| Delta is removed content | Engine dry-wet-delta max error 2.32831e-10; actual VST3 null 1.19209e-8; steady Delta 1 kHz projection accounts for >99.9% of energy |
| Stable silence | Core/Engine; changing parameters/Delta/Bypass on silence emits exact zero |
| No NaN/Inf | Finite output checks across seven frequencies, four levels and 44.1/48/96 kHz; NaN input sanitized; callback ScopedNoDenormals and envelope tiny-value cutoff |
| No obvious discontinuity | Startup/tail reconstruction; actual VST3 block partition equality; Gain/Bypass/Delta switch max step on 0.2 DC is 0.000827625 |
| No callback allocation | Debug CRT hooks around Engine and actual VST3 process calls report 0, including first calls and changing blocks; fixed-array processing, no callback file/GUI/log/lock operations |
| Unrelated functionality preserved | New sibling project; VocalChop source timestamps remain August 2026, no edits to its source/configuration; baseline Debug VST3/Standalone build succeeded |

Additional gates: VST3 discovery reports an effect, mono/stereo layouts accepted, latency is 4096, APVTS state restored, editor created/destroyed. Final Soft frequency trend at all three sample rates satisfies 100 Hz < 1 kHz < 10 kHz reduction.

Detail width counts contiguous bins above half of the 1 kHz peak. At low Detail this includes merged neighboring regions. It is a behavior test, not a proprietary bandwidth definition.

## Built artifacts

Build root: `C:/Users/yuito/.codex/builds/SootheClone`.

- `SootheClone_artefacts/Debug/VST3/SootheClone.vst3/Contents/x86_64-win/SootheClone.vst3`
  - SHA256: `85F6E7C2AA640650F88BE4428C4BA87ED8112A0113BE06387184F230B2C4882F`
- `SootheClone_artefacts/Debug/Standalone/SootheClone.exe`
  - SHA256: `0D9617E84F53D88B1FBF37D17260ED029DC9700997E44C3F2B16DA6A1F4F6097`

## Evidence limits

Tests establish implemented core behavior, not byte-identical Soothe3 behavior, actual Ableton Live interaction, perceived musical quality or hardware dropout limits. Standalone was built; editor and callbacks were exercised through the VST3 host rather than a live device. No Hard/bands/Focus/advanced modes were added. README lists manual listening steps and tunable hypotheses for the next A/B stage.
