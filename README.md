# GR-PEACH Analog Modeling Synth — Phase 4（音生成・音出し専任構成）

## 📌 今回の変更：オーディオ出力をSSIF5生レジスタ版からSSIF0+R_BSP版に切替

別チャット「GY-5102へのSSIF音声出力検証」で、**SSIF0 + Renesas公式
R_BSP_Ssifドライバ（DMAベース、d-kato/mbed-gr-libs由来）** の組み合わせで
実機の音出しに成功したことが確認されました。そのため`main.cpp`が使う
オーディオ出力を、動作実績のあるこちらに切り替えました（新規追加：
`codec/Pcm5102Ssif0Output.h`, `R_BSP/`一式）。

**この切替により、当初order.txtでご指定いただいた「mbed-osへの依存を
減らし将来のFreeRTOS移行に備える」というポータビリティ方針からは、
一旦後退しています。** R_BSP_Ssifはmbed-osのAPI（Thread、Semaphore等）に
依存するライブラリです。動作確認が取れたコードを優先した判断ですが、
将来的にポータビリティを再度優先したい場合は、`ssif/Ssif5Driver.h`
（SSIF5を生レジスタで直接叩く、mbed-os非依存の実装）が引き続き
プロジェクト内に残っています。こちらはSSIFSR.TDEビット位置の修正
（bit0→bit16）まで反映済みですが、その修正後の実機再検証はまだ
行われていません。

音の出力に必要なオーディオバッファは、DMA転送のためにCPUキャッシュを
経由しない`NC_BSS`セクションへ配置する必要があります
（`codec/Pcm5102Ssif0Output.h`内の`g_audioBuffers`参照）。この対応も
別チャットでの検証結果を反映したものです。

---

`synth_design_review.md` セクション4.2の検討結果をベースに、GR-PEACHを
**音生成・音出し専任**とした構成です。パネル入力（エンコーダー/OLED/タクト
スイッチ）は一切持たず、それらは別のコントロール系ボード/PC側に分離される
前提とし、GR-PEACHはMIDI（USB）を受けて音を鳴らすことに専念します。

## 使用ハードウェア

| 部品 | 用途 | 接続 |
|---|---|---|
| PCM5102モジュール | ステレオアナログ出力 | I2S（SSIF0、R_BSP_Ssif経由） |
| USB0 | PCとの通信・電源、USB-MIDI受信、USBシリアルデバッグ | USB |
| USER_BUTTON0 | テストノート再生（MIDI無しで音声パス確認） | オンボード |
| USER_LED (P6_12) | ハートビート | オンボード |
| LED_RED/GREEN/BLUE (P6_13/14/15) | 用途未定（初期化のみ、消灯のまま） | オンボード |

## 🎉 SSIF5の配線について — 全ての未確認事項が解消しました（参考情報として保持）

**現在デフォルトではSSIF0+R_BSP版を使用しているため、以下のSSIF5関連情報は
直接は使われていません。** ただし`ssif/Ssif5Driver.h`は将来のポータビリティ
対応のためプロジェクト内に残してあるため、参考情報として残しています。

「RZ/A1H Software Package for GR-PEACH V1.70」（公式ソフトウェアパッケージ）と、

GR-PEACHの回路図（`X28A-M01-E/F`, `x28am01ef.pdf`）の両方をご提供いただき、
実際に解析した結果、SSIF5まわりの配線・レジスタ情報は**すべて確認できました**。

### CONFIRMED（一次資料から直接確認、推測ではない）

