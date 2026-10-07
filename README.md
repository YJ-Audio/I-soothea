# I Soothea — YJ Audio

現在の製品名は **I Soothea**、開発元は **YJ Audio** です（2026-09-09変更）。ソースフォルダ・CMakeターゲット名は開発用としてSootheCloneを維持します。以下の過去の記録には旧製品名が含まれます。

最新Release VST3：`C:/Users/yuito/VST3-Release/I Soothea.vst3`。配布ZIP：`Reports/I-Soothea-Release.zip`。Standalone：`C:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Release/Standalone/I Soothea.exe`。

Abletonは現在のVST3カスタムフォルダを再スキャンし、必要ならセットを保存してLiveを再起動してください。製品一覧でI Soothea、開発元YJ Audio、画面左上でI SootheaとRelease buildを確認します。旧セッションを開いた際の復元も確認してください。VST3のプロセッサー／コントローラーCID、パラメーターID、状態形式、DSPを維持していますが、実際の旧Abletonセットを開く検証は未実施です。

旧配布バンドルは`C:/Users/yuito/VST3-Backups/SootheClone-before-I-Soothea.vst3`に退避しました。旧版と新版は同じVST3 IDなので同時登録を避けてください。

独立したJUCE VST3 / Standaloneの共振抑制エフェクトです。対象はルートの `SOOTHE_CLONE_GOAL.md` 第36・37節（STFT / Soft / Depth / Detail / Delta）です。VocalChopのソースは変更していません。

## ビルド

確認環境：Windows x64、Visual Studio 2026 Community（MSVC 19.51）、JUCE 9.0.1（`C:/JUCE`）。PowerShellから実行します。

```powershell
& ./Build.ps1
```

通常は最適化されたRelease版のVST3、Standalone、テストをビルドしてCTestを実行します。DAWで使うのはRelease版です。デバッグ用には `-Configuration Debug` を明示してください。Debugは処理負荷が大きく、実用時のCPU評価には適しません。

既定の生成先は `%USERPROFILE%/.codex/builds/SootheClone` です。日本語を含むビルド先ではJUCE生成ツールの文字コード処理が失敗したため、英数字の生成先を使います。ソースを移動する必要はありません。`-BuildDirectory` と `-JuceDirectory` で変更できます。Visual Studio 2026のCMake/C++コンポーネントが必要です。

出力：

- VST3バンドル：`%USERPROFILE%/.codex/builds/SootheClone/SootheClone_artefacts/Release/VST3/SootheClone.vst3`
- Standalone：`%USERPROFILE%/.codex/builds/SootheClone/SootheClone_artefacts/Release/Standalone/SootheClone.exe`
- テスト記録：同ビルドフォルダーの `Testing/Temporary/LastTest.log`

プラグインのシステムフォルダーへの自動コピーはしません。DAWへ入れるときは `SootheClone.vst3` フォルダーをバンドル全体として扱ってください。

## 信号処理

`Input → Hann STFT → 相対スペクトル検出 → 周波数感度 → Detail → Depth → Attack/Release → スペクトル減衰 → Hann iFFT/OLA → Mix/Delta → Output`

- `STFT.h`：事前計算したradix-2 FFT、4096点・1024ホップ、周期的Hann窓。二重窓の重複和1.5を2/3で補正します。任意長ストリームに対応します。
- `SoftDetector.h`：局所dB平均との差を検出します。数値フロアを入力スペクトルのピークに追従させ、入力全体のゲインへの依存を抑えます。Depthは減衰dBの倍率です。Attack/Releaseはホップ周期のビン別包絡です。
- `DetailProcessor.h`：対数周波数上でピークを保持するGaussian包絡を作ります。平均化で高域の細いピークが薄まる問題を避けるための実験的な方式です。対数値の二乗距離包絡により、幅に依存しないO(N)処理にしています。固定個数のノッチフィルターは使いません。
- `Engine.h`：ドライを4096サンプル遅延させ、`Delta = Dry - Wet` をMix前に計算します。Mix/Output/Delta/Bypassの切替には5 msの平滑化を入れています。
- `PluginProcessor.*`：JUCE APVTSでパラメーターと状態を管理。音声コールバックはキャッシュしたatomic値を読みます。モノラルまたはステレオ入出力に対応し、ステレオはLink・M/S・Focusにも対応します。

遅延は4096サンプル（48 kHzで約85.33 ms）をホストに通知します。BypassとMix=0も遅延を維持します。Depth=0への変更直後はReleaseが残ります。設定が落ち着けば無加工復元になります。

## 操作と手動テスト

