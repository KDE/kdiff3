// ShellContextMenu.h: Schnittstelle für die Klasse CShellContextMenu.
//
//////////////////////////////////////////////////////////////////////

#ifndef SHELLCONTEXTMENU_H
#define SHELLCONTEXTMENU_H

/////////////////////////////////////////////////////////////////////
// class to show shell contextmenu of files/folders/shell objects
// developed by R. Engels 2003
/////////////////////////////////////////////////////////////////////

class CShellContextMenu  
{
public:
	HMENU GetMenu ();
	void SetObjects (IShellFolder * psfFolder, LPITEMIDLIST pidlItem);
	void SetObjects (IShellFolder * psfFolder, LPITEMIDLIST * pidlArray, int nItemCount);
	void SetObjects (LPITEMIDLIST pidl);
	void SetObjects (const QString& strObject);
	void SetObjects (const QStringList& strList);
	UINT ShowContextMenu (QWidget* pParent, QPoint pt, QPopupMenu* pMenu);
	CShellContextMenu();
	virtual ~CShellContextMenu();

private:
	int nItems;
	BOOL bDelete;
	HMENU m_hMenu;
	IShellFolder * m_psfFolder;
	LPITEMIDLIST * m_pidlArray;	
	
	void InvokeCommand (LPCONTEXTMENU pContextMenu, UINT idCommand);
	BOOL GetContextMenu (void ** ppContextMenu, int & iMenuType);
	HRESULT SHBindToParentEx (LPCITEMIDLIST pidl, REFIID riid, VOID **ppv, LPCITEMIDLIST *ppidlLast);
	static LRESULT CALLBACK HookWndProc (HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void FreePIDLArray (LPITEMIDLIST * pidlArray);
	LPITEMIDLIST CopyPIDL (LPCITEMIDLIST pidl, int cb = -1);
	UINT GetPIDLSize (LPCITEMIDLIST pidl);
	LPBYTE GetPIDLPos (LPCITEMIDLIST pidl, int nPos);
	int GetPIDLCount (LPCITEMIDLIST pidl);
};

#endif
