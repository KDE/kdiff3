/***************************************************************************
                          directorymergewindow.cpp
                             -------------------
    begin                : Sat Oct 19 2002
    copyright            : (C) 2002 by Joachim Eibl
    email                : joachim.eibl@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "directorymergewindow.h"
#include "optiondialog.h"
#include <vector>
#include <map>

#include <qdir.h>
#include <qapplication.h>
#include <qpixmap.h>
#include <qimage.h>
#include <kpopupmenu.h>
#include <kaction.h>
#include <qregexp.h>
#include <qmessagebox.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qtable.h>
#include <qsplitter.h>
#include <qprogressdialog.h>
#include <kmessagebox.h>
#include <kiconloader.h>
#include <klocale.h>
#include <iostream>
#include <assert.h>

static bool conflictingFileTypes(MergeFileInfos& mfi);

class StatusInfo : public QListView
{
public:
   StatusInfo(QWidget* pParent) : QListView( pParent )
   {
      addColumn("");
      setSorting(-1); //disable sorting
   }

   QListViewItem* m_pLast;
   QListViewItem* last()
   {
      if (firstChild()==0) return 0;
      else                 return m_pLast;
   }
   
   void addText(const QString& s )
   {
      if (firstChild()==0) m_pLast = new QListViewItem( this, s );
      else                 m_pLast = new QListViewItem( this, last(), s );
   }
};


class TempRemover
{
public:
   TempRemover( const QString& origName, FileAccess& fa );
   ~TempRemover();
   QString name() { return m_name; }
   bool success() { return m_bSuccess; }
private:
   QString m_name;
   bool m_bTemp;
   bool m_bSuccess;
};
TempRemover::TempRemover(const QString& origName, FileAccess& fa)
{
   if ( fa.isLocal() )
   {
      m_name = origName;
      m_bTemp = false;
      m_bSuccess = true;
   }
   else
   {
      m_name = FileAccess::tempFileName();
      m_bSuccess = fa.copyFile( m_name );
      m_bTemp = m_bSuccess;
   }
}
TempRemover::~TempRemover()
{
   if ( m_bTemp && ! m_name.isEmpty() )
      FileAccess::removeFile(m_name);
}

void DirectoryMergeWindow::fastFileComparison(
   FileAccess& fi1, FileAccess& fi2,
   bool& bEqual, bool& bError, QString& status )
{
   status = "";
   bEqual = false;
   bError = true;

   if ( !m_bFollowFileLinks )
   {
      if ( fi1.isSymLink() != fi2.isSymLink() )
      {
         status = i18n("Mix of links and normal files.");
         return;
      }
      else if ( fi1.isSymLink() && fi2.isSymLink() )
      {
         bError = false;
         bEqual = fi1.readLink() == fi2.readLink();
         status = i18n("Link: ");
         return;
      }
   }

   if ( fi1.size()!=fi2.size() )
   {
      bEqual = false;
      status = i18n("Size. ");
      return;
   }
   else if ( m_pOptions->m_bDmTrustSize )
   {
      bEqual = true;
      return;
   }

   if ( m_pOptions->m_bDmTrustDate )
   {
      bEqual = ( fi1.lastModified() == fi2.lastModified()  &&  fi1.size()==fi2.size() );
      bError = false;
      status = i18n("Date & Size: ");
      return;
   }

   QString fileName1 = fi1.absFilePath();
   QString fileName2 = fi2.absFilePath();
   TempRemover tr1( fileName1, fi1 );
   if ( !tr1.success() )
   {
      status = i18n("Creating temp copy of %1 failed.").arg(fileName1);
      return;
   }
   TempRemover tr2( fileName2, fi2 );
   if ( !tr2.success() )
   {
      status = i18n("Creating temp copy of %1 failed.").arg(fileName2);
      return;
   }

   std::vector<char> buf1(100000);
   std::vector<char> buf2(buf1.size());

   QFile file1( tr1.name() );

   if ( ! file1.open(IO_ReadOnly) )
   {
      status = i18n("Opening %1 failed.").arg(fileName1);
      return;
   }

   QFile file2( tr2.name() );

   if ( ! file2.open(IO_ReadOnly) )
   {
      status = i18n("Opening %1 failed.").arg(fileName2);
      return;
   }

#if QT_VERSION==230
   typedef int t_FileSize;
#else
   typedef QFile::Offset t_FileSize;
#endif
   t_FileSize size = file1.size();

   while( size>0 )
   {
      int len = min2( size, (t_FileSize)buf1.size() );
      if( len != file1.readBlock( &buf1[0], len ) )
      {
         status = i18n("Error reading from %1").arg(fileName1);
         return;
      }

      if( len != file2.readBlock( &buf2[0], len ) )
      {
         status = i18n("Error reading from %1").arg(fileName2);
         return;
      }

      if ( memcmp( &buf1[0], &buf2[0], len ) != 0 )
      {
         bError = false;
         return;
      }
      size-=len;
   }

   // If the program really arrives here, then the files are really equal.
   bError = false;
   bEqual = true;
}





static int s_nameCol = 0;
static int s_ACol = 1;
static int s_BCol = 2;
static int s_CCol = 3;
static int s_OpCol = 4;
static int s_OpStatusCol = 5;
DirectoryMergeWindow::DirectoryMergeWindow( QWidget* pParent, OptionDialog* pOptions, KIconLoader* pIconLoader )
   : QListView( pParent )
{
   connect( this, SIGNAL(doubleClicked(QListViewItem*)), this, SLOT(onDoubleClick(QListViewItem*)));
   connect( this, SIGNAL(returnPressed(QListViewItem*)), this, SLOT(onDoubleClick(QListViewItem*)));
   connect( this, SIGNAL( pressed(QListViewItem*,const QPoint&, int)),
            this, SLOT(   onClick(QListViewItem*,const QPoint&, int))  );
   connect( this, SIGNAL(selectionChanged(QListViewItem*)), this, SLOT(onSelectionChanged(QListViewItem*)));
   m_pOptions = pOptions;
   m_pIconLoader = pIconLoader;
   m_pDirectoryMergeInfo = 0;
   m_bAllowResizeEvents = true;
   m_bSimulatedMergeStarted=false;
   m_bRealMergeStarted=false;
   m_bError = false;
   m_bSyncMode = false;
   m_pStatusInfo = new StatusInfo(0);
   m_pStatusInfo->hide();
   
   addColumn(i18n("Name"));
   addColumn("A");
   addColumn("B");
   addColumn("C");
   addColumn(i18n("Operation"));
   addColumn(i18n("Status"));
}

DirectoryMergeWindow::~DirectoryMergeWindow()
{
}


int DirectoryMergeWindow::totalColumnWidth()
{
   int w=0;
   for (int i=0; i<s_OpStatusCol; ++i)
   {
      w += columnWidth(i);
   }
   return w;
}

void DirectoryMergeWindow::reload()
{
   if ( isDirectoryMergeInProgress() )
   {
      int result = KMessageBox::warningYesNo(this,
         i18n("You are currently doing a directory merge. Are you sure, you want to abort the merge and rescan the directory?"),
         i18n("Warning"), i18n("Rescan"), i18n("Continue Merging") );
      if ( result!=KMessageBox::Yes )
         return;
   }
   init( m_dirA, m_dirB, m_dirC, m_dirDest, m_bDirectoryMerge );
}

// Copy pm2 onto pm1, but preserve the alpha value from pm1 where pm2 is transparent.
static QPixmap pixCombiner( const QPixmap& pm1, const QPixmap& pm2 )
{
   QImage img1 = pm1.convertToImage().convertDepth(32);
   QImage img2 = pm2.convertToImage().convertDepth(32);

   for (int y = 0; y < img1.height(); y++)
   {
      Q_UINT32 *line1 = reinterpret_cast<Q_UINT32 *>(img1.scanLine(y));
      Q_UINT32 *line2 = reinterpret_cast<Q_UINT32 *>(img2.scanLine(y));
      for (int x = 0; x < img1.width();  x++)
      {
         if ( qAlpha( line2[x] ) >0 )
            line1[x] = (line2[x] | 0xff000000);
      }
   }
   QPixmap pix;
   pix.convertFromImage(img1);
   return pix;
}

// like pixCombiner but let the pm1 color shine through
static QPixmap pixCombiner2( const QPixmap& pm1, const QPixmap& pm2 )
{
   QImage img1 = pm1.convertToImage().convertDepth(32);
   QImage img2 = pm2.convertToImage().convertDepth(32);

   for (int y = 0; y < img1.height(); y++)
   {
      Q_UINT32 *line1 = reinterpret_cast<Q_UINT32 *>(img1.scanLine(y));
      Q_UINT32 *line2 = reinterpret_cast<Q_UINT32 *>(img2.scanLine(y));
      for (int x = 0; x < img1.width();  x++)
      {
         if ( qAlpha( line2[x] ) >0 )
         {
            int r = ( qRed( line1[x] ) + qRed( line2[x] ))/2;
            int g = ( qGreen( line1[x] ) + qGreen( line2[x] ))/2;
            int b = ( qBlue( line1[x] ) + qBlue( line2[x] ))/2;
            line1[x] = qRgba( r,g,b, 0xff );
         }
      }
   }
   QPixmap pix;
   pix.convertFromImage(img1);
   return pix;
}

static void calcDirStatus( bool bThreeDirs, DirMergeItem* i, int& nofFiles,
                           int& nofDirs, int& nofEqualFiles, int& nofManualMerges )
{
   if ( i->m_pMFI->m_bDirA || i->m_pMFI->m_bDirB || i->m_pMFI->m_bDirC )
   {
      ++nofDirs;
   }
   else
   {
      ++nofFiles;
      if ( i->m_pMFI->m_bEqualAB && (!bThreeDirs || i->m_pMFI->m_bEqualAC ))
      {
         ++nofEqualFiles;
      }
      else
      {
         if ( i->m_pMFI->m_eMergeOperation==eMergeABCToDest || i->m_pMFI->m_eMergeOperation==eMergeABToDest )
            ++nofManualMerges;
      }
   }
   for( QListViewItem* p = i->firstChild();  p!=0; p = p->nextSibling() )
      calcDirStatus( bThreeDirs, static_cast<DirMergeItem*>(p), nofFiles, nofDirs, nofEqualFiles, nofManualMerges );
}

bool DirectoryMergeWindow::init
   (
   FileAccess& dirA,
   FileAccess& dirB,
   FileAccess& dirC,
   FileAccess& dirDest,
   bool bDirectoryMerge
   )
{
   m_bFollowDirLinks = m_pOptions->m_bDmFollowDirLinks;
   m_bFollowFileLinks = m_pOptions->m_bDmFollowFileLinks;
   m_bSimulatedMergeStarted=false;
   m_bRealMergeStarted=false;
   m_bError=false;
   m_bDirectoryMerge = bDirectoryMerge;

   clear();

   m_mergeItemList.clear();
   m_currentItemForOperation = m_mergeItemList.end();

   m_dirA = dirA;
   m_dirB = dirB;
   m_dirC = dirC;
   m_dirDest = dirDest;

   // Check if all input directories exist and are valid. The dest dir is not tested now.
   // The test will happen only when we are going to write to it.
   if ( !m_dirA.isDir() || !m_dirB.isDir() ||
        (m_dirC.isValid() && !m_dirC.isDir()) )
   {
       QString text( i18n("Opening of directories failed:") );
       text += "\n\n";
       if ( !dirA.isDir() )
       {  text += i18n("Dir A \"%1\" does not exist or is not a directory.\n").arg(m_dirA.prettyAbsPath()); }

       if ( !dirB.isDir() )
       {  text += i18n("Dir B \"%1\" does not exist or is not a directory.\n").arg(m_dirB.prettyAbsPath()); }

       if ( m_dirC.isValid() && !m_dirC.isDir() )
       {  text += i18n("Dir C \"%1\" does not exist or is not a directory.\n").arg(m_dirC.prettyAbsPath()); }

       KMessageBox::sorry( this, text, i18n("Directory Open Error") );
       return false;
   }

   if ( m_dirC.isValid() &&
        (m_dirDest.prettyAbsPath() == m_dirA.prettyAbsPath()  ||  m_dirDest.prettyAbsPath()==m_dirB.prettyAbsPath() ) )
   {
      KMessageBox::error(this,
         i18n( "The destination directory must not be the same as A or B when "
         "three directories are merged.\nCheck again before continuing."),
         i18n("Parameter Warning"));
      return false;
   }

   m_bSyncMode = m_pOptions->m_bDmSyncMode && !m_dirC.isValid() && !m_dirDest.isValid();

   if ( m_dirDest.isValid() )
      m_dirDestInternal = m_dirDest;
   else
      m_dirDestInternal = m_dirC.isValid() ? m_dirC : m_dirB;

   QString origCurrentDirectory = QDir::currentDirPath();

   g_pProgressDialog->start();

   m_fileMergeMap.clear();
   t_DirectoryList::iterator i;

   // calc how many directories will be read:
   double nofScans = ( m_dirA.isValid() ? 1 : 0 )+( m_dirB.isValid() ? 1 : 0 )+( m_dirC.isValid() ? 1 : 0 );
   int currentScan = 0;

   bool bListDirSuccessA = true;
   bool bListDirSuccessB = true;
   bool bListDirSuccessC = true;
   if ( m_dirA.isValid() )
   {
      g_pProgressDialog->setInformation(i18n("Reading Directory A"));
      g_pProgressDialog->setSubRangeTransformation(currentScan/nofScans, (currentScan+1)/nofScans);
      ++currentScan;

      t_DirectoryList dirListA;
      bListDirSuccessA = m_dirA.listDir( &dirListA,
         m_pOptions->m_bDmRecursiveDirs, m_pOptions->m_bDmFindHidden,
         m_pOptions->m_DmFilePattern, m_pOptions->m_DmFileAntiPattern,
         m_pOptions->m_DmDirAntiPattern, m_pOptions->m_bDmFollowDirLinks,
         m_pOptions->m_bDmUseCvsIgnore);

      for (i=dirListA.begin(); i!=dirListA.end();++i )
      {
         MergeFileInfos& mfi = m_fileMergeMap[i->filePath()];
         //std::cout <<i->filePath()<<std::endl;
         mfi.m_bExistsInA = true;
         mfi.m_fileInfoA = *i;
      }
   }

   if ( m_dirB.isValid() )
   {
      g_pProgressDialog->setInformation(i18n("Reading Directory B"));
      g_pProgressDialog->setSubRangeTransformation(currentScan/nofScans, (currentScan+1)/nofScans);
      ++currentScan;

      t_DirectoryList dirListB;
      bListDirSuccessB =  m_dirB.listDir( &dirListB,
         m_pOptions->m_bDmRecursiveDirs, m_pOptions->m_bDmFindHidden,
         m_pOptions->m_DmFilePattern, m_pOptions->m_DmFileAntiPattern,
         m_pOptions->m_DmDirAntiPattern, m_pOptions->m_bDmFollowDirLinks,
         m_pOptions->m_bDmUseCvsIgnore);

      for (i=dirListB.begin(); i!=dirListB.end();++i )
      {
         MergeFileInfos& mfi = m_fileMergeMap[i->filePath()];
         mfi.m_bExistsInB = true;
         mfi.m_fileInfoB = *i;
      }
   }

   e_MergeOperation eDefaultMergeOp;
   if ( m_dirC.isValid() )
   {
      g_pProgressDialog->setInformation(i18n("Reading Directory C"));
      g_pProgressDialog->setSubRangeTransformation(currentScan/nofScans, (currentScan+1)/nofScans);
      ++currentScan;

      t_DirectoryList dirListC;
      bListDirSuccessC = m_dirC.listDir( &dirListC,
         m_pOptions->m_bDmRecursiveDirs, m_pOptions->m_bDmFindHidden,
         m_pOptions->m_DmFilePattern, m_pOptions->m_DmFileAntiPattern,
         m_pOptions->m_DmDirAntiPattern, m_pOptions->m_bDmFollowDirLinks,
         m_pOptions->m_bDmUseCvsIgnore);

      for (i=dirListC.begin(); i!=dirListC.end();++i )
      {
         MergeFileInfos& mfi = m_fileMergeMap[i->filePath()];
         mfi.m_bExistsInC = true;
         mfi.m_fileInfoC = *i;
      }

      eDefaultMergeOp = eMergeABCToDest;
   }
   else
      eDefaultMergeOp = m_bSyncMode ? eMergeToAB : eMergeABToDest;

   bool bContinue = true;
   if ( !bListDirSuccessA || !bListDirSuccessB || !bListDirSuccessC )
   {
      QString s = i18n("Some subdirectories were not readable in");
      if ( !bListDirSuccessA )   s += "\nA: " + m_dirA.prettyAbsPath();
      if ( !bListDirSuccessB )   s += "\nB: " + m_dirB.prettyAbsPath();
      if ( !bListDirSuccessC )   s += "\nC: " + m_dirC.prettyAbsPath();
      s+="\n";
      s+= i18n("Check the permissions of the subdirectories.");
      bContinue = KMessageBox::Continue == KMessageBox::warningContinueCancel( this, s );
   }

   if ( bContinue )
   {
      prepareListView();
      g_pProgressDialog->hide();

      for( QListViewItem* p = firstChild();  p!=0; p = p->nextSibling() )
      {
         DirMergeItem* pDMI = static_cast<DirMergeItem*>( p );
         calcSuggestedOperation( *pDMI->m_pMFI, eDefaultMergeOp );
      }
   }
   else
   {
      g_pProgressDialog->hide();
      setSelected( 0, true );
   }

   QDir::setCurrent(origCurrentDirectory);

   // Try to improve the view a little bit.
   QWidget* pParent = parentWidget();
   QSplitter* pSplitter = static_cast<QSplitter*>(pParent);
   if (pSplitter!=0)
   {
      QValueList<int> sizes = pSplitter->sizes();
      int total = sizes[0] + sizes[1];
      sizes[0]=total*6/10;
      sizes[1]=total - sizes[0];
      pSplitter->setSizes( sizes );
   }

   if ( bContinue )
   {
      // Generate a status report
      int nofFiles=0;
      int nofDirs=0;
      int nofEqualFiles=0;
      int nofManualMerges=0;
      for( QListViewItem* p = firstChild();  p!=0; p = p->nextSibling() )
         calcDirStatus( m_dirC.isValid(), static_cast<DirMergeItem*>(p),
                        nofFiles, nofDirs, nofEqualFiles, nofManualMerges );

      QString s;
      s = i18n("Directory Comparison Status") + "\n\n" +
          i18n("Number of subdirectories:") +" "+ QString::number(nofDirs)       + "\n"+
          i18n("Number of equal files:")     +" "+ QString::number(nofEqualFiles) + "\n"+
          i18n("Number of different files:") +" "+ QString::number(nofFiles-nofEqualFiles);

      if ( m_dirC.isValid() )
         s += "\n" + i18n("Number of manual merges:")   +" "+ QString::number(nofManualMerges);
      KMessageBox::information( this, s );
      setSelected( firstChild(), true );
   }

   return true;
}



void DirectoryMergeWindow::slotChooseAEverywhere(){ setAllMergeOperations( eCopyAToDest ); }

void DirectoryMergeWindow::slotChooseBEverywhere(){ setAllMergeOperations( eCopyBToDest ); }

void DirectoryMergeWindow::slotChooseCEverywhere(){ setAllMergeOperations( eCopyCToDest ); }

void DirectoryMergeWindow::slotAutoChooseEverywhere()
{
   e_MergeOperation eDefaultMergeOp = m_dirC.isValid() ?  eMergeABCToDest :
                                           m_bSyncMode ?  eMergeToAB      : eMergeABToDest;
   setAllMergeOperations(eDefaultMergeOp );
}

void DirectoryMergeWindow::slotNoOpEverywhere(){ setAllMergeOperations(eNoOperation); }

static void setListViewItemOpen( QListViewItem* p, bool bOpen )
{
   for( QListViewItem* pChild = p->firstChild();  pChild!=0; pChild = pChild->nextSibling() )
      setListViewItemOpen( pChild, bOpen );

   p->setOpen( bOpen );
}

void DirectoryMergeWindow::slotFoldAllSubdirs()
{
   for( QListViewItem* p = firstChild();  p!=0; p = p->nextSibling() )
      setListViewItemOpen( p, false );
}

void DirectoryMergeWindow::slotUnfoldAllSubdirs()
{
   for( QListViewItem* p = firstChild();  p!=0; p = p->nextSibling() )
      setListViewItemOpen( p, true );
}

static void setMergeOperation( QListViewItem* pLVI, e_MergeOperation eMergeOp )
{
   if ( pLVI==0 ) return;

   DirMergeItem* pDMI = static_cast<DirMergeItem*>(pLVI);
   MergeFileInfos& mfi = *pDMI->m_pMFI;

   mfi.setMergeOperation(eMergeOp );
}

// Merge current item (merge mode)
void DirectoryMergeWindow::slotCurrentDoNothing() { setMergeOperation(currentItem(), eNoOperation ); }
void DirectoryMergeWindow::slotCurrentChooseA()   { setMergeOperation(currentItem(), m_bSyncMode ? eCopyAToB : eCopyAToDest ); }
void DirectoryMergeWindow::slotCurrentChooseB()   { setMergeOperation(currentItem(), m_bSyncMode ? eCopyBToA : eCopyBToDest ); }
void DirectoryMergeWindow::slotCurrentChooseC()   { setMergeOperation(currentItem(), eCopyCToDest ); }
void DirectoryMergeWindow::slotCurrentMerge()
{
   bool bThreeDirs = m_dirC.isValid();
   setMergeOperation(currentItem(), bThreeDirs ? eMergeABCToDest : eMergeABToDest );
}
void DirectoryMergeWindow::slotCurrentDelete()    { setMergeOperation(currentItem(), eDeleteFromDest ); }
// Sync current item
void DirectoryMergeWindow::slotCurrentCopyAToB()     { setMergeOperation(currentItem(), eCopyAToB ); }
void DirectoryMergeWindow::slotCurrentCopyBToA()     { setMergeOperation(currentItem(), eCopyBToA ); }
void DirectoryMergeWindow::slotCurrentDeleteA()      { setMergeOperation(currentItem(), eDeleteA );  }
void DirectoryMergeWindow::slotCurrentDeleteB()      { setMergeOperation(currentItem(), eDeleteB );  }
void DirectoryMergeWindow::slotCurrentDeleteAAndB()  { setMergeOperation(currentItem(), eDeleteAB ); }
void DirectoryMergeWindow::slotCurrentMergeToA()     { setMergeOperation(currentItem(), eMergeToA ); }
void DirectoryMergeWindow::slotCurrentMergeToB()     { setMergeOperation(currentItem(), eMergeToB ); }
void DirectoryMergeWindow::slotCurrentMergeToAAndB() { setMergeOperation(currentItem(), eMergeToAB ); }


void DirectoryMergeWindow::keyPressEvent( QKeyEvent* e )
{
   if ( (e->state() & Qt::ControlButton)!=0 )
   {
      bool bThreeDirs = m_dirC.isValid();

      QListViewItem* lvi = currentItem();
      DirMergeItem* pDMI = lvi==0 ? 0 : static_cast<DirMergeItem*>(lvi);
      MergeFileInfos* pMFI = pDMI==0 ? 0 : pDMI->m_pMFI;

      if ( pMFI==0 ) return;
      bool bMergeMode = bThreeDirs || !m_bSyncMode;
      bool bFTConflict = pMFI==0 ? false : conflictingFileTypes(*pMFI);

      if ( bMergeMode )
      {
         switch(e->key())
         {
         case Key_1:      if(pMFI->m_bExistsInA){ slotCurrentChooseA(); }  return;
         case Key_2:      if(pMFI->m_bExistsInB){ slotCurrentChooseB(); }  return;
         case Key_3:      if(pMFI->m_bExistsInC){ slotCurrentChooseC(); }  return;
         case Key_Space:  slotCurrentDoNothing();                          return;
         case Key_4:      if ( !bFTConflict )   { slotCurrentMerge();   }  return;
         case Key_Delete: slotCurrentDelete();                             return;
         default: break;
         }
      }
      else
      {
         switch(e->key())
         {
         case Key_1:      if(pMFI->m_bExistsInA){ slotCurrentCopyAToB(); }  return;
         case Key_2:      if(pMFI->m_bExistsInB){ slotCurrentCopyBToA(); }  return;
         case Key_Space:  slotCurrentDoNothing();                           return;
         case Key_4:      if ( !bFTConflict ) { slotCurrentMergeToAAndB(); }  return;
         case Key_Delete: if( pMFI->m_bExistsInA && pMFI->m_bExistsInB ) slotCurrentDeleteAAndB();
                          else if( pMFI->m_bExistsInA ) slotCurrentDeleteA();
                          else if( pMFI->m_bExistsInB ) slotCurrentDeleteB();
                          return;
         default: break;
         }
      }
   }

   QListView::keyPressEvent(e);
}

void DirectoryMergeWindow::focusInEvent(QFocusEvent*)
{
   updateAvailabilities();
}
void DirectoryMergeWindow::focusOutEvent(QFocusEvent*)
{
   updateAvailabilities();
}

void DirectoryMergeWindow::setAllMergeOperations( e_MergeOperation eDefaultOperation )
{
   if ( KMessageBox::Yes == KMessageBox::warningYesNo(this,
        i18n("This affects all merge operations."),
        i18n("Changing All Merge Operations"),i18n("C&ontinue"), i18n("&Cancel") ) )
   {
      for( QListViewItem* p = firstChild(); p!=0; p = p->nextSibling() )
      {
         DirMergeItem* pDMI = static_cast<DirMergeItem*>( p );
         calcSuggestedOperation( *pDMI->m_pMFI, eDefaultOperation );
      }
   }
}


void DirectoryMergeWindow::compareFilesAndCalcAges( MergeFileInfos& mfi )
{
   std::map<QDateTime,int> dateMap;

   if( mfi.m_bExistsInA )
   {
      mfi.m_bLinkA = mfi.m_fileInfoA.isSymLink();
      mfi.m_bDirA  = mfi.m_fileInfoA.isDir();
      dateMap[ mfi.m_fileInfoA.lastModified() ] = 0;
   }
   if( mfi.m_bExistsInB )
   {
      mfi.m_bLinkB = mfi.m_fileInfoB.isSymLink();
      mfi.m_bDirB  = mfi.m_fileInfoB.isDir();
      dateMap[ mfi.m_fileInfoB.lastModified() ] = 1;
   }
   if( mfi.m_bExistsInC )
   {
      mfi.m_bLinkC = mfi.m_fileInfoC.isSymLink();
      mfi.m_bDirC  = mfi.m_fileInfoC.isDir();
      dateMap[ mfi.m_fileInfoC.lastModified() ] = 2;
   }

   bool bError;
   QString eqStatus;
   if( mfi.m_bExistsInA && mfi.m_bExistsInB )
   {
      if( mfi.m_bDirA ) mfi.m_bEqualAB=true;
      else fastFileComparison( mfi.m_fileInfoA, mfi.m_fileInfoB, mfi.m_bEqualAB, bError, eqStatus );
   }
   if( mfi.m_bExistsInA && mfi.m_bExistsInC )
   {
      if( mfi.m_bDirA ) mfi.m_bEqualAC=true;
      else fastFileComparison( mfi.m_fileInfoA, mfi.m_fileInfoC, mfi.m_bEqualAC, bError, eqStatus );
   }
   if( mfi.m_bExistsInB && mfi.m_bExistsInC )
   {
      if (mfi.m_bEqualAB && mfi.m_bEqualAC)
         mfi.m_bEqualBC = true;
      else
      {
         if( mfi.m_bDirB ) mfi.m_bEqualBC=true;
         else fastFileComparison( mfi.m_fileInfoB, mfi.m_fileInfoC, mfi.m_bEqualBC, bError, eqStatus );
      }
   }

   if (mfi.m_bLinkA!=mfi.m_bLinkB) mfi.m_bEqualAB=false;
   if (mfi.m_bLinkA!=mfi.m_bLinkC) mfi.m_bEqualAC=false;
   if (mfi.m_bLinkB!=mfi.m_bLinkC) mfi.m_bEqualBC=false;

   if (mfi.m_bDirA!=mfi.m_bDirB) mfi.m_bEqualAB=false;
   if (mfi.m_bDirA!=mfi.m_bDirC) mfi.m_bEqualAC=false;
   if (mfi.m_bDirB!=mfi.m_bDirC) mfi.m_bEqualBC=false;

   assert(eNew==0 && eMiddle==1 && eOld==2);

   // The map automatically sorts the keys.
   int age = eNew;
   std::map<QDateTime,int>::reverse_iterator i;
   for( i=dateMap.rbegin(); i!=dateMap.rend(); ++i )
   {
      int n = i->second;
      if ( n==0 && mfi.m_ageA==eNotThere )
      {
         mfi.m_ageA = (e_Age)age; ++age;
         if ( mfi.m_bEqualAB ) { mfi.m_ageB = mfi.m_ageA; ++age; }
         if ( mfi.m_bEqualAC ) { mfi.m_ageC = mfi.m_ageA; ++age; }
      }
      else if  ( n==1 && mfi.m_ageB==eNotThere )
      {
         mfi.m_ageB = (e_Age)age; ++age;
         if ( mfi.m_bEqualAB ) { mfi.m_ageA = mfi.m_ageB; ++age; }
         if ( mfi.m_bEqualBC ) { mfi.m_ageC = mfi.m_ageB; ++age; }
      }
      else if  ( n==2 && mfi.m_ageC==eNotThere)
      {
         mfi.m_ageC = (e_Age)age; ++age;
         if ( mfi.m_bEqualAC ) { mfi.m_ageA = mfi.m_ageC; ++age; }
         if ( mfi.m_bEqualBC ) { mfi.m_ageB = mfi.m_ageC; ++age; }
      }
   }

   // The checks below are necessary when the dates of the file are equal but the
   // files are not. One wouldn't expect this to happen, yet it happens sometimes.
   if ( mfi.m_bExistsInC && mfi.m_ageC==eNotThere )
   {
      mfi.m_ageC = (e_Age)age; ++age;
      mfi.m_bConflictingAges = true;
   }
   if ( mfi.m_bExistsInB && mfi.m_ageB==eNotThere )
   {
      mfi.m_ageB = (e_Age)age; ++age;
      mfi.m_bConflictingAges = true;
   }
   if ( mfi.m_bExistsInA && mfi.m_ageA==eNotThere )
   {
      mfi.m_ageA = (e_Age)age; ++age;
      mfi.m_bConflictingAges = true;
   }

   if ( mfi.m_ageA != eOld  && mfi.m_ageB != eOld && mfi.m_ageC != eOld )
   {
      if (mfi.m_ageA == eMiddle) mfi.m_ageA = eOld;
      if (mfi.m_ageB == eMiddle) mfi.m_ageB = eOld;
      if (mfi.m_ageC == eMiddle) mfi.m_ageC = eOld;
   }
}

static QPixmap* s_pm_dir;
static QPixmap* s_pm_file;

static void setOnePixmap( QListViewItem* pLVI, int col, e_Age eAge, bool bLink, bool bDir )
{
   #include "xpm/equal.xpm"
   #include "xpm/not_equal.xpm"
   #include "xpm/not_everywhere.xpm"
   #include "xpm/not_there.xpm"
   #include "xpm/link_arrow.xpm"

   static QPixmap pmLink( link_arrow );

   static QPixmap pmDirLink( pixCombiner( *s_pm_dir, pmLink) );
   static QPixmap pmFileLink( pixCombiner( *s_pm_file, pmLink ) );

   static QPixmap pmNotThere( not_there_pm );

   static QPixmap pmNew( equal_pm );
   static QPixmap pmOld( not_equal_pm );
   static QPixmap pmMiddle( not_everywhere_pm );

   static QPixmap pmNewLink( pixCombiner( pmNew, pmLink) );
   static QPixmap pmOldLink( pixCombiner( pmOld, pmLink) );
   static QPixmap pmMiddleLink( pixCombiner( pmMiddle, pmLink) );

   static QPixmap pmNewDir( pixCombiner2( pmNew, *s_pm_dir) );
   static QPixmap pmMiddleDir( pixCombiner2( pmMiddle, *s_pm_dir) );
   static QPixmap pmOldDir( pixCombiner2( pmOld, *s_pm_dir) );

   static QPixmap pmNewDirLink( pixCombiner( pmNewDir, pmLink) );
   static QPixmap pmMiddleDirLink( pixCombiner( pmMiddleDir, pmLink) );
   static QPixmap pmOldDirLink( pixCombiner( pmOldDir, pmLink) );

   static QPixmap* ageToPm[]=       { &pmNew,     &pmMiddle,     &pmOld,     &pmNotThere, s_pm_file  };
   static QPixmap* ageToPmLink[]=   { &pmNewLink, &pmMiddleLink, &pmOldLink, &pmNotThere, &pmFileLink };
   static QPixmap* ageToPmDir[]=    { &pmNewDir,  &pmMiddleDir,  &pmOldDir,  &pmNotThere, s_pm_dir   };
   static QPixmap* ageToPmDirLink[]={ &pmNewDirLink, &pmMiddleDirLink, &pmOldDirLink, &pmNotThere, &pmDirLink };

   QPixmap** ppPm = bDir ? ( bLink ? ageToPmDirLink : ageToPmDir ):
                           ( bLink ? ageToPmLink    : ageToPm    );

   pLVI->setPixmap( col, *ppPm[eAge] );
}

static void setPixmaps( MergeFileInfos& mfi, bool bCheckC )
{
   setOnePixmap( mfi.m_pDMI, s_nameCol, eAgeEnd,
      mfi.m_bLinkA || mfi.m_bLinkB || mfi.m_bLinkC,
      mfi.m_bDirA  || mfi.m_bDirB  || mfi.m_bDirC
      );

   if ( mfi.m_bDirA  || mfi.m_bDirB  || mfi.m_bDirC )
   {
      mfi.m_ageA=eNotThere;
      mfi.m_ageB=eNotThere;
      mfi.m_ageC=eNotThere;
      int age = eNew;
      if ( mfi.m_bExistsInC )
      {
         mfi.m_ageC = (e_Age)age;
         if (mfi.m_bEqualAC) mfi.m_ageA = (e_Age)age;
         if (mfi.m_bEqualBC) mfi.m_ageB = (e_Age)age;
         ++age;
      }
      if ( mfi.m_bExistsInB && mfi.m_ageB==eNotThere )
      {
         mfi.m_ageB = (e_Age)age;
         if (mfi.m_bEqualAB) mfi.m_ageA = (e_Age)age;
         ++age;
      }
      if ( mfi.m_bExistsInA && mfi.m_ageA==eNotThere )
      {
         mfi.m_ageA = (e_Age)age;
      }
      if ( mfi.m_ageA != eOld  && mfi.m_ageB != eOld && mfi.m_ageC != eOld )
      {
         if (mfi.m_ageA == eMiddle) mfi.m_ageA = eOld;
         if (mfi.m_ageB == eMiddle) mfi.m_ageB = eOld;
         if (mfi.m_ageC == eMiddle) mfi.m_ageC = eOld;
      }
   }

   setOnePixmap( mfi.m_pDMI, s_ACol, mfi.m_ageA, mfi.m_bLinkA, mfi.m_bDirA );
   setOnePixmap( mfi.m_pDMI, s_BCol, mfi.m_ageB, mfi.m_bLinkB, mfi.m_bDirB );
   if ( bCheckC )
      setOnePixmap( mfi.m_pDMI, s_CCol, mfi.m_ageC, mfi.m_bLinkC, mfi.m_bDirC );
}

// Iterate through the complete tree. Start by specifying QListView::firstChild().
static QListViewItem* treeIterator( QListViewItem* p, bool bVisitChildren=true )
{
   if( p!=0 )
   {
      if ( bVisitChildren && p->firstChild() != 0 )      p = p->firstChild();
      else if ( p->nextSibling() !=0 ) p = p->nextSibling();
      else
      {
         p = p->parent();
         while ( p!=0 )
         {
            if( p->nextSibling()!=0 ) { p = p->nextSibling(); break; }
            else                      { p = p->parent();             }
         }
      }
   }
   return p;
}

void DirectoryMergeWindow::prepareListView()
{
   static bool bFirstTime = true;
   if (bFirstTime)
   {
      #include "xpm/file.xpm"
      #include "xpm/folder.xpm"
      s_pm_dir = new QPixmap( m_pIconLoader->loadIcon("folder", KIcon::Small ) );
      if (s_pm_dir->size()!=QSize(16,16))
      {
         delete s_pm_dir;
         s_pm_dir = new QPixmap( folder_pm );
      }
      s_pm_file= new QPixmap( file_pm );
      bFirstTime=false;
   }

   clear();

   setRootIsDecorated( true );

   bool bCheckC = m_dirC.isValid();

   std::map<QString, MergeFileInfos>::iterator j;
   int nrOfFiles = m_fileMergeMap.size();
   int currentIdx = 1;
   QTime t;
   t.start();
   for( j=m_fileMergeMap.begin(); j!=m_fileMergeMap.end(); ++j )
   {
      const QString& fileName = j->first;
      MergeFileInfos& mfi = j->second;

      mfi.m_subPath = mfi.m_fileInfoA.exists() ? mfi.m_fileInfoA.filePath() :
                      mfi.m_fileInfoB.exists() ? mfi.m_fileInfoB.filePath() :
                      mfi.m_fileInfoC.exists() ? mfi.m_fileInfoC.filePath() :
                      QString("");

      g_pProgressDialog->setInformation(
         i18n("Processing ") + QString::number(currentIdx) +" / "+ QString::number(nrOfFiles)
         +"\n" + fileName, double(currentIdx) / nrOfFiles, false );
      if ( g_pProgressDialog->wasCancelled() ) break;
      ++currentIdx;


      // The comparisons and calculations for each file take place here.
      compareFilesAndCalcAges( mfi );

      bool bEqual = bCheckC ? mfi.m_bEqualAB && mfi.m_bEqualAC : mfi.m_bEqualAB;
      bool bDir = mfi.m_bDirA || mfi.m_bDirB || mfi.m_bDirC;

      if ( m_pOptions->m_bDmShowOnlyDeltas && !bDir && bEqual )
         continue;

      // Get dirname from fileName: Search for "/" from end:
      int pos = fileName.findRev('/');
      QString dirPart;
      QString filePart;
      if (pos==-1)
      {
         // Top dir
         filePart = fileName;
      }
      else
      {
         dirPart = fileName.left(pos);
         filePart = fileName.mid(pos+1);
      }

      if ( dirPart.isEmpty() ) // Top level
      {
         new DirMergeItem( this, filePart, &mfi );
      }
      else
      {
         MergeFileInfos& dirMfi = m_fileMergeMap[dirPart]; // parent
         assert(dirMfi.m_pDMI!=0);
         new DirMergeItem( dirMfi.m_pDMI, filePart, &mfi );
         mfi.m_pParent = &dirMfi;

         if ( !bEqual )  // Set all parents to "not equal"
         {
            MergeFileInfos* p = mfi.m_pParent;
            while(p!=0)
            {
               bool bChange = false;
               if ( !mfi.m_bEqualAB && p->m_bEqualAB ){ p->m_bEqualAB = false; bChange=true; }
               if ( !mfi.m_bEqualAC && p->m_bEqualAC ){ p->m_bEqualAC = false; bChange=true; }
               if ( !mfi.m_bEqualBC && p->m_bEqualBC ){ p->m_bEqualBC = false; bChange=true; }

               if ( bChange )
                  setPixmaps( *p, bCheckC );
               else
                  break;

               p = p->m_pParent;
            }
         }
      }

      setPixmaps( mfi, bCheckC );
   }

   if ( m_pOptions->m_bDmShowOnlyDeltas )
   {
      // Remove all equals. (Search tree depth first)
      QListViewItem* p = firstChild();
      while( p!=0 && firstChild() != 0 )
      {
         QListViewItem* pParent = p->parent();
         QListViewItem* pNextSibling = p->nextSibling();

         DirMergeItem* pDMI = static_cast<DirMergeItem*>(p);
         bool bDirEqual = bCheckC ? pDMI->m_pMFI->m_bEqualAB && pDMI->m_pMFI->m_bEqualAC
                                  : pDMI->m_pMFI->m_bEqualAB;
         if ( pDMI!=0  && pDMI->m_pMFI->m_bDirA && bDirEqual )
         {
            delete p;
            p=0;
         }

         if ( p!=0 && p->firstChild() != 0 )    p = p->firstChild();
         else if ( pNextSibling!=0 )            p = pNextSibling;
         else
         {
            p=pParent;
            while ( p!=0 )
            {
               if( p->nextSibling()!=0 ) { p = p->nextSibling(); break; }
               else                      { p = p->parent();             }
            }
         }
      }
   }
}

static bool conflictingFileTypes(MergeFileInfos& mfi)
{
   // Now check if file/dir-types fit.
   if ( mfi.m_bLinkA || mfi.m_bLinkB || mfi.m_bLinkC )
   {
      if ( mfi.m_bExistsInA && ! mfi.m_bLinkA  ||
           mfi.m_bExistsInB && ! mfi.m_bLinkB  ||
           mfi.m_bExistsInC && ! mfi.m_bLinkC )
      {
         return true;
      }
   }

   if ( mfi.m_bDirA || mfi.m_bDirB || mfi.m_bDirC )
   {
      if ( mfi.m_bExistsInA && ! mfi.m_bDirA  ||
           mfi.m_bExistsInB && ! mfi.m_bDirB  ||
           mfi.m_bExistsInC && ! mfi.m_bDirC )
      {
         return true;
      }
   }
   return false;
}

void DirectoryMergeWindow::calcSuggestedOperation( MergeFileInfos& mfi, e_MergeOperation eDefaultMergeOp )
{
   bool bCheckC = m_dirC.isValid();
   bool bCopyNewer = m_pOptions->m_bDmCopyNewer;

   if ( eDefaultMergeOp == eMergeABCToDest && !bCheckC ) { eDefaultMergeOp = eMergeABToDest; }
   if ( eDefaultMergeOp == eMergeToAB      &&  bCheckC ) { assert(false); }

   if ( eDefaultMergeOp == eMergeToA || eDefaultMergeOp == eMergeToB ||
        eDefaultMergeOp == eMergeABCToDest || eDefaultMergeOp == eMergeABToDest || eDefaultMergeOp == eMergeToAB )
   {
      if ( !bCheckC )
      {
         if ( mfi.m_bEqualAB )
         {
            mfi.setMergeOperation( eNoOperation );  // All is well, nothing to do.
         }
         else if ( mfi.m_bExistsInA && mfi.m_bExistsInB )
         {
            if ( !bCopyNewer || mfi.m_bDirA )
               mfi.setMergeOperation( eDefaultMergeOp );
            else if (  bCopyNewer && mfi.m_bConflictingAges )
            {
               mfi.setMergeOperation( eConflictingAges );
            }
            else
            {
               if ( mfi.m_ageA == eNew )
                  mfi.setMergeOperation( eDefaultMergeOp == eMergeToAB ?  eCopyAToB : eCopyAToDest );
               else
                  mfi.setMergeOperation( eDefaultMergeOp == eMergeToAB ?  eCopyBToA : eCopyBToDest );
            }
         }
         else if ( !mfi.m_bExistsInA && mfi.m_bExistsInB )
         {
            if ( eDefaultMergeOp==eMergeABToDest  ) mfi.setMergeOperation( eCopyBToDest );
            else if ( eDefaultMergeOp==eMergeToB )  mfi.setMergeOperation( eNoOperation );
            else                                    mfi.setMergeOperation( eCopyBToA );
         }
         else if ( mfi.m_bExistsInA && !mfi.m_bExistsInB )
         {
            if ( eDefaultMergeOp==eMergeABToDest  ) mfi.setMergeOperation( eCopyAToDest );
            else if ( eDefaultMergeOp==eMergeToA )  mfi.setMergeOperation( eNoOperation );
            else                                    mfi.setMergeOperation( eCopyAToB );
         }
         else //if ( !mfi.m_bExistsInA && !mfi.m_bExistsInB )
         {
            mfi.setMergeOperation( eNoOperation ); assert(false);
         }
      }
      else
      {
         if ( mfi.m_bEqualAB && mfi.m_bEqualAC )
         {
            mfi.setMergeOperation( eCopyCToDest );
         }
         else if ( mfi.m_bExistsInA && mfi.m_bExistsInB && mfi.m_bExistsInC)
         {
            if ( mfi.m_bEqualAB )
               mfi.setMergeOperation( eCopyCToDest );
            else if ( mfi.m_bEqualAC )
               mfi.setMergeOperation( eCopyBToDest );
            else if ( mfi.m_bEqualBC )
               mfi.setMergeOperation( eCopyCToDest );
            else
               mfi.setMergeOperation( eMergeABCToDest );
         }
         else if ( mfi.m_bExistsInA && mfi.m_bExistsInB && !mfi.m_bExistsInC )
         {
            if ( mfi.m_bEqualAB )
               mfi.setMergeOperation( eDeleteFromDest );
            else
               mfi.setMergeOperation( eCopyBToDest );
         }
         else if ( mfi.m_bExistsInA && !mfi.m_bExistsInB && mfi.m_bExistsInC )
         {
            if ( mfi.m_bEqualAC )
               mfi.setMergeOperation( eDeleteFromDest );
            else
               mfi.setMergeOperation( eCopyCToDest );
         }
         else if ( !mfi.m_bExistsInA && mfi.m_bExistsInB && mfi.m_bExistsInC )
         {
            if ( mfi.m_bEqualBC )
               mfi.setMergeOperation( eCopyCToDest );
            else
               mfi.setMergeOperation( eMergeABCToDest );
         }
         else if ( !mfi.m_bExistsInA && !mfi.m_bExistsInB && mfi.m_bExistsInC )
         {
            mfi.setMergeOperation( eCopyCToDest );
         }
         else if ( !mfi.m_bExistsInA && mfi.m_bExistsInB && !mfi.m_bExistsInC )
         {
            mfi.setMergeOperation( eCopyBToDest );
         }
         else if ( mfi.m_bExistsInA && !mfi.m_bExistsInB && !mfi.m_bExistsInC)
         {
            mfi.setMergeOperation( eDeleteFromDest );
         }
         else //if ( !mfi.m_bExistsInA && !mfi.m_bExistsInB && !mfi.m_bExistsInC )
         {
            mfi.setMergeOperation( eNoOperation ); assert(false);
         }
      }

      // Now check if file/dir-types fit.
      if ( conflictingFileTypes(mfi) )
      {
         mfi.setMergeOperation( eConflictingFileTypes );
      }
   }
   else
   {
      e_MergeOperation eMO = eDefaultMergeOp;
      switch ( eDefaultMergeOp )
      {
      case eConflictingFileTypes:
      case eConflictingAges:
      case eDeleteA:
      case eDeleteB:
      case eDeleteAB:
      case eDeleteFromDest:
      case eNoOperation: break;
      case eCopyAToB:    if ( !mfi.m_bExistsInA ) { eMO = eDeleteB; }        break;
      case eCopyBToA:    if ( !mfi.m_bExistsInB ) { eMO = eDeleteA; }        break;
      case eCopyAToDest: if ( !mfi.m_bExistsInA ) { eMO = eDeleteFromDest; } break;
      case eCopyBToDest: if ( !mfi.m_bExistsInB ) { eMO = eDeleteFromDest; } break;
      case eCopyCToDest: if ( !mfi.m_bExistsInC ) { eMO = eDeleteFromDest; } break;

      case eMergeToA:
      case eMergeToB:
      case eMergeToAB:
      case eMergeABCToDest:
      case eMergeABToDest:
      default:
         assert(false);
      }
      mfi.setMergeOperation( eMO );
   }
}

void DirectoryMergeWindow::onDoubleClick( QListViewItem* lvi )
{
   if (lvi==0) return;

   if ( m_bDirectoryMerge )
      mergeCurrentFile();
   else
      compareCurrentFile();
}

void DirectoryMergeWindow::onSelectionChanged( QListViewItem* lvi )
{
   if ( lvi==0 ) return;

   DirMergeItem* pDMI = static_cast<DirMergeItem*>(lvi);

   MergeFileInfos& mfi = *pDMI->m_pMFI;
   assert( mfi.m_pDMI==pDMI );

   m_pDirectoryMergeInfo->setInfo( m_dirA, m_dirB, m_dirC, m_dirDestInternal, mfi );
}

void DirectoryMergeWindow::onClick( QListViewItem* lvi, const QPoint& p, int c )
{
   if ( lvi==0 ) return;

   DirMergeItem* pDMI = static_cast<DirMergeItem*>(lvi);

   MergeFileInfos& mfi = *pDMI->m_pMFI;
   assert( mfi.m_pDMI==pDMI );

   if ( c!=s_OpCol ) return;

   bool bThreeDirs = m_dirC.isValid();

   KPopupMenu m(this);
   if ( bThreeDirs )
   {
      dirCurrentDoNothing->plug(&m);
      int count=0;
      if ( mfi.m_bExistsInA ) { dirCurrentChooseA->plug(&m); ++count;  }
      if ( mfi.m_bExistsInB ) { dirCurrentChooseB->plug(&m); ++count;  }
      if ( mfi.m_bExistsInC ) { dirCurrentChooseC->plug(&m); ++count;  }
      if ( !conflictingFileTypes(mfi) && count>1 ) dirCurrentMerge->plug(&m);
      dirCurrentDelete->plug(&m);
   }
   else if ( m_bSyncMode )
   {
      dirCurrentSyncDoNothing->plug(&m);
      if ( mfi.m_bExistsInA ) dirCurrentSyncCopyAToB->plug(&m);
      if ( mfi.m_bExistsInB ) dirCurrentSyncCopyBToA->plug(&m);
      if ( mfi.m_bExistsInA ) dirCurrentSyncDeleteA->plug(&m);
      if ( mfi.m_bExistsInB ) dirCurrentSyncDeleteB->plug(&m);
      if ( mfi.m_bExistsInA && mfi.m_bExistsInB )
      {
         dirCurrentSyncDeleteAAndB->plug(&m);
         if ( !conflictingFileTypes(mfi))
         {
            dirCurrentSyncMergeToA->plug(&m);
            dirCurrentSyncMergeToB->plug(&m);
            dirCurrentSyncMergeToAAndB->plug(&m);
         }
      }
   }
   else
   {
      dirCurrentDoNothing->plug(&m);
      if ( mfi.m_bExistsInA ) { dirCurrentChooseA->plug(&m); }
      if ( mfi.m_bExistsInB ) { dirCurrentChooseB->plug(&m); }
      if ( !conflictingFileTypes(mfi) && mfi.m_bExistsInA  &&  mfi.m_bExistsInB ) dirCurrentMerge->plug(&m);
      dirCurrentDelete->plug(&m);
   }

   m.exec( p );
}

// Since Qt 2.3.0 doesn't allow the specification of a compare operator, this trick emulates it.
#if QT_VERSION==230
#define DIRSORT(x) ( pMFI->m_bDirA ? " " : "" )+x
#else
#define DIRSORT(x) x
#endif

DirMergeItem::DirMergeItem( QListView* pParent, const QString& fileName, MergeFileInfos* pMFI )
: QListViewItem( pParent, DIRSORT( fileName ), "","","", i18n("To do.") )
{
   pMFI->m_pDMI = this;
   m_pMFI = pMFI;
}

DirMergeItem::DirMergeItem( DirMergeItem* pParent, const QString& fileName, MergeFileInfos* pMFI )
: QListViewItem( pParent, DIRSORT( fileName ), "","","", i18n("To do.") )
{
   pMFI->m_pDMI = this;
   m_pMFI = pMFI;
}

DirMergeItem::~DirMergeItem()
{
   m_pMFI->m_pDMI = 0;
}

void MergeFileInfos::setMergeOperation( e_MergeOperation eMOp )
{
   if ( eMOp != m_eMergeOperation )
   {
      m_bOperationComplete = false;
      m_pDMI->setText( s_OpStatusCol, "" );
   }

   m_eMergeOperation = eMOp;
   QString s;
   bool bDir = m_bDirA || m_bDirB || m_bDirC;
   if( m_pDMI!=0 )
   {
      switch( m_eMergeOperation )
      {
      case eNoOperation:      s=""; m_pDMI->setText(s_OpCol,""); break;
      case eCopyAToB:         s=i18n("Copy A to B");     break;
      case eCopyBToA:         s=i18n("Copy B to A");     break;
      case eDeleteA:          s=i18n("Delete A");        break;
      case eDeleteB:          s=i18n("Delete B");        break;
      case eDeleteAB:         s=i18n("Delete A & B");    break;
      case eMergeToA:         s=i18n("Merge to A");      break;
      case eMergeToB:         s=i18n("Merge to B");      break;
      case eMergeToAB:        s=i18n("Merge to A & B");  break;
      case eCopyAToDest:      s="A";    break;
      case eCopyBToDest:      s="B";    break;
      case eCopyCToDest:      s="C";    break;
      case eDeleteFromDest:   s=i18n("Delete (if exists)");  break;
      case eMergeABCToDest:   s= bDir ? i18n("Merge") : i18n("Merge (manual)");    break;
      case eMergeABToDest:    s= bDir ? i18n("Merge") : i18n("Merge (manual)");    break;
      case eConflictingFileTypes: s=i18n("Error: Conflicting File Types");         break;
      case eConflictingAges:  s=i18n("Error: Dates are equal but files are not."); break;
      default:                assert(false); break;
      }
      m_pDMI->setText(s_OpCol,s);

      e_MergeOperation eChildrenMergeOp = m_eMergeOperation;
      if ( eChildrenMergeOp == eConflictingFileTypes ) eChildrenMergeOp = eMergeABCToDest;
      QListViewItem* p = m_pDMI->firstChild();
      while ( p!=0 )
      {
         DirMergeItem* pDMI = static_cast<DirMergeItem*>( p );
         DirectoryMergeWindow* pDMW = static_cast<DirectoryMergeWindow*>( p->listView() );
         pDMW->calcSuggestedOperation( *pDMI->m_pMFI, eChildrenMergeOp );
         p = p->nextSibling();
      }
   }
}

void DirectoryMergeWindow::compareCurrentFile()
{
   if (!canContinue()) return;

   if ( m_bRealMergeStarted )
   {
      KMessageBox::sorry(this,i18n("This operation is currently not possible."),i18n("Operation Not Possible"));
      return;
   }

   DirMergeItem* pDMI = static_cast<DirMergeItem*>( selectedItem() );
   if ( pDMI != 0 )
   {
      MergeFileInfos& mfi = *pDMI->m_pMFI;
      if ( !(mfi.m_bDirA || mfi.m_bDirB || mfi.m_bDirC) )
      {
         emit startDiffMerge(
            mfi.m_bExistsInA ? mfi.m_fileInfoA.absFilePath() : QString(""),
            mfi.m_bExistsInB ? mfi.m_fileInfoB.absFilePath() : QString(""),
            mfi.m_bExistsInC ? mfi.m_fileInfoC.absFilePath() : QString(""),
            "",
            "","",""
            );
      }
   }
   emit updateAvailabilities();
}



bool DirectoryMergeWindow::isFileSelected()
{
   DirMergeItem* pDMI = static_cast<DirMergeItem*>( selectedItem() );
   if ( pDMI != 0 )
   {
      MergeFileInfos& mfi = *pDMI->m_pMFI;
      return ! (mfi.m_bDirA || mfi.m_bDirB || mfi.m_bDirC || conflictingFileTypes(mfi) );
   }
   return false;
}

void DirectoryMergeWindow::mergeResultSaved(const QString& fileName)
{
   DirMergeItem* pCurrentItemForOperation = (m_mergeItemList.empty() || m_currentItemForOperation==m_mergeItemList.end() )
                                               ? 0
                                               : *m_currentItemForOperation;

   if ( pCurrentItemForOperation!=0 && pCurrentItemForOperation->m_pMFI==0 )
   {
      KMessageBox::error( this, i18n("This should never happen: \n\nmergeResultSaved: m_pMFI=0\n\nIf you know how to reproduce this, please contact the program author."),i18n("Program Error") );
      return;
   }
   if ( pCurrentItemForOperation!=0 && fileName == fullNameDest(*pCurrentItemForOperation->m_pMFI) )
   {
      if ( pCurrentItemForOperation->m_pMFI->m_eMergeOperation==eMergeToAB )
      {
         MergeFileInfos& mfi = *pCurrentItemForOperation->m_pMFI;
         bool bSuccess = copyFLD( fullNameB(mfi), fullNameA(mfi) );
         if (!bSuccess)
         {
            KMessageBox::error(this, i18n("An error occurred while copying.\n"), i18n("Error") );
            m_pStatusInfo->setCaption(i18n("Merge Error"));
            m_pStatusInfo->show();
            if ( m_pStatusInfo->firstChild()!=0 )
               m_pStatusInfo->ensureItemVisible( m_pStatusInfo->last() );
            m_bError = true;
            pCurrentItemForOperation->setText( s_OpStatusCol, i18n("Error.") );
            mfi.m_eMergeOperation = eCopyBToA;
            return;
         }
      }
      pCurrentItemForOperation->setText( s_OpStatusCol, i18n("Done.") );
      pCurrentItemForOperation->m_pMFI->m_bOperationComplete = true;
      if ( m_mergeItemList.size()==1 )
      {
         m_mergeItemList.clear();
         m_bRealMergeStarted=false;
      }
   }

   emit updateAvailabilities();
}

bool DirectoryMergeWindow::canContinue()
{
   bool bCanContinue=false;
   checkIfCanContinue( &bCanContinue );
   if ( bCanContinue && !m_bError )
   {
      DirMergeItem* pCurrentItemForOperation =
         (m_mergeItemList.empty() || m_currentItemForOperation==m_mergeItemList.end() ) ? 0 : *m_currentItemForOperation;

      if ( pCurrentItemForOperation!=0  && ! pCurrentItemForOperation->m_pMFI->m_bOperationComplete )
      {
         pCurrentItemForOperation->setText( s_OpStatusCol, i18n("Not saved.") );
         pCurrentItemForOperation->m_pMFI->m_bOperationComplete = true;
         if ( m_mergeItemList.size()==1 )
         {
            m_mergeItemList.clear();
            m_bRealMergeStarted=false;
         }
      }
   }
   return bCanContinue;
}

bool DirectoryMergeWindow::executeMergeOperation( MergeFileInfos& mfi, bool& bSingleFileMerge )
{
   bool bCreateBackups = m_pOptions->m_bDmCreateBakFiles;
   // First decide destname
   QString destName;
   switch( mfi.m_eMergeOperation )
   {
   case eNoOperation: break;
   case eDeleteAB:    break;
   case eMergeToAB:   // let the user save in B. In mergeResultSaved() the file will be copied to A.
   case eMergeToB:
   case eDeleteB:
   case eCopyAToB:    destName = fullNameB(mfi); break;
   case eMergeToA:
   case eDeleteA:
   case eCopyBToA:    destName = fullNameA(mfi); break;
   case eMergeABToDest:
   case eMergeABCToDest:
   case eCopyAToDest:
   case eCopyBToDest:
   case eCopyCToDest:
   case eDeleteFromDest: destName = fullNameDest(mfi); break;
   default:
      KMessageBox::error( this, i18n("Unknown merge operation. (This must never happen!)"), i18n("Error") );
      assert(false);
   }

   bool bSuccess = false;
   bSingleFileMerge = false;
   switch( mfi.m_eMergeOperation )
   {
   case eNoOperation: bSuccess = true; break;
   case eCopyAToDest:
   case eCopyAToB:    bSuccess = copyFLD( fullNameA(mfi), destName ); break;
   case eCopyBToDest:
   case eCopyBToA:    bSuccess = copyFLD( fullNameB(mfi), destName ); break;
   case eCopyCToDest: bSuccess = copyFLD( fullNameC(mfi), destName ); break;
   case eDeleteFromDest:
   case eDeleteA:
   case eDeleteB:     bSuccess = deleteFLD( destName, bCreateBackups ); break;
   case eDeleteAB:    bSuccess = deleteFLD( fullNameA(mfi), bCreateBackups ) &&
                                 deleteFLD( fullNameB(mfi), bCreateBackups ); break;
   case eMergeABToDest:
   case eMergeToA:
   case eMergeToAB:
   case eMergeToB:      bSuccess = mergeFLD( fullNameA(mfi), fullNameB(mfi), "",
                                             destName, bSingleFileMerge );
                        break;
   case eMergeABCToDest:bSuccess = mergeFLD(
                                    mfi.m_bExistsInA ? fullNameA(mfi) : QString(""),
                                    mfi.m_bExistsInB ? fullNameB(mfi) : QString(""),
                                    mfi.m_bExistsInC ? fullNameC(mfi) : QString(""),
                                    destName, bSingleFileMerge );
                        break;
   default:
      KMessageBox::error( this, i18n("Unknown merge operation."), i18n("Error") );
      assert(false);
   }

   return bSuccess;
}


// Check if the merge can start, and prepare the m_mergeItemList which then contains all
// items that must be merged.
void DirectoryMergeWindow::prepareMergeStart( QListViewItem* pBegin, QListViewItem* pEnd, bool bVerbose )
{
   if ( bVerbose )
   {
      int status = KMessageBox::warningYesNoCancel(this,
         i18n("The merge is about to begin.\n\n"
         "Choose \"Do it\" if you have read the instructions and know what you are doing.\n"
         "Choosing \"Simulate it\" will tell you what would happen.\n\n"
         "Be aware that this program still has beta status "
         "and there is NO WARRANTY whatsoever! Make backups of your vital data!"),
         i18n("Starting Merge"), i18n("Do It"), i18n("Simulate It") );
      if (status==KMessageBox::Yes)      m_bRealMergeStarted = true;
      else if (status==KMessageBox::No ) m_bSimulatedMergeStarted = true;
      else return;
   }
   else
   {
      m_bRealMergeStarted = true;
   }

   m_mergeItemList.clear();
   if (pBegin == 0)
      return;

   for( QListViewItem* p = pBegin; p!= pEnd; p = treeIterator( p ) )
   {
      DirMergeItem* pDMI = static_cast<DirMergeItem*>(p);

      if ( ! pDMI->m_pMFI->m_bOperationComplete )
      {
         m_mergeItemList.push_back(pDMI);

         if (pDMI!=0 && pDMI->m_pMFI->m_eMergeOperation == eConflictingFileTypes )
         {
            ensureItemVisible( pDMI );
            setSelected( pDMI, true );
            KMessageBox::error(this, i18n("The highlighted item has a different type in the different directories. Select what to do."), i18n("Error"));
            m_mergeItemList.clear();
            m_bRealMergeStarted=false;
            return;
         }
         if (pDMI!=0 && pDMI->m_pMFI->m_eMergeOperation == eConflictingAges )
         {
            ensureItemVisible( pDMI );
            setSelected( pDMI, true );
            KMessageBox::error(this, i18n("The modification dates of the file are equal but the files are not. Select what to do."), i18n("Error"));
            m_mergeItemList.clear();
            m_bRealMergeStarted=false;
            return;
         }
      }
   }

   m_currentItemForOperation = m_mergeItemList.begin();
   return;
}

void DirectoryMergeWindow::slotRunOperationForCurrentItem()
{
   if ( ! canContinue() ) return;

   bool bVerbose = false;
   if ( m_mergeItemList.empty() )
   {
      QListViewItem* pBegin = currentItem();
      QListViewItem* pEnd = pBegin;
      while ( pEnd!=0 && pEnd->nextSibling()==0 )
      {
         pEnd = pEnd->parent();
      }
      if ( pEnd!=0 ) 
         pEnd=pEnd->nextSibling();

      prepareMergeStart( pBegin, pEnd, bVerbose );
      mergeContinue(true, bVerbose);
   }
   else
      mergeContinue(false, bVerbose);
}

void DirectoryMergeWindow::slotRunOperationForAllItems()
{
   if ( ! canContinue() ) return;

   bool bVerbose = true;
   if ( m_mergeItemList.empty() )
   {
      QListViewItem* pBegin = firstChild();

      prepareMergeStart( pBegin, 0, bVerbose );
      mergeContinue(true, bVerbose);
   }
   else
      mergeContinue(false, bVerbose);
}

void DirectoryMergeWindow::mergeCurrentFile()
{
   if (!canContinue()) return;

   if ( m_bRealMergeStarted )
   {
      KMessageBox::sorry(this,i18n("This operation is currently not possible because dir merge currently runs."),i18n("Operation Not Possible"));
      return;
   }

   if ( isFileSelected() )
   {
      DirMergeItem* pDMI = static_cast<DirMergeItem*>( selectedItem() );
      if ( pDMI != 0 )
      {
         MergeFileInfos& mfi = *pDMI->m_pMFI;
         m_mergeItemList.clear();
         m_mergeItemList.push_back( pDMI );
         m_currentItemForOperation=m_mergeItemList.begin();
         bool bDummy=false;
         mergeFLD(
            mfi.m_bExistsInA ? mfi.m_fileInfoA.absFilePath() : QString(""),
            mfi.m_bExistsInB ? mfi.m_fileInfoB.absFilePath() : QString(""),
            mfi.m_bExistsInC ? mfi.m_fileInfoC.absFilePath() : QString(""),
            fullNameDest(mfi),
            bDummy
            );
      }
   }
   emit updateAvailabilities();
}


// When bStart is true then m_currentItemForOperation must still be processed.
// When bVerbose is true then a messagebox will tell when the merge is complete.
void DirectoryMergeWindow::mergeContinue(bool bStart, bool bVerbose)
{
   if ( m_mergeItemList.empty() )
      return;

   int nrOfItems = 0;
   int nrOfCompletedItems = 0;
   int nrOfCompletedSimItems = 0;

   // Count the number of completed items (for the progress bar).
   for( MergeItemList::iterator i = m_mergeItemList.begin(); i!=m_mergeItemList.end(); ++i )
   {
      DirMergeItem* pDMI = *i;
      ++nrOfItems;
      if ( pDMI->m_pMFI->m_bOperationComplete )
         ++nrOfCompletedItems;
      if ( pDMI->m_pMFI->m_bSimOpComplete )
         ++nrOfCompletedSimItems;
   }

   m_pStatusInfo->hide();
   m_pStatusInfo->clear();

   DirMergeItem* pCurrentItemForOperation = m_currentItemForOperation==m_mergeItemList.end() ? 0 : *m_currentItemForOperation;

   bool bContinueWithCurrentItem = bStart;  // true for first item, else false
   bool bSkipItem = false;
   if ( !bStart && m_bError && pCurrentItemForOperation!=0 )
   {
      int status = KMessageBox::warningYesNoCancel(this,
         i18n("There was an error in the last step.\n"
         "Do you want to continue with the item that caused the error or do you want to skip this item?"),
         i18n("Continue merge after an error"), i18n("Continue With Last Item"), i18n("Skip Item") );
      if      (status==KMessageBox::Yes) bContinueWithCurrentItem = true;
      else if (status==KMessageBox::No ) bSkipItem = true;
      else return;
      m_bError = false;
   }

   g_pProgressDialog->start();

   bool bSuccess = true;
   bool bSingleFileMerge = false;
   bool bSim = m_bSimulatedMergeStarted;
   while( bSuccess )
   {
      if ( pCurrentItemForOperation==0 )
      {
         m_mergeItemList.clear();
         m_bRealMergeStarted=false;
         break;
      }

      if ( pCurrentItemForOperation!=0 && !bContinueWithCurrentItem )
      {
         if ( bSim )
         {
            if( pCurrentItemForOperation->firstChild()==0 )
            {
               pCurrentItemForOperation->m_pMFI->m_bSimOpComplete = true;
            }
         }
         else
         {
            if( pCurrentItemForOperation->firstChild()==0 )
            {
               if( !pCurrentItemForOperation->m_pMFI->m_bOperationComplete )
               {
                  pCurrentItemForOperation->setText( s_OpStatusCol, bSkipItem ? i18n("Skipped.") : i18n("Done.") );
                  pCurrentItemForOperation->m_pMFI->m_bOperationComplete = true;
                  bSkipItem = false;
               }
            }
            else
            {
               pCurrentItemForOperation->setText( s_OpStatusCol, i18n("In progress...") );
            }
         }
      }

      if ( ! bContinueWithCurrentItem )
      {
         // Depth first
         QListViewItem* pPrevItem = pCurrentItemForOperation;
         ++m_currentItemForOperation;
         pCurrentItemForOperation = m_currentItemForOperation==m_mergeItemList.end() ? 0 : *m_currentItemForOperation;
         if ( (pCurrentItemForOperation==0 || pCurrentItemForOperation->parent()!=pPrevItem->parent()) && pPrevItem->parent()!=0 )
         {
            // Check if the parent may be set to "Done"
            QListViewItem* pParent = pPrevItem->parent();
            bool bDone = true;
            while ( bDone && pParent!=0 )
            {
               for( QListViewItem* p = pParent->firstChild(); p!=0; p=p->nextSibling() )
               {
                  DirMergeItem* pDMI = static_cast<DirMergeItem*>(p);
                  if ( !bSim && ! pDMI->m_pMFI->m_bOperationComplete   ||  bSim && pDMI->m_pMFI->m_bSimOpComplete )
                  {
                     bDone=false;
                     break;
                  }
               }
               if ( bDone )
               {
                  if (bSim)
                     static_cast<DirMergeItem*>(pParent)->m_pMFI->m_bSimOpComplete = bDone;
                  else
                  {
                     pParent->setText( s_OpStatusCol, i18n("Done.") );
                     static_cast<DirMergeItem*>(pParent)->m_pMFI->m_bOperationComplete = bDone;
                  }
               }
               pParent = pParent->parent();
            }
         }
      }

      if ( pCurrentItemForOperation == 0 ) // end?
      {
         if ( m_bRealMergeStarted )
         {
            if (bVerbose)
            {
               KMessageBox::information( this, i18n("Merge operation complete."), i18n("Merge Complete") );
            }
            m_bRealMergeStarted = false;
            m_pStatusInfo->setCaption(i18n("Merge Complete"));
         }
         if ( m_bSimulatedMergeStarted )
         {
            m_bSimulatedMergeStarted = false;
            for( QListViewItem* p=firstChild(); p!=0; p=treeIterator(p) )
            {
               static_cast<DirMergeItem*>(p)->m_pMFI->m_bSimOpComplete = false;
            }
            m_pStatusInfo->setCaption(i18n("Simulated merge complete: Check if you agree with the proposed operations."));
            m_pStatusInfo->show();
         }
         g_pProgressDialog->hide();
         m_mergeItemList.clear();
         m_bRealMergeStarted=false;
         return;
      }

      MergeFileInfos& mfi = *pCurrentItemForOperation->m_pMFI;

      g_pProgressDialog->setInformation( mfi.m_subPath,
         bSim ? double(nrOfCompletedSimItems)/nrOfItems : double(nrOfCompletedItems)/nrOfItems,
         false // bRedrawUpdate
         );
      g_pProgressDialog->show();

      bSuccess = executeMergeOperation( mfi, bSingleFileMerge );  // Here the real operation happens.

      if ( bSuccess )
      {
         if(bSim) ++nrOfCompletedSimItems;
         else     ++nrOfCompletedItems;
         bContinueWithCurrentItem = false;
      }

      if( g_pProgressDialog->wasCancelled() )
         break;
   }  // end while

   g_pProgressDialog->hide();

   setCurrentItem( pCurrentItemForOperation );
   ensureItemVisible( pCurrentItemForOperation );
   if ( !bSuccess &&  !bSingleFileMerge )
   {
      KMessageBox::error(this, i18n("An error occurred. Press OK to see detailed information.\n"), i18n("Error") );
      m_pStatusInfo->setCaption(i18n("Merge Error"));
      m_pStatusInfo->show();
      if ( m_pStatusInfo->firstChild()!=0 )
         m_pStatusInfo->ensureItemVisible( m_pStatusInfo->last() );
      m_bError = true;
      pCurrentItemForOperation->setText( s_OpStatusCol, i18n("Error.") );
   }
   else
   {
      m_bError = false;
   }
   emit updateAvailabilities();

   if ( m_currentItemForOperation==m_mergeItemList.end() )
   {
      m_mergeItemList.clear();
      m_bRealMergeStarted=false;
   }
}

void DirectoryMergeWindow::allowResizeEvents(bool bAllowResizeEvents )
{
   m_bAllowResizeEvents = bAllowResizeEvents;
}

void DirectoryMergeWindow::resizeEvent( QResizeEvent* e )
{
   if (m_bAllowResizeEvents)
      QListView::resizeEvent(e);
}

bool DirectoryMergeWindow::deleteFLD( const QString& name, bool bCreateBackup )
{
   FileAccess fi(name, true);
   if ( !fi.exists() )
      return true;

   if ( bCreateBackup )
   {
      bool bSuccess = renameFLD( name, name+".orig" );
      if (!bSuccess)
      {
         m_pStatusInfo->addText( i18n("Error: While deleting %1: Creating backup failed.").arg(name) );
         return false;
      }
   }
   else
   {
      if ( fi.isDir() && !fi.isSymLink() )
         m_pStatusInfo->addText(i18n("delete directory recursively( %1 )").arg(name));
      else
         m_pStatusInfo->addText(i18n("delete( %1 )").arg(name));

      if ( m_bSimulatedMergeStarted )
      {
         return true;
      }

      if ( fi.isDir() && !fi.isSymLink() )// recursive directory delete only for real dirs, not symlinks
      {
         t_DirectoryList dirList;
         bool bSuccess = fi.listDir( &dirList, false, true, "*", "", "", false, false );  // not recursive, find hidden files

         if ( !bSuccess )
         {
             // No Permission to read directory or other error.
             m_pStatusInfo->addText( i18n("Error: delete dir operation failed while trying to read the directory.") );
             return false;
         }

         t_DirectoryList::iterator it;      // create list iterator

         for ( it=dirList.begin(); it!=dirList.end(); ++it )       // for each file...
         {
            FileAccess& fi2 = *it;
            if ( fi2.fileName() == "." ||  fi2.fileName()==".." )
               continue;
            bSuccess = deleteFLD( fi2.absFilePath(), false );
            if (!bSuccess) break;
         }
         if (bSuccess)
         {
            bSuccess = FileAccess::removeDir( name );
            if ( !bSuccess )
            {
               m_pStatusInfo->addText( i18n("Error: rmdir( %1 ) operation failed.").arg(name));
               return false;
            }
         }
      }
      else
      {
         bool bSuccess = FileAccess::removeFile( name );
         if ( !bSuccess )
         {
            m_pStatusInfo->addText( i18n("Error: delete operation failed.") );
            return false;
         }
      }
   }
   return true;
}

bool DirectoryMergeWindow::mergeFLD( const QString& nameA,const QString& nameB,const QString& nameC,const QString& nameDest, bool& bSingleFileMerge )
{
   FileAccess fi(nameA);
   if (fi.isDir())
   {
      return makeDir(nameDest);
   }

   // Make sure that the dir exists, into which we will save the file later.
   int pos=nameDest.findRev('/');
   if ( pos>0 )
   {
      QString parentName = nameDest.left(pos);
      bool bSuccess = makeDir(parentName, true /*quiet*/);
      if (!bSuccess)
         return false;
   }

   m_pStatusInfo->addText(i18n("manual merge( %1, %2, %3 -> %4)").arg(nameA).arg(nameB).arg(nameC).arg(nameDest));
   if ( m_bSimulatedMergeStarted )
   {
      m_pStatusInfo->addText(i18n("     Note: After a manual merge the user should continue via F7.") );
      return true;
   }

   bSingleFileMerge = true;
   (*m_currentItemForOperation)->setText( s_OpStatusCol, i18n("In progress...") );
   ensureItemVisible( *m_currentItemForOperation );

   emit startDiffMerge( nameA, nameB, nameC, nameDest, "","","" );

   return false;
}