| 項目 | 値 | 出典 |
|---|---|---|
| SSIF5レジスタベースアドレス | `0xE820D800` | Renesasパッケージ `ssif_iodefine.h` |
| SSIF各レジスタのオフセット | SSICR=0x00, SSISR=0x04, SSIFCR=0x10, SSIFSR=0x14, SSIFTDR=0x18, SSIFRDR=0x1C, SSITDMR=0x20 等 | 同上（`struct st_ssif`を実際にコンパイルし`offsetof()`で算出） |
| SSICR/SSIFCR各ビット位置 | CKS=30, CHNL=22, DWL=19, SWL=16, SCKD=15, SWSD=14, DEL=8, TEN=1, REN=0 / TIE=3, RIE=2, TFRST=1, RFRST=0 | Renesasパッケージ `ssif.h`（`SSIF_CR_SHIFT_*`, `SSIF_FCR_SHIFT_*`） |
| SSIF5クロック供給制御 | CPG.STBCR11（`0xFCFE0440`）のbit0(`MSTP110`)を0で供給 | Renesasパッケージ `ssif.c` |
| SSIF5ソフトウェアリセット | CPG.SWRSTCR1（`0xFCFE0460`）のbit1(`SRST11`)を1→0パルス | 同上 `SSIF_Reset()` |
| SSIF5のチップレベルピン割り当て | P2_4=SSISCK5, P2_5=SSIWS5, P2_6=SSIRxD5, P2_7=SSITxD5（Alt Mode 4） | Renesasパッケージ `ssif_portsetting.c`（RSK向けコードだがチップ仕様として確認） |
| **GR-PEACH基板上での実際の引き出し先** | **CN11コネクタに、Arduino互換拡張ピン D20(SCK)/D21(WS)/D22(RxD)/D23(TxD) として実際に配線されている** | **GR-PEACH回路図 `x28am01ef.pdf` SHT.4「Expansion I/F」・SHT.6「PINMAP」** |
| GPIO/PFCレジスタのベースアドレス | ベース`0xFCFE3004` | Renesasパッケージ `gpio_iodefine.h`（同様に`offsetof()`で算出） |
| SSIF5のクロックソース | オンボードのAUDIO_X1水晶を使用可能（外部AUDIO_CLK不要） | Renesasパッケージ `ssif_if.h`の`ssif_chcfg_cks_t` |

回路図の該当箇所には、P2_4〜P2_7のCPUピン説明にそのまま
`SSISCK5`/`SSIWS5`/`SSIRxD5`/`SSITxD5`という表記があり、かつSHT.6の
PINMAPシートでCN11が「Arduino Compatible Pin Socket/Header」として
D20〜D23にこれらの信号を割り当てていることが明記されています。
これで前回「GR-PEACH基板上での実際の引き出し先が未確認」としていた
最後の項目が解消しました。`ssif/ChipRegs.h`のコメントに反映済みです。

### まだ残っている軽微な確認事項

| 項目 | 状況 |
|---|---|
| CN11上の物理ピン番号(1-8)の並び順 | 回路図のテキスト抽出では表の行列が崩れており、D20-D23がCN11のどのピン番号(1-8)に対応するか正確に復元できませんでした。実際の配線時は回路図の図面そのもの、または基板のシルク印刷でご確認ください。 |
| SSIFSR（FIFOステータスレジスタ）のビット位置 | 公式パッケージのSSIFドライバがDMA転送のみを行いSSIFSRを一度も参照していないため未確認のままです。本プロジェクトはポーリング実装のため`kSsifsrBitTDE`（bit0という暫定値）に依存しています。無音の場合はまずここを疑ってください。 |
| AUDIO_X1水晶の明示的な有効化が必要か | `CPG_SWRSTCR1`のbit7(`AXTALE`)の操作コードがパッケージ内に見つかりませんでした。オンボードWM8978がSSIF0で同じAUDIO_X1を使っている実績から通常起動で有効化されている可能性が高いですが、無音の場合は疑ってください。 |

## SSI5 ↔ PCM5102 結線対応表（更新版）

