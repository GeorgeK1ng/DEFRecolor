#include "DEFRecolor.h"

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl_core.h>
#include <strsafe.h>
#include <thumbcache.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <io.h>
#include <iostream>
#include <list>
#include <locale>
#include <string>
#include <vector>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Uxtheme.lib")
#pragma comment(                                                                                                       \
    linker,                                                                                                            \
    "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma warning(push)
#pragma warning(disable : 26495)
#include "json.hpp"
#pragma warning(pop)

using json = nlohmann::json;

constexpr size_t MaxLoadString = 100;

HINSTANCE hInst;
WCHAR szTitle[MaxLoadString];
WCHAR szWindowClass[MaxLoadString];
HWND hStatusBar;
bool defLoaded = false; // Global flag to track DEF load state
HBITMAP previewBitmap = nullptr;
HBITMAP originalPreviewBitmap = nullptr;
HBITMAP originalAnimationBitmap = nullptr;
HBITMAP recoloredAnimationBitmap = nullptr;
HMODULE previewProviderModule = nullptr;
std::wstring previewMessage;
HWND frameCombo = nullptr;
HWND groupCombo = nullptr;
HWND scopeCombo = nullptr;
HWND hueSlider = nullptr;
HWND saturationSlider = nullptr;
HWND lightnessSlider = nullptr;
HWND preserveShadesCheck = nullptr;
HWND selectionLabel = nullptr;
HWND similaritySlider = nullptr;
HWND magicStrengthSlider = nullptr;
HWND targetSwatch = nullptr;
HWND animationToggle = nullptr;
HWND animationSpeedSlider = nullptr;
HWND previewZoomSlider = nullptr;
HWND previewHorizontalScroll = nullptr;
HWND previewVerticalScroll = nullptr;
HWND editorTabs = nullptr;
HWND contrastSlider = nullptr;
HWND gammaSlider = nullptr;
HWND exposureSlider = nullptr;
HWND vibranceSlider = nullptr;
HWND temperatureSlider = nullptr;
HWND tintSlider = nullptr;
HWND blendModeCombo = nullptr;
HWND blendStrengthSlider = nullptr;
HWND advancedModeCombo = nullptr;
HWND advancedAmountSlider = nullptr;
HWND advancedLevelsSlider = nullptr;
HWND categoryButtons[6]{};
HFONT uiFont = nullptr;
HFONT uiBoldFont = nullptr;
HFONT uiTitleFont = nullptr;
HBRUSH uiBackgroundBrush = nullptr;
HBRUSH uiCardBrush = nullptr;
constexpr COLORREF UiBackground = RGB(248, 250, 252);
constexpr COLORREF UiCard = RGB(255, 255, 255);
constexpr COLORREF UiText = RGB(30, 41, 59);
constexpr COLORREF UiMuted = RGB(100, 116, 139);
constexpr COLORREF UiBorder = RGB(203, 213, 225);
constexpr COLORREF UiAccent = RGB(37, 99, 235);
constexpr COLORREF UiAccentHover = RGB(29, 78, 216);
int previewZoomPercent = 100;
int previewScrollX = 0;
int previewScrollY = 0;
int lastSelectedColor = -1;
COLORREF magicTargetColor = RGB(64, 160, 255);
std::vector<COLORREF> previewPalette(256);
uint32_t animationFrame = 0;
bool animationPlaying = true;
bool magicPreviewActive = false;
bool protectSpecialColors = true;
bool eyedropperEnabled = false;
bool correctionPreviewActive = false;
std::vector<COLORREF> correctionBase(256);
COLORREF rampStartColor = RGB(20, 35, 70);
COLORREF rampMiddleColor = RGB(90, 130, 180);
COLORREF rampEndColor = RGB(235, 245, 255);
std::vector<COLORREF> rampPreviewBase(256);
int rampPreviewMode = 0; // 0 = none, 1 = ramp, 2 = tone
bool advancedPreviewActive = false;
std::vector<COLORREF> advancedPreviewBase(256);
constexpr UINT_PTR AnimationTimerId = 1;
json translations;
std::wstring currentLanguage = L"english";
struct AvailableLanguage
{
    std::wstring id;
    std::wstring displayName;
};
std::vector<AvailableLanguage> availableLanguages;
HWND tooltipWindow = nullptr;
std::list<std::wstring> tooltipTexts;
uint32_t currentFrame = 0;
uint32_t frameCount = 0;
bool selectedPaletteColors[256] = {};
std::vector<COLORREF> adjustmentBase(256);
std::vector<std::vector<COLORREF>> undoHistory;
std::vector<std::vector<COLORREF>> redoHistory;
std::vector<COLORREF> hslHistoryBase;
bool hslHistoryActive = false;
constexpr size_t MaxHistoryEntries = 100;

struct DefEditorFrameInfo
{
    uint32_t group, index, format, width, height;
    wchar_t name[14];
};
std::vector<DefEditorFrameInfo> editorFrames;
using GetFrameCountFn = HRESULT(__stdcall*)(const uint8_t*, size_t, uint32_t*);
using GetFrameInfoFn = HRESULT(__stdcall*)(const uint8_t*, size_t, uint32_t, DefEditorFrameInfo*);
using RenderFrameFn = HRESULT(__stdcall*)(const uint8_t*, size_t, uint32_t, const COLORREF*, UINT, HBITMAP*);
GetFrameCountFn getFrameCount = nullptr;
GetFrameInfoFn getFrameInfo = nullptr;
RenderFrameFn renderFrame = nullptr;
COLORREF customColors[16] = {RGB(255, 255, 255),
                             RGB(192, 192, 192),
                             RGB(128, 128, 128),
                             RGB(64, 64, 64),
                             RGB(255, 0, 0),
                             RGB(0, 255, 0),
                             RGB(0, 0, 255),
                             RGB(255, 255, 0),
                             RGB(0, 255, 255),
                             RGB(255, 0, 255),
                             RGB(128, 0, 0),
                             RGB(0, 128, 0),
                             RGB(0, 0, 128),
                             RGB(128, 128, 0),
                             RGB(0, 128, 128),
                             RGB(128, 0, 128)};

// Palettes and DEF data
std::vector<COLORREF> inputPalette(256);
std::vector<COLORREF> outputPalette(256, RGB(0, 0, 0));
std::vector<unsigned char> buffer(256, 0);
std::wstring defFilePath;
std::wstring startupDefFilePath;
std::vector<std::wstring> recentFiles;
constexpr size_t MaxRecentFiles = 10;

// Forward declarations
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void LoadDEFFile(HWND hWnd);
void SaveDEFFile(HWND hWnd);
std::wstring GetExeDirectory();
bool OpenDEFPath(HWND hWnd, const std::filesystem::path& path);

FILE* originalStdout = nullptr;
FILE* originalStderr = nullptr;
FILE* originalStdin = nullptr;

void SetupConsole()
{
    // Attach to the parent console
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
    {
        // If attaching fails, allocate a new console for the application
        AllocConsole();
    }

    // Redirect stdout
    freopen_s(&originalStdout, "CONOUT$", "w", stdout);
    freopen_s(&originalStderr, "CONOUT$", "w", stderr);
    freopen_s(&originalStdin, "CONIN$", "r", stdin);

    // Disable synchronization for better performance
    std::ios::sync_with_stdio(false);

    // Set wide character mode for std::wcout and std::wcin
    std::wcout.imbue(std::locale("en_US.UTF-8"));
    std::wcin.imbue(std::locale("en_US.UTF-8"));
}

void CleanupConsole()
{
    // Flush and close the streams
    if (originalStdout)
    {
        fflush(stdout);
        fclose(originalStdout);
        originalStdout = nullptr;
    }

    if (originalStderr)
    {
        fflush(stderr);
        fclose(originalStderr);
        originalStderr = nullptr;
    }

    if (originalStdin)
    {
        fclose(originalStdin);
        originalStdin = nullptr;
    }

    // Restore synchronization to default
    std::ios::sync_with_stdio(true);

    // Detach from the console to release CMD
    FreeConsole();
}

void UpdateStatusBar(HWND hWnd, const wchar_t* message)
{
    UNREFERENCED_PARAMETER(hWnd);
    SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)message);
}

namespace
{
constexpr size_t PaletteOffset = 16;
constexpr size_t PaletteSize = 256 * 3;
constexpr CLSID CLSID_DefThumbnailProvider = {
    0x9b4f3e1c, 0x5d90, 0x4d62, {0x8f, 0xa2, 0x57, 0xd7, 0xb9, 0xa1, 0x3c, 0x84}};

using TaskDialogIndirectFn = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
using InitCommonControlsExFn = BOOL(WINAPI*)(const INITCOMMONCONTROLSEX*);
using SetWindowSubclassFn = BOOL(WINAPI*)(HWND, SUBCLASSPROC, UINT_PTR, DWORD_PTR);
using DefSubclassProcFn = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
HMODULE commonControlsModule = nullptr;
TaskDialogIndirectFn taskDialogIndirectFn = nullptr;
InitCommonControlsExFn initCommonControlsExFn = nullptr;
SetWindowSubclassFn setWindowSubclassFn = nullptr;
DefSubclassProcFn defSubclassProcFn = nullptr;

bool LoadCommonControlsApi()
{
    if (!commonControlsModule)
        commonControlsModule = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!commonControlsModule)
    {
        wchar_t systemDirectory[MAX_PATH]{};
        const UINT length = GetSystemDirectoryW(systemDirectory, ARRAYSIZE(systemDirectory));
        if (length && length < ARRAYSIZE(systemDirectory) - 16)
        {
            std::wstring systemComCtl(systemDirectory, length);
            systemComCtl += L"\\comctl32.dll";
            commonControlsModule = LoadLibraryW(systemComCtl.c_str());
        }
    }
    if (!commonControlsModule)
        return false;

    const auto resolve = [](HMODULE module, const char* name, WORD ordinal) -> FARPROC
    {
        FARPROC procedure = GetProcAddress(module, name);
        return procedure ? procedure : GetProcAddress(module, MAKEINTRESOURCEA(ordinal));
    };
    if (!taskDialogIndirectFn)
        taskDialogIndirectFn =
            reinterpret_cast<TaskDialogIndirectFn>(resolve(commonControlsModule, "TaskDialogIndirect", 345));
    if (!initCommonControlsExFn)
        initCommonControlsExFn =
            reinterpret_cast<InitCommonControlsExFn>(GetProcAddress(commonControlsModule, "InitCommonControlsEx"));
    if (!setWindowSubclassFn)
        setWindowSubclassFn =
            reinterpret_cast<SetWindowSubclassFn>(resolve(commonControlsModule, "SetWindowSubclass", 410));
    if (!defSubclassProcFn)
        defSubclassProcFn = reinterpret_cast<DefSubclassProcFn>(resolve(commonControlsModule, "DefSubclassProc", 413));
    if (initCommonControlsExFn)
    {
        INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_WIN95_CLASSES | ICC_BAR_CLASSES | ICC_TAB_CLASSES};
        initCommonControlsExFn(&controls);
    }
    return setWindowSubclassFn && defSubclassProcFn;
}

std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty())
        return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

IStream* CreateMemoryStream(const std::vector<unsigned char>& data)
{
    if (data.empty())
        return nullptr;
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, data.size());
    if (!memory)
        return nullptr;
    void* destination = GlobalLock(memory);
    if (!destination)
    {
        GlobalFree(memory);
        return nullptr;
    }
    std::memcpy(destination, data.data(), data.size());
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream)))
    {
        GlobalFree(memory);
        return nullptr;
    }
    return stream;
}

std::wstring Tr(const char* key)
{
    if (translations.contains(key) && translations[key].is_string())
        return Utf8ToWide(translations[key].get<std::string>());
    return Utf8ToWide(key);
}

bool LoadLanguage(const std::wstring& language)
{
    try
    {
        const auto directory = std::filesystem::path(GetExeDirectory()) / L"languages";
        json merged;
        std::ifstream english(directory / L"english.json");
        if (!english)
            return false;
        english >> merged;
        if (language != L"english")
        {
            std::ifstream localized(directory / (language + L".json"));
            if (!localized)
                return false;
            json overrides;
            localized >> overrides;
            merged.merge_patch(overrides);
        }
        translations = std::move(merged);
        currentLanguage = language;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void DiscoverLanguages()
{
    availableLanguages.clear();
    const auto directory = std::filesystem::path(GetExeDirectory()) / L"languages";
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error))
        return;

    for (const auto& entry : std::filesystem::directory_iterator(directory, error))
    {
        if (error || !entry.is_regular_file(error))
            continue;
        std::wstring extension = entry.path().extension().wstring();
        std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
        if (extension != L".json")
            continue;

        try
        {
            std::ifstream stream(entry.path());
            json languageJson;
            if (!stream || !(stream >> languageJson) || !languageJson.is_object())
                continue;

            const std::wstring id = entry.path().stem().wstring();
            std::wstring displayName = id;
            if (languageJson.contains("language.name") && languageJson["language.name"].is_string())
                displayName = Utf8ToWide(languageJson["language.name"].get<std::string>());
            if (!id.empty() && !displayName.empty())
                availableLanguages.push_back({id, displayName});
        }
        catch (...)
        { /* Ignore malformed language packs without breaking startup. */
        }
    }

    std::sort(availableLanguages.begin(),
              availableLanguages.end(),
              [](const auto& left, const auto& right)
              {
                  if (left.id == L"english")
                      return right.id != L"english";
                  if (right.id == L"english")
                      return false;
                  return _wcsicmp(left.displayName.c_str(), right.displayName.c_str()) < 0;
              });
    if (availableLanguages.size() > IDM_LANGUAGE_LAST - IDM_LANGUAGE_FIRST + 1)
        availableLanguages.resize(IDM_LANGUAGE_LAST - IDM_LANGUAGE_FIRST + 1);
}

void SetText(HWND hWnd, int id, const char* key)
{
    const std::wstring value = Tr(key);
    SetDlgItemTextW(hWnd, id, value.c_str());
}

void UpdateWindowTitle(HWND hWnd)
{
    std::wstring title = Tr("app.title");
    if (defLoaded && !defFilePath.empty())
        title += L" — " + std::filesystem::path(defFilePath).filename().wstring();
    SetWindowTextW(hWnd, title.c_str());
}

void UpdateValueStatus(HWND hWnd, const char* key, int value)
{
    std::wstring text = Tr(key);
    const auto marker = text.find(L"{value}");
    if (marker != std::wstring::npos)
        text.replace(marker, 7, std::to_wstring(value));
    UpdateStatusBar(hWnd, text.c_str());
}

void AddTooltip(HWND owner, HWND control, const char* key)
{
    if (!tooltipWindow || !control)
        return;
    tooltipTexts.push_back(Tr(key));
    TOOLINFOW info{};
    info.cbSize = sizeof(info);
    info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
    info.hwnd = owner;
    info.uId = reinterpret_cast<UINT_PTR>(control);
    info.lpszText = const_cast<wchar_t*>(tooltipTexts.back().c_str());
    SendMessageW(tooltipWindow, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
}

void RebuildTooltips(HWND hWnd)
{
    if (tooltipWindow)
        DestroyWindow(tooltipWindow);
    tooltipTexts.clear();
    tooltipWindow = CreateWindowExW(WS_EX_TOPMOST,
                                    TOOLTIPS_CLASSW,
                                    nullptr,
                                    WS_POPUP | TTS_ALWAYSTIP,
                                    CW_USEDEFAULT,
                                    CW_USEDEFAULT,
                                    CW_USEDEFAULT,
                                    CW_USEDEFAULT,
                                    hWnd,
                                    nullptr,
                                    hInst,
                                    nullptr);
    AddTooltip(hWnd, similaritySlider, "tooltip.similarity");
    AddTooltip(hWnd, hueSlider, "tooltip.hsl");
    AddTooltip(hWnd, saturationSlider, "tooltip.hsl");
    AddTooltip(hWnd, lightnessSlider, "tooltip.hsl");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_RESET_SELECTED), "tooltip.reset_selected");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_RESET_ALL), "tooltip.reset_all");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_RESET_SLIDERS), "tooltip.reset_sliders");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_TARGET_COLOR), "tooltip.target");
    AddTooltip(hWnd, magicStrengthSlider, "tooltip.strength");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_APPLY_MAGIC), "tooltip.apply");
    AddTooltip(hWnd, animationToggle, "tooltip.animation");
    AddTooltip(hWnd, animationSpeedSlider, "tooltip.animation_speed");
    AddTooltip(hWnd, previewZoomSlider, "tooltip.preview_zoom");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_ZOOM_RESET), "tooltip.zoom_reset");
    AddTooltip(hWnd, groupCombo, "tooltip.group");
    AddTooltip(hWnd, frameCombo, "tooltip.frame");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_PROTECT_SPECIAL), "tooltip.protect_special");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_EYEDROPPER), "tooltip.eyedropper");
    for (int id = IDC_SELECT_FRAME_USED; id <= IDC_SELECT_UNUSED; ++id)
        AddTooltip(hWnd, GetDlgItem(hWnd, id), "tooltip.usage");
    for (HWND slider : {contrastSlider, gammaSlider, exposureSlider, vibranceSlider, temperatureSlider, tintSlider})
        AddTooltip(hWnd, slider, "tooltip.corrections");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_CORRECTION_APPLY), "tooltip.correction_apply");
    for (int id : {IDC_RAMP_START, IDC_RAMP_MIDDLE, IDC_RAMP_END})
        AddTooltip(hWnd, GetDlgItem(hWnd, id), "tooltip.ramp_colors");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_RAMP_USE_MIDDLE), "tooltip.ramp_middle");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_RAMP_PRESERVE), "tooltip.ramp_preserve");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_RAMP_APPLY), "tooltip.ramp_apply");
    AddTooltip(hWnd, blendModeCombo, "tooltip.blend_mode");
    AddTooltip(hWnd, blendStrengthSlider, "tooltip.blend_strength");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_BLEND_APPLY), "tooltip.blend_apply");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_CANCEL_PREVIEW), "tooltip.cancel_preview");
    AddTooltip(hWnd, advancedModeCombo, "tooltip.advanced_mode");
    AddTooltip(hWnd, advancedAmountSlider, "tooltip.advanced_amount");
    AddTooltip(hWnd, advancedLevelsSlider, "tooltip.advanced_levels");
    AddTooltip(hWnd, GetDlgItem(hWnd, IDC_ADV_APPLY), "tooltip.advanced_apply");
}

std::filesystem::path SettingsPath()
{
    wchar_t appData[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, appData)))
        return std::filesystem::path(GetExeDirectory()) / L"settings.json";
    return std::filesystem::path(appData) / L"DEFRecolor" / L"settings.json";
}

bool SaveSettings()
{
    try
    {
        const auto path = SettingsPath();
        std::filesystem::create_directories(path.parent_path());
        json settings;
        settings["max_recent_files"] = MaxRecentFiles;
        settings["recent_files"] = json::array();
        for (const auto& file : recentFiles)
            settings["recent_files"].push_back(std::filesystem::path(file).u8string());
        settings["custom_colors"] = json::array();
        for (COLORREF color : customColors)
            settings["custom_colors"].push_back(static_cast<uint32_t>(color));
        std::ofstream output(path, std::ios::trunc);
        if (!output)
            return false;
        output << settings.dump(2) << '\n';
        return output.good();
    }
    catch (...)
    {
        return false;
    }
}

void LoadSettings()
{
    recentFiles.clear();
    try
    {
        std::ifstream input(SettingsPath());
        json settings = json::object();
        if (input)
            input >> settings;
        if (settings.contains("recent_files") && settings["recent_files"].is_array())
        {
            for (const auto& item : settings["recent_files"])
            {
                if (!item.is_string() || recentFiles.size() >= MaxRecentFiles)
                    continue;
                const auto path = std::filesystem::u8path(item.get<std::string>()).wstring();
                if (!path.empty())
                    recentFiles.push_back(path);
            }
        }
        if (settings.contains("custom_colors") && settings["custom_colors"].is_array() &&
            settings["custom_colors"].size() >= ARRAYSIZE(customColors))
        {
            for (size_t index = 0; index < ARRAYSIZE(customColors); ++index)
                customColors[index] = static_cast<COLORREF>(settings["custom_colors"][index].get<uint32_t>());
        }
    }
    catch (...)
    {
        recentFiles.clear();
    }
}

void AddRecentFile(const std::wstring& file)
{
    const auto normalized = std::filesystem::path(file).lexically_normal().wstring();
    recentFiles.erase(std::remove_if(recentFiles.begin(),
                                     recentFiles.end(),
                                     [&](const std::wstring& existing)
                                     { return _wcsicmp(existing.c_str(), normalized.c_str()) == 0; }),
                      recentFiles.end());
    recentFiles.insert(recentFiles.begin(), normalized);
    if (recentFiles.size() > MaxRecentFiles)
        recentFiles.resize(MaxRecentFiles);
    SaveSettings();
}

std::wstring EscapeMenuText(std::wstring text)
{
    size_t position = 0;
    while ((position = text.find(L'&', position)) != std::wstring::npos)
    {
        text.insert(position, 1, L'&');
        position += 2;
    }
    return text;
}