bool DirectoryMergeWindow::copyFLD( const QString& srcName, const QString& destName )
{
   if ( srcName == destName )
      return true;

   if ( FileAccess(destName, true).exists() )
   {
      bool bSuccess = deleteFLD( destName, m_pOptions->m_bDmCreateBakFiles );
      if ( !bSuccess )
      {
         m_pStatusInfo->addText(i18n("Error: copy( %1 -> %2 ) failed."
            "Deleting existing destination failed.").arg(srcName).arg(destName));
         return false;
      }
   }

   FileAccess fi( srcName );

   if ( fi.isSymLink() && (fi.isDir() && !m_bFollowDirLinks  ||  !fi.isDir() && !m_bFollowDirLinks) )
   {
      m_pStatusInfo->addText(i18n("copyLink( %1 -> %2 )").arg(srcName).arg(destName));
#ifdef _WIN32
      // What are links?
#else
      if ( m_bSimulatedMergeStarted )
      {
         return true;
      }
      FileAccess destFi(destName);
      if ( !destFi.isLocal() || !fi.isLocal() )
      {
         m_pStatusInfo->addText(i18n("Error: copyLink failed: Remote links are not yet supported."));
         return false;
      }
      QString linkTarget = fi.readLink();
      bool bSuccess = FileAccess::symLink( linkTarget, destName );
      if (!bSuccess)
         m_pStatusInfo->addText(i18n("Error: copyLink failed."));
      return bSuccess;
#endif
   }

   if ( fi.isDir() )
   {
      bool bSuccess = makeDir( destName );
      return bSuccess;
   }

   int pos=destName.findRev('/');
   if ( pos>0 )
   {
      QString parentName = destName.left(pos);
      bool bSuccess = makeDir(parentName, true /*quiet*/);
      if (!bSuccess)
         return false;
   }

   m_pStatusInfo->addText(i18n("copy( %1 -> %2 )").arg(srcName).arg(destName));

   if ( m_bSimulatedMergeStarted )
   {
      return true;
   }

   FileAccess faSrc ( srcName );
   bool bSuccess = faSrc.copyFile( destName );
   if (! bSuccess ) m_pStatusInfo->addText( faSrc.getStatusText() );
   return bSuccess;
}

