// AgentCfgEditor.cpp : implementation file
//

#include "stdafx.h"
#include "nxcon.h"
#include "AgentCfgEditor.h"
#include "ModifiedAgentCfgDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif


//
// Constants
//

#define MAX_LINE_SIZE      4096

#define COLOR_AUTO         RGB(0, 0, 0)
#define COLOR_COMMENT      RGB(80, 127, 80)
#define COLOR_SECTION      RGB(0, 0, 192)

#define PANE_MESSAGE       0
#define PANE_LINE          1
#define PANE_COLUMN        2
#define PANE_MODIFY        3


/////////////////////////////////////////////////////////////////////////////
// CAgentCfgEditor

IMPLEMENT_DYNCREATE(CAgentCfgEditor, CMDIChildWnd)

CAgentCfgEditor::CAgentCfgEditor()
{
   m_dwNodeId = 0;
   m_bParserActive = FALSE;
   m_pCtxMenu = NULL;
}

CAgentCfgEditor::CAgentCfgEditor(DWORD dwNodeId)
{
   m_dwNodeId = dwNodeId;
   m_pCtxMenu = NULL;
}

CAgentCfgEditor::~CAgentCfgEditor()
{
   if (m_pCtxMenu != NULL)
   {
      m_pCtxMenu->DestroyMenu();
      delete m_pCtxMenu;
   }
}


BEGIN_MESSAGE_MAP(CAgentCfgEditor, CMDIChildWnd)
	//{{AFX_MSG_MAP(CAgentCfgEditor)
	ON_WM_CREATE()
	ON_COMMAND(ID_VIEW_REFRESH, OnViewRefresh)
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_UPDATE_COMMAND_UI(ID_EDIT_UNDO, OnUpdateEditUndo)
	ON_UPDATE_COMMAND_UI(ID_EDIT_PASTE, OnUpdateEditPaste)
	ON_UPDATE_COMMAND_UI(ID_EDIT_CUT, OnUpdateEditCut)
	ON_UPDATE_COMMAND_UI(ID_EDIT_COPY, OnUpdateEditCopy)
	ON_COMMAND(ID_EDIT_UNDO, OnEditUndo)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_DELETE, OnEditDelete)
	ON_COMMAND(ID_EDIT_PASTE, OnEditPaste)
	ON_COMMAND(ID_EDIT_SELECT_ALL, OnEditSelectAll)
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_CONFIG_SAVE, OnConfigSave)
	ON_COMMAND(ID_CONFIG_SAVEANDAPPLY, OnConfigSaveandapply)
	ON_WM_CLOSE()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	//}}AFX_MSG_MAP
	ON_EN_CHANGE(ID_EDIT_CTRL, OnEditCtrlChange)
	ON_NOTIFY(EN_MSGFILTER, ID_EDIT_CTRL, OnEditCtrlMsgFilter)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CAgentCfgEditor message handlers

BOOL CAgentCfgEditor::PreCreateWindow(CREATESTRUCT& cs) 
{
   if (cs.lpszClass == NULL)
      cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, 
                                         NULL, 
                                         GetSysColorBrush(COLOR_WINDOW), 
                                         AfxGetApp()->LoadIcon(IDI_EDITOR));
	return CMDIChildWnd::PreCreateWindow(cs);
}


//
// WM_CREATE message handler
//

int CAgentCfgEditor::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
   RECT rect;
   CHARFORMAT cf;
   static int widths[4] = { 60, 120, 220, -1 };

	if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

   m_pCtxMenu = new CMenu;
   m_pCtxMenu->CreatePopupMenu();
   CopyMenuItems(m_pCtxMenu, theApp.GetContextMenu(16));
   m_pCtxMenu->AppendMenu(MF_SEPARATOR);
   CopyMenuItems(m_pCtxMenu, theApp.GetContextMenu(17));

   GetClientRect(&rect);

   // Create status bar
	m_wndStatusBar.Create(WS_CHILD | WS_VISIBLE, rect, this, IDC_STATUS_BAR);
   m_wndStatusBar.SetParts(4, widths);
   m_iStatusBarHeight = GetWindowSize(&m_wndStatusBar).cy;
   rect.bottom -= m_iStatusBarHeight;

   // Create edit control
   m_wndEdit.Create(WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOHSCROLL | 
                    ES_AUTOVSCROLL | ES_WANTRETURN | WS_HSCROLL | WS_VSCROLL,
                    rect, this, ID_EDIT_CTRL);

   cf.cbSize = sizeof(CHARFORMAT);
   cf.dwMask = CFM_FACE | CFM_BOLD | CFM_COLOR;
   cf.dwEffects = CFE_AUTOCOLOR;
   cf.bPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
   _tcscpy(cf.szFaceName, _T("Courier New"));
   m_wndEdit.SetDefaultCharFormat(cf);

   m_wndEdit.SetEventMask(m_wndEdit.GetEventMask() | ENM_CHANGE | ENM_KEYEVENTS | ENM_MOUSEEVENTS);

   m_dwTimer = SetTimer(1, 1000, NULL);

   PostMessage(WM_COMMAND, ID_VIEW_REFRESH, 0);
	
	return 0;
}


