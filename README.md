# SakuraOS (v6.1)

[🇯🇵 日本語の解説へ移動](#-sakuraos-v61-日本語版)

---

## Important Notices (English)
* **AI Collaboration:** This is a collaborative project where I came up with the original ideas and developed the system in partnership with AI.
* **Language Support:** Please be aware that the system user interface (UI), display text, and all source code comments (notes written after `//`) are only available in Japanese. We sincerely apologize for any inconvenience this may cause.
* **Future Plan:** We are planning to support an English interface in future updates!

---

## About SakuraOS

SakuraOS is an original, lightweight CUI (Command-Line Interface) operating system designed to run on the ESP32 microcontroller board. It allows users to execute operations by entering text commands via the PC's serial monitor or terminals like Tera Term, utilizing an SD card for local storage.

### Hardware Component Requirements
* **Microcontroller**: FREENOVE ESP32 WROOM Development Board (with ESP32-WROOM-32E)
* **SD Card Module**: KKHMF SPI Interface Micro SD Card Shield Module (Model: 000443)
* **RTC (Real-Time Clock)**: ELEGOO DS1307-Module-V (I2C communication / SQW pin is not used)
* **Audio Amplifier**: TVETE MAX98357A I2S Audio Amplifier Module
* **Speaker**: Any simple 2-pin speaker (connected directly to the MAX98357A module)
* **LED**: Standard white LED (connected in series with a 220Ω resistor)
* **Buzzer**: Piezoelectric buzzer (capable of 5V PWM frequency control)
* **Touch Sensor**: A simple bare wire connected to Pin 4 (triggered by direct skin contact)

### Core Command List
* `help` : Displays the full command reference list.
* `sys.info` : Prints current operating system specifications.
* `sys.taskinfo` : Displays active FreeRTOS task statistics and stack margins.
* `speaker.playwav [Filename]` : Initiates playback for a specific `.wav` file on the SD card.
* `file.edit [Path/Filename]` : Launches the built-in "Cherry Blossom Text Editor" to draft or alter files.

*For more details on pin maps and configurations, please check the enclosed Japanese documentation or use a translation tool on this repository.*

---
---

# SakuraOS (v6.1) 【日本語版】

## 注意事項
* **AIとの共同開発:** このプロジェクトは、私がアイディアを出し、AIと共同で開発を進めたものです。
* **画面表示について:** OS内の表示、およびプログラム内のコメント（`//` の後ろのメモ）は日本語のみとなっています。

---

## 動作環境

### ハードウェア構成
* **マイクロコントローラ**: FREENOVE ESP32 WROOM 開発ボード (ESP32-WROOM-32E搭載)
* **SDカードモジュール**: KKHMF SPI インターフェイス マイクロSDストレージボード (型番: 000443)
* **RTC (リアルタイムクロック)**: ELEGOO DS1307-Module-V (I2C通信 / ※SQWは未使用)
* **オーディオアンプ**: TVETE MAX98357A I2S オーディオアンプモジュール
* **スピーカ**: 2極接続の簡易スピーカ (MAX98357Aに接続)
* **LED**: 砲弾型白色LED (220Ω抵抗を直列接続)
* **ブザー**: 圧電ブザー (5V PWM制御)
* **タッチセンサ**: 4番ピンに接続したむき出しのワイヤ（直接手で触れて操作）

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

## 初期設定
* **初期ログインパスワード**: `SakuraOS`
* **初期音量**: `70`
* **製作者**: EITAKU
