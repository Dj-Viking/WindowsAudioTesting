// WindowsAudioTesting.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "WindowsAudioTesting.h"
#include "mmdeviceapi.h"
#include <stdio.h>
#include "wtypes.h"
#include "Functiondiscoverykeys_devpkey.h"
#include "audioclient.h"
#include "Mmdeviceapi.h"

int print_log(const char *format, ...)
{
    static char s_printf_buf[1024];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(s_printf_buf, sizeof(s_printf_buf), format, args);
    va_end(args);
    OutputDebugStringA(s_printf_buf);
    return 0;
}

#define printf(format, ...) \
    print_log(format, __VA_ARGS__)

// release pointers of any/unknown interface
#define SAFE_RELEASE(punk) \
    if ((punk) != NULL)    \
    {                      \
        (punk)->Release(); \
        (punk) = NULL;     \
    }

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;                     // current instance
WCHAR szTitle[MAX_LOADSTRING];       // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING]; // the main window class name

// Forward declarations of functions included in this code module:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);

void RELEASE_WITH_MSG(
    const char *msg,
    IMMDeviceEnumerator *enumerator,
    IMMDeviceCollection *deviceCollection,
    IMMDevice *mmDevice,
    IPropertyStore *propertyStore,
    LPWSTR endpointId)
{
    OutputDebugStringA(msg);
    CoTaskMemFree(endpointId);
    SAFE_RELEASE(enumerator);
    SAFE_RELEASE(deviceCollection);
    SAFE_RELEASE(mmDevice);
    SAFE_RELEASE(propertyStore);
}

int APIENTRY wWinMain(HINSTANCE hInstance,
                      HINSTANCE hPrevInstance,
                      LPWSTR lpCmdLine,
                      int nCmdShow)
{
    (void)(hPrevInstance);
    (void)(lpCmdLine);

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_WINDOWSAUDIOTESTING));
    MSG msg;
    HRESULT hr;

    // start the audio device enumeration
    // Your COM dll requires you to be in Single - Threaded Apartment mode.
    // You need to call CoInitialize prior to using it.
    hr = CoInitialize(NULL);
    if (hr != S_OK)
    {
        OutputDebugStringA("Could not coInitialize");
        return 1;
    }

    // derive some IDs for the enumerator which needs permission to
    // make a new execution context
    const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
    const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);

    UINT count = 0;
    // get collection count
    LPWSTR endpointId = 0;
    IMMDevice *pDeviceEndpoint = 0;
    IPropertyStore *pProps = 0;
    IMMDeviceCollection *p_mmDeviceCollection = 0;
    IMMDeviceEnumerator *pEnumerator = 0;
    DWORD *pdwState = 0;

    hr = CoCreateInstance(
        CLSID_MMDeviceEnumerator, NULL,
        CLSCTX_ALL, IID_IMMDeviceEnumerator,
        // pointer to the device enumerator that we want to create
        (void **)&pEnumerator // IUnknown interface
    );
    if (hr != S_OK)
    {
        RELEASE_WITH_MSG("Could not CoCreateInstance", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
        goto ErrorExit;
    }

    hr = pEnumerator->EnumAudioEndpoints(
        eAll,                 // get all input and output devices [in]
        DEVICE_STATE_ACTIVE,  // get all devices whose state is active [in]
        &p_mmDeviceCollection // pass pointer to device collection struct [out]
    );
    if (hr != S_OK)
    {
        RELEASE_WITH_MSG("Could not enumerate audio endpoints", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
        goto ErrorExit;
    }

    // get count of amount of devices populated by the enumerator
    hr = p_mmDeviceCollection->GetCount(&count);
    if (hr != S_OK)
    {
        RELEASE_WITH_MSG("Could not get device endpoint count from the device collection struct", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
        goto ErrorExit;
    }

    // print all the device information
    for (ULONG i = 0; i < count; i++)
    {
        // get item by index
        hr = p_mmDeviceCollection->Item(i, &pDeviceEndpoint);
        if (hr != S_OK)
        {
            RELEASE_WITH_MSG("could not get device item from from collection", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
            goto ErrorExit;
        }

        hr = pDeviceEndpoint->GetId(&endpointId);
        if (hr != S_OK)
        {
            RELEASE_WITH_MSG(" could not get endpoint id from device endpoint", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
            goto ErrorExit;
        }

        hr = pDeviceEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        if (hr != S_OK)
        {
            RELEASE_WITH_MSG("could not open property store", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
            goto ErrorExit;
        }

        DWORD something = 0;
        DWORD *psomething = &something;
        hr = pDeviceEndpoint->GetState(psomething);
        if (hr != S_OK)
        {
            RELEASE_WITH_MSG("could not get state of device endpoint", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
        }

        PROPVARIANT variantName = {0};
        // initi container for prop value
        PropVariantInit(&variantName);

        // get endpoints friendly-name prop

        hr = pProps->GetValue(PKEY_Device_FriendlyName, &variantName);
        if (hr != S_OK)
        {
            RELEASE_WITH_MSG("could not get props from audio device friendlyname", pEnumerator, p_mmDeviceCollection, pDeviceEndpoint, pProps, endpointId);
            goto ErrorExit;
        }

        // get value succeeds even if PKEY_Device_FriendlyName is not found
        // so in this case check if variantName.vt is set to VT_EMPTY
        // to see the information if the name came out empty
        if (variantName.vt != VT_EMPTY /* 0 */)
        {
            printf("Found audio device endpoint info %d: \"%S\" (%S)\n", i, variantName.pwszVal, endpointId);
        }
        else
        {
            printf("variant name was empty");
        }

        // const IID IID_IAudioClient = __uuidof(IAudioCaptureClient);

        // what was activate for?
        /*hr = Activate(
            IID_IAudioClient,
            DWORD dwClsCtx,
            PROPVARIANT * pActivationParams,
            void **ppInterface);*/

        // done with this device, free up some stuff
        CoTaskMemFree(endpointId);
        endpointId = 0;

        PropVariantClear(&variantName);

        SAFE_RELEASE(pProps);
        SAFE_RELEASE(pDeviceEndpoint);
    }

    SAFE_RELEASE(pEnumerator);
    SAFE_RELEASE(p_mmDeviceCollection);

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_WINDOWSAUDIOTESTING, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;

ErrorExit:
    printf("[ERROR]: hresult was code %d", hr);
    return 1;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSAUDIOTESTING));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_WINDOWSAUDIOTESTING);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE: Processes messages for the main window.
//
//  WM_COMMAND  - process the application menu
//  WM_PAINT    - Paint the main window
//  WM_DESTROY  - post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // Parse the menu selections:
        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        // TODO: Add any drawing code that uses hdc here...
        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    (void)(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