void BuildApplicationMenu(HWND hWnd)
{
    DiscoverLanguages();
    HMENU menu = CreateMenu(), file = CreatePopupMenu(), edit = CreatePopupMenu(), select = CreatePopupMenu(),
          recent = CreatePopupMenu(), language = CreatePopupMenu(), help = CreatePopupMenu();
    const auto shortcutLabel = [](const std::wstring& label, const wchar_t* shortcut)
    { return label + L"\t" + shortcut; };
    AppendMenuW(file, MF_STRING, IDM_LOAD_DEF, shortcutLabel(Tr("menu.load"), L"Ctrl+O").c_str());
    AppendMenuW(file, MF_STRING, IDM_SAVE_DEF, shortcutLabel(Tr("menu.save"), L"Ctrl+S").c_str());
    if (recentFiles.empty())
        AppendMenuW(recent, MF_STRING | MF_GRAYED, 0, Tr("menu.recent_empty").c_str());
    else
    {
        for (size_t index = 0; index < recentFiles.size() && index < MaxRecentFiles; ++index)
        {
            std::wstring label = L"&" + std::to_wstring(index + 1) + L"  " + EscapeMenuText(recentFiles[index]);
            AppendMenuW(recent, MF_STRING, IDM_RECENT_FIRST + static_cast<UINT>(index), label.c_str());
        }
        AppendMenuW(recent, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(recent, MF_STRING, IDM_CLEAR_RECENT, Tr("menu.recent_clear").c_str());
    }
    AppendMenuW(file, MF_POPUP, reinterpret_cast<UINT_PTR>(recent), Tr("menu.recent").c_str());
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_EXPORT_JSON, shortcutLabel(Tr("menu.export"), L"Ctrl+E").c_str());
    AppendMenuW(file, MF_STRING, IDM_IMPORT_JSON, shortcutLabel(Tr("menu.import"), L"Ctrl+I").c_str());
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, IDM_EXIT, shortcutLabel(Tr("menu.exit"), L"Alt+F4").c_str());
    AppendMenuW(edit,
                MF_STRING | (undoHistory.empty() ? MF_GRAYED : 0),
                IDM_UNDO,
                shortcutLabel(Tr("menu.undo"), L"Ctrl+Z").c_str());
    AppendMenuW(edit,
                MF_STRING | (redoHistory.empty() ? MF_GRAYED : 0),
                IDM_REDO,
                shortcutLabel(Tr("menu.redo"), L"Ctrl+Y").c_str());
    const UINT selectState = MF_STRING | (defLoaded ? MF_ENABLED : MF_GRAYED);
    AppendMenuW(select, selectState, IDM_SELECT_REDS, Tr("select.reds").c_str());
    AppendMenuW(select, selectState, IDM_SELECT_GREENS, Tr("select.greens").c_str());
    AppendMenuW(select, selectState, IDM_SELECT_BLUES, Tr("select.blues").c_str());
    AppendMenuW(select, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(select, selectState, IDM_SELECT_WARM, Tr("select.warm").c_str());
    AppendMenuW(select, selectState, IDM_SELECT_COOL, Tr("select.cool").c_str());
    AppendMenuW(select, selectState, IDM_SELECT_NEUTRALS, Tr("select.neutrals").c_str());
    AppendMenuW(select, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(select, selectState, IDM_SELECT_HIGHLIGHTS, Tr("select.highlights").c_str());
    AppendMenuW(select, selectState, IDM_SELECT_SHADOWS, Tr("select.shadows").c_str());
    for (size_t index = 0; index < availableLanguages.size(); ++index)
    {
        const auto& item = availableLanguages[index];
        AppendMenuW(language,
                    MF_STRING | (currentLanguage == item.id ? MF_CHECKED : 0),
                    IDM_LANGUAGE_FIRST + static_cast<UINT>(index),
                    item.displayName.c_str());
    }
    AppendMenuW(help, MF_STRING, IDM_HELP_GUIDE, shortcutLabel(Tr("menu.guide"), L"F1").c_str());
    AppendMenuW(help, MF_STRING, IDM_ABOUT, Tr("menu.about").c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), Tr("menu.file").c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), Tr("menu.edit").c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(select), Tr("menu.select").c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(language), Tr("menu.language").c_str());
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), Tr("menu.help").c_str());
    HMENU old = GetMenu(hWnd);
    SetMenu(hWnd, menu);
    if (old)
        DestroyMenu(old);
    DrawMenuBar(hWnd);
}

struct InformationWindowState
{
    bool guide = false;
    HWND tabs = nullptr;
    HWND text = nullptr;
    HWND close = nullptr;
    HWND github = nullptr;
    std::vector<std::pair<std::wstring, std::wstring>> pages;
};

std::wstring JoinHelp(std::initializer_list<const char*> keys)
{
    std::wstring result;
    for (const char* key : keys)
    {
        const std::wstring value = Tr(key);
        if (value.empty() || value == Utf8ToWide(key))
            continue;
        if (!result.empty())
            result += L"\r\n\r\n";
        result += value;
    }
    return result;
}

void SelectInformationPage(InformationWindowState* state, int index)
{
    if (!state || !state->text || state->pages.empty())
        return;
    index = (std::max)(0, (std::min)(index, static_cast<int>(state->pages.size() - 1)));
    SetWindowTextW(state->text, state->pages[index].second.c_str());
    SendMessageW(state->text, EM_SETSEL, 0, 0);
    SendMessageW(state->text, EM_SCROLLCARET, 0, 0);
}

