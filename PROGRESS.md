# SootheClone progress

Current scope from ../../SOOTHE_CLONE_GOAL.md sections 36–37 is implemented and verified. Stop here; Hard, bands, Focus and advanced modes are not part of this milestone.

## Completed phases

0. Inspected VocalChop Projucer/VisualStudio2026 architecture, custom state management and sampler callback. No reusable STFT existed. Baseline Debug VST3 and Standalone built successfully. Existing VocalChop source files remain untouched.
1. Implemented fixed-array radix-2 FFT and streaming 4096/1024 periodic Hann STFT. Neutral reconstruction passed before adding suppression.
2. Added relative local-baseline Soft detection, configurable frequency sensitivity, per-bin Depth and attack/release. Single-sine level and Depth tests passed before adding Detail.
3. Added log-frequency Detail. Initial normalized averaging diluted high-frequency peaks; measurements led to a peak-preserving Gaussian envelope with bounded O(N) cost. Final frequency trend and Detail tests pass.
4. Added aligned dry, Delta, Mix, output smoothing, JUCE APVTS, mono/stereo effect I/O, generic editor, state serialization and latency-aware bypass. VST3 and Standalone Debug builds pass.

## Verification

Final Build.ps1 invocation completed with exit code 0. All six CTest suites passed in 51.64 seconds. The actual VST3 loaded in the JUCE host; active output was identical across fixed and changing blocks (including zero-length blocks). CRT audio-path allocation count was zero in DSP and VST3 tests.

See README.md for architecture, usage, manual test steps and tuning constants. See VALIDATION.md and Reports/DebugTests.log for the requirement audit and measurements.

## Build environment

JUCE 9.0.1 at C:/JUCE; Visual Studio 2026 Community, MSVC 19.51, Windows x64. JUCE helper commands failed under the original Japanese build path. Build.ps1 uses the ASCII path %USERPROFILE%/.codex/builds/SootheClone and preserves the source location. It scopes away an inherited CMAKE_GENERATOR_PLATFORM warning during its invocation.

Ableton Live listening, hardware dropout measurements, Release validation and direct Soothe3 A/B matching remain outside the automated evidence. No claims of proprietary algorithm equivalence are made.

## UI follow-up — 2026-09-07

User requested a UI close to the supplied image. Added PluginEditor.h/.cpp and EditorTests.cpp; updated PluginProcessor and CMake. Blue sidebar/large Depth knob, frequency graph, lower controls, live input spectrum and reduction, Reset, resizing, APVTS bindings. Atomic single-frame mailbox skips updates rather than blocking the callback. Existing six regressions passed (48.81 s); final UI/VST3 tests passed (4.41 s), including open-editor processing with zero callback allocations. Standard and small snapshots visually checked: Reports/UI.png and UI-small.png. Build output remains the same Debug VST3/Standalone paths. README documents the new UI. Previous binary hashes in VALIDATION.md describe the pre-UI milestone, not this build.

## Frequency weighting / EQ follow-up — 2026-09-07

User explicitly selected a curve that controls resonance-suppression strength. Added BandWeighting.h (five configurable bands, Bell/shelves/low/high processing cutoffs), applied cached per-bin weights after Detail and before the existing reduction envelope. Added APVTS automation/state fields without renaming old IDs. Missing band fields in old session state are restored to neutral defaults.

Editor now supports point drag, scroll width/slope, selected-band numeric controls, shape selection, enable, right-click toggle and double-click reset. White and blue curves share the dB scale. Default bands leave the old sound unchanged. Added BandTests.cpp and expanded EditorTests/VST3Smoke for drag, wheel, state and legacy restoration.

Existing VST3 was locked in use. New output root is C:/Users/yuito/.codex/builds/SootheClone-EQ; the loaded old bundle was not replaced. README documents rebuild and manual tests. Screenshots: Reports/EQ-UI.png and EQ-UI-small.png. Build evidence and tests are in Reports/EQTests.log after the final test run.

