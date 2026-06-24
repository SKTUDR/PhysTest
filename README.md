# PhysTest

## 概要

DirectX11を用いて開発中の3D物理エンジンです。

主な目的は、ゲームエンジンの基礎技術である

* ECS（Entity Component System）
* 衝突判定
* 剛体シミュレーション
* イベントシステム

の理解と実装です。

ソリューション名は開発当初の名称「GWGame」のままになっています。
現在は物理エンジン開発用プロジェクトとして利用しています。

## 開発環境

* OS : Windows 11
* Visual Studio 2022 Community
* C++
* DirectXTK
* DirectXMath

## 実装済み機能

### ECS

* Entity管理
* Component管理
* Queryシステム
* EventQueue

### Collision Detection

* AABB vs AABB
* OBB vs OBB (SAT)

### Physics

* 重力
* インパルス法による衝突解決
* 位置補正
* 回転を考慮した剛体運動

## 工夫した点

### ECSアーキテクチャ

ゲームオブジェクト間の依存を減らすため、Entity-Component-Systemを採用しました。

### ECS設計

大量のエンティティを効率的に処理することを目的として
Archetype ECSを採用しました。

Component構成ごとにエンティティをチャンクへ格納することで、
システム実行時のキャッシュ効率向上を狙っています。

また、EntityIDとRecordによる間接参照を採用し、
エンティティの移動や削除が発生しても高速にアクセスできるよう設計しました。

さらに、利用者が必要なコンポーネント群を宣言的に指定するだけで
対象エンティティを取得できるクエリシステムも実装しました。

### SATによるOBB衝突判定

15軸分離軸テストを実装し、回転したボックス同士の衝突判定に対応しています。

### 物理シミュレーション

衝突解決にはインパルス法を採用し、複数回のソルバ反復によって安定性を向上させています。

## 実行方法

1. Visual Studio 2026でソリューションを開く
2. x64 / Debug または Release を選択
3. 実行

## 今後の予定

* 多点接触
* Broad Phaseの改善
* ジョイント機能
* Continuous Collision Detection
* マルチスレッド化
* OBB vs その他形状の衝突判定
