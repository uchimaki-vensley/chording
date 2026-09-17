# Chording

Chordingは、Windows 11 / Cubase向けのリアルタイムMIDIコード解析・コード進行支援プラグインです。音声は解析せず、MIDIノートから現在のコード、構成音、別解、次のコード候補を表示します。

Cubase Artist 14では、インストゥルメントトラックの**MIDI Inserts**へ`Chording MIDI`を追加する使い方を推奨します。演奏用トラックだけで解析でき、追加のMIDIトラックやルーティングは必要ありません。

## 主な機能

- ライブ演奏と記録済みMIDIのコードをリアルタイム解析
- コード名、構成音、確信度、別解を表示
- 140ms安定したコードを進行履歴へ記録
- メジャー／マイナーキーの自動推定と手動指定
- ベーシック、ポップス、ロック、ジャズに応じた次コード候補
- 明るい、切ない、緊張、意外性などによる候補順位の調整
- 借用和音の有効／無効切り替え
- シャープ／フラット表記の切り替え
- 履歴のUndo、消去、クリップボードへのコピー
- 設定と進行履歴をCubaseプロジェクトへ保存

サステインペダル（CC64）は音源へそのまま渡します。コード解析には現在物理的に押している鍵盤だけを使うため、ペダルで鳴り続けている離鍵済みノートはコード表示に残りません。

対応コードの詳細は[要件定義](docs/requirements.md)を参照してください。

## 動作環境

### 使用

- Windows 11
- Cubase Artist 14
- 64-bit環境

### ビルド

- Visual Studio 2022（Desktop development with C++）
- CMake 3.22以上
- Git
- 初回構成時にJUCE 8.0.13を取得するためのインターネット接続

公式ビルドはJUCEをAGPLv3で使用します。JUCEと同梱SDKを含む第三者ソフトウェアの条件は[Third-party notices](THIRD_PARTY_NOTICES.md)を確認してください。

## ビルド

Developer PowerShell for Visual Studio 2022で実行します。

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

生成物は次の場所に作成されます。

```text
build/Release/ChordingMidiInsert.dll
build/Chording_artefacts/Release/VST3/Chording.vst3
```

JUCEを使わない解析コアだけを検証する場合は、次のようにビルドします。

```powershell
cmake -S . -B build-core -DCHORDING_BUILD_PLUGIN=OFF
cmake --build build-core --config Release
ctest --test-dir build-core -C Release --output-on-failure
```

## Cubase Artist 14で使う

### MIDI Insert（推奨）

1. Cubaseを終了します。
2. 管理者権限のPowerShellでDLLとライセンスを配置します。

   ```powershell
   $destination = 'C:\Program Files\Steinberg\Cubase 14\Components\ChordingMidi'
   New-Item -ItemType Directory -Force -Path $destination
   Copy-Item 'build\Release\ChordingMidiInsert.dll' "$destination\ChordingMidi.dll"
   Copy-Item 'ThirdParty\vst-ma\LICENSE.txt' "$destination\LICENSE.txt"
   ```

3. Cubaseを起動し、使用するインストゥルメントトラックを選択します。
4. Inspectorの**MIDI Inserts**を開き、`Chording MIDI`を追加します。
5. MIDI Insertの編集ボタンを押してChording画面を開きます。

受信したノート、ペダル、その他のMIDIイベントは変更せず音源へ渡されます。トランスポートの開始、停止、位置移動、サイクル折り返し時には解析状態をクリアし、ノートの残留を防ぎます。

通常のオーディオInsertsへ`Chording.vst3`を追加しても、インストゥルメントトラックの入力MIDIは届きません。詳しい仕様と配置方法は[Cubase Artist 14 MIDI Insert](docs/midi-insert.md)を参照してください。

### VST3（従来方式）

VST3版を使用する場合は、`Chording.vst3`フォルダーを次へ配置し、CubaseのVSTプラグインマネージャーで再スキャンします。

```text
C:\Program Files\Common Files\VST3
```

VST3版にはMIDIを別途ルーティングする必要があります。

1. オーディオトラック、グループ、またはFXチャンネルのInsertにChordingを追加します。
2. MIDIトラックを追加し、その出力先にChordingを指定します。
3. MIDIトラックのモニターまたは録音待機を有効にします。

音声は変更せず、そのまま出力します。無音チャンネル上でMIDIだけを解析する場合は、Cubaseの「無音信号を受信していないときにVST3プラグインの処理を停止」を無効にしてください。

## プロジェクト構成

```text
Source/ChordInputState.*    物理的な押鍵状態の追跡
Source/ChordDetector.*      コード解析
Source/HarmonyAdvisor.*     キー推定と次コード候補
Source/MidiInsert.*         Cubase MIDI Insertアダプター
Source/MidiInsertState.*    ライブ入力と再生ノートの追跡
Source/PluginProcessor.*    MIDI受信、履歴、状態保存
Source/PluginEditor.*       プラグインUI
Tests/                      解析コアとMIDI Insertのテスト
ThirdParty/vst-ma/          Steinberg VST-MAインターフェース
```

## Releaseパッケージ

Releaseでは、MIDI Insert版とVST3版を別々のZIPで配布します。どちらにもライセンス本文と第三者ライセンスを同梱し、同じReleaseに対応ソースZIPと`SHA256SUMS.txt`を添付します。

ビルド・テスト後、クリーンなチェックアウトで次を実行すると`dist`に配布ファイルが生成されます。

```powershell
powershell -ExecutionPolicy Bypass -File scripts/package-release.ps1 `
  -BuildDirectory build -OutputDirectory dist
```

詳細は[Release procedure](docs/releasing.md)を参照してください。

## ライセンス

Chordingは[GNU Affero General Public License v3.0 only](LICENSE)で公開します。利用、改変、再配布を行う場合はAGPLv3の条件に従ってください。第三者ソフトウェアの著作権表示とライセンスは[Third-party notices](THIRD_PARTY_NOTICES.md)にまとめています。

コントリビューションの扱いは[CONTRIBUTING.md](CONTRIBUTING.md)を参照してください。