Final EQ verification: Debug VST3/Standalone build succeeded; all 8 CTest suites passed (89.32 s). Selected 1 kHz amplitude: neutral 0.0477214, stronger 0.00237775, weaker 0.112369. Weighted Delta null 6.52052e-9; band automation max sample step 0.00952955; callback allocation count 0. VST3 active block partition null remains 0. Actual binary SHA256: 53E269D45504D4FD1DC4FFF86D2AD29DC899694B10677D32D62EE9BEE5FA7A43. Visual review passed at 1100x680 and 880x544. Direct Soothe3 matching and live Ableton listening are not claimed.

## Hard follow-up — 2026-09-07

Implemented the user's request for Hard. Added Source/HardDetector.h and Tests/HardTests.cpp, updated Engine, Processor, Editor, CMake and state/UI tests. FFT magnitudes are Hann-normalized to dBFS; the seven supplied sensitivity points are log-interpolated. Tunable initial threshold -5 dB, ratio 2:1, knee 6 dB. Soft remains default; new Mode parameter appended and pre-Hard states restore Soft. Detail/weighting/Depth/AR and Delta remain shared; switching keeps the envelope and STFT state.

Debug VST3 and Standalone build passed in C:/Users/yuito/.codex/builds/SootheClone-EQ. All nine suites passed (98.54 seconds); after a UI-only radio-button notification guard, final VST3/Editor tests also passed (6.17 seconds). Evidence: Reports/HardTests.log and HardUITests.log. Screenshots checked at 1100x680 and 880x544; Hard-UI.png and Hard-UI-small.png.

At 48 kHz / 1 kHz, inputs -6/-12/-24/-36 dB yield reductions 9.45895/5.87781/0.0357412/~0 dB. At the supplied reference levels the 44.1/48/96 kHz results are 2.1-3.0 dB. Hard/Soft switching Delta null 6.52069e-9, maximum sample step 0.00894198, audio-path allocations 0. Additional tests cover transfer knee/ratio, sensitivity points/interpolation, Depth/Detail monotonicity, band weighting, silence, actual VST3 Hard level response, mode persistence and pre-Hard state migration.

README includes manual tests and tuning limitations. This remains an effective initial model, not a claim of Soothe3 internal implementation or exact matching. No Focus/M-S/quality features added.

## Stereo / M-S / Focus follow-up — 2026-09-07

Implemented user-authorized Phase 7. STFT push/synthesise split allows synchronous stereo analysis. StereoEngine keeps L/R and M/S detectors warm, links per-bin reduction towards the stronger component, applies per-band Focus, and crossfades decoded L/R spectra on mode changes (30 ms time constant). M=(L+R)/2, S=(L-R)/2. Mono uses the existing Engine and ignores stereo controls. No audio-thread allocation, locks, file IO or logging added.

Added Stereo mode, Link and five Focus parameters after all previous parameters. Missing fields restore L/R / 0% link / centred focus. Editor has domain selection, percentage Link and per-band Focus with domain labels; mono disables these controls. README explains model choices, visualisation semantics, added CPU work and manual listening tests. Quality/Low Latency/Linear Phase remain pending.

Full ten-suite CTest passed (53.97 seconds). Stereo tests: exact legacy L/R agreement; neutral reconstruction max error 1.2666e-7; switching Delta null 2.98023e-8; linked amplitude ratio error 7.45058e-9; opposite Focus settings produce +/-6.79831 dB component balance on the test signal in both domains; observed audio allocations zero. Expanded real-VST3/editor tests cover M/S variable block sizes, Delta, parameter round-trip, legacy migration, automation, mono UI and snapshots. Final build and host evidence: Reports/StereoFinalBuild.log and StereoHostTests.log. Reports/StereoBuild.log records the full regression pass before final UI polish and expanded host tests.
Final VST3/Editor verification passed in 7.92 seconds after UI polish. Final VST3 binary SHA256: 353E04F4320F853935C56B685A6C23545D3513F55E987819CA551102A37C46F9. Standard/small snapshots: Reports/Stereo-UI.png and Stereo-UI-small.png; visual check passed.

## CPU follow-up — 2026-09-07

