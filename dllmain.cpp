// dllmain.cpp : Defines the entry point for the DLL application.

#include <vu>
#include "resource.h"
#include "CFFExplorerSDK.h"

#pragma pack(push, 8)
#include <psapi.h>
#pragma pack(pop)

#define USE_DBGHELP TRUE

#ifdef USE_DBGHELP
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif // USE_DBGHELP

#define CFF_API extern "C" __declspec(dllexport)

/**
 * VExtension.asm
 */
extern "C" void get_rdx_register(QWORD& rdx);
extern "C" void set_rdx_register(QWORD& rdx);

static HINSTANCE hInstance = nullptr;
static LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

vu::INLHooking INLHooking;
typedef QWORD(*conv_ansi_to_unicode_t)(QWORD rcx, QWORD rdx);
static conv_ansi_to_unicode_t conv_ansi_to_unicode_backup = NULL;
static vu::ulongptr conv_ansi_to_unicode = NULL;

static QWORD g_rdx = NULL;

static QWORD update_rdx_resgiter()
{
  std::string str = (char*)g_rdx;
  if (vu::starts_with_A(str, "?"))
  {
    static char data[KiB] = { 0 };
    memset(data, 0, sizeof(data));
    #ifdef USE_DBGHELP
    UnDecorateSymbolName(str.c_str(), data, sizeof(data), UNDNAME_COMPLETE);
    #else  // USE_DBGHELP
    str = vu::undecorate_cpp_symbol_A(str);
    strcpy_s(data, str.c_str());
    #endif // USE_DBGHELP
    g_rdx = QWORD(&data);
  }

  return g_rdx;
}

static QWORD conv_ansi_to_unicode_hook(QWORD rcx, QWORD rdx)
{
  get_rdx_register(g_rdx);
  rdx = update_rdx_resgiter();
  set_rdx_register(g_rdx);
  return conv_ansi_to_unicode_backup(rcx, rdx);
}

CFF_API BOOL __cdecl ExtensionLoad(EXTINITDATA* pExtInitData)
{
  CFF_Initialize(pExtInitData);

  #ifdef USE_DBGHELP
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
  SymInitialize(GetCurrentProcess(), NULL, TRUE);
  #endif // USE_DBGHELP

  MODULEINFO mi = { 0 };
  auto module = GetModuleHandle(nullptr);
  GetModuleInformation(GetCurrentProcess(), module, &mi, sizeof(mi));
  if (mi.lpBaseOfDll == 0 || mi.SizeOfImage == 0)
  {
    return FALSE;
  }

  // <cff_explorer.conv_ansi_to_unicode>
  // 48:895424 10             | mov qword ptr ss:[rsp+10],rdx
  // 48:894C24 08             | mov qword ptr ss:[rsp+8],rcx
  // 48:83EC 48               | sub rsp,48
  // 48:837C24 58 00          | cmp qword ptr ss:[rsp+58],0
  // 74 10                    | je cff_explorer.<...>
  // 48:8B4C24 58             | mov rcx,qword ptr ss:[rsp+58]
  // E8 ?? ?? ?? ??           | call cff_explorer.<...>
  const auto pattern = "48 89 54 24 10 48 89 4C 24 08 48 83 EC 48 48 83 7C 24 58 00 74 10 48 8B 4C 24 58 E8";
  auto offset = vu::find_pattern_A(mi.lpBaseOfDll, mi.SizeOfImage, pattern);
  if (!offset.first)
  {
    return FALSE;
  }

  conv_ansi_to_unicode = vu::ulongptr(mi.lpBaseOfDll) + offset.second;

  return INLHooking.attach(
    LPVOID(conv_ansi_to_unicode), LPVOID(conv_ansi_to_unicode_hook), (void**)&conv_ansi_to_unicode_backup);
}

CFF_API VOID __cdecl ExtensionUnload()
{
  INLHooking.detach(
    LPVOID(conv_ansi_to_unicode), (void**)&conv_ansi_to_unicode_backup);

  #ifdef USE_DBGHELP
  SymCleanup(GetCurrentProcess());
  #endif // USE_DBGHELP
}

CFF_API WCHAR* __cdecl ExtensionName()
{
  return (WCHAR*)L"VExtension";
}

CFF_API WCHAR* __cdecl ExtensionDescription()
{
  return (WCHAR*)L"VExtension for CFF Explorer";
}

CFF_API BOOL __cdecl ExtensionNeeded(VOID* pObject, UINT uSize)
{
  __try
  {
    auto pDOSHeader = PIMAGE_DOS_HEADER(pObject);
    if (pDOSHeader == nullptr)
    {
      return FALSE;
    }

    if (pDOSHeader->e_magic != IMAGE_DOS_SIGNATURE)
    {
      return FALSE;
    }

    auto pNTHeaders = PIMAGE_NT_HEADERS(pDOSHeader->e_lfanew + vu::ulongptr(pDOSHeader));
    if (pNTHeaders == nullptr)
    {
      return FALSE;
    }

    if (pNTHeaders->Signature != IMAGE_NT_SIGNATURE)
    {
      return FALSE;
    }

    if (pNTHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC &&
      pNTHeaders->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
    {
      return FALSE;
    }

    BOOL has_import_or_export = FALSE;
    IMAGE_DATA_DIRECTORY idd = { 0 };

    CFF.eaGetDataDirectory(pObject, uSize, IMAGE_DIRECTORY_ENTRY_EXPORT, &idd);
    has_import_or_export |= idd.VirtualAddress != NULL;

    CFF.eaGetDataDirectory(pObject, uSize, IMAGE_DIRECTORY_ENTRY_IMPORT, &idd);
    has_import_or_export |= idd.VirtualAddress != NULL;

    return has_import_or_export;
  }
  __except (EXCEPTION_EXECUTE_HANDLER)
  {
    // do nothing
  }

  return FALSE;
}

CFF_API VOID* __cdecl ExtensionExecute(LPARAM lParam)
{
  static EXTEVENTSDATA eed;
  eed.cbSize = sizeof(eed);

  eed.hInstance = hInstance;
  eed.DlgID = IDD_VEXTENSION;
  eed.DlgProc = DlgProc;

  return (VOID*)&eed;
}

LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_INITDIALOG:
    {
      // do nothing
    }
    break;

  case WM_COMMAND:
    {
      // do nothing
    }
    break;

  default:
    break;
  }

  return FALSE;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
  switch (ul_reason_for_call)
  {
  case DLL_PROCESS_ATTACH:
  case DLL_PROCESS_DETACH:
    hInstance = hModule;
    DisableThreadLibraryCalls(hModule);
    break;
  }

  return TRUE;
}
