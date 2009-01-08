// ScriptManager.cpp : implementation file
//

#include "stdafx.h"
#include "nxcon.h"
#include "ScriptManager.h"
#include "InputBox.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

#define MODE_LIST    0
#define MODE_TREE    1


/////////////////////////////////////////////////////////////////////////////
// CScriptManager

IMPLEMENT_DYNCREATE(CScriptManager, CMDIChildWnd)

CScriptManager::CScriptManager()
{
   m_nMode = -1;
}

CScriptManager::~CScriptManager()
{
}


BEGIN_MESSAGE_MAP(CScriptManager, CMDIChildWnd)
	//{{AFX_MSG_MAP(CScriptManager)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_SETFOCUS()
	ON_COMMAND(ID_VIEW_REFRESH, OnViewRefresh)
	ON_COMMAND(ID_SCRIPT_VIEWASLIST, OnScriptViewaslist)
	ON_UPDATE_COMMAND_UI(ID_SCRIPT_VIEWASLIST, OnUpdateScriptViewaslist)
	ON_COMMAND(ID_SCRIPT_VIEWASTREE, OnScriptViewastree)
	ON_UPDATE_COMMAND_UI(ID_SCRIPT_VIEWASTREE, OnUpdateScriptViewastree)
	ON_COMMAND(ID_SCRIPT_NEW, OnScriptNew)
	ON_WM_CLOSE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_SCRIPT_DELETE, OnScriptDelete)
	ON_UPDATE_COMMAND_UI(ID_SCRIPT_DELETE, OnUpdateScriptDelete)
	ON_COMMAND(ID_SCRIPT_RENAME, OnScriptRename)
	ON_UPDATE_COMMAND_UI(ID_SCRIPT_RENAME, OnUpdateScriptRename)
	//}}AFX_MSG_MAP
   ON_NOTIFY(TVN_SELCHANGING, AFX_IDW_PANE_FIRST, OnTreeViewSelChanging)
   ON_NOTIFY(TVN_SELCHANGED, AFX_IDW_PANE_FIRST, OnTreeViewSelChange)
   ON_NOTIFY(LVN_ITEMCHANGING, AFX_IDW_PANE_FIRST, OnListViewItemChanging)
   ON_NOTIFY(LVN_ITEMCHANGED, AFX_IDW_PANE_FIRST, OnListViewItemChange)
   ON_NOTIFY(NM_DBLCLK, AFX_IDW_PANE_FIRST, OnViewDblClk)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CScriptManager message handlers

BOOL CScriptManager::PreCreateWindow(CREATESTRUCT& cs) 
{
   if (cs.lpszClass == NULL)
      cs.lpszClass = AfxRegisterWndClass(CS_HREDRAW | CS_VREDRAW, 
                                         NULL, 
                                         GetSysColorBrush(COLOR_WINDOW), 
                                         AfxGetApp()->LoadIcon(IDI_SCRIPT_LIBRARY));
	return CMDIChildWnd::PreCreateWindow(cs);
}


//
// WM_CREATE message handler
//

int CScriptManager::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
   RECT rect = { 0, 0, 100, 100 };

	if (CMDIChildWnd::OnCreate(lpCreateStruct) == -1)
		return -1;
	
   theApp.OnViewCreate(VIEW_SCRIPT_MANAGER, this);

   // Create image list
   m_imageList.Create(16, 16, ILC_COLOR8 | ILC_MASK, 4, 4);
   m_imageList.Add(AfxGetApp()->LoadIcon(IDI_SCRIPT_LIBRARY));
   m_imageList.Add(AfxGetApp()->LoadIcon(IDI_CLOSED_FOLDER));
   m_imageList.Add(AfxGetApp()->LoadIcon(IDI_OPEN_FOLDER));
   m_imageList.Add(AfxGetApp()->LoadIcon(IDI_SCRIPT));

   // Create splitter
   m_wndSplitter.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE, IDC_SPLITTER);

   // Create tree view control
   m_wndTreeCtrl.Create(WS_CHILD | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
                        rect, &m_wndSplitter, m_wndSplitter.IdFromRowCol(0, 0));
   m_wndTreeCtrl.SetImageList(&m_imageList, TVSIL_NORMAL);

   // Create list control
   m_wndListCtrl.Create(WS_CHILD | LVS_REPORT | LVS_NOCOLUMNHEADER | LVS_SHAREIMAGELISTS | LVS_SHOWSELALWAYS | LVS_SINGLESEL | LVS_SORTASCENDING,
                        rect, &m_wndSplitter, m_wndSplitter.IdFromRowCol(0, 0));
   m_wndListCtrl.InsertColumn(0, _T("Script"));
   m_wndListCtrl.SetImageList(&m_imageList, LVSIL_SMALL);

   // Create script view
   m_wndScriptView.Create(NULL, NULL, WS_CHILD | WS_VISIBLE, rect,
                          &m_wndSplitter, m_wndSplitter.IdFromRowCol(0, 1));
	
   // Finish splitter setup
   m_wndSplitter.SetColumnInfo(0, 200, 0);
   m_wndSplitter.InitComplete();
   m_wndSplitter.RecalcLayout();

   SetMode(MODE_LIST);

   PostMessage(WM_COMMAND, ID_VIEW_REFRESH, 0);

	return 0;
}