User reported approximately 60% CPU. Previous deliverable was an unoptimised Debug build. Added repeatable DSP-only benchmark (SoothePerformance; 48 kHz, 256 samples, 10 seconds per mode, FTZ/DAZ matching the plugin). Before Debug averaged 43.53–102.68% of audio real time; the same source in Release averaged 2.79–3.12%. These are local DSP measurements, not the user's DAW meter.

Skipped unused Soft baseline/prominence analysis while Hard is selected, retaining the identical magnitude calculation and gain pipeline in both mono and stereo. Soft diagnostic baseline/raw arrays are only refreshed in Soft; displayed magnitudes/reduction remain current. Kept both L/R and M/S chains warm to preserve switching behaviour. Build.ps1 now defaults to Release; Debug remains explicitly available for development. Updated README paths and install/manual performance comparison instructions.

Final Release averages: Soft LR 2.80982%, Soft MS 2.86661%, Hard LR 2.83322%, Hard MS 2.89723%; p99 block 716.6–841.9 us. Hard improves a further 6–7% relative to the pre-change Release. All ten Release tests passed in 6.13 s, including real VST3 and editor tests. Logs: CPU-Release-BuildTests.log, CPU-Release-TestDetails.log, Performance-Before-Debug.txt, Performance-Before-Release.txt, Performance-After-Release.txt. VST3 and Standalone available under the existing EQ build root's Release directory. Reports/SootheClone-Release-CPU-Fix.zip contains the complete VST3 bundle. SHA256: 9936C00BE5854FBEF3CBA990B24C5E83DEBE764D7A433B036681AF13AD003EB1. No DAW shutdown or installed-bundle replacement performed.
Final Debug VST3/Standalone build and all ten tests also passed (69.13 s). Debug CRT allocation hooks remained active; performance distribution is still the Release bundle, not Debug.

## Quality follow-up — 2026-09-08

User approved moving on after the CPU fix. Began Phase 8 with Normal/High Quality, preserving 4096-point FFT, 4096-sample latency and all detector sensitivity parameters. Normal hop=1024, High hop=512. STFT changes hop at frame boundaries and tracks overlap weights to normalize mixed-hop synthesis. Reset initializes missing prehistory weights to prevent startup gain lift. Envelope and M/S mode blend use the actual frame interval, preserving time constants. No added audio-thread allocations/locks.

Quality parameter appended after existing IDs. Default and legacy restore are Normal. Lower UI quality selector uses APVTS attachment and warns that High costs more CPU. Added QualityTests (44.1/48/96 kHz, steady Soft/Hard reduction, rapidly changing Quality, simultaneous stereo/mode/focus changes, unity reconstruction, Delta, frame density and time constants); expanded Editor/VST3 state/legacy and host tests. Extended performance tool to compare qualities. README documents exact model, CPU and manual tests. This is a time-resolution quality setting, not an FFT-resolution or latency change; Low Latency and Linear Phase remain pending.

Release VST3/Standalone and all eleven tests passed (8.47 s). Neutral switching error 1.19209e-7; Delta error 7.45058e-9; steady Normal/High reduction difference at test frequencies at most 0.00684893 dB. Performance (DSP only, 48k/256): Normal 3.16039–3.21823%, High 6.1132–7.72066%; host CPU can differ. Actual GUI snapshots reviewed at 1100x680 and 880x544. Logs/snapshots: Reports/Quality-Release.log, Quality-Release-Details.log, Quality-Performance.txt, Quality-UI.png, Quality-UI-small.png. Packaged Reports/SootheClone-Release-Quality.zip, complete VST3 bundle. Release binary SHA256: 669A13AFFE7939775A9C189F5BAA8812337E0DA5A604C34158379139B1CC5208. No installed bundle replaced and no DAW listening claimed.
Final Debug VST3/Standalone and all eleven suites passed (106.15 s). Quality Debug CRT allocation hook recorded zero audio-path allocations. Details: Reports/Quality-Debug.log and Quality-Debug-Details.log. Release package is the user-facing deliverable.

