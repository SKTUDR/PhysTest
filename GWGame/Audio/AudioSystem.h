#pragma once
#include "AudioClip.h"
#include "AudioRegistry.h"
#include "../ECS/World.h"
#include "../ECS/Query.h"
#include "../Components/Transform.h"
#include "../Components/AudioSource.h"
#include "../Components/AudioListener.h"

#include <xaudio2.h>
#include <wrl/client.h>
#include <unordered_map>
#include <vector>
#include <cmath>
#include <algorithm>
#include <stdexcept>

#pragma comment(lib, "xaudio2.lib")

namespace Audio
{

    // ---- VoiceEntry -------------------------------------------------------------
    // AudioSystem が内部で管理する XAudio2 ソースボイスのキャッシュ。
    // エンティティ 1 つにつき 1 ボイスを割り当てる。
    // ----------------------------------------------------------------------------
    struct VoiceEntry
    {
        IXAudio2SourceVoice* voice = nullptr;
        Audio::ClipID clipId = Audio::INVALID_CLIP_ID;
        bool isLooping = false;
    };

    // ---- AudioSystem ------------------------------------------------------------
    // 毎フレーム Update() を呼ぶことで以下を処理する:
    //
    //   1. AudioListenerComp の位置・向きを取得
    //   2. AudioSourceComp の requestPlay/Stop/Pause/Resume フラグを処理
    //   3. 3D サウンド: Listener とのを距離に応じて volume を計算
    //   4. playOnStart: 初回フレームに自動再生
    //   5. ボイスの volume / pitch を XAudio2 に反映
    //
    // PlayOneShot(): ワンショット再生（完了で自動破棄）
    // ----------------------------------------------------------------------------
    class AudioSystem
    {
    public:
        // ---- 初期化 -------------------------------------------------------------
        void Initialize(AudioRegistry& registry)
        {
            m_registry = &registry;

            // XAudio2 エンジン生成
            HRESULT hr = XAudio2Create(m_xaudio2.GetAddressOf(), 0);
            if (FAILED(hr))
                throw std::runtime_error("XAudio2Create failed");

            // マスタリングボイス生成（最終出力先）
            hr = m_xaudio2->CreateMasteringVoice(&m_masterVoice);
            if (FAILED(hr))
                throw std::runtime_error("CreateMasteringVoice failed");
        }

        void Shutdown()
        {
            // 全ボイスを破棄
            for (auto& [eid, entry] : m_voices)
            {
                if (entry.voice)
                {
                    entry.voice->Stop();
                    entry.voice->DestroyVoice();
                }
            }
            m_voices.clear();

            if (m_masterVoice)
            {
                m_masterVoice->DestroyVoice();
                m_masterVoice = nullptr;
            }
            m_xaudio2.Reset();
        }

        // ---- マスターボリューム -------------------------------------------------
        void SetMasterVolume(float volume) noexcept
        {
            if (m_masterVoice)
                m_masterVoice->SetVolume(volume);
        }

        // ---- PlayOneShot --------------------------------------------------------
        // ECS エンティティを使わずにワンショット再生する。
        // GrenadeSystem などイベント駆動の効果音に使う。
        // pos: ワールド空間の発音位置（3D 距離減衰に使う）
        void PlayOneShot(ClipID clipId, float volume = 1.f,
                         const DirectX::SimpleMath::Vector3& pos = DirectX::SimpleMath::Vector3::Zero)
        {
            const AudioClip* clip = m_registry->Get(clipId);
            if (!clip || clip->pcmData.empty())
                return;

            IXAudio2SourceVoice* voice = nullptr;
            HRESULT hr = m_xaudio2->CreateSourceVoice(&voice, &clip->format);
            if (FAILED(hr) || !voice)
                return;

            // 3D 距離減衰
            const float attenuated = volume * CalcAttenuation(pos, 1.f, 30.f);

            XAUDIO2_BUFFER buf = {};
            buf.AudioBytes = static_cast<UINT32>(clip->pcmData.size());
            buf.pAudioData = clip->pcmData.data();
            buf.Flags = XAUDIO2_END_OF_STREAM;

            voice->SubmitSourceBuffer(&buf);
            voice->SetVolume(attenuated);
            voice->Start();

            // ワンショットは完了後に自動破棄させる
            // （XAudio2 の XAUDIO2_VOICE_NOSAMPLESPLAYED フラグで管理）
            m_oneShotVoices.push_back(voice);
        }