1. Ableton LiveのオーディオトラックにVST3を挿し、1 kHz正弦波を入力します。Depth=5 / Detail=5 / Mix=1 / Output=0 dBから始めます。
2. 入力レベルを−6 / −12 / −24 / −36 dBと変え、入力に対する減衰比がほぼ一定であることを確認します。
3. Depthを0→2.5→5→10と上げ、減衰が増えることを確認します。
4. 500 Hz + 1 kHz + 2 kHzを入力し、Detail=0 / 5 / 10を比較します。低設定では広く、高設定では各ピークの周囲へ処理が集中します。現在の中央グラフは白線が周波数ウェイト、青線が減衰量、背景の濃い領域が入力スペクトルです。Stereoの解析表示では左右の大きい値を表示します。
5. Deltaをオンにして取り除かれた音を確認します。1 kHz入力では主に1 kHzが聞こえます。
6. Mix=0、Bypass、Depth=0をそれぞれ確認し、切替後の平滑化が落ち着いてから元音と比較します。外部で差分を取る場合は4096サンプルの遅延を合わせます。
7. プロジェクトを保存・再読込し、Depth/Detail/Delta等が復元されることを確認します。

Standaloneはオーディオ入力を処理するアプリです。サンプルファイルを開くプレーヤーではありません。入力デバイスと出力デバイスを設定して使用します。

## 自動検証

| テスト | 検証対象 |
|---|---|
| SootheCore | 決定論的ノイズの無加工復元、開始と末尾、1/17/64/127/511/1024/2049サンプル、無音、有限値 |
| SootheSoft | 初期Soft検出器単独のレベル不変性、局在性、Depth単調性 |
| SootheDetail | 3周波数同時入力、Detail 0/2.5/5/7.5/10の減衰幅とピーク、谷の分離 |
| SootheEngine | 7周波数×4レベル×44.1/48/96 kHz、周波数感度、Delta差分、無音、非有限入力、CRTメモリ確保監視 |
| SootheSignal | 最終処理でのDepth単調性、Deltaの1 kHz成分、ノイズ＋共振のレベル不変性、ゲイン/Bypass/Delta切替 |
| SootheVST3 | 実VST3の検出・生成・編集画面・状態復元、モノ/ステレオ、遅延、可変長/0長ブロック、処理中のメモリ確保、実バイナリのDelta差分 |

CRT監視はDebug版で行います。FFT、解析、Detail、遅延バッファは事前確保し、コールバックにファイルI/O・GUI操作・ログ・ロック待機を置いていません。

## 測定結果と調整候補

48 kHz、Depth=5 / Detail=5、単音の定常区間：

| 周波数 | 減衰 dB |
|---:|---:|
| 100 Hz | 2.39 |
| 200 Hz | 5.34 |
| 500 Hz | 7.53 |
| 1 kHz | 10.27 |
| 2 kHz | 12.44 |
| 5 kHz | 14.62 |
| 10 kHz | 16.00 |

これは今回の実装の測定値です。Soothe3そのものの内部実装や一致精度を示すものではありません。

次のA/B比較では、`Tuning` のbaseline幅（1 octave）、prominence knee（3 dB）、soft ratio（0.12）、周波数重み（100 Hz=0.8 / 1 kHz=1 / 10 kHz=1.3）、最大減衰（36 dB）、Attack/Release（20/120 ms）を調整候補にしてください。Detailはsigma最大0.6 octave、非線形幅マッピング、強さ0.4～2.0です。いずれも調整可能な仮説です。

## 制限

- Linear Phase、Low Latency、Quality（Normal/High）、Hard、ステレオリンク、M/S、Focusは実装済みです。最新の配布と制約は末尾のLinear Phase節を参照してください。
- 2026-09-07に参照画像をもとに専用UIを追加しました。青系の配色、左のノブ、中央のスペクトル／減衰表示、下部の操作バーです。1100×680を標準とし、880×544～1650×1020にリサイズできます。追加の依頼により、周波数ウェイトのドラッグ編集にも対応しました。
- DSP診断配列そのものは音声スレッド専用です。GUIには256点のスナップショットをatomicの所有権受け渡しで渡します。音声側は未読データがあれば更新をスキップし、待機・メモリ確保を行いません。表示更新は約30 Hzです。

## UI確認（2026-09-07）

`PluginEditor.h/.cpp` と `Tests/EditorTests.cpp` を追加し、ProcessorとCMakeを更新しました。DSPの減衰アルゴリズムとパラメーターIDはそのままです。

- Depth/Detailノブ、Attack/Release（ms）、Mix（%）、Output（dB）を既存APVTSに接続。
- Bypass/Delta、入力スペクトル表示切替、パラメーターのResetに対応。
- ノブからの値変更、Reset、実信号のグラフ描画、標準／縮小サイズを自動テスト。
- VST3でエディターを開いて信号処理した状態も、音声コールバックのメモリ確保0件を確認。
- Debug VST3/Standaloneビルド成功。6系統の既存回帰テストと追加UIテストが合格。

完成画面は `Reports/UI.png`、縮小画面は `Reports/UI-small.png` に保存しています。
- 固定FFTなので低域分解能と遅延に制約があり、サンプルレートやFFTビン位置で減衰量が変わります。
- Link 0%は独立処理です。Link 100%・Focus中央では共通の抑制量へ収束します。Focusを動かした場合は意図的に左右／M/Sの処理量が変わります。
- 実VST3の読み込みはJUCEホストで自動検証しています。Ableton Liveでの試聴・ハードウェアの実時間ドロップアウト検証、Soothe3との直接A/B比較は未実施です。