## Low Latency follow-up — 2026-09-08

Implemented 2048-sample output latency with 4096-sample FFT analysis retained. A short-FFT prototype changed steady Soft reduction by up to 2.77699 dB, so it was replaced with the last 2048 samples of the 4096-point inverse FFT and a shorter Hann synthesis window. Normalize by analysis/synthesis-window overlap. Standard path remains full-window synthesis. Engines are specialized by synthesis latency; only the selected pair runs. Detector parameters, analysis binning, hop/Quality settings, bands/Focus and reduction curves remain the same. Separate dry delays preserve Delta/Mix/Bypass. Maximum steady output difference in the tested Normal-quality signals is 0.369509 dB; both qualities are tested against a 0.5 dB bound and exact detector/reduction-curve agreement.

Low latency parameter appended, default OFF, non-automatable. Parameter changes remain pending until prepareToPlay, where engine choice and reported latency are set together; reset does not activate pending latency. UI shows actual delay in milliseconds and 'reload to apply' while pending. Save/reload or restart audio engine is required, not necessarily a transport stop/start. Legacy state missing the parameter restores OFF. Reported tail bounds now include analysis history plus synthesis latency, with an actual VST3 impulse-drain check.

Release VST3/Standalone and all twelve tests passed (9.30 s). Final host tests after tail-length correction passed (3.69 s), then the additional tail-drain host test passed (1.46 s). Debug VST3/Standalone and all twelve suites passed (111.47 s), including CRT allocation checks. Tests cover both qualities/rates/latencies, pending/apply/legacy states, real VST3 mono/stereo and variable blocks, Delta, dry impulse alignment, finite output and silence. Logs: Reports/LowLatency-Release.log, LowLatency-FinalHostTests.log, LowLatency-TailTest.log, LowLatency-Debug.log, LowLatency-Debug-Details.log.

Release DSP-only 48k/256 benchmark: LowLatency Normal 2.39155–2.48475%, High 4.74891–4.87669%; standard latency in the same run 2.40321–2.44543% / 4.75548–4.9472%. No claim of DAW-meter equivalence. GUI screenshots reviewed: Reports/LowLatency-UI.png and LowLatency-UI-small.png. Package: Reports/SootheClone-Release-LowLatency.zip, complete VST3 bundle. Binary SHA256 8487DC1B4B9AF8E8B75E66A405FE699A8B90A30DB1D8A0E6C05A50079DC1EE92. README has exact mode/application semantics and manual checks. Linear Phase remains pending; no actual Ableton/PDC listening check or automatic installation performed.

## Linear Phase follow-up — 2026-09-08

Implemented the next Phase 8 item. LinearPhaseEngine shares the 4096-point detector curves and builds symmetric 4097-tap FIRs, processed by 8192-point overlap-save convolution. A symmetric 2x2 FIR matrix handles L/R, M/S, Link and Focus, with per-hop crossfades between old/new filter outputs. Packed real-channel FFTs reduce processing cost. Fixed 3072-sample latency (2048 FIR group delay + 1024 output scheduling) for both qualities; dry/Delta/Bypass match it. Frozen FIRs are linear phase; time-varying suppression remains a time-varying system. Finite FIR/windowing approximates requested magnitudes.

Added non-automatable linearPhase parameter at the end, default/legacy OFF. prepareToPlay latches it and host latency together. Pending UI requests require save/reload or audio-engine restart. Linear Phase overrides Low Latency while preserving its stored setting. Added right-bottom control and disabled Low Latency interaction while requested. Mono ignores stereo controls. All working memory preallocated. Conservative tail bound 9216 samples flushes the full convolution input history and output queue, including FFT roundoff, without changing playback latency.

Release VST3/Standalone and all thirteen suites passed (14.73 s). Added symmetric FIR, packed/unpacked FFT equivalence, phase, detector agreement, steady reduction, Quality/mode/Focus switching, neutral/Delta/Bypass alignment, non-finite input and silence tests. Extended real-VST3 and editor tests for mono/stereo variable blocks, state migration, mode precedence, pending activation, tail drain, automation and actual snapshots. Steady reduction difference <=0.1745 dB across tested rates/frequencies, frozen dephased imaginary error 3.03375e-7, fitted tone phase error 2.54237e-5 radians, neutral error 1.39698e-7, Delta error 7.45058e-9. Screenshots checked at 1100x680 and 880x544.

