#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx9.h"
#include "imgui/imgui.h"
#include "../libraries/opus/include/opus.h"

#include "d3d9.h"
#include "tchar.h"

static LPDIRECT3D9              g_pD3D = nullptr;
static LPDIRECT3DDEVICE9        g_pd3dDevice = nullptr;
static bool                     g_DeviceLost = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static D3DPRESENT_PARAMETERS    g_d3dpp = {};

// Window handle declaration
extern HWND hwnd;
extern bool show_imgui_window;

// Reverb effect class forward declaration
class FreeverbReverb;
extern FreeverbReverb reverbProcessor;

bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void ResetDevice();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Configuration functions
void CreateConfiguration(const char* file_path);
void SaveConfiguration(const char* file_path);
void LoadConfiguration(const char* file_path);
void ResetConfiguration();
const char* GetKeyName(int key);
void ChangeHotkey(int newKey);

// Audio processing functions
void ApplyAudioEffects(float* audioBuffer, int bufferSize, int channels);
extern "C" opus_int32 custom_opus_encode(OpusEncoder *st, const opus_int16 *pcm, int frame_size,
                                   unsigned char *data, opus_int32 max_data_bytes);

namespace utilities::ui {
	void start();
}

void AnimatedSlider(const char* label, float* value, float min, float max, const char* tooltip);