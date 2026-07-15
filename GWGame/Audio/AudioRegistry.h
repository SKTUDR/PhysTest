#pragma once
#include "AudioClip.h"

#include <xaudio2.h>
#include <wrl/client.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <vector>
#include <unordered_map>
#include <memory>
#include <stdexcept>
#include <string>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

namespace Audio
{

    // ---- AudioRegistry ----------------------------------------------------------
    // AudioClip の所有と管理を行う。ECS の外側で Game が所有する。
    // .wav / .mp3 / .ogg（MF 経由）を読み込んで PCM に変換して保持する。
    // AudioSourceComp は ClipID だけを持ち、Registry 経由で実体を引く。
    //
    // ModelRegistry と同じ設計原則:
    //   コンポーネント = uint32_t ハンドルのみ
    //   重いリソース = Registry が所有
    // ----------------------------------------------------------------------------
    class AudioRegistry
    {
    public:
        AudioRegistry() = default;
        ~AudioRegistry() = default;

        AudioRegistry(const AudioRegistry&) = delete;
        AudioRegistry& operator=(const AudioRegistry&) = delete;
        AudioRegistry(AudioRegistry&&) = default;
        AudioRegistry& operator=(AudioRegistry&&) = default;

        // ---- 初期化 -------------------------------------------------------------
        // XAudio2 マスタリングボイスが作成済みのあとに呼ぶ
        void Initialize()
        {
            MFStartup(MF_VERSION);
        }

        void Shutdown()
        {
            m_clips.clear();
            MFShutdown();
        }

        // ---- ロード（.wav / .mp3 / MF 対応フォーマット）------------------------
        // 同じパスを 2 回渡すとキャッシュから返す
        ClipID Load(const wchar_t* path)
        {
            std::wstring key(path);
            auto it = m_pathToId.find(key);
            if (it != m_pathToId.end())
                return it->second;

            auto clip = std::make_unique<AudioClip>();
            clip->path = key;
            LoadAudioFile(path, *clip);

            ClipID id = static_cast<ClipID>(m_clips.size());
            m_pathToId[key] = id;
            m_clips.push_back(std::move(clip));
            return id;
        }

        // ---- アクセス -----------------------------------------------------------
        const AudioClip* Get(ClipID id) const noexcept
        {
            if (id >= m_clips.size())
                return nullptr;
            return m_clips[id].get();
        }

        bool IsValid(ClipID id) const noexcept
        {
            return id < m_clips.size() && m_clips[id] != nullptr;
        }

    private:
        std::vector<std::unique_ptr<AudioClip>> m_clips;
        std::unordered_map<std::wstring, ClipID> m_pathToId;

        // ---- Media Foundation で PCM に変換 ------------------------------------
        static void LoadAudioFile(const wchar_t* path, AudioClip& out)
        {
            Microsoft::WRL::ComPtr<IMFSourceReader> reader;
            HRESULT hr = MFCreateSourceReaderFromURL(path, nullptr, &reader);
            if (FAILED(hr))
                throw std::runtime_error("AudioRegistry: ファイルを開けません");

            // PCM フォーマットを指定して展開
            Microsoft::WRL::ComPtr<IMFMediaType> mediaType;
            MFCreateMediaType(&mediaType);
            mediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
            mediaType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
            reader->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), nullptr,
                                        mediaType.Get());

            // フォーマット情報を取得
            Microsoft::WRL::ComPtr<IMFMediaType> outputType;
            reader->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), &outputType);

            UINT32 waveFormatSize = 0;
            WAVEFORMATEX* waveFormat = nullptr;
            MFCreateWaveFormatExFromMFMediaType(outputType.Get(), &waveFormat, &waveFormatSize);
            out.format = *waveFormat;
            CoTaskMemFree(waveFormat);

            // PCM データを読み出す
            out.pcmData.clear();
            while (true)
            {
                Microsoft::WRL::ComPtr<IMFSample> sample;
                DWORD flags = 0;
                reader->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM), 0, nullptr, &flags, nullptr,
                                   &sample);

                if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
                    break;
                if (!sample)
                    continue;

                Microsoft::WRL::ComPtr<IMFMediaBuffer> buffer;
                sample->ConvertToContiguousBuffer(&buffer);

                BYTE* data = nullptr;
                DWORD length = 0;
                buffer->Lock(&data, nullptr, &length);
                out.pcmData.insert(out.pcmData.end(), data, data + length);
                buffer->Unlock();
            }
        }
    };

} // namespace Audio