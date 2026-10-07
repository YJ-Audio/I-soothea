# I Soothea — YJ Audio

現在の製品名は **I Soothea**、開発元は **YJ Audio** です。

独立したJUCE VST3 / Standaloneの共振抑制エフェクトです。対象はルートの `SOOTHE_CLONE_GOAL.md` 第36・37節（STFT / Soft / Depth / Detail / Delta）です。
出力：

- VST3バンドル：`%USERPROFILE%/.codex/builds/SootheClone/SootheClone_artefacts/Release/VST3/SootheClone.vst3`
- Standalone：`%USERPROFILE%/.codex/builds/SootheClone/SootheClone_artefacts/Release/Standalone/SootheClone.exe`
- テスト記録：同ビルドフォルダーの `Testing/Temporary/LastTest.log`

## 信号処理

`Input → Hann STFT → 相対スペクトル検出 → 周波数感度 → Detail → Depth → Attack/Release → スペクトル減衰 → Hann iFFT/OLA → Mix/Delta → Output`

- `STFT.h`：事前計算したradix-2 FFT、4096点・1024ホップ、周期的Hann窓。二重窓の重複和1.5を2/3で補正します。任意長ストリームに対応します。
- `SoftDetector.h`：局所dB平均との差を検出します。数値フロアを入力スペクトルのピークに追従させ、入力全体のゲインへの依存を抑えます。Depthは減衰dBの倍率です。Attack/Releaseはホップ周期のビン別包絡です。
- `DetailProcessor.h`：対数周波数上でピークを保持するGaussian包絡を作ります。平均化で高域の細いピークが薄まる問題を避けるための実験的な方式です。対数値の二乗距離包絡により、幅に依存しないO(N)処理にしています。固定個数のノッチフィルターは使いません。
- `Engine.h`：ドライを4096サンプル遅延させ、`Delta = Dry - Wet` をMix前に計算します。Mix/Output/Delta/Bypassの切替には5 msの平滑化を入れています。
- `PluginProcessor.*`：JUCE APVTSでパラメーターと状態を管理。音声コールバックはキャッシュしたatomic値を読みます。モノラルまたはステレオ入出力に対応し、ステレオはLink・M/S・Focusにも対応します。

遅延は4096サンプル（48 kHzで約85.33 ms）をホストに通知します。BypassとMix=0も遅延を維持します。Depth=0への変更直後はReleaseが残ります。設定が落ち着けば無加工復元になります。