        // ---- Update -------------------------------------------------------------
        void Update(ECS::World& world)
        {
            if (!m_xaudio2 || !m_registry)
                return;

            // ---- 1. Listener の位置・向きを取得 ---------------------------------
            DirectX::SimpleMath::Vector3 listenerPos = DirectX::SimpleMath::Vector3::Zero;
            DirectX::SimpleMath::Quaternion listenerRot = DirectX::SimpleMath::Quaternion::Identity;
            bool listenerFound = false;

            auto listenerDesc = ECS::QueryBuilder{}.All<ECS::TransformComp, ECS::AudioListenerComp>().Build();

            world.Query(listenerDesc)
                .Each<ECS::TransformComp>(
                    [&](ECS::EntityID, const ECS::TransformComp& tr)
                    {
                        if (listenerFound)
                            return; // 最初の 1 つだけ使う
                        listenerPos = tr.position;
                        listenerRot = tr.rotation;
                        listenerFound = true;
                    });

            // ---- 2. AudioSourceComp を持つ全エンティティを処理 ------------------
            auto sourceDesc = ECS::QueryBuilder{}.All<ECS::AudioSourceComp>().Build();

            world.Query(sourceDesc)
                .Each<ECS::AudioSourceComp>(
                    [&](ECS::EntityID eid, ECS::AudioSourceComp& src)
                    {
                        // ClipID が無効なら何もしない
                        if (src.clipId == INVALID_CLIP_ID)
                            return;

                        // ---- playOnStart: 初回に自動再生 --------------------------------
                        if (src.playOnStart && src.IsStopped() && !IsVoiceActive(eid))
                        {
                            src.requestPlay = true;
                            src.playOnStart = false; // 一度だけ
                        }

                        // ---- フラグ処理 -------------------------------------------------
                        if (src.requestPlay)
                        {
                            DoPlay(eid, src);
                            src.requestPlay = false;
                        }
                        if (src.requestStop)
                        {
                            DoStop(eid, src);
                            src.requestStop = false;
                        }
                        if (src.requestPause)
                        {
                            DoPause(eid, src);
                            src.requestPause = false;
                        }
                        if (src.requestResume)
                        {
                            DoResume(eid, src);
                            src.requestResume = false;
                        }

                        // ---- 3. volume / pitch を毎フレーム更新 ------------------------
                        auto it = m_voices.find(eid.value);
                        if (it == m_voices.end())
                            return;
                        VoiceEntry& entry = it->second;
                        if (!entry.voice)
                            return;

                        float finalVolume = src.volume;

                        // 3D 距離減衰
                        if (src.spatialize && world.HasComponent<ECS::TransformComp>(eid))
                        {
                            const auto& tr = world.GetComponent<ECS::TransformComp>(eid);
                            finalVolume *= CalcAttenuation(tr.position, listenerPos, src.minDistance, src.maxDistance);
                        }

                        entry.voice->SetVolume(std::clamp(finalVolume, 0.f, 1.f));

                        // pitch: XAudio2 の周波数比率 = pitch（1.0 = 等倍）
                        entry.voice->SetFrequencyRatio(std::max(src.pitch, 0.01f));

                        // ---- 4. 再生完了の検出（ループなし）-----------------------------
                        if (!src.loop)
                        {
                            XAUDIO2_VOICE_STATE state;
                            entry.voice->GetState(&state);
                            if (state.BuffersQueued == 0 && src.IsPlaying())
                            {
                                src.state = ECS::AudioSourceComp::State::Stopped;
                            }
                        }
                    });

            // ---- 5. ワンショットボイスの後処理 ----------------------------------
            CleanupOneShotVoices();
        }

        // ---- アクセサ -----------------------------------------------------------
        IXAudio2* GetXAudio2() const noexcept
        {
            return m_xaudio2.Get();
        }

    private:
        Microsoft::WRL::ComPtr<IXAudio2> m_xaudio2;
        IXAudio2MasteringVoice* m_masterVoice = nullptr;
        AudioRegistry* m_registry = nullptr;

        // EntityID.value → VoiceEntry
        std::unordered_map<uint32_t, VoiceEntry> m_voices;

