# Servo Controller GUI (Go + Fyne)

Go言語 + Fyneで実装した、UART経由の4軸サーボコントローラGUI。Pythonの `firmware/test/gui_control.py` の問題を解決する完全な新規実装です。

## 特徴

### 🔧 技術スタック
- **言語**: Go 1.26
- **GUI**: Fyne v2（クロスプラットフォーム）
- **シリアル通信**: jacobsa/go-serial
- **並行処理**: goroutines + channels

### 🎯 主要機能

#### 1. **リアルタイムモニタリング**
- 電圧 (V)
- 電流 (mA)
- 温度 (°C)
- フィードバック電圧 CH0-3 (V)
- Kalmanフィルタで平滑化

#### 2. **サーボ制御**
- 4ch独立制御
- パルス幅: 0-3000μs
- リアルタイムスライダー操作

#### 3. **LED制御** (cmd: 0x05)
- PWMデューティサイクル: 0-255
- スライダー入力

#### 4. **USB-PD制御** (cmd: 0x06)
- プリセット: 5V, 9V, 15V, 20V
- 任意電圧カスタム入力

#### 5. **サーボキャリブレーション**
- 段階的な状態遷移
  - **Init** → 初期化
  - **Coarse** → 100μsステップで粗探索
  - **Fine** → 1μsステップで微調整
  - **Save** → フラッシュに保存
- リップル計測による限界検出
- エラーハンドリング付き

#### 6. **グラフ表示**
- リアルタイムラインプロット（Fyneキャンバス使用）
- 4つの独立したグラフ
- スクロール対応の最新100点を表示

### 📊 UI構成

```
┌─────────────────────────────────────────────────┐
│ Status: Connected: /dev/ttyUSB0                │
│ V: 12.45V  I: 250mA  T: 35.2°C                 │
├──────────────────┬──────────────┬───────────────┤
│ LED Control      │   Logs       │               │
│ ────────────────  │ ────────────  │   Graph 1     │
│ LED: 128/255     │              │               │
│ ███████████████  │ ✓ Connected  │   Graph 2     │
│                  │ ✓ LED set... │               │
│ USB-PD Voltage   │              │   Graph 3     │
│ ────────────────  │ ✓ CH0:1500us │               │
│ [5V] [9V] [15V]  │              │   Graph 4     │
│ [20V] [Custom]   │              │               │
│ Apply            │              │               │
│                  │              │               │
│ Servos           │              │               │
│ ────────────────  │              │               │
│ CH0: 1500μs      │              │               │
│ CH1: 1500μs      │              │               │
│ CH2: 1500μs      │              │               │
│ CH3: 1500μs      │              │               │
│                  │              │               │
│ Calibration      │              │               │
│ ────────────────  │              │               │
│ Ready            │              │               │
│ [CH0] [CH1]...   │              │               │
│ [Start] [Stop]   │              │               │
└──────────────────┴──────────────┴───────────────┘
```

## ビルド方法

### 前提条件
- Go 1.21以上
- Linux/macOS/Windows

### 簡単なビルド（推奨）

```bash
cd software
./build.sh all
```

**出力:**
- Linux: `bin/linux/servo-controller` (32MB)
- Windows: `bin/windows/servo-controller.exe` (32MB) ※Windows クロスコンパイラが必要

### プラットフォーム別ビルド

```bash
# Linux のみ
./build.sh linux

# Windows のみ（mingw-w64 が必要）
./build.sh windows

# テスト実行
./build.sh test

# クリーン
./build.sh clean

# ヘルプ
./build.sh help
```

### 使用している Make コマンド

```bash
# 全プラットフォームビルド
make all

# Linux ビルド
make build-linux

# Windows ビルド
make build-win

# テスト
make test

# クリーン
make clean
```

### 実行

```bash
# Linux
./bin/linux/servo-controller

# Windows
./bin/windows/servo-controller.exe
```

## ビルド出力

```
bin/
├── linux/
│   └── servo-controller        # Linux 64-bit実行ファイル
└── windows/
    └── servo-controller.exe     # Windows 64-bit実行ファイル
```

### Windows クロスコンパイル環境のセットアップ

```bash
# Ubuntu/Debian
sudo apt-get install mingw-w64

# macOS
brew install mingw-w64

# Fedora/CentOS
sudo dnf install mingw64-gcc mingw64-gcc-c++
```