LRESULT CALLBACK InformationWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<InformationWindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    switch (message)
    {
    case WM_CREATE:
    {
        state = reinterpret_cast<InformationWindowState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        state->tabs = CreateWindowW(WC_TABCONTROLW,
                                    nullptr,
                                    WS_CHILD | (state->guide ? WS_VISIBLE : 0) | WS_TABSTOP,
                                    0,
                                    0,
                                    0,
                                    0,
                                    window,
                                    nullptr,
                                    hInst,
                                    nullptr);
        state->text = CreateWindowExW(WS_EX_CLIENTEDGE,
                                      L"EDIT",
                                      nullptr,
                                      WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_MULTILINE | ES_READONLY |
                                          ES_AUTOVSCROLL | WS_VSCROLL,
                                      0,
                                      0,
                                      0,
                                      0,
                                      window,
                                      nullptr,
                                      hInst,
                                      nullptr);
        state->close = CreateWindowW(L"BUTTON",
                                     L"OK",
                                     WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                                     0,
                                     0,
                                     0,
                                     0,
                                     window,
                                     reinterpret_cast<HMENU>(IDOK),
                                     hInst,
                                     nullptr);
        if (!state->guide)
            state->github = CreateWindowW(L"BUTTON",
                                          L"GitHub",
                                          WS_CHILD | WS_VISIBLE | WS_TABSTOP,
                                          0,
                                          0,
                                          0,
                                          0,
                                          window,
                                          reinterpret_cast<HMENU>(9002),
                                          hInst,
                                          nullptr);
        for (HWND control : {state->tabs, state->text, state->close, state->github})
            if (control)
                SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(uiFont), TRUE);
        if (state->guide)
        {
            for (const auto& page : state->pages)
            {
                TCITEMW item{};
                item.mask = TCIF_TEXT;
                item.pszText = const_cast<wchar_t*>(page.first.c_str());
                TabCtrl_InsertItem(state->tabs, TabCtrl_GetItemCount(state->tabs), &item);
            }
            TabCtrl_SetCurSel(state->tabs, 0);
        }
        SelectInformationPage(state, 0);
        return 0;
    }
    case WM_SIZE:
        if (state)
        {
            RECT client{};
            GetClientRect(window, &client);
            const int margin = 16, buttonHeight = 34, bottom = client.bottom - margin - buttonHeight;
            if (state->guide)
            {
                MoveWindow(state->tabs, margin, margin, client.right - margin * 2, 32, TRUE);
                MoveWindow(state->text, margin, margin + 40, client.right - margin * 2, bottom - margin - 40, TRUE);
            }
            else
                MoveWindow(state->text, margin, margin, client.right - margin * 2, bottom - margin, TRUE);
            MoveWindow(state->close, client.right - margin - 110, bottom, 110, buttonHeight, TRUE);
            if (state->github)
                MoveWindow(state->github, margin, bottom, 130, buttonHeight, TRUE);
        }
        return 0;
    case WM_NOTIFY:
        if (state && reinterpret_cast<NMHDR*>(lParam)->hwndFrom == state->tabs &&
            reinterpret_cast<NMHDR*>(lParam)->code == TCN_SELCHANGE)
            SelectInformationPage(state, TabCtrl_GetCurSel(state->tabs));
        return 0;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            DestroyWindow(window);
            return 0;
        }
        if (LOWORD(wParam) == 9002)
        {
            ShellExecuteW(
                window, L"open", L"https://github.com/GeorgeK1ng/DEFRecolor", nullptr, nullptr, SW_SHOWNORMAL);
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

void ShowInformationDialog(HWND owner, bool guide)
{
    InformationWindowState state{};
    state.guide = guide;
    if (guide)
    {
        state.pages = {
            {Tr("help.heading"), JoinHelp({"help.body", "help.details", "help.footer"})},
            {Tr("tab.selection"),
             JoinHelp(
                 {"selection.hint", "tooltip.eyedropper", "tooltip.group", "tooltip.frame", "tooltip.similarity"})},
            {Tr("tab.corrections"), JoinHelp({"correction.help", "tooltip.hsl", "tooltip.correction_reset"})},
            {Tr("tab.ramp"),
             JoinHelp({"ramp.help", "tooltip.ramp_middle", "tooltip.blend_strength", "tooltip.blend_apply"})},
            {Tr("tab.magic"), JoinHelp({"magic.help", "tooltip.target", "tooltip.strength", "tooltip.apply"})},
            {Tr("tab.advanced"),
             JoinHelp({"advanced.help",
                       "tooltip.advanced_mode",
                       "tooltip.advanced_amount",
                       "tooltip.advanced_levels",
                       "tooltip.advanced_apply"})},
            {Tr("tab.restore"),
             JoinHelp(
                 {"tooltip.reset_selected", "tooltip.reset_all", "tooltip.reset_sliders", "tooltip.cancel_preview"})},
            {Tr("menu.edit"),
             L"Ctrl+O  —  " + Tr("menu.load") + L"\r\nCtrl+S  —  " + Tr("menu.save") + L"\r\nCtrl+Z  —  " +
                 Tr("menu.undo") + L"\r\nCtrl+Y  —  " + Tr("menu.redo") + L"\r\nF1  —  " + Tr("menu.guide")}};
    }
    else
        state.pages = {{Tr("about.heading"), JoinHelp({"about.body", "about.details"})}};

    static const wchar_t className[] = L"DEFRecolor.InformationWindow";
    static bool registered = false;
    if (!registered)
    {
        WNDCLASSEXW windowClass{sizeof(windowClass)};
        windowClass.lpfnWndProc = InformationWindowProc;
        windowClass.hInstance = hInst;
        windowClass.hIcon = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_DEFRECOLOR));
        windowClass.hIconSm = LoadIconW(hInst, MAKEINTRESOURCEW(IDI_SMALL));
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        windowClass.lpszClassName = className;
        registered = RegisterClassExW(&windowClass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    if (!registered)
        return;

    RECT ownerRect{};
    GetWindowRect(owner, &ownerRect);
    const int width = guide ? 900 : 620, height = guide ? 650 : 440;
    const int x = ownerRect.left + ((ownerRect.right - ownerRect.left) - width) / 2;
    const int y = ownerRect.top + ((ownerRect.bottom - ownerRect.top) - height) / 2;
    HWND window = CreateWindowExW(WS_EX_DLGMODALFRAME,
                                  className,
                                  Tr(guide ? "help.title" : "about.title").c_str(),
                                  WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                                  x,
                                  y,
                                  width,
                                  height,
                                  owner,
                                  nullptr,
                                  hInst,
                                  &state);
    if (!window)
        return;
    EnableWindow(owner, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    MSG message{};
    while (IsWindow(window) && GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageW(window, &message))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    EnableWindow(owner, TRUE);
    SetForegroundWindow(owner);
}

void UpdateSelectionLabel();
void PopulateFrameCombo(uint32_t group, uint32_t preferredOrdinal);

void RelocalizeFrameSelectors()
{
    if (!defLoaded || !groupCombo || editorFrames.empty())
        return;
    const uint32_t selectedOrdinal = (std::min)(currentFrame, static_cast<uint32_t>(editorFrames.size() - 1));
    const uint32_t selectedGroup = editorFrames[selectedOrdinal].group;
    SendMessageW(groupCombo, CB_RESETCONTENT, 0, 0);
    std::vector<uint32_t> groups;
    int selectedItem = 0;
    for (const auto& info : editorFrames)
    {
        if (std::find(groups.begin(), groups.end(), info.group) != groups.end())
            continue;
        groups.push_back(info.group);
        const std::wstring label = std::to_wstring(info.group);
        const int item =
            static_cast<int>(SendMessageW(groupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
        SendMessageW(groupCombo, CB_SETITEMDATA, item, info.group);
        if (info.group == selectedGroup)
            selectedItem = item;
    }
    SendMessageW(groupCombo, CB_SETCURSEL, selectedItem, 0);
    PopulateFrameCombo(selectedGroup, selectedOrdinal);
}

void UpdateEditorTabVisibility(HWND hWnd)
{
    const int active = editorTabs ? TabCtrl_GetCurSel(editorTabs) : 0;
    const int selectionControls[] = {IDC_LABEL_HINT,
                                     IDC_LABEL_HUE,
                                     IDC_HUE_SLIDER,
                                     IDC_LABEL_SATURATION,
                                     IDC_SAT_SLIDER,
                                     IDC_LABEL_LIGHTNESS,
                                     IDC_LIGHT_SLIDER,
                                     IDC_SELECTION_LABEL,
                                     IDC_SELECT_ALL,
                                     IDC_CLEAR_SELECTION,
                                     IDC_PRESERVE_SHADES,
                                     IDC_PROTECT_SPECIAL,
                                     IDC_SELECT_FRAME_USED,
                                     IDC_SELECT_GROUP_USED,
                                     IDC_SELECT_DEF_USED,
                                     IDC_SELECT_GROUP_COMMON,
                                     IDC_SELECT_UNUSED,
                                     IDC_EYEDROPPER};
    const int magicControls[] = {IDC_LABEL_SIMILARITY,
                                 IDC_SIMILARITY_SLIDER,
                                 IDC_TARGET_COLOR,
                                 IDC_TARGET_SWATCH,
                                 IDC_LABEL_STRENGTH,
                                 IDC_MAGIC_STRENGTH,
                                 IDC_APPLY_MAGIC,
                                 IDC_LABEL_MAGIC_HINT};
    const int correctionControls[] = {IDC_LABEL_CONTRAST,
                                      IDC_CONTRAST_SLIDER,
                                      IDC_LABEL_GAMMA,
                                      IDC_GAMMA_SLIDER,
                                      IDC_LABEL_EXPOSURE,
                                      IDC_EXPOSURE_SLIDER,
                                      IDC_LABEL_VIBRANCE,
                                      IDC_VIBRANCE_SLIDER,
                                      IDC_LABEL_TEMPERATURE,
                                      IDC_TEMPERATURE_SLIDER,
                                      IDC_LABEL_TINT,
                                      IDC_TINT_SLIDER,
                                      IDC_CORRECTION_APPLY,
                                      IDC_CORRECTION_HELP};
    const int rampControls[] = {IDC_RAMP_START,
                                IDC_RAMP_MIDDLE,
                                IDC_RAMP_END,
                                IDC_RAMP_START_SWATCH,
                                IDC_RAMP_MIDDLE_SWATCH,
                                IDC_RAMP_END_SWATCH,
                                IDC_RAMP_USE_MIDDLE,
                                IDC_RAMP_PRESERVE,
                                IDC_RAMP_APPLY,
                                IDC_BLEND_MODE,
                                IDC_BLEND_STRENGTH,
                                IDC_BLEND_APPLY,
                                IDC_LABEL_BLEND,
                                IDC_RAMP_HELP};
    const int advancedControls[] = {IDC_ADV_MODE,
                                    IDC_ADV_AMOUNT,
                                    IDC_ADV_LEVELS,
                                    IDC_ADV_APPLY,
                                    IDC_ADV_HELP,
                                    IDC_LABEL_ADV_AMOUNT,
                                    IDC_LABEL_ADV_LEVELS};
    const int resetControls[] = {IDC_RESET_SELECTED, IDC_RESET_ALL, IDC_RESET_SLIDERS, IDC_CANCEL_PREVIEW};
    for (int id : selectionControls)
        ShowWindow(GetDlgItem(hWnd, id), active == 0 ? SW_SHOW : SW_HIDE);
    for (int id : correctionControls)
        ShowWindow(GetDlgItem(hWnd, id), active == 1 ? SW_SHOW : SW_HIDE);
    for (int id : rampControls)
        ShowWindow(GetDlgItem(hWnd, id), active == 2 ? SW_SHOW : SW_HIDE);
    for (int id : magicControls)
        ShowWindow(GetDlgItem(hWnd, id), active == 3 ? SW_SHOW : SW_HIDE);
    for (int id : advancedControls)
        ShowWindow(GetDlgItem(hWnd, id), active == 4 ? SW_SHOW : SW_HIDE);
    for (int id : resetControls)
        ShowWindow(GetDlgItem(hWnd, id), active == 5 ? SW_SHOW : SW_HIDE);
    for (int id : {IDC_GROUP_SELECTION, IDC_GROUP_MAGIC, IDC_GROUP_RESET})
        ShowWindow(GetDlgItem(hWnd, id), SW_HIDE);
}

void RelocalizeEditorTabs(HWND hWnd)
{
    if (!editorTabs)
        return;
    const int active = max(0, TabCtrl_GetCurSel(editorTabs));
    TabCtrl_DeleteAllItems(editorTabs);
    int categoryIndex = 0;
    for (const char* key : {"tab.selection", "tab.corrections", "tab.ramp", "tab.magic", "tab.advanced", "tab.restore"})
    {
        std::wstring label = Tr(key);
        TCITEMW item{};
        item.mask = TCIF_TEXT;
        item.pszText = label.data();
        TabCtrl_InsertItem(editorTabs, TabCtrl_GetItemCount(editorTabs), &item);
        if (categoryIndex < 6 && categoryButtons[categoryIndex])
            SetWindowTextW(categoryButtons[categoryIndex], label.c_str());
        ++categoryIndex;
    }
    TabCtrl_SetCurSel(editorTabs, min(active, 5));
    UpdateEditorTabVisibility(hWnd);
}

void LayoutEditorControls(HWND hWnd)
{
    RECT client{};
    GetClientRect(hWnd, &client);
    auto move = [&](int id, int x, int y, int w, int h)
    {
        if (HWND control = GetDlgItem(hWnd, id))
            MoveWindow(control, x, y, max(1, w), max(1, h), FALSE);
    };
    move(IDC_GROUP_FRAME, 10, 4, client.right - 20, 66);
    const int pauseX = max(620, client.right - 190);
    const int toolbarLeft = 20, toolbarRight = pauseX - 18, toolbarGap = 14;
    const int toolbarWidth = max(420, toolbarRight - toolbarLeft - toolbarGap * 3);
    const int groupWidth = max(88, toolbarWidth * 16 / 100);
    const int frameWidth = max(175, toolbarWidth * 32 / 100);
    const int animationWidth = max(135, toolbarWidth * 25 / 100);
    const int zoomWidth = max(110, toolbarWidth - groupWidth - frameWidth - animationWidth);
    int toolbarX = toolbarLeft;
    move(IDC_LABEL_GROUP, toolbarX, 7, groupWidth, 22);
    move(IDC_GROUP_COMBO, toolbarX, 31, groupWidth, 300);
    toolbarX += groupWidth + toolbarGap;
    move(IDC_LABEL_FRAME, toolbarX, 7, frameWidth, 22);
    move(IDC_FRAME_COMBO, toolbarX, 31, frameWidth, 300);
    toolbarX += frameWidth + toolbarGap;
    move(IDC_LABEL_ANIM_SPEED, toolbarX, 7, animationWidth, 22);
    move(IDC_ANIMATION_SPEED, toolbarX, 29, animationWidth, 34);
    toolbarX += animationWidth + toolbarGap;
    move(IDC_LABEL_PREVIEW_ZOOM, toolbarX, 7, zoomWidth, 22);
    const int zoomResetWidth = min(70, max(48, zoomWidth / 3));
    move(IDC_PREVIEW_ZOOM, toolbarX, 29, max(50, zoomWidth - zoomResetWidth - 8), 34);
    move(IDC_ZOOM_RESET, toolbarX + zoomWidth - zoomResetWidth, 29, zoomResetWidth, 34);
    move(IDC_ANIMATION_TOGGLE, pauseX, 29, max(120, client.right - pauseX - 20), 34);
    const int sidebarWidth = min(410, max(350, client.right / 3));
    const int sidebarLeft = client.right - sidebarWidth;
    const int panelX = sidebarLeft + 18, panelWidth = sidebarWidth - 36;
    MoveWindow(editorTabs, -1000, -1000, 10, 10, FALSE);
    const int categoryGap = 6, categoryWidth = (sidebarWidth - 28 - categoryGap) / 2;
    for (int i = 0; i < 6; ++i)
        if (categoryButtons[i])
            MoveWindow(categoryButtons[i],
                       sidebarLeft + 14 + (i % 2) * (categoryWidth + categoryGap),
                       82 + (i / 2) * 38,
                       categoryWidth,
                       34,
                       FALSE);

    move(IDC_LABEL_HINT, panelX, 202, panelWidth, 38);
    move(IDC_LABEL_HUE, panelX, 248, panelWidth, 22);
    move(IDC_HUE_SLIDER, panelX, 266, panelWidth, 40);
    move(IDC_LABEL_SATURATION, panelX, 314, panelWidth, 22);
    move(IDC_SAT_SLIDER, panelX, 332, panelWidth, 40);
    move(IDC_LABEL_LIGHTNESS, panelX, 380, panelWidth, 22);
    move(IDC_LIGHT_SLIDER, panelX, 398, panelWidth, 40);
    move(IDC_SELECTION_LABEL, panelX, 448, panelWidth, 24);
    const int half = (panelWidth - 8) / 2;
    move(IDC_SELECT_ALL, panelX, 476, half, 32);
    move(IDC_CLEAR_SELECTION, panelX + half + 8, 476, half, 32);
    move(IDC_PRESERVE_SHADES, panelX, 520, panelWidth, 24);
    move(IDC_PROTECT_SPECIAL, panelX, 550, panelWidth, 24);
    const int actionWidth = (panelWidth - 8) / 2;
    move(IDC_EYEDROPPER, panelX, 586, panelWidth, 34);
    for (int i = 0; i < 5; ++i)
        move(IDC_SELECT_FRAME_USED + i, panelX + (i % 2) * (actionWidth + 8), 628 + (i / 2) * 40, actionWidth, 32);

    move(IDC_LABEL_SIMILARITY, panelX, 212, panelWidth, 22);
    move(IDC_SIMILARITY_SLIDER, panelX, 234, panelWidth, 40);
    move(IDC_TARGET_COLOR, panelX, 288, panelWidth - 48, 34);
    move(IDC_TARGET_SWATCH, panelX + panelWidth - 40, 288, 40, 34);
    move(IDC_LABEL_STRENGTH, panelX, 338, panelWidth, 22);
    move(IDC_MAGIC_STRENGTH, panelX, 360, panelWidth, 40);
    move(IDC_APPLY_MAGIC, panelX, 418, panelWidth, 36);
    move(IDC_LABEL_MAGIC_HINT, panelX, 470, panelWidth, 90);
    const int correctionIds[6][2] = {{IDC_LABEL_CONTRAST, IDC_CONTRAST_SLIDER},
                                     {IDC_LABEL_GAMMA, IDC_GAMMA_SLIDER},
                                     {IDC_LABEL_EXPOSURE, IDC_EXPOSURE_SLIDER},
                                     {IDC_LABEL_VIBRANCE, IDC_VIBRANCE_SLIDER},
                                     {IDC_LABEL_TEMPERATURE, IDC_TEMPERATURE_SLIDER},
                                     {IDC_LABEL_TINT, IDC_TINT_SLIDER}};
    for (int i = 0; i < 6; ++i)
    {
        const int y = 210 + i * 58;
        move(correctionIds[i][0], panelX, y, panelWidth, 22);
        move(correctionIds[i][1], panelX, y + 19, panelWidth, 38);
    }
    move(IDC_CORRECTION_APPLY, panelX, 570, panelWidth, 36);
    move(IDC_CORRECTION_HELP, panelX, 618, panelWidth, 90);
    move(IDC_RAMP_START, panelX, 214, panelWidth - 48, 34);
    move(IDC_RAMP_START_SWATCH, panelX + panelWidth - 40, 214, 40, 34);
    move(IDC_RAMP_MIDDLE, panelX, 258, panelWidth - 48, 34);
    move(IDC_RAMP_MIDDLE_SWATCH, panelX + panelWidth - 40, 258, 40, 34);
    move(IDC_RAMP_END, panelX, 302, panelWidth - 48, 34);
    move(IDC_RAMP_END_SWATCH, panelX + panelWidth - 40, 302, 40, 34);
    move(IDC_RAMP_USE_MIDDLE, panelX, 350, panelWidth, 24);
    move(IDC_RAMP_PRESERVE, panelX, 378, panelWidth, 24);
    move(IDC_RAMP_APPLY, panelX, 414, panelWidth, 36);
    move(IDC_LABEL_BLEND, panelX, 466, panelWidth, 22);
    move(IDC_BLEND_MODE, panelX, 490, panelWidth, 200);
    move(IDC_BLEND_STRENGTH, panelX, 528, panelWidth, 40);
    move(IDC_BLEND_APPLY, panelX, 578, panelWidth, 36);
    move(IDC_RAMP_HELP, panelX, 626, panelWidth, 80);
    move(IDC_ADV_MODE, panelX, 214, panelWidth, 200);
    move(IDC_LABEL_ADV_AMOUNT, panelX, 264, panelWidth, 22);
    move(IDC_ADV_AMOUNT, panelX, 286, panelWidth, 40);
    move(IDC_LABEL_ADV_LEVELS, panelX, 338, panelWidth, 22);
    move(IDC_ADV_LEVELS, panelX, 360, panelWidth, 40);
    move(IDC_ADV_APPLY, panelX, 418, panelWidth, 36);
    move(IDC_ADV_HELP, panelX, 470, panelWidth, 100);
    move(IDC_RESET_SELECTED, panelX, 218, panelWidth, 38);
    move(IDC_RESET_ALL, panelX, 266, panelWidth, 38);
    move(IDC_RESET_SLIDERS, panelX, 314, panelWidth, 38);
    move(IDC_CANCEL_PREVIEW, panelX, 362, panelWidth, 38);
    UpdateEditorTabVisibility(hWnd);
}

void ApplyLanguage(HWND hWnd)
{
    if (!defLoaded)
        previewMessage = Tr("preview.load");
    UpdateWindowTitle(hWnd);
    SetText(hWnd, IDC_GROUP_FRAME, "group.frame");
    SetText(hWnd, IDC_GROUP_SELECTION, "group.selection");
    SetText(hWnd, IDC_GROUP_MAGIC, "group.magic");
    SetText(hWnd, IDC_GROUP_RESET, "group.reset");
    SetText(hWnd, IDC_LABEL_ANIM_SPEED, "animation.speed");
    SetText(hWnd, IDC_LABEL_PREVIEW_ZOOM, "preview.zoom");
    SetText(hWnd, IDC_ZOOM_RESET, "preview.zoom_reset");
    SetText(hWnd, IDC_LABEL_FRAME, "frame");
    SetText(hWnd, IDC_LABEL_HINT, "selection.hint");
    SetText(hWnd, IDC_LABEL_HUE, "hue");
    SetText(hWnd, IDC_LABEL_SATURATION, "saturation");
    SetText(hWnd, IDC_LABEL_LIGHTNESS, "lightness");
    SetText(hWnd, IDC_SELECT_ALL, "select_all");
    SetText(hWnd, IDC_CLEAR_SELECTION, "clear");
    SetText(hWnd, IDC_RESET_SELECTED, "reset_selected");
    SetText(hWnd, IDC_RESET_ALL, "reset_all");
    SetText(hWnd, IDC_RESET_SLIDERS, "reset_sliders");
    SetText(hWnd, IDC_PRESERVE_SHADES, "preserve");
    SetText(hWnd, IDC_LABEL_MAGIC, "magic.title");
    SetText(hWnd, IDC_LABEL_SIMILARITY, "magic.similarity");
    SetText(hWnd, IDC_TARGET_COLOR, "magic.target");
    SetText(hWnd, IDC_LABEL_STRENGTH, "magic.strength");
    SetText(hWnd, IDC_APPLY_MAGIC, "magic.apply");
    SetText(hWnd, IDC_LABEL_MAGIC_HINT, "magic.hint");
    SetText(hWnd, IDC_LABEL_GROUP, "group");
    SetText(hWnd, IDC_PROTECT_SPECIAL, "protect.special");
    SetText(hWnd, IDC_SELECT_FRAME_USED, "usage.frame");
    SetText(hWnd, IDC_SELECT_GROUP_USED, "usage.group");
    SetText(hWnd, IDC_SELECT_DEF_USED, "usage.def");
    SetText(hWnd, IDC_SELECT_GROUP_COMMON, "usage.common");
    SetText(hWnd, IDC_SELECT_UNUSED, "usage.unused");
    RelocalizeEditorTabs(hWnd);
    SetText(hWnd, IDC_EYEDROPPER, "eyedropper");
    SetText(hWnd, IDC_LABEL_CONTRAST, "correction.contrast");
    SetText(hWnd, IDC_LABEL_GAMMA, "correction.gamma");
    SetText(hWnd, IDC_LABEL_EXPOSURE, "correction.exposure");
    SetText(hWnd, IDC_LABEL_VIBRANCE, "correction.vibrance");
    SetText(hWnd, IDC_LABEL_TEMPERATURE, "correction.temperature");
    SetText(hWnd, IDC_LABEL_TINT, "correction.tint");
    SetText(hWnd, IDC_CORRECTION_APPLY, "correction.apply");
    SetText(hWnd, IDC_RAMP_START, "ramp.start");
    SetText(hWnd, IDC_RAMP_MIDDLE, "ramp.middle");
    SetText(hWnd, IDC_RAMP_END, "ramp.end");
    SetText(hWnd, IDC_RAMP_USE_MIDDLE, "ramp.use_middle");
    SetText(hWnd, IDC_RAMP_PRESERVE, "ramp.preserve");
    SetText(hWnd, IDC_RAMP_APPLY, "ramp.apply");
    SetText(hWnd, IDC_LABEL_BLEND, "blend.mode");
    SetText(hWnd, IDC_BLEND_APPLY, "blend.apply");
    SetText(hWnd, IDC_CORRECTION_HELP, "correction.help");
    SetText(hWnd, IDC_RAMP_HELP, "ramp.help");
    SetText(hWnd, IDC_CANCEL_PREVIEW, "preview.cancel");
    SetText(hWnd, IDC_LABEL_ADV_AMOUNT, "advanced.amount");
    SetText(hWnd, IDC_LABEL_ADV_LEVELS, "advanced.levels");
    SetText(hWnd, IDC_ADV_APPLY, "advanced.apply");
    SetText(hWnd, IDC_ADV_HELP, "advanced.help");
    if (blendModeCombo)
    {
        const int selection = max(0, static_cast<int>(SendMessageW(blendModeCombo, CB_GETCURSEL, 0, 0)));
        SendMessageW(blendModeCombo, CB_RESETCONTENT, 0, 0);
        for (const char* key : {"blend.color", "blend.multiply", "blend.screen", "blend.overlay"})
        {
            const std::wstring item = Tr(key);
            SendMessageW(blendModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        }
        SendMessageW(blendModeCombo, CB_SETCURSEL, min(selection, 3), 0);
    }
    if (advancedModeCombo)
    {
        const int selection = max(0, static_cast<int>(SendMessageW(advancedModeCombo, CB_GETCURSEL, 0, 0)));
        SendMessageW(advancedModeCombo, CB_RESETCONTENT, 0, 0);
        for (const char* key : {"advanced.grayscale",
                                "advanced.sepia",
                                "advanced.invert",
                                "advanced.posterize",
                                "advanced.auto_contrast"})
        {
            const std::wstring item = Tr(key);
            SendMessageW(advancedModeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(item.c_str()));
        }
        SendMessageW(advancedModeCombo, CB_SETCURSEL, min(selection, 4), 0);
        const BOOL posterize = min(selection, 4) == 3;
        EnableWindow(advancedLevelsSlider, posterize);
        EnableWindow(GetDlgItem(hWnd, IDC_LABEL_ADV_LEVELS), posterize);
    }
    SetWindowTextW(animationToggle, Tr(animationPlaying ? "animation.pause" : "animation.play").c_str());
    RelocalizeFrameSelectors();
    UpdateSelectionLabel();
    BuildApplicationMenu(hWnd);
    RebuildTooltips(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);
}

struct EditorLayout
{
    RECT inputPalette;
    RECT outputPalette;
    RECT preview;
    int cellWidth;
    int cellHeight;
};

EditorLayout GetEditorLayout(HWND hWnd)
{
    RECT client{};
    GetClientRect(hWnd, &client);
    RECT status{};
    if (hStatusBar)
        GetWindowRect(hStatusBar, &status);

    constexpr int padding = 10;
    constexpr int toolbarHeight = 112;
    constexpr int gap = 10;
    const int statusHeight = hStatusBar ? status.bottom - status.top : 20;
    const int contentBottom = max(padding, client.bottom - statusHeight - padding);
    const int sidebarWidth = min(410, max(350, client.right / 3));
    const int workspaceRight = max(360, client.right - sidebarWidth - padding);
    const int availableWidth = max(32, workspaceRight - 3 * padding);
    const int paletteWidth = availableWidth / 2;
    const int availableHeight = max(64, contentBottom - padding - toolbarHeight);
    // Palettes need enough room to pick colors, but should not consume all vertical space
    // on a tall window. The remaining height is more valuable for the four DEF previews.
    const int paletteHeight = min(224, max(128, availableHeight * 35 / 100));

    EditorLayout layout{};
    layout.cellWidth = max(1, paletteWidth / 16);
    layout.cellHeight = max(1, paletteHeight / 16);
    const int actualWidth = layout.cellWidth * 16;
    const int actualHeight = layout.cellHeight * 16;
    layout.inputPalette = {
        padding, padding + toolbarHeight, padding + actualWidth, padding + toolbarHeight + actualHeight};
    layout.outputPalette = {2 * padding + paletteWidth,
                            padding + toolbarHeight,
                            2 * padding + paletteWidth + actualWidth,
                            padding + toolbarHeight + actualHeight};
    layout.preview = {padding, padding + toolbarHeight + actualHeight + gap, workspaceRight - padding, contentBottom};
    return layout;
}

void UpdatePreviewScrollbars(HWND hWnd)
{
    if (!previewHorizontalScroll || !previewVerticalScroll)
        return;
    const EditorLayout layout = GetEditorLayout(hWnd);
    constexpr int scrollSize = 17;
    const bool visible = previewZoomPercent > 100;
    const int viewportWidth = max(1, layout.preview.right - layout.preview.left - (visible ? scrollSize : 0));
    const int viewportHeight = max(1, layout.preview.bottom - layout.preview.top - (visible ? scrollSize : 0));
    const int panelWidth = max(1, (viewportWidth - 12) / 2);
    const int imageHeight = max(1, (viewportHeight - 12) / 2 - 46);
    int contentWidth = panelWidth;
    int contentHeight = imageHeight;
    for (HBITMAP image : {originalPreviewBitmap, previewBitmap, originalAnimationBitmap, recoloredAnimationBitmap})
    {
        if (!image)
            continue;
        BITMAP bitmap{};
        GetObjectW(image, sizeof(bitmap), &bitmap);
        if (!bitmap.bmWidth || !bitmap.bmHeight)
            continue;
        const double fit =
            min(static_cast<double>(panelWidth) / bitmap.bmWidth, static_cast<double>(imageHeight) / bitmap.bmHeight);
        contentWidth = max(contentWidth, static_cast<int>(bitmap.bmWidth * fit * previewZoomPercent / 100.0));
        contentHeight = max(contentHeight, static_cast<int>(bitmap.bmHeight * fit * previewZoomPercent / 100.0));
    }

    SCROLLINFO horizontal{sizeof(horizontal), SIF_RANGE | SIF_PAGE | SIF_POS};
    horizontal.nMin = 0;
    horizontal.nMax = contentWidth - 1;
    horizontal.nPage = panelWidth;
    previewScrollX = min(previewScrollX, max(0, contentWidth - panelWidth));
    horizontal.nPos = previewScrollX;
    SetScrollInfo(previewHorizontalScroll, SB_CTL, &horizontal, TRUE);

    SCROLLINFO vertical{sizeof(vertical), SIF_RANGE | SIF_PAGE | SIF_POS};
    vertical.nMin = 0;
    vertical.nMax = contentHeight - 1;
    vertical.nPage = imageHeight;
    previewScrollY = min(previewScrollY, max(0, contentHeight - imageHeight));
    vertical.nPos = previewScrollY;
    SetScrollInfo(previewVerticalScroll, SB_CTL, &vertical, TRUE);
    MoveWindow(previewHorizontalScroll,
               layout.preview.left,
               layout.preview.bottom - scrollSize,
               viewportWidth,
               scrollSize,
               FALSE);
    MoveWindow(previewVerticalScroll,
               layout.preview.right - scrollSize,
               layout.preview.top,
               scrollSize,
               viewportHeight,
               FALSE);
    if ((IsWindowVisible(previewHorizontalScroll) != FALSE) != visible)
    {
        ShowWindow(previewHorizontalScroll, visible ? SW_SHOW : SW_HIDE);
        ShowWindow(previewVerticalScroll, visible ? SW_SHOW : SW_HIDE);
    }
}

void CenterPreviewOnVisibleContent(HWND hWnd)
{
    double focusX = 0.0, focusY = 0.0;
    int focusedImages = 0;
    for (HBITMAP image : {originalPreviewBitmap, previewBitmap, originalAnimationBitmap, recoloredAnimationBitmap})
    {
        if (!image)
            continue;
        DIBSECTION dib{};
        if (GetObjectW(image, sizeof(dib), &dib) != sizeof(dib) || !dib.dsBm.bmBits || dib.dsBm.bmBitsPixel != 32 ||
            dib.dsBm.bmWidth <= 0 || dib.dsBm.bmHeight == 0)
            continue;
        const int width = dib.dsBm.bmWidth;
        const int height = abs(dib.dsBm.bmHeight);
        int left = width, top = height, right = -1, bottom = -1;
        const auto* bits = static_cast<const uint8_t*>(dib.dsBm.bmBits);
        for (int memoryY = 0; memoryY < height; ++memoryY)
        {
            const auto* row = bits + static_cast<size_t>(memoryY) * dib.dsBm.bmWidthBytes;
            const int logicalY = dib.dsBm.bmHeight > 0 ? height - 1 - memoryY : memoryY;
            for (int x = 0; x < width; ++x)
            {
                if (row[x * 4 + 3] <= 8)
                    continue;
                left = min(left, x);
                right = max(right, x);
                top = min(top, logicalY);
                bottom = max(bottom, logicalY);
            }
        }
        if (right < left || bottom < top)
            continue;
        focusX += (left + right + 1.0) / (2.0 * width);
        focusY += (top + bottom + 1.0) / (2.0 * height);
        ++focusedImages;
    }
    if (focusedImages)
    {
        focusX /= focusedImages;
        focusY /= focusedImages;
    }
    else
    {
        focusX = focusY = 0.5;
    }

    SCROLLINFO horizontal{sizeof(horizontal), SIF_RANGE | SIF_PAGE};
    SCROLLINFO vertical{sizeof(vertical), SIF_RANGE | SIF_PAGE};
    GetScrollInfo(previewHorizontalScroll, SB_CTL, &horizontal);
    GetScrollInfo(previewVerticalScroll, SB_CTL, &vertical);
    const int horizontalRange = max(0, horizontal.nMax - static_cast<int>(horizontal.nPage) + 1);
    const int verticalRange = max(0, vertical.nMax - static_cast<int>(vertical.nPage) + 1);
    previewScrollX =
        max(0, min(horizontalRange, static_cast<int>(focusX * (horizontal.nMax + 1) - horizontal.nPage / 2.0)));
    previewScrollY = max(0, min(verticalRange, static_cast<int>(focusY * (vertical.nMax + 1) - vertical.nPage / 2.0)));
    UpdatePreviewScrollbars(hWnd);
}

struct PreviewPanelLayout
{
    RECT viewport{};
    std::array<RECT, 4> panels{};
};

PreviewPanelLayout GetPreviewPanelLayout(HWND hWnd)
{
    PreviewPanelLayout result{};
    result.viewport = GetEditorLayout(hWnd).preview;
    if (previewZoomPercent > 100)
    {
        result.viewport.right -= 17;
        result.viewport.bottom -= 17;
    }

    constexpr int gap = 12;
    const int viewportWidth = max(1, result.viewport.right - result.viewport.left);
    const int viewportHeight = max(1, result.viewport.bottom - result.viewport.top);
    const int columnWidth = max(1, (viewportWidth - gap) / 2);
    const int rowHeight = max(1, (viewportHeight - gap) / 2);
    for (int index = 0; index < static_cast<int>(result.panels.size()); ++index)
    {
        const int column = index % 2;
        const int row = index / 2;
        const int left = result.viewport.left + column * (columnWidth + gap);
        const int top = result.viewport.top + row * (rowHeight + gap);
        result.panels[index] = {left, top, left + columnWidth, top + rowHeight};
    }
    return result;
}

RECT GetPreviewImageArea(RECT panel)
{
    InflateRect(&panel, -12, -12);
    panel.top += 34;
    return panel;
}

int HitTestPreviewPaletteIndex(HWND hWnd, POINT point)
{
    if (!defLoaded || !renderFrame || editorFrames.empty())
        return -1;
    const PreviewPanelLayout layout = GetPreviewPanelLayout(hWnd);
    if (!PtInRect(&layout.viewport, point))
        return -1;

    int panelIndex = -1;
    RECT area{};
    for (int candidate = 0; candidate < static_cast<int>(layout.panels.size()); ++candidate)
    {
        const RECT candidateArea = GetPreviewImageArea(layout.panels[candidate]);
        if (PtInRect(&candidateArea, point))
        {
            panelIndex = candidate;
            area = candidateArea;
            break;
        }
    }
    if (panelIndex < 0)
        return -1;
    const uint32_t ordinal = panelIndex < 2 ? currentFrame : animationFrame;
    std::array<COLORREF, 256> markers{};
    for (int index = 0; index < 256; ++index)
        markers[index] = RGB(index, 255 - index, index ^ 0xA5);
    HBITMAP bitmap = nullptr;
    if (FAILED(renderFrame(buffer.data(), buffer.size(), ordinal, markers.data(), 512, &bitmap)) || !bitmap)
        return -1;
    DIBSECTION dib{};
    if (GetObjectW(bitmap, sizeof(dib), &dib) != sizeof(dib) || !dib.dsBm.bmBits)
    {
        DeleteObject(bitmap);
        return -1;
    }
    const int areaWidth = area.right - area.left, areaHeight = area.bottom - area.top;
    const double fit = min(static_cast<double>(areaWidth) / dib.dsBm.bmWidth,
                           static_cast<double>(areaHeight) / abs(dib.dsBm.bmHeight));
    const double scale = fit * previewZoomPercent / 100.0;
    const int drawnWidth = max(1, static_cast<int>(dib.dsBm.bmWidth * scale));
    const int drawnHeight = max(1, static_cast<int>(abs(dib.dsBm.bmHeight) * scale));
    SCROLLINFO horizontal{sizeof(horizontal), SIF_RANGE | SIF_PAGE};
    SCROLLINFO vertical{sizeof(vertical), SIF_RANGE | SIF_PAGE};
    GetScrollInfo(previewHorizontalScroll, SB_CTL, &horizontal);
    GetScrollInfo(previewVerticalScroll, SB_CTL, &vertical);
    const int maxX = max(0, horizontal.nMax - static_cast<int>(horizontal.nPage) + 1);
    const int maxY = max(0, vertical.nMax - static_cast<int>(vertical.nPage) + 1);
    const int overflowX = max(0, drawnWidth - areaWidth), overflowY = max(0, drawnHeight - areaHeight);
    const int drawX =
        overflowX ? area.left - static_cast<int>(overflowX * (maxX ? static_cast<double>(previewScrollX) / maxX : 0.5))
                  : area.left + (areaWidth - drawnWidth) / 2;
    const int drawY =
        overflowY ? area.top - static_cast<int>(overflowY * (maxY ? static_cast<double>(previewScrollY) / maxY : 0.5))
                  : area.top + (areaHeight - drawnHeight) / 2;
    if (point.x < drawX || point.y < drawY || point.x >= drawX + drawnWidth || point.y >= drawY + drawnHeight)
    {
        DeleteObject(bitmap);
        return -1;
    }
    const int sourceX = min(dib.dsBm.bmWidth - 1, max(0, static_cast<int>((point.x - drawX) / scale)));
    const int logicalY = min(abs(dib.dsBm.bmHeight) - 1, max(0, static_cast<int>((point.y - drawY) / scale)));
    const int sourceY = dib.dsBm.bmHeight > 0 ? abs(dib.dsBm.bmHeight) - 1 - logicalY : logicalY;
    const auto* pixel = static_cast<const uint8_t*>(dib.dsBm.bmBits) +
                        static_cast<size_t>(sourceY) * dib.dsBm.bmWidthBytes + sourceX * 4;
    const int index = pixel[2];
    const bool marker = pixel[1] == 255 - index && pixel[0] == (index ^ 0xA5);
    DeleteObject(bitmap);
    return marker ? index : -1;
}

void ScrollPreview(HWND hWnd, int bar, UINT request, int thumb)
{
    HWND scrollbar = bar == SB_HORZ ? previewHorizontalScroll : previewVerticalScroll;
    if (!scrollbar)
        return;
    SCROLLINFO info{sizeof(info), SIF_ALL};
    GetScrollInfo(scrollbar, SB_CTL, &info);
    int position = info.nPos;
    const int line = 32;
    switch (request)
    {
    case SB_LINEUP:
        position -= line;
        break;
    case SB_LINEDOWN:
        position += line;
        break;
    case SB_PAGEUP:
        position -= static_cast<int>(info.nPage);
        break;
    case SB_PAGEDOWN:
        position += static_cast<int>(info.nPage);
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        position = thumb;
        break;
    case SB_TOP:
        position = info.nMin;
        break;
    case SB_BOTTOM:
        position = info.nMax;
        break;
    default:
        return;
    }
    position = max(info.nMin, min(position, info.nMax - static_cast<int>(info.nPage) + 1));
    info.fMask = SIF_POS;
    info.nPos = position;
    SetScrollInfo(scrollbar, SB_CTL, &info, TRUE);
    if (bar == SB_HORZ)
        previewScrollX = position;
    else
        previewScrollY = position;
    const EditorLayout layout = GetEditorLayout(hWnd);
    InvalidateRect(hWnd, &layout.preview, TRUE);
}

LRESULT CALLBACK SliderSubclassProc(HWND slider, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_MOUSEWHEEL)
    {
        const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
        if (!notches)
            return 0;
        int step = 1;
        const int id = GetDlgCtrlID(slider);
        if (id == IDC_ANIMATION_SPEED || id == IDC_PREVIEW_ZOOM)
            step = 5;
        const int minimum = static_cast<int>(SendMessageW(slider, TBM_GETRANGEMIN, 0, 0));
        const int maximum = static_cast<int>(SendMessageW(slider, TBM_GETRANGEMAX, 0, 0));
        const int current = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
        const int position = max(minimum, min(maximum, current + notches * step));
        SendMessageW(slider, TBM_SETPOS, TRUE, position);
        SendMessageW(
            GetParent(slider), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, position), reinterpret_cast<LPARAM>(slider));
        return 0;
    }
    return defSubclassProcFn ? defSubclassProcFn(slider, message, wParam, lParam)
                             : DefWindowProcW(slider, message, wParam, lParam);
}

RECT PaletteCell(const RECT& palette, int cellWidth, int cellHeight, int index)
{
    const int column = index % 16;
    const int row = index / 16;
    return {palette.left + column * cellWidth,
            palette.top + row * cellHeight,
            palette.left + (column + 1) * cellWidth,
            palette.top + (row + 1) * cellHeight};
}

COLORREF AdjustHsl(COLORREF color, int hueShift, int saturationShift, int lightnessShift)
{
    double r = GetRValue(color) / 255.0, g = GetGValue(color) / 255.0, b = GetBValue(color) / 255.0;
    const double high = max(r, max(g, b)), low = min(r, min(g, b));
    double h = 0.0, s = 0.0, l = (high + low) / 2.0;
    if (high != low)
    {
        const double delta = high - low;
        s = l > 0.5 ? delta / (2.0 - high - low) : delta / (high + low);
        if (high == r)
            h = (g - b) / delta + (g < b ? 6.0 : 0.0);
        else if (high == g)
            h = (b - r) / delta + 2.0;
        else
            h = (r - g) / delta + 4.0;
        h /= 6.0;
    }
    h = std::fmod(h + hueShift / 360.0 + 1.0, 1.0);
    s = max(0.0, min(1.0, s + saturationShift / 100.0));
    l = max(0.0, min(1.0, l + lightnessShift / 100.0));
    auto hue = [](double p, double q, double t)
    {
        if (t < 0)
            t += 1;
        if (t > 1)
            t -= 1;
        if (t < 1.0 / 6)
            return p + (q - p) * 6 * t;
        if (t < 0.5)
            return q;
        if (t < 2.0 / 3)
            return p + (q - p) * (2.0 / 3 - t) * 6;
        return p;
    };
    if (s == 0)
        r = g = b = l;
    else
    {
        const double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        const double p = 2 * l - q;
        r = hue(p, q, h + 1.0 / 3);
        g = hue(p, q, h);
        b = hue(p, q, h - 1.0 / 3);
    }
    return RGB(static_cast<int>(r * 255 + 0.5), static_cast<int>(g * 255 + 0.5), static_cast<int>(b * 255 + 0.5));
}

int ColorDistance(COLORREF a, COLORREF b)
{
    const int dr = static_cast<int>(GetRValue(a)) - GetRValue(b);
    const int dg = static_cast<int>(GetGValue(a)) - GetGValue(b);
    const int db = static_cast<int>(GetBValue(a)) - GetBValue(b);
    return static_cast<int>(std::sqrt(2.0 * dr * dr + 4.0 * dg * dg + 3.0 * db * db));
}

COLORREF MagicRecolor(COLORREF source, COLORREF target, int strength, bool preserveShading)
{
    double tr = GetRValue(target), tg = GetGValue(target), tb = GetBValue(target);
    if (preserveShading)
    {
        const double sourceLight = 0.2126 * GetRValue(source) + 0.7152 * GetGValue(source) + 0.0722 * GetBValue(source);
        const double targetLight = max(1.0, 0.2126 * tr + 0.7152 * tg + 0.0722 * tb);
        const double scale = sourceLight / targetLight;
        tr = min(255.0, tr * scale);
        tg = min(255.0, tg * scale);
        tb = min(255.0, tb * scale);
    }
    const double amount = strength / 100.0;
    const int r = static_cast<int>(GetRValue(source) * (1 - amount) + tr * amount + 0.5);
    const int g = static_cast<int>(GetGValue(source) * (1 - amount) + tg * amount + 0.5);
    const int b = static_cast<int>(GetBValue(source) * (1 - amount) + tb * amount + 0.5);
    return RGB(r, g, b);
}

COLORREF ApplyColorCorrections(
    COLORREF source, int contrast, int gamma, int exposure, int vibrance, int temperature, int tint)
{
    auto clampChannel = [](double value) { return max(0.0, min(255.0, value)); };
    double r = GetRValue(source) / 255.0, g = GetGValue(source) / 255.0, b = GetBValue(source) / 255.0;
    const double exposureFactor = std::pow(2.0, exposure / 50.0);
    r *= exposureFactor;
    g *= exposureFactor;
    b *= exposureFactor;
    const double contrastFactor = std::pow(2.0, contrast / 50.0);
    r = (r - 0.5) * contrastFactor + 0.5;
    g = (g - 0.5) * contrastFactor + 0.5;
    b = (b - 0.5) * contrastFactor + 0.5;
    const double gammaValue = std::pow(2.0, gamma / 50.0);
    r = std::pow(max(0.0, r), 1.0 / gammaValue);
    g = std::pow(max(0.0, g), 1.0 / gammaValue);
    b = std::pow(max(0.0, b), 1.0 / gammaValue);
    r += temperature / 250.0;
    b -= temperature / 250.0;
    r += tint / 500.0;
    b += tint / 500.0;
    g -= tint / 250.0;
    const double maximum = max(r, max(g, b)), minimum = min(r, min(g, b));
    const double saturation = maximum <= 0 ? 0 : (maximum - minimum) / maximum;
    const double vibranceFactor =
        vibrance >= 0 ? 1.0 + (vibrance / 100.0) * (1.0 - saturation) : 1.0 + vibrance / 100.0;
    const double luminance = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    r = luminance + (r - luminance) * vibranceFactor;
    g = luminance + (g - luminance) * vibranceFactor;
    b = luminance + (b - luminance) * vibranceFactor;
    return RGB(static_cast<int>(clampChannel(r * 255.0) + 0.5),
               static_cast<int>(clampChannel(g * 255.0) + 0.5),
               static_cast<int>(clampChannel(b * 255.0) + 0.5));
}

void ResetCorrectionSliders()
{
    for (HWND slider : {contrastSlider, gammaSlider, exposureSlider, vibranceSlider, temperatureSlider, tintSlider})
        if (slider)
            SendMessageW(slider, TBM_SETPOS, TRUE, 0);
}

COLORREF MixColor(COLORREF a, COLORREF b, double amount)
{
    amount = max(0.0, min(1.0, amount));
    return RGB(static_cast<int>(GetRValue(a) + (GetRValue(b) - GetRValue(a)) * amount + 0.5),
               static_cast<int>(GetGValue(a) + (GetGValue(b) - GetGValue(a)) * amount + 0.5),
               static_cast<int>(GetBValue(a) + (GetBValue(b) - GetBValue(a)) * amount + 0.5));
}

double ColorLuminance(COLORREF color)
{
    return 0.2126 * GetRValue(color) + 0.7152 * GetGValue(color) + 0.0722 * GetBValue(color);
}

COLORREF BlendColor(COLORREF source, COLORREF tone, int mode, int strength)
{
    COLORREF result = tone;
    if (mode == 0)
        result = MagicRecolor(source, tone, 100, true);
    else
    {
        auto channel = [&](int s, int t)
        {
            if (mode == 1)
                return s * t / 255;
            if (mode == 2)
                return 255 - (255 - s) * (255 - t) / 255;
            return s < 128 ? 2 * s * t / 255 : 255 - 2 * (255 - s) * (255 - t) / 255;
        };
        result = RGB(channel(GetRValue(source), GetRValue(tone)),
                     channel(GetGValue(source), GetGValue(tone)),
                     channel(GetBValue(source), GetBValue(tone)));
    }
    return MixColor(source, result, strength / 100.0);
}

bool IsPaletteIndexProtected(size_t index);

bool BuildRampOrTonePreview(HWND hWnd, int mode)
{
    std::vector<size_t> indices;
    for (size_t i = 0; i < outputPalette.size(); ++i)
        if (selectedPaletteColors[i] && !IsPaletteIndexProtected(i))
            indices.push_back(i);
    if (indices.empty())
        return false;
    if (!rampPreviewMode)
        rampPreviewBase = outputPalette;
    else
        outputPalette = rampPreviewBase;
    rampPreviewMode = mode;

    if (mode == 1)
    {
        std::sort(indices.begin(),
                  indices.end(),
                  [&](size_t a, size_t b)
                  { return ColorLuminance(rampPreviewBase[a]) < ColorLuminance(rampPreviewBase[b]); });
        const bool preserve = SendDlgItemMessageW(hWnd, IDC_RAMP_PRESERVE, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const bool middle = SendDlgItemMessageW(hWnd, IDC_RAMP_USE_MIDDLE, BM_GETCHECK, 0, 0) == BST_CHECKED;
        const double low = ColorLuminance(rampPreviewBase[indices.front()]);
        const double high = ColorLuminance(rampPreviewBase[indices.back()]);
        for (size_t rank = 0; rank < indices.size(); ++rank)
        {
            double t = indices.size() == 1 ? 0.5 : static_cast<double>(rank) / (indices.size() - 1);
            if (preserve && high > low)
                t = (ColorLuminance(rampPreviewBase[indices[rank]]) - low) / (high - low);
            outputPalette[indices[rank]] = middle ? (t < 0.5 ? MixColor(rampStartColor, rampMiddleColor, t * 2)
                                                             : MixColor(rampMiddleColor, rampEndColor, (t - 0.5) * 2))
                                                  : MixColor(rampStartColor, rampEndColor, t);
        }
    }
    else
    {
        const int blendMode = max(0, static_cast<int>(SendMessageW(blendModeCombo, CB_GETCURSEL, 0, 0)));
        const int strength = static_cast<int>(SendMessageW(blendStrengthSlider, TBM_GETPOS, 0, 0));
        for (size_t index : indices)
            outputPalette[index] = BlendColor(rampPreviewBase[index], rampStartColor, blendMode, strength);
    }
    return true;
}

void BuildPreviewPalette()
{
    previewPalette = outputPalette;
    if (!magicPreviewActive)
        return;
    const int strength =
        magicStrengthSlider ? static_cast<int>(SendMessageW(magicStrengthSlider, TBM_GETPOS, 0, 0)) : 100;
    const bool preserve = !preserveShadesCheck || SendMessageW(preserveShadesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
    for (size_t i = 0; i < previewPalette.size(); ++i)
        if (selectedPaletteColors[i])
            previewPalette[i] = MagicRecolor(outputPalette[i], magicTargetColor, strength, preserve);
}

void ClearPreview()
{
    if (previewBitmap)
    {
        DeleteObject(previewBitmap);
        previewBitmap = nullptr;
    }
    if (originalPreviewBitmap)
    {
        DeleteObject(originalPreviewBitmap);
        originalPreviewBitmap = nullptr;
    }
    if (originalAnimationBitmap)
    {
        DeleteObject(originalAnimationBitmap);
        originalAnimationBitmap = nullptr;
    }
    if (recoloredAnimationBitmap)
    {
        DeleteObject(recoloredAnimationBitmap);
        recoloredAnimationBitmap = nullptr;
    }
}

void ResetAdjustmentSliders()
{
    hslHistoryBase.clear();
    hslHistoryActive = false;
    if (hueSlider)
        SendMessageW(hueSlider, TBM_SETPOS, TRUE, 0);
    if (saturationSlider)
        SendMessageW(saturationSlider, TBM_SETPOS, TRUE, 0);
    if (lightnessSlider)
        SendMessageW(lightnessSlider, TBM_SETPOS, TRUE, 0);
    adjustmentBase = outputPalette;
}

void UpdatePreview(HWND hWnd);

bool CancelLivePreview(HWND hWnd, bool repaint = true)
{
    const bool active =
        correctionPreviewActive || rampPreviewMode || advancedPreviewActive || magicPreviewActive || hslHistoryActive;
    if (correctionPreviewActive)
        outputPalette = correctionBase;
    else if (rampPreviewMode)
        outputPalette = rampPreviewBase;
    else if (advancedPreviewActive)
        outputPalette = advancedPreviewBase;
    else if (hslHistoryActive && !hslHistoryBase.empty())
        outputPalette = hslHistoryBase;
    correctionPreviewActive = false;
    rampPreviewMode = 0;
    advancedPreviewActive = false;
    magicPreviewActive = false;
    hslHistoryActive = false;
    hslHistoryBase.clear();
    ResetCorrectionSliders();
    ResetAdjustmentSliders();
    if (repaint && active)
        UpdatePreview(hWnd);
    return active;
}

void UpdateHistoryMenu(HWND hWnd)
{
    if (HMENU menu = GetMenu(hWnd))
    {
        EnableMenuItem(menu, IDM_UNDO, MF_BYCOMMAND | (undoHistory.empty() ? MF_GRAYED : MF_ENABLED));
        EnableMenuItem(menu, IDM_REDO, MF_BYCOMMAND | (redoHistory.empty() ? MF_GRAYED : MF_ENABLED));
        DrawMenuBar(hWnd);
    }
}

void CommitPaletteChange(HWND hWnd, const std::vector<COLORREF>& before)
{
    if (before == outputPalette)
        return;
    undoHistory.push_back(before);
    if (undoHistory.size() > MaxHistoryEntries)
        undoHistory.erase(undoHistory.begin());
    redoHistory.clear();
    UpdateHistoryMenu(hWnd);
}

void ClearPaletteHistory(HWND hWnd)
{
    undoHistory.clear();
    redoHistory.clear();
    hslHistoryBase.clear();
    hslHistoryActive = false;
    UpdateHistoryMenu(hWnd);
}

bool UndoPaletteChange(HWND hWnd)
{
    if (CancelLivePreview(hWnd))
        return true;
    if (undoHistory.empty())
        return false;
    redoHistory.push_back(outputPalette);
    outputPalette = std::move(undoHistory.back());
    undoHistory.pop_back();
    ResetAdjustmentSliders();
    ResetCorrectionSliders();
    correctionPreviewActive = false;
    rampPreviewMode = 0;
    rampPreviewBase = outputPalette;
    advancedPreviewActive = false;
    advancedPreviewBase = outputPalette;
    magicPreviewActive = false;
    UpdateHistoryMenu(hWnd);
    return true;
}

bool RedoPaletteChange(HWND hWnd)
{
    if (redoHistory.empty())
        return false;
    undoHistory.push_back(outputPalette);
    outputPalette = std::move(redoHistory.back());
    redoHistory.pop_back();
    ResetAdjustmentSliders();
    ResetCorrectionSliders();
    correctionPreviewActive = false;
    rampPreviewMode = 0;
    rampPreviewBase = outputPalette;
    advancedPreviewActive = false;
    advancedPreviewBase = outputPalette;
    magicPreviewActive = false;
    UpdateHistoryMenu(hWnd);
    return true;
}

bool IsPaletteIndexProtected(size_t index)
{
    return protectSpecialColors && index < 8;
}

COLORREF AdvancedColorEffect(
    COLORREF source, int mode, int amount, int levels, double minimumLuminance, double maximumLuminance)
{
    auto byte = [](double value) { return static_cast<int>(max(0.0, min(255.0, value)) + 0.5); };
    const double r = GetRValue(source), g = GetGValue(source), b = GetBValue(source);
    const double gray = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    COLORREF target = source;
    if (mode == 0)
        target = RGB(byte(gray), byte(gray), byte(gray));
    else if (mode == 1)
        target = RGB(byte(0.393 * r + 0.769 * g + 0.189 * b),
                     byte(0.349 * r + 0.686 * g + 0.168 * b),
                     byte(0.272 * r + 0.534 * g + 0.131 * b));
    else if (mode == 2)
        target = RGB(255 - static_cast<int>(r), 255 - static_cast<int>(g), 255 - static_cast<int>(b));
    else if (mode == 3)
    {
        const int count = max(2, levels), denominator = count - 1;
        auto posterize = [&](double value)
        { return byte(std::round(value / 255.0 * denominator) * 255.0 / denominator); };
        target = RGB(posterize(r), posterize(g), posterize(b));
    }
    else if (mode == 4 && maximumLuminance > minimumLuminance)
    {
        const double stretched = (gray - minimumLuminance) * 255.0 / (maximumLuminance - minimumLuminance);
        const double factor = gray > 0.001 ? stretched / gray : 0.0;
        target = RGB(byte(r * factor), byte(g * factor), byte(b * factor));
    }
    return MixColor(source, target, amount / 100.0);
}

bool BuildAdvancedPreview(HWND hWnd)
{
    std::vector<size_t> indices;
    for (size_t i = 0; i < outputPalette.size(); ++i)
        if (selectedPaletteColors[i] && !IsPaletteIndexProtected(i))
            indices.push_back(i);
    if (indices.empty())
        return false;
    if (!advancedPreviewActive)
        advancedPreviewBase = outputPalette;
    else
        outputPalette = advancedPreviewBase;
    advancedPreviewActive = true;
    const int mode = max(0, static_cast<int>(SendMessageW(advancedModeCombo, CB_GETCURSEL, 0, 0)));
    const int amount = static_cast<int>(SendMessageW(advancedAmountSlider, TBM_GETPOS, 0, 0));
    const int levels = static_cast<int>(SendMessageW(advancedLevelsSlider, TBM_GETPOS, 0, 0));
    double low = 255.0, high = 0.0;
    for (size_t index : indices)
    {
        const double l = ColorLuminance(advancedPreviewBase[index]);
        low = min(low, l);
        high = max(high, l);
    }
    for (size_t index : indices)
        outputPalette[index] = AdvancedColorEffect(advancedPreviewBase[index], mode, amount, levels, low, high);
    return true;
}

bool GetFrameUsageMask(uint32_t ordinal, std::array<bool, 256>& used)
{
    used.fill(false);
    if (!renderFrame || ordinal >= editorFrames.size())
        return false;
    std::array<COLORREF, 256> markerPalette{};
    for (int index = 0; index < 256; ++index)
        markerPalette[index] = RGB(index, 255 - index, index ^ 0xA5);
    HBITMAP bitmap = nullptr;
    const UINT size = max(1u, max(editorFrames[ordinal].width, editorFrames[ordinal].height));
    if (FAILED(renderFrame(buffer.data(), buffer.size(), ordinal, markerPalette.data(), size, &bitmap)) || !bitmap)
        return false;
    DIBSECTION dib{};
    const bool valid = GetObjectW(bitmap, sizeof(dib), &dib) == sizeof(dib) && dib.dsBm.bmBits;
    if (valid)
    {
        const auto* rows = static_cast<const uint8_t*>(dib.dsBm.bmBits);
        const int height = abs(dib.dsBm.bmHeight);
        for (int y = 0; y < height; ++y)
        {
            const auto* pixel = rows + static_cast<size_t>(y) * dib.dsBm.bmWidthBytes;
            for (int x = 0; x < dib.dsBm.bmWidth; ++x, pixel += 4)
            {
                const int index = pixel[2];
                if (pixel[1] == 255 - index && pixel[0] == (index ^ 0xA5))
                    used[index] = true;
            }
        }
    }
    DeleteObject(bitmap);
    return valid;
}

enum class UsageSelection
{
    Frame,
    Group,
    Def,
    CommonGroup,
    Unused
};

void SelectByUsage(HWND hWnd, UsageSelection mode)
{
    if (!defLoaded || editorFrames.empty())
        return;
    std::array<bool, 256> result{};
    const size_t selectedFrame = (std::min)(static_cast<size_t>(currentFrame), editorFrames.size() - 1);
    const uint32_t group = editorFrames[selectedFrame].group;
    if (mode == UsageSelection::CommonGroup)
        result.fill(true);
    bool processed = false;
    for (uint32_t ordinal = 0; ordinal < editorFrames.size(); ++ordinal)
    {
        const bool include = mode == UsageSelection::Frame ? ordinal == currentFrame
                             : (mode == UsageSelection::Group || mode == UsageSelection::CommonGroup)
                                 ? editorFrames[ordinal].group == group
                                 : true;
        if (!include)
            continue;
        std::array<bool, 256> frame{};
        if (!GetFrameUsageMask(ordinal, frame))
            continue;
        if (mode == UsageSelection::CommonGroup && processed)
            for (size_t i = 0; i < result.size(); ++i)
                result[i] = result[i] && frame[i];
        else if (mode == UsageSelection::CommonGroup)
            result = frame;
        else
            for (size_t i = 0; i < result.size(); ++i)
                result[i] = result[i] || frame[i];
        processed = true;
        if (mode == UsageSelection::Frame)
            break;
    }
    if (!processed)
        return;
    if (mode == UsageSelection::Unused)
        for (size_t i = 0; i < result.size(); ++i)
            result[i] = !result[i];
    for (size_t i = 0; i < result.size(); ++i)
        selectedPaletteColors[i] = !IsPaletteIndexProtected(i) && result[i];
    lastSelectedColor = -1;
    ResetAdjustmentSliders();
    UpdateSelectionLabel();
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusBar(hWnd, Tr("status.usage_selected").c_str());
}

void UpdateSelectionLabel()
{
    const size_t selected =
        static_cast<size_t>(std::count(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), true));
    std::wstring text = Tr("selection.count");
    const auto marker = text.find(L"{count}");
    if (marker != std::wstring::npos)
        text.replace(marker, 7, std::to_wstring(selected));
    if (selectionLabel)
        SetWindowTextW(selectionLabel, text.c_str());
}

void SelectColorRange(HWND hWnd, int mode)
{
    if (!defLoaded)
        return;
    for (size_t i = 0; i < 256; ++i)
    {
        const COLORREF color = outputPalette[i];
        const double r = GetRValue(color) / 255.0, g = GetGValue(color) / 255.0, b = GetBValue(color) / 255.0;
        const double high = max(r, max(g, b)), low = min(r, min(g, b)), delta = high - low;
        double hue = 0.0;
        if (delta > 0.0001)
        {
            if (high == r)
                hue = 60.0 * std::fmod((g - b) / delta + 6.0, 6.0);
            else if (high == g)
                hue = 60.0 * ((b - r) / delta + 2.0);
            else
                hue = 60.0 * ((r - g) / delta + 4.0);
        }
        const double saturation = high <= 0.0001 ? 0.0 : delta / high;
        const double lightness = (high + low) / 2.0;
        bool match = false;
        switch (mode)
        {
        case IDM_SELECT_REDS:
            match = saturation >= 0.18 && (hue < 30.0 || hue >= 330.0);
            break;
        case IDM_SELECT_GREENS:
            match = saturation >= 0.18 && hue >= 75.0 && hue < 170.0;
            break;
        case IDM_SELECT_BLUES:
            match = saturation >= 0.18 && hue >= 185.0 && hue < 270.0;
            break;
        case IDM_SELECT_WARM:
            match = saturation >= 0.12 && (hue < 85.0 || hue >= 325.0);
            break;
        case IDM_SELECT_COOL:
            match = saturation >= 0.12 && hue >= 85.0 && hue < 325.0;
            break;
        case IDM_SELECT_NEUTRALS:
            match = saturation < 0.14;
            break;
        case IDM_SELECT_HIGHLIGHTS:
            match = lightness >= 0.68;
            break;
        case IDM_SELECT_SHADOWS:
            match = lightness <= 0.30;
            break;
        }
        selectedPaletteColors[i] = match && !IsPaletteIndexProtected(i);
    }
    lastSelectedColor = -1;
    ResetAdjustmentSliders();
    UpdateSelectionLabel();
    InvalidateRect(hWnd, nullptr, FALSE);
    UpdateStatusBar(hWnd, Tr("status.range_selected").c_str());
}

void ApplyUiFont(HWND hWnd)
{
    if (!uiFont)
        uiFont = CreateFontW(-16,
                             0,
                             0,
                             0,
                             FW_NORMAL,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE,
                             L"Segoe UI");
    if (!uiBoldFont)
        uiBoldFont = CreateFontW(-16,
                                 0,
                                 0,
                                 0,
                                 FW_SEMIBOLD,
                                 FALSE,
                                 FALSE,
                                 FALSE,
                                 DEFAULT_CHARSET,
                                 OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY,
                                 DEFAULT_PITCH | FF_DONTCARE,
                                 L"Segoe UI");
    if (!uiTitleFont)
        uiTitleFont = CreateFontW(-26,
                                  0,
                                  0,
                                  0,
                                  FW_SEMIBOLD,
                                  FALSE,
                                  FALSE,
                                  FALSE,
                                  DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE,
                                  L"Segoe UI");
    EnumChildWindows(
        hWnd,
        [](HWND child, LPARAM font) -> BOOL
        {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
            wchar_t className[32]{};
            GetClassNameW(child, className, ARRAYSIZE(className));
            SetWindowTheme(child,
                           (_wcsicmp(className, WC_TABCONTROLW) == 0 || _wcsicmp(className, TRACKBAR_CLASSW) == 0)
                               ? L""
                               : L"Explorer",
                           nullptr);
            if (_wcsicmp(className, L"Button") == 0)
            {
                const LONG_PTR style = GetWindowLongPtrW(child, GWL_STYLE);
                const LONG_PTR kind = style & BS_TYPEMASK;
                if (kind == BS_PUSHBUTTON || kind == BS_DEFPUSHBUTTON || GetDlgCtrlID(child) == IDC_EYEDROPPER)
                    SetWindowLongPtrW(child, GWL_STYLE, (style & ~BS_TYPEMASK) | BS_OWNERDRAW);
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(uiFont));
    for (int id : {IDC_APPLY_MAGIC, IDC_CORRECTION_APPLY, IDC_RAMP_APPLY, IDC_BLEND_APPLY, IDC_ADV_APPLY})
        if (HWND control = GetDlgItem(hWnd, id))
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(uiBoldFont), TRUE);
    for (int id : {IDC_LABEL_GROUP, IDC_LABEL_FRAME, IDC_LABEL_ANIM_SPEED, IDC_LABEL_PREVIEW_ZOOM})
        if (HWND control = GetDlgItem(hWnd, id))
            SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(uiBoldFont), TRUE);
}

HRESULT CreatePreviewProvider(IThumbnailProvider** provider)
{
    *provider = nullptr;
    if (!previewProviderModule)
    {
        const std::filesystem::path localProvider =
            std::filesystem::path(GetExeDirectory()) / L"DefThumbnailProvider.dll";
        previewProviderModule = LoadLibraryW(localProvider.c_str());
    }
    if (!previewProviderModule)
        return HRESULT_FROM_WIN32(GetLastError());

    using DllGetClassObjectFn = HRESULT(__stdcall*)(REFCLSID, REFIID, void**);
    const auto getClassObject =
        reinterpret_cast<DllGetClassObjectFn>(GetProcAddress(previewProviderModule, "DllGetClassObject"));
    if (!getClassObject)
        return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

    IClassFactory* factory = nullptr;
    HRESULT result = getClassObject(CLSID_DefThumbnailProvider, IID_PPV_ARGS(&factory));
    if (SUCCEEDED(result))
        result = factory->CreateInstance(nullptr, IID_PPV_ARGS(provider));
    if (factory)
        factory->Release();
    return result;
}

bool LoadEditorApi()
{
    if (renderFrame && getFrameCount && getFrameInfo)
        return true;
    if (!previewProviderModule)
    {
        const auto dll = std::filesystem::path(GetExeDirectory()) / L"DefThumbnailProvider.dll";
        previewProviderModule = LoadLibraryW(dll.c_str());
    }
    if (!previewProviderModule)
        return false;
    getFrameCount = reinterpret_cast<GetFrameCountFn>(GetProcAddress(previewProviderModule, "DefEditorGetFrameCount"));
    getFrameInfo = reinterpret_cast<GetFrameInfoFn>(GetProcAddress(previewProviderModule, "DefEditorGetFrameInfo"));
    renderFrame = reinterpret_cast<RenderFrameFn>(GetProcAddress(previewProviderModule, "DefEditorRenderFrame"));
    return getFrameCount && getFrameInfo && renderFrame;
}

void PopulateFrameCombo(uint32_t group, uint32_t preferredOrdinal = UINT32_MAX)
{
    if (!frameCombo)
        return;
    SendMessageW(frameCombo, CB_RESETCONTENT, 0, 0);
    int selectedItem = -1;
    for (uint32_t ordinal = 0; ordinal < editorFrames.size(); ++ordinal)
    {
        const auto& info = editorFrames[ordinal];
        if (info.group != group)
            continue;
        wchar_t label[128]{};
        swprintf_s(label,
                   L"%u  ·  %ls  ·  %u × %u",
                   info.index,
                   info.name[0] ? info.name : L"unnamed",
                   info.width,
                   info.height);
        const int item = static_cast<int>(SendMessageW(frameCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label)));
        SendMessageW(frameCombo, CB_SETITEMDATA, item, ordinal);
        if (ordinal == preferredOrdinal)
            selectedItem = item;
    }
    if (selectedItem < 0 && SendMessageW(frameCombo, CB_GETCOUNT, 0, 0) > 0)
        selectedItem = 0;
    if (selectedItem >= 0)
    {
        SendMessageW(frameCombo, CB_SETCURSEL, selectedItem, 0);
        currentFrame = static_cast<uint32_t>(SendMessageW(frameCombo, CB_GETITEMDATA, selectedItem, 0));
        animationFrame = currentFrame;
    }
}

void RefreshFrameList()
{
    frameCount = 0;
    currentFrame = 0;
    editorFrames.clear();
    if (!frameCombo || !groupCombo)
        return;
    SendMessageW(frameCombo, CB_RESETCONTENT, 0, 0);
    SendMessageW(groupCombo, CB_RESETCONTENT, 0, 0);
    if (!LoadEditorApi())
    {
        previewMessage = Tr("preview.outdated");
        UpdateStatusBar(nullptr, previewMessage.c_str());
        return;
    }
    if (FAILED(getFrameCount(buffer.data(), buffer.size(), &frameCount)))
    {
        previewMessage = Tr("preview.enumeration_error");
        UpdateStatusBar(nullptr, previewMessage.c_str());
        return;
    }
    for (uint32_t ordinal = 0; ordinal < frameCount; ++ordinal)
    {
        DefEditorFrameInfo info{};
        if (FAILED(getFrameInfo(buffer.data(), buffer.size(), ordinal, &info)))
            continue;
        editorFrames.push_back(info);
    }
    frameCount = static_cast<uint32_t>(editorFrames.size());
    std::vector<uint32_t> groups;
    for (const auto& info : editorFrames)
        if (std::find(groups.begin(), groups.end(), info.group) == groups.end())
            groups.push_back(info.group);
    for (const uint32_t group : groups)
    {
        std::wstring label = std::to_wstring(group);
        const int item =
            static_cast<int>(SendMessageW(groupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
        SendMessageW(groupCombo, CB_SETITEMDATA, item, group);
    }
    if (!groups.empty())
    {
        SendMessageW(groupCombo, CB_SETCURSEL, 0, 0);
        PopulateFrameCombo(groups.front());
    }
}

void UpdatePreview(HWND hWnd)
{
    ClearPreview();
    if (!defLoaded || buffer.size() < PaletteOffset + PaletteSize)
    {
        previewMessage = Tr("preview.load");
        InvalidateRect(hWnd, nullptr, TRUE);
        return;
    }

    if (LoadEditorApi() && frameCount)
    {
        BuildPreviewPalette();
        if (animationFrame >= frameCount)
            animationFrame = currentFrame;
        renderFrame(buffer.data(), buffer.size(), currentFrame, inputPalette.data(), 512, &originalPreviewBitmap);
        const HRESULT editorResult =
            renderFrame(buffer.data(), buffer.size(), currentFrame, previewPalette.data(), 512, &previewBitmap);
        if (animationFrame == currentFrame)
        {
            originalAnimationBitmap =
                static_cast<HBITMAP>(CopyImage(originalPreviewBitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
            recoloredAnimationBitmap =
                static_cast<HBITMAP>(CopyImage(previewBitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
        }
        else
        {
            renderFrame(
                buffer.data(), buffer.size(), animationFrame, inputPalette.data(), 384, &originalAnimationBitmap);
            renderFrame(
                buffer.data(), buffer.size(), animationFrame, previewPalette.data(), 384, &recoloredAnimationBitmap);
        }
        if (SUCCEEDED(editorResult) && previewBitmap)
        {
            previewMessage.clear();
            UpdatePreviewScrollbars(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);
            return;
        }
    }

    std::vector<unsigned char> previewData = buffer;
    for (size_t i = 0; i < 256; ++i)
    {
        const size_t base = PaletteOffset + i * 3;
        previewData[base] = static_cast<unsigned char>(GetRValue(outputPalette[i]));
        previewData[base + 1] = static_cast<unsigned char>(GetGValue(outputPalette[i]));
        previewData[base + 2] = static_cast<unsigned char>(GetBValue(outputPalette[i]));
    }

    IStream* stream = CreateMemoryStream(previewData);
    IThumbnailProvider* provider = nullptr;
    IInitializeWithStream* initializer = nullptr;
    HRESULT result = stream ? CreatePreviewProvider(&provider) : E_OUTOFMEMORY;
    if (SUCCEEDED(result))
        result = provider->QueryInterface(IID_PPV_ARGS(&initializer));
    if (SUCCEEDED(result))
        result = initializer->Initialize(stream, STGM_READ);

    WTS_ALPHATYPE alphaType = WTSAT_UNKNOWN;
    if (SUCCEEDED(result))
        result = provider->GetThumbnail(512, &previewBitmap, &alphaType);

    if (initializer)
        initializer->Release();
    if (provider)
        provider->Release();
    if (stream)
        stream->Release();

    if (FAILED(result) || !previewBitmap)
    {
        ClearPreview();
        previewMessage = result == REGDB_E_CLASSNOTREG ? Tr("preview.outdated") : Tr("preview.decode_error");
    }
    else
        previewMessage.clear();

    UpdatePreviewScrollbars(hWnd);
    InvalidateRect(hWnd, nullptr, TRUE);
}

void UpdateAnimationPreview(HWND hWnd)
{
    if (!renderFrame || !frameCount)
        return;
    if (originalAnimationBitmap)
    {
        DeleteObject(originalAnimationBitmap);
        originalAnimationBitmap = nullptr;
    }
    if (recoloredAnimationBitmap)
    {
        DeleteObject(recoloredAnimationBitmap);
        recoloredAnimationBitmap = nullptr;
    }
    BuildPreviewPalette();
    renderFrame(buffer.data(), buffer.size(), animationFrame, inputPalette.data(), 384, &originalAnimationBitmap);
    renderFrame(buffer.data(), buffer.size(), animationFrame, previewPalette.data(), 384, &recoloredAnimationBitmap);
    UpdatePreviewScrollbars(hWnd);
    const EditorLayout layout = GetEditorLayout(hWnd);
    InvalidateRect(hWnd, &layout.preview, TRUE);
}

bool LoadDEF(const std::filesystem::path& path, std::wstring& error)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
    {
        error = L"Unable to open the DEF file.";
        return false;
    }
    const std::streampos end = file.tellg();
    if (end < 0 || static_cast<size_t>(end) < PaletteOffset + PaletteSize)
    {
        error = L"Invalid DEF file: the palette data is incomplete.";
        return false;
    }

    std::vector<unsigned char> loaded(static_cast<size_t>(end));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(loaded.data()), static_cast<std::streamsize>(loaded.size())))
    {
        error = L"Unable to read the complete DEF file.";
        return false;
    }

    for (size_t i = 0; i < 256; ++i)
    {
        const size_t base = PaletteOffset + i * 3;
        inputPalette[i] = RGB(loaded[base], loaded[base + 1], loaded[base + 2]);
        outputPalette[i] = inputPalette[i];
    }
    buffer.swap(loaded);
    defFilePath = path.wstring();
    defLoaded = true;
    return true;
}

bool SaveDEF(const std::filesystem::path& path, std::wstring& error)
{
    if (!defLoaded || buffer.size() < PaletteOffset + PaletteSize)
    {
        error = L"Load a valid DEF file before saving.";
        return false;
    }

    std::vector<unsigned char> saved = buffer;
    for (size_t i = 0; i < 256; ++i)
    {
        const size_t base = PaletteOffset + i * 3;
        saved[base] = static_cast<unsigned char>(GetRValue(outputPalette[i]));
        saved[base + 1] = static_cast<unsigned char>(GetGValue(outputPalette[i]));
        saved[base + 2] = static_cast<unsigned char>(GetBValue(outputPalette[i]));
    }
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file || !file.write(reinterpret_cast<const char*>(saved.data()), static_cast<std::streamsize>(saved.size())))
    {
        error = L"Unable to write the DEF file.";
        return false;
    }
    buffer.swap(saved);
    defFilePath = path.wstring();
    return true;
}

bool ExportPalette(const std::filesystem::path& path, std::wstring& error)
{
    std::ofstream file(path, std::ios::trunc);
    if (!file)
    {
        error = L"Unable to create the JSON file.";
        return false;
    }
    json j;
    j["palette"] = json::array();
    for (const COLORREF color : outputPalette)
        j["palette"].push_back({{"r", GetRValue(color)}, {"g", GetGValue(color)}, {"b", GetBValue(color)}});
    file << j.dump(2) << '\n';
    if (!file)
    {
        error = L"Unable to write the JSON file.";
        return false;
    }
    return true;
}

bool ImportPalette(const std::filesystem::path& path, std::wstring& error)
{
    try
    {
        std::ifstream file(path);
        if (!file)
        {
            error = L"Unable to open the JSON file.";
            return false;
        }
        json j;
        file >> j;
        if (!j.contains("palette") || !j["palette"].is_array() || j["palette"].size() != 256)
            throw std::runtime_error("invalid palette size");

        std::vector<COLORREF> imported(256);
        for (size_t i = 0; i < imported.size(); ++i)
        {
            const auto& color = j["palette"][i];
            if (!color.is_object() || !color.contains("r") || !color.contains("g") || !color.contains("b") ||
                !color["r"].is_number_integer() || !color["g"].is_number_integer() || !color["b"].is_number_integer())
                throw std::runtime_error("invalid color entry");
            const int r = color["r"].get<int>();
            const int g = color["g"].get<int>();
            const int b = color["b"].get<int>();
            if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255)
                throw std::runtime_error("color component out of range");
            imported[i] = RGB(r, g, b);
        }
        if (protectSpecialColors)
            for (size_t i = 0; i < 8; ++i)
                imported[i] = outputPalette[i];
        outputPalette.swap(imported);
        return true;
    }
    catch (const std::exception&)
    {
        error = L"Invalid JSON palette. Expected 256 RGB colors with values from 0 to 255.";
        return false;
    }
}

std::filesystem::path WithExtension(std::filesystem::path path, const wchar_t* extension)
{
    if (!path.has_extension())
        path.replace_extension(extension);
    return path;
}
} // namespace

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // Parse command-line arguments
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    bool commandLineMode = false;
    for (int i = 1; i < argc; ++i)
    {
        if (_wcsicmp(argv[i], L"/help") == 0 || _wcsicmp(argv[i], L"/?") == 0 || _wcsicmp(argv[i], L"/load") == 0 ||
            _wcsicmp(argv[i], L"/save") == 0 || _wcsicmp(argv[i], L"/import") == 0 ||
            _wcsicmp(argv[i], L"/export") == 0)
        {
            commandLineMode = true;
            break;
        }
    }

    if (argc > 1 && !commandLineMode)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::filesystem::path candidate(argv[i]);
            if (_wcsicmp(candidate.extension().c_str(), L".def") == 0)
            {
                startupDefFilePath = candidate.wstring();
                break;
            }
        }
    }

    if (argc > 1 && commandLineMode)
    {
        SetupConsole(); // Attach console for CLI mode

        // Parse CLI arguments
        std::wstring loadFile, saveFile, importFile, exportFile;
        for (int i = 1; i < argc; ++i)
        {
            if (_wcsicmp(argv[i], L"/help") == 0 || _wcsicmp(argv[i], L"/?") == 0)
            {
                std::wcout << LR"(
Usage: DEFRecolor [options]
Options:
  /load <file.def>       Load a DEF file.
  /save <file.def>       Save the current DEF to a file.
  /import <file.json>    Import a palette from a JSON file.
  /export <file.json>    Export the current palette to a JSON file.
  /help or /?            Show this help message.
Examples:
  DEFRecolor /load myfile.def /import palette.json /save newfile.def
                DEFRecolor /load myfile.def /export palette.json
)" << std::endl;

                LocalFree(argv);
                CleanupConsole();
                if (SUCCEEDED(comResult))
                    CoUninitialize();
                return 0;
            }
            else if (_wcsicmp(argv[i], L"/load") == 0 && i + 1 < argc)
            {
                loadFile = argv[++i];
            }
            else if (_wcsicmp(argv[i], L"/save") == 0 && i + 1 < argc)
            {
                saveFile = argv[++i];
            }
            else if (_wcsicmp(argv[i], L"/import") == 0 && i + 1 < argc)
            {
                importFile = argv[++i];
            }
            else if (_wcsicmp(argv[i], L"/export") == 0 && i + 1 < argc)
            {
                exportFile = argv[++i];
            }
        }

        bool success = true;
        std::wstring error;
        if (loadFile.empty() && (!saveFile.empty() || !importFile.empty() || !exportFile.empty()))
        {
            std::wcerr << L"Error: /load is required for palette operations.\n";
            success = false;
        }

        if (!loadFile.empty())
        {
            std::wcout << L"Loading DEF file: " << loadFile << std::endl;
            if (!LoadDEF(loadFile, error))
            {
                std::wcerr << L"Error: " << error << std::endl;
                success = false;
            }
        }

        if (success && !importFile.empty())
        {
            std::wcout << L"Importing palette from JSON: " << importFile << std::endl;
            if (!ImportPalette(importFile, error))
            {
                std::wcerr << L"Error: " << error << std::endl;
                success = false;
            }
        }

        if (success && !exportFile.empty())
        {
            const auto path = WithExtension(exportFile, L".json");
            std::wcout << L"Exporting palette to JSON: " << path.wstring() << std::endl;
            if (!ExportPalette(path, error))
            {
                std::wcerr << L"Error: " << error << std::endl;
                success = false;
            }
        }

        if (success && !saveFile.empty())
        {
            const auto path = WithExtension(saveFile, L".def");
            std::wcout << L"Saving DEF file: " << path.wstring() << std::endl;
            if (!SaveDEF(path, error))
            {
                std::wcerr << L"Error: " << error << std::endl;
                success = false;
            }
        }

        // Cleanup and exit
        LocalFree(argv);
        CleanupConsole();
        if (SUCCEEDED(comResult))
            CoUninitialize();
        return success ? 0 : 1;
    }

    // GUI mode
    LocalFree(argv);
    if (!LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MaxLoadString))
        StringCchCopyW(szTitle, MaxLoadString, L"DEFRecolor");
    if (!LoadStringW(hInstance, IDC_DEFRECOLOR, szWindowClass, MaxLoadString))
        StringCchCopyW(szWindowClass, MaxLoadString, L"DEFRecolor.MainWindow");
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        const DWORD error = GetLastError();
        wchar_t message[256]{};
        StringCchPrintfW(
            message, ARRAYSIZE(message), L"DEFRecolor could not create its main window. Windows error: %lu.", error);
        MessageBoxW(nullptr, message, L"DEFRecolor startup error", MB_OK | MB_ICONERROR);
        if (SUCCEEDED(comResult))
            CoUninitialize();
        return FALSE;
    }

    ACCEL acceleratorEntries[] = {{FCONTROL | FVIRTKEY, 'O', IDM_LOAD_DEF},
                                  {FCONTROL | FVIRTKEY, 'S', IDM_SAVE_DEF},
                                  {FCONTROL | FVIRTKEY, 'E', IDM_EXPORT_JSON},
                                  {FCONTROL | FVIRTKEY, 'I', IDM_IMPORT_JSON},
                                  {FCONTROL | FVIRTKEY, 'Z', IDM_UNDO},
                                  {FCONTROL | FVIRTKEY, 'Y', IDM_REDO},
                                  {FCONTROL | FVIRTKEY, '1', IDC_CATEGORY_SELECTION},
                                  {FCONTROL | FVIRTKEY, '2', IDC_CATEGORY_CORRECTIONS},
                                  {FCONTROL | FVIRTKEY, '3', IDC_CATEGORY_RAMP},
                                  {FCONTROL | FVIRTKEY, '4', IDC_CATEGORY_MAGIC},
                                  {FCONTROL | FVIRTKEY, '5', IDC_CATEGORY_ADVANCED},
                                  {FCONTROL | FVIRTKEY, '6', IDC_CATEGORY_RESTORE},
                                  {FVIRTKEY, VK_F1, IDM_HELP_GUIDE}};
    HACCEL hAccelTable = CreateAcceleratorTableW(acceleratorEntries, ARRAYSIZE(acceleratorEntries));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        const HWND acceleratorTarget = msg.hwnd ? GetAncestor(msg.hwnd, GA_ROOT) : nullptr;
        if (!acceleratorTarget || !TranslateAcceleratorW(acceleratorTarget, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    if (hAccelTable)
        DestroyAcceleratorTable(hAccelTable);
    if (SUCCEEDED(comResult))
        CoUninitialize();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_DEFRECOLOR));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!uiBackgroundBrush)
        uiBackgroundBrush = CreateSolidBrush(UiBackground);
    if (!uiCardBrush)
        uiCardBrush = CreateSolidBrush(UiCard);
    wcex.hbrBackground = uiBackgroundBrush;
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DEFRECOLOR);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    RECT desktop{};
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &desktop, 0);
    const int workWidth = desktop.right - desktop.left, workHeight = desktop.bottom - desktop.top;
    const int width = min(1280, workWidth);
    const int height = min(960, workHeight);
    const int xPos = desktop.left + (workWidth - width) / 2;
    const int yPos = desktop.top + (workHeight - height) / 2;

    HWND hWnd = CreateWindowW(szWindowClass,
                              szTitle,
                              WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                              xPos,
                              yPos,
                              width,
                              height,
                              nullptr,
                              nullptr,
                              hInstance,
                              nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    if (!startupDefFilePath.empty())
        OpenDEFPath(hWnd, startupDefFilePath);

    return TRUE;
}