## 抑制カーブ（EQ操作）

このカーブは、周波数ごとの抑制量を調整します。音そのものをブーストするEQではありません。白線がウェイト、青線が実際の減衰量、背景が入力スペクトルです。既存の共振検出器は独立したまま、Detail処理後にウェイトを掛け、DepthとAttack/Releaseを通して反映します。

- ポイントを左右へドラッグ：周波数（20 Hz～20 kHz）を変更。
- Bell/Shelfのポイントを上下へドラッグ：Strength（-24～+24 dB）を変更。+6 dBで抑制dB量が約2倍、-6 dBで約半分です。実際の減衰は36 dBを上限とします。
- ポイント上でホイール：Bell/Shelfの幅（oct）、Low/High cutの傾き（dB/oct）を変更。
- ポイントをダブルクリック：削除（無効化して非表示）。グラフの空白をダブルクリック：Bellポイント追加（最大5点）。右クリックでも無効化できます。下部のバンド選択とenabledで再有効化できます。
- 下のBand選択・Enabled・Shape・数値欄でも操作できます。ドラッグ開始時は対象バンドが有効になります。
- 形状はBell / Low shelf / High shelf / Low cut / High cutから選択可能です。CutのStrengthは使用しません。
- 最初は3つのBellを0 dB、Low/High cutを無効にしており、以前の処理音を維持します。古いプロジェクトにバンド設定がなければ同じ初期値で復元します。
- Low/High cutは対象外の周波数で抑制量を小さくします。原音の低域・高域を切るフィルターではありません。ウェイトは内部で-60～+24 dBに制限します。

現在のVST3が使用中だったため、EQ対応版は別フォルダーにビルドしています。

```powershell
& ./Build.ps1 -BuildDirectory C:/Users/yuito/.codex/builds/SootheClone-EQ
```

出力は `C:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Debug/VST3/SootheClone.vst3` と同階層の `Standalone/SootheClone.exe` です。プラグインIDは同じです。DAWで更新するときは元のインスタンスを閉じ、既存バンドルをこの新しいバンドルで置き換えてから再スキャンしてください。

手動確認：500 Hz + 1 kHz + 2 kHzを入力し、1 kHzのBellを幅0.3 octで上下に動かします。1 kHzの抑制が増減し、離れた2音への影響は小さいことを確認します。次にLow cutを有効にし、低域が処理されにくくなることを確認します。保存・再読込してカーブが戻ること、Deltaが除去された音のままであることも確認します。

## Hardモード（2026-09-07）

左上の soft / hard で切り替えます。Softは周辺スペクトルに対する相対的な突出、Hardは正規化した絶対レベルに反応します。低レベルではHardの減衰が減り、しきい値より十分に小さい入力ではほぼ処理しません。

HardDetector.hに独立した検出器を追加しました。周期的Hann窓のFFT振幅をN/4（DC/NyquistはN/2）で割ってdBFSへ変換し、仕様書の感度ポイントを対数周波数で補間します。初期しきい値は-5 dB、比率2:1、ニー幅6 dBです。これらはHardTuningに集約した調整用定数で、Soothe3の内部値を断定したものではありません。初期UIはモード切替のみで、しきい値・比率・ニーの個別ノブは設けていません。

Detail、5バンドのウェイト、Depth、Attack/Release、Mix、Delta、Bypassは共通です。モードを変えても再生位置や包絡をリセットしません。追加パラメーターModeは既存パラメーターの末尾に追加し、IDと並びを維持しています。Modeのない古い保存状態はSoftで復元します。

48 kHz、Depth=5、Detail=5、ウェイトがフラットな状態での1 kHz測定：

| 入力 dBFS | Hardの減衰 dB |
|---:|---:|
| -6 | 9.46 |
| -12 | 5.88 |
| -24 | 0.04 |
| -36 | 約0 |

仕様書の約3 dB減衰を得る基準入力（100/200/500/1000/2000/5000/10000 Hzに対し0/-4.5/-13/-17.5/-24.5/-38/-38 dBFS）では、44.1/48/96 kHzの測定が約2.1～3.0 dBでした。FFTビンの位置と窓による差がある初期モデルで、実機との直接A/B調整は未実施です。

手動テスト：

1. Resetでフラットな状態に戻し、Hardを選択します。
2. 1 kHz入力を-6→-12→-24→-36 dBFSと下げ、減衰が減ることを確認します。
3. Softへ切り替え、入力レベルへの反応の違いを比較します。
4. 再生中にSoft/Hardを切り替え、クリックや再生位置の飛びがないことを確認します。
5. カーブを編集して対象帯域の処理が変わること、Deltaが除去成分を返すことを確認します。
6. Hardで保存・再読込し、選択が復元されることを確認します。

最新のHard対応VST3/StandaloneはEQ対応時と同じ `C:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Debug` 以下に出力します。Hardを選択した画面はReports/Hard-UI.png、縮小画面はReports/Hard-UI-small.pngです。

