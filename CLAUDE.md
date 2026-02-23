# WetStringReverb — Claude Code 開発ルール

## プロジェクト概要
JUCE (C++17) による弦楽器特化リバーブプラグイン。
3層ハイブリッド構成: OVN Early → FDN Core (OS対応) → DVN Tail

## ビルド
- CMake + Visual Studio 2022
- ビルドコマンド: `cmake --build build --config Release`
- プロセス停止: `Stop-Process -Name "WetStringReverb" -Force -ErrorAction SilentlyContinue`
- テスト: `ctest --test-dir build/`
- OS: Windows (PowerShell)

## コーディングルール
- C++17、JUCE コーディングスタイル
- ヘッダーに `#pragma once` + インライン実装
- .cpp はインクルードのみのスタブ（例: `#include "DSP/DelayLine.h"`）
- パラメータ ID は小文字スネークケース
- APVTS 必須

## processBlock 内の禁止事項（厳守）
- new / malloc / free / delete
- std::vector の生成・リサイズ
- juce::AudioBuffer のコンストラクタ呼び出し・makeCopyOf
- std::string の生成
- juce::String の生成
- 一切のヒープアロケーション
→ 全バッファは prepareToPlay で事前確保し、processBlock では copyFrom のみ使用

## アーキテクチャ
- Input → PreDelay → Early (OVN, 通常レート) + FDN (OSレート) → DVN (通常レート) → ReverbMixer → Output
- FDN のみオーバーサンプリング対象
- FDN内部: DelayRead → FeedbackMatrix → Saturation → ToneFilter → AttenuationFilter → DelayWrite

## ファイル構成
- Source/PluginProcessor.h/.cpp — メインプロセッサ
- Source/PluginEditor.h/.cpp — GUI
- Source/Parameters.h — 全19パラメータ定義
- Source/DSP/*.h — 各DSPモジュール（実装はヘッダー内）
- Source/DSP/*.cpp — スタブのみ
- Tests/*.cpp — juce::UnitTest ベースのテスト

## 修正ルール
- 修正は根拠必須。推測だけで変更しない
- 変更後は必ずビルドして確認
- 3回修正しても解決しない場合はロールバックして報告
- 既存テストの削除・無効化は禁止

## 参考文献
- Dal Santo et al., "Optimizing Tiny Colorless FDNs", EURASIP (2025)
- Fagerström et al., "Velvet-Noise FDN", DAFx-20 (2020)
- Fagerström et al., "Non-Exponential Reverb via DVN", JAES 72(6) (2024)
- Schlecht & Habets, "Scattering in FDNs", IEEE/ACM TASLP (2020)
