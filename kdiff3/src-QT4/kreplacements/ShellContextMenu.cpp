/***************************************************************************
                          ShellContextMenu.cpp  -  description
                             -------------------
    begin                : Sat Mar 4 2006
    copyright            : (C) 2005-2007 by Joachim Eibl
    email                : joachim dot eibl at gmx dot de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
// ShellContextMenu.cpp: Implementierung der Klasse CShellContextMenu.
// http://www.codeproject.com/Articles/4025/Use-Shell-ContextMenu-in-your-applications
//
//////////////////////////////////////////////////////////////////////
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <malloc.h>
#include <QString>
#include <QStringList>
#include <qwidget.h>
#include <QDir>
#include <QMenu>
#include "ShellContextMenu.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Konstruktion/Destruktion
//////////////////////////////////////////////////////////////////////

#define MIN_ID 100
#define MAX_ID 10000


void showShellContextMenu( const QString& itemPath, QPoint pt, QWidget* pParentWidget, QMenu* pMenu )
{
    CShellContextMenu scm;
    scm.SetObjects(QDir::toNativeSeparators(QDir::cleanPath(itemPath)));
    int id = scm.ShowContextMenu (pParentWidget, pt, pMenu);
    if (id>=1)
       pMenu->actions().value(id-1)->trigger();
}

IContextMenu2 * g_IContext2 = NULL;
IContextMenu3 * g_IContext3 = NULL;
static WNDPROC OldWndProc = 0;

CShellContextMenu::CShellContextMenu()
{
	m_psfFolder = NULL;
	m_pidlArray = NULL;
	m_hMenu = NULL;
}

CShellContextMenu::~CShellContextMenu()
{
	// free all allocated datas
	if (m_psfFolder && bDelete)
		m_psfFolder->Release ();
	m_psfFolder = NULL;
	FreePIDLArray (m_pidlArray);
	m_pidlArray = NULL;

	if (m_hMenu)
		DestroyMenu( m_hMenu );
}



// this functions determines which version of IContextMenu is avaibale for those objects (always the highest one)
// and returns that interface
BOOL CShellContextMenu::GetContextMenu (void ** ppContextMenu, int & iMenuType)
{
	*ppContextMenu = NULL;
	LPCONTEXTMENU icm1 = NULL;

        if ( m_psfFolder==0 )
           return FALSE;
	
	// first we retrieve the normal IContextMenu interface (every object should have it)
	m_psfFolder->GetUIObjectOf (NULL, nItems, (LPCITEMIDLIST *) m_pidlArray, IID_IContextMenu, NULL, (void**) &icm1);

	if (icm1)
	{	// since we got an IContextMenu interface we can now obtain the higher version interfaces via that
		if (icm1->QueryInterface (IID_IContextMenu3, ppContextMenu) == NOERROR)
			iMenuType = 3;
		else if (icm1->QueryInterface (IID_IContextMenu2, ppContextMenu) == NOERROR)
			iMenuType = 2;

		if (*ppContextMenu) 
			icm1->Release(); // we can now release version 1 interface, cause we got a higher one
		else 
		{	
			iMenuType = 1;
			*ppContextMenu = icm1;	// since no higher versions were found
		}							// redirect ppContextMenu to version 1 interface
	}
	else
		return (FALSE);	// something went wrong
	
	return (TRUE); // success
}


LRESULT CALLBACK CShellContextMenu::HookWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{ 
	case WM_MENUCHAR:	// only supported by IContextMenu3
		if (g_IContext3)
		{
			LRESULT lResult = 0;
			g_IContext3->HandleMenuMsg2 (message, wParam, lParam, &lResult);
			return (lResult);
		}
		break;

	case WM_DRAWITEM:
	case WM_MEASUREITEM:
		if (wParam) 
			break; // if wParam != 0 then the message is not menu-related
  
	case WM_INITMENUPOPUP:
		if (g_IContext2)
			g_IContext2->HandleMenuMsg (message, wParam, lParam);
		else	// version 3
			g_IContext3->HandleMenuMsg (message, wParam, lParam);
		return (message == WM_INITMENUPOPUP ? 0 : TRUE); // inform caller that we handled WM_INITPOPUPMENU by ourself
		break;

	default:
		break;
	}

	// call original WndProc of window to prevent undefined bevhaviour of window
    return ::CallWindowProc (OldWndProc, hWnd, message, wParam, lParam);
}

UINT CShellContextMenu::ShowContextMenu(QWidget * pParentWidget, QPoint pt, QMenu* pMenu )
{
        HWND hWnd = pParentWidget->winId();
        int iMenuType = 0;	// to know which version of IContextMenu is supported
	LPCONTEXTMENU pContextMenu;	// common pointer to IContextMenu and higher version interface
   
	if (!GetContextMenu ((void**) &pContextMenu, iMenuType))	
		return (0);	// something went wrong

	if (!m_hMenu)
	{
		DestroyMenu( m_hMenu );
		m_hMenu = CreatePopupMenu ();
	}

        int i;
        QList<QAction*> actionList = pMenu->actions();
        for( i=0; i<actionList.count(); ++i )
        {
           QString s = actionList.at(i)->text();
           if (!s.isEmpty())
              AppendMenuW( m_hMenu, MF_STRING, i+1, (LPCWSTR)s.utf16() );
        }
        AppendMenuW( m_hMenu, MF_SEPARATOR, i+1, L"" );

	// lets fill the our popupmenu  
	pContextMenu->QueryContextMenu (m_hMenu, GetMenuItemCount (m_hMenu), MIN_ID, MAX_ID, CMF_NORMAL | CMF_EXPLORE);
 
	// subclass window to handle menurelated messages in CShellContextMenu 

	if (iMenuType > 1)	// only subclass if its version 2 or 3
	{
        OldWndProc = (WNDPROC) SetWindowLongPtr (hWnd, GWLP_WNDPROC, (DWORD_PTR) HookWndProc);
		if (iMenuType == 2)
			g_IContext2 = (LPCONTEXTMENU2) pContextMenu;
		else	// version 3
			g_IContext3 = (LPCONTEXTMENU3) pContextMenu;
	}
	else
		OldWndProc = NULL;

	UINT idCommand = TrackPopupMenu (m_hMenu,TPM_RETURNCMD | TPM_LEFTALIGN, pt.x(), pt.y(), 0, pParentWidget->winId(), 0);

	if (OldWndProc) // unsubclass
        SetWindowLongPtr (hWnd, GWLP_WNDPROC, (DWORD_PTR) OldWndProc);

	if (idCommand >= MIN_ID && idCommand <= MAX_ID)	// see if returned idCommand belongs to shell menu entries
	{
		InvokeCommand (pContextMenu, idCommand - MIN_ID);	// execute related command
		idCommand = 0;
	}
	
	pContextMenu->Release();
	g_IContext2 = NULL;
	g_IContext3 = NULL;

	return (idCommand);
}


void CShellContextMenu::InvokeCommand (LPCONTEXTMENU pContextMenu, UINT idCommand)
{
	CMINVOKECOMMANDINFO cmi = {0};
	cmi.cbSize = sizeof (CMINVOKECOMMANDINFO);
	cmi.lpVerb = (LPSTR) MAKEINTRESOURCE (idCommand);
	cmi.nShow = SW_SHOWNORMAL;
	
	pContextMenu->InvokeCommand (&cmi);
}


void CShellContextMenu::SetObjects(const QString& strObject)
{
	// only one object is passed
	QStringList strArray;
	strArray << strObject;	// create a CStringArray with one element
	
	SetObjects (strArray);		// and pass it to SetObjects (CStringArray &strArray)
								// for further processing
}


void CShellContextMenu::SetObjects(const QStringList &strList)
{
	// free all allocated datas
	if (m_psfFolder && bDelete)
		m_psfFolder->Release ();
	m_psfFolder = NULL;
	FreePIDLArray (m_pidlArray);
	m_pidlArray = NULL;
	
	// get IShellFolder interface of Desktop (root of shell namespace)
	IShellFolder * psfDesktop = NULL;
	SHGetDesktopFolder (&psfDesktop);	// needed to obtain full qualified pidl

	// ParseDisplayName creates a PIDL from a file system path relative to the IShellFolder interface
	// but since we use the Desktop as our interface and the Desktop is the namespace root
	// that means that it's a fully qualified PIDL, which is what we need
	LPITEMIDLIST pidl = NULL;
	
	psfDesktop->ParseDisplayName (NULL, 0, (LPOLESTR)strList[0].utf16(), NULL, &pidl, NULL);

	// now we need the parent IShellFolder interface of pidl, and the relative PIDL to that interface
	LPITEMIDLIST pidlItem = NULL;	// relative pidl
	SHBindToParentEx (pidl, IID_IShellFolder, (void **) &m_psfFolder, NULL);
	free (pidlItem);
	// get interface to IMalloc (need to free the PIDLs allocated by the shell functions)
	LPMALLOC lpMalloc = NULL;
	SHGetMalloc (&lpMalloc);
	lpMalloc->Free (pidl);

	// now we have the IShellFolder interface to the parent folder specified in the first element in strArray
	// since we assume that all objects are in the same folder (as it's stated in the MSDN)
	// we now have the IShellFolder interface to every objects parent folder
	
	IShellFolder * psfFolder = NULL;
	nItems = strList.size ();
	for (int i = 0; i < nItems; i++)
	{
                pidl=0;
		psfDesktop->ParseDisplayName (NULL, 0, (LPOLESTR)strList[i].utf16(), NULL, &pidl, NULL);
                if (pidl)
                {
                     m_pidlArray = (LPITEMIDLIST *) realloc (m_pidlArray, (i + 1) * sizeof (LPITEMIDLIST));
                     // get relative pidl via SHBindToParent
                     SHBindToParentEx (pidl, IID_IShellFolder, (void **) &psfFolder, (LPCITEMIDLIST *) &pidlItem);
                     m_pidlArray[i] = CopyPIDL (pidlItem);	// copy relative pidl to pidlArray
                     free (pidlItem);
                     lpMalloc->Free (pidl);		// free pidl allocated by ParseDisplayName
                     psfFolder->Release ();
                }
	}
	lpMalloc->Release ();
	psfDesktop->Release ();

	bDelete = TRUE;	// indicates that m_psfFolder should be deleted by CShellContextMenu
}


// only one full qualified PIDL has been passed
void CShellContextMenu::SetObjects(LPITEMIDLIST /*pidl*/)
{
/*
   // free all allocated datas
	if (m_psfFolder && bDelete)
		m_psfFolder->Release ();
	m_psfFolder = NULL;
	FreePIDLArray (m_pidlArray);
	m_pidlArray = NULL;

		// full qualified PIDL is passed so we need
	// its parent IShellFolder interface and its relative PIDL to that
	LPITEMIDLIST pidlItem = NULL;
	SHBindToParent ((LPCITEMIDLIST) pidl, IID_IShellFolder, (void **) &m_psfFolder, (LPCITEMIDLIST *) &pidlItem);	

	m_pidlArray = (LPITEMIDLIST *) malloc (sizeof (LPITEMIDLIST));	// allocate ony for one elemnt
	m_pidlArray[0] = CopyPIDL (pidlItem);


	// now free pidlItem via IMalloc interface (but not m_psfFolder, that we need later
	LPMALLOC lpMalloc = NULL;
	SHGetMalloc (&lpMalloc);
	lpMalloc->Free (pidlItem);
	lpMalloc->Release();

	nItems = 1;
	bDelete = TRUE;	// indicates that m_psfFolder should be deleted by CShellContextMenu
*/
}