## ステレオリンク・M/S・Focus（2026-09-07）

上部のL/R・M/S選択とLink、選択バンド欄のFocusを追加しました。新規・旧セッションとも初期値はL/R、Link 0%、Focus 0%で、従来の処理を維持します。追加パラメーターは既存のModeより後ろに置き、以前のID・順序を維持しています。

- Link 0%：左右（M/S時はMid/Side）を独立検出。100%：各周波数で大きい側の抑制量を共有。中間値は独立量と共有量を補間します。
- Focus 0%：両成分を等しく処理。負側はLeft/Mid、正側はRight/Sideへ処理を寄せます。選択した側を増幅せず、反対側への処理を帯域内で弱めます。Linkの後に適用するためLink 100%でも有効です。
- Focusは有効なバンドにのみ作用します。Bellはベル形の範囲、Shelfは棚側、Cutは処理を残す通過側に作用します。複数バンドのFocusは乗算で合成し、バンドを無効にするとそのFocusも無効です。Strength 0 dBのBellでもFocusは使えます。
- M/SはMid=(L+R)/2、Side=(L-R)/2、復元はL=Mid+Side、R=Mid-Sideです。中央の同一音はHard検出のレベルを維持します。片側だけの入力はMid/Sideそれぞれで6 dB低くなる規約です。
- L/RとM/Sの検出・包絡を両方更新し、切替時はL/Rに戻したスペクトルを時定数30 msでクロスフェードします。過去の遅延音の成分を切替で読み替えません。両系統の検出を動かすためCPU負荷は従来より増えます。
- 遅延は4096サンプルのままです（48 kHzで約85.3 ms）。Monoは従来のEngineで処理し、Stereo/Link/Focus欄は無効になります。
- 白線は共通の周波数ウェイトです。Focus別の線ではありません。背景は選択ドメインのスペクトル最大値、青線は両成分の最大抑制量です。切替直後の青線はクロスフェード途中の実効量ではなく選択先の診断値です。

これは仕様の方向性に沿った独自モデルで、実機のリンク方式・Focusカーブを再現したと断定するものではありません。

自動検証：Debug VST3/Standaloneビルド、全10スイート合格。44.1/48/96 kHz、Soft/Hard、モード切替、Depth 0再合成、Delta再合成、Linkによる左右比維持、L/RとM/SのFocus、無音・非有限入力、音声処理のメモリ確保0件を確認。旧L/R出力との差は0、Depth 0最大誤差1.27e-7、Delta最大誤差2.98e-8。VST3の可変ブロック処理、設定保存・復元、旧状態の移行、Mono時のUI無効化も確認しています。証跡はReports/StereoBuild.log、StereoHostTests.log、Stereo-UI.png、Stereo-UI-small.pngです。

手動テスト：

1. 左右で音量差のあるステレオ素材を再生し、HardでLinkを0→100%へ変更。Focus中央で左右の処理が連動することを確認します。
2. Band 3を選び、1 kHz付近の音に対してFocusを-100→0→+100%へ変更。抑制が左→両側→右へ移ることを確認します。
3. M/Sへ切り替え、中央と広がりのある素材でFocusのMid/Sideへの効果を聴きます。
4. 再生中にL/R・M/Sを切り替え、Delta、Bypass、Mixを確認します。
5. 保存・再読込してStereo mode、Link、各バンドのFocusが戻ることを確認します。

出力先は引き続き C:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Debug です。Ableton Liveでの試聴、実機負荷測定、Release版検証は未実施です。
## CPU負荷修正（2026-09-07）

前回案内したDebugビルドは最適化が無効で、DAWで高負荷になる主要因でした。実用向けの配布・Build.ps1既定値をReleaseへ変更しました。またHard中に不要だったSoftの局所平均・共振スコア計算を省き、FFT振幅のみをHardへ渡します。Hardの入力値、減衰式、包絡、帯域ウェイトは同じです。Hard中の内部Soft診断配列（baseline/resonance/raw）は更新しません。GUIで使用する振幅と最終減衰は更新します。L/RとM/Sの同時解析は滑らかな切替と音を維持するため残しています。

最新出力は `C:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Release/VST3/SootheClone.vst3` と `Release/Standalone/SootheClone.exe` です。上記の過去の節にあるDebugパスは旧ビルドです。DAWを閉じてから、登録済みのSootheClone.vst3バンドルをRelease版で置き換え、再起動・再スキャンしてください。同じプラグインIDなので旧Debug版を別名で追加登録するのではなく、使用中のバンドルを差し替えます。自動インストールは行いません。

測定用のSoothePerformanceターゲットを追加しました。48 kHz、256 samples、10秒のステレオ複合信号を各モードで処理し、コールバック相当の処理時間の平均・p99・最大値を記録します。実プラグイン同様、非正規化数をゼロ扱いにします。測定はDSP単体で、DAWのCPUメーター、GUI、他プラグイン、ドライバーは含みません。タイミングを合否条件とする不安定なテストにはしていません。証跡はReports/Performance-Before-Debug.txt、Performance-Before-Release.txt、Performance-After-Release.txtです。