| SSIF5信号 | チップ側ピン | GR-PEACH側コネクタ/ピン | 役割 | PCM5102側ピン |
|---|---|---|---|---|
| SSISCK5 | P2_4 | **CN11 / D20** | ビットクロック(BCK) | BCK |
| SSIWS5 | P2_5 | **CN11 / D21** | ワードセレクト(LRCK) | LRCK |
| SSITxD5 | P2_7 | **CN11 / D23** | 送信データ | DIN |
| SSIRxD5 | P2_6 | CN11 / D22（本用途では未使用） | 受信データ | — |
| （AUDIO_X1使用のため不要） | — | — | システムクロック | SCK（GND接続でPCM5102内部PLL自動生成モードを推奨） |
| GND | — | — | グラウンド | GND |
| VIN | — | — | 電源 | VIN（モジュールの許容電圧を確認） |
| （未使用） | — | — | フォーマット選択 | FMT（多くのモジュールでI2S固定済み、実配線確認） |
| （未使用） | — | — | デエンファシス | DEMP（通常GND固定） |
| （未使用） | — | — | ソフトミュート制御 | XSMT（通常VIN側プルアップ済み） |
| （未使用） | — | — | フィルタ選択 | FLT（通常GND固定） |

## 🔧 ビルドエラー修正（USBMIDIのAPI差異）

前回のビルドで以下の2エラーが発生していました。

```
error: no viable conversion from 'Callback<void (MIDIMessage)>' to 'Callback<void ()>'
error: no member named 'pitchbend' in 'MIDIMessage'
```

### 原因

- **mbed classicのUSBMIDI**（多くのCookbook記事に載っている書き方）は
  `void attach(void (*fptr)(MIDIMessage))` という「MIDIMessageを直接渡す」
  コールバックでしたが、**mbed-os 6のUSBMIDIはAPIが変更**されており、
  `void attach(mbed::Callback<void()> callback)` という「引数なしの通知
  コールバック」になっています。実際のメッセージは、通知を受けてから
  `bool read(MIDIMessage*)` を呼んで個別に取り出す設計です
  （`readable()`で読み出し可能かどうかも確認可能）。公式リファレンス
  （mbed-os v6.16 USBMIDI Class Reference）で確認済みです。
- ピッチベンドの値取得は `msg.pitch()` が正しいメソッド名でした
  （`pitchbend()`という名前は存在しません）。

### 修正内容

`usb/UsbMidiInput.h`を全面的に書き直し、`attach()`には空の通知コールバックを
渡し、`poll()`（メインループから周期呼び出し）の中で`readable()`/`read()`を
使って実際のMIDIMessageを取り出しディスパッチする方式に変更しました。
`pitch()`への修正も反映済みです。

## ポータビリティ方針（mbed-osへの依存を最小化）

- **`ssif/Ssif5Driver.h` / `ssif/ChipRegs.h` / `ssif/Ssif5Regs.h`**: クロック
  供給（CPG.STBCR11）・ソフトウェアリセット（CPG.SWRSTCR1）・ピン機能設定
  （GPIO.PFC2等）・SSIF5本体の制御（SSICR等）まで、すべて`<mbed.h>`を
  一切includeしないvolatileポインタ直接アクセスで実装しています。
  FreeRTOSへ移行する際もこの3ファイルはほぼそのまま持っていけます。
