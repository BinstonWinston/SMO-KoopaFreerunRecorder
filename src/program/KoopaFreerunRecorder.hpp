#pragma once

#include <string>

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
    size_t m_filePos = 0;

    struct Frame {
        sead::Vector3f pos, rot;
        int animId;
        float animFrame;
    };
    void recordFrame(Frame const& frame);
    void writeString(std::string const& s);
};