        // ワンショット再生中のボイスリスト
        std::vector<IXAudio2SourceVoice*> m_oneShotVoices;

        // ---- 再生 ---------------------------------------------------------------
        void DoPlay(ECS::EntityID eid, ECS::AudioSourceComp& src)
        {
            const AudioClip* clip = m_registry->Get(src.clipId);
            if (!clip || clip->pcmData.empty())
                return;

            // 既存のボイスを止めて作り直す
            DoStop(eid, src);

            IXAudio2SourceVoice* voice = nullptr;
            HRESULT hr = m_xaudio2->CreateSourceVoice(&voice, &clip->format);
            if (FAILED(hr) || !voice)
                return;

            XAUDIO2_BUFFER buf = {};
            buf.AudioBytes = static_cast<UINT32>(clip->pcmData.size());
            buf.pAudioData = clip->pcmData.data();
            buf.Flags = XAUDIO2_END_OF_STREAM;
            if (src.loop)
                buf.LoopCount = XAUDIO2_LOOP_INFINITE;

            voice->SubmitSourceBuffer(&buf);
            voice->SetVolume(std::clamp(src.volume, 0.f, 1.f));
            voice->SetFrequencyRatio(std::max(src.pitch, 0.01f));
            voice->Start();

            m_voices[eid.value] = {voice, src.clipId, src.loop};
            src.state = ECS::AudioSourceComp::State::Playing;
        }

        void DoStop(ECS::EntityID eid, ECS::AudioSourceComp& src)
        {
            auto it = m_voices.find(eid.value);
            if (it == m_voices.end())
                return;

            if (it->second.voice)
            {
                it->second.voice->Stop();
                it->second.voice->FlushSourceBuffers();
                it->second.voice->DestroyVoice();
            }
            m_voices.erase(it);
            src.state = ECS::AudioSourceComp::State::Stopped;
        }

        void DoPause(ECS::EntityID eid, ECS::AudioSourceComp& src)
        {
            auto it = m_voices.find(eid.value);
            if (it == m_voices.end() || !it->second.voice)
                return;
            it->second.voice->Stop(); // Stop = 一時停止（バッファは維持）
            src.state = ECS::AudioSourceComp::State::Paused;
        }

        void DoResume(ECS::EntityID eid, ECS::AudioSourceComp& src)
        {
            auto it = m_voices.find(eid.value);
            if (it == m_voices.end() || !it->second.voice)
                return;
            it->second.voice->Start();
            src.state = ECS::AudioSourceComp::State::Playing;
        }

        bool IsVoiceActive(ECS::EntityID eid) const noexcept
        {
            return m_voices.count(eid.value) > 0;
        }

        // ---- 3D 距離減衰（線形減衰）--------------------------------------------
        static float CalcAttenuation(const DirectX::SimpleMath::Vector3& sourcePos,
                                     const DirectX::SimpleMath::Vector3& listenerPos, float minDist,
                                     float maxDist) noexcept
        {
            const float dist = (sourcePos - listenerPos).Length();
            if (dist <= minDist)
                return 1.f;
            if (dist >= maxDist)
                return 0.f;
            return 1.f - (dist - minDist) / (maxDist - minDist);
        }

        // PlayOneShot の距離減衰（Listener 位置なし版）
        static float CalcAttenuation(const DirectX::SimpleMath::Vector3& /*pos*/, float /*minDist*/,
                                     float /*maxDist*/) noexcept
        {
            return 1.f; // Listener なし = 減衰なし
        }

        // ---- ワンショットの後処理 -----------------------------------------------
        void CleanupOneShotVoices()
        {
            m_oneShotVoices.erase(std::remove_if(m_oneShotVoices.begin(), m_oneShotVoices.end(),
                                                 [](IXAudio2SourceVoice* v)
                                                 {
                                                     if (!v)
                                                         return true;
                                                     XAUDIO2_VOICE_STATE state;
                                                     v->GetState(&state);
                                                     if (state.BuffersQueued == 0)
                                                     {
                                                         v->Stop();
                                                         v->DestroyVoice();
                                                         return true;
                                                     }
                                                     return false;
                                                 }),
                                  m_oneShotVoices.end());
        }
    };

} // namespace Audio