// Rename is not an operation that can be selected by the user.
// It will only be used to create backups.
// Hence it will delete an existing destination without making a backup (of the old backup.)
bool DirectoryMergeWindow::renameFLD( const QString& srcName, const QString& destName )
{
   if ( srcName == destName )
      return true;

   if ( FileAccess(destName, true).exists() )
   {
      bool bSuccess = deleteFLD( destName, false /*no backup*/ );
      if (!bSuccess)
      {
         m_pStatusInfo->addText( i18n("Error during rename( %1 -> %2 ): "
                             "Cannot delete existing destination." ).arg(srcName).arg(destName));
         return false;
      }
   }

   m_pStatusInfo->addText(i18n("rename( %1 -> %2 )").arg(srcName).arg(destName))	;
   if ( m_bSimulatedMergeStarted )
   {
      return true;
   }

   bool bSuccess = FileAccess( srcName ).rename( destName );
   if (!bSuccess)
   {
      m_pStatusInfo->addText( i18n("Error: Rename failed.") );
      return false;
   }

   return true;
}

bool DirectoryMergeWindow::makeDir( const QString& name, bool bQuiet )
{
   FileAccess fi(name, true);
   if( fi.exists() && fi.isDir() )
      return true;

   if( fi.exists() && !fi.isDir() )
   {
      bool bSuccess = deleteFLD( name, true );
      if (!bSuccess)
      {
         m_pStatusInfo->addText( i18n("Error during makeDir of %1. "
                             "Cannot delete existing file." ).arg(name));
         return false;
      }
   }

   int pos=name.findRev('/');
   if ( pos>0 )
   {
      QString parentName = name.left(pos);
      bool bSuccess = makeDir(parentName,true);
      if (!bSuccess)
         return false;
   }

   if ( ! bQuiet )
      m_pStatusInfo->addText(i18n("makeDir( %1 )").arg(name));

   if ( m_bSimulatedMergeStarted )
   {
      return true;
   }

   bool bSuccess = FileAccess::makeDir( name );
   if ( bSuccess == false )
   {
      m_pStatusInfo->addText( i18n("Error while creating directory.") );
      return false;
   }
   return true;
}


