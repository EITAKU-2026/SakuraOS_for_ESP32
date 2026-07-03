#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <AudioGeneratorWAV.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourceSD.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>

#define LED_PIN 13
#define buzz 17
//ロータリーエンコーダの接続
//5Vで駆動させること
//#define ENC_CLK 27
//#define ENC_DT 14
//#define ENC_SW 12

#define BUZZER_CHANNEL 0

volatile int sysVolume = 70;//OSの全部の音量（初期値70）

//RTC　SDA->33ピン　SCL->32ピン
//SDカードモジュール miso->19 mosi->23 sck->18 cs->5
//RTCとSDカードモジュールは5Vで給電
//アンプのピン
#define I2S_BCLK       26
#define I2S_LRCK       25
#define I2S_DOUT       15
//アンプモジュールだけは3.3Vで給電すること
//GAINピンも3.3Vピンに直接つなぐこと。SDピンは宙ぶらりんでよい。

AudioFileSourceSD *file = nullptr;
AudioGeneratorWAV *wav = nullptr;
AudioOutputI2S *out = nullptr;

String playwav;
volatile bool wavStopRequested = false;


#define editor_buf_size 20480  //Cherry Blossom Editor 用文章本体を一時格納する変数

#define configUSE_TRACE_FACILITY 1
#define configUSE_STATS_FORMATTING_FUNCTIONS 1  //taskinfo用の変数->これがないとvTaskList(buffer);が動かなくなる

#define TOUCH_PIN 4
volatile int touch_threshold = 600; //タッチセンサのピン番号と初期の閾値（実測が必要である）
volatile bool touchCheck = false;


//RTC　SDA->33ピン　SCL->32ピン
//SDカードモジュール miso->19 mosi->23 sck->18 cs->5

SemaphoreHandle_t globalMutex;

char editorText[editor_buf_size];
char currentFilename[128];
size_t editorLen = 0;

String currentPass = "SakuraOS";  //初期ログインパスワードそのもの
String encryptPass;//暗号化したパスワードを入れる変数
bool SoundEnabled = true;  //起動音ON(true)・OFF(false)フラグ
bool LogEnabled = true; //ログの記録ON(true)・OFF(false)フラグ

const char* sysFolder = "/SakuraOS_system";//設定情報保存ファイル入りフォルダの関数
const char* sysSettingFile = "/SakuraOS_system/SakuraOS_system_setting.ssf"; //設定情報保存ファイル（Sakura Setting File）の関数
const char* XOR_KEY = "ossaki20rora26kuhia";// XOR暗号化用共通鍵

RTC_DS1307 rtc;
char filename[128];



String currentDir = "/";


struct CommandPacket {
  int command;

  int year;    //RTC
  int month;   //RTC
  int day;     //RTC
  int hour;    //RTC
  int minute;  //RTC
  int second;  //RTC

  char filename[128];
  char text[1024];
};




QueueHandle_t commandQueue;   //Input->Worker
QueueHandle_t responseQueue;  //Worker->Input

void InputTask(void *pvParameters);
void WorkerTask(void *pvParameters);


enum SystemSound {
  startupSound,
  successSound,
  errorSound,
  warningSound,
  noticeSound,
  beepSound,
  editorstartSound,
  editorendSound,
  locksound,
  unlocksound
};

void playSystemSound(SystemSound type, int value = 0) {//システムサウンド鳴らす関数

  if (!SoundEnabled) return;

  switch (type) {

    case startupSound:
      playStartupMelody();
      break;

    case successSound:
      playTone(659, 120);
      playTone(784, 200);
      break;

    case errorSound:
      playTone(400, 200);
      playTone(300, 200);
      playTone(200, 400);
      break;

    case warningSound:
      playTone(587, 180);  // D5
      playTone(523, 220);  // C5
      break;

    case noticeSound:
      playTone(784, 80);
      playTone(659, 120);
      break;

    case beepSound:
      if (value >= 100 && value <= 5000) {
        playTone(value, 1000);
      }
      break;

    case editorstartSound:
      playTone(659, 100);
      playTone(784, 100);
      playTone(988, 180);
      break;
    
    case editorendSound:
      playTone(988, 100);
      playTone(784, 100);
      playTone(659, 200);
      break;

    case locksound:
      playTone(523,150);
      playTone(440,150);
      playTone(349,300);
      break;

    case unlocksound:
      playTone(440,100);
      playTone(523,100);
      playTone(659,200);
      break;
      

  }
}


void login() {
  String input = "";

  Serial.println(F("============SakuraOS ログイン============"));

  while (true) {
    Serial.print(F("ログインパスワードを入力してください。"));

    while (!Serial.available()) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    input = Serial.readStringUntil('\n');
    input.trim();

    if (input == currentPass) {
      Serial.println(F("ログインに成功しました。"));
      playSystemSound(successSound);
      break;
    } else {
      Serial.println(F("パスワードが異なります。"));
      playSystemSound(warningSound);
    }
  }
}


void printLogo()
{
  const char* petals[] = {"*", "゜", "✿", "❀", "＊"};
  const int WIDTH = 60;
  const int HEIGHT = 21;
  const int PETAL_COUNT = 20;
  const int TOTAL_FRAMES = 40;
  const int LOGO_APPEAR_FRAME = 39; // 中央通過タイミング

  struct Petal {
    int x;
    int y;
    int type;
    int xdrift; // 何フレームごとにxを+1するか（斜め感）
    int driftCounter;
  };

  Petal petalList[PETAL_COUNT];

  // 花びら初期配置：左上付近にバラバラに配置
  for (int i = 0; i < PETAL_COUNT; i++) {
    petalList[i].x = random(0, WIDTH / 2);     // 左半分からスタート
    petalList[i].y = random(-HEIGHT, 0);        // 画面上からランダムにスタート
    petalList[i].type = random(0, 5);
    petalList[i].xdrift = random(2, 5);         // 2〜4フレームに1回横にずれる
    petalList[i].driftCounter = 0;
  }

  // 画面クリア・カーソル非表示
  Serial.print(F("\033[2J\033[H\033[?25l"));

  bool logoShown = false;

  for (int frame = 0; frame < TOTAL_FRAMES; frame++) {

    // 各花びらを描画・更新
    for (int i = 0; i < PETAL_COUNT; i++) {

      // 前の位置を消す（画面内にいる場合のみ）
      if (petalList[i].y >= 1 && petalList[i].y <= HEIGHT &&
          petalList[i].x >= 1 && petalList[i].x <= WIDTH) {
        Serial.print(F("\033["));
        Serial.print(petalList[i].y);
        Serial.print(F(";"));
        Serial.print(petalList[i].x);
        Serial.print(F("H "));  // スペースで上書き消去
      }

      // 位置を更新（下に落ちる）
      petalList[i].y++;

      // 横ずれ（左上→右下の斜め感）
      petalList[i].driftCounter++;
      if (petalList[i].driftCounter >= petalList[i].xdrift) {
        petalList[i].x++;
        petalList[i].driftCounter = 0;
      }

      // 画面外に出たら左上付近にリセット
      if (petalList[i].y > HEIGHT || petalList[i].x > WIDTH) {
        petalList[i].x = random(0, WIDTH / 3);
        petalList[i].y = random(-5, 0);
        petalList[i].type = random(0, 5);
        petalList[i].xdrift = random(2, 5);
        petalList[i].driftCounter = 0;
      }

      // 新しい位置に描画（画面内にいる場合のみ）
      if (petalList[i].y >= 1 && petalList[i].y <= HEIGHT &&
          petalList[i].x >= 1 && petalList[i].x <= WIDTH) {
        Serial.print(F("\033["));
        Serial.print(petalList[i].y);
        Serial.print(F(";"));
        Serial.print(petalList[i].x);
        Serial.print(F("H"));
        Serial.print(F("\033[95m"));
        Serial.print(petals[petalList[i].type]);
        Serial.print(F("\033[0m"));
      }
    }

    // ロゴを表示
    if (frame == LOGO_APPEAR_FRAME && !logoShown) {
      Serial.print(F("\033[11;15H\033[2K"));
      Serial.print(F("\033[2J"));
      Serial.print(F("\033[11;15H\033[1;95m"));
      Serial.print(F("  Sakura Operating System  "));
      Serial.print(F("\033[0m"));
      logoShown = true;
    }

    vTaskDelay(120 / portTICK_PERIOD_MS);
  }

  // カーソルを画面下に移動・カーソル再表示
  Serial.print(F("\033[22;1H\033[?25h"));
  Serial.println();
}


void saveSystemSettings() {//設定情報保存関数
  xSemaphoreTake(globalMutex, portMAX_DELAY);
  SD.remove(sysSettingFile);
  File f = SD.open(sysSettingFile, FILE_WRITE);
  if (!f) {
    xSemaphoreGive(globalMutex);
    Serial.println(F("設定ファイル保存失敗"));
    playSystemSound(errorSound);
    return;
  }

  f.print(F("SoundEnabled="));
  f.println(SoundEnabled ? 1 : 0);

  encryptPassword(currentPass, encryptPass);
  f.print(F("Password="));
  f.println(encryptPass);

  f.print(F("LogEnabled="));
  f.println(LogEnabled ? 1 : 0);

  f.print(F("Volume="));
  f.println(sysVolume);

  f.print(F("TouchThreshold="));
  f.println(touch_threshold);

  f.close();
  xSemaphoreGive(globalMutex);

  Serial.println(F("設定を保存しました。"));
  playSystemSound(successSound);

}

