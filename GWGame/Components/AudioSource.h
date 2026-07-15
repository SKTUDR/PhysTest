#pragma once
#include "../Audio/AudioClip.h"

namespace ECS
{

    // ---- AudioSourceComp --------------------------------------------------------
    // Unity の AudioSource に相当するコンポーネント。
    // 純粋データ。XAudio2 ボイスの生ポインタは持たない。
    // AudioSystem が毎フレーム状態を読んで XAudio2 を操作する。
    //
    // 3D サウンドを使う場合は TransformComp::position を発音位置として使う。
    // ----------------------------------------------------------------------------
    struct AudioSourceComp
    {
        // ---- 再生設定 -----------------------------------------------------------
        Audio::ClipID clipId = Audio::INVALID_CLIP_ID;
        float volume = 1.f; // [0, 1]
        float pitch = 1.f;  // 1.0 = 等倍, 2.0 = 1 オクターブ上
        bool loop = false;
        bool playOnStart = false; // エンティティ生成時に自動再生

        // ---- 再生状態（AudioSystem が書き込む・ユーザーは読むだけ）-----------
        enum class State
        {
            Stopped,
            Playing,
            Paused
        } state = State::Stopped;

        // ---- 3D オーディオ設定 -------------------------------------------------
        bool spatialize = false;  // true = TransformComp の位置から 3D 距離減衰を計算
        float maxDistance = 30.f; // この距離以上は無音
        float minDistance = 1.f;  // この距離以内は減衰なし

        // ---- 制御フラグ（ユーザーが書き込む・AudioSystem が処理してリセット）--
        bool requestPlay = false;
        bool requestStop = false;
        bool requestPause = false;
        bool requestResume = false;

        // ---- ヘルパー -----------------------------------------------------------
        void Play() noexcept
        {
            requestPlay = true;
        }
        void Stop() noexcept
        {
            requestStop = true;
        }
        void Pause() noexcept
        {
            requestPause = true;
        }
        void Resume() noexcept
        {
            requestResume = true;
        }

        bool IsPlaying() const noexcept
        {
            return state == State::Playing;
        }
        bool IsStopped() const noexcept
        {
            return state == State::Stopped;
        }
        bool IsPaused() const noexcept
        {
            return state == State::Paused;
        }
    };

} // namespace ECS