- **`usb/UsbMidiInput.h`**: USB2.0デバイススタックの自前実装は非現実的なため、
  ここだけはmbed-osの`USBMIDI`クラスに意図的に依存しています。将来の移行では
  [TinyUSB](https://github.com/hathach/tinyusb)への置き換えを推奨します。
- **`dsp/*`**: シンセDSPコア一式はハードウェア非依存の標準C++のみで構成。
- LED/ボタン等の単純GPIOは`mbed.h`の`DigitalOut`/`DigitalIn`/`InterruptIn`の
  ままにしています。

## USB0の複合デバイス化について（既知の制約）

mbed-osの標準`USBMIDI`クラスと`USBSerial`(CDC)クラスは、それぞれ単独では
提供されていますが、標準構成では1つのUSBデバイスとして両方を同時に
enumerateさせる複合(コンポジット)クラスは用意されていません。本プロジェクトでは
`USBMIDI`のみを実装しています。複合化には`USBDevice`基底クラスを直接継承し、
両クラスのディスクリプタを手動で統合する実装が必要です（TODO）。

## テストノート機能（USER_BUTTON0）

MIDI入力なしで音声パスの動作確認ができるよう、USER_BUTTON0を押すと
`dsp/SoundDefaults.h`の`SYNTH_TEST_NOTE_MIDI_NUMBER`（デフォルト: 60=C4）を
`SYNTH_TEST_NOTE_DURATION_MS`（デフォルト: 500ms）だけ発音します。
ボタンの割り込み極性（アクティブLow前提で`fall()`にアタッチ）は実機で
要確認です。

## 音色パラメータの調整方法

パネルUIを持たない構成のため、音作りは`dsp/SoundDefaults.h`の`#define`値を
直接編集し、再ビルドする運用です。USB MIDIのProgram Changeで`PatchBank`内の
16プリセット（現状は全て`SoundDefaults.h`の値と同一）を切り替えることも可能です。

## ディレクトリ構成

```
gr-peach-synth/
├── mbed_app.json           # ビルド設定（サンプルレート・ボイス数・USBDEVICE有効化）
├── main.cpp                 # エントリーポイント
├── dsp/                      # ハードウェア非依存のシンセDSPコア
│   ├── SynthConfig.h
│   ├── SoundDefaults.h       # ★音色パラメータのdefine一元管理
│   ├── Oscillator.h / Filter.h / EnvelopeLfo.h / ModMatrix.h
│   ├── Patch.h / Voice.h / VoiceAllocator.h / Arpeggiator.h / Effects.h
│   └── SynthEngine.h
├── hal/
│   └── AudioOutputHAL.h      # オーディオ出力の抽象インターフェース
├── ssif/                      # 【現在未使用・将来のポータビリティ対応用に保持】
│   ├── ChipRegs.h             # CPG/GPIO(PFC)の実アドレス定義（公式パッケージ+回路図で確認済み）
│   ├── Ssif5Regs.h            # SSIF5レジスタオフセット/ビット定義（公式パッケージ+公式HWマニュアルで確認済み）
│   └── Ssif5Driver.h          # SSIF5生レジスタドライバ（mbed-os完全非依存、実機再検証待ち）
├── codec/
│   ├── Pcm5102Ssif0Output.h   # ★現在使用中★ SSIF0+R_BSP_Ssif版（実機動作確認済み）
│   └── Pcm5102Ssif5Output.h   # SSIF5生レジスタ版（現在未使用、ssif/Ssif5Driver.hを使う側）
├── R_BSP/                     # Renesas公式SSIFドライバ一式（d-kato/mbed-gr-libsより同梱）
├── usb/
│   └── UsbMidiInput.h         # USB MIDI入力（mbed-osのUSBMIDI使用、API修正済み）
└── board/
    ├── BoardConfig.h          # LED/ボタン/USB0/SSIF5ベースアドレス(現在未使用)等の定義
    └── InitTrace.h            # pinmapエラー切り分け用のシリアルトレースヘルパー
```

## Keil Studioへのインポート手順

1. Keil Studioで「Import Project」→ このフォルダ（`gr-peach-synth/`）を指定
2. ターゲットボードとして **GR-PEACH (RZ_A1H)** を選択
3. `mbed-os`ライブラリが未取得の場合はKeil Studioの依存解決機能で追加
4. ビルド後、実機のシリアルコンソールで`[init]`ログを確認する

## 未実装・今後の課題

- CN11上のD20-D23の物理ピン番号(1-8)を回路図の図面または基板シルクで確認
- SSIFSRのビット位置確認、またはDMA化（`renesas/drivers/r_ssif/src/lld/ssif_dma.c`が参考実装）
- USB0でのUSB Serial + USB MIDI複合(コンポジット)デバイス化
- microSDへのパッチ保存・読込（本構成では未着手）
- LED_RED/GREEN/BLUE（P6_13-15）の用途確定
- AUDIO_X1水晶の明示的な有効化が必要かどうかの実機確認
