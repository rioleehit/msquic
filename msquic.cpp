#include "msquic.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "msquic", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "msquic", __VA_ARGS__))

extern "C" {
    //
    //#include "quic_platform.h"
    //#ifdef QUIC_CLOG
    //#include "quic_trace.h"
    //#endif

    /*此简单函数返回平台 ABI，此动态本地库为此平台 ABI 进行编译。*/
    const char* msquic::getPlatformABI()
    {
#if defined(__arm__)
#if defined(__ARM_ARCH_7A__)
#if defined(__ARM_NEON__)
#define ABI "armeabi-v7a/NEON"
#else
#define ABI "armeabi-v7a"
#endif
#else
#define ABI "armeabi"
#endif
#elif defined(__i386__)
#define ABI "x86"
#else
#define ABI "unknown"
#endif
        LOGI("This dynamic shared library is compiled with ABI: %s", ABI);
        return "This native library is compiled with ABI: %s" ABI ".";
    }

    void msquic()
    {
    }

    msquic::msquic()
    {
    }

    msquic::~msquic()
    {
    }


    //void
    //MsQuicLibraryLoad(
    //    void
    //);

    //void
    //MsQuicLibraryUnload(
    //    void
    //);

    //    BOOL
    //    __stdcall
    //    DllMain(
    //        _In_ HINSTANCE Instance,
    //        _In_ DWORD Reason,
    //        _In_ LPVOID Reserved
    //    )
    //    {
    //        UNREFERENCED_PARAMETER(Reserved);
    //
    //        switch (Reason)
    //        {
    //
    //            case DLL_PROCESS_ATTACH:
    //#ifndef _MT // Don't disable thread library calls with static CRT!
    //                DisableThreadLibraryCalls(Instance);
    //#else
    //                UNREFERENCED_PARAMETER(Instance);
    //#endif
    //                CxPlatSystemLoad();
    //                MsQuicLibraryLoad();
    //                break;
    //
    //            case DLL_PROCESS_DETACH:
    //                MsQuicLibraryUnload();
    //                CxPlatSystemUnload();
    //                break;
    //        }
    //
    //        return TRUE;
    //    }

}
