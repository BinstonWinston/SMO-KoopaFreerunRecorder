#include "KoopaFreerunRecorder.hpp"

#include <optional>
#include <string>

#include <nn/fs.h>

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

void KoopaFreerunRecorder::recordFrame(KoopaFreerunRecorder::Frame const& frame) {
    char buffer[2048];
    int length = sprintf(buffer, R"(
    - - %.2f
      - %.2f
      - %.2f
      - %.2f
      - %.2f
      - %.2f
      - !l %d
      - %.3f
      - !l 0
      - !l 0
)",
    frame.pos.x, frame.pos.y, frame.pos.z,
    frame.rot.x, frame.rot.y, frame.rot.z,
    frame.animId,
    frame.animFrame);
    if (length < 0) {
        return;
    }
    writeFile(RECORDING_PATH, m_filePos, buffer, length);
    m_filePos += length;
}