Release DSP-only benchmark (48k/256, 10 s per configuration): Linear Phase Normal 7.55649–8.74613%, High 13.9579–18.6793%. High observed maximum callback 8.0074 ms; averages do not guarantee glitch-free playback. README documents CPU/phase limitations, exact latency/application behaviour and manual DAW checks. No actual Ableton listening/PDC verification or installed-bundle replacement performed.

Evidence: Reports/LinearPhase-Release.log, LinearPhase-Release-Details.log, LinearPhase-Performance.txt, LinearPhase-UI.png, LinearPhase-UI-small.png. Complete Release VST3 bundle packaged and ZIP entries checked: Reports/SootheClone-Release-LinearPhase.zip. Binary SHA256: 5E7DCBD853DAFA2D8185976CEDB614C3BEF217658DA10FD3508E656D49CD45C4. Standalone Release output exists in the EQ build root.

Final Debug VST3/Standalone build and all thirteen suites passed (265.52 s). Debug CRT audio-path allocation checks recorded zero allocations. Evidence: Reports/LinearPhase-Debug.log and LinearPhase-Debug-Details.log. Release ZIP remains the distribution artifact.

## Debug registration follow-up — 2026-09-09

User reports ~50% CPU with Linear Phase OFF, Normal, 48k, output buffer4096. Ableton's latest PluginScanner.txt explicitly scans the Debug bundle as its custom VST3 location (2026-09-09 02:50:19). Verified MSVCP140D/VCRUNTIME140D imports in that binary. Evidence captured in Reports/BuildIdentity-InstalledEvidence.txt. No loaded SootheClone module was present at inspection, so no claim of observing its live callback or fixing the DAW preference directly.

Final change adds Release build / red DEBUG HIGH CPU to the editor. Performance executable now accepts rate/block and optional normal-only filter, and reports p99 realtime percentage and over-budget blocks. Exploratory SIMD FFT and distributed Linear Phase work was reverted after the user's configuration and scanner evidence identified the Debug registration; intermediate FFT-/Scheduled- reports are experiments, NOT the final deliverable. DSP and all mode latencies remain as before this turn.

Release VST3/Standalone and all13 regression suites passed (15.86s). Release DSP-only 48k/4096 Normal: mean3.74119–3.83341%, p99 5.14746–5.39859%, zero over-budget callbacks across the four tested configurations. Packaged Reports/SootheClone-Release-BuildIdentity.zip and copied the complete verified Release bundle to C:/Users/yuito/VST3-Release/SootheClone.vst3. Destination binary SHA256 matches build output: F2A43D8DC9C683738919A2B4718FFBE6608C042AE3E857F685271FD14BD9C326. Actual release screenshot reviewed. README documents replacing Ableton's custom Debug scan folder with the new Release-only parent folder, restarting/rescanning and checking the visible build label. No Ableton settings changed or DAW terminated.

Debug VST3/Standalone build plus actual host/editor tests passed (2 suites,129.68s). Debug warning screenshot reviewed. Debug 48k/4096 Normal averaged41.5075–157.96%, with deadline overruns in3/4 cases; large variability means no strict speedup ratio claimed. Release and Debug checksums both4.71531. Logs: BuildIdentity-Debug.log, BuildIdentity-Debug-Tests.log, BuildIdentity-Debug-4096.txt, BuildIdentity-Debug-UI.png. Final handoff requires user to select C:/Users/yuito/VST3-Release in Live and confirm Release build; application preference changes and subsequent live CPU improvement have not been verified.

## Measurement support — 2026-09-09

