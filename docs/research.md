# リサーチ結果（research.md）

対象機: **Fender Super-Sonic 22 Combo**（モデル化のリファレンス実機）。
本プロジェクトは実機をクローンするのではなく、公開サービス回路図と公式資料に基づいて回路挙動を忠実にモデル化する。
各項目に信頼度分類を付す。

- **VERIFIED**: 純正回路図（R1, 図番 0078327000 Rev A）または公式資料（R2）で確認済み
- **INFERRED**: 複数資料から推定
- **APPROXIMATED**: 実装上の近似
- **UNVERIFIED**: 未確認（捏造しない）

出典記号は [references.md](references.md) 参照。

## 1. 製品概要

| 項目 | 内容 | 分類 | 出典 |
|---|---|---|---|
| 正式名称 | Fender Super-Sonic 22 Combo | VERIFIED | R1,R2 |
| 年代 | 2010–present（回路図 Rev A: 2010-01-08） | VERIFIED | R1,R3 |
| 出力 | 22 W（8Ω） | VERIFIED | R2 |
| パワー段 | 2× 6V6GT プッシュプル（"Deluxe Reverb パワー・プラットフォーム"） | VERIFIED | R1,R2 |
| 整流 | ソリッドステート（真空管整流ではない） | VERIFIED | R1,R2 |
| 真空管（プリ） | 3× 12AX7（V1,V2,V4）＋ 2× 12AT7（V3,V5） | VERIFIED | R1 |
| 真空管（パワー） | 2× 6V6GT（V6,V7） | VERIFIED | R1,R2 |
| スピーカー | 12" / 50W / 8Ω Fender "Lightning Bolt"（Eminence 製） | VERIFIED | R2 |
| リバーブ | 4スプリング・タンク `4AB3C1B`（P/N 0084083000）、真空管ドライブ | VERIFIED | R1,R2 |
| チャンネル | 2ch（Vintage / Burn）＋共有スプリング・リバーブ | VERIFIED | R1,R2 |
| 寸法/重量 | 約 17.5"H×24"W×8.5"D / 約 40 lb | INFERRED | R2,R4 |

## 2. 真空管の役割（V1〜V7）

回路図 Sheet 1/2 の管番号から確定。

| 管 | 型 | 役割 | 分類 |
|---|---|---|---|
| V1 | 12AX7A | V1-A=入力初段（Vintage/Burn 共用）、V1-B=第2段（Vintage は共用、Burn は Gain1 後段） | VERIFIED |
| V2 | 12AX7A | V2-A=Burn 第3ゲイン段、V2-B=出力ドライバ段（両ch 共用、Volume 前） | VERIFIED |
| V3 | 12AT7 | リバーブ・ドライバ（スプリングタンクを駆動） | VERIFIED |
| V4 | 12AX7A | リバーブ・リカバリ（タンク出力の回収・増幅） | VERIFIED |
| V5 | 12AT7 | 位相反転（ロングテールド・ペア） | VERIFIED |
| V6,V7 | 6V6GT | 出力管（プッシュプル） | VERIFIED |

- 固定バイアス（バイアス調整 R156 "BIAS ADJ"、−43.7V/−54.5V バイアス供給）。VERIFIED
- B+ 系統: +420V（B+）/ +418V / +376V / +317V / +280V の分圧タップ。VERIFIED
- SS/リレー/FX/リバーブ用に ±15V レギュレータ（7815/7915）を別途搭載。VERIFIED

## 3. コントロール配置（フロントパネル、Sheet 3 で確定）

左から:
**INPUT** → **VOLUME**（Vintage） → **NORMAL/FAT スイッチ** → **TREBLE**（Vintage） → **BASS**（Vintage） → **VINTAGE/BURN スイッチ** → **GAIN 1**（Burn） → **GAIN 2**（Burn） → **TREBLE**（Burn） → **BASS**（Burn） → **MID**（Burn） → **VOLUME**（Burn） → **REVERB**（共有）

- Vintage チャンネル: Volume / Treble / Bass＋Fat（Middle なし＝ブラックフェイス系） VERIFIED
- Burn チャンネル: Gain1 / Gain2 / Treble / Bass / Middle / Volume（3バンド TMB） VERIFIED
- リバーブは 1 ノブ共有。チャンネル割当不可。 VERIFIED（R1）/ 挙動は R4 とも一致
- 背面: REVERB JACKS、FOOT SWITCH、FX SEND / FX RETURN（バッファード直列/並列ループ） VERIFIED

## 4. フットスイッチ

4ボタン: Channel（Vintage/Burn）、Fat On/Off、Reverb On/Off、Effects Loop On/Off。VERIFIED（R2）。
DIN ピン配列は Sheet 1 に記載（PIN1=GND, 2=NORM/FAT, 3=FX, 4=VINT/BURN, 5=RVB, 7=TYPE SELECT）。VERIFIED（R1）

## 5. リバーブ

- 本物のスプリング・リバーブ。タンク `4AB3C1B`（4=ロングディケイ, 入出力インピーダンス規定）。VERIFIED
- V3（12AT7）＋オペアンプ（4560 系）でドライブ、V4（12AX7）でリカバリ。VERIFIED
- デジタル・ホール系ではない。モデル化はスプリング特有の分散・多重反射・金属的な減衰を対象とする。

## 6. 周波数特性・歪み特性の実測データ

- 公開された実機の周波数応答・THD スペクトルの一次測定データは**未入手**。UNVERIFIED。
- 検証は回路図からのシミュレーション値を基準とし、実機実測が得られた時点で突き合わせる（[known_unknowns.md](known_unknowns.md) 参照）。

## 7. 本プロジェクトで「実機回路」と「シミュレータ独自処理」の区別

- 実機回路由来: 入力ロード、各真空管段、Vintage/Burn トーンスタック、Fat、位相反転、6V6 プッシュプル、OT、NFB、スプリング・リバーブ、FX ループ。
- シミュレータ独自（実機に無い補助処理）: オーバーサンプリング、DC サーボ/ブロッカ、I/O ゲイン・メーター、測定用バイパス、プリセット管理。これらは GUI 上も実機コントロールと区別する。