//
// WM_DESTROY message handler
//

void CAgentCfgEditor::OnDestroy() 
{
   KillTimer(m_dwTimer);
	CMDIChildWnd::OnDestroy();
}


//
// WM_COMMAND::ID_VIEW_REFRESH message handler
//

void CAgentCfgEditor::OnViewRefresh() 
{
   DWORD dwResult;
   TCHAR *pszConfig;

   if (m_wndEdit.GetModify())
   {
      if (MessageBox(_T("All changes made since last save will be discarded. Are you sure?"),
                     _T("Warning"), MB_YESNO | MB_ICONEXCLAMATION) != IDYES)
      {
         return;
      }
   }

   dwResult = DoRequestArg3(NXCGetAgentConfig, g_hSession, (void *)m_dwNodeId,
                            &pszConfig, _T("Requesting agent's configuration file..."));
   if (dwResult == RCC_SUCCESS)
   {
      OnStartParsing();
      m_wndEdit.SetWindowText(pszConfig);
      free(pszConfig);
      ParseFile();
      OnStopParsing();
      m_wndEdit.SetModify(FALSE);
      m_wndStatusBar.SetText(_T(""), PANE_MODIFY, 0);
      WriteStatusMsg(_T("Loaded successfully"));
   }
   else
   {
      m_wndEdit.SetWindowText(_T(""));
      theApp.ErrorBox(dwResult, _T("Error getting agent's configuration file: %s"));
      WriteStatusMsg(_T("Error loading configuration file"));
   }
}


//
// WM_SIZE message handler
//

void CAgentCfgEditor::OnSize(UINT nType, int cx, int cy) 
{
   RECT rect;
   int nShift;

	CMDIChildWnd::OnSize(nType, cx, cy);

   m_wndStatusBar.SetWindowPos(NULL, 0, 0, 0, 0, SWP_NOZORDER);
   m_wndEdit.SetWindowPos(NULL, 0, 0, cx, cy - m_iStatusBarHeight, SWP_NOZORDER);
   
   nShift = GetSystemMetrics(SM_CXVSCROLL);
   m_wndStatusBar.GetClientRect(&rect);
   int widths[4] = { rect.right - 170 - nShift, rect.right - 120 - nShift, 
                     rect.right - 70 - nShift, rect.right -  nShift };
   m_wndStatusBar.SetParts(4, widths);
}


//
// WM_SETFOCUS message handler
//

void CAgentCfgEditor::OnSetFocus(CWnd* pOldWnd) 
{
	CMDIChildWnd::OnSetFocus(pOldWnd);
   m_wndEdit.SetFocus();	
}


//
// UI command update handlers
//

void CAgentCfgEditor::OnUpdateEditUndo(CCmdUI* pCmdUI) 
{
   pCmdUI->Enable(m_wndEdit.CanUndo());
}

void CAgentCfgEditor::OnUpdateEditPaste(CCmdUI* pCmdUI) 
{
   pCmdUI->Enable(m_wndEdit.CanPaste(CF_TEXT));
}

void CAgentCfgEditor::OnUpdateEditCut(CCmdUI* pCmdUI) 
{
   pCmdUI->Enable((m_wndEdit.GetSelectionType() & (SEL_TEXT | SEL_MULTICHAR)) ? TRUE : FALSE);
}

void CAgentCfgEditor::OnUpdateEditCopy(CCmdUI* pCmdUI) 
{
   pCmdUI->Enable((m_wndEdit.GetSelectionType() & (SEL_TEXT | SEL_MULTICHAR)) ? TRUE : FALSE);
}


//
// WM_COMMAND::ID_EDIT_UNDO message handler
//

void CAgentCfgEditor::OnEditUndo() 
{
   m_wndEdit.Undo();
}


//
// WM_COMMAND::ID_EDIT_COPY message handler
//

