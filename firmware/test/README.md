# firmware/test

| 種類 | 実機 | 実行方法 |
| :-- | :-- | :-- |
| `host/` — ファームウェアのロジックテスト (パーサ・TTL 転送・エラー応答・ACK・Config CRC)。x86 上でハードウェアをスタブ化してビルド | 不要 (CI で実行) | `make -C firmware/test/host test` |
| `test_protocol_utils.py` — Python プロトコル実装のテスト | 不要 (CI で実行) | `python -m pytest firmware/test/test_protocol_utils.py` |
| `software/cmd/selftest` — 接続したボードへの非破壊チェック (サーボは動かさない) | 必要 | `cd software && go run ./cmd/selftest -port <PORT>` |
| `test_*.py`, `calibrate_servos.py`, `record_sweep.py` など — 実機を対話的に操作するスクリプト | 必要 | `python test_sensor.py <PORT>` 等 |

Python スクリプトのパケット処理はすべて `protocol_utils.py` を使う (独自の CRC / パケット実装を持たない)。
プロトコル仕様は [`../docs/PROTOCOL.md`](../docs/PROTOCOL.md) (v3.0: TTL 付き)。

`gui_control.py` は旧 Python GUI。通常は `software/` の GUI を使う。
