#include "DEFRecolor.h"
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <commctrl.h>
#include <io.h>
#include <fcntl.h>
#include <vector>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <string>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#include <shellapi.h>

#pragma warning(push)
#pragma warning(disable: 26495) // Disable uninitialized variable warning
#include "json.hpp" // Include nlohmann/json
#pragma warning(pop)

using json = nlohmann::json;

#include <locale>
#include <codecvt>

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
HWND hStatusBar;
bool defLoaded = false; // Global flag to track DEF load state
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

// Forward declarations
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void                LoadDEFFile(HWND hWnd);
void                SaveDEFFile(HWND hWnd);

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


// Converts std::wstring to std::string
std::string WStringToString(const std::wstring& wstr)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(wstr);
}

void UpdateStatusBar(HWND hWnd, const wchar_t* message) {
    SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)message);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR lpCmdLine,
    _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);

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

                CleanupConsole(); // Ensure cleanup before exit
                ExitProcess(0);   // Terminate the process cleanly
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

        // Execute CLI commands
        if (!loadFile.empty())
        {
            std::wcout << L"Loading DEF file: " << loadFile << std::endl;
            // Implement DEF file loading logic here
        }

        if (!importFile.empty())
        {
            std::wcout << L"Importing palette from JSON: " << importFile << std::endl;
            // Implement JSON import logic here
        }

        if (!exportFile.empty())
        {
            std::wcout << L"Exporting palette to JSON: " << exportFile << std::endl;
            // Implement JSON export logic here
        }

        if (!saveFile.empty())
        {
            std::wcout << L"Saving DEF file: " << saveFile << std::endl;
            // Implement DEF file saving logic here
        }

        // Cleanup and exit
        CleanupConsole();
        ExitProcess(0); // Ensure immediate process termination
    }

    // GUI mode
    LocalFree(argv);
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_DEFRECOLOR, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_DEFRECOLOR));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}



ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
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
    int width = 750;
    int height = 450;

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
    GetModuleFileName(NULL, exePath, MAX_PATH);
    PathRemoveFileSpec(exePath); // Remove the executable name, leaving the directory
    return std::wstring(exePath);
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
    {
        defFilePath = szFile;
        std::ifstream file(defFilePath, std::ios::binary);

        if (file)
        {
            file.seekg(0, std::ios::end);
            auto size = static_cast<size_t>(file.tellg());
            file.seekg(0, std::ios::beg);

            buffer.resize(size);
            file.read(reinterpret_cast<char*>(buffer.data()), size);

            // Extract palette
            for (int i = 0; i < 256; ++i)
            {
                inputPalette[i] = RGB(
                    buffer[static_cast<size_t>(16) + static_cast<size_t>(i) * 3],
                    buffer[static_cast<size_t>(16) + static_cast<size_t>(i) * 3 + 1],
                    buffer[static_cast<size_t>(16) + static_cast<size_t>(i) * 3 + 2]
                );
                outputPalette[i] = inputPalette[i];
            }

            defLoaded = true; // Mark DEF file as loaded
            UpdateStatusBar(hWnd, L"DEF file loaded successfully!");
        }
    }
}

void SaveDEFFile(HWND hWnd)
{
    OPENFILENAME ofn = {};
    WCHAR szFile[260] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"DEF Files\0*.def\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetSaveFileName(&ofn))
    {
        defFilePath = szFile;

        // Update palette in buffer
        for (int i = 0; i < 256; ++i)
        {
            size_t baseIndex = static_cast<size_t>(16) + static_cast<size_t>(i) * 3;
            buffer[baseIndex] = static_cast<unsigned char>(GetRValue(outputPalette[i]));
            buffer[baseIndex + 1] = static_cast<unsigned char>(GetGValue(outputPalette[i]));
            buffer[baseIndex + 2] = static_cast<unsigned char>(GetBValue(outputPalette[i]));
        }

        std::ofstream file(defFilePath, std::ios::binary);
        file.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
        UpdateStatusBar(hWnd, L"DEF file saved successfully!");
    }
}

