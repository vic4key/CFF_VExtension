// dllmain.cpp : Defines the entry point for the DLL application.

#include <vu>
#include <regex>
#include <fstream>
#include <psapi.h>
#include "resource.h"
#include "CFFExplorerSDK.h"
#include "VExtension.Page.h"

#define LOG(s) OutputDebugStringA("VExtension: " ## s)

#define CFF_API extern "C" __declspec(dllexport)

#define USE_DBGHELP TRUE

#ifdef USE_DBGHELP
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")
#endif // USE_DBGHELP

static json g_prefs;

/**
 * VExtension.asm
 */
extern "C" void get_rdx_register(QWORD& rdx);
extern "C" void set_rdx_register(QWORD& rdx);

// Global - Types & Constants & Variables

typedef void* CWnd;

CWnd* ImportExportDirectory_Below_Grid = NULL;

const UINT ImportExportDirectory_Above_Grid_ID = 1006;
const UINT ImportExportDirectory_Below_Grid_ID = 1007;

bool g_hooking_succeed = false;
static HINSTANCE hInstance = nullptr;
static LRESULT CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

enum INL_Hooking
{
  INL_conv_ansi_to_unicode,
  INL_grid_add_item,
  INL_CGridCtrl_SendMessageToParent,
  INL_CGridCtrl_OnSize,
  Count,
};

vu::INLHooking INLHooking[INL_Hooking::Count];

// UINT CWnd::GetSafeHwnd(CWnd* pWnd)

HWND CWnd_GetSafeHwnd(CWnd* pWnd)
{
  if (pWnd == nullptr)
  {
    return nullptr;
  }

  const ULONG_PTR Offset_MFC_CWnd_m_hWnd = 0x8;
  return HWND(*(QWORD*)(pWnd + Offset_MFC_CWnd_m_hWnd));
}

// UINT CWnd::GetDlgCtrlID(CWnd* pWnd)

UINT CWnd_GetDlgCtrlID(CWnd* pWnd)
{
  HWND hWnd = CWnd_GetSafeHwnd(pWnd);
  if (hWnd == nullptr)
  {
    return -1;
  }

  return GetDlgCtrlID(hWnd);
}

// void CGridCtrl::ExpandLastColumn()

typedef void (__fastcall *CGridCtrl_ExpandLastColumn_t)(CWnd* pWnd);
static CGridCtrl_ExpandLastColumn_t CGridCtrl_ExpandLastColumn = NULL;

void CGridCtrl_ExpandLastColumn_Ex(CWnd* pWnd)
{
  if (CGridCtrl_ExpandLastColumn == nullptr)
  {
    return;
  }

  auto hWnd = CWnd_GetSafeHwnd(pWnd);
  if (!IsWindow(hWnd))
  {
    return;
  }

  const auto ID = CWnd_GetDlgCtrlID(pWnd);
  if (ID != ImportExportDirectory_Below_Grid_ID)
  {
    return;
  }

  CGridCtrl_ExpandLastColumn(pWnd);
}

// void CGridCtrl::OnSize(UINT nType, int cx, int cy)

typedef void (__fastcall *CGridCtrl_OnSize_t)(CWnd* pWnd, unsigned int nType, unsigned int cx, unsigned int cy);
static CGridCtrl_OnSize_t CGridCtrl_OnSize_backup = NULL;
static CGridCtrl_OnSize_t CGridCtrl_OnSize = NULL;

void __fastcall CGridCtrl_OnSize_hook(CWnd* pWnd, unsigned int nType, unsigned int cx, unsigned int cy)
{
  CGridCtrl_OnSize_backup(pWnd, nType, cx, cy);

  const auto ID = CWnd_GetDlgCtrlID(pWnd);
  if (ID == ImportExportDirectory_Below_Grid_ID)
  {
    ImportExportDirectory_Below_Grid = pWnd;

    bool auto_fit_width_column_name = json_get_option(g_prefs, "auto_fit_width_column_name", true);
    if (auto_fit_width_column_name)
    {
      vu::Debouncer::instance().debounce(ImportExportDirectory_Below_Grid_ID, 100, [&]() // 100ms
      {
        CGridCtrl_ExpandLastColumn_Ex(ImportExportDirectory_Below_Grid);
      });
    }
  }
}

// CString __fastcall CGridCtrl::GetItemText(int nRow, int nCol)

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

typedef CString* (__fastcall *CGridCtrl_GetItemText_t)(CWnd* pWnd, CString str, int nRow, int nCol);
static CGridCtrl_GetItemText_t CGridCtrl_GetItemText_Ex = NULL;

