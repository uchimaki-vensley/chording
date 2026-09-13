# Chording

Windows 11 / Cubase向けの、リアルタイムMIDIコード解析・コード進行支援プラグインです。
音声は解析せず、MIDIノートイベントだけを使用します。

## 現在の機能

- 演奏中のコード名、構成音、確信度、別解を表示
- 140ms安定したコードを進行履歴へ記録
- Note Off中に生じる不完全なコードを履歴から除外
- メジャー／マイナーキーの自動推定と手動固定
- ベーシック、ポップス、ロック、ジャズの次コード候補
- 明るい、切ない、緊張、意外性による候補順位の調整
- 借用和音の有効／無効切り替え
- シャープ／フラット表記切り替え
- 履歴のUndo、消去、クリップボードへのコピー
- Cubaseプロジェクト内への設定と進行履歴の保存

対応コードの詳細は [要件定義](docs/requirements.md) を参照してください。

## 必要なもの

- Windows 11
- Visual Studio 2022（Desktop development with C++）
- CMake 3.22以上
- Git

初回構成時にCMakeがJUCE 8.0.13を取得します。JUCEの利用条件は
[JUCEライセンス](https://juce.com/legal/)を確認してください。

## ビルド

Developer PowerShell for VS 2022で実行します。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

JUCEを使わず解析コアだけを検証する場合は、次のように構成できます。

```powershell
cmake -S . -B build-core -DCHORDING_BUILD_PLUGIN=OFF
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

生成物は通常、次に作成されます。

```text
build/Chording_artefacts/Release/VST3/Chording.vst3
build/Release/ChordingMidiInsert.dll
```

`Chording.vst3`フォルダーを次へ配置し、CubaseのVSTプラグインマネージャーで
再スキャンします。

```text
C:\Program Files\Common Files\VST3
```

## Cubaseでの接続

Cubase Artist 14では、インストゥルメントトラックのInspectorにある「MIDI Inserts」へ
`Chording MIDI`を追加します。追加のMIDIトラックやルーティングは不要です。通常の
オーディオInsertsへ`Chording.vst3`を追加しても、その位置には押鍵MIDIが届きません。

MIDI Insertのインストール方法は [Cubase Artist 14 MIDI Insert](docs/midi-insert.md) を参照してください。

従来のVST3を使う場合は次のように接続します。

1. オーディオトラック、グループ、またはFXチャンネルのInsertにChordingを追加します。
2. MIDIトラックを追加し、その出力先にChordingを選びます。
3. MIDIトラックのモニターまたは録音待機を有効にして演奏します。

Chordingは音声を変更せず、そのまま出力します。無音チャンネル上でMIDIを認識しない場合は、
Cubaseの「音声信号を受信していないときにVST3プラグインの処理を停止」設定を無効にしてください。

## プロジェクト構成

```text
Source/ChordDetector.*    JUCE非依存のコード認識
Source/HarmonyAdvisor.*  キー推定と次コード候補
Source/PluginProcessor.* MIDI受信、履歴、状態保存
Source/PluginEditor.*    プラグインUI
Tests/                   解析コアの単体テスト
```