手動確認：同じセッション・入力・サンプルレート・バッファでRelease版に差し替え、Soft/HardとL/R・M/Sを比較してください。UIを開閉した場合の負荷と、再生中の切替時の音切れも確認してください。
CPU測定結果（このPC、48 kHz / 256 samples、DSP単体。値は音声実時間に対する処理時間%）：

| モード | 旧Debug | 旧Release | 修正Release |
|---|---:|---:|---:|
| Soft L/R | 68.67 | 2.79 | 2.81 |
| Soft M/S | 102.68 | 2.86 | 2.87 |
| Hard L/R | 43.90 | 3.01 | 2.83 |
| Hard M/S | 43.53 | 3.12 | 2.90 |

旧Releaseと修正ReleaseのSoft差は約0.01～0.02ポイントで、処理方式は変更していません。Hardは不要解析の省略で約6～7%の追加短縮。修正Releaseのp99ブロック時間は約717～842 us、最大は約1.90～2.57 msでした。OSのスケジューリングや他処理にも影響される測定であり、DAWのCPU表示や無音切れを保証する値ではありません。出力チェックサムは各ビルドで表示精度内で一致し、音の正当性は回帰テストでも確認しています。

差し替え用のVST3全体をReports/SootheClone-Release-CPU-Fix.zipにも格納しました。Releaseの全10テストは6.13秒で合格。VST3バイナリSHA256: 9936C00BE5854FBEF3CBA990B24C5E83DEBE764D7A433B036681AF13AD003EB1。

## Quality：Normal／High（2026-09-08）

画面下部のquality欄にNormal／Highを追加しました。Normalが初期値で、古い状態にQualityがなければNormalへ復元します。新しいQualityパラメーターは既存パラメーター末尾へ追加し、ホストのオートメーションと保存・復元に対応します。

| 設定 | FFTサイズ | 更新間隔 | 48 kHzでの更新間隔 | 遅延 |
|---|---:|---:|---:|---:|
| Normal | 4096 | 1024 samples | 約21.3 ms | 4096 samples |
| High | 4096 | 512 samples | 約10.7 ms | 4096 samples |

Highは時間方向の解析更新を細かくします。周波数分解能、検出感度、しきい値、Depth、Detailは変更しません。Attack/ReleaseとM/Sクロスフェードの係数は実際の更新間隔で補正し、秒単位の時定数を維持します。過渡的な入力では、更新頻度の違いにより抑制の推移が変わります。これは独自のQualityモデルであり、Soothe3内部の品質モードの再現を断定するものではありません。

STFTの更新間隔はフレーム境界で切り替えます。重ね合わせの実際の窓の重みで出力を正規化するため、更新間隔の異なるフレームが混在しても無加工時の音量を維持します。リセット前の無音フレーム分の重みも初期化し、起動時に正規化によって音量が持ち上がらないようにしています。再生位置・遅延・包絡はQuality切替でリセットしません。

Release検証：全11スイート合格（8.47秒）。44.1/48/96 kHz、Soft/Hard、100/1000/10000 Hzの定常信号でNormal/Highの減衰差は最大0.00685 dB。Qualityを繰り返し切り替えた無加工復元誤差1.19e-7、Delta再合成誤差7.45e-9。M/S・Focus・Soft/Hardとの同時切替、保存復元、旧状態移行、実VST3可変ブロック処理を確認しました。

CPU測定（Release・DSP単体、48 kHz / 256 samples、DAWの表示とは別）：Normalは約3.16～3.22%、Highは約6.11～7.72%。Highは解析回数が倍になるため負荷が増えます。品質切替を扱う正規化処理の追加により、Normalにも従来より処理が増えています。通常はNormalを使い、必要な箇所でHighへ変更してください。測定ログはReports/Quality-Performance.txtです。

手動確認：

1. 更新版Release VST3へ差し替え、qualityがNormalで起動することを確認します。
2. 同じ音源を再生しながらHighに切り替え、CPU負荷とアタック部分の変化を比較します。
3. Depth 0で切り替え、音量が揺れないことを確認します（Depth変更直後はReleaseが残ります）。
4. Delta、M/S、Focusを使用してQualityを変更し、音切れがないか確認します。
5. Highで保存・再読込し、選択が復元されることを確認します。

最新パッケージ：Reports/SootheClone-Release-Quality.zip。ビルド先は引き続きC:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Releaseです。スクリーンショットはReports/Quality-UI.png、Quality-UI-small.png。Low Latency／Linear Phaseは次の検討項目です。Ableton Liveでの試聴・ドロップアウト検証は未実施です。Debug版でもVST3/Standaloneをビルドし、全11テストが合格しました（106.15秒）。Debugの音声処理メモリ確保チェックは0件です。配布・DAW使用には上記のRelease版を使用してください。

## Low Latency（2026-09-08）

