# Documentation Structure

## 既存ドキュメント配置

### Root Level
- **README.md** - プロジェクト概要 (特徴・仕様・開発環境)
- **CLAUDE.md** - AI向けコンテキスト

### firmware/docs/
- **PROTOCOL.md** - プロトコル仕様詳細
- **FIRMWARE_SPEC.md** - ファームウェア機能仕様
- **FIRMWARE_RE-DESIGN.md** - 再設計ノート
- **pinassign.csv** - ピンアサイン（簡略版）

### software/
- **README.md** - Go GUI機能説明

### docs/ (新規)
- **PINASSIGN_VERIFIED.csv** - ソースコード検証版ピンアサイン
- **STRUCTURE.md** - このファイル

## 整理指針

各ドキュメントは機能要件・問題ごとに分離されるべき:

1. **PROTOCOL.md** - コマンド仕様のみ
2. **FIRMWARE_SPEC.md** - モジュール機能のみ
3. **pinassign.csv** - 既存版は簡略版、PINASSIGN_VERIFIED.csvはソース検証版
4. **README.md** - 全体概要のみ
5. **software/README.md** - GUI機能説明のみ

汎用的な「統一ドキュメント」は不要。各ドキュメントは独立して参照可能であること。

