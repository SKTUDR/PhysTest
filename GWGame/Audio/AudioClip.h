#pragma once
#include <xaudio2.h>
#include <cstdint>
#include <string>
#include <vector>

namespace Audio
{

    using ClipID = uint32_t;
    static constexpr ClipID INVALID_CLIP_ID = UINT32_MAX;

    // ---- AudioClip --------------------------------------------------------------
    // 1 つのオーディオアセットの PCM データと再生パラメータを保持する。
    // AudioRegistry が所有し、AudioSourceComp は ClipID だけを持つ。
    // ----------------------------------------------------------------------------
    struct AudioClip
    {
        std::vector<BYTE> pcmData; // デコード済み PCM バイト列
        WAVEFORMATEX format = {};  // サンプルレート・チャンネル数・ビット深度
        std::wstring path;         // デバッグ用パス
        float defaultVolume = 1.f;
        float defaultPitch = 1.f;
        bool defaultLoop = false;
    };

} // namespace Audio