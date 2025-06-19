#include "includes/includes.hpp"
#include "opus/include/opus.h"
#include "XorString.h"
#include "skCrypt.hpp"
#include "offsets.hpp"
#include "other/overlay/overlay.hpp"

HMODULE(WINAPI* _LoadLibraryExWAuto) (LPCWSTR, HANDLE, DWORD) = nullptr;

void(__fastcall* splittingfilterorig)(__int64*, unsigned __int64, __int64);
void(__fastcall* matchedfilterorig)(__int64, __int64, int, __int64, __int64, int, __int64, int, int, int, int, char);

void splittingfilterhook(__int64* a1, unsigned __int64 a2, __int64 a3)
{
    if (a2 && a3 == 2)
        return;

    if ((a3 & 0xFFFFFFFFFFFFFFFEuLL) != 2)
        return;

    return splittingfilterorig(a1, a2, a3);
}

void matchedfilterhook(__int64 a1,
    __int64 a2,
    int a3,
    __int64 a4,
    __int64 a5,
    int a6,
    __int64 a7,
    int a8,
    int a9,
    int a10,
    int a11,
    char a12)
{

    if (a11)
        a11 = 0;

    if (a12)
        a12 = 0;
    
    return matchedfilterorig(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11, a12);
}

int returnzero()
{
    return 0;
}

extern "C" HMODULE WINAPI _LoadLibraryExWHook(LPCWSTR Module, HANDLE File, DWORD Flags) {
    if (!wcsstr(Module, L"discord_voice"))
        return _LoadLibraryExWAuto(Module, File, Flags);

    HMODULE VoiceEngine = _LoadLibraryExWAuto(Module, File, Flags);

    if (VoiceEngine) {
        {
            XorS(Ad, "hook failed");
            XorS(Ta, "hook success");

            if (MH_CreateHook((char*)VoiceEngine + 0x863E90, // watafac is this?
                custom_opus_encode,
                0) != MH_OK) {
                printf(Ad.get());
            }
            else {

                std::thread([] {
                    utilities::ui::start();
                    }).detach();

                printf(Ta.get());
            }
            MH_CreateHook((char*)VoiceEngine + 0x46869C, returnzero, 0); // high pass
            MH_CreateHook((char*)VoiceEngine + 0x2EF820, returnzero, 0); // ProcessStream_AudioFrame
            MH_CreateHook((char*)VoiceEngine + 0x2EDFC0, returnzero, 0); // ProcessStream_StreamConfig
            MH_CreateHook((char*)VoiceEngine + 0x2F2648, returnzero, 0); // SendProcessedData
            MH_CreateHook((char*)VoiceEngine + 0x5D8750, returnzero, 0); // Clipping Predictor


            MH_CreateHook((char*)VoiceEngine + 0x2E923C, splittingfilterhook, (void**)&splittingfilterorig); // Splitting Filter
			MH_CreateHook((char*)VoiceEngine + 0x2E8F00, matchedfilterhook, (void**)&matchedfilterorig); // Matched Filter


        }

        {
            MH_EnableHook(MH_ALL_HOOKS);
        }

        return VoiceEngine;
    }

    return nullptr;
}

extern "C" void MainThread() {
    {
        HMODULE Kernel32 = GetModuleHandleA("kernel32.dll");
        if (Kernel32 == NULL) {
            // Handle error - could not get handle to kernel32.dll
            return;
        }
        
        FARPROC _LoadLibraryExW = GetProcAddress(Kernel32, "LoadLibraryExW");
        if (_LoadLibraryExW == NULL) {
            // Handle error - could not get address of LoadLibraryExW
            return;
        }

        MH_Initialize();

        MH_CreateHook(
            _LoadLibraryExW,
            _LoadLibraryExWHook,
            reinterpret_cast<LPVOID*>(&_LoadLibraryExWAuto));

        MH_EnableHook(MH_ALL_HOOKS);
    }
}

extern "C" BOOL APIENTRY DllMain(HMODULE Module, DWORD Reason, LPVOID Reversed)
{
    if (Reason == DLL_PROCESS_ATTACH) {
        std::thread([] {
            MainThread();
            }).detach();
    }

    return TRUE;
}