std::wstring GetExeDirectory()
{
    WCHAR exePath[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, exePath, ARRAYSIZE(exePath));
    if (!length || length >= ARRAYSIZE(exePath))
        return {};
    return std::filesystem::path(std::wstring(exePath, length)).parent_path().wstring();
}

bool OpenDEFPath(HWND hWnd, const std::filesystem::path& path)
{
    std::wstring error;
    if (!LoadDEF(path, error))
    {
        UpdateStatusBar(hWnd, error.c_str());
        return false;
    }
    adjustmentBase = outputPalette;
    std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
    lastSelectedColor = -1;
    magicPreviewActive = false;
    ClearPaletteHistory(hWnd);
    ResetAdjustmentSliders();
    UpdateSelectionLabel();
    UpdateStatusBar(hWnd, Tr("status.loaded").c_str());
    UpdateWindowTitle(hWnd);
    RefreshFrameList();
    UpdatePreview(hWnd);
    AddRecentFile(path.wstring());
    BuildApplicationMenu(hWnd);
    return true;
}

void LoadDEFFile(HWND hWnd)
{
    OPENFILENAME ofn = {};
    WCHAR szFile[260] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"DEF Files\0*.def\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
        OpenDEFPath(hWnd, szFile);
}

void SaveDEFFile(HWND hWnd)
{
    OPENFILENAME ofn = {};
    WCHAR szFile[260] = {};
    std::wstring initialDirectory;

    if (defLoaded && !defFilePath.empty())
    {
        const std::filesystem::path sourcePath(defFilePath);
        const std::wstring suggestedName = sourcePath.stem().wstring() + L"_recolored.def";
        wcsncpy_s(szFile, suggestedName.c_str(), _TRUNCATE);
        initialDirectory = sourcePath.parent_path().wstring();
    }

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"DEF Files\0*.def\0All Files\0*.*\0";
    ofn.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"def";

    if (GetSaveFileName(&ofn))
    {
        std::wstring error;
        if (SaveDEF(WithExtension(szFile, L".def"), error))
        {
            UpdateStatusBar(hWnd, Tr("status.saved").c_str());
            UpdateWindowTitle(hWnd);
        }
        else
            UpdateStatusBar(hWnd, error.c_str());
    }
}

