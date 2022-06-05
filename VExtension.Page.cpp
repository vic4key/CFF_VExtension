#include <vu>
#include <fstream>
#include <windowsx.h>

#include "VExtension.Page.h"
#include "types.h"
#include "resource.h"

extern std::string get_prefs_file_path();

VExtensionPage::VExtensionPage(json& prefs)
  : m_prefs(prefs)
  , m_option_resolve_ordinal(nullptr)
  , m_option_undecorate_symbol(nullptr)
  , m_option_shorten_undecorated_symbol(nullptr)
  , m_option_auto_expand_last_column(nullptr)
{
}

VExtensionPage::~VExtensionPage()
{
}

LRESULT VExtensionPage::DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
  case WM_INITDIALOG:
    {
      m_option_resolve_ordinal = GetDlgItem(hWnd, IDC_OPTION_RESOLVE_ORDINAL);
      m_option_undecorate_symbol = GetDlgItem(hWnd, IDC_OPTION_UNDECORATE_SYMBOL);
      m_option_shorten_undecorated_symbol = GetDlgItem(hWnd, IDC_OPTION_SHORTEN_UNDECORATED_SYMBOL);
      m_option_auto_expand_last_column = GetDlgItem(hWnd, IDC_OPTION_AUTO_EXPAND_LAST_COLUMN);

      bool resolve_ordinal = json_get_option(m_prefs, "resolve_ordinal", true);
      Button_SetCheck(m_option_resolve_ordinal, resolve_ordinal ? BST_CHECKED : BST_UNCHECKED);

      bool undecorate_symbol = json_get_option(m_prefs, "undecorate_symbol", true);
      Button_SetCheck(m_option_undecorate_symbol, undecorate_symbol ? BST_CHECKED : BST_UNCHECKED);

      bool shorten_undecorated_symbol = json_get_option(m_prefs, "shorten_undecorated_symbol", true);
      Button_SetCheck(m_option_shorten_undecorated_symbol, shorten_undecorated_symbol ? BST_CHECKED : BST_UNCHECKED);
      Button_Enable(m_option_shorten_undecorated_symbol, undecorate_symbol ? TRUE : FALSE);

      bool auto_expand_last_column = json_get_option(m_prefs, "auto_expand_name_column_width", true);
      Button_SetCheck(m_option_auto_expand_last_column, auto_expand_last_column ? BST_CHECKED : BST_UNCHECKED);
    }
    break;

  case WM_COMMAND:
    {
      switch (LOWORD(wParam))
      {
        case IDC_OPTION_RESOLVE_ORDINAL:
        {
          bool resolve_ordinal = Button_GetCheck(m_option_resolve_ordinal) == BST_CHECKED;
          json_set_option(m_prefs, "resolve_ordinal", resolve_ordinal);
        }
        break;

        case IDC_OPTION_UNDECORATE_SYMBOL:
        {
          bool undecorate_symbol = Button_GetCheck(m_option_undecorate_symbol) == BST_CHECKED;
          json_set_option(m_prefs, "undecorate_symbol", undecorate_symbol);
          Button_Enable(m_option_shorten_undecorated_symbol, undecorate_symbol ? TRUE : FALSE);
        }
        break;

        case IDC_OPTION_SHORTEN_UNDECORATED_SYMBOL:
        {
          bool shorten_undecorated_symbol = Button_GetCheck(m_option_shorten_undecorated_symbol) == BST_CHECKED;
          json_set_option(m_prefs, "shorten_undecorated_symbol", shorten_undecorated_symbol);
        }
        break;

        case IDC_OPTION_AUTO_EXPAND_LAST_COLUMN:
        {
          bool auto_expand_last_column = Button_GetCheck(m_option_auto_expand_last_column) == BST_CHECKED;
          json_set_option(m_prefs, "auto_expand_name_column_width", auto_expand_last_column);
        }
        break;

        case IDC_OPTION_SAVE:
        {
          try
          {
            auto prefs_file_path = get_prefs_file_path();
            std::ofstream f;
            f.open(prefs_file_path);
            f << m_prefs.dump(2, ' ');
            f.close();
          }
          catch (...)
          {
            auto error = vu::get_last_error_A();
            vu::msg_box_A(hWnd, MB_OK | MB_ICONERROR, "VExtension", "Save failed (%s).", error.c_str());
          }
        }
        break;
      }
    }
    break;

  default:
    break;
  }

  return FALSE;
}
