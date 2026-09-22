#include "DEFRecolor.h"
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <shobjidl_core.h>
#include <thumbcache.h>
#include <uxtheme.h>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <cwctype>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>
#include <cmath>
#include <list>
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Msimg32.lib")
#pragma comment(lib, "Uxtheme.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <shellapi.h>
#include <shlobj.h>

#pragma warning(push)
#pragma warning(disable: 26495) // Disable uninitialized variable warning
#include "json.hpp" // Include nlohmann/json
#pragma warning(pop)

using json = nlohmann::json;

#include <locale>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
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
HFONT uiFont = nullptr;
int previewZoomPercent = 100;
int previewScrollX = 0;
int previewScrollY = 0;
int lastSelectedColor = -1;
COLORREF magicTargetColor = RGB(64, 160, 255);
std::vector<COLORREF> previewPalette(256);
uint32_t animationFrame = 0;
bool animationPlaying = true;
bool magicPreviewActive = false;
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
COLORREF customColors[16] = {
    RGB(255, 255, 255), RGB(192, 192, 192), RGB(128, 128, 128), RGB(64, 64, 64),
    RGB(255, 0, 0), RGB(0, 255, 0), RGB(0, 0, 255), RGB(255, 255, 0),
    RGB(0, 255, 255), RGB(255, 0, 255), RGB(128, 0, 0), RGB(0, 128, 0),
    RGB(0, 0, 128), RGB(128, 128, 0), RGB(0, 128, 128), RGB(128, 0, 128)
};

// Palettes and DEF data
std::vector<COLORREF> inputPalette(256);
std::vector<COLORREF> outputPalette(256, RGB(0, 0, 0)); // Initialize to black
std::vector<unsigned char> buffer(256, 0); // Initialize buffer with size and default value
std::wstring defFilePath;
std::vector<std::wstring> recentFiles;
constexpr size_t MaxRecentFiles = 10;

// Forward declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void                LoadDEFFile(HWND hWnd);
void                SaveDEFFile(HWND hWnd);
std::wstring        GetExeDirectory();

// Original FILE pointers for cleanup
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


void UpdateStatusBar(HWND hWnd, const wchar_t* message) {
    UNREFERENCED_PARAMETER(hWnd);
    SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)message);
}

namespace
{
    constexpr size_t PaletteOffset = 16;
    constexpr size_t PaletteSize = 256 * 3;
    constexpr CLSID CLSID_DefThumbnailProvider =
        { 0x9b4f3e1c, 0x5d90, 0x4d62, { 0x8f, 0xa2, 0x57, 0xd7, 0xb9, 0xa1, 0x3c, 0x84 } };

    using TaskDialogIndirectFn = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
    using SetWindowSubclassFn = BOOL(WINAPI*)(HWND, SUBCLASSPROC, UINT_PTR, DWORD_PTR);
    using DefSubclassProcFn = LRESULT(WINAPI*)(HWND, UINT, WPARAM, LPARAM);
    HMODULE commonControlsModule = nullptr;
    TaskDialogIndirectFn taskDialogIndirectFn = nullptr;
    SetWindowSubclassFn setWindowSubclassFn = nullptr;
    DefSubclassProcFn defSubclassProcFn = nullptr;

    bool LoadCommonControlsApi()
    {
        if (!commonControlsModule)
            commonControlsModule = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!commonControlsModule) return false;

