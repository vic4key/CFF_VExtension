#pragma once

#include <windows.h>
#include "types.h"

class VExtensionPage
{
public:
  VExtensionPage(json& prefs);
  virtual ~VExtensionPage();

  virtual LRESULT DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
  json& m_prefs;
  HWND  m_option_resolve_ordinal;
  HWND  m_option_undecorate_symbol;
  HWND  m_option_shorten_undecorated_symbol;
  HWND  m_option_auto_fit_width_column_name;
};