//
// WM_DESTROY message handler
//

void CScriptManager::OnDestroy() 
{
   theApp.OnViewDestroy(VIEW_SCRIPT_MANAGER, this);
	CMDIChildWnd::OnDestroy();
}


//
// WM_SIZE message handler
//

void CScriptManager::OnSize(UINT nType, int cx, int cy) 
{
	CMDIChildWnd::OnSize(nType, cx, cy);
   m_wndSplitter.SetWindowPos(NULL, 0, 0, cx, cy, SWP_NOZORDER);
}


//
// WM_SETFOCUS message handler
//

void CScriptManager::OnSetFocus(CWnd* pOldWnd) 
{
	CMDIChildWnd::OnSetFocus(pOldWnd);
	
	// TODO: Add your message handler code here
	
}


//
// WM_COMMAND::ID_VIEW_REFRESH message handler
//

void CScriptManager::OnViewRefresh() 
{
   DWORD i, dwResult, dwNumScripts;
   NXC_SCRIPT_INFO *pList;

   m_wndListCtrl.DeleteAllItems();
   m_wndTreeCtrl.DeleteAllItems();
   m_hRootItem = m_wndTreeCtrl.InsertItem(_T("[root]"), 0, 0);

   dwResult = DoRequestArg3(NXCGetScriptList, g_hSession, &dwNumScripts,
                            &pList, _T("Loading list of scripts..."));
   if (dwResult == RCC_SUCCESS)
   {
      for(i = 0; i < dwNumScripts; i++)
      {
         InsertScript(pList[i].dwId, pList[i].szName, FALSE);
      }
      safe_free(pList);
   }
   else
   {
      theApp.ErrorBox(dwResult, _T("Cannot load list of scripts: %s"));
   }
}


//
// WM_NOTIFY::TVN_SELCHANGED message handler
//

void CScriptManager::OnTreeViewSelChange(NMHDR *lpnmt, LRESULT *pResult)
{
   DWORD dwId;
   CString strName;

   dwId = m_wndTreeCtrl.GetItemData(((LPNMTREEVIEW)lpnmt)->itemNew.hItem);
   if (dwId != 0)
   {
      BuildScriptName(((LPNMTREEVIEW)lpnmt)->itemNew.hItem, strName);
      m_wndScriptView.SetEditMode(dwId, (LPCTSTR)strName);
   }
   else
   {
      m_wndScriptView.SetListMode(m_wndTreeCtrl, ((LPNMTREEVIEW)lpnmt)->itemNew.hItem);
   }
   *pResult = 0;
}


//
// WM_NOTIFY::TVN_SELCHANGING message handler
//

void CScriptManager::OnTreeViewSelChanging(NMHDR *lpnmt, LRESULT *pResult)
{
   DWORD dwId;

   *pResult = 0;
   if (((LPNMTREEVIEW)lpnmt)->itemOld.hItem != NULL)
   {
      dwId = m_wndTreeCtrl.GetItemData(((LPNMTREEVIEW)lpnmt)->itemOld.hItem);
      if (dwId != 0)
      {
         if (!m_wndScriptView.ValidateClose())
            *pResult = 1;
      }
   }
}


//
// Advanced command routing
//

BOOL CScriptManager::OnCmdMsg(UINT nID, int nCode, void* pExtra, AFX_CMDHANDLERINFO* pHandlerInfo) 
{
   if (m_wndScriptView.OnCmdMsg(nID, nCode, pExtra, pHandlerInfo))
      return TRUE;
	
	return CMDIChildWnd::OnCmdMsg(nID, nCode, pExtra, pHandlerInfo);
}