void CAgentCfgEditor::OnEditCopy() 
{
   m_wndEdit.Copy();
}


//
// WM_COMMAND::ID_EDIT_CUT message handler
//

void CAgentCfgEditor::OnEditCut() 
{
   m_wndEdit.Cut();
}


//
// WM_COMMAND::ID_EDIT_DELETE message handler
//

void CAgentCfgEditor::OnEditDelete() 
{
   m_wndEdit.Clear();
}


//
// WM_COMMAND::ID_EDIT_PASTE message handler
//

void CAgentCfgEditor::OnEditPaste() 
{
   m_wndEdit.PasteSpecial(CF_TEXT);
}


//
// WM_COMMAND::ID_EDIT_SELECT_ALL message handler
//

void CAgentCfgEditor::OnEditSelectAll() 
{
   m_wndEdit.SetSel(0, -1);
}


//
// Highlight text
//

void CAgentCfgEditor::HighlightText(int nStartPos, int nEndPos, COLORREF rgbColor)
{
   CHARFORMAT cf;

   m_wndEdit.SetSel(nStartPos, nEndPos);
   cf.cbSize = sizeof(CHARFORMAT);
   cf.dwMask = CFM_COLOR;
   cf.dwEffects = (rgbColor == COLOR_AUTO) ? CFE_AUTOCOLOR : 0;
   cf.crTextColor = rgbColor;
   m_wndEdit.SetSelectionCharFormat(cf);
   m_wndEdit.SetSel(-1, -1);
}


//
// Parse entire file
//

void CAgentCfgEditor::ParseFile()
{
   int i, nLineCount;

   nLineCount = m_wndEdit.GetLineCount();
   for(i = 0; i < nLineCount; i++)
      ParseLine(i);
}


//
// Parse specific line
//

void CAgentCfgEditor::ParseLine(int nLine)
{
   int nCurrPos, nEndPos, nIndex, nElementStart = -1;
   COLORREF rgbElementColor;
   TCHAR szLine[MAX_LINE_SIZE];
   BOOL bFirstChar;

   m_wndEdit.GetLine(nLine, szLine, MAX_LINE_SIZE);
   nCurrPos = m_wndEdit.LineIndex(nLine);
   nEndPos = nCurrPos + m_wndEdit.LineLength(nCurrPos);

   // Reset line to default color
   HighlightText(nCurrPos, nEndPos, COLOR_AUTO);

   // Skip leading spaces and tabs
   for(nIndex = 0; 
       (nCurrPos < nEndPos) && 
       ((szLine[nIndex] == _T(' ')) || 
        (szLine[nIndex] == _T('\t'))); nIndex++, nCurrPos++);

   for(bFirstChar = TRUE; nCurrPos < nEndPos; nIndex++, nCurrPos++)
   {
      switch(szLine[nIndex])
      {
         case _T('#'):
            if (nElementStart != -1)
               HighlightText(nElementStart, nCurrPos - 1, rgbElementColor);
            HighlightText(nCurrPos, nEndPos, COLOR_COMMENT);
            nCurrPos = nEndPos;
            nElementStart = -1;
            break;
         case _T('*'):
            if (bFirstChar)
            {
               nElementStart = nCurrPos;
               rgbElementColor = COLOR_SECTION;
            }
            break;
         default:
            break;
      }
      bFirstChar = FALSE;
   }

   if (nElementStart != -1)
      HighlightText(nElementStart, nEndPos, rgbElementColor);
}


//
// Handle edit control changes
//

void CAgentCfgEditor::OnEditCtrlChange()
{
   int nCurrLine;

   if (!m_bParserActive)
   {
      OnStartParsing();

      nCurrLine = m_wndEdit.LineFromChar(-1);
      if (nCurrLine > 0)
         ParseLine(nCurrLine - 1);
      ParseLine(nCurrLine);
      if (nCurrLine < m_wndEdit.GetLineCount() - 1)
         ParseLine(nCurrLine + 1);

      OnStopParsing();
      m_wndStatusBar.SetText(_T("Modified"), PANE_MODIFY, 0);
   }
}


//
// Handle start of parsing process
//

void CAgentCfgEditor::OnStartParsing()
{
   m_bParserActive = TRUE;
   m_wndEdit.SetEventMask(m_wndEdit.GetEventMask() & ~ENM_CHANGE);
   m_wndEdit.SetRedraw(FALSE);
   m_wndEdit.GetSel(m_crSavedSelection);
}