// IShellFolder interface with a relative pidl has been passed
void CShellContextMenu::SetObjects(IShellFolder *psfFolder, LPITEMIDLIST pidlItem)
{
	// free all allocated datas
	if (m_psfFolder && bDelete)
		m_psfFolder->Release ();
	m_psfFolder = NULL;
	FreePIDLArray (m_pidlArray);
	m_pidlArray = NULL;

	m_psfFolder = psfFolder;

	m_pidlArray = (LPITEMIDLIST *) malloc (sizeof (LPITEMIDLIST));
	m_pidlArray[0] = CopyPIDL (pidlItem);
	
	nItems = 1;
	bDelete = FALSE;	// indicates wheter m_psfFolder should be deleted by CShellContextMenu
}

void CShellContextMenu::SetObjects(IShellFolder * psfFolder, LPITEMIDLIST *pidlArray, int nItemCount)
{
	// free all allocated datas
	if (m_psfFolder && bDelete)
		m_psfFolder->Release ();
	m_psfFolder = NULL;
	FreePIDLArray (m_pidlArray);
	m_pidlArray = NULL;

	m_psfFolder = psfFolder;

	m_pidlArray = (LPITEMIDLIST *) malloc (nItemCount * sizeof (LPITEMIDLIST));

	for (int i = 0; i < nItemCount; i++)
		m_pidlArray[i] = CopyPIDL (pidlArray[i]);

	nItems = nItemCount;
	bDelete = FALSE;	// indicates wheter m_psfFolder should be deleted by CShellContextMenu
}