//
// Build full script name
//

void CScriptManager::BuildScriptName(HTREEITEM hLast, CString &strName)
{
   HTREEITEM hItem;

   strName = m_wndTreeCtrl.GetItemText(hLast);
   hItem = m_wndTreeCtrl.GetParentItem(hLast);
   while(hItem != m_hRootItem)
   {
      strName.Insert(0, _T("::"));
      strName.Insert(0, (LPCTSTR)m_wndTreeCtrl.GetItemText(hItem));
      hItem = m_wndTreeCtrl.GetParentItem(hItem);
   }
}


//
// Set list or tree mode
//

void CScriptManager::SetMode(int nMode)
{
   DWORD dwId;
   HTREEITEM hItem;
   int iItem;

   if (m_nMode != nMode)
   {
      m_nMode = nMode;
      if (m_nMode == MODE_LIST)
      {
         hItem = m_wndTreeCtrl.GetSelectedItem();
         if (hItem != NULL)
         {
            dwId = m_wndTreeCtrl.GetItemData(hItem);
            if (dwId != NULL)
            {
               LVFINDINFO lvfi;

               lvfi.flags = LVFI_PARAM;
               lvfi.lParam = dwId;
               iItem = m_wndListCtrl.FindItem(&lvfi, -1);
            }
            else
            {
               iItem = -1;
            }
         }
         else
         {
            iItem = -1;
         }

         SelectListViewItem(&m_wndListCtrl, iItem);
         if (iItem == -1)
         {
            if (m_wndScriptView.ValidateClose())
               m_wndScriptView.SetEmptyMode();
         }

         m_wndListCtrl.ShowWindow(SW_SHOW);
         m_wndTreeCtrl.ShowWindow(SW_HIDE);
 
         m_wndTreeCtrl.SetDlgCtrlID(-1);
         m_wndListCtrl.SetDlgCtrlID(m_wndSplitter.IdFromRowCol(0, 0));
      }
      else
      {
         iItem = m_wndListCtrl.GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
         if (iItem != -1)
         {
            dwId = m_wndListCtrl.GetItemData(iItem);
            hItem = FindTreeCtrlItemEx(m_wndTreeCtrl, TVI_ROOT, dwId);
         }
         else
         {
            hItem = NULL;
         }

         m_wndTreeCtrl.SelectItem(hItem);
         if (hItem == NULL)
         {
            if (m_wndScriptView.ValidateClose())
               m_wndScriptView.SetEmptyMode();
         }

         m_wndTreeCtrl.ShowWindow(SW_SHOW);
         m_wndListCtrl.ShowWindow(SW_HIDE);
 
         m_wndListCtrl.SetDlgCtrlID(-1);
         m_wndTreeCtrl.SetDlgCtrlID(m_wndSplitter.IdFromRowCol(0, 0));
      }
      m_wndSplitter.RecalcLayout();
   }
}


//
// Change script library view mode
//

void CScriptManager::OnScriptViewaslist() 
{
   SetMode(MODE_LIST);
}

void CScriptManager::OnUpdateScriptViewaslist(CCmdUI* pCmdUI) 
{
   pCmdUI->SetRadio(m_nMode == MODE_LIST);
}

void CScriptManager::OnScriptViewastree() 
{
   SetMode(MODE_TREE);
}

void CScriptManager::OnUpdateScriptViewastree(CCmdUI* pCmdUI) 
{
   pCmdUI->SetRadio(m_nMode == MODE_TREE);
}


//
// Create new script
//

void CScriptManager::OnScriptNew() 
{
   CInputBox dlg;
   DWORD dwResult, dwId;
   TCHAR szBuffer[MAX_DB_STRING];

   dlg.m_strTitle = _T("Create New Script");
   dlg.m_strHeader = _T("Script name");
   dlg.m_pfTextValidator = IsValidScriptName;
   dlg.m_strErrorMsg = _T("Invalid script name entered");
   if (dlg.DoModal() == IDOK)
   {
      // Ask to save currently open script if needed
      if (m_wndScriptView.ValidateClose())
      {
         m_wndScriptView.SetEmptyMode();
         dwId = 0;
         dwResult = DoRequestArg4(NXCUpdateScript, g_hSession,
                                  &dwId, (void *)((LPCTSTR)dlg.m_strText),
                                  _T(""), _T("Updating script..."));
         if (dwResult == RCC_SUCCESS)
         {
            nx_strncpy(szBuffer, (LPCTSTR)dlg.m_strText, MAX_DB_STRING);
            InsertScript(dwId, szBuffer, TRUE);
            PostMessage(WM_COMMAND, ID_SCRIPT_EDIT, 0);
         }
         else
         {
            // Clear selection on failure
            m_wndTreeCtrl.SelectItem(NULL);
            SelectListViewItem(&m_wndListCtrl, -1);
         }
      }
   }
}


