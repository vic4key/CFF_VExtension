// dllmain.cpp : Defines the entry point for the DLL application.

#include <vu>
#include <regex>
#include <fstream>
#include "resource.h"
#include "CFFExplorerSDK.h"

#pragma pack(push, 8)
#include <psapi.h>
#pragma pack(pop)

#define CFF_API extern "C" __declspec(dllexport)

#define USE_DBGHELP TRUE

#ifdef USE_DBGHELP
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif // USE_DBGHELP

#include <json.hpp>
#include <fifo_map.hpp>

template<class K, class V, class dummy_compare, class A>
using unordered_fifo_map = nlohmann::fifo_map<K, V, nlohmann::fifo_map_compare<K>, A>;
using json = nlohmann::basic_json<unordered_fifo_map>;

template <typename value_t>
value_t json_get(const json& jobject, const std::string& name, const value_t def)
{
  return jobject.contains(name) ? jobject[name].get<value_t>() : def;
}

static json g_prefs;

/**
 * VExtension.asm
 */
extern "C" void get_rdx_register(QWORD& rdx);
extern "C" void set_rdx_register(QWORD& rdx);

static HINSTANCE hInstance = nullptr;
static LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

enum INL_Hooking
{
  INL_conv_ansi_to_unicode,
  INL_grid_add_item,
  INL_CGridCtrl_SendMessageToParent,
  Count,
};

vu::INLHooking INLHooking[INL_Hooking::Count];

// UINT CWnd::GetDlgCtrlID(QWORD* vtable)

UINT CWnd_GetDlgCtrlID(QWORD* vtable)
{
  const ULONG_PTR Offset_MFC_CWnd_m_hWnd = 0x8;
  HWND hWnd = HWND(*(QWORD*)(vtable + Offset_MFC_CWnd_m_hWnd));
  return GetDlgCtrlID(hWnd);
}

// CString CGridCtrl::GetItemText(int nRow, int nCol) const

struct CString
{
  union
  {
    WCHAR data[MAXBYTE] = { 0 };
    LPCWSTR ptr;
  };

  std::wstring c_str()
  {
    return ptr == NULL ? L"" : ptr;
  }
};

typedef CString* (__fastcall* CGridCtrl_GetItemText_t)(QWORD* vtable_CGridCtrl, CString str, int nRow, int nCol);
static CGridCtrl_GetItemText_t CGridCtrl_GetItemText_Ex = CGridCtrl_GetItemText_t(0x0000000140160680);

std::wstring CGridCtrl_GetItemText(QWORD* vtable_CGridCtrl, int nRow, int nCol)
{
  static CString s;
  auto result = CGridCtrl_GetItemText_Ex(vtable_CGridCtrl, s, nRow, nCol);
  return result->c_str();
}

// LRESULT CGridCtrl::SendMessageToParent(int nRow, int nCol, int nMessage) const

typedef QWORD (__fastcall *CGridCtrl_SendMessageToParent_t)(QWORD* vtable_CGridCtrl, int nRow, int nCol, int nMessage);
static CGridCtrl_SendMessageToParent_t CGridCtrl_SendMessageToParent_backup = NULL;
static vu::ulongptr CGridCtrl_SendMessageToParent = 0x000000014014B1E0;

std::unordered_map<DWORD, LPCSTR> g_ImportDirectory_Grid_IID_EOT_ENT; // mapping of Exported Ordinal Table and Exported Name Table

QWORD __fastcall CGridCtrl_SendMessageToParent_hook(QWORD* vtable_CGridCtrl, int nRow, int nCol, int nMessage)
{
  const UINT GVN_SELCHANGING = 0xFFFFFF9C;
  if (nMessage == GVN_SELCHANGING && nRow >= 0 && nCol >= 0)
  {
    // get the text of the selected cell
    // auto s = CGridCtrl_GetItemText(vtable_CGridCtrl, nRow, nCol);
    // OutputDebugStringW(s.c_str());

    auto ID = CWnd_GetDlgCtrlID(vtable_CGridCtrl);
    const UINT ImportDirectory_Grid_IID_ID = 1006;
    if (ID == ImportDirectory_Grid_IID_ID)
    {
      g_ImportDirectory_Grid_IID_EOT_ENT.clear();

      auto module_name = CGridCtrl_GetItemText(vtable_CGridCtrl, nRow, 0);
      if (!module_name.empty())
      {
        if (auto pBase = LoadLibraryExW(module_name.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES))
        {
          if (auto pDOSHeader = PIMAGE_DOS_HEADER(pBase))
          {
            if (auto pNTHeaders = PIMAGE_NT_HEADERS(pDOSHeader->e_lfanew + vu::ulongptr(pDOSHeader)))
            {
              auto IED = pNTHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
              if (IED.VirtualAddress != NULL && IED.Size != NULL)
              {
                auto pIED = PIMAGE_EXPORT_DIRECTORY(PBYTE(pBase) + IED.VirtualAddress);
                if (pIED != nullptr && pIED->AddressOfNames != NULL && pIED->AddressOfNameOrdinals != NULL)
                {
                  auto pENT = PDWORD(PBYTE(pBase) + pIED->AddressOfNames); // Exported Name Table
                  auto pEOT = PWORD(PBYTE(pBase) + pIED->AddressOfNameOrdinals); // Exported Ordinal Table
                  for (DWORD i = 0; i < pIED->NumberOfNames; i++)
                  {
                    const auto ordinal = pEOT[i] + pIED->Base;
                    const auto name = LPCSTR(PBYTE(pBase) + pENT[i]);
                    g_ImportDirectory_Grid_IID_EOT_ENT[ordinal] = name;
                  }
                }
              }
            }
          }
        }
      }
    }
  }

  return CGridCtrl_SendMessageToParent_backup(vtable_CGridCtrl, nRow, nCol, nMessage);
}

