#pragma once

#include <sead/math/seadVector.h>

class KoopaFreerunRecorder {
public:
    struct Frame {
        sead::Vector3f pos, rot;
        int animId;
        float animFrame;
    };

    void recordFrame(Frame const& frame);
private:
    size_t m_filePos = 0;
};