DirectoryMergeInfo::DirectoryMergeInfo( QWidget* pParent )
: QFrame(pParent)
{
   QVBoxLayout *topLayout = new QVBoxLayout( this );

   QGridLayout *grid = new QGridLayout( topLayout );
   grid->setColStretch(1,10);

   int line=0;

   m_pA = new QLabel("A",this);        grid->addWidget( m_pA,line, 0 );
   m_pInfoA = new QLabel(this);        grid->addWidget( m_pInfoA,line,1 ); ++line;
   m_pB = new QLabel("B",this);        grid->addWidget( m_pB,line, 0 );
   m_pInfoB = new QLabel(this);        grid->addWidget( m_pInfoB,line,1 ); ++line;
   m_pC = new QLabel("C",this);        grid->addWidget( m_pC,line, 0 );
   m_pInfoC = new QLabel(this);        grid->addWidget( m_pInfoC,line,1 ); ++line;
   m_pDest = new QLabel(i18n("Dest"),this);  grid->addWidget( m_pDest,line, 0 );
   m_pInfoDest = new QLabel(this);     grid->addWidget( m_pInfoDest,line,1 ); ++line;

   m_pInfoList = new QListView(this);  topLayout->addWidget( m_pInfoList );
   m_pInfoList->addColumn(i18n("Dir"));
   m_pInfoList->addColumn(i18n("Type"));
   m_pInfoList->addColumn(i18n("Size"));
   m_pInfoList->addColumn(i18n("Attr"));
   m_pInfoList->addColumn(i18n("Last Modification"));
   m_pInfoList->addColumn(i18n("Link-Destination"));
   setMinimumSize( 100,100 );
}

