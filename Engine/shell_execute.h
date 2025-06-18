typedef void* HINSTANCE; // Actually it's a handle, so void* is fine for our purpose
typedef const char* LPCSTR;
typedef unsigned int UINT;

// Constants for ShellExecute
#define SW_SHOWNORMAL 1

#ifdef _WIN64
#define SHELLEXECUTEAPI __declspec(dllimport)
#else
#define SHELLEXECUTEAPI __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Declaration of ShellExecuteA (ANSI version)
HINSTANCE __stdcall ShellExecuteA(
    void* hwnd,
    LPCSTR lpOperation,
    LPCSTR lpFile,
    LPCSTR lpParameters,
    LPCSTR lpDirectory,
    int nShowCmd);

#ifdef __cplusplus
}
#endif