std::wstring CGridCtrl_GetItemText(CWnd* pWnd, int nRow, int nCol)
{
  static CString s;
  auto result = CGridCtrl_GetItemText_Ex(pWnd, s, nRow, nCol);
  return result->c_str();
}

// LRESULT CGridCtrl::SendMessageToParent(int nRow, int nCol, int nMessage) const

typedef QWORD (__fastcall *CGridCtrl_SendMessageToParent_t)(CWnd* pWnd, int nRow, int nCol, int nMessage);
static CGridCtrl_SendMessageToParent_t CGridCtrl_SendMessageToParent_backup = NULL;
static vu::ulongptr CGridCtrl_SendMessageToParent = NULL;

std::unordered_map<DWORD, LPCSTR> g_ImportDirectory_Grid_IID_EOT_ENT; // mapping of Exported Ordinal Table and Exported Name Table

QWORD __fastcall CGridCtrl_SendMessageToParent_hook(CWnd* pWnd, int nRow, int nCol, int nMessage)
{
  const UINT GVN_SELCHANGING = 0xFFFFFF9C;
  if (nMessage == GVN_SELCHANGING && nRow >= 0 && nCol >= 0)
  {
    const auto ID = CWnd_GetDlgCtrlID(pWnd);

    if (ID == ImportExportDirectory_Below_Grid_ID)
    {
      ImportExportDirectory_Below_Grid = pWnd;
    }

    if (ID == ImportExportDirectory_Above_Grid_ID)
    {
      g_ImportDirectory_Grid_IID_EOT_ENT.clear();

      bool resolve_ordinal = json_get_option(g_prefs, "resolve_ordinal", true);
      if (resolve_ordinal)
      {
        auto module_name = CGridCtrl_GetItemText(pWnd, nRow, 0);
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
  }

  auto result = CGridCtrl_SendMessageToParent_backup(pWnd, nRow, nCol, nMessage);

  bool auto_fit_width_column_name = json_get_option(g_prefs, "auto_fit_width_column_name", true);
  if (auto_fit_width_column_name)
  {
    CGridCtrl_ExpandLastColumn_Ex(ImportExportDirectory_Below_Grid);
  }

  return result;
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
  bool undecorate_symbol = json_get_option(g_prefs, "undecorate_symbol", true);
  bool shorten_undecorated_symbol = json_get_option(g_prefs, "shorten_undecorated_symbol", true);

  std::string str = (char*)g_rdx;
  if (undecorate_symbol && vu::starts_with_A(str, "?"))
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

    if (shorten_undecorated_symbol)
    {
      str = update_symbol_name_for_friendly(str);
      memset(data, 0, sizeof(data));
      strcpy_s(data, str.c_str());
    }

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

// QWORD grid_add_item(QWORD rcx, QWORD rdx)

typedef QWORD(*grid_add_item_t)(QWORD rcx, QWORD rdx);
static grid_add_item_t grid_add_item_backup = NULL;
static vu::ulongptr grid_add_item = NULL;

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

bool find_addresses()
{
  MODULEINFO mi = { 0 };
  auto module = GetModuleHandle(nullptr);
  GetModuleInformation(GetCurrentProcess(), module, &mi, sizeof(mi));
  if (mi.lpBaseOfDll == 0 || mi.SizeOfImage == 0)
  {
    return false;
  }

  const auto base_address = vu::ulongptr(mi.lpBaseOfDll);

  // <cff_explorer>.`QWORD conv_ansi_to_unicode(QWORD rcx, QWORD rdx)`
  // pattern = "48 89 54 24 10 48 89 4C 24 08 48 83 EC ?? 48 83 7C 24 58 00 74 10 48 8B 4C 24 58 E8";
  // 00000001400186E0 | 48:895424 10    | mov qword ptr ss:[rsp+10],rdx
  // 00000001400186E5 | 48:894C24 08    | mov qword ptr ss:[rsp+8],rcx
  // 00000001400186EA | 48:83EC 48      | sub rsp,48
  // 00000001400186EE | 48:837C24 58 00 | cmp qword ptr ss:[rsp+58],0
  // 00000001400186F4 | 74 10           | je <cff explorer.loc_140018706>
  // 00000001400186F6 | 48:8B4C24 58    | mov rcx,qword ptr ss:[rsp+58]
  // 00000001400186FB | E8 70000000     | call <cff explorer.ATL::ChTraitsCRT<wchar_t>::GetBaseTypeLength(char const *)>
  conv_ansi_to_unicode = base_address + 0x186e0;

  // <cff_explorer>.`QWORD grid_add_item(QWORD rcx, QWORD rdx)`
  // pattern = "48 89 54 24 10 48 89 4C 24 08 48 83 EC ?? 48 8B 54 24 38 48 8B 4C 24 30 E8 C3 02 00 00 48 8B 44 24 30";
  // 00000001400057D0 | 48:895424 10 | mov qword ptr ss:[rsp+10],rdx
  // 00000001400057D5 | 48:894C24 08 | mov qword ptr ss:[rsp+8],rcx
  // 00000001400057DA | 48:83EC 28   | sub rsp,28
  // 00000001400057DE | 48:8B5424 38 | mov rdx,qword ptr ss:[rsp+38]
  // 00000001400057E3 | 48:8B4C24 30 | mov rcx,qword ptr ss:[rsp+30]
  // 00000001400057E8 | E8 C3020000  | call <ATL::CSimpleStringT<wchar_t,0>::operator=(wchar_t const *)>
  // 00000001400057ED | 48:8B4424 30 | mov rax,qword ptr ss:[rsp+30]
  grid_add_item = base_address + 0x57d0;

  // <cff_explorer>.`LRESULT CGridCtrl::SendMessageToParent(int nRow, int nCol, int nMessage)`
  // pattern = "44 89 4C 24 ?? 44 89 44 24 ?? 89 54 24 ?? 48 89 4C 24 ?? 48 83 EC ?? 48 8B 4C 24 60 48 8B 49 40";
  // 000000014014B1E0 | 44:894C24 20 | mov dword ptr ss:[rsp+20],r9d
  // 000000014014B1E5 | 44:894424 18 | mov dword ptr ss:[rsp+18],r8d
  // 000000014014B1EA | 895424 10    | mov dword ptr ss:[rsp+10],edx
  // 000000014014B1EE | 48:894C24 08 | mov qword ptr ss:[rsp+8],rcx
  // 000000014014B1F3 | 48:83EC 58   | sub rsp,58
  // 000000014014B1F7 | 48:8B4C24 60 | mov rcx,qword ptr ss:[rsp+60]
  // 000000014014B1FC | 48:8B49 40   | mov rcx,qword ptr ds:[rcx+40]
  CGridCtrl_SendMessageToParent = base_address + 0x14b1e0;

  // <cff_explorer>.`CString CGridCtrl::GetItemText(CString str, int nRow, int nCol)`
  // pattern = "44 89 4C 24 20 44 89 44 24 18 48 89 54 24 10 48 89 4C 24 08 48 83 EC ?? C7 44 24 28 00 00 00 00 83 7C 24 50 00 7C 29";
  // 0000000140160680 | 44:894C24 20       | mov dword ptr ss:[rsp+20],r9d
  // 0000000140160685 | 44:894424 18       | mov dword ptr ss:[rsp+18],r8d
  // 000000014016068A | 48:895424 10       | mov qword ptr ss:[rsp+10],rdx
  // 000000014016068F | 48:894C24 08       | mov qword ptr ss:[rsp+8],rcx
  // 0000000140160694 | 48:83EC 38         | sub rsp,38
  // 0000000140160698 | C74424 28 00000000 | mov dword ptr ss:[rsp+28],0
  // 00000001401606A0 | 837C24 50 00       | cmp dword ptr ss:[rsp+50],0
  // 00000001401606A5 | 7C 29              | jl <cff explorer.loc_1401606D0>
  CGridCtrl_GetItemText_Ex = CGridCtrl_GetItemText_t(base_address + 0x160680);

  // <cff_explorer>.`void CGridCtrl::ExpandLastColumn()`
  // pattern = "48 89 4C 24 ?? 48 83 EC ?? 48 8B 4C 24 ?? E8 ?? ?? ?? ?? 85 C0 7F 05 E9 ?? ?? ?? ?? 48"
  // 0000000140159060 | 48:894C24 08 | mov qword ptr ss:[rsp+8],rcx
  // 0000000140159065 | 48:83EC 58   | sub rsp,58
  // 0000000140159069 | 48:8B4C24 60 | mov rcx,qword ptr ss:[rsp+60]
  // 000000014015906E | E8 4DC4ECFF  | call <cff explorer.CGridCtrl_GetColumnCount>
  // 0000000140159073 | 85C0         | test eax,eax
  // 0000000140159075 | 7F 05        | jg <cff explorer.loc_14015907C>
  // 0000000140159077 | E9 DD000000  | jmp <cff explorer.loc_140159159>
  // 000000014015907C | 48:8B4C24 60 | mov rcx,qword ptr ss:[rsp+60]
  CGridCtrl_ExpandLastColumn = CGridCtrl_ExpandLastColumn_t(base_address + 0x159060);

  // <cff_explorer>.`void CGridCtrl::OnSize(UINT nType, int cx, int cy)`
  // 000000014014BA70 | 44:894C24 20     | mov dword ptr ss:[rsp+20],r9d
  // 000000014014BA75 | 44:894424 18     | mov dword ptr ss:[rsp+18],r8d
  // 000000014014BA7A | 895424 10        | mov dword ptr ss:[rsp+10],edx
  // 000000014014BA7E | 48:894C24 08     | mov qword ptr ss:[rsp+8],rcx
  // 000000014014BA83 | 48:83EC 28       | sub rsp,28
  // 000000014014BA87 | 833D 823F1500 00 | cmp dword ptr ds:[<bAlreadyInsideThisProcedure>],0
  CGridCtrl_OnSize = CGridCtrl_OnSize_t(base_address + 0x14BA70);

  return true;
}

// Extension's Exportation Callback Functions

std::string get_prefs_file_path()
{
  char tmp[KiB] = { 0 };
  GetModuleFileNameA(hInstance, tmp, sizeof(tmp));

  auto file_dir = vu::extract_file_directory_A(tmp);
  auto file_name = vu::extract_file_name_A(tmp, false) + ".json";

  vu::PathA prefs_file;
  prefs_file.join(file_dir).join(file_name).finalize();

  return prefs_file.as_string();
}

CFF_API BOOL __cdecl ExtensionLoad(EXTINITDATA* pExtInitData)
{
  CFF_Initialize(pExtInitData);

  try
  {
    auto prefs_file_path = get_prefs_file_path();
    std::ifstream fs(prefs_file_path);
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

  g_hooking_succeed = find_addresses();

  if (!g_hooking_succeed)
  {
    return false;
  }

  g_hooking_succeed &= INLHooking[INL_Hooking::INL_conv_ansi_to_unicode].attach(
    LPVOID(conv_ansi_to_unicode),
    LPVOID(conv_ansi_to_unicode_hook),
    (void**)&conv_ansi_to_unicode_backup);

  g_hooking_succeed &= INLHooking[INL_Hooking::INL_grid_add_item].attach(
    LPVOID(grid_add_item),
    LPVOID(grid_add_item_hook),
    (void**)&grid_add_item_backup);

  g_hooking_succeed &= INLHooking[INL_Hooking::INL_CGridCtrl_SendMessageToParent].attach(
    LPVOID(CGridCtrl_SendMessageToParent),
    LPVOID(CGridCtrl_SendMessageToParent_hook),
    (void**)&CGridCtrl_SendMessageToParent_backup);

  g_hooking_succeed &= INLHooking[INL_Hooking::INL_CGridCtrl_OnSize].attach(
    LPVOID(CGridCtrl_OnSize),
    LPVOID(CGridCtrl_OnSize_hook),
    (void**)&CGridCtrl_OnSize_backup);

  return g_hooking_succeed;
}

CFF_API VOID __cdecl ExtensionUnload()
{
  #ifdef USE_DBGHELP
  SymCleanup(GetCurrentProcess());
  #endif // USE_DBGHELP

  if (!g_hooking_succeed)
  {
    return;
  }

  INLHooking[INL_Hooking::INL_CGridCtrl_OnSize].detach(
    LPVOID(CGridCtrl_OnSize), (void**)&CGridCtrl_OnSize_backup);

  INLHooking[INL_Hooking::INL_CGridCtrl_SendMessageToParent].detach(
    LPVOID(CGridCtrl_SendMessageToParent), (void**)&CGridCtrl_SendMessageToParent_backup);

  INLHooking[INL_Hooking::INL_grid_add_item].detach(
    LPVOID(grid_add_item), (void**)&grid_add_item_backup);

  INLHooking[INL_Hooking::INL_conv_ansi_to_unicode].detach(
    LPVOID(conv_ansi_to_unicode), (void**)&conv_ansi_to_unicode_backup);
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
  static VExtensionPage Page(g_prefs);
  return Page.DlgProc(hWnd, uMsg, wParam, lParam);
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