        const auto resolve = [](HMODULE module, const char* name, WORD ordinal) -> FARPROC {
            FARPROC procedure = GetProcAddress(module, name);
            return procedure ? procedure : GetProcAddress(module, MAKEINTRESOURCEA(ordinal));
        };
        if (!taskDialogIndirectFn)
            taskDialogIndirectFn = reinterpret_cast<TaskDialogIndirectFn>(resolve(commonControlsModule, "TaskDialogIndirect", 345));
        if (!setWindowSubclassFn)
            setWindowSubclassFn = reinterpret_cast<SetWindowSubclassFn>(resolve(commonControlsModule, "SetWindowSubclass", 410));
        if (!defSubclassProcFn)
            defSubclassProcFn = reinterpret_cast<DefSubclassProcFn>(resolve(commonControlsModule, "DefSubclassProc", 413));
        return setWindowSubclassFn && defSubclassProcFn;
    }

    std::wstring Utf8ToWide(const std::string& value)
    {
        if (value.empty()) return {};
        const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
        std::wstring result(static_cast<size_t>(size), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
        return result;
    }

    IStream* CreateMemoryStream(const std::vector<unsigned char>& data)
    {
        if (data.empty()) return nullptr;
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, data.size());
        if (!memory) return nullptr;
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
            if (!english) return false;
            english >> merged;
            if (language != L"english")
            {
                std::ifstream localized(directory / (language + L".json"));
                if (!localized) return false;
                json overrides; localized >> overrides;
                merged.merge_patch(overrides);
            }
            translations = std::move(merged);
            currentLanguage = language;
            return true;
        }
        catch (...) { return false; }
    }

    void DiscoverLanguages()
    {
        availableLanguages.clear();
        const auto directory = std::filesystem::path(GetExeDirectory()) / L"languages";
        std::error_code error;
        if (!std::filesystem::is_directory(directory, error)) return;

        for (const auto& entry : std::filesystem::directory_iterator(directory, error))
        {
            if (error || !entry.is_regular_file(error)) continue;
            std::wstring extension = entry.path().extension().wstring();
            std::transform(extension.begin(), extension.end(), extension.begin(), towlower);
            if (extension != L".json") continue;

            try
            {
                std::ifstream stream(entry.path());
                json languageJson;
                if (!stream || !(stream >> languageJson) || !languageJson.is_object()) continue;

                const std::wstring id = entry.path().stem().wstring();
                std::wstring displayName = id;
                if (languageJson.contains("language.name") && languageJson["language.name"].is_string())
                    displayName = Utf8ToWide(languageJson["language.name"].get<std::string>());
                if (!id.empty() && !displayName.empty())
                    availableLanguages.push_back({ id, displayName });
            }
            catch (...) { /* Ignore malformed language packs without breaking startup. */ }
        }

        std::sort(availableLanguages.begin(), availableLanguages.end(), [](const auto& left, const auto& right) {
            if (left.id == L"english") return right.id != L"english";
            if (right.id == L"english") return false;
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
        if (marker != std::wstring::npos) text.replace(marker, 7, std::to_wstring(value));
        UpdateStatusBar(hWnd, text.c_str());
    }

    void AddTooltip(HWND owner, HWND control, const char* key)
    {
        if (!tooltipWindow || !control) return;
        tooltipTexts.push_back(Tr(key));
        TOOLINFOW info{}; info.cbSize = sizeof(info); info.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
        info.hwnd = owner; info.uId = reinterpret_cast<UINT_PTR>(control);
        info.lpszText = const_cast<wchar_t*>(tooltipTexts.back().c_str());
        SendMessageW(tooltipWindow, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));
    }

    void RebuildTooltips(HWND hWnd)
    {
        if (tooltipWindow) DestroyWindow(tooltipWindow);
        tooltipTexts.clear();
        tooltipWindow = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_ALWAYSTIP,
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, hWnd, nullptr, hInst, nullptr);
        AddTooltip(hWnd, similaritySlider, "tooltip.similarity");
        AddTooltip(hWnd, hueSlider, "tooltip.hsl"); AddTooltip(hWnd, saturationSlider, "tooltip.hsl"); AddTooltip(hWnd, lightnessSlider, "tooltip.hsl");
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
    }

    std::filesystem::path SettingsPath()
    {
        wchar_t appData[MAX_PATH]{};
        if (FAILED(SHGetFolderPathW(nullptr, CSIDL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, appData)))
            return std::filesystem::path(GetExeDirectory()) / L"settings.json";
        return std::filesystem::path(appData) / L"DEFRecolor" / L"settings.json";
    }

    void SaveSettings()
    {
        try
        {
            const auto path = SettingsPath();
            std::filesystem::create_directories(path.parent_path());
            json settings;
            settings["max_recent_files"] = MaxRecentFiles;
            settings["recent_files"] = json::array();
            for (const auto& file : recentFiles) settings["recent_files"].push_back(std::filesystem::path(file).u8string());
            std::ofstream output(path, std::ios::trunc);
            if (output) output << settings.dump(2) << '\n';
        }
        catch (...) {}
    }

    void LoadSettings()
    {
        recentFiles.clear();
        try
        {
            std::ifstream input(SettingsPath());
            json settings; if (!input) return; input >> settings;
            if (!settings.contains("recent_files") || !settings["recent_files"].is_array()) return;
            for (const auto& item : settings["recent_files"])
            {
                if (!item.is_string() || recentFiles.size() >= MaxRecentFiles) continue;
                const auto path = std::filesystem::u8path(item.get<std::string>()).wstring();
                if (!path.empty()) recentFiles.push_back(path);
            }
        }
        catch (...) { recentFiles.clear(); }
    }

    void AddRecentFile(const std::wstring& file)
    {
        const auto normalized = std::filesystem::path(file).lexically_normal().wstring();
        recentFiles.erase(std::remove_if(recentFiles.begin(), recentFiles.end(), [&](const std::wstring& existing) {
            return _wcsicmp(existing.c_str(), normalized.c_str()) == 0;
        }), recentFiles.end());
        recentFiles.insert(recentFiles.begin(), normalized);
        if (recentFiles.size() > MaxRecentFiles) recentFiles.resize(MaxRecentFiles);
        SaveSettings();
    }

    std::wstring EscapeMenuText(std::wstring text)
    {
        size_t position = 0;
        while ((position = text.find(L'&', position)) != std::wstring::npos) { text.insert(position, 1, L'&'); position += 2; }
        return text;
    }

    void BuildApplicationMenu(HWND hWnd)
    {
        DiscoverLanguages();
        HMENU menu = CreateMenu(), file = CreatePopupMenu(), edit = CreatePopupMenu(), recent = CreatePopupMenu(), language = CreatePopupMenu(), help = CreatePopupMenu();
        const auto shortcutLabel = [](const std::wstring& label, const wchar_t* shortcut) { return label + L"\t" + shortcut; };
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
        AppendMenuW(edit, MF_STRING | (undoHistory.empty() ? MF_GRAYED : 0), IDM_UNDO, shortcutLabel(Tr("menu.undo"), L"Ctrl+Z").c_str());
        AppendMenuW(edit, MF_STRING | (redoHistory.empty() ? MF_GRAYED : 0), IDM_REDO, shortcutLabel(Tr("menu.redo"), L"Ctrl+Y").c_str());
        for (size_t index = 0; index < availableLanguages.size(); ++index)
        {
            const auto& item = availableLanguages[index];
            AppendMenuW(language, MF_STRING | (currentLanguage == item.id ? MF_CHECKED : 0),
                IDM_LANGUAGE_FIRST + static_cast<UINT>(index), item.displayName.c_str());
        }
        AppendMenuW(help, MF_STRING, IDM_HELP_GUIDE, shortcutLabel(Tr("menu.guide"), L"F1").c_str());
        AppendMenuW(help, MF_STRING, IDM_ABOUT, Tr("menu.about").c_str());
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), Tr("menu.file").c_str());
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), Tr("menu.edit").c_str());
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(language), Tr("menu.language").c_str());
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), Tr("menu.help").c_str());
        HMENU old = GetMenu(hWnd); SetMenu(hWnd, menu); if (old) DestroyMenu(old); DrawMenuBar(hWnd);
    }

    HRESULT CALLBACK InformationDialogCallback(HWND, UINT notification, WPARAM, LPARAM value, LONG_PTR)
    {
        if (notification == TDN_HYPERLINK_CLICKED)
            ShellExecuteW(nullptr, L"open", reinterpret_cast<const wchar_t*>(value), nullptr, nullptr, SW_SHOWNORMAL);
        return S_OK;
    }

    void ShowInformationDialog(HWND owner, bool guide)
    {
        const std::wstring title = Tr(guide ? "help.title" : "about.title");
        const std::wstring heading = Tr(guide ? "help.heading" : "about.heading");
        const std::wstring content = Tr(guide ? "help.body" : "about.body");
        const std::wstring details = Tr(guide ? "help.details" : "about.details");
        const std::wstring footer = Tr(guide ? "help.footer" : "about.footer");
        const std::wstring completeContent = details.empty() ? content : content + L"\n\n" + details;
        TASKDIALOGCONFIG config{};
        config.cbSize = sizeof(config); config.hwndParent = owner; config.hInstance = hInst;
        config.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT;
        config.dwCommonButtons = TDCBF_OK_BUTTON;
        config.pszWindowTitle = title.c_str(); config.pszMainInstruction = heading.c_str();
        config.pszContent = completeContent.c_str();
        config.pszFooter = footer.c_str(); config.pszMainIcon = MAKEINTRESOURCEW(IDI_DEFRECOLOR);
        config.pfCallback = InformationDialogCallback;
        LoadCommonControlsApi();
        if (!taskDialogIndirectFn || FAILED(taskDialogIndirectFn(&config, nullptr, nullptr, nullptr)))
            MessageBoxW(owner, completeContent.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
    }

    void UpdateSelectionLabel();
    void PopulateFrameCombo(uint32_t group, uint32_t preferredOrdinal);

    void RelocalizeFrameSelectors()
    {
        if (!defLoaded || !groupCombo || editorFrames.empty()) return;
        const uint32_t selectedOrdinal = (std::min)(currentFrame, static_cast<uint32_t>(editorFrames.size() - 1));
        const uint32_t selectedGroup = editorFrames[selectedOrdinal].group;
        SendMessageW(groupCombo, CB_RESETCONTENT, 0, 0);
        std::vector<uint32_t> groups;
        int selectedItem = 0;
        for (const auto& info : editorFrames)
        {
            if (std::find(groups.begin(), groups.end(), info.group) != groups.end()) continue;
            groups.push_back(info.group);
            const std::wstring label = Tr("group") + L" " + std::to_wstring(info.group);
            const int item = static_cast<int>(SendMessageW(groupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
            SendMessageW(groupCombo, CB_SETITEMDATA, item, info.group);
            if (info.group == selectedGroup) selectedItem = item;
        }
        SendMessageW(groupCombo, CB_SETCURSEL, selectedItem, 0);
        PopulateFrameCombo(selectedGroup, selectedOrdinal);
    }

    void ApplyLanguage(HWND hWnd)
    {
        if (!defLoaded) previewMessage = Tr("preview.load");
        UpdateWindowTitle(hWnd);
        SetText(hWnd, IDC_GROUP_FRAME, "group.frame"); SetText(hWnd, IDC_GROUP_SELECTION, "group.selection");
        SetText(hWnd, IDC_GROUP_MAGIC, "group.magic"); SetText(hWnd, IDC_GROUP_RESET, "group.reset");
        SetText(hWnd, IDC_LABEL_ANIM_SPEED, "animation.speed"); SetText(hWnd, IDC_LABEL_PREVIEW_ZOOM, "preview.zoom");
        SetText(hWnd, IDC_ZOOM_RESET, "preview.zoom_reset"); SetText(hWnd, IDC_LABEL_FRAME, "frame");
        SetText(hWnd, IDC_LABEL_HINT, "selection.hint"); SetText(hWnd, IDC_LABEL_HUE, "hue");
        SetText(hWnd, IDC_LABEL_SATURATION, "saturation"); SetText(hWnd, IDC_LABEL_LIGHTNESS, "lightness");
        SetText(hWnd, IDC_SELECT_ALL, "select_all"); SetText(hWnd, IDC_CLEAR_SELECTION, "clear");
        SetText(hWnd, IDC_RESET_SELECTED, "reset_selected"); SetText(hWnd, IDC_RESET_ALL, "reset_all");
        SetText(hWnd, IDC_RESET_SLIDERS, "reset_sliders"); SetText(hWnd, IDC_PRESERVE_SHADES, "preserve");
        SetText(hWnd, IDC_LABEL_MAGIC, "magic.title"); SetText(hWnd, IDC_LABEL_SIMILARITY, "magic.similarity");
        SetText(hWnd, IDC_TARGET_COLOR, "magic.target"); SetText(hWnd, IDC_LABEL_STRENGTH, "magic.strength");
        SetText(hWnd, IDC_APPLY_MAGIC, "magic.apply"); SetText(hWnd, IDC_LABEL_MAGIC_HINT, "magic.hint");
        SetText(hWnd, IDC_LABEL_GROUP, "group");
        SetWindowTextW(animationToggle, Tr(animationPlaying ? "animation.pause" : "animation.play").c_str());
        RelocalizeFrameSelectors(); UpdateSelectionLabel(); BuildApplicationMenu(hWnd); RebuildTooltips(hWnd); InvalidateRect(hWnd, nullptr, TRUE);
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
        if (hStatusBar) GetWindowRect(hStatusBar, &status);

        constexpr int padding = 10;
        constexpr int toolbarHeight = 292;
        constexpr int gap = 10;
        const int statusHeight = hStatusBar ? status.bottom - status.top : 20;
        const int contentBottom = max(padding, client.bottom - statusHeight - padding);
        const int availableWidth = max(32, client.right - 3 * padding);
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
        layout.inputPalette = { padding, padding + toolbarHeight, padding + actualWidth, padding + toolbarHeight + actualHeight };
        layout.outputPalette = { 2 * padding + paletteWidth, padding + toolbarHeight, 2 * padding + paletteWidth + actualWidth, padding + toolbarHeight + actualHeight };
        layout.preview = { padding, padding + toolbarHeight + actualHeight + gap, client.right - padding, contentBottom };
        return layout;
    }

    void UpdatePreviewScrollbars(HWND hWnd)
    {
        if (!previewHorizontalScroll || !previewVerticalScroll) return;
        const EditorLayout layout = GetEditorLayout(hWnd);
        constexpr int scrollSize = 17;
        const bool visible = previewZoomPercent > 100;
        const int viewportWidth = max(1, layout.preview.right - layout.preview.left - (visible ? scrollSize : 0));
        const int viewportHeight = max(1, layout.preview.bottom - layout.preview.top - (visible ? scrollSize : 0));
        const int panelWidth = max(1, (viewportWidth - 24) / 4);
        const int imageHeight = max(1, viewportHeight - 24);
        int contentWidth = panelWidth;
        int contentHeight = imageHeight;
        for (HBITMAP image : { originalPreviewBitmap, previewBitmap, originalAnimationBitmap, recoloredAnimationBitmap })
        {
            if (!image) continue;
            BITMAP bitmap{}; GetObjectW(image, sizeof(bitmap), &bitmap);
            if (!bitmap.bmWidth || !bitmap.bmHeight) continue;
            const double fit = min(static_cast<double>(panelWidth) / bitmap.bmWidth,
                                   static_cast<double>(imageHeight) / bitmap.bmHeight);
            contentWidth = max(contentWidth, static_cast<int>(bitmap.bmWidth * fit * previewZoomPercent / 100.0));
            contentHeight = max(contentHeight, static_cast<int>(bitmap.bmHeight * fit * previewZoomPercent / 100.0));
        }

        SCROLLINFO horizontal{ sizeof(horizontal), SIF_RANGE | SIF_PAGE | SIF_POS };
        horizontal.nMin = 0; horizontal.nMax = contentWidth - 1; horizontal.nPage = panelWidth;
        previewScrollX = min(previewScrollX, max(0, contentWidth - panelWidth));
        horizontal.nPos = previewScrollX;
        SetScrollInfo(previewHorizontalScroll, SB_CTL, &horizontal, TRUE);

        SCROLLINFO vertical{ sizeof(vertical), SIF_RANGE | SIF_PAGE | SIF_POS };
        vertical.nMin = 0; vertical.nMax = contentHeight - 1; vertical.nPage = imageHeight;
        previewScrollY = min(previewScrollY, max(0, contentHeight - imageHeight));
        vertical.nPos = previewScrollY;
        SetScrollInfo(previewVerticalScroll, SB_CTL, &vertical, TRUE);
        MoveWindow(previewHorizontalScroll, layout.preview.left, layout.preview.bottom - scrollSize,
                   viewportWidth, scrollSize, FALSE);
        MoveWindow(previewVerticalScroll, layout.preview.right - scrollSize, layout.preview.top,
                   scrollSize, viewportHeight, FALSE);
        if ((IsWindowVisible(previewHorizontalScroll) != FALSE) != visible)
        {
            ShowWindow(previewHorizontalScroll, visible ? SW_SHOW : SW_HIDE);
            ShowWindow(previewVerticalScroll, visible ? SW_SHOW : SW_HIDE);
        }
    }

    void ScrollPreview(HWND hWnd, int bar, UINT request, int thumb)
    {
        HWND scrollbar = bar == SB_HORZ ? previewHorizontalScroll : previewVerticalScroll;
        if (!scrollbar) return;
        SCROLLINFO info{ sizeof(info), SIF_ALL };
        GetScrollInfo(scrollbar, SB_CTL, &info);
        int position = info.nPos;
        const int line = 32;
        switch (request)
        {
        case SB_LINEUP: position -= line; break;
        case SB_LINEDOWN: position += line; break;
        case SB_PAGEUP: position -= static_cast<int>(info.nPage); break;
        case SB_PAGEDOWN: position += static_cast<int>(info.nPage); break;
        case SB_THUMBPOSITION:
        case SB_THUMBTRACK: position = thumb; break;
        case SB_TOP: position = info.nMin; break;
        case SB_BOTTOM: position = info.nMax; break;
        default: return;
        }
        position = max(info.nMin, min(position, info.nMax - static_cast<int>(info.nPage) + 1));
        info.fMask = SIF_POS; info.nPos = position;
        SetScrollInfo(scrollbar, SB_CTL, &info, TRUE);
        if (bar == SB_HORZ) previewScrollX = position; else previewScrollY = position;
        const EditorLayout layout = GetEditorLayout(hWnd);
        InvalidateRect(hWnd, &layout.preview, TRUE);
    }

    LRESULT CALLBACK SliderSubclassProc(HWND slider, UINT message, WPARAM wParam, LPARAM lParam,
                                        UINT_PTR, DWORD_PTR)
    {
        if (message == WM_MOUSEWHEEL)
        {
            const int notches = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            if (!notches) return 0;
            int step = 1;
            const int id = GetDlgCtrlID(slider);
            if (id == IDC_ANIMATION_SPEED || id == IDC_PREVIEW_ZOOM) step = 5;
            const int minimum = static_cast<int>(SendMessageW(slider, TBM_GETRANGEMIN, 0, 0));
            const int maximum = static_cast<int>(SendMessageW(slider, TBM_GETRANGEMAX, 0, 0));
            const int current = static_cast<int>(SendMessageW(slider, TBM_GETPOS, 0, 0));
            const int position = max(minimum, min(maximum, current + notches * step));
            SendMessageW(slider, TBM_SETPOS, TRUE, position);
            SendMessageW(GetParent(slider), WM_HSCROLL, MAKEWPARAM(TB_THUMBPOSITION, position), reinterpret_cast<LPARAM>(slider));
            return 0;
        }
        return defSubclassProcFn ? defSubclassProcFn(slider, message, wParam, lParam)
                                 : DefWindowProcW(slider, message, wParam, lParam);
    }

    RECT PaletteCell(const RECT& palette, int cellWidth, int cellHeight, int index)
    {
        const int column = index % 16;
        const int row = index / 16;
        return { palette.left + column * cellWidth, palette.top + row * cellHeight,
                 palette.left + (column + 1) * cellWidth, palette.top + (row + 1) * cellHeight };
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
            if (high == r) h = (g - b) / delta + (g < b ? 6.0 : 0.0);
            else if (high == g) h = (b - r) / delta + 2.0;
            else h = (r - g) / delta + 4.0;
            h /= 6.0;
        }
        h = std::fmod(h + hueShift / 360.0 + 1.0, 1.0);
        s = max(0.0, min(1.0, s + saturationShift / 100.0));
        l = max(0.0, min(1.0, l + lightnessShift / 100.0));
        auto hue = [](double p, double q, double t) {
            if (t < 0) t += 1; if (t > 1) t -= 1;
            if (t < 1.0 / 6) return p + (q - p) * 6 * t;
            if (t < 0.5) return q;
            if (t < 2.0 / 3) return p + (q - p) * (2.0 / 3 - t) * 6;
            return p;
        };
        if (s == 0) r = g = b = l;
        else
        {
            const double q = l < 0.5 ? l * (1 + s) : l + s - l * s;
            const double p = 2 * l - q;
            r = hue(p, q, h + 1.0 / 3); g = hue(p, q, h); b = hue(p, q, h - 1.0 / 3);
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
            tr = min(255.0, tr * scale); tg = min(255.0, tg * scale); tb = min(255.0, tb * scale);
        }
        const double amount = strength / 100.0;
        const int r = static_cast<int>(GetRValue(source) * (1 - amount) + tr * amount + 0.5);
        const int g = static_cast<int>(GetGValue(source) * (1 - amount) + tg * amount + 0.5);
        const int b = static_cast<int>(GetBValue(source) * (1 - amount) + tb * amount + 0.5);
        return RGB(r, g, b);
    }

    void BuildPreviewPalette()
    {
        previewPalette = outputPalette;
        if (!magicPreviewActive) return;
        const int strength = magicStrengthSlider ? static_cast<int>(SendMessageW(magicStrengthSlider, TBM_GETPOS, 0, 0)) : 100;
        const bool preserve = !preserveShadesCheck || SendMessageW(preserveShadesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
        for (size_t i = 0; i < previewPalette.size(); ++i)
            if (selectedPaletteColors[i]) previewPalette[i] = MagicRecolor(outputPalette[i], magicTargetColor, strength, preserve);
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
        if (originalAnimationBitmap) { DeleteObject(originalAnimationBitmap); originalAnimationBitmap = nullptr; }
        if (recoloredAnimationBitmap) { DeleteObject(recoloredAnimationBitmap); recoloredAnimationBitmap = nullptr; }
    }

    void ResetAdjustmentSliders()
    {
        hslHistoryBase.clear();
        hslHistoryActive = false;
        if (hueSlider) SendMessageW(hueSlider, TBM_SETPOS, TRUE, 0);
        if (saturationSlider) SendMessageW(saturationSlider, TBM_SETPOS, TRUE, 0);
        if (lightnessSlider) SendMessageW(lightnessSlider, TBM_SETPOS, TRUE, 0);
        adjustmentBase = outputPalette;
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
        if (before == outputPalette) return;
        undoHistory.push_back(before);
        if (undoHistory.size() > MaxHistoryEntries) undoHistory.erase(undoHistory.begin());
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
        if (undoHistory.empty()) return false;
        redoHistory.push_back(outputPalette);
        outputPalette = std::move(undoHistory.back());
        undoHistory.pop_back();
        ResetAdjustmentSliders();
        magicPreviewActive = false;
        UpdateHistoryMenu(hWnd);
        return true;
    }

    bool RedoPaletteChange(HWND hWnd)
    {
        if (redoHistory.empty()) return false;
        undoHistory.push_back(outputPalette);
        outputPalette = std::move(redoHistory.back());
        redoHistory.pop_back();
        ResetAdjustmentSliders();
        magicPreviewActive = false;
        UpdateHistoryMenu(hWnd);
        return true;
    }

    void UpdateSelectionLabel()
    {
        const size_t selected = static_cast<size_t>(std::count(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), true));
        std::wstring text = Tr("selection.count");
        const auto marker = text.find(L"{count}");
        if (marker != std::wstring::npos) text.replace(marker, 7, std::to_wstring(selected));
        if (selectionLabel) SetWindowTextW(selectionLabel, text.c_str());
    }

    void ApplyUiFont(HWND hWnd)
    {
        if (!uiFont)
            uiFont = CreateFontW(-14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                 DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                 CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        EnumChildWindows(hWnd, [](HWND child, LPARAM font) -> BOOL {
            SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
            SetWindowTheme(child, L"Explorer", nullptr);
            return TRUE;
        }, reinterpret_cast<LPARAM>(uiFont));
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
        if (!previewProviderModule) return HRESULT_FROM_WIN32(GetLastError());

        using DllGetClassObjectFn = HRESULT(__stdcall*)(REFCLSID, REFIID, void**);
        const auto getClassObject = reinterpret_cast<DllGetClassObjectFn>(
            GetProcAddress(previewProviderModule, "DllGetClassObject"));
        if (!getClassObject) return HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND);

        IClassFactory* factory = nullptr;
        HRESULT result = getClassObject(CLSID_DefThumbnailProvider, IID_PPV_ARGS(&factory));
        if (SUCCEEDED(result)) result = factory->CreateInstance(nullptr, IID_PPV_ARGS(provider));
        if (factory) factory->Release();
        return result;
    }

    bool LoadEditorApi()
    {
        if (renderFrame && getFrameCount && getFrameInfo) return true;
        if (!previewProviderModule)
        {
            const auto dll = std::filesystem::path(GetExeDirectory()) / L"DefThumbnailProvider.dll";
            previewProviderModule = LoadLibraryW(dll.c_str());
        }
        if (!previewProviderModule) return false;
        getFrameCount = reinterpret_cast<GetFrameCountFn>(GetProcAddress(previewProviderModule, "DefEditorGetFrameCount"));
        getFrameInfo = reinterpret_cast<GetFrameInfoFn>(GetProcAddress(previewProviderModule, "DefEditorGetFrameInfo"));
        renderFrame = reinterpret_cast<RenderFrameFn>(GetProcAddress(previewProviderModule, "DefEditorRenderFrame"));
        return getFrameCount && getFrameInfo && renderFrame;
    }

    void PopulateFrameCombo(uint32_t group, uint32_t preferredOrdinal = UINT32_MAX)
    {
        if (!frameCombo) return;
        SendMessageW(frameCombo, CB_RESETCONTENT, 0, 0);
        int selectedItem = -1;
        for (uint32_t ordinal = 0; ordinal < editorFrames.size(); ++ordinal)
        {
            const auto& info = editorFrames[ordinal];
            if (info.group != group) continue;
            wchar_t label[128]{};
            swprintf_s(label, L"%ls %u  |  %ls  (%u x %u)", Tr("frame").c_str(), info.index,
                       info.name[0] ? info.name : L"unnamed", info.width, info.height);
            const int item = static_cast<int>(SendMessageW(frameCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label)));
            SendMessageW(frameCombo, CB_SETITEMDATA, item, ordinal);
            if (ordinal == preferredOrdinal) selectedItem = item;
        }
        if (selectedItem < 0 && SendMessageW(frameCombo, CB_GETCOUNT, 0, 0) > 0) selectedItem = 0;
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
        if (!frameCombo || !groupCombo) return;
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
            if (FAILED(getFrameInfo(buffer.data(), buffer.size(), ordinal, &info))) continue;
            editorFrames.push_back(info);
        }
        frameCount = static_cast<uint32_t>(editorFrames.size());
        std::vector<uint32_t> groups;
        for (const auto& info : editorFrames)
            if (std::find(groups.begin(), groups.end(), info.group) == groups.end()) groups.push_back(info.group);
        for (const uint32_t group : groups)
        {
            std::wstring label = Tr("group") + L" " + std::to_wstring(group);
            const int item = static_cast<int>(SendMessageW(groupCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str())));
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
            if (animationFrame >= frameCount) animationFrame = currentFrame;
            renderFrame(buffer.data(), buffer.size(), currentFrame, inputPalette.data(), 512, &originalPreviewBitmap);
            const HRESULT editorResult = renderFrame(buffer.data(), buffer.size(), currentFrame,
                                                     previewPalette.data(), 512, &previewBitmap);
            if (animationFrame == currentFrame)
            {
                originalAnimationBitmap = static_cast<HBITMAP>(CopyImage(originalPreviewBitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
                recoloredAnimationBitmap = static_cast<HBITMAP>(CopyImage(previewBitmap, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
            }
            else
            {
                renderFrame(buffer.data(), buffer.size(), animationFrame, inputPalette.data(), 384, &originalAnimationBitmap);
                renderFrame(buffer.data(), buffer.size(), animationFrame, previewPalette.data(), 384, &recoloredAnimationBitmap);
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
        if (SUCCEEDED(result)) result = provider->QueryInterface(IID_PPV_ARGS(&initializer));
        if (SUCCEEDED(result)) result = initializer->Initialize(stream, STGM_READ);

        WTS_ALPHATYPE alphaType = WTSAT_UNKNOWN;
        if (SUCCEEDED(result)) result = provider->GetThumbnail(512, &previewBitmap, &alphaType);

        if (initializer) initializer->Release();
        if (provider) provider->Release();
        if (stream) stream->Release();

        if (FAILED(result) || !previewBitmap)
        {
            ClearPreview();
            previewMessage = result == REGDB_E_CLASSNOTREG
                ? Tr("preview.outdated")
                : Tr("preview.decode_error");
        }
        else
            previewMessage.clear();

        UpdatePreviewScrollbars(hWnd);
        InvalidateRect(hWnd, nullptr, TRUE);
    }

    void UpdateAnimationPreview(HWND hWnd)
    {
        if (!renderFrame || !frameCount) return;
        if (originalAnimationBitmap) { DeleteObject(originalAnimationBitmap); originalAnimationBitmap = nullptr; }
        if (recoloredAnimationBitmap) { DeleteObject(recoloredAnimationBitmap); recoloredAnimationBitmap = nullptr; }
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
        if (!file) { error = L"Unable to open the DEF file."; return false; }
        const std::streampos end = file.tellg();
        if (end < 0 || static_cast<size_t>(end) < PaletteOffset + PaletteSize)
        { error = L"Invalid DEF file: the palette data is incomplete."; return false; }

        std::vector<unsigned char> loaded(static_cast<size_t>(end));
        file.seekg(0, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(loaded.data()), static_cast<std::streamsize>(loaded.size())))
        { error = L"Unable to read the complete DEF file."; return false; }

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
        { error = L"Load a valid DEF file before saving."; return false; }

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
        { error = L"Unable to write the DEF file."; return false; }
        buffer.swap(saved);
        defFilePath = path.wstring();
        return true;
    }

    bool ExportPalette(const std::filesystem::path& path, std::wstring& error)
    {
        std::ofstream file(path, std::ios::trunc);
        if (!file) { error = L"Unable to create the JSON file."; return false; }
        json j;
        j["palette"] = json::array();
        for (const COLORREF color : outputPalette)
            j["palette"].push_back({ {"r", GetRValue(color)}, {"g", GetGValue(color)}, {"b", GetBValue(color)} });
        file << j.dump(2) << '\n';
        if (!file) { error = L"Unable to write the JSON file."; return false; }
        return true;
    }

    bool ImportPalette(const std::filesystem::path& path, std::wstring& error)
    {
        try
        {
            std::ifstream file(path);
            if (!file) { error = L"Unable to open the JSON file."; return false; }
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
        if (!path.has_extension()) path.replace_extension(extension);
        return path;
    }
}

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

    if (argc > 1) // Command-line mode
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
                if (SUCCEEDED(comResult)) CoUninitialize();
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
            if (!LoadDEF(loadFile, error)) { std::wcerr << L"Error: " << error << std::endl; success = false; }
        }

        if (success && !importFile.empty())
        {
            std::wcout << L"Importing palette from JSON: " << importFile << std::endl;
            if (!ImportPalette(importFile, error)) { std::wcerr << L"Error: " << error << std::endl; success = false; }
        }

        if (success && !exportFile.empty())
        {
            const auto path = WithExtension(exportFile, L".json");
            std::wcout << L"Exporting palette to JSON: " << path.wstring() << std::endl;
            if (!ExportPalette(path, error)) { std::wcerr << L"Error: " << error << std::endl; success = false; }
        }

        if (success && !saveFile.empty())
        {
            const auto path = WithExtension(saveFile, L".def");
            std::wcout << L"Saving DEF file: " << path.wstring() << std::endl;
            if (!SaveDEF(path, error)) { std::wcerr << L"Error: " << error << std::endl; success = false; }
        }

        // Cleanup and exit
        LocalFree(argv);
        CleanupConsole();
        if (SUCCEEDED(comResult)) CoUninitialize();
        return success ? 0 : 1;
    }

    // GUI mode
    LocalFree(argv);
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DEFRECOLOR, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        if (SUCCEEDED(comResult)) CoUninitialize();
        return FALSE;
    }

    ACCEL acceleratorEntries[] = {
        { FCONTROL | FVIRTKEY, 'O', IDM_LOAD_DEF },
        { FCONTROL | FVIRTKEY, 'S', IDM_SAVE_DEF },
        { FCONTROL | FVIRTKEY, 'E', IDM_EXPORT_JSON },
        { FCONTROL | FVIRTKEY, 'I', IDM_IMPORT_JSON },
        { FCONTROL | FVIRTKEY, 'Z', IDM_UNDO },
        { FCONTROL | FVIRTKEY, 'Y', IDM_REDO },
        { FVIRTKEY, VK_F1, IDM_HELP_GUIDE }
    };
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

    if (hAccelTable) DestroyAcceleratorTable(hAccelTable);
    if (SUCCEEDED(comResult)) CoUninitialize();
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
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_DEFRECOLOR);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;

    // Define default size
    int width = 1280;
    int height = 960;

    // Calculate center position
    RECT desktop;
    GetWindowRect(GetDesktopWindow(), &desktop);
    int xPos = (desktop.right - width) / 2;
    int yPos = (desktop.bottom - height) / 2;

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        xPos, yPos, width, height, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}