void CShellContextMenu::FreePIDLArray(LPITEMIDLIST *pidlArray)
{
	if (!pidlArray)
		return;

	int iSize = _msize (pidlArray) / sizeof (LPITEMIDLIST);

	for (int i = 0; i < iSize; i++)
		free (pidlArray[i]);
	free (pidlArray);
}


LPITEMIDLIST CShellContextMenu::CopyPIDL (LPCITEMIDLIST pidl, int cb)
{
	if (cb == -1)
		cb = GetPIDLSize (pidl); // Calculate size of list.

    LPITEMIDLIST pidlRet = (LPITEMIDLIST) calloc (cb + sizeof (USHORT), sizeof (BYTE));
    if (pidlRet)
		CopyMemory(pidlRet, pidl, cb);

    return (pidlRet);
}


UINT CShellContextMenu::GetPIDLSize (LPCITEMIDLIST pidl)
{  
	if (!pidl) 
		return 0;
	int nSize = 0;
	LPITEMIDLIST pidlTemp = (LPITEMIDLIST) pidl;
	while (pidlTemp->mkid.cb)
	{
		nSize += pidlTemp->mkid.cb;
		pidlTemp = (LPITEMIDLIST) (((LPBYTE) pidlTemp) + pidlTemp->mkid.cb);
	}
	return nSize;
}

