#include "KoopaFreerunRecorder.hpp"

#include <optional>
#include <string>

#include <nn/fs.h>

#include <sead/math/seadQuatCalcCommon.h>
#include <al/Library/LiveActor/ActorPoseKeeper.h>

const char* RECORDING_PATH = "sd:/koopafreerun.yaml";

Result writeFile(std::string const& str, s64 offset, const void* data, size_t length) {
    nn::fs::DirectoryEntryType entryType;
    Result rc = nn::fs::GetEntryType(&entryType, str.c_str());

    if (rc == 0x202) {  // Path does not exist
        R_TRY(nn::fs::CreateFile(str.c_str(), offset + length));
    } else if (R_FAILED(rc))
        return rc;

    if (entryType == nn::fs::DirectoryEntryType_Directory) return -1;

    nn::fs::FileHandle handle;
    R_TRY(nn::fs::OpenFile(&handle, str.c_str(), nn::fs::OpenMode_ReadWrite | nn::fs::OpenMode_Append));

    Result r;
    s64 fileSize;
    r = nn::fs::GetFileSize(&fileSize, handle);
    if (R_FAILED(r)) {
        nn::fs::CloseFile(handle);
        return r;
    }

    if (fileSize < offset + (s64)length) {  // make sure we have enough space
        r = nn::fs::SetFileSize(handle, offset + length);

        if (R_FAILED(r)) {
            nn::fs::CloseFile(handle);
            return r;
        }
    }

    r = nn::fs::WriteFile(handle, offset, data, length,
                          nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush));

    nn::fs::CloseFile(handle);
    return r;
}

void deleteFileIfExists(std::string const& path) {
    nn::fs::DeleteFile(path.c_str());
}

void KoopaFreerunRecorder::startRecording() {
    m_isRecording = true;
    deleteFileIfExists(RECORDING_PATH);
    m_filePos = 0;
    writeString("HackName: null\nMaterialCode: [Sand, NoCollide, Puddle, Lawn, Soil]\nActionName: [Wait, Move, Jump, Jump2, Jump3, SpinCapStart, NoDamageDown, DamageLand, SquatStart, JumpBroad, JumpReverse]\nActionNameCap: [SpinCapStart, FlyingWaitR, StayR]\nDataArray:\n");
}

void KoopaFreerunRecorder::stopRecording() {
    m_isRecording = false;
}

bool KoopaFreerunRecorder::isRecording() const {
    return m_isRecording;
}

void KoopaFreerunRecorder::recordFrame(PlayerActorBase* playerBase) {
    if (!isRecording()) {
        return;
    }

    sead::Vector3f rot;
    sead::QuatCalcCommon<float>::calcRPY(rot, al::getQuat(playerBase));
    rot *= 180.f/std::numbers::pi; // radians to degrees
    KoopaFreerunRecorder::Frame frame{
        .pos = al::getTrans(playerBase),
        .rot = rot,
        .animId = 1,
        .animFrame = 0,
    };
    recordFrame(frame);
}

void KoopaFreerunRecorder::recordFrame(KoopaFreerunRecorder::Frame const& frame) {
    char buffer[2048];
    int length = sprintf(buffer, "- [%.2f, %.2f, %.2f, %.2f, %.2f, %.2f, !l %d, %.3f, !l 0, !l 0]\n",
        frame.pos.x, frame.pos.y, frame.pos.z,
        frame.rot.x, frame.rot.y, frame.rot.z,
        frame.animId,
        frame.animFrame);
    if (length < 0) {
        return;
    }
    writeString(std::string(buffer, length));
}

void KoopaFreerunRecorder::writeString(std::string const& s) {
    writeFile(RECORDING_PATH, m_filePos, s.c_str(), s.size());
    m_filePos += s.size();
}