static void addListViewItem( QListView* pListView, const QString& dir,
   const QString& basePath, FileAccess& fi )
{
   if ( basePath.isEmpty() )
   {
      return;
   }
   else
   {
      if ( fi.exists() )
      {
#if QT_VERSION==230
         QString dateString = fi.lastModified().toString();
#else
         QString dateString = fi.lastModified().toString("yyyy-MM-dd hh:mm:ss");
#endif

         new QListViewItem(
            pListView,
            dir,
            QString( fi.isDir() ? i18n("Dir") : i18n("File") ) + (fi.isSymLink() ? "-Link" : ""),
            QString::number(fi.size()),
            QString(fi.isReadable() ? "r" : " ") + (fi.isWritable()?"w" : " ")
#ifdef _WIN32
            /*Future: Use GetFileAttributes()*/,
#else
            + (fi.isExecutable()?"x" : " "),
#endif
            dateString,
            QString(fi.isSymLink() ? (" -> " + fi.readLink()) : QString(""))
            );
      }
      else
      {
         new QListViewItem(
            pListView,
            dir,
            i18n("not available"),
            "",
            "",
            "",
            ""
            );
      }
   }
}

void DirectoryMergeInfo::setInfo(
   const FileAccess& dirA,
   const FileAccess& dirB,
   const FileAccess& dirC,
   const FileAccess& dirDest,
   MergeFileInfos& mfi )
{
   bool bHideDest = false;
   if ( dirA.absFilePath()==dirDest.absFilePath() )
   {
      m_pA->setText( i18n("A (Dest): ") );  bHideDest=true;
   }
   else
      m_pA->setText( !dirC.isValid() ? QString("A:    ") : i18n("A (Base): "));

   m_pInfoA->setText( dirA.prettyAbsPath() );

   if ( dirB.absFilePath()==dirDest.absFilePath() )
   {
      m_pB->setText( i18n("B (Dest): ") );  bHideDest=true;
   }
   else
      m_pB->setText( "B:    " );
   m_pInfoB->setText( dirB.prettyAbsPath() );

   if ( dirC.absFilePath()==dirDest.absFilePath() )
   {
      m_pC->setText( i18n("C (Dest): ") );  bHideDest=true;
   }
   else
      m_pC->setText( "C:    " );
   m_pInfoC->setText( dirC.prettyAbsPath() );

   m_pDest->setText( i18n("Dest: ") ); m_pInfoDest->setText( dirDest.prettyAbsPath() );

   if (!dirC.isValid())    { m_pC->hide(); m_pInfoC->hide();   }
   else                     { m_pC->show(); m_pInfoC->show();   }

   if (!dirDest.isValid()||bHideDest) { m_pDest->hide(); m_pInfoDest->hide(); }
   else                                { m_pDest->show(); m_pInfoDest->show(); }

   m_pInfoList->clear();
   addListViewItem( m_pInfoList, "A", dirA.prettyAbsPath(), mfi.m_fileInfoA );
   addListViewItem( m_pInfoList, "B", dirB.prettyAbsPath(), mfi.m_fileInfoB );
   addListViewItem( m_pInfoList, "C", dirC.prettyAbsPath(), mfi.m_fileInfoC );
   if (!bHideDest)
   {
      FileAccess fiDest( dirDest.prettyAbsPath() + "/" + mfi.m_subPath, true );
      addListViewItem( m_pInfoList, i18n("Dest"), dirDest.prettyAbsPath(), fiDest );
   }
}


