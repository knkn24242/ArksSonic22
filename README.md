# Arks Sonic 22 (Arks Plugin) v0.1.0

高忠実度の真空管アンプ・シミュレータ。リファレンス実機は **Fender Super-Sonic 22 Combo**
（純正サービス回路図 0078327000 Rev A）。公開回路図と公式資料に基づいて回路挙動をモデル化する。
JUCE 9.0.3 / C++17。仮称 "Arks Sonic 22"。

> 本プラグインは実機を参考にした独自製品です。Fender／Super-Sonic は各社の商標であり、
> 本プロジェクトとの公式な関係はありません。ロゴ・製品画像・独自グラフィック素材は使用しません。

- 動作環境(AU): macOS 10.13 以降(Intel) / macOS 11 以降(Apple Silicon)
- 形式: AU(Logic Pro 用、macOS でビルド)、VST3 / Standalone(Windows での開発・テスト用)
- コード: メーカー `Arks` / プラグイン `Ss22` / Bundle ID `com.arksplugin.arkssonic22`

## 開発状況（指示書の Phase 1–9）

| Phase | 内容 | 状態 |
|---|---|---|
| 1 | 資料調査・回路解析 | 完了（[docs/](docs/)） |
| 2 | DSP 基盤・入出力/パラメータ管理・自動テスト | 完了（[docs/TEST_RESULTS.md](docs/TEST_RESULTS.md)） |
| 3 | Clean(Vintage) チャンネル | 完了（[docs/TEST_RESULTS.md](docs/TEST_RESULTS.md)） |
| 4 | Burn チャンネル | 完了（[docs/TEST_RESULTS.md](docs/TEST_RESULTS.md)） |
| 5 | パワーアンプ（位相反転・6V6 PP・OT・NFB） | 未着手 |
| 6 | スピーカー / キャビネット | 未着手 |
| 7 | スプリング・リバーブ | 未着手 |
| 8 | GUI・プリセット仕上げ | 未着手 |
| 9 | 実機比較・最適化 | 未着手 |

Phase 2 ではプラグインの土台のみを実装する。回路モデルはまだ恒等（素通し）で、I/O ゲイン・
バイパス（クリックレス）・メーター・パラメータ管理・オーバーサンプリング枠組み・自動テストを用意する。
各回路ブロックは Phase 3 以降、独立クラスとしてオーバーサンプリング領域内に差し込む（[Source/AmpEngine.h](Source/AmpEngine.h)）。

## ドキュメント（Phase 1 調査成果）

| ファイル | 内容 |
|---|---|
| [docs/research.md](docs/research.md) | 製品仕様・真空管構成・コントロール配置・リバーブ（VERIFIED 分類付き） |
| [docs/circuit_analysis.md](docs/circuit_analysis.md) | 回路図からの実定数・信号経路（段ごと） |
| [docs/known_unknowns.md](docs/known_unknowns.md) | 確認済み/未確認の分類と検証方法 |
| [docs/references.md](docs/references.md) | 参照資料・出典 |
| [docs/assets/ss22_schematic.pdf](docs/assets/ss22_schematic.pdf) | 純正サービス回路図（根拠資料） |

## コントロール

実機コントロール:
- **Channel**: Vintage / Burn
- **Vintage**: Volume / Treble / Bass / **Fat**（Middle なし）
- **Burn**: Gain 1 / Gain 2 / Treble / Bass / Middle / Volume
- **Reverb**（共有）

シミュレータ独自（実機には無い補助）:
- **Input / Output**（レベル）、**Oversampling**（Off/2x/4x/8x、Phase 3 以降で使用）、**Bypass**、入出力メーター＋オーバーロード表示

## ビルドとテスト（Windows: 開発/テスト）

JUCE は隣の `SimpleSpectrumAnalyzer/external/JUCE`（9.0.3）を自動参照。

```bash
cmake -B build -DAS22_BUILD_TESTS=ON
cmake --build build --config Release
```

テスト:
- `PassthroughTest` — バイパス/恒等の素通し（mono/stereo/mono→stereo、各SR）、状態保存、バス構成
- `DspTest` — 無音・ゲイン・mono→stereo・SR 不変・クリック無し・メーター・プリセット・長時間安定
- `CpuBenchmark` — processBlock の CPU 負荷
- `Vst3HostTest "<...>/Arks Sonic 22.vst3"` — DAW 相当のホストでロード/バス/バイパス/状態/エディタ

測定ツール: [Tools/measure.py](Tools/measure.py)（THD・周波数応答・エイリアス、Phase 3 以降で使用）。

## Mac での AU ビルド

```bash
bash build_mac.sh
```
Xcode CLT + CMake + git が必要。JUCE 9.0.3 を取得し、ユニバーサル AU をビルド・インストールして `auval` で検証する。