//
// Insert script into list/tree
//

void CScriptManager::InsertScript(DWORD dwId, LPTSTR pszName, BOOL bSelect)
{
   TCHAR *pszCurr, *pszNext;
   HTREEITEM hItem, hNextItem;
   int iItem;

   iItem = m_wndListCtrl.InsertItem(0x7FFFFFFF, pszName, 3);
   m_wndListCtrl.SetItemData(iItem, dwId);

   for(pszCurr = pszName, hItem = m_hRootItem; ;
       pszCurr = pszNext + 2, hItem = hNextItem)
   {
      pszNext = _tcsstr(pszCurr, _T("::"));
      if (pszNext == NULL)
         break;
      *pszNext = 0;
      hNextItem = FindTreeCtrlItem(m_wndTreeCtrl, hItem, pszCurr);
      if (hNextItem == NULL)
      {
         hNextItem = m_wndTreeCtrl.InsertItem(pszCurr, 1, 1, hItem);
         m_wndTreeCtrl.SetItemData(hNextItem, 0);
         m_wndTreeCtrl.SortChildren(hItem);
      }
   }
   hNextItem = m_wndTreeCtrl.InsertItem(pszCurr, 3, 3, hItem);
   m_wndTreeCtrl.SetItemData(hNextItem, dwId);
   m_wndTreeCtrl.SortChildren(hItem);

   if (bSelect)
   {
      SelectListViewItem(&m_wndListCtrl, iItem);
      m_wndTreeCtrl.SelectItem(hNextItem);
   }
}


//
// WM_NOTIFY::LVN_ITEMCHANGED message handler
//

void CScriptManager::OnListViewItemChange(NMHDR *pItem, LRESULT *pResult)
{
   DWORD dwId;
   TCHAR szName[MAX_DB_STRING];

   if (((LPNMLISTVIEW)pItem)->iItem != -1)
   {
      if ((((LPNMLISTVIEW)pItem)->uChanged & LVIF_STATE) &&
          (((LPNMLISTVIEW)pItem)->uNewState & LVIS_FOCUSED))
      {
         dwId = m_wndListCtrl.GetItemData(((LPNMLISTVIEW)pItem)->iItem);
         if (dwId != 0)
         {
            m_wndListCtrl.GetItemText(((LPNMLISTVIEW)pItem)->iItem, 0, szName, MAX_DB_STRING);
            m_wndScriptView.SetEditMode(dwId, szName);
         }
      }
   }
   *pResult = 0;
}


//
// WM_NOTIFY::LVN_ITEMCHANGING message handler
//

void CScriptManager::OnListViewItemChanging(NMHDR *pItem, LRESULT *pResult)
{
   DWORD dwId;

   *pResult = 0;
   if (((LPNMLISTVIEW)pItem)->uChanged & LVIF_STATE)
   {
      if ((((LPNMLISTVIEW)pItem)->uOldState & LVIS_FOCUSED) && (!(((LPNMLISTVIEW)pItem)->uNewState & LVIS_FOCUSED)))
      {
         dwId = m_wndListCtrl.GetItemData(((LPNMLISTVIEW)pItem)->iItem);
         if (dwId != 0)
         {
            if (!m_wndScriptView.ValidateClose())
               *pResult = 1;
         }
      }
   }
}


//
// Handler for double click in list or tree view
//

void CScriptManager::OnViewDblClk(NMHDR *pHdr, LRESULT *pResult)
{
   m_wndScriptView.PostMessage(WM_COMMAND, ID_SCRIPT_EDIT, 0);
}


//
// WM_CLOSE message handler
//

void CScriptManager::OnClose() 
{
   if (m_wndScriptView.ValidateClose())
	   CMDIChildWnd::OnClose();
}


//
// WM_CONTEXTMENU message handler
//