void DirectoryMergeWindow::initDirectoryMergeActions( QObject* pKDiff3App, KActionCollection* ac )
{
#include "xpm/startmerge.xpm"
   DirectoryMergeWindow* p = this;

   dirStartOperation = new KAction(i18n("Start/Continue Directory Merge"), Key_F7, p, SLOT(slotRunOperationForAllItems()), ac, "dir_start_operation");
   dirRunOperationForCurrentItem = new KAction(i18n("Run Operation for Current Item"), Key_F6, p, SLOT(slotRunOperationForCurrentItem()), ac, "dir_run_operation_for_current_item");
   dirCompareCurrent = new KAction(i18n("Compare Selected File"), 0, p, SLOT(compareCurrentFile()), ac, "dir_compare_current");
   dirMergeCurrent = new KAction(i18n("Merge Current File"), QIconSet(QPixmap(startmerge)), 0, pKDiff3App, SLOT(slotMergeCurrentFile()), ac, "merge_current");
   dirFoldAll = new KAction(i18n("Fold All Subdirs"), 0, p, SLOT(slotFoldAllSubdirs()), ac, "dir_fold_all");
   dirUnfoldAll = new KAction(i18n("Unfold All Subdirs"), 0, p, SLOT(slotUnfoldAllSubdirs()), ac, "dir_unfold_all");
   dirRescan = new KAction(i18n("Rescan"), SHIFT+Key_F5, p, SLOT(reload()), ac, "dir_rescan");
   dirChooseAEverywhere = new KAction(i18n("Choose A for All Items"), 0, p, SLOT(slotChooseAEverywhere()), ac, "dir_choose_a_everywhere");
   dirChooseBEverywhere = new KAction(i18n("Choose B for All Items"), 0, p, SLOT(slotChooseBEverywhere()), ac, "dir_choose_b_everywhere");
   dirChooseCEverywhere = new KAction(i18n("Choose C for All Items"), 0, p, SLOT(slotChooseCEverywhere()), ac, "dir_choose_c_everywhere");
   dirAutoChoiceEverywhere = new KAction(i18n("Auto-Choose Operation for All Items"), 0, p, SLOT(slotAutoChooseEverywhere()), ac, "dir_autochoose_everywhere");
   dirDoNothingEverywhere = new KAction(i18n("No Operation for All Items"), 0, p, SLOT(slotNoOpEverywhere()), ac, "dir_nothing_everywhere");

   dirCurrentDoNothing = new KAction(i18n("Do Nothing"), 0, p, SLOT(slotCurrentDoNothing()), ac, "dir_current_do_nothing");
   dirCurrentChooseA = new KAction(i18n("A"), 0, p, SLOT(slotCurrentChooseA()), ac, "dir_current_choose_a");
   dirCurrentChooseB = new KAction(i18n("B"), 0, p, SLOT(slotCurrentChooseB()), ac, "dir_current_choose_b");
   dirCurrentChooseC = new KAction(i18n("C"), 0, p, SLOT(slotCurrentChooseC()), ac, "dir_current_choose_c");
   dirCurrentMerge   = new KAction(i18n("Merge"), 0, p, SLOT(slotCurrentMerge()), ac, "dir_current_merge");
   dirCurrentDelete  = new KAction(i18n("Delete (If Exists)"), 0, p, SLOT(slotCurrentDelete()), ac, "dir_current_delete");

   dirCurrentSyncDoNothing = new KAction(i18n("Do Nothing"), 0, p, SLOT(slotCurrentDoNothing()), ac, "dir_current_sync_do_nothing");
   dirCurrentSyncCopyAToB = new KAction(i18n("Copy A to B"), 0, p, SLOT(slotCurrentCopyAToB()), ac, "dir_current_sync_copy_a_to_b" );
   dirCurrentSyncCopyBToA = new KAction(i18n("Copy B to A"), 0, p, SLOT(slotCurrentCopyBToA()), ac, "dir_current_sync_copy_b_to_a" );
   dirCurrentSyncDeleteA  = new KAction(i18n("Delete A"), 0, p, SLOT(slotCurrentDeleteA()), ac,"dir_current_sync_delete_a");
   dirCurrentSyncDeleteB  = new KAction(i18n("Delete B"), 0, p, SLOT(slotCurrentDeleteB()), ac,"dir_current_sync_delete_b");
   dirCurrentSyncDeleteAAndB  = new KAction(i18n("Delete A and B"), 0, p, SLOT(slotCurrentDeleteAAndB()), ac,"dir_current_sync_delete_a_and_b");
   dirCurrentSyncMergeToA   = new KAction(i18n("Merge to A"), 0, p, SLOT(slotCurrentMergeToA()), ac,"dir_current_sync_merge_to_a");
   dirCurrentSyncMergeToB   = new KAction(i18n("Merge to B"), 0, p, SLOT(slotCurrentMergeToB()), ac,"dir_current_sync_merge_to_b");
   dirCurrentSyncMergeToAAndB   = new KAction(i18n("Merge to A and B"), 0, p, SLOT(slotCurrentMergeToAAndB()), ac,"dir_current_sync_merge_to_a_and_b");
}