画面上部のlow latencyを追加しました。通常は4096 samples、Low Latencyは2048 samplesです。48 kHzで約85.3→42.7 ms、44.1 kHzで約92.9→46.4 ms、96 kHzで約42.7→21.3 msになります。ゼロレイテンシーではありません。初期値と旧セッションの復元はOFFです。

Low Latencyの変更は即時には適用しません。変更するとボタンに「reload to apply」と表示されます。設定を保存してプラグインを再読み込みするか、DAWのオーディオエンジンを再起動してください。DAWによっては単なる再生停止・再開では適用されません。上部のms表示が変わったことを確認してください。ホストがprepareToPlayを呼ぶ時点で処理方式と通知遅延を同時に確定します。音声処理中やresetだけでは切り替えず、未適用の間は元の遅延を維持します。Low Latencyは再生中のオートメーション対象から外しています。通常のパラメーター保存・復元には含まれます。

方式：4096点のFFT解析は維持し、逆FFTの後半2048サンプルを短いHann合成窓で出力します。重ね合わせは解析窓と合成窓の積で正規化します。検出しきい値、感度、帯域カーブ、Detail、Attack/Releaseは共通で、検出・減衰カーブは通常モードと同じです。Mix/Delta/Bypassのドライも2048サンプルに合わせます。Quality Normalの更新間隔1024、Highの512はそのままです。低遅延用と通常用のメモリーは事前確保し、実行するのは選択されたエンジンだけです。

短い合成窓のため、過渡応答や狭い帯域の聞こえ方は通常モードと完全には一致しません。定常正弦波比較（44.1/48/96 kHz、100～10000 Hz）では、Quality Normalで最大約0.37 dBの出力減衰差でした。Normal/High両品質の比較テストは0.5 dB以内を確認しています。解析窓を維持するため、信号変化に対する検出器の履歴長は短くなりません。実機Soothe3の方式や音との一致を保証するものではありません。

CPU測定（ReleaseのDSP単体、48 kHz/256 samples）：Low LatencyはQuality Normalで約2.39～2.49%、Highで約4.75～4.88%。同条件の通常遅延モードは約2.40～2.45%／約4.76～4.95%で、ほぼ同じ負荷でした。DAWメーター・GUI・ドライバーを含む数値ではありません。Reports/LowLatency-Performance.txtに記録しています。

Release版の全12スイートが合格（9.30秒）。両遅延・両品質・モノ／ステレオ・可変ブロック、再合成、Delta、ドライ／Bypassのインパルス位置、検出曲線一致、無音・非有限値、状態保存、旧状態移行、未適用状態の表示を検証しました。ホストへ通知するテール長には解析履歴と合成遅延の上限を含め、信号末尾の処理が切れないことも実VST3で追加確認しています。追加ホスト結果はReports/LowLatency-FinalHostTests.logとLowLatency-TailTest.logです。

手動確認：

1. DAWを閉じてReleaseのVST3へ差し替えます。
2. low latencyをONにして設定を保存し、プラグイン再読み込みまたはオーディオエンジン再起動を行います。
3. 48 kHzなら上部が42.7 msになることと、ホストの遅延表示が2048 samplesになることを確認します。
4. 同じ音源でQuality Normal/High、Soft/Hard、M/S、Focus、Deltaを比較します。
5. OFFに戻した後も適用操作を行い、85.3 msへ戻ることを確認します。

最新パッケージ：Reports/SootheClone-Release-LowLatency.zip。VST3/Standaloneのビルド先はC:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Releaseです。画面はReports/LowLatency-UI.png、LowLatency-UI-small.png。Ableton Liveでの実際の遅延補正・試聴・録音確認は未実施です。次の未実装項目はLinear Phaseです。Debug版もVST3/Standaloneと全12スイートが合格しました（111.47秒）。配布版はReleaseです。

## Linear Phase（2026-09-08）

右下にlinear phaseを追加しました。初期値・旧状態ではOFFです。ONにして設定を保存し、プラグインを再読み込みするか、DAWのオーディオエンジンを再起動すると適用されます。変更待ちは「reload to apply」と表示します。prepareToPlayで処理方式とホストへの通知遅延を同時に確定するため、単なる再生停止・再開では適用されない場合があります。再生中のオートメーション対象には含めません。

Linear Phaseの遅延はQualityにかかわらず3072 samples（48 kHzで64.0 ms）。Low Latencyより優先し、有効にする間はLow Latencyの操作を無効にします。Low Latencyの保存値は保持するため、Linear PhaseをOFFにして再適用すると元の選択に戻ります。上部のms表示は実際に動いている方式の遅延です。

4096点の共通解析・検出から4097タップの対称FIRを作り、8192点FFTのoverlap-saveで畳み込みます。FIR群遅延2048 samplesと一定のブロック遅延1024 samplesの合計が3072です。係数更新はNormalで1024、Highで512 samples。更新前後のフィルター出力をクロスフェードし、左右信号とフィルター設計のFFTをまとめて計算します。処理用メモリーは事前確保し、選択されたエンジンだけを動かします。ホストへのテール通知は畳み込み履歴と出力キューが完全に空になる上限9216 samplesで、追加の再生遅延ではありません。