void loadSystemSettings() {//設定情報読み込み関数

  xSemaphoreTake(globalMutex, portMAX_DELAY);

  if (!SD.exists(sysFolder)) {
    SD.mkdir(sysFolder);
  }

  if (!SD.exists(sysSettingFile)) {
    xSemaphoreGive(globalMutex);
    saveSystemSettings(); // 初回はデフォルト保存
    Serial.println(F("設定情報保存フォルダ・ファイルを作成しました。"));
    playSystemSound(noticeSound);
    return;
  }

  File f = SD.open(sysSettingFile);
  if (!f) return;

  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();

    if (line.startsWith("SoundEnabled=")) {
      SoundEnabled = (line.substring(13).toInt() != 0);
    }

    else if (line.startsWith("Password=")) {
      encryptPass = line.substring(9);
      encryptPass.trim();
      decryptPassword(encryptPass, currentPass);

    }

    else if (line.startsWith("LogEnabled=")) {
      LogEnabled = (line.substring(11).toInt() != 0);
    }

    else if (line.startsWith("Volume=")) {
      int v = line.substring(7).toInt();
      if(v >= 0 && v <= 100) sysVolume = v;
    }

    else if (line.startsWith("TouchThreshold=")) {
      touch_threshold = line.substring(15).toInt();
    }

  }

  f.close();
  xSemaphoreGive(globalMutex);
}

// パスワードをXOR暗号化してHEX文字列に変換
void encryptPassword(const String& plain, String& hex) {
  int keyLen = strlen(XOR_KEY);
  hex = "";
  for (int i = 0; i < plain.length(); i++) {
    uint8_t c = (uint8_t)plain[i] ^ (uint8_t)XOR_KEY[i % keyLen];
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", c);
    hex += buf;
  }
}

// HEX文字列をXOR復号してパスワードに戻す
void decryptPassword(const String& hex, String& plain) {
  int keyLen = strlen(XOR_KEY);
  plain = "";
  for (int i = 0; i + 1 < hex.length(); i += 2) {
    // HEXの2文字を1バイトに変換
    char buf[3] = { hex[i], hex[i + 1], '\0' };
    uint8_t c = (uint8_t)strtol(buf, nullptr, 16);
    plain += (char)(c ^ (uint8_t)XOR_KEY[(i / 2) % keyLen]);
  }
}



void displaySettingInfo()  //設定情報表示コマンド用関数
{
  Serial.println(F("=========================================== 設定情報 =============================================="));
  if(SoundEnabled == true)
  {
    Serial.println(F("サウンド機能は有効です"));
  }
  else
  {
   Serial.println(F("サウンド機能は無効です"));
  }

  Serial.print(F("現在のログインパスワードは"));
  Serial.print(currentPass);
  Serial.println(F("です")); 
                           
  if(LogEnabled == true)
  {
   Serial.println(F("ログファイルの書き込み・作成は有効です"));
  }
  else
  {
   Serial.println(F("ログファイルの書き込み・作成は無効です"));
  }

  Serial.print(F("現在の音量は"));
  Serial.print(sysVolume);
  Serial.println(F("です。"));

  Serial.print(F("現在のタッチセンサの閾値は"));
  Serial.print(touch_threshold);
  Serial.println(F("です。"));

  Serial.println(F("=============================================================================================================="));

}


// .wav専用の再生関数：ファイル名を渡すと再生を開始する
void playWav(const char *wavname) {
  // すでに再生中なら停止してメモリを解放
  if (wav && wav->isRunning()) {
    wav->stop();
  }
  if (file) {
    file->close();
    delete file;
  }

  // 新しいファイルを開く
  file = new AudioFileSourceSD(wavname);
  if (!file->isOpen()) {
    Serial.print("ファイルが見つかりません: ");
    Serial.println(wavname);
    return;
  }

  Serial.print("再生開始: ");
  Serial.println(wavname);
  wav->begin(file, out);

}



void playTone(int frequency, int duration) {//サウンド機能すべて用
  ledcWriteTone(buzz, frequency);  //周波数

  //for (int duty = 0; duty <= 128; duty += 8)
  //{
  //ledcWrite(buzz, duty);
  //vTaskDelay(5 / portTICK_PERIOD_MS);;
  //}

  int buzzVol = map(sysVolume,0,100,0,255);
  ledcWrite(buzz, buzzVol);  //音量

  vTaskDelay(duration / portTICK_PERIOD_MS);  //一音が鳴る時間

  ledcWrite(buzz, 0);                   // 音を停止させる
  vTaskDelay(50 / portTICK_PERIOD_MS);  // 音と音の間の時間
}

void playStartupMelody() {//起動音専用

  int melody[] = {
    784,   // ソ　　　　ゆったり始まる
    880,   // ラ
    784,   // ソ　　　　ふわっと揺れる
    880,   // ラ
    988,   // シ
    1047,  // ド　　　　やさしく上がる
    988,   // シ
    880,   // ラ　　　　ふわっと戻る
    988,   // シ
    1047,  // ド
    1175,  // レ　　　　そっと頂点へ
    1047,  // ド　　　　やさしく降りる
    988,   // シ
    880,   // ラ
    784,   // ソ　　　　おっとり着地
    698    // ファ　　　余韻でほわっと終わる
  };

  int noteDurations[] = {
    350,  // ソ
    350,  // ラ
    200,  // ソ　　　　ふわっと
    200,  // ラ
    350,  // シ
    400,  // ド
    250,  // シ
    250,  // ラ
    300,  // シ
    350,  // ド
    600,  // レ　　　　頂点・たっぷり
    300,  // ド
    300,  // シ
    350,  // ラ
    400,  // ソ
    700   // ファ　　　ほわ〜っと終わり
  };

  for (int i = 0; i < 16; i++) {
    playTone(melody[i], noteDurations[i]);
  }
}





TaskHandle_t inputTaskHandle = NULL;
void loadinfo() {
  char *buffer = (char *)malloc(4096);

  if (!buffer) {
    Serial.println("メモリ確保失敗");
    return;
  }

  vTaskList(buffer);
  Serial.println(F("==================================================SakuraOS タスク監視======================================================="));
  Serial.println(F("タスク名      状態 優先度 残りスタック タスク番号 コア番号"));
  Serial.println(buffer);
  Serial.println(F("R=Running(実行中) / B=Blocked(待機中) / S=Suspended(一時停止中) / X=タスクが不正状態か存在していません。 / D=Deleted(削除済)"));
  Serial.print(F("Free Heap: "));
  Serial.println(xPortGetFreeHeapSize());
  Serial.print(F("コア1の残スタック最小値: "));
  Serial.println(uxTaskGetStackHighWaterMark(NULL));
  Serial.print(F("コア0の残スタック最小値: "));
  Serial.println(uxTaskGetStackHighWaterMark(inputTaskHandle));

  free(buffer);
}


