#include <numbers>

#include "lib.hpp"
#include "imgui_backend/imgui_impl_nvn.hpp"
#include "patches.hpp"
#include "logger/Logger.hpp"
#include "fs.h"
#include "helpers/InputHelper.h"
#include "helpers/PlayerHelper.h"
#include "imgui_nvn.h"
#include "ExceptionHandler.h"

#include <basis/seadRawPrint.h>
#include <prim/seadSafeString.h>
#include <resource/seadResourceMgr.h>
#include <filedevice/nin/seadNinSDFileDeviceNin.h>
#include <filedevice/seadFileDeviceMgr.h>
#include <filedevice/seadPath.h>
#include <resource/seadArchiveRes.h>
#include <heap/seadHeapMgr.h>
#include <devenv/seadDebugFontMgrNvn.h>
#include <gfx/seadTextWriter.h>
#include <gfx/seadViewport.h>

#include <al/Library/File/FileLoader.h>
#include <al/Library/File/FileUtil.h>

#include <game/StageScene/StageScene.h>
#include <game/System/GameSystem.h>
#include <game/System/Application.h>
#include <game/HakoniwaSequence/HakoniwaSequence.h>
#include <game/GameData/GameDataFunction.h>

#include "rs/util.hpp"

#include "agl/utl.h"

#include "KoopaFreerunRecorder.hpp"

static const char *DBG_FONT_PATH = "DebugData/Font/nvn_font_jis1.ntx";
static const char *DBG_SHADER_PATH = "DebugData/Font/nvn_font_shader_jis1.bin";
static const char *DBG_TBL_PATH = "DebugData/Font/nvn_font_jis1_tbl.bin";

#define IMGUI_ENABLED true

sead::TextWriter *gTextWriter;
KoopaFreerunRecorder recorder;

void drawDebugWindow() {
    HakoniwaSequence *gameSeq = (HakoniwaSequence *) GameSystemFunction::getGameSystem()->mCurSequence;

    bool _;
    const ImVec4 c(0.95f, 0.95f, 0.95f, 1.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, c);
    ImGui::Begin("KoopaFreerunRecorder", &_,
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::SetWindowSize(ImVec2(210, 80), ImGuiCond_FirstUseEver);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.1f, 0.1f, 0.1f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Button, c);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f*c.x, 0.9f*c.y, 0.9f*c.z, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f*c.x, 0.75f*c.y, 0.75f*c.z, 1.f));
    if (recorder.isRecording()) {
        if (ImGui::Button("STOP Recording")) {
            recorder.stopRecording();
        }
    }
    else {
        if (ImGui::Button("START Recording")) {
            recorder.startRecording();
        }
    }
    ImGui::PopStyleColor(4);

    ImGui::End();
}

HOOK_DEFINE_TRAMPOLINE(ControlHook) {
    static void Callback(StageScene *scene) {

        bool isInGame = scene && scene->mIsAlive;
        if (isInGame) {
            PlayerActorBase *playerBase = rs::getPlayerActor(scene);
            recorder.recordFrame(playerBase);
        }

        Orig(scene);
    }
};

HOOK_DEFINE_REPLACE(ReplaceSeadPrint) {
    static void Callback(const char *format, ...) {
        va_list args;
        va_start(args, format);
        Logger::log(format, args);
        va_end(args);
    }
};

HOOK_DEFINE_TRAMPOLINE(CreateFileDeviceMgr) {
    static void Callback(sead::FileDeviceMgr *thisPtr) {

        Orig(thisPtr);

        thisPtr->mMountedSd = nn::fs::MountSdCardForDebug("sd").isSuccess();

        sead::NinSDFileDevice *sdFileDevice = new sead::NinSDFileDevice();

        thisPtr->mount(sdFileDevice);
    }
};

HOOK_DEFINE_TRAMPOLINE(RedirectFileDevice) {
    static sead::FileDevice *
    Callback(sead::FileDeviceMgr *thisPtr, sead::SafeString &path, sead::BufferedSafeString *pathNoDrive) {

        sead::FixedSafeString<32> driveName;
        sead::FileDevice *device;

        // Logger::log("Path: %s\n", path.cstr());

        if (!sead::Path::getDriveName(&driveName, path)) {

            device = thisPtr->findDevice("sd");

            if (!(device && device->isExistFile(path))) {

                device = thisPtr->getDefaultFileDevice();

                if (!device) {
                    Logger::log("drive name not found and default file device is null\n");
                    return nullptr;
                }

            } else {
                Logger::log("Found File on SD! Path: %s\n", path.cstr());
            }

        } else
            device = thisPtr->findDevice(driveName);

        if (!device)
            return nullptr;

        if (pathNoDrive != nullptr)
            sead::Path::getPathExceptDrive(pathNoDrive, path);

        return device;
    }
};