各時点の固定FIRは線形位相です。抑制量が時間変化する動的処理全体を、時不変の線形位相フィルターと同一視はできません。プリリンギングや過渡音の変化はあり得ます。検出感度・しきい値・Detail・帯域ウェイト・Attack/Releaseは共通で、検出減衰曲線の一致を検証しています。有限長の窓付きFIRによって実際の周波数応答は近似になります。青線は検出器の目標減衰量であり、FIRの実効応答そのものではありません。定常信号比較（44.1/48/96 kHz、Soft/Hard、100～10000 Hz）の通常モードとの減衰差は最大0.1745 dBでした。

CPU計測：このPCのRelease、DSP単体、48 kHz / 256 samples、10秒の複合ステレオ信号で、Linear PhaseのNormalは約7.56～8.75%、Highは約13.96～18.68%。Highの最大ブロック時間には8.01 msのスパイクも記録されました。平均値だけでドロップアウトの有無は判断できません。通常モードより重いため、まずNormalで実際のセッション負荷を確認してください。DAWメーター・GUI・ドライバーを含む測定ではありません。詳細はReports/LinearPhase-Performance.txtです。

Release VST3/Standaloneのビルドと全13スイートが合格（14.73秒）。FIR対称性・位相・FFTをまとめた計算の一致、共有検出曲線、Quality切替、L/R・M/S・Focus、Delta、Bypass、固定遅延、Mono、可変ブロック、状態保存・旧状態移行・未適用表示、テールの完全排出を検証しました。無加工復元誤差は最大1.40e-7、Delta誤差7.45e-9。画面は1100×680と880×544で確認しました。

手動確認：

1. DAWを閉じ、登録済みのSootheClone.vst3をZIP内のReleaseバンドル全体で差し替え、再起動・必要に応じて再スキャンします。
2. linear phaseをONにして保存・再読込し、48 kHzで上部が64.0 ms、ホストの遅延表示が3072 samplesになることを確認します。
3. Quality Normalから始め、同じ素材で通常モードとの音・CPU負荷を比較します。切り替えごとに適用操作を行います。
4. Soft/Hard、M/S、Focus、Mix、Delta、BypassとHighを確認し、アタック付近の変化や音切れを聴きます。
5. 保存・再読込の復元と、Linear PhaseをOFFにして再適用した際にLow Latencyの保存設定へ戻ることを確認します。

最新配布：Reports/SootheClone-Release-LinearPhase.zip。VST3/Standaloneの出力先はC:/Users/yuito/.codex/builds/SootheClone-EQ/SootheClone_artefacts/Releaseです。証跡はReports/LinearPhase-Release.log、LinearPhase-Release-Details.log、LinearPhase-UI.png、LinearPhase-UI-small.png。Ableton Liveでの実際の遅延補正・試聴・ドロップアウト確認は未実施です。

Debug版もVST3/Standaloneのビルドと全13スイートが合格しました（265.52秒）。Debug CRTフックによる音声処理のメモリ確保は0件。証跡はReports/LinearPhase-Debug.logとLinearPhase-Debug-Details.logです。DAW使用・配布にはRelease ZIPを使用してください。

## Debug版の誤登録対策（2026-09-09・最新配布）

ユーザー報告はLinear Phase OFF、Quality Normal、48 kHz、出力バッファ4096で約50%でした。Ableton Live 12.3.2の2026-09-09 02:50スキャンログで、カスタムVST3フォルダがSootheClone-EQ/SootheClone_artefacts/Debug/VST3/SootheClone.vst3/Contents/x86_64-winに設定されていることを確認しました。対象バイナリもMSVCP140D.dllとVCRUNTIME140D.dllを参照するDebugビルドです。ReleaseのZIPを用意するだけでは、この読み込み先は変わりません。

画面左上のYuito Audioの下に、実行中のビルドを表示するようにしました。Releaseは「Release build」、Debugは赤い「DEBUG / HIGH CPU」です。最終版のDSP、設定保存形式、遅延は前回のLinear Phase実装から変更していません。通常4096／Low Latency 2048／Linear Phase 3072 samplesです。

**実用版の配置先：C:/Users/yuito/VST3-Release**。このフォルダに、検証済みReleaseのSootheClone.vst3バンドル全体を配置しました。ビルドキャッシュやDebugフォルダの中を登録しないでください。

手動適用：

1. 作業中のセットを保存します。
2. Abletonの設定→Plug-Insで、VST3カスタムフォルダを「C:\Users\yuito\VST3-Release」に変更し、再スキャンします。既存のDebugフォルダ指定を置き換えます。
3. Liveを再起動し、SootheCloneを開いて「Release build」を確認します。VST3バンドル内部のContents/x86_64-winを個別指定する必要はありません。
4. 同じセット・音源・48 kHz・4096 samples・Normal・Linear Phase OFFでCPUを比較します。