std::wstring GetExeDirectory()
{
    WCHAR exePath[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, exePath, ARRAYSIZE(exePath));
    if (!length || length >= ARRAYSIZE(exePath)) return {};
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
        LoadCommonControlsApi();
        if (!LoadLanguage(L"english")) translations = json::object();
        LoadSettings();
        BuildApplicationMenu(hWnd);

        // Create a status bar
        hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hWnd, (HMENU)0, hInst, NULL);
        SendMessageW(hStatusBar, SB_SETTEXT, 0, reinterpret_cast<LPARAM>(Tr("status.ready").c_str()));
        previewHorizontalScroll = CreateWindowW(L"SCROLLBAR", nullptr, WS_CHILD | SBS_HORZ,
            0, 0, 0, 0, hWnd, nullptr, hInst, nullptr);
        previewVerticalScroll = CreateWindowW(L"SCROLLBAR", nullptr, WS_CHILD | SBS_VERT,
            0, 0, 0, 0, hWnd, nullptr, hInst, nullptr);

        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 4, 1240, 66, hWnd, reinterpret_cast<HMENU>(IDC_GROUP_FRAME), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 74, 815, 122, hWnd, reinterpret_cast<HMENU>(IDC_GROUP_SELECTION), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            835, 74, 415, 122, hWnd, reinterpret_cast<HMENU>(IDC_GROUP_RESET), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10, 200, 1240, 84, hWnd, reinterpret_cast<HMENU>(IDC_GROUP_MAGIC), hInst, nullptr);

        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            25, 30, 50, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_GROUP), hInst, nullptr);
        groupCombo = CreateWindowW(WC_COMBOBOXW, nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            78, 24, 135, 300, hWnd, reinterpret_cast<HMENU>(IDC_GROUP_COMBO), hInst, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
            228, 30, 50, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_FRAME), hInst, nullptr);
        frameCombo = CreateWindowW(WC_COMBOBOXW, nullptr,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            280, 24, 180, 300, hWnd, reinterpret_cast<HMENU>(IDC_FRAME_COMBO), hInst, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 25, 96, 780, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_HINT), hInst, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 25, 127, 55, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_HUE), hInst, nullptr);
        hueSlider = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            80, 116, 175, 38, hWnd, reinterpret_cast<HMENU>(IDC_HUE_SLIDER), hInst, nullptr);
        SendMessageW(hueSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-180, 180));
        SendMessageW(hueSlider, TBM_SETTICFREQ, 45, 0); SendMessageW(hueSlider, TBM_SETPAGESIZE, 0, 45);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 270, 127, 80, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_SATURATION), hInst, nullptr);
        saturationSlider = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            350, 116, 180, 38, hWnd, reinterpret_cast<HMENU>(IDC_SAT_SLIDER), hInst, nullptr);
        SendMessageW(saturationSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
        SendMessageW(saturationSlider, TBM_SETTICFREQ, 25, 0); SendMessageW(saturationSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 545, 127, 75, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_LIGHTNESS), hInst, nullptr);
        lightnessSlider = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            620, 116, 180, 38, hWnd, reinterpret_cast<HMENU>(IDC_LIGHT_SLIDER), hInst, nullptr);
        SendMessageW(lightnessSlider, TBM_SETRANGE, TRUE, MAKELPARAM(-100, 100));
        SendMessageW(lightnessSlider, TBM_SETTICFREQ, 25, 0); SendMessageW(lightnessSlider, TBM_SETPAGESIZE, 0, 25);
        selectionLabel = CreateWindowW(L"STATIC", L"0 colors selected", WS_CHILD | WS_VISIBLE,
            25, 163, 145, 24, hWnd, reinterpret_cast<HMENU>(IDC_SELECTION_LABEL), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Select all", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            175, 157, 105, 30, hWnd, reinterpret_cast<HMENU>(IDC_SELECT_ALL), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            285, 157, 125, 30, hWnd, reinterpret_cast<HMENU>(IDC_CLEAR_SELECTION), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Reset selected", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            850, 116, 125, 34, hWnd, reinterpret_cast<HMENU>(IDC_RESET_SELECTED), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Reset all", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            980, 116, 115, 34, hWnd, reinterpret_cast<HMENU>(IDC_RESET_ALL), hInst, nullptr);
        CreateWindowW(L"BUTTON", L"Reset sliders", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1100, 116, 135, 34, hWnd, reinterpret_cast<HMENU>(IDC_RESET_SLIDERS), hInst, nullptr);
        preserveShadesCheck = CreateWindowW(L"BUTTON", L"Preserve relative shades",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            430, 160, 360, 24, hWnd, reinterpret_cast<HMENU>(IDC_PRESERVE_SHADES), hInst, nullptr);
        SendMessageW(preserveShadesCheck, BM_SETCHECK, BST_CHECKED, 0);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 25, 228, 80, 24, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_SIMILARITY), hInst, nullptr);
        similaritySlider = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            105, 216, 220, 40, hWnd, reinterpret_cast<HMENU>(IDC_SIMILARITY_SLIDER), hInst, nullptr);
        SendMessageW(similaritySlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 765));
        SendMessageW(similaritySlider, TBM_SETPOS, TRUE, 50);
        SendMessageW(similaritySlider, TBM_SETTICFREQ, 50, 0); SendMessageW(similaritySlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"BUTTON", L"Target color...", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            345, 218, 125, 30, hWnd, reinterpret_cast<HMENU>(IDC_TARGET_COLOR), hInst, nullptr);
        targetSwatch = CreateWindowW(L"STATIC", nullptr, WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            475, 218, 34, 30, hWnd, reinterpret_cast<HMENU>(IDC_TARGET_SWATCH), hInst, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 530, 228, 65, 24, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_STRENGTH), hInst, nullptr);
        magicStrengthSlider = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            595, 216, 200, 40, hWnd, reinterpret_cast<HMENU>(IDC_MAGIC_STRENGTH), hInst, nullptr);
        SendMessageW(magicStrengthSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(magicStrengthSlider, TBM_SETPOS, TRUE, 100);
        SendMessageW(magicStrengthSlider, TBM_SETTICFREQ, 25, 0); SendMessageW(magicStrengthSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"BUTTON", L"Apply magic recolor", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            815, 218, 155, 30, hWnd, reinterpret_cast<HMENU>(IDC_APPLY_MAGIC), hInst, nullptr);
        animationToggle = CreateWindowW(L"BUTTON", L"Pause animation", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1095, 24, 140, 30, hWnd, reinterpret_cast<HMENU>(IDC_ANIMATION_TOGGLE), hInst, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 25, 258, 900, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_MAGIC_HINT), hInst, nullptr);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 480, 30, 110, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_ANIM_SPEED), hInst, nullptr);
        animationSpeedSlider = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            590, 18, 175, 40, hWnd, reinterpret_cast<HMENU>(IDC_ANIMATION_SPEED), hInst, nullptr);
        SendMessageW(animationSpeedSlider, TBM_SETRANGE, TRUE, MAKELPARAM(25, 200));
        SendMessageW(animationSpeedSlider, TBM_SETPOS, TRUE, 100);
        SendMessageW(animationSpeedSlider, TBM_SETTICFREQ, 25, 0); SendMessageW(animationSpeedSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 780, 30, 55, 20, hWnd, reinterpret_cast<HMENU>(IDC_LABEL_PREVIEW_ZOOM), hInst, nullptr);
        previewZoomSlider = CreateWindowW(TRACKBAR_CLASSW, nullptr, WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            835, 18, 175, 40, hWnd, reinterpret_cast<HMENU>(IDC_PREVIEW_ZOOM), hInst, nullptr);
        SendMessageW(previewZoomSlider, TBM_SETRANGE, TRUE, MAKELPARAM(25, 400));
        SendMessageW(previewZoomSlider, TBM_SETPOS, TRUE, 100);
        SendMessageW(previewZoomSlider, TBM_SETTICFREQ, 25, 0); SendMessageW(previewZoomSlider, TBM_SETPAGESIZE, 0, 25);
        CreateWindowW(L"BUTTON", L"100 %", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            1015, 24, 70, 30, hWnd, reinterpret_cast<HMENU>(IDC_ZOOM_RESET), hInst, nullptr);
        for (HWND slider : { hueSlider, saturationSlider, lightnessSlider, similaritySlider,
                             magicStrengthSlider, animationSpeedSlider, previewZoomSlider })
        {
            SendMessageW(slider, TBM_SETLINESIZE, 0, 1);
            if (setWindowSubclassFn) setWindowSubclassFn(slider, SliderSubclassProc, 1, 0);
        }
        ApplyUiFont(hWnd);
        ApplyLanguage(hWnd);
        UpdatePreviewScrollbars(hWnd);
        SetTimer(hWnd, AnimationTimerId, 140, nullptr);

        // Load custom colors
        std::wstring customColorsPath = GetExeDirectory() + L"\\custom_colors.dat";
        std::ifstream customColorFile(customColorsPath, std::ios::binary);
        if (customColorFile.is_open())
        {
            customColorFile.read(reinterpret_cast<char*>(customColors), sizeof(customColors));
            customColorFile.close();
        }

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

    case WM_LBUTTONDOWN:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const EditorLayout layout = GetEditorLayout(hWnd);

        // Check Output Palette
        for (int i = 0; i < 256; ++i)
        {
            RECT rect = PaletteCell(layout.outputPalette, layout.cellWidth, layout.cellHeight, i);

            if (PtInRect(&rect, pt))
            {
                if (!(GetKeyState(VK_CONTROL) & 0x8000))
                    std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
                selectedPaletteColors[i] = !selectedPaletteColors[i];
                lastSelectedColor = i;
                ResetAdjustmentSliders();
                UpdateSelectionLabel();
                UpdateValueStatus(hWnd, "status.selection_changed", static_cast<int>(std::count(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), true)));
                if (magicPreviewActive) UpdatePreview(hWnd); else InvalidateRect(hWnd, nullptr, FALSE);
                return 0;
            }
        }
    }
    break;

    case WM_LBUTTONDBLCLK:
    {
        POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        const EditorLayout layout = GetEditorLayout(hWnd);
        for (int i = 0; i < 256; ++i)
        {
            RECT rect = PaletteCell(layout.outputPalette, layout.cellWidth, layout.cellHeight, i);
            if (PtInRect(&rect, pt))
            {
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
            SCROLLINFO oldHorizontal{ sizeof(oldHorizontal), SIF_RANGE | SIF_PAGE | SIF_POS };
            SCROLLINFO oldVertical{ sizeof(oldVertical), SIF_RANGE | SIF_PAGE | SIF_POS };
            GetScrollInfo(previewHorizontalScroll, SB_CTL, &oldHorizontal);
            GetScrollInfo(previewVerticalScroll, SB_CTL, &oldVertical);
            const int oldHorizontalRange = max(0, oldHorizontal.nMax - static_cast<int>(oldHorizontal.nPage) + 1);
            const int oldVerticalRange = max(0, oldVertical.nMax - static_cast<int>(oldVertical.nPage) + 1);
            const double horizontalPosition = oldHorizontalRange ? static_cast<double>(previewScrollX) / oldHorizontalRange : 0.5;
            const double verticalPosition = oldVerticalRange ? static_cast<double>(previewScrollY) / oldVerticalRange : 0.5;
            previewZoomPercent = static_cast<int>(SendMessageW(previewZoomSlider, TBM_GETPOS, 0, 0));
            UpdatePreviewScrollbars(hWnd);
            SCROLLINFO newHorizontal{ sizeof(newHorizontal), SIF_RANGE | SIF_PAGE };
            SCROLLINFO newVertical{ sizeof(newVertical), SIF_RANGE | SIF_PAGE };
            GetScrollInfo(previewHorizontalScroll, SB_CTL, &newHorizontal);
            GetScrollInfo(previewVerticalScroll, SB_CTL, &newVertical);
            previewScrollX = static_cast<int>(horizontalPosition * max(0, newHorizontal.nMax - static_cast<int>(newHorizontal.nPage) + 1));
            previewScrollY = static_cast<int>(verticalPosition * max(0, newVertical.nMax - static_cast<int>(newVertical.nPage) + 1));
            UpdatePreviewScrollbars(hWnd);
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
        if (movedSlider == magicStrengthSlider || movedSlider == similaritySlider)
        {
            if (movedSlider == magicStrengthSlider && magicPreviewActive) UpdatePreview(hWnd);
            if (movedSlider == similaritySlider && lastSelectedColor >= 0)
            {
                const int tolerance = static_cast<int>(SendMessageW(similaritySlider, TBM_GETPOS, 0, 0));
                const COLORREF seed = outputPalette[static_cast<size_t>(lastSelectedColor)];
                std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
                for (size_t i = 8; i < outputPalette.size(); ++i)
                    selectedPaletteColors[i] = ColorDistance(seed, outputPalette[i]) <= tolerance;
                ResetAdjustmentSliders(); UpdateSelectionLabel();
                if (magicPreviewActive) UpdatePreview(hWnd); else InvalidateRect(hWnd, nullptr, FALSE);
            }
            if (LOWORD(wParam) == TB_ENDTRACK || LOWORD(wParam) == TB_THUMBPOSITION)
                UpdateStatusBar(hWnd, Tr(movedSlider == similaritySlider ? "status.similarity_changed" : "status.strength_changed").c_str());
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
                    if (selectedPaletteColors[i]) { sharedColor = AdjustHsl(adjustmentBase[i], hue, saturation, lightness); break; }
            for (size_t i = 0; i < outputPalette.size(); ++i)
                if (selectedPaletteColors[i])
                    outputPalette[i] = preserve ? AdjustHsl(adjustmentBase[i], hue, saturation, lightness) : sharedColor;
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
            do { next = (next + 1) % frameCount; } while (next != animationFrame && editorFrames[next].group != group);
            animationFrame = next;
            UpdateAnimationPreview(hWnd);
        }
        break;

    case WM_DRAWITEM:
        if (wParam == IDC_TARGET_SWATCH)
        {
            auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lParam);
            HBRUSH brush = CreateSolidBrush(magicTargetColor);
            FillRect(item->hDC, &item->rcItem, brush); DeleteObject(brush);
            FrameRect(item->hDC, &item->rcItem, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
            return TRUE;
        }
        break;

    case WM_CTLCOLORSTATIC:
    {
        HDC controlDc = reinterpret_cast<HDC>(wParam);
        SetBkMode(controlDc, TRANSPARENT);
        SetBkColor(controlDc, GetSysColor(COLOR_WINDOW));
        return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
    }

    case WM_CTLCOLORBTN:
        if (reinterpret_cast<HWND>(lParam) == preserveShadesCheck)
        {
            HDC controlDc = reinterpret_cast<HDC>(wParam);
            SetBkMode(controlDc, TRANSPARENT);
            SetBkColor(controlDc, GetSysColor(COLOR_WINDOW));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
        }
        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        if (wmId >= IDM_RECENT_FIRST && wmId <= IDM_RECENT_LAST)
        {
            const size_t index = static_cast<size_t>(wmId - IDM_RECENT_FIRST);
            if (index < recentFiles.size()) OpenDEFPath(hWnd, recentFiles[index]);
            return 0;
        }
        if (wmId >= IDM_LANGUAGE_FIRST && wmId <= IDM_LANGUAGE_LAST)
        {
            const size_t index = static_cast<size_t>(wmId - IDM_LANGUAGE_FIRST);
            if (index < availableLanguages.size() && LoadLanguage(availableLanguages[index].id))
                ApplyLanguage(hWnd);
            return 0;
        }
        switch (wmId)
        {
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
            recentFiles.clear(); SaveSettings(); BuildApplicationMenu(hWnd);
            UpdateStatusBar(hWnd, Tr("status.recent_cleared").c_str());
            break;
        case IDC_ANIMATION_TOGGLE:
            animationPlaying = !animationPlaying;
            SetWindowTextW(animationToggle, Tr(animationPlaying ? "animation.pause" : "animation.play").c_str());
            UpdateStatusBar(hWnd, Tr(animationPlaying ? "status.animation_playing" : "status.animation_paused").c_str());
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
                ResetAdjustmentSliders(); UpdateSelectionLabel();
                if (magicPreviewActive) UpdatePreview(hWnd); else InvalidateRect(hWnd, nullptr, FALSE);
            }
            else
                UpdateStatusBar(hWnd, Tr("status.select_color").c_str());
            break;
        case IDC_TARGET_COLOR:
        {
            CHOOSECOLOR cc{}; cc.lStructSize = sizeof(cc); cc.hwndOwner = hWnd;
            cc.lpCustColors = customColors; cc.rgbResult = magicTargetColor; cc.Flags = CC_FULLOPEN | CC_RGBINIT;
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
        case IDC_APPLY_MAGIC:
        {
            const auto before = outputPalette;
            const int strength = static_cast<int>(SendMessageW(magicStrengthSlider, TBM_GETPOS, 0, 0));
            const bool preserve = SendMessageW(preserveShadesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED;
            for (size_t i = 0; i < outputPalette.size(); ++i)
                if (selectedPaletteColors[i]) outputPalette[i] = MagicRecolor(outputPalette[i], magicTargetColor, strength, preserve);
            CommitPaletteChange(hWnd, before);
            magicPreviewActive = false;
            ResetAdjustmentSliders(); UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.magic_applied").c_str());
        }
            break;
        case IDC_SELECT_ALL:
            std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), true);
            for (size_t i = 0; i < 8; ++i) selectedPaletteColors[i] = false;
            ResetAdjustmentSliders(); UpdateSelectionLabel(); InvalidateRect(hWnd, nullptr, FALSE);
            UpdateStatusBar(hWnd, Tr("status.selected_all").c_str());
            break;
        case IDC_CLEAR_SELECTION:
            std::fill(std::begin(selectedPaletteColors), std::end(selectedPaletteColors), false);
            ResetAdjustmentSliders(); UpdateSelectionLabel(); InvalidateRect(hWnd, nullptr, FALSE);
            UpdateStatusBar(hWnd, Tr("status.selection_cleared").c_str());
            break;
        case IDC_RESET_SELECTED:
        {
            const auto before = outputPalette;
            for (size_t i = 0; i < outputPalette.size(); ++i)
                if (selectedPaletteColors[i]) outputPalette[i] = inputPalette[i];
            CommitPaletteChange(hWnd, before);
            ResetAdjustmentSliders(); UpdatePreview(hWnd);
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
            ResetAdjustmentSliders(); UpdatePreview(hWnd);
            UpdateSelectionLabel();
            UpdateStatusBar(hWnd, Tr("status.reset_all").c_str());
            break;
        }
        case IDC_RESET_SLIDERS:
        {
            const auto before = outputPalette;
            outputPalette = adjustmentBase;
            CommitPaletteChange(hWnd, before);
            ResetAdjustmentSliders(); UpdatePreview(hWnd);
            UpdateStatusBar(hWnd, Tr("status.reset_sliders").c_str());
            break;
        }
        case IDC_PRESERVE_SHADES:
            UpdateStatusBar(hWnd, Tr(SendMessageW(preserveShadesCheck, BM_GETCHECK, 0, 0) == BST_CHECKED
                ? "status.preserve_on" : "status.preserve_off").c_str());
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
        if (hStatusBar) SendMessage(hStatusBar, WM_SIZE, 0, 0);
        UpdatePreviewScrollbars(hWnd);
        InvalidateRect(hWnd, nullptr, TRUE);
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        const EditorLayout layout = GetEditorLayout(hWnd);

        // Draw input palette
        for (int i = 0; i < 256; ++i)
        {
            COLORREF color = defLoaded ? inputPalette[i] : (inputPalette[i] == RGB(0, 0, 0) ? RGB(200, 200, 200) : inputPalette[i]);
            HBRUSH hBrush = CreateSolidBrush(color);
            RECT rect = PaletteCell(layout.inputPalette, layout.cellWidth, layout.cellHeight, i);
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }

        // Draw output palette
        for (int i = 0; i < 256; ++i)
        {
            COLORREF color = defLoaded ? outputPalette[i] : (outputPalette[i] == RGB(0, 0, 0) ? RGB(200, 200, 200) : outputPalette[i]);
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
        }

        FrameRect(hdc, &layout.inputPalette, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
        FrameRect(hdc, &layout.outputPalette, static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));

        if ((originalPreviewBitmap || previewBitmap) && layout.preview.right > layout.preview.left && layout.preview.bottom > layout.preview.top)
        {
            RECT previewViewport = layout.preview;
            if (previewZoomPercent > 100) { previewViewport.right -= 17; previewViewport.bottom -= 17; }
            const int viewportWidth = previewViewport.right - previewViewport.left;
            // The four viewport panels never change size. Only their bitmap content is zoomed
            // and panned, so changing zoom cannot rearrange the editor UI.
            const int columnWidth = max(1, (viewportWidth - 24) / 4);
            const int columnGap = 8;
            auto previewColumn = [&](int column) {
                const int left = previewViewport.left + column * (columnWidth + columnGap);
                return RECT{ left, previewViewport.top, left + columnWidth, previewViewport.bottom };
            };
            RECT originalArea = previewColumn(0);
            RECT recoloredArea = previewColumn(1);
            RECT originalAnimationArea = previewColumn(2);
            RECT recoloredAnimationArea = previewColumn(3);
            auto drawPreview = [&](HBITMAP image, RECT area, const wchar_t* label) {
                RECT labelRect = area; labelRect.bottom = labelRect.top + 22;
                DrawTextW(hdc, label, -1, &labelRect, DT_CENTER | DT_SINGLELINE);
                area.top += 24;
                if (!image) return;
                BITMAP bitmap{}; GetObjectW(image, sizeof(bitmap), &bitmap);
                const int areaWidth = area.right - area.left, areaHeight = area.bottom - area.top;
                const double fitScale = min(static_cast<double>(areaWidth) / bitmap.bmWidth,
                                            static_cast<double>(areaHeight) / bitmap.bmHeight);
                const double scale = fitScale * previewZoomPercent / 100.0;
                const int width = max(1, static_cast<int>(bitmap.bmWidth * scale));
                const int height = max(1, static_cast<int>(bitmap.bmHeight * scale));
                SCROLLINFO horizontal{ sizeof(horizontal), SIF_RANGE | SIF_PAGE };
                SCROLLINFO vertical{ sizeof(vertical), SIF_RANGE | SIF_PAGE };
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
                BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
                const int savedDc = SaveDC(hdc);
                IntersectClipRect(hdc, area.left, area.top, area.right, area.bottom);
                AlphaBlend(hdc, x, y, width, height, memoryDC, 0, 0, bitmap.bmWidth, bitmap.bmHeight, blend);
                RestoreDC(hdc, savedDc);
                SelectObject(memoryDC, oldBitmap); DeleteDC(memoryDC);
            };
            const int previewClip = SaveDC(hdc);
            IntersectClipRect(hdc, previewViewport.left, previewViewport.top, previewViewport.right, previewViewport.bottom);
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
            DrawTextW(hdc, previewMessage.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        }

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
        std::wstring customColorsPath = GetExeDirectory() + L"\\custom_colors.dat";
        std::ofstream customColorFile(customColorsPath, std::ios::binary);
        if (customColorFile.is_open())
        {
            customColorFile.write(reinterpret_cast<char*>(customColors), sizeof(customColors));
            customColorFile.close();
        }
        PostQuitMessage(0);
    }
    break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