// QWORD conv_ansi_to_unicode(QWORD rcx, QWORD rdx)

typedef QWORD (*conv_ansi_to_unicode_t)(QWORD rcx, QWORD rdx);
static conv_ansi_to_unicode_t conv_ansi_to_unicode_backup = NULL;
static vu::ulongptr conv_ansi_to_unicode = NULL;

static QWORD g_rdx = NULL;

enum symbol_name_rule_type_t : int
{
  simple,
  regex,
};

struct symbol_name_rule_object_t
{
  int type; // symbol_name_rule_type_t
  std::string from;
  std::string to;
};

std::vector<symbol_name_rule_object_t> g_symbol_name_rules;

std::string update_symbol_name_for_friendly(const std::string& text)
{
  std::string result = text;

  for (auto& rule : g_symbol_name_rules)
  {
    if (rule.type == symbol_name_rule_type_t::simple)
    {
      result = vu::replace_string_A(result, rule.from, rule.to);
    }
    else if (rule.type == symbol_name_rule_type_t::regex)
    {
      result = vu::regex_replace_string_A(result, std::regex(rule.from), rule.to);
    }
  }

  return result;
}

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

    str.assign(data);
    str = update_symbol_name_for_friendly(str);
    memset(data, 0, sizeof(data));
    strcpy_s(data, str.c_str());

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

// grid_add_item_hook

typedef QWORD(*grid_add_item_t)(QWORD rcx, QWORD rdx);
static grid_add_item_t grid_add_item_backup = NULL;
static vu::ulongptr grid_add_item = 0x00000001400057D0;

static QWORD grid_add_item_hook(QWORD rcx, QWORD rdx)
{
  if (rdx != NULL && !g_ImportDirectory_Grid_IID_EOT_ENT.empty())
  {
    std::wstring str = (wchar_t*)rdx;
    if (vu::starts_with_W(str, L"Ordinal:") && !vu::ends_with_W(str, L")"))
    {
      DWORD ordinal = -1;
      swscanf_s(str.c_str(), L"Ordinal: %08X", &ordinal); // `Ordinal: XXXXXXXX`
      if (ordinal != -1)
      {
        auto it = g_ImportDirectory_Grid_IID_EOT_ENT.find(ordinal);
        if (it != g_ImportDirectory_Grid_IID_EOT_ENT.cend())
        {
          static wchar_t data[MAXBYTE] = { 0 };
          memset(data, 0, sizeof(data));

          str += L" (" + vu::to_string_W(it->second) + L")";
          wcscpy_s(data, str.c_str());

          rdx = QWORD(&data);
        }
      }
    }
  }

  return grid_add_item_backup(rcx, rdx);
}

// Extension's Exportation Callback Functions

CFF_API BOOL __cdecl ExtensionLoad(EXTINITDATA* pExtInitData)
{
  CFF_Initialize(pExtInitData);

  try
  {
    char tmp[KiB] = { 0 };
    GetModuleFileNameA(hInstance, tmp, sizeof(tmp));

    auto file_dir = vu::extract_file_directory_A(tmp);
    auto file_name = vu::extract_file_name_A(tmp, false) + ".json";

    vu::PathA prefs_file;
    prefs_file.join(file_dir).join(file_name).finalize();

    std::ifstream fs(prefs_file.as_string());
    g_prefs = json::parse(fs, nullptr, true, true);

    g_symbol_name_rules.clear();
    if (g_prefs.contains("symbol_name_rules"))
    {
      auto& jsymbol_name_rules = g_prefs["symbol_name_rules"];
      for (auto& e : jsymbol_name_rules)
      {
        symbol_name_rule_object_t symbol_name_rule;
        symbol_name_rule.type = json_get(e, "type", -1);
        symbol_name_rule.from = json_get(e, "from", std::string(""));
        symbol_name_rule.to   = json_get(e, "to", std::string(""));
        g_symbol_name_rules.push_back(std::move(symbol_name_rule));
      }
    }
  }
  catch (json::parse_error& e)
  {
    OutputDebugStringA(e.what());
  }
  catch (...)
  {
    OutputDebugStringA("Unknown Exception");
  }

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

  bool result = true;

  result &= INLHooking[INL_Hooking::INL_conv_ansi_to_unicode].attach(
    LPVOID(conv_ansi_to_unicode), LPVOID(conv_ansi_to_unicode_hook), (void**)&conv_ansi_to_unicode_backup);

  result &= INLHooking[INL_Hooking::INL_grid_add_item].attach(
    LPVOID(grid_add_item), LPVOID(grid_add_item_hook), (void**)&grid_add_item_backup);

  result &= INLHooking[INL_Hooking::INL_CGridCtrl_SendMessageToParent].attach(
    LPVOID(CGridCtrl_SendMessageToParent), LPVOID(CGridCtrl_SendMessageToParent_hook), (void**)&CGridCtrl_SendMessageToParent_backup);

  return result;
}

CFF_API VOID __cdecl ExtensionUnload()
{
  INLHooking[INL_Hooking::INL_conv_ansi_to_unicode].detach(
    LPVOID(conv_ansi_to_unicode), (void**)&conv_ansi_to_unicode_backup);

  INLHooking[INL_Hooking::INL_grid_add_item].detach(
    LPVOID(grid_add_item), (void**)&grid_add_item_backup);

  INLHooking[INL_Hooking::INL_CGridCtrl_SendMessageToParent].detach(
    LPVOID(CGridCtrl_SendMessageToParent), (void**)&CGridCtrl_SendMessageToParent_backup);

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
