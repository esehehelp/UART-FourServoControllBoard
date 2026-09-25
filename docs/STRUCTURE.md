# Documentation Structure

各ドキュメントは 1 つの話題の Single Source of Truth とし、同じ情報を複数箇所に書かない (他からはリンクする)。

| ファイル | 内容 |
| :-- | :-- |
| `README.md` | プロジェクト概要 (特徴・仕様・ビルド方法) |
| `CLAUDE.md` | AI 向けコンテキスト・開発ルール |
| `docs/pinassign.md` | ピンアサイン・コネクタ・チャンネル対応 (回路図とソースで検証済み) |
| `docs/STRUCTURE.md` | このファイル |
| `firmware/docs/PROTOCOL.md` | 通信プロトコル仕様 |
| `firmware/docs/firmware.md` | ファームウェア仕様・設計判断・既知の制約 |
| `firmware/docs/constants.md` | 回路定数とソフトウェア換算係数の対応 |
| `software/README.md` | GUI の機能とビルド |

廃止したファイル (#33): `firmware/docs/pinassign.csv`, `docs/PINASSIGN_VERIFIED.csv` → `docs/pinassign.md` /
`firmware/docs/FIRMWARE_SPEC.md`, `FIRMWARE_RE-DESIGN.md` → `firmware/docs/firmware.md` /
`firmware/docs/constants.txt` → `firmware/docs/constants.md`