void ExportPaletteToJSON(HWND hWnd)
{
    // Save JSON file dialog
    OPENFILENAME ofn = {};
    WCHAR szFile[260] = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

    if (GetSaveFileName(&ofn))
    {
        std::wstring wideFilePath = szFile;
        std::string filePath = WStringToString(wideFilePath);

        // Check and append ".json" if missing
        if (filePath.find(".json") == std::string::npos)
        {
            filePath += ".json";
        }

        // Build JSON object
        json j;
        j["palette"] = json::array();
        for (const auto& color : outputPalette)
        {
            j["palette"].push_back({
                {"r", GetRValue(color)},
                {"g", GetGValue(color)},
                {"b", GetBValue(color)}
                });
        }

        // Write to file with custom formatting
        std::ofstream file(filePath);
        if (file.is_open())
        {
            file << "{\n";
            file << "  \"palette\": [\n";

            for (size_t i = 0; i < j["palette"].size(); ++i)
            {
                const auto& color = j["palette"][i];
                file << "    { \"r\": " << color["r"] << ", \"g\": " << color["g"] << ", \"b\": " << color["b"] << " }";
                if (i < j["palette"].size() - 1)
                {
                    file << ",";
                }
                file << "\n";
            }

            file << "  ]\n";
            file << "}\n";
            file.close();

            UpdateStatusBar(hWnd, L"Palette exported successfully!");
        }
        else
        {
            UpdateStatusBar(hWnd, L"Failed to save the file!");
        }
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
        std::wstring wideFilePath = szFile;
        std::string filePath = WStringToString(wideFilePath); // Convert to std::string

        // Read JSON file
        std::ifstream file(filePath);
        if (file.is_open())
        {
            json j;
            file >> j;

            if (j.contains("palette") && j["palette"].is_array() && j["palette"].size() == 256)
            {
                int i = 0;
                for (const auto& color : j["palette"])
                {
                    int r = color.value("r", 0);
                    int g = color.value("g", 0);
                    int b = color.value("b", 0);
                    outputPalette[i] = RGB(r, g, b);
                    ++i;
                }
                InvalidateRect(hWnd, NULL, TRUE); // Refresh display
                UpdateStatusBar(hWnd, L"Palette imported successfully!");
            }
            else
            {
                UpdateStatusBar(hWnd, L"Invalid JSON file or palette size!");
            }
        }
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static int selectedColorIndex = -1; // Keeps track of the clicked color index

    switch (message)
    {
    case WM_CREATE:
    {
        HMENU hMenu = CreateMenu();

        // File menu
        HMENU hSubMenuFile = CreatePopupMenu();
        AppendMenu(hSubMenuFile, MF_STRING, IDM_LOAD_DEF, L"Load DEF\tCtrl+L");
        AppendMenu(hSubMenuFile, MF_STRING, IDM_SAVE_DEF, L"Save DEF\tCtrl+S");
        AppendMenu(hSubMenuFile, MF_SEPARATOR, 0, NULL);
        AppendMenu(hSubMenuFile, MF_STRING, IDM_EXPORT_JSON, L"Export palette\tCtrl+E");
        AppendMenu(hSubMenuFile, MF_STRING, IDM_IMPORT_JSON, L"Import palette\tCtrl+I");
        AppendMenu(hSubMenuFile, MF_SEPARATOR, 0, NULL);
        AppendMenu(hSubMenuFile, MF_STRING, IDM_EXIT, L"Exit");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenuFile, L"File");

        // Help menu
        HMENU hSubMenuHelp = CreatePopupMenu();
        AppendMenu(hSubMenuHelp, MF_STRING, IDM_ABOUT, L"About");
        AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hSubMenuHelp, L"Help");

        SetMenu(hWnd, hMenu);

        // Create a status bar
        hStatusBar = CreateWindowEx(0, STATUSCLASSNAME, NULL,
            WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
            0, 0, 0, 0, hWnd, (HMENU)0, hInst, NULL);
        SendMessage(hStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");

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

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        int paletteWidth = (clientRect.right - 30) / 2; // Adjust for padding
        int paletteHeight = clientRect.bottom - 50;

        // Check Input Palette
        for (int i = 0; i < 256; ++i)
        {
            RECT rect = {
                10 + (i % 16) * (paletteWidth / 16),
                10 + (i / 16) * (paletteHeight / 16),
                10 + ((i % 16) + 1) * (paletteWidth / 16),
                10 + ((i / 16) + 1) * (paletteHeight / 16)
            };

            if (PtInRect(&rect, pt))
            {
                selectedColorIndex = i;

                // Open Color Dialog
                CHOOSECOLOR cc = {};
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hWnd;
                cc.lpCustColors = customColors; // Use the global custom colors
                cc.rgbResult = inputPalette[i];
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;

                if (ChooseColor(&cc))
                {
                    inputPalette[i] = cc.rgbResult;
                    InvalidateRect(hWnd, &rect, TRUE); // Redraw updated color
                }
                return 0;
            }
        }

        // Check Output Palette
        for (int i = 0; i < 256; ++i)
        {
            RECT rect = {
                20 + paletteWidth + (i % 16) * (paletteWidth / 16),
                10 + (i / 16) * (paletteHeight / 16),
                20 + paletteWidth + ((i % 16) + 1) * (paletteWidth / 16),
                10 + ((i / 16) + 1) * (paletteHeight / 16)
            };

            if (PtInRect(&rect, pt))
            {
                selectedColorIndex = i;

                // Open Color Dialog
                CHOOSECOLOR cc = {};
                cc.lStructSize = sizeof(cc);
                cc.hwndOwner = hWnd;
                cc.lpCustColors = customColors; // Use the global custom colors
                cc.rgbResult = outputPalette[i];
                cc.Flags = CC_FULLOPEN | CC_RGBINIT;

                if (ChooseColor(&cc))
                {
                    outputPalette[i] = cc.rgbResult;
                    InvalidateRect(hWnd, &rect, TRUE); // Redraw updated color
                }
                return 0;
            }
        }
    }
    break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
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
            MessageBox(hWnd, L"DEFRecolor\nVersion 1.0\nDeveloped in 2024", L"About", MB_OK | MB_ICONINFORMATION);
            break;
        }
    }
    break;

    case WM_SIZE:
    {
        int width = LOWORD(lParam);  // Window width
        int height = HIWORD(lParam); // Window height

        // Constants for layout
        const int padding = 10;          // Padding between elements
        const int labelHeight = 30;      // Height of the labels
        const int statusBarHeight = 20;  // Approximate height of the status bar

        // Available height for palettes and labels
        int availableHeight = height - statusBarHeight - 3 * padding - labelHeight;

        // Calculate square dimensions
        int squareWidth = (width - 3 * padding) / 2; // Two squares with spacing
        int squareHeight = availableHeight;         // Remaining vertical space
        int squareSize = min(squareWidth, squareHeight); // Ensure squares are proportional

        // Calculate positions
        int leftSquareX = padding;                        // Left square X
        int rightSquareX = leftSquareX + squareSize + padding; // Right square X
        int labelY = padding;                             // Label Y position
        int squareY = labelY + labelHeight + padding;     // Square Y position

        // Position the labels above the squares
        MoveWindow(GetDlgItem(hWnd, 1001), leftSquareX, labelY, squareSize, labelHeight, TRUE);
        MoveWindow(GetDlgItem(hWnd, 1002), rightSquareX, labelY, squareSize, labelHeight, TRUE);

        // Calculate and invalidate palette rectangles
        RECT leftSquare = { leftSquareX, squareY, leftSquareX + squareSize, squareY + squareSize };
        RECT rightSquare = { rightSquareX, squareY, rightSquareX + squareSize, squareY + squareSize };
        InvalidateRect(hWnd, &leftSquare, TRUE);
        InvalidateRect(hWnd, &rightSquare, TRUE);

        // Resize the status bar
        SendMessage(hStatusBar, WM_SIZE, 0, 0);
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT clientRect;
        GetClientRect(hWnd, &clientRect);

        int paletteWidth = (clientRect.right - 30) / 2;
        int paletteHeight = clientRect.bottom - 50;

        // Draw input palette
        for (int i = 0; i < 256; ++i)
        {
            COLORREF color = defLoaded ? inputPalette[i] : (inputPalette[i] == RGB(0, 0, 0) ? RGB(200, 200, 200) : inputPalette[i]);
            HBRUSH hBrush = CreateSolidBrush(color);
            RECT rect = {
                10 + (i % 16) * (paletteWidth / 16),
                10 + (i / 16) * (paletteHeight / 16),
                10 + ((i % 16) + 1) * (paletteWidth / 16),
                10 + ((i / 16) + 1) * (paletteHeight / 16)
            };
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }

        // Draw output palette
        for (int i = 0; i < 256; ++i)
        {
            COLORREF color = defLoaded ? outputPalette[i] : (outputPalette[i] == RGB(0, 0, 0) ? RGB(200, 200, 200) : outputPalette[i]);
            HBRUSH hBrush = CreateSolidBrush(color);
            RECT rect = {
                20 + paletteWidth + (i % 16) * (paletteWidth / 16),
                10 + (i / 16) * (paletteHeight / 16),
                20 + paletteWidth + ((i % 16) + 1) * (paletteWidth / 16),
                10 + ((i / 16) + 1) * (paletteHeight / 16)
            };
            FillRect(hdc, &rect, hBrush);
            DeleteObject(hBrush);
        }

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_DESTROY:
    {
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
