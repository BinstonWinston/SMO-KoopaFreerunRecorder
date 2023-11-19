#include "KoopaFreerunRecorder.hpp"

#include <optional>
#include <string>

#include <nn/fs.h>
#include <al/Library/Memory/HeapUtil.h>
#include <sead/stream/seadStream.h>
#include <sead/stream/seadStreamSrc.h>

#include <sead/math/seadQuatCalcCommon.h>
#include <al/Library/LiveActor/ActorPoseKeeper.h>

#include "logger/Logger.hpp"

const char* RECORDING_PATH = "sd:/koopafreerun.byml";

bool isFileExist(const char *path) {
    nn::fs::DirectoryEntryType type;
    nn::fs::GetEntryType(&type, path);

    return type == nn::fs::DirectoryEntryType_File;
}

nn::Result writeFileToPath(void *buf, size_t size, const char *path) {
    nn::fs::FileHandle handle;

    if (isFileExist(path)) {
        nn::fs::DeleteFile(path); // remove previous file
    }

    if (nn::fs::CreateFile(path, size)) {
        return 1;
    }

    if (nn::fs::OpenFile(&handle, path, nn::fs::OpenMode_Write)) {
        return 1;
    }

    if (nn::fs::WriteFile(handle, 0, buf, size, nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush))) {
        return 1;
    }


    nn::fs::CloseFile(handle);

    return 0;
}

void KoopaFreerunRecorder::startRecording() {
    m_isRecording = true;
    m_byamlWriter = std::make_unique<al::ByamlWriter>(al::getWorldResourceHeap(), true);
    m_byamlWriter->pushHash();
    m_byamlWriter->addString("HackName", "");
    m_byamlWriter->pushArray("MaterialCode");
        m_byamlWriter->addString("Sand");
        m_byamlWriter->addString("NoCollide");
        m_byamlWriter->addString("Puddle");
        m_byamlWriter->addString("Lawn");
        m_byamlWriter->addString("Soil");
        m_byamlWriter->pop();
    m_byamlWriter->pushArray("ActionName");
        m_byamlWriter->addString("Wait");
        m_byamlWriter->addString("Move");
        m_byamlWriter->addString("Jump");
        m_byamlWriter->addString("Jump2");
        m_byamlWriter->addString("Jump3");
        m_byamlWriter->addString("SpinCapStart");
        m_byamlWriter->addString("NoDamageDown");
        m_byamlWriter->addString("DamageLand");
        m_byamlWriter->addString("SquatStart");
        m_byamlWriter->addString("JumpBroad");
        m_byamlWriter->addString("JumpReverse");
        m_byamlWriter->pop();
    m_byamlWriter->pushArray("ActionNameCap");
        m_byamlWriter->addString("SpinCapStart");
        m_byamlWriter->addString("FlyingWaitR");
        m_byamlWriter->addString("StayR");
        m_byamlWriter->pop();
    m_byamlWriter->pushArray("DataArray");
}

namespace sead {

struct MyRamStreamSrc : sead::StreamSrc
{
    void* mBuffer;
    u32 mLength;
    int mPos;

    u32 read(void* data, u32 size) override {
        memcpy(data, mBuffer+mPos, size);
        mPos += size;
        return size;
    }
    u32 write(const void* data, u32 size) override {
        memcpy(mBuffer+mPos, data, size);
        mPos += size;
        return size;
    }
    u32 skip(s32 offset) override { 
        mPos += offset;
        return offset;
    }
    void rewind() override { mPos = 0; }
    bool isEOF() override { return mPos >= mLength; }
    bool flush() override { return true; }
};

struct MyRamWriteStream : public sead::WriteStream
{
public:
    MyRamWriteStream(void* buffer, u32 length, sead::Stream::Modes mode) {
        mSrc.mBuffer = buffer;
        mSrc.mLength = length;
        mSrc.mPos = 0;
        setMode(mode);
        mSrcPtr = &mSrc;
    }
private:
    MyRamStreamSrc mSrc;
};
}

// static_assert(sizeof(sead::WriteStream) == 0x20, "sead::WriteStream unexpected size");
// static_assert(sizeof(sead::RamWriteStream) == 0x38, "sead::RamWriteStream unexpected size");
// static_assert(sizeof(sead::RamStreamSrc) == 0x18, "sead::RamStreamSrc unexpected size");

void KoopaFreerunRecorder::stopRecording() {
    m_isRecording = false;
    m_byamlWriter->pop();
    m_byamlWriter->pop();

    auto const length = m_byamlWriter->calcHeaderSize() + m_byamlWriter->calcPackSize();

    auto heap = al::getWorldResourceHeap();
    char* buffer = (char*)heap->tryAlloc(length, 8);
    if (!buffer) {
        Logger::log("Out of memory, could not allocate buffer\n");
        return;
    }
    sead::MyRamWriteStream ws(buffer, length, sead::Stream::Modes::Binary);
    m_byamlWriter->write(&ws);
    // const u32 BUF_SIZE = 1;
    // for (int i = 0; i < length; i+=BUF_SIZE) {
    //     writeFile(RECORDING_PATH, i, buffer+i, std::min(BUF_SIZE, length-i));
    // }
    writeFileToPath(buffer, length, RECORDING_PATH);
    // char magicNum[2] = {'Y', 'B'};
    // writeFile(RECORDING_PATH, 0, magicNum, sizeof(magicNum));
}

bool KoopaFreerunRecorder::isRecording() const {
    return m_isRecording;
}

void KoopaFreerunRecorder::recordFrame(PlayerActorBase* playerBase) {
    if (!isRecording() || !m_byamlWriter) {
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
    m_byamlWriter->pushArray();
        m_byamlWriter->addFloat(frame.pos.x);
        m_byamlWriter->addFloat(frame.pos.y);
        m_byamlWriter->addFloat(frame.pos.z);

        m_byamlWriter->addFloat(frame.rot.x);
        m_byamlWriter->addFloat(frame.rot.y);
        m_byamlWriter->addFloat(frame.rot.z);

        m_byamlWriter->addInt(frame.animId);
        m_byamlWriter->addFloat(frame.animFrame);

        m_byamlWriter->addInt(0);
        m_byamlWriter->addInt(0);
        
        m_byamlWriter->pop();
}