void DirectoryMergeWindow::updateAvailabilities( bool bDirCompare, bool bDiffWindowVisible,
   KToggleAction* chooseA, KToggleAction* chooseB, KToggleAction* chooseC )
{
   dirStartOperation->setEnabled( bDirCompare );
   dirRunOperationForCurrentItem->setEnabled( bDirCompare );
   dirFoldAll->setEnabled( bDirCompare );
   dirUnfoldAll->setEnabled( bDirCompare );

   dirCompareCurrent->setEnabled( bDirCompare  &&  isVisible()  &&  isFileSelected() );

   dirMergeCurrent->setEnabled( bDirCompare  &&  isVisible()  &&  isFileSelected()
                                || bDiffWindowVisible );

   dirRescan->setEnabled( bDirCompare );

   dirAutoChoiceEverywhere->setEnabled( bDirCompare &&  isVisible() );
   dirDoNothingEverywhere->setEnabled( bDirCompare &&  isVisible() );
   dirChooseAEverywhere->setEnabled( bDirCompare &&  isVisible() );
   dirChooseBEverywhere->setEnabled( bDirCompare &&  isVisible() );
   dirChooseCEverywhere->setEnabled( bDirCompare &&  isVisible() );

   bool bThreeDirs = m_dirC.isValid();

   QListViewItem* lvi = currentItem();
   DirMergeItem* pDMI = lvi==0 ? 0 : static_cast<DirMergeItem*>(lvi);
   MergeFileInfos* pMFI = pDMI==0 ? 0 : pDMI->m_pMFI;

   bool bItemActive = bDirCompare &&  isVisible() && pMFI!=0;//  &&  hasFocus();
   bool bMergeMode = bThreeDirs || !m_bSyncMode;
   bool bFTConflict = pMFI==0 ? false : conflictingFileTypes(*pMFI);

   bool bDirWindowHasFocus = isVisible() && hasFocus();
   
   dirCurrentDoNothing->setEnabled( bItemActive && bMergeMode );
   dirCurrentChooseA->setEnabled( bItemActive && bMergeMode && pMFI->m_bExistsInA );
   dirCurrentChooseB->setEnabled( bItemActive && bMergeMode && pMFI->m_bExistsInB );
   dirCurrentChooseC->setEnabled( bItemActive && bMergeMode && pMFI->m_bExistsInC );
   dirCurrentMerge->setEnabled( bItemActive && bMergeMode && !bFTConflict );
   dirCurrentDelete->setEnabled( bItemActive && bMergeMode );
   if ( bDirWindowHasFocus )
   {
      chooseA->setEnabled( bItemActive && pMFI->m_bExistsInA );
      chooseB->setEnabled( bItemActive && pMFI->m_bExistsInB );
      chooseC->setEnabled( bItemActive && pMFI->m_bExistsInC );
      chooseA->setChecked( false );
      chooseB->setChecked( false );
      chooseC->setChecked( false );
   }
   
   dirCurrentSyncDoNothing->setEnabled( bItemActive && !bMergeMode );
   dirCurrentSyncCopyAToB->setEnabled( bItemActive && !bMergeMode && pMFI->m_bExistsInA );
   dirCurrentSyncCopyBToA->setEnabled( bItemActive && !bMergeMode && pMFI->m_bExistsInB );
   dirCurrentSyncDeleteA->setEnabled( bItemActive && !bMergeMode && pMFI->m_bExistsInA );
   dirCurrentSyncDeleteB->setEnabled( bItemActive && !bMergeMode && pMFI->m_bExistsInB );
   dirCurrentSyncDeleteAAndB->setEnabled( bItemActive && !bMergeMode && pMFI->m_bExistsInB && pMFI->m_bExistsInB );
   dirCurrentSyncMergeToA->setEnabled( bItemActive && !bMergeMode && !bFTConflict );
   dirCurrentSyncMergeToB->setEnabled( bItemActive && !bMergeMode && !bFTConflict );
   dirCurrentSyncMergeToAAndB->setEnabled( bItemActive && !bMergeMode && !bFTConflict );
}


#include "directorymergewindow.moc"