//
// Handle termination of parsing process
//

void CAgentCfgEditor::OnStopParsing()
{
   m_wndEdit.SetSel(m_crSavedSelection);
   m_wndEdit.SetRedraw(TRUE);
   m_wndEdit.SetEventMask(m_wndEdit.GetEventMask() | ENM_CHANGE);
   m_wndEdit.Invalidate(FALSE);
   m_bParserActive = FALSE;
}


//
// WM_CONTEXTMENU message handler
//

void CAgentCfgEditor::OnContextMenu(CWnd* pWnd, CPoint point) 
{
   m_pCtxMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this, NULL);
}


//
// WM_COMMAND::ID_CONFIG_SAVE message handler
//

void CAgentCfgEditor::OnConfigSave() 
{
   SaveConfig(FALSE);
}


//
// WM_COMMAND::ID_CONFIG_SAVEANDAPPLY message handler
//

void CAgentCfgEditor::OnConfigSaveandapply() 
{
   SaveConfig(TRUE);
}


//
// Save and apply agent's configuration file
//

BOOL CAgentCfgEditor::SaveConfig(BOOL bApply)
{
   DWORD dwResult, dwLength;
   TCHAR *pszText;
   BOOL bResult = FALSE;

   if (bApply)
   {
      if (MessageBox(_T("In order to apply new configuration agent will be restarted. Are you sure?"),
                     _T("Warning"), MB_YESNO | MB_ICONEXCLAMATION) != IDYES)
      {
         return FALSE;
      }
   }

   dwLength = m_wndEdit.GetWindowTextLength();
   pszText = (TCHAR *)malloc(sizeof(TCHAR) * (dwLength + 2));
   m_wndEdit.GetWindowText(pszText, dwLength + 1);
   dwResult = DoRequestArg4(NXCUpdateAgentConfig, g_hSession, (void *)m_dwNodeId,
                            pszText, (void *)bApply, _T("Updating agent's configuration file..."));
   if (dwResult == RCC_SUCCESS)
   {
      m_wndEdit.SetModify(FALSE);
      m_wndStatusBar.SetText(_T(""), PANE_MODIFY, 0);
      bResult = TRUE;
      WriteStatusMsg(_T("Saved successfully"));
   }
   else
   {
      theApp.ErrorBox(dwResult, _T("Error updating agent's configuration: %s"));
      WriteStatusMsg(_T("Error updating agent's configuration"));
   }
   return bResult;
}


//
// WM_CLOSE message handler
//

void CAgentCfgEditor::OnClose() 
{
   BOOL bAllowClose = TRUE;

   if (m_wndEdit.GetModify())
   {
      CModifiedAgentCfgDlg dlg;
      int iAction;

      iAction = dlg.DoModal();
      switch(iAction)
      {
         case IDC_SAVE:
            bAllowClose = SaveConfig(FALSE);
            break;
         case IDC_APPLY:
            bAllowClose = SaveConfig(TRUE);
            break;
         case IDCANCEL:
            bAllowClose = FALSE;
            break;
         default:
            break;
      }
   }
	
   if (bAllowClose)
	   CMDIChildWnd::OnClose();
}


//
// Write message to the status bar and set timer
//

void CAgentCfgEditor::WriteStatusMsg(TCHAR *pszMsg)
{
   m_wndStatusBar.SetText(pszMsg, PANE_MESSAGE, 0);
   m_iMsgTimer = 0;
}


//
// WM_TIMER message handler
//

void CAgentCfgEditor::OnTimer(UINT nIDEvent) 
{
   m_iMsgTimer++;
   if (m_iMsgTimer == 60)
      m_wndStatusBar.SetText(_T(""), PANE_MESSAGE, 0);
}


//
// Mouse and keyboard message filter for edit control
//

void CAgentCfgEditor::OnEditCtrlMsgFilter(NMHDR *pNotifyStruct, LRESULT *pResult)
{
   CHARRANGE cr;
   TCHAR szBuffer[32];
   int nLine;

   m_wndEdit.GetSel(cr);
   nLine = m_wndEdit.LineFromChar(cr.cpMin);

   _stprintf(szBuffer, _T("L: %d"), nLine + 1);
   m_wndStatusBar.SetText(szBuffer, PANE_LINE, 0);

   _stprintf(szBuffer, _T("C: %d"), cr.cpMin - m_wndEdit.LineIndex(nLine) + 1);
   m_wndStatusBar.SetText(szBuffer, PANE_COLUMN, 0);

   *pResult = 0;
}
