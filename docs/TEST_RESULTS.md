# テスト結果（TEST_RESULTS.md）

## Phase 2（DSP 基盤）2026-10-23 / Windows 11, MSVC 19.44, JUCE 9.0.3, Release

Phase 2 は回路モデル未実装（恒等）のため、土台（I/O ゲイン・バイパス・メーター・パラメータ管理・
mono→stereo・数値安定性）を検証する。音色検証は Phase 3 以降で本ファイルに追加する。

### PassthroughTest — ALL PASSED（0 failures）
- バイパスのビット完全素通し: mono / stereo / mono→stereo × 44.1/48/88.2/96 kHz × block 64/512/1024（ゲイン設定下でも）
- Phase 2 恒等モデルでのニュートラル素通しもビット完全
- 状態保存/復元（全パラメータ＋選択プリセット）、プログラム再送で編集が消えない、ホスト Bypass 公開
- バス構成: mono→stereo 受理 / stereo→mono 拒否、レイテンシ 0

### DspTest — ALL PASSED（0 failures）
- 無音→無音、ニュートラル/バイパスのビット完全一致
- Input +6 dB=×2.0 / Output −6 dB=×0.5
- mono 入力を両出力へファンアウト
- 44.1/48/88.2/96 kHz で +3 dB ゲイン一致・出力有限
- バイパス切替・パラメータ急変でクリックなし・有限
- メーター（入力ピーク一致、出力が入力ゲインに追従）
- 全プリセット（6種）が値をロードしホストに反映
- 5 分連続＋ランダムオートメーション: NaN/Inf なし、出力有界

### Vst3HostTest — ALL PASSED（0 failures）
DAW 相当のホスト（JUCE VST3 ホスティング）で:
- スキャン/ロード、名称 `Arks Sonic 22` / ベンダ `Arks Plugin` / v0.1.0、レイテンシ 0
- stereo / mono / mono→stereo 受理、各構成で素通し
- ホスト Bypass＝Bypass、バイパスで無改変、状態ラウンドトリップ、エディタ起動

### CpuBenchmark（参考、Intel i3-8145U, 3 s/case）
- 全ケースで 1 コアの平均 0.1% 未満（恒等モデルのベースライン）。Phase 3 以降のオーバーサンプリング/非線形段で増加する。

## Phase 2 完了条件の充足
- ビルド成功（VST3 / Standalone、Windows）: ✓
- DAW 相当ホストでロード・動作: ✓（Vst3HostTest）
- 入出力/パラメータ管理・状態保存: ✓
- 自動テスト整備・実行: ✓（4 種）
- CPU 負荷計測: ✓
- 既知の制約（回路モデル未実装＝恒等）を明記: ✓（本書・README・AmpEngine.h）

## Phase 3（Vintage チャンネル実回路）2026-10-23 / 同環境

Vintage 信号経路を実回路としてモデル化: **V1-A(12AX7) → Fat → Fender Treble/Bass トーンスタック → Volume → V1-B(12AX7)**、非線形段はオーバーサンプリング領域内。
トーンスタックは回路図の実定数（R1=250k/R2=250k/R3=6.8k/R4=100k, C1=250pF/C2=0.1µF/C3=0.047µF）を Fender 伝達関数（Yeh & Smith, Faust）に写像し、bilinear 変換で実装。Burn/パワー段/リバーブは未実装（恒等）。

### DspTest — ALL PASSED
- 基盤（無音・ゲイン・mono→stereo・bypass）は Burn（恒等）経由で検証
- トーンスタック: Treble 10→90 で 3kHz が +13dB、Bass 10→90 で 100Hz が +6.7dB（正しい帯域に作用）
- Volume を上げると倍音増加、2 次倍音が基音比で明確に存在（単段の偶数次倍音）
- Fat ON で 180Hz 低中域 +3.6dB
- **オーバーサンプリング: 10kHz 入力の折返し（18kHz）が OS Off −11dB → 8x −100dB（−89dB 改善）**
- 安定性: バイパス切替クリックなし、極端設定（Vol100/Tre100/Bas100/Fat/+24dB）で有限・有界、2 分オートメーションで有限・有界
- メーター一致、全プリセット値ロード
- ニュートラル（Treble/Bass/Vol 中央, 入力 −6dB）で出力 −6.4dB ≒ ユニティ（ヘッドルーム校正済）

### PassthroughTest / Vst3HostTest — ALL PASSED
- Bypass はビット完全素通し（mono/stereo/mono→stereo、全SR、ゲイン設定下でも）
- DAW 相当ホストで各レイアウトが有限・非無音出力、Bypass 無改変、状態ラウンドトリップ、エディタ起動

### CpuBenchmark（Intel i3-8145U, 3 s/case）
- 既定（Vintage, 4x OS）ステレオ ≈ 1 コアの 1% 未満
- フルドライブ Vintage, 8x OS: 48kHz ≈ 6.3% / 96kHz ≈ 12.6%（低スペック機、1 コア比）

### 未校正（Phase 9 で実機突合）
- 真空管デバイス係数（μ/gm）は INFERRED、ポットのカーブは APPROXIMATED、各段の絶対動作点は実機 TP 電圧と未照合。

## Phase 4（Burn チャンネル実回路）2026-10-24 / 同環境

Burn 信号経路を実回路としてモデル化: **V1-A → Gain1 → V1-B → Gain2 → V2-A → TMB トーン → V2-B → Volume**（4 段の 12AX7 カスケード）。
TMB トーンは回路図の実定数（Treble R47 250k/C22 150pF、Bass R48 250k/C23 0.15µF・C24 0.022µF、Mid R49 25k、slope R46 120k）を Fender 伝達関数に写像。両チャンネルとも非線形段はオーバーサンプリング領域内。OS 既定を 4x に変更。パワー段/リバーブは未実装（恒等）。

### DspTest — ALL PASSED
- 基盤（無音・Bypass ビット一致・入力ドライブで出力増加・mono→stereo 両ch 一致）
- Burn ゲイン構造: Gain Low→High で 220Hz THD **2.5% → 25.3%**
- **Burn は Vintage より飽和（noon THD: Vintage 1.6% / Burn 18.8%）**
- Burn TMB: Treble +10.4dB / Middle +10.7dB / Bass +5.4dB（各帯域に作用）
- Burn フルゲインで有限・有界
- Vintage 系（トーン・倍音・Fat）継続合格、OS 8x で 18kHz 折返し −100dB
- 安定性（バイパス無クリック・極端設定・2 分オートメーション）、メーター、全プリセット

### PassthroughTest / Vst3HostTest — ALL PASSED
- Bypass ビット完全素通し、DAW 相当ホストで各レイアウト有限・非無音、状態ラウンドトリップ、エディタ起動

### CpuBenchmark（Intel i3-8145U, 3 s/case）
- フルドライブ Vintage, 8x OS: 48kHz ≈ 5.8% / 96kHz ≈ 11.5%（1 コア比）。既定は 4x。

### 未校正（Phase 9）
- 真空管デバイス係数・ポットのカーブ・各段の絶対動作点は実機 TP 電圧と未照合。パワー段・OT・キャビ・リバーブは Phase 5–7。