User authorized the next step. Major mode phases already implemented; completed a reproducible offline listening/measurement exporter for spec sections27/31. Added Tests/MeasurementExport.cpp and CMake SootheMeasurements target. No plugin/DSP changes. Generates33 signals (7 tones x4 levels, three-tone detail0/5/10, deterministic white noise, silence), each Soft/Hard through mono Engine, Normal,4096 latency. Writes165 float32 mono48k WAVs (input aligned, wet, actual Delta engine output),66 diagnostic curves and66-row summary CSV. Files have2s input +8192 tail,5ms ramps, specified sampled peak normalization. RMS comparison excludes starts/end ramps. Hard omits stale Soft-only diagnostic values; silence dB undefined fields empty. Refuses nonempty destinations.

Release full export passed, maximum Delta reconstruction7.45058e-9. Debug target built and four smoke renders passed. Independently validated all165 WAV headers/lengths and66 summary rows; all10 common smoke WAVs have byte-identical Debug/Release hashes. Evidence: Reports/Measurements-Build.log, Measurements-Release.log, Measurements-Debug.log, Measurements-Validation.txt. Listening pack and guide: Reports/Measurements. Root README explains reproducible CLI. This is the shared mono Engine output, not real-host, stereo, other latency-mode or Soothe3 comparison. Existing Release VST3 in C:/Users/yuito/VST3-Release unchanged.

## Product rename — 2026-09-09

User requested product I Soothea, developer YJ Audio. Updated JUCE PRODUCT_NAME/COMPANY_NAME, processor getName now uses JucePlugin_Name, editor branding, real-host discovery expectation and VST3 CTest bundle path. Internal CMake target/source directory and manufacturer/plugin codes retained. Verified both processor/controller CIDs match the prior bundle exactly: ABCDEF019182FAEB5975697453746863 and ABCDEF011234ABCD5975697453746863. DSP, parameter IDs and state format unchanged.

Release VST3/Standalone build and all13 tests passed (15.27s), including actual-host product/vendor discovery. Small editor snapshot checked; Reports/I-Soothea-UI.png and I-Soothea-UI-small.png. Packaged Reports/I-Soothea-Release.zip. Confirmed no SootheClone module loaded in Live at replacement; moved old Release bundle outside the scan root to C:/Users/yuito/VST3-Backups/SootheClone-before-I-Soothea.vst3 and installed C:/Users/yuito/VST3-Release/I Soothea.vst3. Installed/build SHA256 match:4038DD07F0E5FE6C48701827A31F014744D9A57D269ED997783C763D68495FE2. README leads with current branding, artifact paths, rescan/restart instructions and old-session validation caveat. No actual old Ableton project loaded or preferences changed.
Debug VST3/Standalone build and host/editor tests also passed (2 suites,50.93s). Evidence: Reports/Rename-Release.log and Rename-Debug.log. Release remains the installed distribution.

## Double-click point editing — 2026-09-09

User requested adding/removing weighting points by double click. PluginEditor now hides disabled bands and excludes them from hit tests; double-click active point disables it, double-click empty graph initializes the first disabled slot as Bell at cursor freq/amount (width1,slope24,focus0,enable last). Maximum5; full-capacity hint, outside graph ignored. Existing drag/wheel and lower band controls retained; previous double-click amount-reset replaced. Stable existing parameter IDs/state and DSP retained. Added EditorTests exercising actual down/up/down/double/up sequence, delete/reuse, disabled hit exclusion, scaled coordinates, full capacity no-op, Cut deletion, save/restore.

Release VST3/Standalone and13 tests passed14.07s; Debug VST3/Standalone and Editor test passed. Logs Points-Release.log and Points-Debug.log; small snapshot Points-UI-small.png reviewed. Packaged I-Soothea-Release-Points.zip; no loaded I Soothea module observed before installation. Prior bundle moved to C:/Users/yuito/VST3-Backups/I Soothea-before-points.vst3. Updated C:/Users/yuito/VST3-Release/I Soothea.vst3 and verified source/destination SHA256 CF132B4B0A8D63D262A57601B16AEFE4F2B48F2A411D9FFB6FEAB72C2C98B498. User still needs Live reload/rescan; actual DAW mouse interaction not claimed.
