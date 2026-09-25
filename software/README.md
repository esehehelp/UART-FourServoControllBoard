# Servo Controller GUI (Go + Wails)

UART-FSCB を USB 経由で操作・監視する PC 用 GUI。バックエンドは Go、フロントエンドは React (TypeScript) で、[Wails v2](https://wails.io/) で 1 つの実行ファイルにまとめています。

## 機能

- **自動接続**: シリアルポートを走査し、`0x02` (センサー読み出し) に応答したポートに接続
  - ボードの USB VID (WCH `0x1A86`) を持つポート → その他の USB ポート → `ttyUSB` → `ttyACM` / `COM*` → `ttyS` の順に試行
- **リアルタイムモニタリング** (~30 fps)
  - 電圧 (V)・電流 (mA)・温度 (°C)・サーボフィードバック CH0-3
  - 電圧・電流は Kalman フィルタで平滑化
  - 2 秒間応答がないとデータを無効扱いにし、ステータスバーに "No sensor data" を表示
- **サーボ制御** (`0x01`): 4ch スライダー、500–2500 µs (ファームウェアのデフォルト範囲)
- **LED 制御** (`0x05`): LED1 / LED2 それぞれ 0–255
- **USB-PD 電圧** (`0x06`): 5 / 9 / 15 / 20 V プリセット + 任意電圧 (mV)
- **位置キャリブレーション** (`0x07`)
  1. サーボをセンター (1500 µs) へ移動
  2. PWM を停止 (`0x09`) し、ユーザがアームを最小位置へ動かして Confirm
  3. 最大位置へ動かして Confirm
  4. 2 点のフィードバック値から傾き・切片を計算しフラッシュへ保存

チャンネル番号と基板上のピンの対応は [`firmware/docs/PROTOCOL.md`](../firmware/docs/PROTOCOL.md) を参照してください。

## ビルド

### 前提条件

- Go (go.mod 参照)
- Node.js / npm (フロントエンド)
- Wails CLI v2: `go install github.com/wailsapp/wails/v2/cmd/wails@latest`
- Linux の場合は Wails の依存パッケージ (GTK3, WebKit2GTK)。`wails doctor` で確認できます
- シリアルポートアクセスのため CGO が必要

### コマンド

```bash
cd software

make build-linux     # -> bin/servo-controller
make build-win       # -> bin/servo-controller.exe
make dev             # ホットリロード付き開発サーバ (wails dev)
make test            # Go ユニットテスト
make clean

./build.sh [linux|windows|all|test|dev|clean]   # 同等のスクリプト
```

## プロジェクト構成

```
software/
├── main.go                    # Wails アプリ起動
├── app.go                     # JS から呼べるメソッド、イベント送出 (~30 fps)
├── config/config.go           # プロトコル定数・スケール係数・各種パラメータ
├── pkg/
│   ├── serial/manager.go      # パケット (Marshal/Unmarshal, CRC8)、ポート検出、送受信 goroutine
│   ├── device/controller.go   # 高レベル API (SetServo, SetLED, ...)、センサーデータ解析
│   ├── device/packet.go       # pkg/serial のパケット型のエイリアス
│   ├── data/ringbuffer.go     # スレッドセーフなリングバッファ + 1D Kalman フィルタ
│   └── calibration/           # 位置キャリブレーションの状態機械
├── frontend/src/              # React UI (StatusBar, ServoControl, LEDControl, PDControl,
│                              #           CalibrationPanel, SensorGraph)
└── test/                      # パケットのユニットテスト
```

## 通信フロー

```
React UI ──(Wails binding)──> App ──> Controller ──> serial.Manager ──> USB-CDC ──> Firmware
React UI <──(Wails events)─── App <── Controller <── rx channel     <──
```

- `serial.Manager` は受信 goroutine でパケットを切り出し、容量 128 の channel に流します (溢れた場合はログに累計破棄数を出力)
- `Controller` は受信パケットの処理と定期的なセンサー要求を 1 つの goroutine で行います
- フロントエンドへは `sensor-data` / `plot-data` / `status` / `cal-status` イベントで通知します

## プロトコル

パケット形式・コマンド一覧は [`firmware/docs/PROTOCOL.md`](../firmware/docs/PROTOCOL.md) を参照してください。