ReleaseのDSP単体計測は同じ48 kHz／4096 samples／Normalで平均3.74～3.83%、p99は5.15～5.40%、計測中のバッファ予算超過0件でした。Soft/Hard、L/R・M/Sの4通り、各約10秒の測定です。DAW全体のCPU値を保証するものではありません。Performanceツールはサンプルレートとブロックサイズの引数、およびnormal引数（通常遅延・Normalのみ）に対応しました。

ReleaseのVST3/Standaloneビルドと全13スイートが合格（15.86秒）。配置先とビルド元のSHA256一致を確認しました。配布ZIPはReports/SootheClone-Release-BuildIdentity.zipです。証跡はReports/BuildIdentity-InstalledEvidence.txt、BuildIdentity-Release.log、BuildIdentity-Release-Details.log、BuildIdentity-Release-4096.txt、BuildIdentity-Release-UI.png。Liveの設定自体はまだ変更していないため、上記の読み込み先変更が必要です。

Debug版もVST3/Standaloneビルドとホスト・Editorの2スイートが合格しました（129.68秒）。赤いDebug警告表示を実画像で確認しました。同じDSP単体条件でDebugは平均41.5～158.0%と大きく変動し、複数条件でバッファ予算超過も記録しました。ReleaseとDebugの出力チェックサムは表示精度内で一致（4.71531）。これらは別時点の測定でPCの他処理にも影響されるため、厳密な速度倍率の比較には用いません。ログはBuildIdentity-Debug.log、BuildIdentity-Debug-Tests.log、BuildIdentity-Debug-4096.txtです。

## 測定・試聴パック（2026-09-09）

仕様書27・31節の比較作業向けに、オフラインのSootheMeasurementsターゲットを追加しました。DSP／プラグイン本体は変更していません。Reports/Measurements/README.mdに試聴方法、信号条件、CSV列の定義があります。33信号をSoft/Hardで処理し、遅延を揃えた入力／Wet／Deltaの165 WAVと66曲線CSV、一覧measurements.csvを生成しました。

再生成はSootheMeasurementsをReleaseでビルドし、プロジェクトフォルダから以下を実行します。既存ファイルを壊さないよう、出力先は空のフォルダまたは未作成のフォルダに限ります。日本語を含む絶対パスのコマンドライン引数を避け、下記のようにASCIIの相対パスを指定してください。

```powershell
& C:/Users/yuito/.codex/builds/SootheClone-EQ/Release/SootheMeasurements.exe Reports/Measurements-New
```

--smokeを末尾に付けると1kHz/-12dBFSと無音の4処理だけを書き出します。生成ツールはWAV/CSVを出力し、試聴ガイドはReports/Measurements/README.mdを参照します。

検証：Release全66処理で有限出力・Delta再合成を確認し、最大誤差7.45058e-9。Debugビルドと4処理のスモーク検証も成功。全165 WAVのRIFF長／float32 mono／48kHz形式を独立に確認し、DebugとReleaseの共通10 WAVはSHA256一致。証跡はReports/Measurements-Build.log、Measurements-Release.log、Measurements-Debug.log、Measurements-Validation.txt。既存VST3を差し替える必要はありません。

製品名変更の検証：Release VST3/Standaloneと全13テスト合格。Debug VST3/Standaloneとホスト・画面の2テストも合格（50.93秒）。ログはReports/Rename-Release.log、Rename-Debug.logです。

## ポイントの追加・削除（2026-09-09）

グラフの空白をダブルクリックすると、その周波数・強さにBellポイントを追加します。幅1 octave、Focus中央から開始します。既存のポイントをダブルクリックすると、形状にかかわらず削除して非表示にします。従来のダブルクリックによる強さ0へのリセットを置き換えます。ドラッグ移動・ホイールによる幅／傾き変更は継続します。

最大5点です。5点使用中はグラフ下に案内を表示し、空白のダブルクリックは既存点を変更しません。初期状態は3点なので2点追加できます。内部は固定5バンドのenabledを利用し、削除したバンドのパラメーターIDは維持します。削除した枠への追加はBellの初期値で置き換えます。保存・再読込に対応し、DSPと遅延、オーディオスレッドの処理量は変更しません。

Release全13テスト合格（14.07秒）。DebugのVST3/StandaloneビルドとEditor検証も合格。操作テストは通常クリック順序での追加・削除、グラフ外、非表示点のドラッグ無効、縮小画面、上限、Cut削除、保存復元を含みます。配布はReports/I-Soothea-Release-Points.zip、配置先C:/Users/yuito/VST3-Release/I Soothea.vst3を更新済みです。Liveを再起動・必要に応じて再スキャンして反映してください。旧版はC:/Users/yuito/VST3-Backups/I Soothea-before-points.vst3へ退避しました。

手動確認：グラフ空白をダブルクリック→ポイントをドラッグ→同じポイントをダブルクリックして削除→セット保存・再読込で配置を確認。音の処理は通常のEQゲインではなく、周波数ごとの共振抑制ウェイトです。
