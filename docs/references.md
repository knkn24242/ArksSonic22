# 参照資料（references.md）

Fender Super-Sonic 22 Combo のモデル化にあたって参照した資料の一覧。
各資料には信頼度の目安を付す。一次資料（メーカー公式・純正サービス回路図）を最優先とする。

## 一次資料（最優先）

| # | 資料 | 種別 | 信頼度 | 入手元 |
|---|---|---|---|---|
| R1 | **Fender Service Diagram "SUPER-SONIC 22"** 図番 `0078327000` Rev A（2010-01-08、全3枚） | 純正回路図 | ★★★ 一次 | thetubestore.com にホストされた PDF を取得（`docs/assets/ss22_schematic.pdf` に保存） |
| R2 | Fender 公式製品ページ Super-Sonic™ 22 Combo | メーカー公式 | ★★★ 一次 | fender.com/products/super-sonic-22-combo |

R1 の表題ブロック:「SERVICE DIAGRAM, COMBINED (schematic) / SUPER-SONIC 22 / FENDER MUSICAL INSTRUMENTS, Corona, CA / DATABASE FILE: Z904S1.SCH」。
3 枚構成: Sheet 1=プリアンプ／リバーブ／FX ループ、Sheet 2=パワーアンプ／電源、Sheet 3=PCB レイアウト（コントロール配置確認用）。

## 二次資料（スペック確認・相互照合）

| # | 資料 | 用途 | 信頼度 |
|---|---|---|---|
| R3 | Mojotone Fender Field Guide: Super-Sonic 22 Combo | 真空管構成・年代（2010–present） | ★★ |
| R4 | Premier Guitar / MusicRadar / Audiofanzine レビュー | 操作感・リバーブ非チャンネル割当などの挙動 | ★★ |
| R5 | Amplified Parts / thetubestore（真空管セット） | 真空管本数の相互確認 | ★★ |
| R6 | Fender 取扱説明書 PDF `2160000_gamp_manual.pdf`（fmicassets.com） | パネル機能・定格 | ★★ |
| R7 | Yeh & Smith, "Discretization of the '59 Fender Bassman Tone Stack", DAFx-06（CCRMA） | トーンスタック伝達関数の導出 | ★★★ |
| R8 | Faust `tonestacks` ライブラリ（grame-cncm/faustlibraries） | Fender トーンスタック係数の閉形式・相互確認 | ★★ |

## スピーカー・リバーブ・トランス部品番号（R1 より）

- 出力トランス OT: P/N `0041318000`
- 電源トランス（国内用）: P/N `0078334000`（輸出用 100V/230V/240V は別番号、Sheet 2 に記載）
- リバーブ・パン: P/N `0084083000`（タンク型番 `4AB3C1B`）
- スピーカー: 12", 50W, 8Ω Fender "Lightning Bolt"（Eminence 製）— R2 記載。Celestion ではない。

## 注意

- ManualsLib 版サービスマニュアルは閲覧時に信頼性の懸念があるとのフォーラム指摘があり、採用しない。回路値はすべて R1（純正 PDF）から直接読み取る。
- フォーラム（TDPRI, el34world 等）は背景理解の参考に留め、回路定数の根拠としては使わない。