## プロジェクト構成

```
software/
├── config/
│   └── config.go              # 定数・パラメータ
├── pkg/
│   ├── serial/
│   │   └── manager.go         # UART通信管理
│   ├── device/
│   │   ├── packet.go          # パケット構造・CRC8
│   │   └── controller.go      # デバイス制御・データ解析
│   ├── data/
│   │   └── ringbuffer.go      # リングバッファ・Kalmanフィルタ
│   ├── ui/
│   │   ├── widgets.go         # UIコンポーネント
│   │   └── graph.go           # グラフレンダリング
│   └── calibration/
│       └── state_machine.go   # キャリブレーション状態機械
├── test/
│   └── packet_test.go         # ユニットテスト
├── main.go                     # メインアプリケーション
├── go.mod                      # モジュール定義
├── go.sum                      # 依存関係ロック
└── servo-controller            # ビルド済み実行ファイル
```

## コード行数

| ファイル | 行数 |
|---------|------|
| config.go | 104 |
| packet.go | 107 |
| ringbuffer.go | 115 |
| manager.go | 343 |
| controller.go | 227 |
| graph.go | 268 |
| widgets.go | 332 |
| state_machine.go | 336 |
| main.go | 187 |
| **合計** | **2,019** |

## プロトコル仕様

### コマンド一覧

| Cmd | 説明 | データ形式 | 応答 |
|-----|------|----------|------|
| 0x01 | サーボ制御 | [ch, duty_h, duty_l] | なし |
| 0x02 | センサー読込 | [type] | 0x82: [v_h, v_l, ...] |
| 0x03 | 全サーボ同期 | [ch0_h, ch0_l, ...] | なし |
| 0x05 | LED制御 | [duty: 0-255] | なし |
| 0x06 | PD電圧設定 | [mv_h, mv_l] | なし |
| 0x07 | キャリ保存 | [ch, slope(4B), intercept(4B), min_h, min_l, max_h, max_l] | なし |

### パケット構造

```
Byte 0:   Header (0xAA)
Byte 1:   Target ID
Byte 2:   Source ID (0x00)
Byte 3:   Command
Byte 4:   Data Length
Byte 5+:  Data
Final:    CRC8 checksum
```

## スレッド安全性

- **SerialManager**: goroutines + channels で受信を管理
- **RingBuffer**: sync.RWMutex で保護
- **UI更新**: Fyneのイベントループで同期

## 改善点（Pythonからの向上）

### 🔴 Pythonの問題点

```python
# Python: スレッド不安全
c.v_history.append(v)  # 別スレッドで読込中に追記
while len(buffer) >= 6:  # バッファ操作が同期されない
```

### 🟢 Goでの解決

```go
// Go: スレッド安全
c.volts.Push(filteredV)  // RWMutex で保護
for pkt := <-m.rxChan:  // channels で明確な通信フロー
```

### 主な改善

| 項目 | Python | Go |
|------|--------|-----|
| スレッド管理 | threading.Thread | goroutines |
| データ保護 | threading.Lock (不完全) | sync.RWMutex + channels |
| 並行処理 | 複数スレッド + キュー | チャネル型通信 |
| エラーハンドリング | try-except | error型 + グレースフルエラー |
| UI更新 | root.after() | Fyne イベントループ |
| グラフ | matplotlib | Fyneキャンバス（軽量） |

## デバッグ方法

### ログ出力

```bash
GOLOG=debug ./servo-controller
```

### パケット監視

シリアルモニタで送受信パケットを確認：

```bash
minicom -D /dev/ttyUSB0 -b 115200 -8 -N
```

## 今後の改善予定

- [ ] グラフの高度なプロット（X軸ラベル、Y軸スケール表示）
- [ ] センサーデータの CSV エクスポート
- [ ] キャリブレーション結果の可視化
- [ ] マクロ記録・再生機能
- [ ] Web UI（Wails使用）

## ライセンス

MIT License

## 作成者

Copilot (GitHub)

## 更新履歴

- **2026-03-11**: v1.0 初版リリース（Go + Fyne 完全実装）
  - シリアル通信、パケット処理
  - LED/PD/サーボ制御
  - キャリブレーション状態機械
  - リアルタイムグラフ表示
  - ユニットテスト