HOOK_DEFINE_TRAMPOLINE(FileLoaderLoadArc) {
    static sead::ArchiveRes *
    Callback(al::FileLoader *thisPtr, sead::SafeString &path, const char *ext, sead::FileDevice *device) {

        // Logger::log("Path: %s\n", path.cstr());

        sead::FileDevice *sdFileDevice = sead::FileDeviceMgr::instance()->findDevice("sd");

        if (sdFileDevice && sdFileDevice->isExistFile(path)) {

            Logger::log("Found File on SD! Path: %s\n", path.cstr());

            device = sdFileDevice;
        }

        return Orig(thisPtr, path, ext, device);
    }
};

sead::FileDevice *tryFindNewDevice(sead::SafeString &path, sead::FileDevice *orig) {
    sead::FileDevice *sdFileDevice = sead::FileDeviceMgr::instance()->findDevice("sd");

    if (sdFileDevice && sdFileDevice->isExistFile(path))
        return sdFileDevice;

    return orig;
}

HOOK_DEFINE_TRAMPOLINE(FileLoaderIsExistFile) {
    static bool Callback(al::FileLoader *thisPtr, sead::SafeString &path, sead::FileDevice *device) {
        return Orig(thisPtr, path, tryFindNewDevice(path, device));
    }
};

HOOK_DEFINE_TRAMPOLINE(FileLoaderIsExistArchive) {
    static bool Callback(al::FileLoader *thisPtr, sead::SafeString &path, sead::FileDevice *device) {
        return Orig(thisPtr, path, tryFindNewDevice(path, device));
    }
};

HOOK_DEFINE_TRAMPOLINE(GameSystemInit) {
    static void Callback(GameSystem *thisPtr) {

        sead::Heap *curHeap = sead::HeapMgr::instance()->getCurrentHeap();

        sead::DebugFontMgrJis1Nvn::createInstance(curHeap);

        if (al::isExistFile(DBG_SHADER_PATH) && al::isExistFile(DBG_FONT_PATH) && al::isExistFile(DBG_TBL_PATH)) {
            sead::DebugFontMgrJis1Nvn::instance()->initialize(curHeap, DBG_SHADER_PATH, DBG_FONT_PATH, DBG_TBL_PATH,
                                                              0x100000);
        }

        sead::TextWriter::setDefaultFont(sead::DebugFontMgrJis1Nvn::instance());

        al::GameDrawInfo *drawInfo = Application::instance()->mDrawInfo;

        agl::DrawContext *context = drawInfo->mDrawContext;
        agl::RenderBuffer *renderBuffer = drawInfo->mFirstRenderBuffer;

        sead::Viewport *viewport = new sead::Viewport(*renderBuffer);

        gTextWriter = new sead::TextWriter(context, viewport);

        gTextWriter->setupGraphics(context);

        gTextWriter->mColor = sead::Color4f(1.f, 1.f, 1.f, 0.8f);

        Orig(thisPtr);

    }
};

HOOK_DEFINE_TRAMPOLINE(DrawDebugMenu) {
    static void Callback(HakoniwaSequence *thisPtr) {

        Orig(thisPtr);

        gTextWriter->beginDraw();

        gTextWriter->setCursorFromTopLeft(sead::Vector2f(10.f, 10.f));
        // gTextWriter->printf("FPS: %d\n", static_cast<int>(round(Application::instance()->mFramework->calcFps())));

        gTextWriter->endDraw();

    }
};

extern "C" void exl_main(void *x0, void *x1) {
    /* Setup hooking enviroment. */
    exl::hook::Initialize();

    nn::os::SetUserExceptionHandler(exception_handler, nullptr, 0, nullptr);
    installExceptionStub();

    Logger::instance().init(LOGGER_IP, 3080);

    runCodePatches();

    GameSystemInit::InstallAtOffset(0x535850);

    // SD File Redirection

    RedirectFileDevice::InstallAtOffset(0x76CFE0);
    FileLoaderLoadArc::InstallAtOffset(0xA5EF64);
    CreateFileDeviceMgr::InstallAtOffset(0x76C8D4);
    FileLoaderIsExistFile::InstallAtSymbol(
            "_ZNK2al10FileLoader11isExistFileERKN4sead14SafeStringBaseIcEEPNS1_10FileDeviceE");
    FileLoaderIsExistArchive::InstallAtSymbol(
            "_ZNK2al10FileLoader14isExistArchiveERKN4sead14SafeStringBaseIcEEPNS1_10FileDeviceE");

    // Sead Debugging Overriding

    ReplaceSeadPrint::InstallAtOffset(0xB59E28);

    // Debug Text Writer Drawing

    DrawDebugMenu::InstallAtOffset(0x50F1D8);

    // General Hooks

    ControlHook::InstallAtSymbol("_ZN10StageScene7controlEv");

    // ImGui Hooks
#if IMGUI_ENABLED
    nvnImGui::InstallHooks();

    nvnImGui::addDrawFunc(drawDebugWindow);
#endif

}

extern "C" NORETURN void exl_exception_entry() {
    /* TODO: exception handling */
    EXL_ABORT(0x420);
}