HMENU  CShellContextMenu::GetMenu()
{
	if (!m_hMenu)
	{
		m_hMenu = CreatePopupMenu();	// create the popupmenu (its empty)
	}
	return (m_hMenu);
}


// this is workaround function for the Shell API Function SHBindToParent
// SHBindToParent is not available under Win95/98
HRESULT CShellContextMenu::SHBindToParentEx (LPCITEMIDLIST pidl, REFIID riid, VOID **ppv, LPCITEMIDLIST *ppidlLast)
{
	HRESULT hr = 0;
	if (!pidl || !ppv)
		return E_POINTER;
	
	int nCount = GetPIDLCount (pidl);
	if (nCount == 0)	// desktop pidl of invalid pidl
		return E_POINTER;

	IShellFolder * psfDesktop = NULL;
	SHGetDesktopFolder (&psfDesktop);
	if (nCount == 1)	// desktop pidl
	{
		if ((hr = psfDesktop->QueryInterface(riid, ppv)) == S_OK)
		{
			if (ppidlLast) 
				*ppidlLast = CopyPIDL (pidl);
		}
		psfDesktop->Release ();
		return hr;
	}

	LPBYTE pRel = GetPIDLPos (pidl, nCount - 1);
	LPITEMIDLIST pidlParent = NULL;
	pidlParent = CopyPIDL (pidl, pRel - (LPBYTE) pidl);
	IShellFolder * psfFolder = NULL;
	
	if ((hr = psfDesktop->BindToObject (pidlParent, NULL, IID_IShellFolder, (void **) &psfFolder)) != S_OK)
	{
		free (pidlParent);
		psfDesktop->Release ();
		return hr;
	}
	if ((hr = psfFolder->QueryInterface (riid, ppv)) == S_OK)
	{
		if (ppidlLast)
			*ppidlLast = CopyPIDL ((LPCITEMIDLIST) pRel);
	}
	free (pidlParent);
	psfFolder->Release ();
	psfDesktop->Release ();
	return hr;
}


LPBYTE CShellContextMenu::GetPIDLPos (LPCITEMIDLIST pidl, int nPos)
{
	if (!pidl)
		return 0;
	int nCount = 0;
	
	BYTE * pCur = (BYTE *) pidl;
	while (((LPCITEMIDLIST) pCur)->mkid.cb)
	{
		if (nCount == nPos)
			return pCur;
		nCount++;
		pCur += ((LPCITEMIDLIST) pCur)->mkid.cb;	// + sizeof(pidl->mkid.cb);
	}
	if (nCount == nPos) 
		return pCur;
	return NULL;
}


int CShellContextMenu::GetPIDLCount (LPCITEMIDLIST pidl)
{
	if (!pidl)
		return 0;

	int nCount = 0;
	BYTE*  pCur = (BYTE *) pidl;
	while (((LPCITEMIDLIST) pCur)->mkid.cb)
	{
		nCount++;
		pCur += ((LPCITEMIDLIST) pCur)->mkid.cb;
	}
	return nCount;
}

#endif

