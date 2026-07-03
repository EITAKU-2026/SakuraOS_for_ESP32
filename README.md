# SakuraOS (v6.1)

ESP32開発ボード上で動作する、シリアルCLI（Command-Line Interface）を採用した独自CUIオペレーティングシステムです。PCのシリアルモニタやTera Termなどからコマンドを入力して操作し、ストレージとしてSDカードを使用します。

---

## 動作環境

### ハードウェア構成
* **マイクロコントローラ**: FREENOVE ESP32 WROOM 開発ボード (ESP32-WROOM-32E搭載)
* **SDカードモジュール**: KKHMF SPI インターフェイス マイクロSDストレージボード (型番: 000443 / ASIN: B083DT3LQK)
* **RTC (リアルタイムクロック)**: ELEGOO DS1307-Module-V (I2C通信 / ※SQWは未使用)
* **オーディオアンプ**: TVETE MAX98357A I2S オーディオアンプモジュール (ASIN: BFF4QD7T4)
* **スピーカ**: 2極接続の簡易スピーカ (MAX98357Aに接続)
* **LED**: 砲弾型白色LED (220Ω抵抗を直列接続)
* **ブザー**: 圧電ブザー (5V PWM制御)
* **タッチセンサ**: 4番ピンに接続したむき出しのワイヤ（直接手で触れて操作）

### 使用ライブラリ

| プログラム内ヘッダ | 正式名 | 提供元 |
| :--- | :--- | :--- |
| `Arduino.h` | Arduino Core | Arduino / Espressif |
| `freertos/FreeRTOS.h` | FreeRTOS | Real Time Engineers Ltd. |
| `freertos/task.h` | FreeRTOS | Real Time Engineers Ltd. |
| `freertos/queue.h` | FreeRTOS | Real Time Engineers Ltd. |
| `AudioGeneratorWAV.h` | ESP8266Audio | earlephilhower氏 |
| `AudioOutputI2S.h` | ESP8266Audio | earlephilhower氏 |
| `AudioFileSourceSD.h` | ESP8266Audio | earlephilhower氏 |
| `FS.h` | FS | Espressif |
| `SD.h` | SD | Arduino公式 |
| `SPI.h` | SPI | Arduino公式 |
| `Wire.h` | Wire | Arduino公式 |
| `RTClib.h` | RTClib | Adafruit |

---

## ピン接続表

> **注意**: すべてのパーツのGNDは共通（共通GND）にしてください。

| ハードウェア | ESP32ピン番号 | 備考 / 接続方法 |
| :--- | :---: | :--- |
| **LED** | 13 | 220Ω抵抗を直列に接続 |
| **圧電ブザー** | 17 | |
| **RTC (SDA)** | 33 | 電源: 5V |
| **RTC (SCL)** | 32 | |
| **SDカード (MISO)** | 19 | 電源: 5V |
| **SDカード (MOSI)** | 23 | |
| **SDカード (SCK)** | 18 | |
| **SDカード (CS)** | 5 | |
| **アンプ (BCLK)** | 26 | 電源: 3.3V |
| **アンプ (LRC/LRCK)**| 25 | |
| **アンプ (DOUT/DIN)** | 15 | |
| **アンプ (GAIN)** | - | 100kΩ抵抗を介して3.3Vに接続 |
| **アンプ (SD)** | - | 未接続（宙ぶらりんでOK） |
| **タッチセンサ** | 4 | 被覆を剥いたワイヤ1本のみ接続 |

---

## 主要コマンド一覧

OS起動後、シリアルモニタから以下のコマンドを入力して操作します。詳細な一覧は `help` コマンドで画面に表示可能です。

### システム・情報
* `help` : コマンド一覧を表示します。
* `sys.info` : OSの情報を表示します。
* `sys.load` : ESP32のメモリ使用量を簡易表示します。
* `sys.taskinfo` : FreeRTOSの各タスク状態、優先度、スタック残量等を表示します。
* `sys.uptime` : 起動してからの駆動時間を表示します。
* `sys.checksetting` : 現在の設定情報（音量やパスワード状態）を表示します。
* `sys.lock` : OSをロックします（解除にはログインパスワードが必要）。

### ハードウェア制御
* `led.on` / `led.off` : LEDを点灯 / 消灯します。
* `buzz.beep [周波数]` : 指定した周波数でブザーテストを行います。
* `sys.touchcheck` : タッチセンサの値を実測し、閾値調整のための数値を表示します。
* `sys.touchsetting [閾値]` : タッチセンサの閾値を設定します。

### サウンド（WAV再生）
* `speaker.playwav [ファイル名]` : SDカード内の指定した `.wav` ファイルを再生します。
* `speaker.stopwav` : 再生中の `.wav` ファイルを停止します（タッチセンサ接触でも停止可能）。
* `sys.volume [0〜100]` : システム全体の音量を調節します。

### ストレージ・ファイル操作（SDカード）
* `dir.list` : 現在のディレクトリ内のファイル一覧を表示します。
* `dir.now` / `dir.move [パス]` : 現在の階層表示 / ディレクトリの移動を行います。
* `file.open [ファイル名]` : テキストファイルの内容を表示します。
* `file.edit [パス/ファイル名]` : 独自テキストエディタ「Cherry Blossom」を起動し、新規作成・再編集を行います。

---

## WAVファイルの再生条件
アンプから正常に音声を再生するために、音声ファイルは **Audacity** 等を使用して以下の条件で作成してください。
* **ファイル種類**: WAV (Microsoft)
* **エンコーディング**: Signed 16-bit PCM
* **サンプリング周波数**: 22050 Hz
* **チャンネル**: モノラル
* **その他**: アーティスト名などの「タグ情報」はすべて削除してください（雑音の原因になります）。

---

## 出力ファイルについて
SakuraOSはSDカード内に2つの独自拡張子ファイルを出力します（PCでは `.txt` として閲覧可能です）。
1. **`.slf` (Sakura Log File)** : コマンドの実行履歴を自動記録するログファイルです。
2. **`.ssf` (Sakura Setting File)** : パスワード（XOR暗号化済）、音量、タッチ閾値などの設定を保存するファイルです。

---

## 注意事項
私がアイディアを出し、AIと共同で開発を進めたプロジェクトです。

## 初期設定
* **初期ログインパスワード**: `SakuraOS`
* **初期音量**: `70`
* **製作者**: EITAKU