void ExportPaletteToJSON(HWND hWnd)
{
    OPENFILENAME ofn = {};
    WCHAR szFile[260] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = L"json";

    if (GetSaveFileName(&ofn))
    {
        std::wstring error;
        if (ExportPalette(WithExtension(szFile, L".json"), error))
            UpdateStatusBar(hWnd, Tr("status.exported").c_str());
        else
            UpdateStatusBar(hWnd, error.c_str());
    }
}

void ImportPaletteFromJSON(HWND hWnd)
{
    OPENFILENAME ofn = {};
    WCHAR szFile[260] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn))
    {
        std::wstring error;
        const auto before = outputPalette;
        if (ImportPalette(szFile, error))
        {
            CommitPaletteChange(hWnd, before);
            ResetAdjustmentSliders();
            UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.imported").c_str());
        }
        else
            UpdateStatusBar(hWnd, error.c_str());
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        DragAcceptFiles(hWnd, TRUE);
        LoadCommonControlsApi();
        if (!LoadLanguage(L"english"))
            translations = json::object();
        LoadSettings();
        BuildApplicationMenu(hWnd);
        editorTabs = CreateWindowW(WC_TABCONTROLW,
                                   nullptr,
                                   WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_OWNERDRAWFIXED | TCS_FIXEDWIDTH |
                                       TCS_MULTILINE | TCS_BUTTONS | TCS_FLATBUTTONS,
                                   10,
                                   74,
                                   1240,
                                   170,
                                   hWnd,
                                   reinterpret_cast<HMENU>(IDC_EDITOR_TABS),
                                   hInst,
                                   nullptr);
        SendMessageW(editorTabs, TCM_SETITEMSIZE, 0, MAKELPARAM(142, 34));
        const int categoryIds[6] = {IDC_CATEGORY_SELECTION,
                                    IDC_CATEGORY_CORRECTIONS,
                                    IDC_CATEGORY_RAMP,
                                    IDC_CATEGORY_MAGIC,
                                    IDC_CATEGORY_ADVANCED,
                                    IDC_CATEGORY_RESTORE};
        for (int i = 0; i < 6; ++i)
            categoryButtons[i] = CreateWindowW(L"BUTTON",
                                               L"",
                                               WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                               0,
                                               0,
                                               120,
                                               34,
                                               hWnd,
                                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(categoryIds[i])),
                                               hInst,
                                               nullptr);

        // Create a status bar
        hStatusBar = CreateWindowEx(
            0, STATUSCLASSNAME, NULL, WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP, 0, 0, 0, 0, hWnd, (HMENU)0, hInst, NULL);
        SendMessageW(hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(Tr("status.ready").c_str()));
        previewHorizontalScroll =
            CreateWindowW(L"SCROLLBAR", nullptr, WS_CHILD | SBS_HORZ, 0, 0, 0, 0, hWnd, nullptr, hInst, nullptr);
        previewVerticalScroll =
            CreateWindowW(L"SCROLLBAR", nullptr, WS_CHILD | SBS_VERT, 0, 0, 0, 0, hWnd, nullptr, hInst, nullptr);

        CreateWindowW(L"BUTTON",
                      L"",
                      WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      10,
                      4,
                      1240,
                      66,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_GROUP_FRAME),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"",
                      WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      10,
                      74,
                      815,
                      122,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_GROUP_SELECTION),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"",
                      WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      835,
                      74,
                      415,
                      122,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_GROUP_RESET),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"",
                      WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      10,
                      200,
                      1240,
                      84,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_GROUP_MAGIC),
                      hInst,
                      nullptr);

        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      25,
                      30,
                      50,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_GROUP),
                      hInst,
                      nullptr);
        groupCombo = CreateWindowW(WC_COMBOBOXW,
                                   nullptr,
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   78,
                                   24,
                                   135,
                                   300,
                                   hWnd,
                                   reinterpret_cast<HMENU>(IDC_GROUP_COMBO),
                                   hInst,
                                   nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      228,
                      30,
                      50,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_FRAME),
                      hInst,
                      nullptr);
        frameCombo = CreateWindowW(WC_COMBOBOXW,
                                   nullptr,
                                   WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                                   280,
                                   24,
                                   180,
                                   300,
                                   hWnd,
                                   reinterpret_cast<HMENU>(IDC_FRAME_COMBO),
                                   hInst,
                                   nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      25,
                      96,
                      780,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_HINT),
                      hInst,
                      nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      25,
                      127,
                      55,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_HUE),
                      hInst,
                      nullptr);
        hueSlider = CreateWindowW(TRACKBAR_CLASSW,
                                  nullptr,
                                  WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                  80,
                                  116,
                                  175,
                                  38,
                                  hWnd,
                                  reinterpret_cast<HMENU>(IDC_HUE_SLIDER),
                                  hInst,
                                  nullptr);
        SendMessageW(hueSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-180, 180));
        SendMessageW(hueSlider, TBM_SETTICFREQ, 45, 0);
        SendMessageW(hueSlider, TBM_SETPAGESIZE, 0, 45);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      270,
                      127,
                      80,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_SATURATION),
                      hInst,
                      nullptr);
        saturationSlider = CreateWindowW(TRACKBAR_CLASSW,
                                         nullptr,
                                         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                         350,
                                         116,
                                         180,
                                         38,
                                         hWnd,
                                         reinterpret_cast<HMENU>(IDC_SAT_SLIDER),
                                         hInst,
                                         nullptr);
        SendMessageW(saturationSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
        SendMessageW(saturationSlider, TBM_SETTICFREQ, 25, 0);
        SendMessageW(saturationSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      545,
                      127,
                      75,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_LIGHTNESS),
                      hInst,
                      nullptr);
        lightnessSlider = CreateWindowW(TRACKBAR_CLASSW,
                                        nullptr,
                                        WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                        620,
                                        116,
                                        180,
                                        38,
                                        hWnd,
                                        reinterpret_cast<HMENU>(IDC_LIGHT_SLIDER),
                                        hInst,
                                        nullptr);
        SendMessageW(lightnessSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
        SendMessageW(lightnessSlider, TBM_SETTICFREQ, 25, 0);
        SendMessageW(lightnessSlider, TBM_SETPAGESIZE, 0, 25);
        selectionLabel = CreateWindowW(L"STATIC",
                                       L"0 colors selected",
                                       WS_CHILD | WS_VISIBLE,
                                       25,
                                       163,
                                       145,
                                       24,
                                       hWnd,
                                       reinterpret_cast<HMENU>(IDC_SELECTION_LABEL),
                                       hInst,
                                       nullptr);
        CreateWindowW(L"BUTTON",
                      L"Select all",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      175,
                      157,
                      105,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_SELECT_ALL),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Clear",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      285,
                      157,
                      125,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_CLEAR_SELECTION),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Reset selected",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      850,
                      116,
                      125,
                      34,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_RESET_SELECTED),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Reset all",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      980,
                      116,
                      115,
                      34,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_RESET_ALL),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Reset sliders",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      1100,
                      116,
                      135,
                      34,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_RESET_SLIDERS),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Cancel live preview",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      0,
                      0,
                      180,
                      36,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_CANCEL_PREVIEW),
                      hInst,
                      nullptr);
        preserveShadesCheck = CreateWindowW(L"BUTTON",
                                            L"Preserve relative shades",
                                            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                            430,
                                            160,
                                            360,
                                            24,
                                            hWnd,
                                            reinterpret_cast<HMENU>(IDC_PRESERVE_SHADES),
                                            hInst,
                                            nullptr);
        SendMessageW(preserveShadesCheck, BM_SETCHECK, BST_CHECKED, 0);
        CreateWindowW(L"BUTTON",
                      L"Protect special colors",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      550,
                      160,
                      250,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_PROTECT_SPECIAL),
                      hInst,
                      nullptr);
        SendDlgItemMessageW(hWnd, IDC_PROTECT_SPECIAL, BM_SETCHECK, BST_CHECKED, 0);
        CreateWindowW(L"BUTTON",
                      L"Current frame",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      25,
                      198,
                      120,
                      28,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_SELECT_FRAME_USED),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Current group",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      150,
                      198,
                      120,
                      28,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_SELECT_GROUP_USED),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Entire DEF",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      275,
                      198,
                      120,
                      28,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_SELECT_DEF_USED),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Common in group",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      400,
                      198,
                      140,
                      28,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_SELECT_GROUP_COMMON),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Unused",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      545,
                      198,
                      120,
                      28,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_SELECT_UNUSED),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Eyedropper",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX | BS_PUSHLIKE,
                      670,
                      198,
                      120,
                      28,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_EYEDROPPER),
                      hInst,
                      nullptr);
        const int correctionLabelIds[] = {IDC_LABEL_CONTRAST,
                                          IDC_LABEL_GAMMA,
                                          IDC_LABEL_EXPOSURE,
                                          IDC_LABEL_VIBRANCE,
                                          IDC_LABEL_TEMPERATURE,
                                          IDC_LABEL_TINT};
        const int correctionSliderIds[] = {IDC_CONTRAST_SLIDER,
                                           IDC_GAMMA_SLIDER,
                                           IDC_EXPOSURE_SLIDER,
                                           IDC_VIBRANCE_SLIDER,
                                           IDC_TEMPERATURE_SLIDER,
                                           IDC_TINT_SLIDER};
        HWND* correctionSliders[] = {
            &contrastSlider, &gammaSlider, &exposureSlider, &vibranceSlider, &temperatureSlider, &tintSlider};
        for (int i = 0; i < 6; ++i)
        {
            CreateWindowW(L"STATIC",
                          L"",
                          WS_CHILD | WS_VISIBLE,
                          0,
                          0,
                          80,
                          20,
                          hWnd,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(correctionLabelIds[i])),
                          hInst,
                          nullptr);
            *correctionSliders[i] = CreateWindowW(TRACKBAR_CLASSW,
                                                  nullptr,
                                                  WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                                  0,
                                                  0,
                                                  160,
                                                  40,
                                                  hWnd,
                                                  reinterpret_cast<HMENU>(static_cast<INT_PTR>(correctionSliderIds[i])),
                                                  hInst,
                                                  nullptr);
            SendMessageW(*correctionSliders[i], TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
            SendMessageW(*correctionSliders[i], TBM_SETTICFREQ, 25, 0);
            SendMessageW(*correctionSliders[i], TBM_SETPAGESIZE, 0, 10);
        }
        CreateWindowW(L"BUTTON",
                      L"Apply corrections",
                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      25,
                      207,
                      145,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_CORRECTION_APPLY),
                      hInst,
                      nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      340,
                      207,
                      700,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_CORRECTION_HELP),
                      hInst,
                      nullptr);
        for (int id : {IDC_RAMP_START, IDC_RAMP_MIDDLE, IDC_RAMP_END})
            CreateWindowW(L"BUTTON",
                          L"Color...",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          0,
                          0,
                          105,
                          30,
                          hWnd,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                          hInst,
                          nullptr);
        for (int id : {IDC_RAMP_START_SWATCH, IDC_RAMP_MIDDLE_SWATCH, IDC_RAMP_END_SWATCH})
            CreateWindowW(L"STATIC",
                          nullptr,
                          WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                          0,
                          0,
                          34,
                          30,
                          hWnd,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                          hInst,
                          nullptr);
        CreateWindowW(L"BUTTON",
                      L"Use middle",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      0,
                      0,
                      150,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_RAMP_USE_MIDDLE),
                      hInst,
                      nullptr);
        CreateWindowW(L"BUTTON",
                      L"Preserve contrast",
                      WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                      0,
                      0,
                      180,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_RAMP_PRESERVE),
                      hInst,
                      nullptr);
        SendDlgItemMessageW(hWnd, IDC_RAMP_PRESERVE, BM_SETCHECK, BST_CHECKED, 0);
        CreateWindowW(L"BUTTON",
                      L"Apply ramp",
                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      0,
                      0,
                      145,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_RAMP_APPLY),
                      hInst,
                      nullptr);
        CreateWindowW(L"STATIC",
                      L"Blend",
                      WS_CHILD | WS_VISIBLE,
                      0,
                      0,
                      80,
                      22,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_BLEND),
                      hInst,
                      nullptr);
        blendModeCombo = CreateWindowW(WC_COMBOBOXW,
                                       nullptr,
                                       WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                       0,
                                       0,
                                       145,
                                       200,
                                       hWnd,
                                       reinterpret_cast<HMENU>(IDC_BLEND_MODE),
                                       hInst,
                                       nullptr);
        blendStrengthSlider = CreateWindowW(TRACKBAR_CLASSW,
                                            nullptr,
                                            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                            0,
                                            0,
                                            180,
                                            40,
                                            hWnd,
                                            reinterpret_cast<HMENU>(IDC_BLEND_STRENGTH),
                                            hInst,
                                            nullptr);
        SendMessageW(blendStrengthSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(blendStrengthSlider, TBM_SETPOS, TRUE, 50);
        SendMessageW(blendStrengthSlider, TBM_SETTICFREQ, 25, 0);
        CreateWindowW(L"BUTTON",
                      L"Apply tone",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      0,
                      0,
                      145,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_BLEND_APPLY),
                      hInst,
                      nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      25,
                      202,
                      900,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_RAMP_HELP),
                      hInst,
                      nullptr);
        advancedModeCombo = CreateWindowW(WC_COMBOBOXW,
                                          nullptr,
                                          WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                          0,
                                          0,
                                          190,
                                          200,
                                          hWnd,
                                          reinterpret_cast<HMENU>(IDC_ADV_MODE),
                                          hInst,
                                          nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      0,
                      0,
                      70,
                      22,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_ADV_AMOUNT),
                      hInst,
                      nullptr);
        advancedAmountSlider = CreateWindowW(TRACKBAR_CLASSW,
                                             nullptr,
                                             WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                             0,
                                             0,
                                             210,
                                             40,
                                             hWnd,
                                             reinterpret_cast<HMENU>(IDC_ADV_AMOUNT),
                                             hInst,
                                             nullptr);
        SendMessageW(advancedAmountSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(advancedAmountSlider, TBM_SETPOS, TRUE, 100);
        SendMessageW(advancedAmountSlider, TBM_SETTICFREQ, 25, 0);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      0,
                      0,
                      70,
                      22,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_ADV_LEVELS),
                      hInst,
                      nullptr);
        advancedLevelsSlider = CreateWindowW(TRACKBAR_CLASSW,
                                             nullptr,
                                             WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                             0,
                                             0,
                                             180,
                                             40,
                                             hWnd,
                                             reinterpret_cast<HMENU>(IDC_ADV_LEVELS),
                                             hInst,
                                             nullptr);
        SendMessageW(advancedLevelsSlider, TBM_SETRANGE, TRUE, MAKELPARAM(2, 16));
        SendMessageW(advancedLevelsSlider, TBM_SETPOS, TRUE, 6);
        SendMessageW(advancedLevelsSlider, TBM_SETTICFREQ, 2, 0);
        CreateWindowW(L"BUTTON",
                      L"Apply",
                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      0,
                      0,
                      145,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_ADV_APPLY),
                      hInst,
                      nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      0,
                      0,
                      900,
                      50,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_ADV_HELP),
                      hInst,
                      nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      25,
                      228,
                      80,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_SIMILARITY),
                      hInst,
                      nullptr);
        similaritySlider = CreateWindowW(TRACKBAR_CLASSW,
                                         nullptr,
                                         WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                         105,
                                         216,
                                         220,
                                         40,
                                         hWnd,
                                         reinterpret_cast<HMENU>(IDC_SIMILARITY_SLIDER),
                                         hInst,
                                         nullptr);
        SendMessageW(similaritySlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 765));
        SendMessageW(similaritySlider, TBM_SETPOS, TRUE, 50);
        SendMessageW(similaritySlider, TBM_SETTICFREQ, 50, 0);
        SendMessageW(similaritySlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"BUTTON",
                      L"Target color...",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      345,
                      218,
                      125,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_TARGET_COLOR),
                      hInst,
                      nullptr);
        targetSwatch = CreateWindowW(L"STATIC",
                                     nullptr,
                                     WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
                                     475,
                                     218,
                                     34,
                                     30,
                                     hWnd,
                                     reinterpret_cast<HMENU>(IDC_TARGET_SWATCH),
                                     hInst,
                                     nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      530,
                      228,
                      65,
                      24,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_STRENGTH),
                      hInst,
                      nullptr);
        magicStrengthSlider = CreateWindowW(TRACKBAR_CLASSW,
                                            nullptr,
                                            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                            595,
                                            216,
                                            200,
                                            40,
                                            hWnd,
                                            reinterpret_cast<HMENU>(IDC_MAGIC_STRENGTH),
                                            hInst,
                                            nullptr);
        SendMessageW(magicStrengthSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(magicStrengthSlider, TBM_SETPOS, TRUE, 100);
        SendMessageW(magicStrengthSlider, TBM_SETTICFREQ, 25, 0);
        SendMessageW(magicStrengthSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"BUTTON",
                      L"Apply magic recolor",
                      WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                      815,
                      218,
                      155,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_APPLY_MAGIC),
                      hInst,
                      nullptr);
        animationToggle = CreateWindowW(L"BUTTON",
                                        L"Pause animation",
                                        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                        1095,
                                        24,
                                        140,
                                        30,
                                        hWnd,
                                        reinterpret_cast<HMENU>(IDC_ANIMATION_TOGGLE),
                                        hInst,
                                        nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      25,
                      258,
                      900,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_MAGIC_HINT),
                      hInst,
                      nullptr);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      480,
                      30,
                      110,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_ANIM_SPEED),
                      hInst,
                      nullptr);
        animationSpeedSlider = CreateWindowW(TRACKBAR_CLASSW,
                                             nullptr,
                                             WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                             590,
                                             18,
                                             175,
                                             40,
                                             hWnd,
                                             reinterpret_cast<HMENU>(IDC_ANIMATION_SPEED),
                                             hInst,
                                             nullptr);
        SendMessageW(animationSpeedSlider, TBM_SETRANGE, TRUE, MAKELPARAM(25, 200));
        SendMessageW(animationSpeedSlider, TBM_SETPOS, TRUE, 100);
        SendMessageW(animationSpeedSlider, TBM_SETTICFREQ, 25, 0);
        SendMessageW(animationSpeedSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"STATIC",
                      L"",
                      WS_CHILD | WS_VISIBLE,
                      780,
                      30,
                      55,
                      20,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_LABEL_PREVIEW_ZOOM),
                      hInst,
                      nullptr);
        previewZoomSlider = CreateWindowW(TRACKBAR_CLASSW,
                                          nullptr,
                                          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
                                          835,
                                          18,
                                          175,
                                          40,
                                          hWnd,
                                          reinterpret_cast<HMENU>(IDC_PREVIEW_ZOOM),
                                          hInst,
                                          nullptr);
        SendMessageW(previewZoomSlider, TBM_SETRANGE, TRUE, MAKELPARAM(25, 400));
        SendMessageW(previewZoomSlider, TBM_SETPOS, TRUE, 100);
        SendMessageW(previewZoomSlider, TBM_SETTICFREQ, 25, 0);
        SendMessageW(previewZoomSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"BUTTON",
                      L"100 %",
                      WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                      1015,
                      24,
                      70,
                      30,
                      hWnd,
                      reinterpret_cast<HMENU>(IDC_ZOOM_RESET),
                      hInst,
                      nullptr);
        for (HWND slider : {hueSlider,
                            saturationSlider,
                            lightnessSlider,
                            similaritySlider,
                            magicStrengthSlider,
                            animationSpeedSlider,
                            previewZoomSlider,
                            contrastSlider,
                            gammaSlider,
                            exposureSlider,
                            vibranceSlider,
                            temperatureSlider,
                            tintSlider,
                            blendStrengthSlider})
        {
            SendMessageW(slider, TBM_SETLINESIZE, 0, 1);
            if (setWindowSubclassFn)
                setWindowSubclassFn(slider, SliderSubclassProc, 1, 0);
        }
        ShowWindow(GetDlgItem(hWnd, IDC_GROUP_FRAME), SW_HIDE);
        ApplyUiFont(hWnd);
        ApplyLanguage(hWnd);
        LayoutEditorControls(hWnd);
        UpdatePreviewScrollbars(hWnd);
        SetTimer(hWnd, AnimationTimerId, 140, nullptr);

        //  // Create static labels
        //  HWND hInputLabel = CreateWindowW(L"STATIC", L"Input",
        //      WS_CHILD | WS_VISIBLE | SS_CENTER,
        //      0, 0, 0, 0, hWnd, (HMENU)1001, hInst, NULL);
        //  HWND hOutputLabel = CreateWindowW(L"STATIC", L"Output",
        //      WS_CHILD | WS_VISIBLE | SS_CENTER,
        //      0, 0, 0, 0, hWnd, (HMENU)1002, hInst, NULL);
        //
        //  // Set label font
        //  HFONT hFont = CreateFontW(
        //      16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
        //      DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        //      DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
        //  SendMessage(hInputLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        //  SendMessage(hOutputLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    break;

    case WM_SETCURSOR:
        if (eyedropperEnabled && LOWORD(lParam) == HTCLIENT)
        {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(hWnd, &point);
            const PreviewPanelLayout previewLayout = GetPreviewPanelLayout(hWnd);
            if (PtInRect(&previewLayout.viewport, point))
            {
                SetCursor(LoadCursorW(nullptr, IDC_CROSS));
                return TRUE;
            }
        }
        break;

    case WM_LBUTTONDOWN:
    {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const EditorLayout layout = GetEditorLayout(hWnd);

        // Check Output Palette
        for (int i = 0; i < 256; ++i)
        {
            RECT rect = PaletteCell(layout.outputPalette, layout.cellWidth, layout.cellHeight, i);

            if (PtInRect(&rect, pt))
            {
                if (IsPaletteIndexProtected(i))
                {
                    UpdateStatusBar(hWnd, Tr("status.special_protected").c_str());
                    return 0;
                }
                if (!(GetKeyState(VK_CONTROL) & 0x8000))
                    std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
                selectedPaletteColors[i] = !selectedPaletteColors[i];
                lastSelectedColor = i;
                ResetAdjustmentSliders();
                UpdateSelectionLabel();
                UpdateValueStatus(hWnd,
                                  "status.selection_changed",
                                  static_cast<int>(std::count(
                                      std::begin(selectedPaletteColors), std::end(selectedPaletteColors), true)));
                if (magicPreviewActive)
                    UpdatePreview(hWnd);
                else
                    InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }
        const int previewIndex = eyedropperEnabled ? HitTestPreviewPaletteIndex(hWnd, pt) : -1;
        if (previewIndex >= 0)
        {
            if (IsPaletteIndexProtected(previewIndex))
                UpdateStatusBar(hWnd, Tr("status.special_protected").c_str());
            else
            {
                if (!(GetKeyState(VK_CONTROL) & 0x8000))
                    std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
                selectedPaletteColors[previewIndex] = true;
                lastSelectedColor = previewIndex;
                ResetAdjustmentSliders();
                UpdateSelectionLabel();
                InvalidateRect(hWnd, nullptr, FALSE);
                UpdateStatusBar(hWnd, Tr("status.eyedropper_selected").c_str());
            }
            return 0;
        }
    }
    break;

    case WM_MOUSEWHEEL:
    {
        POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        ScreenToClient(hWnd, &point);
        const EditorLayout layout = GetEditorLayout(hWnd);
        if (PtInRect(&layout.preview, point) && previewZoomSlider)
        {
            const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            if (notches)
            {
                const int minimum = static_cast<int>(SendMessageW(previewZoomSlider, TBM_GETRANGEMIN, 0, 0));
                const int maximum = static_cast<int>(SendMessageW(previewZoomSlider, TBM_GETRANGEMAX, 0, 0));
                const int current = static_cast<int>(SendMessageW(previewZoomSlider, TBM_GETPOS, 0, 0));
                const int position = max(minimum, min(maximum, current + notches * 5));
                SendMessageW(previewZoomSlider, TBM_SETPOS, TRUE, position);
                SendMessageW(hWnd,
                             WM_HSCROLL,
                             MAKEWPARAM(TB_THUMBPOSITION, position),
                             reinterpret_cast<LPARAM>(previewZoomSlider));
            }
            return 0;
        }
        break;
    }

    case WM_DROPFILES:
    {
        const HDROP drop = reinterpret_cast<HDROP>(wParam);
        const UINT fileCount = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        bool opened = false;
        for (UINT index = 0; index < fileCount && !opened; ++index)
        {
            const UINT length = DragQueryFileW(drop, index, nullptr, 0);
            std::wstring path(static_cast<size_t>(length) + 1, L'\0');
            if (DragQueryFileW(drop, index, path.data(), length + 1))
            {
                path.resize(length);
                const std::filesystem::path candidate(path);
                if (_wcsicmp(candidate.extension().c_str(), L".def") == 0)
                    opened = OpenDEFPath(hWnd, candidate);
            }
        }
        DragFinish(drop);
        if (!opened)
            UpdateStatusBar(hWnd, Tr("status.drop_invalid").c_str());
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        POINT pt = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        const EditorLayout layout = GetEditorLayout(hWnd);
        for (int i = 0; i < 256; ++i)
        {
            RECT rect = PaletteCell(layout.outputPalette, layout.cellWidth, layout.cellHeight, i);
            if (PtInRect(&rect, pt))
            {
                if (IsPaletteIndexProtected(i))
                {
                    UpdateStatusBar(hWnd, Tr("status.special_protected").c_str());
                    return 0;
                }
                CHOOSECOLOR cc = {};
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hWnd;
                cc.lpCustColors = customColors; // Use the global custom colors
                cc.rgbResult = outputPalette[i];
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;

                if (ChooseColor(&cc))
                {
                    const auto before = outputPalette;
                    outputPalette[i] = cc.rgbResult;
                    CommitPaletteChange(hWnd, before);
                    ResetAdjustmentSliders();
                    UpdatePreview(hWnd);
                    UpdateStatusBar(hWnd, Tr("status.color_changed").c_str());
                }
                return 0;
            }
        }
    }
    break;

    case WM_HSCROLL:
    {
        const HWND movedSlider = reinterpret_cast<HWND>(lParam);
        if (movedSlider == previewHorizontalScroll)
        {
            ScrollPreview(hWnd, SB_HORZ, LOWORD(wParam), HIWORD(wParam));
            break;
        }
        if (movedSlider == previewZoomSlider)
        {
            previewZoomPercent = static_cast<int>(SendMessageW(previewZoomSlider, TBM_GETPOS, 0, 0));
            UpdatePreviewScrollbars(hWnd);
            CenterPreviewOnVisibleContent(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
                UpdateValueStatus(hWnd, "status.zoom", previewZoomPercent);
            break;
        }
        if (movedSlider == animationSpeedSlider)
        {
            const int speedPercent = static_cast<int>(SendMessageW(animationSpeedSlider, TBM_GETPOS, 0, 0));
            const UINT intervalMs = static_cast<UINT>(max(35, 14000 / max(25, speedPercent)));
            SetTimer(hWnd, AnimationTimerId, intervalMs, nullptr);
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
                UpdateValueStatus(hWnd, "status.animation_speed", speedPercent);
            break;
        }
        if (movedSlider == blendStrengthSlider)
        {
            if (BuildRampOrTonePreview(hWnd, 2))
                UpdatePreview(hWnd);
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
                UpdateStatusBar(hWnd, Tr("status.tone_preview").c_str());
            break;
        }
        if (movedSlider == advancedAmountSlider || movedSlider == advancedLevelsSlider)
        {
            if (BuildAdvancedPreview(hWnd))
                UpdatePreview(hWnd);
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
                UpdateStatusBar(hWnd, Tr("status.advanced_preview").c_str());
            break;
        }
        if (movedSlider == magicStrengthSlider || movedSlider == similaritySlider)
        {
            if (movedSlider == magicStrengthSlider)
            {
                const bool hasSelection = std::any_of(std::begin(selectedPaletteColors),
                                                      std::end(selectedPaletteColors),
                                                      [](bool selected) { return selected; });
                if (hasSelection)
                {
                    magicPreviewActive = true;
                    UpdatePreview(hWnd);
                }
            }
            if (movedSlider == similaritySlider && lastSelectedColor >= 0)
            {
                const int tolerance = static_cast<int>(SendMessageW(similaritySlider, TBM_GETPOS, 0, 0));
                const COLORREF seed = outputPalette[static_cast<size_t>(lastSelectedColor)];
                std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
                for (size_t i = 8; i < outputPalette.size(); ++i)
                    selectedPaletteColors[i] = ColorDistance(seed, outputPalette[i]) <= tolerance;
                ResetAdjustmentSliders();
                UpdateSelectionLabel();
                if (magicPreviewActive)
                    UpdatePreview(hWnd);
                else
                    InvalidateRect(hWnd, nullptr, FALSE);
            }
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
                UpdateStatusBar(
                    hWnd,
                    Tr(movedSlider == similaritySlider ? "status.similarity_changed" : "status.strength_changed")
                        .c_str());
            break;
        }
        if (movedSlider == contrastSlider || movedSlider == gammaSlider || movedSlider == exposureSlider ||
            movedSlider == vibranceSlider || movedSlider == temperatureSlider || movedSlider == tintSlider)
        {
            if (!correctionPreviewActive)
            {
                correctionBase = outputPalette;
                correctionPreviewActive = true;
            }
            const int contrast = static_cast<int>(SendMessageW(contrastSlider, TBM_GETPOS, 0, 0));
            const int gamma = static_cast<int>(SendMessageW(gammaSlider, TBM_GETPOS, 0, 0));
            const int exposure = static_cast<int>(SendMessageW(exposureSlider, TBM_GETPOS, 0, 0));
            const int vibrance = static_cast<int>(SendMessageW(vibranceSlider, TBM_GETPOS, 0, 0));
            const int temperature = static_cast<int>(SendMessageW(temperatureSlider, TBM_GETPOS, 0, 0));
            const int tint = static_cast<int>(SendMessageW(tintSlider, TBM_GETPOS, 0, 0));
            for (size_t i = 0; i < outputPalette.size(); ++i)
                if (selectedPaletteColors[i] && !IsPaletteIndexProtected(i))
                    outputPalette[i] = ApplyColorCorrections(
                        correctionBase[i], contrast, gamma, exposure, vibrance, temperature, tint);
            UpdatePreview(hWnd);
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
                UpdateStatusBar(hWnd, Tr("status.correction_preview").c_str());
            break;
        }
        if (movedSlider == hueSlider || movedSlider == saturationSlider || movedSlider == lightnessSlider)
        {
            if (!hslHistoryActive)
            {
                hslHistoryBase = outputPalette;
                hslHistoryActive = true;
            }
            const int hue = static_cast<int>(SendMessageW(hueSlider, TBM_GETPOS, 0, 0));
            const int saturation = static_cast<int>(SendMessageW(saturationSlider, TBM_GETPOS, 0, 0));
            const int lightness = static_cast<int>(SendMessageW(lightnessSlider, TBM_GETPOS, 0, 0));
            const bool preserve = SendMessageW(preserveShadesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            COLORREF sharedColor = RGB(0, 0, 0);
            if (!preserve)
                for (size_t i = 0; i < outputPalette.size(); ++i)
                    if (selectedPaletteColors[i])
                    {
                        sharedColor = AdjustHsl(adjustmentBase[i], hue, saturation, lightness);
                        break;
                    }
            for (size_t i = 0; i < outputPalette.size(); ++i)
                if (selectedPaletteColors[i])
                    outputPalette[i] =
                        preserve ? AdjustHsl(adjustmentBase[i], hue, saturation, lightness) : sharedColor;
            UpdatePreview(hWnd);
            const UINT scrollCode = LOWORD(wParam);
            if (scrollCode != TB_THUMBTRACK)
            {
                CommitPaletteChange(hWnd, hslHistoryBase);
                hslHistoryBase.clear();
                hslHistoryActive = false;
            }
            if (scrollCode == TB_ENDTRACK || scrollCode == TB_THUMBPOSITION)
                UpdateStatusBar(hWnd, Tr("status.hsl_changed").c_str());
        }
    }
    break;

    case WM_VSCROLL:
        if (reinterpret_cast<HWND>(lParam) == previewVerticalScroll)
            ScrollPreview(hWnd, SB_VERT, LOWORD(wParam), HIWORD(wParam));
        break;

    case WM_TIMER:
        if (wParam == AnimationTimerId && animationPlaying && frameCount && currentFrame < editorFrames.size())
        {
            const uint32_t group = editorFrames[currentFrame].group;
            uint32_t next = animationFrame;
            do
            {
                next = (next + 1) % frameCount;
            } while (next != animationFrame && editorFrames[next].group != group);
            animationFrame = next;
            UpdateAnimationPreview(hWnd);
        }
        break;

    case WM_DRAWITEM:
        if (wParam == IDC_EDITOR_TABS)
        {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            wchar_t text[128]{};
            TCITEMW tab{};
            tab.mask = TCIF_TEXT;
            tab.pszText = text;
            tab.cchTextMax = ARRAYSIZE(text);
            TabCtrl_GetItem(editorTabs, item->itemID, &tab);
            const bool selected = static_cast<int>(item->itemID) == TabCtrl_GetCurSel(editorTabs);
            const COLORREF background = selected ? UiCard : RGB(237, 241, 246);
            HBRUSH brush = CreateSolidBrush(background);
            FillRect(item->hDC, &item->rcItem, brush);
            DeleteObject(brush);
            if (selected)
            {
                RECT accent = item->rcItem;
                accent.top = accent.bottom - 3;
                HBRUSH accentBrush = CreateSolidBrush(UiAccent);
                FillRect(item->hDC, &accent, accentBrush);
                DeleteObject(accentBrush);
            }
            HPEN separator = CreatePen(PS_SOLID, 1, UiBorder);
            HGDIOBJ oldPen = SelectObject(item->hDC, separator);
            MoveToEx(item->hDC, item->rcItem.right - 1, item->rcItem.top + 7, nullptr);
            LineTo(item->hDC, item->rcItem.right - 1, item->rcItem.bottom - 7);
            SelectObject(item->hDC, oldPen);
            DeleteObject(separator);
            SetBkMode(item->hDC, TRANSPARENT);
            SetTextColor(item->hDC, selected ? UiAccent : UiMuted);
            HFONT previous = static_cast<HFONT>(SelectObject(item->hDC, selected ? uiBoldFont : uiFont));
            RECT textRect = item->rcItem;
            InflateRect(&textRect, -12, 0);
            DrawTextW(item->hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(item->hDC, previous);
            return TRUE;
        }
        if (reinterpret_cast<DRAWITEMSTRUCT*>(lParam)->CtlType == ODT_BUTTON)
        {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            const int id = static_cast<int>(item->CtlID);
            const bool primary = id == IDC_APPLY_MAGIC || id == IDC_CORRECTION_APPLY || id == IDC_RAMP_APPLY ||
                                 id == IDC_BLEND_APPLY || id == IDC_ADV_APPLY;
            const bool toggle = id == IDC_EYEDROPPER;
            const bool checked = toggle && SendMessageW(item->hwndItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
            const bool category = id >= IDC_CATEGORY_SELECTION && id <= IDC_CATEGORY_RESTORE;
            const bool categorySelected =
                category && editorTabs && TabCtrl_GetCurSel(editorTabs) == id - IDC_CATEGORY_SELECTION;
            const bool disabled = (item->itemState & ODS_DISABLED) != 0;
            const bool pressed = (item->itemState & ODS_SELECTED) != 0;
            const bool hot = (item->itemState & ODS_HOTLIGHT) != 0;
            COLORREF fill = UiCard;
            COLORREF border = UiBorder;
            COLORREF textColor = UiText;
            if (primary)
            {
                fill = disabled ? RGB(203, 213, 225) : (pressed ? UiAccentHover : UiAccent);
                border = fill;
                textColor = RGB(255, 255, 255);
            }
            else if (disabled)
            {
                fill = RGB(241, 245, 249);
                textColor = RGB(148, 163, 184);
            }
            else if (checked || categorySelected)
            {
                fill = RGB(239, 246, 255);
                border = UiAccent;
                textColor = UiAccent;
            }
            else if (pressed || hot)
            {
                fill = pressed ? RGB(226, 232, 240) : RGB(248, 250, 252);
                border = pressed ? RGB(148, 163, 184) : UiAccent;
            }
            HBRUSH brush = CreateSolidBrush(fill);
            HPEN pen = CreatePen(PS_SOLID, 1, border);
            HGDIOBJ oldBrush = SelectObject(item->hDC, brush), oldPen = SelectObject(item->hDC, pen);
            RoundRect(item->hDC, item->rcItem.left, item->rcItem.top, item->rcItem.right, item->rcItem.bottom, 8, 8);
            SelectObject(item->hDC, oldBrush);
            SelectObject(item->hDC, oldPen);
            DeleteObject(brush);
            DeleteObject(pen);
            wchar_t text[256]{};
            GetWindowTextW(item->hwndItem, text, ARRAYSIZE(text));
            SetBkMode(item->hDC, TRANSPARENT);
            SetTextColor(item->hDC, textColor);
            HFONT previous = static_cast<HFONT>(SelectObject(item->hDC, primary ? uiBoldFont : uiFont));
            RECT textRect = item->rcItem;
            InflateRect(&textRect, -10, -2);
            DrawTextW(item->hDC, text, -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectObject(item->hDC, previous);
            if (categorySelected)
            {
                RECT accent = item->rcItem;
                accent.top = accent.bottom - 3;
                InflateRect(&accent, -8, 0);
                HBRUSH accentBrush = CreateSolidBrush(UiAccent);
                FillRect(item->hDC, &accent, accentBrush);
                DeleteObject(accentBrush);
            }
            if (item->itemState & ODS_FOCUS)
            {
                RECT focus = item->rcItem;
                InflateRect(&focus, -4, -4);
                DrawFocusRect(item->hDC, &focus);
            }
            return TRUE;
        }
        if (wParam == IDC_TARGET_SWATCH || wParam == IDC_RAMP_START_SWATCH || wParam == IDC_RAMP_MIDDLE_SWATCH ||
            wParam == IDC_RAMP_END_SWATCH)
        {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            COLORREF color = magicTargetColor;
            if (wParam == IDC_RAMP_START_SWATCH)
                color = rampStartColor;
            else if (wParam == IDC_RAMP_MIDDLE_SWATCH)
                color = rampMiddleColor;
            else if (wParam == IDC_RAMP_END_SWATCH)
                color = rampEndColor;
            HBRUSH brush = CreateSolidBrush(color);
            FillRect(item->hDC, &item->rcItem, brush);
            DeleteObject(brush);
            FrameRect(item->hDC, &item->rcItem, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
            return TRUE;
        }
        break;

    case WM_CTLCOLORSTATIC:
    {
        HDC controlDc = reinterpret_cast<HDC>(wParam);
        SetBkMode(controlDc, TRANSPARENT);
        SetTextColor(controlDc, UiText);
        SetBkColor(controlDc, UiBackground);
        return reinterpret_cast<LRESULT>(uiBackgroundBrush ? uiBackgroundBrush : GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_CTLCOLORBTN:
    {
        HDC controlDc = reinterpret_cast<HDC>(wParam);
        SetBkMode(controlDc, TRANSPARENT);
        SetTextColor(controlDc, UiText);
        SetBkColor(controlDc, UiBackground);
        return reinterpret_cast<LRESULT>(uiBackgroundBrush ? uiBackgroundBrush : GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        if (wmId >= IDM_RECENT_FIRST && wmId <= IDM_RECENT_LAST)
        {
            const size_t index = static_cast<size_t>(wmId - IDM_RECENT_FIRST);
            if (index < recentFiles.size())
                OpenDEFPath(hWnd, recentFiles[index]);
            return 0;
        }
        if (wmId >= IDM_LANGUAGE_FIRST && wmId <= IDM_LANGUAGE_LAST)
        {
            const size_t index = static_cast<size_t>(wmId - IDM_LANGUAGE_FIRST);
            if (index < availableLanguages.size() && LoadLanguage(availableLanguages[index].id))
                ApplyLanguage(hWnd);
            return 0;
        }
        if (wmId >= IDC_CATEGORY_SELECTION && wmId <= IDC_CATEGORY_RESTORE)
        {
            const int category = wmId - IDC_CATEGORY_SELECTION;
            const int previous = editorTabs ? TabCtrl_GetCurSel(editorTabs) : 0;
            if (previous != category)
            {
                const bool cancelled = CancelLivePreview(hWnd, false);
                if (cancelled)
                    UpdatePreview(hWnd);
            }
            TabCtrl_SetCurSel(editorTabs, category);
            UpdateEditorTabVisibility(hWnd);
            for (HWND button : categoryButtons)
                if (button)
                    InvalidateRect(button, nullptr, TRUE);
            InvalidateRect(hWnd, nullptr, FALSE);
            return 0;
        }
        switch (wmId)
        {
        case IDM_SELECT_REDS:
        case IDM_SELECT_GREENS:
        case IDM_SELECT_BLUES:
        case IDM_SELECT_WARM:
        case IDM_SELECT_COOL:
        case IDM_SELECT_NEUTRALS:
        case IDM_SELECT_HIGHLIGHTS:
        case IDM_SELECT_SHADOWS:
            SelectColorRange(hWnd, wmId);
            break;
        case IDM_UNDO:
            if (UndoPaletteChange(hWnd))
            {
                UpdatePreview(hWnd);
                UpdateStatusBar(hWnd, Tr("status.undo").c_str());
            }
            break;
        case IDM_REDO:
            if (RedoPaletteChange(hWnd))
            {
                UpdatePreview(hWnd);
                UpdateStatusBar(hWnd, Tr("status.redo").c_str());
            }
            break;
        case IDM_HELP_GUIDE:
            ShowInformationDialog(hWnd, true);
            break;
        case IDM_CLEAR_RECENT:
            recentFiles.clear();
            SaveSettings();
            BuildApplicationMenu(hWnd);
            UpdateStatusBar(hWnd, Tr("status.recent_cleared").c_str());
            break;
        case IDC_ANIMATION_TOGGLE:
            animationPlaying = !animationPlaying;
            SetWindowTextW(animationToggle, Tr(animationPlaying ? "animation.pause" : "animation.play").c_str());
            UpdateStatusBar(hWnd,
                            Tr(animationPlaying ? "status.animation_playing" : "status.animation_paused").c_str());
            break;
        case IDC_ZOOM_RESET:
            previewZoomPercent = 100;
            previewScrollX = previewScrollY = 0;
            SendMessageW(previewZoomSlider, TBM_SETPOS, TRUE, 100);
            UpdatePreviewScrollbars(hWnd);
            InvalidateRect(hWnd, nullptr, FALSE);
            UpdateValueStatus(hWnd, "status.zoom", previewZoomPercent);
            break;
        case IDC_SELECT_SIMILAR:
            if (lastSelectedColor >= 0)
            {
                const int tolerance = static_cast<int>(SendMessageW(similaritySlider, TBM_GETPOS, 0, 0));
                const COLORREF seed = outputPalette[static_cast<size_t>(lastSelectedColor)];
                std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
                for (size_t i = 8; i < outputPalette.size(); ++i)
                    selectedPaletteColors[i] = ColorDistance(seed, outputPalette[i]) <= tolerance;
                ResetAdjustmentSliders();
                UpdateSelectionLabel();
                if (magicPreviewActive)
                    UpdatePreview(hWnd);
                else
                    InvalidateRect(hWnd, nullptr, FALSE);
            }
            else
                UpdateStatusBar(hWnd, Tr("status.select_color").c_str());
            break;
        case IDC_TARGET_COLOR:
        {
            CHOOSECOLOR cc{};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hWnd;
            cc.lpCustColors = customColors;
            cc.rgbResult = magicTargetColor;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc))
            {
                magicTargetColor = cc.rgbResult;
                magicPreviewActive = true;
                InvalidateRect(targetSwatch, nullptr, TRUE);
                UpdatePreview(hWnd);
                UpdateStatusBar(hWnd, Tr("status.target_changed").c_str());
            }
        }
        break;
        case IDC_RAMP_START:
        case IDC_RAMP_MIDDLE:
        case IDC_RAMP_END:
        {
            COLORREF* color =
                wmId == IDC_RAMP_START ? &rampStartColor : (wmId == IDC_RAMP_MIDDLE ? &rampMiddleColor : &rampEndColor);
            CHOOSECOLOR cc{};
            cc.lStructSize = sizeof(cc);
            cc.hwndOwner = hWnd;
            cc.lpCustColors = customColors;
            cc.rgbResult = *color;
            cc.Flags = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColorW(&cc))
            {
                *color = cc.rgbResult;
                const int swatch = wmId == IDC_RAMP_START
                                       ? IDC_RAMP_START_SWATCH
                                       : (wmId == IDC_RAMP_MIDDLE ? IDC_RAMP_MIDDLE_SWATCH : IDC_RAMP_END_SWATCH);
                InvalidateRect(GetDlgItem(hWnd, swatch), nullptr, TRUE);
                const int mode = rampPreviewMode ? rampPreviewMode : 1;
                if (BuildRampOrTonePreview(hWnd, mode))
                {
                    UpdatePreview(hWnd);
                    UpdateStatusBar(hWnd, Tr(mode == 1 ? "status.ramp_preview" : "status.tone_preview").c_str());
                }
            }
            break;
        }
        case IDC_RAMP_APPLY:
        {
            if ((!rampPreviewMode && !BuildRampOrTonePreview(hWnd, 1)) ||
                (rampPreviewMode != 1 && !BuildRampOrTonePreview(hWnd, 1)))
            {
                UpdateStatusBar(hWnd, Tr("status.select_color").c_str());
                break;
            }
            CommitPaletteChange(hWnd, rampPreviewBase);
            rampPreviewMode = 0;
            ResetAdjustmentSliders();
            UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.ramp_applied").c_str());
            break;
        }
        case IDC_BLEND_APPLY:
        {
            if ((!rampPreviewMode && !BuildRampOrTonePreview(hWnd, 2)) ||
                (rampPreviewMode != 2 && !BuildRampOrTonePreview(hWnd, 2)))
            {
                UpdateStatusBar(hWnd, Tr("status.select_color").c_str());
                break;
            }
            CommitPaletteChange(hWnd, rampPreviewBase);
            rampPreviewMode = 0;
            ResetAdjustmentSliders();
            UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.blend_applied").c_str());
            break;
        }
        case IDC_RAMP_USE_MIDDLE:
        case IDC_RAMP_PRESERVE:
            if (BuildRampOrTonePreview(hWnd, 1))
            {
                UpdatePreview(hWnd);
                UpdateStatusBar(hWnd, Tr("status.ramp_preview").c_str());
            }
            break;
        case IDC_BLEND_MODE:
            if (HIWORD(wParam) == CBN_SELCHANGE && BuildRampOrTonePreview(hWnd, 2))
            {
                UpdatePreview(hWnd);
                UpdateStatusBar(hWnd, Tr("status.tone_preview").c_str());
            }
            break;
        case IDC_ADV_MODE:
            if (HIWORD(wParam) == CBN_SELCHANGE && BuildAdvancedPreview(hWnd))
            {
                UpdatePreview(hWnd);
                UpdateStatusBar(hWnd, Tr("status.advanced_preview").c_str());
            }
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                const BOOL posterize = SendMessageW(advancedModeCombo, CB_GETCURSEL, 0, 0) == 3;
                EnableWindow(advancedLevelsSlider, posterize);
                EnableWindow(GetDlgItem(hWnd, IDC_LABEL_ADV_LEVELS), posterize);
            }
            break;
        case IDC_ADV_APPLY:
            if ((!advancedPreviewActive && !BuildAdvancedPreview(hWnd)))
            {
                UpdateStatusBar(hWnd, Tr("status.select_color").c_str());
                break;
            }
            CommitPaletteChange(hWnd, advancedPreviewBase);
            advancedPreviewActive = false;
            ResetAdjustmentSliders();
            UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.advanced_applied").c_str());
            break;
        case IDC_CANCEL_PREVIEW:
            CancelLivePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.preview_cancelled").c_str());
            break;
        case IDC_APPLY_MAGIC:
        {
            const bool hasSelection = std::any_of(std::begin(selectedPaletteColors),
                                                  std::end(selectedPaletteColors),
                                                  [](bool selected) { return selected; });
            if (!hasSelection)
            {
                UpdateStatusBar(hWnd, Tr("status.select_color").c_str());
                break;
            }
            const auto before = outputPalette;
            const int strength = static_cast<int>(SendMessageW(magicStrengthSlider, TBM_GETPOS, 0, 0));
            const bool preserve = SendMessageW(preserveShadesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            for (size_t i = 0; i < outputPalette.size(); ++i)
                if (selectedPaletteColors[i])
                    outputPalette[i] = MagicRecolor(outputPalette[i], magicTargetColor, strength, preserve);
            CommitPaletteChange(hWnd, before);
            magicPreviewActive = false;
            ResetAdjustmentSliders();
            UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.magic_applied").c_str());
        }
        break;
        case IDC_PROTECT_SPECIAL:
            protectSpecialColors = SendDlgItemMessageW(hWnd, IDC_PROTECT_SPECIAL, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (protectSpecialColors)
                for (size_t i = 0; i < 8; ++i)
                    selectedPaletteColors[i] = false;
            ResetAdjustmentSliders();
            UpdateSelectionLabel();
            InvalidateRect(hWnd, nullptr, FALSE);
            UpdateStatusBar(hWnd, Tr(protectSpecialColors ? "status.protection_on" : "status.protection_off").c_str());
            break;
        case IDC_EYEDROPPER:
            eyedropperEnabled = !eyedropperEnabled;
            SendDlgItemMessageW(hWnd, IDC_EYEDROPPER, BM_SETCHECK, eyedropperEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
            InvalidateRect(GetDlgItem(hWnd, IDC_EYEDROPPER), nullptr, TRUE);
            UpdateStatusBar(hWnd, Tr(eyedropperEnabled ? "status.eyedropper_on" : "status.eyedropper_off").c_str());
            break;
        case IDC_CORRECTION_APPLY:
            if (correctionPreviewActive)
            {
                CommitPaletteChange(hWnd, correctionBase);
                correctionPreviewActive = false;
                ResetCorrectionSliders();
                ResetAdjustmentSliders();
                UpdatePreview(hWnd);
                UpdateStatusBar(hWnd, Tr("status.correction_applied").c_str());
            }
            else
                UpdateStatusBar(hWnd, Tr("status.select_color").c_str());
            break;
        case IDC_SELECT_FRAME_USED:
            SelectByUsage(hWnd, UsageSelection::Frame);
            break;
        case IDC_SELECT_GROUP_USED:
            SelectByUsage(hWnd, UsageSelection::Group);
            break;
        case IDC_SELECT_DEF_USED:
            SelectByUsage(hWnd, UsageSelection::Def);
            break;
        case IDC_SELECT_GROUP_COMMON:
            SelectByUsage(hWnd, UsageSelection::CommonGroup);
            break;
        case IDC_SELECT_UNUSED:
            SelectByUsage(hWnd, UsageSelection::Unused);
            break;
        case IDC_SELECT_ALL:
            std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), true);
            if (protectSpecialColors)
                for (size_t i = 0; i < 8; ++i)
                    selectedPaletteColors[i] = false;
            ResetAdjustmentSliders();
            UpdateSelectionLabel();
            InvalidateRect(hWnd, nullptr, FALSE);
            UpdateStatusBar(hWnd, Tr("status.selected_all").c_str());
            break;
        case IDC_CLEAR_SELECTION:
            std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
            ResetAdjustmentSliders();
            UpdateSelectionLabel();
            InvalidateRect(hWnd, nullptr, FALSE);
            UpdateStatusBar(hWnd, Tr("status.selection_cleared").c_str());
            break;
        case IDC_RESET_SELECTED:
        {
            const auto before = outputPalette;
            for (size_t i = 0; i < outputPalette.size(); ++i)
                if (selectedPaletteColors[i])
                    outputPalette[i] = inputPalette[i];
            CommitPaletteChange(hWnd, before);
            ResetAdjustmentSliders();
            UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.reset_selected").c_str());
            break;
        }
        case IDC_RESET_ALL:
        {
            const auto before = outputPalette;
            outputPalette = inputPalette;
            CommitPaletteChange(hWnd, before);
            std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
            lastSelectedColor = -1;
            magicPreviewActive = false;
            ResetAdjustmentSliders();
            UpdatePreview(hWnd);
            UpdateSelectionLabel();
            UpdateStatusBar(hWnd, Tr("status.reset_all").c_str());
            break;
        }
        case IDC_RESET_SLIDERS:
        {
            const bool cancelled = CancelLivePreview(hWnd, false);
            if (cancelled)
                UpdatePreview(hWnd);
            else
                ResetAdjustmentSliders();
            UpdateStatusBar(hWnd, Tr("status.reset_sliders").c_str());
            break;
        }
        case IDC_PRESERVE_SHADES:
            if (magicPreviewActive)
                UpdatePreview(hWnd);
            if (hueSlider && saturationSlider && lightnessSlider &&
                (SendMessageW(hueSlider, TBM_GETPOS, 0, 0) != 0 ||
                 SendMessageW(saturationSlider, TBM_GETPOS, 0, 0) != 0 ||
                 SendMessageW(lightnessSlider, TBM_GETPOS, 0, 0) != 0))
            {
                const int position = static_cast<int>(SendMessageW(hueSlider, TBM_GETPOS, 0, 0));
                SendMessageW(
                    hWnd, WM_HSCROLL, MAKEWPARAM(TB_THUMBTRACK, position), reinterpret_cast<LPARAM>(hueSlider));
            }
            UpdateStatusBar(hWnd,
                            Tr(SendMessageW(preserveShadesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED
                                   ? "status.preserve_on"
                                   : "status.preserve_off")
                                .c_str());
            break;
        case IDC_GROUP_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                const int selected = static_cast<int>(SendMessageW(groupCombo, CB_GETCURSEL, 0, 0));
                if (selected >= 0)
                {
                    const uint32_t group = static_cast<uint32_t>(SendMessageW(groupCombo, CB_GETITEMDATA, selected, 0));
                    PopulateFrameCombo(group);
                    UpdatePreview(hWnd);
                    UpdateStatusBar(hWnd, Tr("status.group_changed").c_str());
                }
            }
            break;
        case IDC_FRAME_COMBO:
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                const int selected = static_cast<int>(SendMessageW(frameCombo, CB_GETCURSEL, 0, 0));
                if (selected >= 0)
                {
                    currentFrame = static_cast<uint32_t>(SendMessageW(frameCombo, CB_GETITEMDATA, selected, 0));
                    animationFrame = currentFrame;
                    UpdatePreview(hWnd);
                    UpdateStatusBar(hWnd, Tr("status.frame_changed").c_str());
                }
            }
            break;
        case IDM_LOAD_DEF:
            LoadDEFFile(hWnd);
            InvalidateRect(hWnd, NULL, TRUE);
            break;
        case IDM_SAVE_DEF:
            SaveDEFFile(hWnd);
            break;
        case IDM_EXPORT_JSON:
            ExportPaletteToJSON(hWnd);
            break;
        case IDM_IMPORT_JSON:
            ImportPaletteFromJSON(hWnd);
            break;
        case IDM_EXIT:
            PostQuitMessage(0);
            break;
        case IDM_ABOUT:
            ShowInformationDialog(hWnd, false);
            break;
        }
    }
    break;

    case WM_SIZE:
    {
        if (hStatusBar)
            SendMessage(hStatusBar, WM_SIZE, 0, 0);
        LayoutEditorControls(hWnd);
        UpdatePreviewScrollbars(hWnd);
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    break;

    case WM_NOTIFY:
        if (reinterpret_cast<NMHDR*>(lParam)->idFrom == IDC_EDITOR_TABS &&
            reinterpret_cast<NMHDR*>(lParam)->code == TCN_SELCHANGE)
        {
            if (correctionPreviewActive && TabCtrl_GetCurSel(editorTabs) != 1)
            {
                CommitPaletteChange(hWnd, correctionBase);
                correctionPreviewActive = false;
                ResetCorrectionSliders();
                ResetAdjustmentSliders();
            }
            if (rampPreviewMode && TabCtrl_GetCurSel(editorTabs) != 2)
            {
                CommitPaletteChange(hWnd, rampPreviewBase);
                rampPreviewMode = 0;
                ResetAdjustmentSliders();
            }
            if (advancedPreviewActive && TabCtrl_GetCurSel(editorTabs) != 4)
            {
                CommitPaletteChange(hWnd, advancedPreviewBase);
                advancedPreviewActive = false;
                ResetAdjustmentSliders();
            }
            UpdateEditorTabVisibility(hWnd);
            InvalidateRect(hWnd, nullptr, TRUE);
            return 0;
        }
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        HFONT paintFont = static_cast<HFONT>(SelectObject(hdc, uiFont));
        const EditorLayout layout = GetEditorLayout(hWnd);
        RECT clientRect{};
        GetClientRect(hWnd, &clientRect);
        FillRect(hdc, &ps.rcPaint, uiBackgroundBrush ? uiBackgroundBrush : GetSysColorBrush(COLOR_WINDOW));
        RECT navigationCard{10, 4, clientRect.right - 10, 70};
        FillRect(hdc, &navigationCard, uiBackgroundBrush);
        HBRUSH navigationBorder = CreateSolidBrush(UiBorder);
        FrameRect(hdc, &navigationCard, navigationBorder);
        DeleteObject(navigationBorder);
        RECT propertiesCard{layout.preview.right + 10, 74, clientRect.right - 10, clientRect.bottom - 32};
        FillRect(hdc, &propertiesCard, uiBackgroundBrush);
        HBRUSH propertiesBorder = CreateSolidBrush(UiBorder);
        FrameRect(hdc, &propertiesCard, propertiesBorder);
        DeleteObject(propertiesBorder);

        if (!defLoaded)
        {
            RECT emptyCard{80, layout.inputPalette.top - 24, layout.preview.right - 70, layout.preview.bottom - 35};
            FillRect(hdc, &emptyCard, uiCardBrush);
            HBRUSH emptyBorder = CreateSolidBrush(UiBorder);
            FrameRect(hdc, &emptyCard, emptyBorder);
            DeleteObject(emptyBorder);
            RECT accent = emptyCard;
            accent.bottom = accent.top + 5;
            HBRUSH accentBrush = CreateSolidBrush(UiAccent);
            FillRect(hdc, &accent, accentBrush);
            DeleteObject(accentBrush);
            RECT title = emptyCard;
            title.top += 105;
            title.bottom = title.top + 42;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, UiText);
            SelectObject(hdc, uiTitleFont);
            DrawTextW(hdc, Tr("empty.title").c_str(), -1, &title, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            RECT subtitle = emptyCard;
            subtitle.top = title.bottom + 8;
            subtitle.bottom = subtitle.top + 60;
            SetTextColor(hdc, UiMuted);
            SelectObject(hdc, uiFont);
            const std::wstring emptyText =
                previewMessage.empty() || previewMessage == Tr("preview.load") ? Tr("empty.subtitle") : previewMessage;
            DrawTextW(hdc, emptyText.c_str(), -1, &subtitle, DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
            SelectObject(hdc, paintFont);
            EndPaint(hWnd, &ps);
            return 0;
        }

        auto drawPaletteCard = [&](const RECT& palette, const wchar_t* text)
        {
            RECT card{palette.left, palette.top - 30, palette.right, palette.bottom};
            FillRect(hdc, &card, uiCardBrush);
            HBRUSH cardBorder = CreateSolidBrush(UiBorder);
            FrameRect(hdc, &card, cardBorder);
            DeleteObject(cardBorder);
            RECT caption{card.left + 12, card.top, card.right - 12, palette.top};
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, UiMuted);
            HFONT previous = static_cast<HFONT>(SelectObject(hdc, uiBoldFont));
            DrawTextW(hdc, text, -1, &caption, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
            SelectObject(hdc, previous);
        };
        drawPaletteCard(layout.inputPalette, Tr("preview.original_palette").c_str());
        drawPaletteCard(layout.outputPalette, Tr("preview.edited_palette").c_str());

        // Draw input palette
        for (int i = 0; i < 256; ++i)
        {
            COLORREF color = inputPalette[i];
            HBRUSH hBrush = CreateSolidBrush(color);
            RECT rect = PaletteCell(layout.inputPalette, layout.cellWidth, layout.cellHeight, i);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }

        // Draw output palette
        for (int i = 0; i < 256; ++i)
        {
            COLORREF color = outputPalette[i];
            HBRUSH hBrush = CreateSolidBrush(color);
            RECT rect = PaletteCell(layout.outputPalette, layout.cellWidth, layout.cellHeight, i);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
            if (selectedPaletteColors[i])
            {
                FrameRect(hdc, &rect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
                InflateRect(&rect, -1, -1);
                FrameRect(hdc, &rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
            }
            if (IsPaletteIndexProtected(i))
            {
                RECT lockRect = PaletteCell(layout.outputPalette, layout.cellWidth, layout.cellHeight, i);
                FrameRect(hdc, &lockRect, static_cast<HBRUSH>(GetStockObject(DKGRAY_BRUSH)));
                MoveToEx(hdc, lockRect.left + 2, lockRect.top + 2, nullptr);
                LineTo(hdc, lockRect.right - 2, lockRect.bottom - 2);
                MoveToEx(hdc, lockRect.right - 3, lockRect.top + 2, nullptr);
                LineTo(hdc, lockRect.left + 1, lockRect.bottom - 2);
            }
        }

        HBRUSH borderBrush = CreateSolidBrush(UiBorder);
        FrameRect(hdc, &layout.inputPalette, borderBrush);
        FrameRect(hdc, &layout.outputPalette, borderBrush);
        DeleteObject(borderBrush);

        if ((originalPreviewBitmap || previewBitmap) && layout.preview.right > layout.preview.left &&
            layout.preview.bottom > layout.preview.top)
        {
            const PreviewPanelLayout previewLayout = GetPreviewPanelLayout(hWnd);
            const RECT previewViewport = previewLayout.viewport;
            const RECT originalArea = previewLayout.panels[0];
            const RECT recoloredArea = previewLayout.panels[1];
            const RECT originalAnimationArea = previewLayout.panels[2];
            const RECT recoloredAnimationArea = previewLayout.panels[3];
            auto drawPreview = [&](HBITMAP image, RECT area, const wchar_t* label)
            {
                HBRUSH panelBrush = CreateSolidBrush(UiCard);
                FillRect(hdc, &area, panelBrush);
                DeleteObject(panelBrush);
                HBRUSH panelBorder = CreateSolidBrush(UiBorder);
                FrameRect(hdc, &area, panelBorder);
                DeleteObject(panelBorder);
                RECT header = area;
                header.bottom = header.top + 34;
                HBRUSH headerBrush = CreateSolidBrush(RGB(248, 250, 252));
                FillRect(hdc, &header, headerBrush);
                DeleteObject(headerBrush);
                HPEN headerPen = CreatePen(PS_SOLID, 1, UiBorder);
                HGDIOBJ oldHeaderPen = SelectObject(hdc, headerPen);
                MoveToEx(hdc, header.left, header.bottom, nullptr);
                LineTo(hdc, header.right, header.bottom);
                SelectObject(hdc, oldHeaderPen);
                DeleteObject(headerPen);
                RECT marker{header.left + 12, header.top + 13, header.left + 20, header.top + 21};
                HBRUSH markerBrush = CreateSolidBrush(UiAccent);
                FillRect(hdc, &marker, markerBrush);
                DeleteObject(markerBrush);
                RECT labelRect{header.left + 28, header.top, header.right - 10, header.bottom};
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, UiText);
                HFONT previousFont = static_cast<HFONT>(SelectObject(hdc, uiBoldFont));
                DrawTextW(hdc, label, -1, &labelRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
                SelectObject(hdc, previousFont);
                area = GetPreviewImageArea(area);
                if (!image)
                    return;
                BITMAP bitmap{};
                GetObjectW(image, sizeof(bitmap), &bitmap);
                const int areaWidth = area.right - area.left, areaHeight = area.bottom - area.top;
                const double fitScale = min(static_cast<double>(areaWidth) / bitmap.bmWidth,
                                            static_cast<double>(areaHeight) / bitmap.bmHeight);
                const double scale = fitScale * previewZoomPercent / 100.0;
                const int width = max(1, static_cast<int>(bitmap.bmWidth * scale));
                const int height = max(1, static_cast<int>(bitmap.bmHeight * scale));
                SCROLLINFO horizontal{sizeof(horizontal), SIF_RANGE | SIF_PAGE};
                SCROLLINFO vertical{sizeof(vertical), SIF_RANGE | SIF_PAGE};
                GetScrollInfo(previewHorizontalScroll, SB_CTL, &horizontal);
                GetScrollInfo(previewVerticalScroll, SB_CTL, &vertical);
                const int maxScrollX = max(0, horizontal.nMax - static_cast<int>(horizontal.nPage) + 1);
                const int maxScrollY = max(0, vertical.nMax - static_cast<int>(vertical.nPage) + 1);
                const double positionX = maxScrollX ? static_cast<double>(previewScrollX) / maxScrollX : 0.5;
                const double positionY = maxScrollY ? static_cast<double>(previewScrollY) / maxScrollY : 0.5;
                const int overflowX = max(0, width - areaWidth), overflowY = max(0, height - areaHeight);
                const int x = overflowX ? area.left - static_cast<int>(overflowX * positionX)
                                        : area.left + (areaWidth - width) / 2;
                const int y = overflowY ? area.top - static_cast<int>(overflowY * positionY)
                                        : area.top + (areaHeight - height) / 2;
                HDC memoryDC = CreateCompatibleDC(hdc);
                HGDIOBJ oldBitmap = SelectObject(memoryDC, image);
                BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
                const int savedDc = SaveDC(hdc);
                IntersectClipRect(hdc, area.left, area.top, area.right, area.bottom);
                AlphaBlend(hdc, x, y, width, height, memoryDC, 0, 0, bitmap.bmWidth, bitmap.bmHeight, blend);
                RestoreDC(hdc, savedDc);
                SelectObject(memoryDC, oldBitmap);
                DeleteDC(memoryDC);
            };
            const int previewClip = SaveDC(hdc);
            IntersectClipRect(
                hdc, previewViewport.left, previewViewport.top, previewViewport.right, previewViewport.bottom);
            drawPreview(originalPreviewBitmap, originalArea, Tr("preview.original").c_str());
            drawPreview(previewBitmap, recoloredArea, Tr("preview.recolored").c_str());
            drawPreview(originalAnimationBitmap, originalAnimationArea, Tr("preview.original_animation").c_str());
            drawPreview(recoloredAnimationBitmap, recoloredAnimationArea, Tr("preview.recolored_animation").c_str());
            RestoreDC(hdc, previewClip);
        }
        else if (!previewMessage.empty())
        {
            RECT textRect = layout.preview;
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
            DrawTextW(
                hdc, previewMessage.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

        SelectObject(hdc, paintFont);
        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
    {
        KillTimer(hWnd, AnimationTimerId);
        ClearPreview();
        if (previewProviderModule)
        {
            FreeLibrary(previewProviderModule);
            previewProviderModule = nullptr;
        }
        if (uiFont)
        {
            DeleteObject(uiFont);
            uiFont = nullptr;
        }
        if (uiBoldFont)
        {
            DeleteObject(uiBoldFont);
            uiBoldFont = nullptr;
        }
        if (uiTitleFont)
        {
            DeleteObject(uiTitleFont);
            uiTitleFont = nullptr;
        }
        SaveSettings();
        PostQuitMessage(0);
    }
    break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