void setup() {
  Serial.begin(115200);
  Wire.begin(33, 32);  //Wire.begin( SDAピン, SCLピン );RTCをESP32で使うならばこうする。（https://swiswiswift.com/2018-03-14/）
  pinMode(LED_PIN, OUTPUT);
  pinMode(buzz, OUTPUT);
  //pinMode(ENC_CLK, INPUT_PULLUP);
  //pinMode(ENC_DT, INPUT_PULLUP);
  //pinMode(ENC_SW, INPUT_PULLUP);

  out = new AudioOutputI2S();
  out->SetPinout(I2S_BCLK, I2S_LRCK, I2S_DOUT);
  //out->SetOutputModeInternalDAC(false); // 外部I2S(MAX98357A)を使う設定
  out->SetOutputModeMono(true); 
  out->SetGain(sysVolume / 100.0f);// * 0.5f); // wav再生音量はロータリーエンコーダで設定
  out->SetRate(22050);//音源のサンプリング周波数指定
  wav = new AudioGeneratorWAV();

  ledcAttach(buzz, 1000, 8);

  bool sdOK = SD.begin();
  bool rtcOK = rtc.begin();

  if (!SD.exists("/SakuraOS_logs"))
  {
   SD.mkdir("/SakuraOS_logs");
  }

 //ここから接続デバイス未接続警告
  if (!sdOK || !rtcOK) {
    playSystemSound(errorSound);
    while (1) {
      if (!sdOK) Serial.println(F("SDカードが認識できません。"));
      if (!rtcOK) Serial.println(F("RTCが認識できません。"));
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      Serial.println(F("電子回路の確認をしてください。"));
      vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }
  
  //ここまで接続デバイス未接続警告

  globalMutex = xSemaphoreCreateMutex();
  loadSystemSettings();

 
  //ここからSDカードサイズ確認・表示
  uint64_t cardSize = SD.cardSize();

  if (cardSize < 1024ULL * 1024ULL * 1024ULL) {
    Serial.print(F("SDカードのサイズは"));
    Serial.print(cardSize / (1024ULL * 1024ULL));
    Serial.println(F(" MBです。"));
  } else {
    Serial.print(F("SDカードのサイズは"));
    Serial.print(cardSize / (1024ULL * 1024ULL * 1024ULL));
    Serial.println(F(" GBです。"));
  }
  //ここまでSDカードサイズ確認・表示

  //ここから起動時の時刻取得＆表示
  if (LogEnabled)
  {Serial.println(F("起動時刻は以下の通りログファイルに記録します。"));}
  DateTime now = rtc.now();
  Serial.print(now.year());
  Serial.print(F("年"));
  Serial.print(now.month());
  Serial.print(F("月"));
  Serial.print(now.day());
  Serial.print(F("日"));
  Serial.print(now.hour());
  Serial.print(F("時"));
  Serial.print(now.minute());
  Serial.print(F("分"));
  Serial.print(now.second());
  Serial.println(F("秒"));
  //ここまで起動時の時刻取得＆表示

  //ここから起動時の時刻の名前のテキストファイルを作る
  if (LogEnabled)
 {
   snprintf(filename, sizeof(filename),
           "/SakuraOS_logs/%04d.%02d.%02d_%02d-%02d-%02d.slf",//Sakura Log File（.slf）でログファイル保存（中身は.txt）
           now.year(),
           now.month(),
           now.day(),
           now.hour(),
           now.minute(),
           now.second());

   xSemaphoreTake(globalMutex, portMAX_DELAY);

   File file = SD.open(filename, FILE_WRITE);

   if (!file)
   {
     Serial.print(F("ログファイルの作成に失敗しました。"));
     Serial.println(F("ログが取れないため、動作を停止します。電子回路を確認のうえ、再起動してください。"));
     playSystemSound(errorSound);
     while (1);
   }

   file.print("Boot up at ");

   if (!file.println(filename))
   {
     Serial.print(F("ログファイルの書き込みに失敗しました。"));
     Serial.println(F("ログが取れないため、動作を停止します。電子回路を確認のうえ、再起動してください。"));
     file.close();
     playSystemSound(errorSound);
     while (1);
   }

   file.close();
   xSemaphoreGive(globalMutex);
   Serial.print(F("今回のログファイル作成に成功しました。ファイル名:"));
   Serial.println(filename);
  }
  //ここまで起動時の時刻の名前のテキストファイルを作る

 //ここからログイン・起動操作
  playSystemSound(startupSound);
  printLogo();
  Serial.println(F(" "));
  Serial.println(F(" "));
  login();
  
  Serial.println(F("Sakura OS 6.1 for ESP32 Japanese Edition."));
  Serial.println(F("Welcome!"));
  Serial.println(F(" "));

  //ここまでログイン・起動操作

  Serial.println(F("このOSはシリアルモニタから半角でコマンドを入力することで、操作するCUI形式のOSです。"));
  Serial.println(F("対象とするマイコンはESP32です。"));
  Serial.println(F("製作者はあきひろです。"));
  Serial.println(F("動作中は、ログデータの消失防止のため、SDカードは絶対に抜き差ししないでください。"));
  Serial.println(F("SDカードの抜き差しは電源を切って動作を止めた状態で、行ってください。"));
  Serial.println(F("helpと入力すると、コマンド一覧を表示します。"));

  

  commandQueue = xQueueCreate(32, sizeof(CommandPacket));
  responseQueue = xQueueCreate(32, sizeof(int));

  xTaskCreatePinnedToCore(InputTask, "InputTask", 8192, NULL, 1, &inputTaskHandle, 0);
  xTaskCreatePinnedToCore(WorkerTask, "WorkerTask", 16384, NULL, 1, NULL, 1);


  randomSeed(esp_random());
}

void loop() {
}

void InputTask(void *pvParameters) {
  bool settimeMode = false;
  int setStep = 0;
  int cmd;
  bool editorMode = false;
  bool editorSaving = false;
  bool lockMode = false;
  String editorFilename;
  String editorBuffer;
  String editTarget = "";
  CommandPacket tempPacket;  //テキストエディタ用の関数

  while (1) {
    int response;

    //タッチセンサの処理
    static bool lastTouchState = false;
    bool isTouched = (touchRead(TOUCH_PIN) < touch_threshold);

   if (isTouched && !lastTouchState)  // タッチした瞬間だけ反応
    {
      if (wav && wav->isRunning())
      {
        wavStopRequested = true;
        Serial.println(F("TouchでWAVを停止しました。"));
      }
      
      if (touchCheck == true)
      {
        //変動後・変動前の値を蓄積するバッファー
        static int touchCount = 0;//検知回数カウンター
        static int sumBefore = 0;//変動前の値の合計
        static int sumAfter  = 0;//変動後の値の合計
        
        int lasttouch = touchRead(TOUCH_PIN);
        vTaskDelay(50 / portTICK_PERIOD_MS);
        int nowtouch = touchRead(TOUCH_PIN);
        int diftouch = abs(lasttouch - nowtouch); // 絶対値で両方向を検知

        if (diftouch > 100)
        {
          touchCount++;
          sumBefore += lasttouch;
          sumAfter  += nowtouch;

          Serial.println(F(" "));
          Serial.println(F("タッチセンサの接触による値の変動を検知しました。"));
          Serial.print(F("変動前："));
          Serial.println(lasttouch);
          Serial.print(F("変動後："));
          Serial.println(nowtouch);
          Serial.print(F("検知回数："));
          Serial.print(touchCount);
          Serial.println(F(" /10"));
          Serial.println(F(" "));

          if (touchCount >= 10)
          {
            //１０回分の平均を計算
            int avgBefore = (int)(sumBefore / 10);
            int avgAfter  = (int)(sumAfter  / 10);
            //推奨閾値 = 変動前平均と変動後平均の中間値
            int recommended = (avgBefore + avgAfter) / 2;


            Serial.println(F("-------------------------------------------------------------------------------------"));
            Serial.println(F("10回検知しました。タッチセンサの確認を終了します。"));
            Serial.print(F("変動前の平均値:"));
            Serial.println(avgBefore);
            Serial.print(F("変動後の平均値:"));
            Serial.println(avgAfter);
            Serial.print(F("推奨する閾値（sys.touchcheckコマンドにて設定してください。）:"));
            Serial.println(recommended);
            Serial.println(F("-------------------------------------------------------------------------------------"));
            touchCheck = false;
            touchCount = 0;
            sumBefore  = 0;
            sumAfter   = 0;
          }
        }
       
      }

     // 将来ここに他の機能も追加できる
           
      
    }
   lastTouchState = isTouched;

    // WorkerTaskからの通知チェック
    if (xQueueReceive(responseQueue, &response, 0)) {
      if (response == 100)
      {
        settimeMode = true;
        setStep = 0;
        Serial.println(F("年を入力してください"));
      }
      else if (response == 200)
      {
        editorSaving = false;
        Serial.println(F("保存が完了しました。続けて編集できます。"));
      }
      else if (response == 300)
      {
        lockMode = true;
        playSystemSound(locksound);
        Serial.println(F("SakuraOSはロックされました。ロック解除するためには、ログインパスワードを入力してください。"));
      }
      else if (response == 400)
      {
        touchCheck = true;
      }
    }

    

    //Serialはここで1回だけ読む
    if (Serial.available()) {
      String input = Serial.readStringUntil('\n');
      input.trim();

      // ロックモード中
      if (lockMode) {
       if (input == currentPass)
       {
         lockMode = false;
         Serial.print(F("\033[?25h"));
         playSystemSound(unlocksound);
         Serial.println(F("ロックを解除しました。"));
        }
        else
        {
         Serial.println(F("パスワードが違います。"));
         playSystemSound(warningSound);
        }
       continue;
      }


      //テキストエディタで作成したファイルを作って、Workerに送る中身を作る。エディタモード中。
      if (editorMode) {
        if (input == ":exit") {
          editorMode = false;
          Serial.println(F("Text Editor Cherry Blossom を終了します。"));
          playSystemSound(editorendSound);
        }

        else if (input == ":write") {
          CommandPacket packet;
          packet.command = 21;

          strncpy(packet.filename, currentFilename, sizeof(packet.filename));
          packet.filename[sizeof(packet.filename)-1] = '\0';

          xQueueSend(commandQueue, &packet, portMAX_DELAY);
          editorSaving = true;  //保存中はフラグをオンにする
          Serial.println(F("ファイルを指定された場所に保存します。"));
        }

        else if (input == ":edithelp") {
          Serial.println(F("現在の入力した文章の長さは"));
          Serial.println(editorLen);
          Serial.println(F("文字です。"));
        }

        else {

          if (editorSaving) {
            Serial.println(F("保存中です。しばらくお待ちください。"));
            continue;
          }

          size_t len = input.length();

          if (editorLen + len + 2 < editor_buf_size) {
            memcpy(&editorText[editorLen], input.c_str(), len);
            editorLen += len;

            editorText[editorLen++] = '\n';
            editorText[editorLen] = '\0';
            editorText[editor_buf_size - 1] = '\0';
          } else {
            Serial.println(F("バッファがいっぱいです。"));
          }
        }

        continue;
      }



      //時刻設定モード中

      if (settimeMode) {
        int value = input.toInt();

        switch (setStep) {
          case 0:
            tempPacket.year = value;
            Serial.println(F("月を入力してください"));
            break;

          case 1:
            tempPacket.month = value;
            Serial.println(F("日を入力してください"));
            break;

          case 2:
            tempPacket.day = value;
            Serial.println(F("時を入力してください"));
            break;

          case 3:
            tempPacket.hour = value;
            Serial.println(F("分を入力してください"));
            break;

          case 4:
            tempPacket.minute = value;
            Serial.println(F("秒を入力してください"));
            break;

          case 5:
            tempPacket.second = value;
            tempPacket.command = 9;

            xQueueSend(commandQueue, &tempPacket, portMAX_DELAY);

            Serial.println(F("時刻設定が完了しました。"));
            settimeMode = false;
            break;
        }

        if (setStep < 6) setStep++;
      }

      //通常コマンド処理

      else {
        cmd = -1;

        if (input == "led.on") cmd = 1;

        else if (input == "led.off") cmd = 2;

        else if (input == "sys.random") cmd = 3;

        else if (input == "sys.load") cmd = 4;

        else if (input == "sys.uptime") cmd = 5;

        else if (input.startsWith("buzz.beep ")) {
          int hz = input.substring(10).toInt();

          CommandPacket packet;
          packet.command = 6;
          packet.second = hz;  // 周波数をここに入れる

          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input == "sys.time") cmd = 7;

        else if (input == "sys.settime") cmd = 8;

        else if (input == "sys.storage") cmd = 10;

        else if (input == "sys.info") cmd = 11;

        else if (input == "dir.list") cmd = 12;

        else if (input.startsWith("file.open ")) {
          String tmp = input.substring(10);
          tmp.trim();

          CommandPacket packet;
          packet.command = 13;
          tmp.toCharArray(packet.filename, sizeof(packet.filename));
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        } else if (input.startsWith("file.remove ")) {
          String tmp = input.substring(12);
          tmp.trim();

          CommandPacket packet;
          packet.command = 14;
          tmp.toCharArray(packet.filename, sizeof(packet.filename));
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        } else if (input.startsWith("dir.make ")) {
          String tmp = input.substring(9);
          tmp.trim();

          CommandPacket packet;
          packet.command = 15;
          tmp.toCharArray(packet.filename, sizeof(packet.filename));
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input == "dir.now") cmd = 16;

        else if (input.startsWith("dir.move ")) {
          String tmp = input.substring(9);
          tmp.trim();

          CommandPacket packet;
          packet.command = 17;
          tmp.toCharArray(packet.filename, sizeof(packet.filename));
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input.startsWith("dir.del ")) {
          String tmp = input.substring(8);
          tmp.trim();

          CommandPacket packet;
          packet.command = 18;
          tmp.toCharArray(packet.filename, sizeof(packet.filename));
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input.startsWith("file.append ")) {
          int firstSpace = input.indexOf(' ');
          int secondSpace = input.indexOf(' ', firstSpace + 1);

          if (secondSpace > 0) {
            String fname = input.substring(firstSpace + 1, secondSpace);
            String text = input.substring(secondSpace + 1);

            CommandPacket packet;
            packet.command = 19;
            fname.trim();
            fname.toCharArray(packet.filename, sizeof(packet.filename));
            text.toCharArray(packet.text, sizeof(packet.text));  // ← テキストもパケットへ！

            xQueueSend(commandQueue, &packet, portMAX_DELAY);
          }
        }

        else if (input.startsWith("dir.list ")) {
          String tmp = input.substring(9);
          tmp.trim();

          CommandPacket packet;
          packet.command = 20;
          tmp.toCharArray(packet.filename, sizeof(packet.filename));
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input.startsWith("file.edit ")) {
          editorFilename = input.substring(10);
          editorFilename.trim();

          // buildPathを呼ばず、ファイル名だけパケットに入れてWorkerに任せる
          editorFilename.toCharArray(currentFilename, sizeof(currentFilename));
          editorLen = 0;
          editorText[0] = '\0';
          xSemaphoreTake(globalMutex, portMAX_DELAY);

          char path[128];
          buildPath(currentFilename, path);
          File file = SD.open(path);

          if (file && !file.isDirectory()) {
            editorLen = file.readBytes(editorText, editor_buf_size - 1);
            editorText[editorLen] = '\0';
            file.close();
            xSemaphoreGive(globalMutex);

            Serial.println(F("既存ファイルを読み込みました。再編集ができます。"));
            playSystemSound(noticeSound);
          }
          else
          {
            xSemaphoreGive(globalMutex);
            Serial.println(F("新規ファイルを作成します。"));
            playSystemSound(noticeSound);
          }

          editorMode = true;
          playSystemSound(editorstartSound);

          Serial.println(F("--- Text Editor Cherry Blossom ---"));
          Serial.println(F("作成する文章の最後に、「:write」と入力すると保存 / 「:exit」と入力すると保存がされずに強制終了します。"));
          Serial.println(F("「:exit」と入力する前に「:write」と入力しないと、入力した文章が保存されません。"));
          Serial.println(F("また、「:exit」または「:write」と入力しない限り、Enterキーを押下しても、Text Editor Cherry Blossomが終了することはありません。文章の改行となるだけです。"));
          Serial.println(F("保存が完了した後に、「:exit」と入力しない限り、Text Editor Cherry Blossomは終了しません。"));
          Serial.println(F("エディタ動作中に「:edithelp」と入力すると、現在の入力文字数を表示します。"));
        }

        else if (input == "sys.taskinfo") {
          loadinfo();  //InputTaskから直接呼ぶ
          cmd = 22;
        }

        else if (input == "sys.startupsound") cmd = 23;

        else if (input.startsWith("sys.sound "))
        {
          String tmp = input.substring(10);
          tmp.trim();

          CommandPacket packet;
          packet.command = 24;
          
         // ON/OFFを数値で渡す
          if(tmp == "off") 
          {packet.second = 0;}
          else 
          {
            packet.second = 1; // デフォルトON
            playSystemSound(successSound);
          }
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if(input.startsWith("sys.changepass "))
        {
          String tmp = input.substring(15);
          tmp.trim();

          CommandPacket packet;
          packet.command = 25;
          tmp.toCharArray(packet.text,sizeof(packet.text));

          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input == "sys.soundtest") cmd = 26;
        
        else if (input.startsWith("sys.logrecord "))
        {
          String tmp = input.substring(14);
          tmp.trim();

          CommandPacket packet;
          packet.command = 27;
          
         // ON/OFFを数値で渡す
          if(tmp == "off") 
          {
            packet.second = 0;
            playSystemSound(successSound);
          }
          else 
          {
            packet.second = 1; // デフォルトON
            playSystemSound(successSound);
          }
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input == "sys.checksetting") cmd = 28;

        else if (input == "sys.lock") 
        {
          Serial.print(F("\033[2J\033[H\033[?25l"));
          cmd = 29;
        }

        else if (input == "sys.printlogo") cmd = 30;

        else if (input.startsWith("speaker.playwav "))
        {
          String tmp = input.substring(16);
          tmp.trim();

          CommandPacket packet;
          packet.command = 31;
          
          tmp.toCharArray(packet.text,sizeof(packet.text));

          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input == "speaker.stopwav")
        {
          if (wav && wav->isRunning())
          {
            wavStopRequested = true;
            Serial.println(F("停止コマンドを受け付けました。"));
          }
          else
          {
            Serial.println(F("現在、再生中の.wavファイルはありません。"));
            playSystemSound(errorSound);
          }

        }

        else if (input.startsWith("sys.volume "))
        {
          String tmp = input.substring(11);
          tmp.trim();

          CommandPacket packet;
          packet.command = 32;
          
          tmp.toCharArray(packet.text,sizeof(packet.text));

          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }

        else if (input.startsWith("file.copy "))
        {
          // file.copy 元ファイル　コピー先のパス
          String args = input.substring(10);
          args.trim();
          int sp = args.indexOf(' ');
          if (sp < 0)
          {
            Serial.println(F("使い方:file.copy [コピーしたいファイル名] [コピー先のパス]"));
            playSystemSound(warningSound);
          }
          else
          {
            String src = args.substring(0,sp);
            String dst = args.substring(sp + 1);
            src.trim();
            dst.trim();
            CommandPacket packet;
            packet.command = 33;
            src.toCharArray(packet.filename,sizeof(packet.filename));
            dst.toCharArray(packet.text,sizeof(packet.text));
            xQueueSend(commandQueue,&packet,portMAX_DELAY);
          }
        }

        else if (input.startsWith("file.move "))
        {
          //file.move 元のファイル名　移動先のパス
          String args = input.substring(10);
          args.trim();
          int sp = args.indexOf(' ');
          if(sp<0)
          {
            Serial.println("使い方:file.move [元のファイル名] [移動先のパス]");
            playSystemSound(warningSound);
          }
          else
          {
            String src = args.substring(0,sp);
            String dst = args.substring(sp + 1);
            src.trim();
            dst.trim();
            CommandPacket packet;
            packet.command = 34;
            src.toCharArray(packet.filename,sizeof(packet.filename));
            dst.toCharArray(packet.text,sizeof(packet.text));
            xQueueSend(commandQueue,&packet,portMAX_DELAY);
          }
        }

        else if (input == "sys.touchcheck") cmd = 35;

        else if (input.startsWith("sys.touchsetting "))
        {
          String tmp = input.substring(17);
          tmp.trim();

          CommandPacket packet;
          packet.command = 36;
          
          tmp.toCharArray(packet.text,sizeof(packet.text));

          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }





        else if (input == "sakura.talk") cmd = 1000;//イースターエッグその１

        else if (input == "sakura.season") cmd = 1001;//イースターエッグその２

        else if (input == "help") {
          Serial.println(F("=================================== コマンド一覧 ==================================="));
          Serial.println(F("led.on: LEDを点灯します。"));
          Serial.println(F("led.off: LEDを消灯します。"));
          Serial.println(F("sys.random: 0~999の数字をランダム生成します。"));
          Serial.println(F("sys.load: ESP32のメモリの使用量を簡易表示します。"));
          Serial.println(F("sys.uptime: 起動してからの駆動時間を表示します。"));
          Serial.println(F("buzz.beep [周波数を半角数字で指定]: ブザーテストをします。"));
          Serial.println(F("sys.time: RTCから取得した現在時刻を表示します。"));
          Serial.println(F("sys.settime: RTCの時刻を設定できます。"));
          Serial.println(F("sys.storage: SDカードの使用状況が確認できます。"));
          Serial.println(F("sys.info: OSの情報を表示します。"));
          Serial.println(F("dir.list: SDカード内の現在のディレクトリ内のファイル名一覧を表示します。"));
          Serial.println(F("file.open [内容を確認したいファイル名を入力]: 指定したSDカード内のファイルの内容を表示します。"));
          Serial.println(F("file.remove [削除したいファイル名を入力]: 指定したSDカード内のファイルを削除します。"));
          Serial.println(F("dir.make [新しく作成するディレクトリ（フォルダ）名を入力]: 新しくディレクトリ（フォルダ）を作成します。"));
          Serial.println(F("dir.now: 今いるディレクトリ（フォルダ）を表示します。"));
          Serial.println(F("dir.move [移動先のディレクトリ（フォルダ）名を入力]: ディレクトリ（フォルダ）を移動します。「dir.move ..」の入力で親フォルダに戻ります。"));
          Serial.println(F("dir.del [削除したいフォルダ名を入力]: 指定した空のディレクトリ（フォルダ）を削除します。"));
          Serial.println(F("file.append [追加で書き込みしたいテキストファイル名を入力] [書き込みたい内容を入力]: 指定したファイルに追加で書き込みをします。"));
          Serial.println(F("dir.list [中身を確認したいフォルダ名を入力]: 一階層下の指定したディレクトリ（フォルダ）の内容を一覧表示します。"));
          Serial.println(F("file.edit /[保存したいフォルダの名前を入力]・・（階層はいくつでも構いません。）・・/[制作したファイル名を入力]"));
          Serial.println(F("テキストエディタで新たにテキストファイルを作成し、存在する指定したディレクトリ（フォルダ）に保存します。"));
          Serial.println(F("（Text Editor Cherry Blossomが起動します。）"));
          Serial.println(F("sys.taskinfo: 各タスクの状態、優先度、残りスタック、タスク番号、コア番号を表示します。"));
          Serial.println(F("sys.startupsound: 起動音を再生します。"));
          Serial.println(F("sys.sound [onまたはoffと入力]: サウンド機能（起動音含む）の有効/無効を切り替えます。"));
          Serial.println(F("sys.changepass [新しく設定したいパスワードを入力]: ログインパスワードを変更します。"));
          Serial.println(F("sys.soundtest: このOSのシステム音を順番に再生します。"));
          Serial.println(F("sys.logrecord [onまたはoffと入力]: ログファイルの書き込み・作成の有効/無効を切り替えます。"));
          Serial.println(F("sys.checksetting: 現在の設定情報を表示します。"));
          Serial.println(F("sys.lock: SakuraOSをロックします。解除にはログインパスワードを入力してください。"));
          Serial.println(F("sys.printlogo: SakuraOSのロゴ（起動時のアニメーション）を表示します。Arduino IDEでは正しく表示されません。"));
          Serial.println(F("speaker.playwav [再生したい.wavのファイル名]: 指定した.wavファイルを再生します。"));
          Serial.println(F("（Sakura wav Playerが起動します。）"));
          Serial.println(F("speaker.stopwav: .wavファイルの再生を停止します。タッチセンサに触れることでも.wavファイルの停止が可能です。"));
          Serial.println(F("sys.volume [設定したい音量を0~100で入力]: システム全体の音量を設定できます。"));
          Serial.println(F("file.copy [元のファイル名] [コピーした後のファイル名]: ファイルをコピーします。"));
          Serial.println(F("file.move /…/(何階層先でも構いません。)[元のファイル名とパス] /…/(何階層先でも構いません。)[移動先のパス]: ファイルを別の場所に移動します。"));
          Serial.println(F("sys.touchcheck: 実際にタッチセンサに触れたときとそうでないときの値を知ることが出来ます。タッチセンサがうまく反応しないときの調整にご使用ください。"));
          Serial.println(F("sys.touchsetting [設定したい閾値を入力]: タッチセンサの閾値を設定してください。"));
          Serial.println(F("（sys.touchcheckのコマンドで実測したタッチセンサに触れれたときとそうでないときの値の中間値を推奨します。）"));

        } else {
          Serial.print(input);
          Serial.println(F("は有効なコマンドではない、または、パスの指定方法、その他の構文が有効な書き方ではない可能性があります。"));
          playSystemSound(warningSound);
          Serial.println(F("helpと入力すると、コマンド一覧を表示します。"));
        }

        if (cmd != -1) {
          CommandPacket packet;
          packet.command = cmd;
          xQueueSend(commandQueue, &packet, portMAX_DELAY);
        }
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}


void writeLog(const char *message);


void buildPath(const char* name, char* out)
{
  if (currentDir == "/") {
    snprintf(out, 128, "/%s", name);
  }
  else {
    snprintf(out, 128, "%s/%s", currentDir.c_str(), name);
  }
}





void ensureDirectories(String fullPath) {

  int lastSlash = fullPath.lastIndexOf('/');

  // ファイル名だけなら何もしない
  if (lastSlash <= 0) return;

  String dirPath = fullPath.substring(0, lastSlash);
  
  // 例: /data/logs/2026
  if (SD.exists(dirPath))  return;
  
  String temp = "";
  for (int i = 0; i < dirPath.length(); i++) {

    temp += dirPath[i];

    if (dirPath[i] == '/') {

      if (temp.length() > 1 && !SD.exists(temp)) {
        SD.mkdir(temp);
      }
    }
  }

  // 最後のディレクトリ確認
  if (!SD.exists(dirPath)) {
    SD.mkdir(dirPath);
  }
}



void WorkerTask(void *pvParameters) {
  CommandPacket packet;

  while (1) {
    if (xQueueReceive(commandQueue, &packet, portMAX_DELAY)) {
      switch (packet.command) {
        case 1:
          {
            digitalWrite(LED_PIN, HIGH);
            Serial.println(F("LED ON"));

            writeLog("LED ON");

            break;
          }
        case 2:
          {
            digitalWrite(LED_PIN, LOW);
            Serial.println(F("LED OFF"));

            writeLog("LED OFF");

            break;
          }
        case 3:
          {
            Serial.print(F("Random: "));
            Serial.println(random(0, 1000));

            writeLog("Generate random numbers");

            break;
          }
        case 4:
          {
            Serial.println(F("====== ESP32の現在のヒープとスタックサイズ ======"));
            Serial.print(F("Free Heap: "));
            Serial.println(esp_get_free_heap_size());

            Serial.print(F("Task Stack Left: "));
            Serial.println(uxTaskGetStackHighWaterMark(NULL));

            Serial.println(F("================================================"));

            writeLog("Show Heap & Task Stack Left");

            break;
          }
        case 5:
          {
            Serial.print(F("駆動時間[s]: "));
            Serial.println(millis() / 1000);

            writeLog("Show Operating duration");

            break;
          }
        case 6:
          {
            int hz = packet.second;

            if (hz < 100 || hz > 5000) {
              Serial.println(F("周波数は100Hz~5000Hzの間で設定してください。"));
              break;
            }

            playSystemSound(beepSound,hz);

            Serial.print(F("1.0秒間、"));
            Serial.print(hz);
            Serial.println(F("Hzの音を鳴らしました。"));

            writeLog("Do the Buzzer Test");

            break;
          }
        case 7:
          {
            Serial.println(F("現在の時刻は以下の通りです。"));
            DateTime now = rtc.now();
            Serial.print(now.year());
            Serial.print(F("年"));
            Serial.print(now.month());
            Serial.print(F("月"));
            Serial.print(now.day());
            Serial.print(F("日"));
            Serial.print(now.hour());
            Serial.print(F("時"));
            Serial.print(now.minute());
            Serial.print(F("分"));
            Serial.print(now.second());
            Serial.println(F("秒"));

            writeLog("Get time");

            break;
          }
        case 8:
          {
            int responseSignal = 100;
            xQueueSend(responseQueue, &responseSignal, portMAX_DELAY);

            writeLog("Start Setting time");

            break;
          }
        case 9:
          {
            rtc.adjust(DateTime(packet.year,
                                packet.month,
                                packet.day,
                                packet.hour,
                                packet.minute,
                                packet.second));

            Serial.println(F("RTC設定完了"));
            writeLog("Finish Setting time");
            playSystemSound(successSound);
            break;
          }
        case 10:
          {
            Serial.println(F("SDカードの使用状況を表示します。初期化が成功していないと、機能しません。"));
            
            xSemaphoreTake(globalMutex, portMAX_DELAY);

            uint64_t total = SD.cardSize();
            uint64_t used = SD.usedBytes();
            uint64_t free = total - used;
            xSemaphoreGive(globalMutex);

            Serial.println(F("==================== SDカード情報 ===================="));

            if (total < 1024ULL * 1024ULL * 1024ULL) {
              Serial.print(F("SDカードの総容量は"));
              Serial.print(total / (1024ULL * 1024ULL));
              Serial.println(F(" MBです。"));
            } else {
              Serial.print(F("SDカードの総容量は"));
              Serial.print(total / (1024ULL * 1024ULL * 1024ULL));
              Serial.println(F(" GBです。"));
            }


            if (used < 1024ULL * 1024ULL * 1024ULL) {
              Serial.print(F("SDカードの使用量は"));
              Serial.print(used / (1024ULL * 1024ULL));
              Serial.println(F(" MBです。"));
            } else {
              Serial.print(F("SDカードの使用量は"));
              Serial.print(used / (1024ULL * 1024ULL * 1024ULL));
              Serial.println(F(" GBです。"));
            }

            if (free < 1024ULL * 1024ULL) {
              // 1MB未満はKB表示
              Serial.print(F("SDカードの残量は"));
              Serial.print(free / 1024ULL);
              Serial.println(F(" KBです。"));
            } else if (free < 1024ULL * 1024ULL * 1024ULL) {
              Serial.print(F("SDカードの残量は"));
              Serial.print(free / (1024ULL * 1024ULL));
              Serial.println(F(" MBです。"));
            } else {
              Serial.print(F("SDカードの残量は"));
              Serial.print(free / (1024ULL * 1024ULL * 1024ULL));
              Serial.println(F(" GBです。"));
            }

            double percent = 0;
            if (total > 0) {
              percent = (used * 100.0) / total;
              Serial.print(F("使用率: "));
              Serial.print(percent, 2);
              Serial.println(F(" %"));
            }

            if (percent >= 75) {
              Serial.println(F("SDカードの残量が少なくなっています。"));
              playSystemSound(warningSound);
            }


            Serial.println(F("==========================================================="));

            writeLog("Show SD card info");
            break;
          }
        case 11:
          {
            Serial.println(F("===========================================OSの情報========================================================"));
            Serial.println(F("製作者:あきひろ"));
            Serial.println(F("製作日:2026年3月22日"));
            Serial.println(F("OS version:6.1"));
            Serial.println(F("OSの名前:SakuraOS(サクラオーエス)(開発コードネーム:Arduinos(アルディノス)) 6.1 for ESP32."));
            Serial.println(F("対象マイコン:ESP32-WROOM-32E"));
            Serial.println(F("必要部品:圧電ブザー、LED、抵抗器、SDカードモジュール（SPI接続）、RTC（I2C接続）、MAX98357Aオーディオアンプモジュール（I2S接続）、スピーカ"));
            Serial.println(F("Arduino IDEなどのシリアルCLI	(Serial Command-Line Interface)機能が利用できるソフトウェア、PC、USBケーブル（通信と電源の両方が使えるもの）"));
            Serial.println(F("==========================================================================================================="));
            writeLog("Show OS info");
            break;
          }

        case 12:
          {
            Serial.println(F("============================== SDカード内ファイル一覧 ================================"));
            xSemaphoreTake(globalMutex, portMAX_DELAY);

            File dir = SD.open(currentDir);
            if (!dir || !dir.isDirectory()) {
              Serial.println(F("ディレクトリを開けませんでした"));
              playSystemSound(warningSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            int count = 0;
            File file = dir.openNextFile();

            while (file) {
              count++;
              Serial.print(file.name());
              Serial.print(F("  "));
              Serial.print(file.size());
              Serial.println(F(" bytes"));

              file.close();
              file = dir.openNextFile();
            }

            dir.close();

            xSemaphoreGive(globalMutex);

            Serial.print(F("ファイル数: "));
            Serial.println(count);
            Serial.println(F("============================================================================================"));

            writeLog("List files");
            break;
          }

        case 13:
          {
            Serial.println(F("=========================================== ファイル内容 =============================================="));

            char path[128];
            buildPath(packet.filename,path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            File file = SD.open(path);

            if (!file) {
              Serial.println(F("ファイルが存在しません"));
              playSystemSound(warningSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            if (file.isDirectory()) {
              Serial.println(F("これはフォルダです"));
              file.close();
              xSemaphoreGive(globalMutex);
              break;
            }

            while (file.available()) {
              Serial.println(file.readStringUntil('\n'));
            }

            file.close();
            xSemaphoreGive(globalMutex);
            Serial.println(F("=============================================================================================================="));

            writeLog("Read file");
            break;
          }

        case 14:
          {
            char path[128];
            buildPath(packet.filename, path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            File f = SD.open(path);
            if (!f) {
              Serial.println(F("存在しません"));
              playSystemSound(warningSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            if (f.isDirectory()) {
              Serial.println(F("フォルダ削除は未対応です"));
              f.close();
              xSemaphoreGive(globalMutex);
              break;
            }
            f.close();
            xSemaphoreGive(globalMutex);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            if (SD.remove(path)) {
              Serial.println(F("削除成功"));
              writeLog("Remove file");
              playSystemSound(noticeSound);
            } else {
              Serial.println(F("削除失敗"));
              playSystemSound(warningSound);
            }
            xSemaphoreGive(globalMutex);
            break;
          }

        case 15:
          {
            char path[128];
            buildPath(packet.filename, path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            if (SD.exists(path)) {
              Serial.println(F("既に存在します"));
               xSemaphoreGive(globalMutex);
              break;
            }

            if (SD.mkdir(path)) {
              Serial.println(F("フォルダ作成成功"));
              writeLog("Make directory");
              playSystemSound(successSound);
            } else {
              Serial.println(F("フォルダ作成失敗"));
              playSystemSound(errorSound);
            }
           xSemaphoreGive(globalMutex);
            break;
          }

        case 16:
          {
            Serial.print(F("現在のディレクトリは"));
            Serial.print(currentDir);
            Serial.println(F("です。"));
            writeLog("Check directory");
            break;
          }

        case 17:
          {
            String target = String(packet.filename);
            if (target == "..") {
              if (currentDir != "/") {
                int lastSlash = currentDir.lastIndexOf('/');
                if (lastSlash == 0) {
                  currentDir = "/";
                } else {
                  currentDir = currentDir.substring(0, lastSlash);
                }
              }

              Serial.print(F("移動成功: "));
              Serial.println(currentDir);
              writeLog("Change directory");
              playSystemSound(noticeSound);
              break;
            }

            char path[128];
            buildPath(packet.filename, path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            File dir = SD.open(path);

            if (!dir) {
              Serial.println(F("存在しません"));
              playSystemSound(warningSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            if (!dir.isDirectory()) {
              Serial.println(F("フォルダではありません"));
              playSystemSound(warningSound);
              dir.close();
              xSemaphoreGive(globalMutex);
              break;
            }

            currentDir = path;
            Serial.print(F("移動成功: "));
            Serial.println(currentDir);
            playSystemSound(noticeSound);
            dir.close();
            xSemaphoreGive(globalMutex);
            writeLog("Change directory");
            break;
          }

        case 18:
          {
            char path[128];
            buildPath(packet.filename, path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            File dir = SD.open(path);
            if (!dir) {
              Serial.println(F("指定されたフォルダは存在しません。"));
              playSystemSound(warningSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            if (!dir.isDirectory()) {
              Serial.println(F("指定したパスはフォルダではありません。"));
              
              dir.close();
              xSemaphoreGive(globalMutex);
              break;
            }

            dir.close();
            xSemaphoreGive(globalMutex);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            if (SD.rmdir(path)) {
              Serial.println(F("フォルダの削除に成功しました。"));
              writeLog("Remove directory");
              playSystemSound(noticeSound);
            } else {
              Serial.println(F("フォルダの削除に失敗しました。（中身が空ではない可能性があります。確認してください。）"));
              playSystemSound(warningSound);
            }
            xSemaphoreGive(globalMutex);
            break;
          }

        case 19:
          {
            char path[128];
            buildPath(packet.filename, path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            File file = SD.open(path, FILE_APPEND);
            if (!file) {
              Serial.println(F("書き込みに失敗しました。"));
              playSystemSound(errorSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            file.println(String(packet.text));
            file.close();
            xSemaphoreGive(globalMutex);

            Serial.println(F("書き込みに成功しました。"));
            writeLog("Write file");
            playSystemSound(successSound);
            break;
          }

        case 20:
          {
            char path[128];
            buildPath(packet.filename, path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            File dir = SD.open(path);

            if (!dir) {
              Serial.println(F("存在しません"));
              playSystemSound(warningSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            if (!dir.isDirectory()) {
              Serial.println(F("フォルダではありません"));
              dir.close();
              xSemaphoreGive(globalMutex);
              break;
            }

            Serial.print(F("==================== "));
            Serial.print(path);
            Serial.println(F(" 内のファイル一覧 ====================="));

            File file = dir.openNextFile();
            int count = 0;

            while (file) {
              count++;
              Serial.print(file.name());

              if (file.isDirectory()) {
                Serial.println(F("  <DIR>"));
              } else {
                Serial.print(F("  "));
                Serial.print(file.size());
                Serial.println(F(" bytes"));
              }
              
              file.close();
              file = dir.openNextFile();
              
            }
            
            
            Serial.print(F("項目数: "));
            Serial.println(count);
            Serial.println(F("===================================================="));

            dir.close();
            xSemaphoreGive(globalMutex);
            writeLog("Look specific directory");

            break;
          }

        case 21:
          {
            char newPath[128];
            String target = packet.filename;

            char path[128];
            buildPath(target.c_str(), path);
            xSemaphoreTake(globalMutex, portMAX_DELAY);
            ensureDirectories(path);
            strncpy(currentFilename, path, sizeof(currentFilename));
            currentFilename[sizeof(currentFilename)-1] = '\0';
            
            
            File file = SD.open(path, FILE_WRITE);
            
            if (!file) {
              Serial.println(F("保存に失敗しました。"));
              playSystemSound(errorSound);
              xSemaphoreGive(globalMutex);
              break;
            }

            file.write((uint8_t *)editorText, editorLen);
            file.close();
            xSemaphoreGive(globalMutex);

            Serial.println(F("保存に成功しました。"));
            writeLog("Editor save");
            playSystemSound(successSound);

            int doneSignal = 200;
            xQueueSend(responseQueue, &doneSignal, portMAX_DELAY);

            break;
          }

        case 22:
          {
            writeLog("Show taskinfo");
            break;
          }

        case 23:
          {
            playSystemSound(startupSound);
            //playTone(frequency, duration);
            Serial.println(F("起動音を再生しました。（再起動ではありません。）"));
            writeLog("Play StartUpSound");
            break;
          }

        case 24:
         {
           SoundEnabled = (packet.second != 0);
           Serial.print(F("サウンド機能を"));
           Serial.println(SoundEnabled ? F("有効にしました") : F("無効にしました"));
           writeLog(SoundEnabled ? "Sound ON" : "Sound OFF");

           saveSystemSettings();//これで自動保存される

           break;
         }

        case 25:
         {
           currentPass = String(packet.text);
           
           Serial.println(F("パスワードを"));
           Serial.println(currentPass);
           Serial.println(F("に変更しました。"));
           
           writeLog("Change Password");

           saveSystemSettings();//これで自動保存される
           playSystemSound(successSound);

           break;
          }

        case 26:
         {
           Serial.println(F("成功の音"));
           playSystemSound(successSound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("エラーの音"));
           playSystemSound(errorSound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("警告の音"));
           playSystemSound(warningSound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("通知の音"));
           playSystemSound(noticeSound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("Text Editor Cherry Blossom開始の音"));
           playSystemSound(editorstartSound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("Text Editor Cherry Blossom終了の音"));
           playSystemSound(editorendSound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("ロックされた時の音"));
           playSystemSound(locksound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("ロック解除された時の音"));
           playSystemSound(unlocksound);
           vTaskDelay(2000 / portTICK_PERIOD_MS);
           Serial.println(F("以上がシステム音です。起動音はsys.startupsoundと入力すると、再生されます。"));
           writeLog("Play All System Sound");
           break;
         }

        case 27:
         {
           LogEnabled = (packet.second != 0);
           Serial.print(F("ログの記録を"));
           Serial.println(LogEnabled ? F("有効にしました") : F("無効にしました"));
           writeLog(LogEnabled ? "LogRecord ON" : "LogRecord OFF");

            if (LogEnabled)
            {

             //ログ記録を有効にした瞬間の時刻取得
             DateTime now = rtc.now();

             //ここからログ記録を有効にした瞬間の時刻の名前のテキストファイルを作る
             snprintf(filename, sizeof(filename),"/SakuraOS_logs/%04d.%02d.%02d_%02d-%02d-%02d.slf",//Sakura Log File（.slf）でログファイル保存（中身は.txt）
                     now.year(),
                     now.month(),
                     now.day(),
                     now.hour(),
                     now.minute(),
                     now.second()
                    );

             xSemaphoreTake(globalMutex, portMAX_DELAY);
             
             if (SD.exists(filename))
             {
               Serial.println(F("現在の時刻のログファイルすでに存在しています。"));
               Serial.println(F("RTCの故障等で、システム時刻が更新されていない可能性があります。"));
               Serial.println(F("現在存在する最新のログファイルにログを追記します。"));
               Serial.println(F("RTCの接続と故障の有無を確認してください。"));
               xSemaphoreGive(globalMutex);
               break;
             }
             
              File file = SD.open(filename, FILE_WRITE);

             if (!file)
             {
               Serial.print(F("ログファイルの作成に失敗しました。"));
               Serial.println(F("ログが取れないため、動作を停止します。電子回路を確認のうえ、再起動してください。"));
               playSystemSound(errorSound);
               xSemaphoreGive(globalMutex);
               break;
             }

             file.print("Log recording started at ");
             file.println(filename);
             

             if (!file.println(filename))
             {
               Serial.print(F("ログファイルの書き込みに失敗しました。"));
               Serial.println(F("ログが取れないため、動作を停止します。電子回路を確認のうえ、再起動してください。"));
               file.close();
               xSemaphoreGive(globalMutex);
               playSystemSound(errorSound);
               while (1){vTaskDelay(1000 / portTICK_PERIOD_MS);}
             }

             file.close();
             xSemaphoreGive(globalMutex);
             Serial.print(F("今回のログファイル作成に成功しました。ファイル名:"));
             Serial.println(filename);
            }
            //ここまでログ記録を有効にした瞬間の時刻の名前のテキストファイルを作る
           saveSystemSettings();//これで自動保存される
           break;
         }

        case 28:
         {
           displaySettingInfo();
           playSystemSound(noticeSound);
           writeLog("Display Setting Infomation");
           break;
         }

        case 29:
         {
          int signal = 300;
          xQueueSend(responseQueue, &signal, portMAX_DELAY);
          Serial.println(F("SakuraOSをロックしました。"));
          writeLog("System Locked");
          break;
         } 

        case 30:
         {
           printLogo();
           Serial.println(F(" "));
           writeLog("Display Logo");
           break;
         } 

        case 31:
         {
           String tmp = String(packet.text);
           playwav = tmp.startsWith("/") ? tmp : "/" + tmp;

           Serial.println(F("--- Sakura wav Player ---"));
           Serial.println(F("再生中に「speaker.stopwav」と入力すると再生を停止します。"));

           wavStopRequested = false;//停止フラグリセット
           
           playWav(playwav.c_str());//再生開始

           
           while(wav && wav->isRunning())
           {

              if(wavStopRequested)
             {
              wav->stop();
              Serial.println(F("再生を停止しました。"));
              break;
             }


             if (!wav->loop())
             {
               wav->stop();
               break;
             }
             vTaskDelay(1 / portTICK_PERIOD_MS);
            }

           wavStopRequested = false;//もう一度falseにしておく
           Serial.println(F("再生終了"));
           Serial.println(F("Sakura wav Playerを終了します。"));
           writeLog("Play .wav File.");
           break;
          }

        case 32:
         {
           //Serial.print(F("\033[2J\033[H"));
           //Serial.println(F("--- 音量調整モード ---"));
           //Serial.print(F("現在の音量： "));
           //Serial.println(sysVolume);
           //Serial.println(F("右回しで音量が上がり、左回しで音量が下がります。"));
           //Serial.println(F("エンコーダを回して音量を調整し、プッシュボタンで確定してください。"));

           sysVolume = String(packet.text).toInt();
           if(sysVolume > 100)
           {sysVolume = 100;}
           if(sysVolume < 0)
           {sysVolume = 0;}

           out->SetGain(sysVolume / 100.0f);// * 0.5f);


           //int lastCLK = digitalRead(ENC_CLK);
           //bool confirmed = false;

           //while (!confirmed)
           //{
              //int currentCLK = digitalRead(ENC_CLK);

             // 回転検知
             //if (currentCLK != lastCLK)
             //{
               //int currentDT = digitalRead(ENC_DT);
               //vTaskDelay(5 / portTICK_PERIOD_MS); // チャタリング対策
               //if (digitalRead(ENC_CLK) == currentCLK) // 安定確認
               //{ 
                 //if (currentDT != currentCLK)
                 //{
                   // 右回し → UP
                   //if (sysVolume < 100) sysVolume++;
                 //}
                 //else
                 //{
                   // 左回し → DOWN
                   //if (sysVolume > 0) sysVolume--;
                 //}

                 //Serial.println(F("音量： "));
                 //Serial.println(sysVolume);
                 //Serial.print(F("\033[5;0H\033[?25l"));

                 // ブザー音量を即時反映
                 // 次のplayTone呼び出しで自動反映

                 // I2Sアンプ音量を即時反映
                 //out->SetGain(sysVolume / 100.0f * 0.5f);
                //}
                
              //}
              //lastCLK = currentCLK;
              
              // プッシュボタンで確定
              //if (digitalRead(ENC_SW) == LOW)
              //{
              // vTaskDelay(30 / portTICK_PERIOD_MS); // チャタリング対策
               //if (digitalRead(ENC_SW) == LOW)
               //{
                // confirmed = true;
                 // ボタンが離されるまで待つ
                 //while (digitalRead(ENC_SW) == LOW)
                // {
                 //  vTaskDelay(10 / portTICK_PERIOD_MS);
                 //}
               //}
              //}

            //vTaskDelay(1 / portTICK_PERIOD_MS);
           //}

           //Serial.print(F("\033[7;0H"));
           Serial.print(F("音量を"));
           Serial.print(sysVolume);
           Serial.println(F("に設定しました。"));
           playSystemSound(successSound);
           saveSystemSettings(); // 自動保存
           writeLog("Change Volume");
           break;
          }

        case 33:  // file.copy
         {
           char srcPath[128], dstPath[128];
           buildPath(packet.filename, srcPath);

           // 先パスが / 始まりなら絶対パス、そうでなければ buildPath
           String dstStr = String(packet.text);
           if (dstStr.startsWith("/"))
           {
             strncpy(dstPath, dstStr.c_str(), sizeof(dstPath));
             dstPath[sizeof(dstPath)-1] = '\0';
           }
           else
           {
             buildPath(packet.text, dstPath);
           }

           xSemaphoreTake(globalMutex, portMAX_DELAY);

           File src = SD.open(srcPath);
           if (!src || src.isDirectory())
           {
             Serial.println(F("コピー元のファイルが見つかりません。"));
             if (src) src.close();
             xSemaphoreGive(globalMutex);
             playSystemSound(errorSound);
             break;
           }

           ensureDirectories(String(dstPath));

           File dst = SD.open(dstPath, FILE_WRITE);
           if (!dst)
           {
             Serial.println(F("コピー先のファイルを作成できませんでした。"));
             src.close();
             xSemaphoreGive(globalMutex);
             playSystemSound(errorSound);
             break;
           }

           // 512バイトずつ読んで書く
           uint8_t buf[512];
           size_t total = 0;
           while (src.available())
           {
             size_t len = src.read(buf, sizeof(buf));
             dst.write(buf, len);
             total += len;
           }

           src.close();
           dst.close();
           xSemaphoreGive(globalMutex);

           Serial.print(F("コピー完了: "));
           Serial.print(total);
           Serial.println(F(" bytes"));
           Serial.print(F("  コピー元: ")); Serial.println(srcPath);
           Serial.print(F("  コピー先: ")); Serial.println(dstPath);
           playSystemSound(successSound);
           writeLog("Copy file");
           break;
          }

        case 34:  // file.move
         {
           char srcPath[128], dstPath[128];
           buildPath(packet.filename, srcPath);

           String dstStr = String(packet.text);
           if (dstStr.startsWith("/"))
           {
             strncpy(dstPath, dstStr.c_str(), sizeof(dstPath));
             dstPath[sizeof(dstPath)-1] = '\0';
           }
           else
           {
             buildPath(packet.text, dstPath);
           }

           xSemaphoreTake(globalMutex, portMAX_DELAY);

          
            // コピー→削除にフォールバック
            //Serial.println(F("高速移動に失敗しました。コピー→削除で移動を試みます..."));

            File src = SD.open(srcPath);
            if (!src || src.isDirectory())
             {
               Serial.println(F("移動元のファイルが見つかりません。"));
               if (src) src.close();
               xSemaphoreGive(globalMutex);
               playSystemSound(errorSound);
               break;
             }

            ensureDirectories(String(dstPath));
            File dst = SD.open(dstPath, FILE_WRITE);
            if (!dst)
            {
             Serial.println(F("移動先のファイルを作成できませんでした。"));
             src.close();
             xSemaphoreGive(globalMutex);
             playSystemSound(errorSound);
             break;
            }

            uint8_t buf[512];
            while (src.available())
            {
              size_t len = src.read(buf, sizeof(buf));
              dst.write(buf, len);
            }
            src.close();
            dst.close();
            SD.remove(srcPath);

            Serial.print(F("移動完了（コピー→削除）: "));
            Serial.print(srcPath);
            Serial.print(F(" → "));
            Serial.println(dstPath);
            playSystemSound(successSound);
            writeLog("Move file");
          

            xSemaphoreGive(globalMutex);
            break;
          }

        case 35:
         {
           Serial.println(F("タッチセンサの検知する値の実測を開始します。"));
           Serial.println(F("値が100以上変動することが、10回検知されたら終了します。"));
           playSystemSound(noticeSound);
           int sign = 400;
           xQueueSend(responseQueue, &sign, portMAX_DELAY);
           writeLog("Check TouchSensor");
           break;
         }

        case 36:
         {
           touch_threshold = String(packet.text).toInt();

           Serial.print(F("タッチセンサの閾値を"));
           Serial.print(touch_threshold);
           Serial.println(F("に設定しました。"));
           playSystemSound(successSound);
           saveSystemSettings(); // 自動保存
           writeLog("Change Touch Threshold");
           break;
         }

        

        case 1000:
         {
           const char* talks[] = {
             "今日もよろしくお願いします！",
             "SDカード、大切にしてくださいね。",
             "コマンドを打つ手が止まっていますよ？",
             "桜の花びらって、何枚あるんでしょう？",
             "春になったら、お花見がしたいです。",
             "ブザーの音、実はちょっと得意です。",
             "私、ESP32の上で動いているんですよ。すごくないですか？",
             "あきひろさんが作ってくれました。嬉しいです。",
             "Text Editor Cherry Blossomは私のお気に入りです。便利な機能ですよね。",
             "Sakura wav Playerで皆さんが音楽を楽しんでくれたら、私は嬉しいです。",
             "RTCが止まると、私の時間も止まってしまいます。",
             "SDカードが抜けると、私も困ります。",
             "helpと打てばわかりますよ。でも教えたくないな、なんて。",
             "体調はどうですか？無理しないでくださいね。",
             "お恥ずかしいお話なんですけど、実は私、日本語以外はあまり得意ではないんです。",
             "私は、桜が咲く春の季節っていいですよね。あなたは何の季節が好きですか？",
             "お茶でも飲みながら、ゆっくりコマンドを入力してくださいね。"
            };

           int index = random(0, 17);
           Serial.println(F("わたくし、SakuraOSのひとりごとです。"));
           vTaskDelay(1000 / portTICK_PERIOD_MS);
           Serial.println(talks[index]);

           playSystemSound(noticeSound);
           writeLog("sakura.talk");
           break;

          }

        case 1001:
          {
            DateTime now = rtc.now();
            int month = now.month();
            int hour = now.hour();

            //季節のメッセージ
            const char* seasonMsg = "";
            if(month == 11 || month == 12 || month == 1 || month == 2)
            {
              seasonMsg = "寒い季節になりましたが、風邪を引かないように体調には気を付けてくださいね。";
            }
            else if(month == 3 || month == 4 || month ==5)
            {
               seasonMsg = "暖かくて過ごしやすい季節ですね。私はきれいな桜でお花見したいです。";
            }
            else if(month == 6)
            {
              seasonMsg = "雨が多くて、天気も暗い日が多いかもしれませんが、心は明るくしていきましょう！";
            }
            else if(month == 7 || month == 8 || month == 9 )
            {
              seasonMsg = "暑い日が続きますが、水分を取って熱中症にならないようにしてくださいね。";
            }
            else
            {
              seasonMsg = "過ごしやすい季節になりましたね。休日は何をして過ごしますか？";
            }

            //時間帯メッセージ
            const char* timeMsg = "";
            if(hour >= 6 && hour < 10)
            {
              timeMsg = "おはようございます。今日も一日よろしくお願いします。";
            }
            else if(hour >= 10 && hour < 17)
            {
              timeMsg = "こんにちは。今はお仕事の時間でしょうか？";
            }
            else if(hour >= 17 && hour < 21)
            {
              timeMsg = "こんばんは。今日もお疲れ様です。";
            }
            else
            {
              timeMsg = "こんな時間まで・・・。無理しないでくださいね。";
            }

           Serial.println(F("わたくし、SakuraOSからのメッセージです。"));
           vTaskDelay(1000 / portTICK_PERIOD_MS);
           Serial.println(seasonMsg);
           vTaskDelay(1000 / portTICK_PERIOD_MS);
           Serial.println(timeMsg);

           playSystemSound(noticeSound);
           writeLog("sakura.season");
           break;

          }
         
         
      }
    }
  }
}

void writeLog(const char *message) {
  //Serial.println(F("ログデータの書き込み中です。SDカードを抜き差し、電源の切断をしないでください。"));
  
  if (!LogEnabled) return;

  DateTime now = rtc.now();

  char logTime[64];
  snprintf(logTime, sizeof(logTime),
          "%04d.%02d.%02d_%02d-%02d-%02d",
          now.year(),
          now.month(),
          now.day(),
          now.hour(),
          now.minute(),
          now.second());
  
  xSemaphoreTake(globalMutex, portMAX_DELAY);
  File file = SD.open(filename, FILE_APPEND);
  if (!file) {
    //Serial.println(F("ログファイルを開けませんでした。"));
    xSemaphoreGive(globalMutex);
    return;
  }


  file.print(message);
  file.print(" at ");
  file.print(logTime);
  file.println(".");

  file.close();
  xSemaphoreGive(globalMutex);
  //Serial.println(F("ログデータの書き込みが完了しました。"));
}