void CScriptManager::OnContextMenu(CWnd* pWnd, CPoint point) 
{
   CMenu *pMenu;

   if (m_nMode == MODE_TREE)
   {
      HTREEITEM hItem;
      UINT uFlags;
      CPoint pt;

      pt = point;
      m_wndTreeCtrl.ScreenToClient(&pt);

      hItem = m_wndTreeCtrl.HitTest(pt, &uFlags);
      if ((hItem != NULL) && (uFlags & TVHT_ONITEM))
         m_wndTreeCtrl.Select(hItem, TVGN_CARET);
   }
   pMenu = theApp.GetContextMenu(19);
   pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point.x, point.y, this, NULL);
}


//
// Delete selected script
//

void CScriptManager::OnScriptDelete() 
{
   DWORD dwScriptId, dwResult;
   HTREEITEM hItem;
   int iItem;
   LVFINDINFO lvfi;

   dwScriptId = GetSelectedScriptID();
   if (dwScriptId != 0)
   {
      dwResult = DoRequestArg2(NXCDeleteScript, g_hSession, (void *)dwScriptId,
                               _T("Deleting script..."));
      if (dwResult == RCC_SUCCESS)
      {
         m_wndScriptView.SetEmptyMode();

         hItem = FindTreeCtrlItemEx(m_wndTreeCtrl, TVI_ROOT, dwScriptId);
         if (hItem != NULL)
            m_wndTreeCtrl.DeleteItem(hItem);

         lvfi.flags = LVFI_PARAM;
         lvfi.lParam = dwScriptId;
         iItem = m_wndListCtrl.FindItem(&lvfi, -1);
         if (iItem != -1)
            m_wndListCtrl.DeleteItem(iItem);
      }
      else
      {
         theApp.ErrorBox(dwResult, _T("Cannot delete script: %s"));
      }
   }
}

void CScriptManager::OnUpdateScriptDelete(CCmdUI* pCmdUI) 
{
   pCmdUI->Enable(GetSelectedScriptID() != 0);
}


//
// Get ID of currently selected script
//

DWORD CScriptManager::GetSelectedScriptID()
{
   DWORD dwId = 0;

   if (m_nMode == MODE_LIST)
   {
      int iItem;

      iItem = m_wndListCtrl.GetNextItem(-1, LVNI_SELECTED | LVNI_FOCUSED);
      if (iItem != -1)
         dwId = m_wndListCtrl.GetItemData(iItem);
   }
   else
   {
      HTREEITEM hItem;

      hItem = m_wndTreeCtrl.GetSelectedItem();
      if (hItem != NULL)
         dwId = m_wndTreeCtrl.GetItemData(hItem);
   }
   return dwId;
}


//
// Rename selected script
//

void CScriptManager::OnScriptRename() 
{
   DWORD dwScriptId, dwResult;
   HTREEITEM hItem;
   int iItem;
   LVFINDINFO lvfi;
   CInputBox dlg;

   dwScriptId = GetSelectedScriptID();
   if (dwScriptId != 0)
   {
      hItem = FindTreeCtrlItemEx(m_wndTreeCtrl, TVI_ROOT, dwScriptId);

      lvfi.flags = LVFI_PARAM;
      lvfi.lParam = dwScriptId;
      iItem = m_wndListCtrl.FindItem(&lvfi, -1);

      if (iItem != -1)
         dlg.m_strText = m_wndListCtrl.GetItemText(iItem, 0);

      dlg.m_strTitle = _T("Rename Script");
      dlg.m_strHeader = _T("Script name");
      dlg.m_pfTextValidator = IsValidScriptName;
      dlg.m_strErrorMsg = _T("Invalid script name entered");
      if (dlg.DoModal() == IDOK)
      {
         dwResult = DoRequestArg3(NXCRenameScript, g_hSession, (void *)dwScriptId,
                                  (void *)((LPCTSTR)dlg.m_strText),
                                  _T("Changing script name..."));
         if (dwResult == RCC_SUCCESS)
         {
            if (iItem != -1)
               m_wndListCtrl.SetItemText(iItem, 0, (LPCTSTR)dlg.m_strText);
            if (hItem != NULL)
               m_wndTreeCtrl.SetItemText(hItem, (LPCTSTR)dlg.m_strText);
         }
         else
         {
            theApp.ErrorBox(dwResult, _T("Cannot rename script: %s"));
         }
      }
   }
}

void CScriptManager::OnUpdateScriptRename(CCmdUI* pCmdUI) 
{
   pCmdUI->Enable(GetSelectedScriptID() != 0);
}
