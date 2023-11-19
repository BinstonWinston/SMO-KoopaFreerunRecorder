#pragma once

#include <string>
#include <memory>

#include <al/Library/Yaml/Writer/ByamlWriter.h>

#include <sead/math/seadVector.h>
#include <game/Player/PlayerActorBase.h>

class KoopaFreerunRecorder {
public:
    void startRecording();
    void stopRecording();
    bool isRecording() const;
    void recordFrame(PlayerActorBase* playerBase);
private:
    bool m_isRecording = false;
    std::unique_ptr<al::ByamlWriter> m_byamlWriter = nullptr;

    struct Frame {
        sead::Vector3f pos, rot;
        int animId;
        float animFrame;
    };
    void recordFrame(Frame const& frame);
};