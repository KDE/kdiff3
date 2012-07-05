/***************************************************************************
                          directorymergewindow.cpp
                             -----------------
    begin                : Sat Oct 19 2002
    copyright            : (C) 2002-2011 by Joachim Eibl
    email                : joachim.eibl at gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "stable.h"
#include "directorymergewindow.h"
#include "options.h"
#include "progress.h"
#include <vector>
#include <map>

#include <QDir>
#include <QApplication>
#include <QPixmap>
#include <QImage>
#include <QTextStream>
#include <QKeyEvent>
#include <QMenu>
#include <QRegExp>
#include <QMessageBox>
#include <QLayout>
#include <QLabel>
#include <QSplitter>
#include <QTextEdit>
#include <QItemDelegate>
#include <QPushButton>
#include <algorithm>

#include <kmenu.h>
#include <kaction.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kiconloader.h>
#include <klocale.h>
#include <ktoggleaction.h>

#include <assert.h>
//#include <konq_popupmenu.h>

#include "guiutils.h"

static bool conflictingFileTypes(MergeFileInfos& mfi);
static QPixmap getOnePixmap( e_Age eAge, bool bLink, bool bDir );

class StatusInfo : public QDialog
{
   QTextEdit* m_pTextEdit;
public:
   StatusInfo(QWidget* pParent) : QDialog( pParent )
   {
      QVBoxLayout* pVLayout = new QVBoxLayout( this );     
      m_pTextEdit = new QTextEdit(this);
      pVLayout->addWidget( m_pTextEdit );
      setObjectName("StatusInfo");
      setWindowFlags(Qt::Dialog);
      m_pTextEdit->setWordWrapMode(QTextOption::NoWrap);
      m_pTextEdit->setReadOnly(true);
      QPushButton* pClose = new QPushButton(tr("Close"));
      connect( pClose, SIGNAL(clicked()), this, SLOT(accept()));
      pVLayout->addWidget(pClose);
   }

   bool isEmpty(){ 
      return m_pTextEdit->toPlainText().isEmpty(); 
   }

   void addText(const QString& s )
   {
      m_pTextEdit->append(s);
   }

   void clear()
   {
      m_pTextEdit->clear();
   }

   void setVisible(bool bVisible)
   {
      if (bVisible)
      {
         m_pTextEdit->moveCursor ( QTextCursor::End );
         m_pTextEdit->moveCursor ( QTextCursor::StartOfLine );
         m_pTextEdit->ensureCursorVisible();
      }

      QDialog::setVisible(bVisible);
      if ( bVisible )
         setWindowState( windowState() | Qt::WindowMaximized );
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
      FileAccess::removeTempFile(m_name);
}


enum Columns
{
   s_NameCol = 0,
   s_ACol = 1,
   s_BCol = 2,
   s_CCol = 3,
   s_OpCol = 4,
   s_OpStatusCol = 5,
   s_UnsolvedCol = 6,    // Nr of unsolved conflicts (for 3 input files)
   s_SolvedCol = 7,      // Nr of auto-solvable conflicts (for 3 input files)
   s_NonWhiteCol = 8,    // Nr of nonwhite deltas (for 2 input files)
   s_WhiteCol = 9        // Nr of white deltas (for 2 input files)
};

enum e_OperationStatus
{
   eOpStatusNone,
   eOpStatusDone,
   eOpStatusError,
   eOpStatusSkipped,
   eOpStatusNotSaved,
   eOpStatusInProgress,
   eOpStatusToDo
};

class MergeFileInfos
{
public:
   MergeFileInfos()
   { 
      m_bEqualAB=false; m_bEqualAC=false; m_bEqualBC=false;
      m_pParent=0;                                         
      m_bOperationComplete=false; m_bSimOpComplete = false;
      m_eMergeOperation=eNoOperation;
      m_eOpStatus = eOpStatusNone;
      m_ageA = eNotThere; m_ageB=eNotThere; m_ageC=eNotThere;
      m_bConflictingAges=false; 
      m_pFileInfoA = 0; m_pFileInfoB = 0; m_pFileInfoC = 0; 
   }
   ~MergeFileInfos()
   {
      //for( int i=0; i<m_children.count(); ++i )
      //   delete m_children[i];
      m_children.clear();
   }
   //bool operator>( const MergeFileInfos& );
   QString subPath() const
   {
      return m_pFileInfoA && m_pFileInfoA->exists() ? m_pFileInfoA->filePath() :
             m_pFileInfoB && m_pFileInfoB->exists() ? m_pFileInfoB->filePath() :
             m_pFileInfoC && m_pFileInfoC->exists() ? m_pFileInfoC->filePath() :
             QString("");
   }
   QString fileName() const
   {
      return m_pFileInfoA && m_pFileInfoA->exists() ? m_pFileInfoA->fileName() :
             m_pFileInfoB && m_pFileInfoB->exists() ? m_pFileInfoB->fileName() :
             m_pFileInfoC && m_pFileInfoC->exists() ? m_pFileInfoC->fileName() :
             QString("");
   }
   bool dirA() const { return m_pFileInfoA ? m_pFileInfoA->isDir() : false; }
   bool dirB() const { return m_pFileInfoB ? m_pFileInfoB->isDir() : false; }
   bool dirC() const { return m_pFileInfoC ? m_pFileInfoC->isDir() : false; }
   bool isLinkA() const { return m_pFileInfoA ? m_pFileInfoA->isSymLink() : false; }
   bool isLinkB() const { return m_pFileInfoB ? m_pFileInfoB->isSymLink() : false; }
   bool isLinkC() const { return m_pFileInfoC ? m_pFileInfoC->isSymLink() : false; }
   bool existsInA() const { return m_pFileInfoA!=0; }
   bool existsInB() const { return m_pFileInfoB!=0; }
   bool existsInC() const { return m_pFileInfoC!=0; }
   MergeFileInfos* m_pParent;
   FileAccess*     m_pFileInfoA;
   FileAccess*     m_pFileInfoB;
   FileAccess*     m_pFileInfoC;
   TotalDiffStatus m_totalDiffStatus;
   QList<MergeFileInfos*> m_children;

   e_MergeOperation m_eMergeOperation : 5;
   e_OperationStatus m_eOpStatus : 4;

   e_Age m_ageA                  : 3;
   e_Age m_ageB                  : 3;
   e_Age m_ageC                  : 3;

   bool m_bOperationComplete     : 1;
   bool m_bSimOpComplete         : 1;

   bool m_bEqualAB               : 1;
   bool m_bEqualAC               : 1;
   bool m_bEqualBC               : 1;
   bool m_bConflictingAges       : 1;       // Equal age but files are not!
};



class DirectoryMergeWindow::Data : public QAbstractItemModel
{
public:
   DirectoryMergeWindow* q;
   Data( DirectoryMergeWindow* pDMW )
   {
      q = pDMW;
      m_pOptions = 0;
      m_pIconLoader = 0;
      m_pDirectoryMergeInfo = 0;
      m_bSimulatedMergeStarted=false;
      m_bRealMergeStarted=false;
      m_bError = false;
      m_bSyncMode = false;
      m_pStatusInfo = new StatusInfo(q);
      m_pStatusInfo->hide();
      m_bScanning = false;
      m_bCaseSensitive = true;
      m_bUnfoldSubdirs = false;
      m_bSkipDirStatus = false;
      m_pRoot = new MergeFileInfos;
   }
   ~Data()
   {
      delete m_pRoot;
   }
   // Implement QAbstractItemModel
   QVariant	data ( const QModelIndex & index, int role = Qt::DisplayRole ) const;
   //Qt::ItemFlags flags ( const QModelIndex & index ) const
   QModelIndex	parent ( const QModelIndex & index ) const
   {
      MergeFileInfos* pMFI = getMFI( index );
      if ( pMFI == 0 || pMFI==m_pRoot || pMFI->m_pParent==m_pRoot )
         return QModelIndex();
      else
      {
         MergeFileInfos* pParentsParent = pMFI->m_pParent->m_pParent;
         return createIndex( pParentsParent->m_children.indexOf(pMFI->m_pParent), 0, pMFI->m_pParent );
      }
   }
   int	rowCount ( const QModelIndex & parent = QModelIndex() ) const
   {
      MergeFileInfos* pParentMFI = getMFI( parent );
      if ( pParentMFI!=0 )
         return pParentMFI->m_children.count();
      else
         return m_pRoot->m_children.count();
   }
   int	columnCount ( const QModelIndex & /*parent*/ ) const 
   {
      return 10;
   }
   QModelIndex	index ( int row, int column, const QModelIndex & parent ) const
   {
      MergeFileInfos* pParentMFI = getMFI( parent );
      if ( pParentMFI == 0 && row < m_pRoot->m_children.count() )
         return createIndex( row, column, m_pRoot->m_children[row] );
      else if ( pParentMFI != 0 && row < pParentMFI->m_children.count() )
         return createIndex( row, column, pParentMFI->m_children[row] );
      else
         return QModelIndex();
   }
   QVariant	headerData ( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;
   void sort( int column, Qt::SortOrder order );
   // private data and helper methods
   MergeFileInfos* getMFI( const QModelIndex& mi ) const
   {
      if ( mi.isValid() )
         return (MergeFileInfos*)mi.internalPointer();
      else
         return 0;
   }
   MergeFileInfos* m_pRoot;

   QString fullNameA( const MergeFileInfos& mfi )
   { return mfi.existsInA() ? mfi.m_pFileInfoA->absoluteFilePath() : m_dirA.absoluteFilePath() + "/" + mfi.subPath(); }
   QString fullNameB( const MergeFileInfos& mfi )
   { return mfi.existsInB() ? mfi.m_pFileInfoB->absoluteFilePath() : m_dirB.absoluteFilePath() + "/" + mfi.subPath(); }
   QString fullNameC( const MergeFileInfos& mfi )
   { return mfi.existsInC() ? mfi.m_pFileInfoC->absoluteFilePath() : m_dirC.absoluteFilePath() + "/" + mfi.subPath(); }
   QString fullNameDest( const MergeFileInfos& mfi )
   { if       ( m_dirDestInternal.prettyAbsPath() == m_dirC.prettyAbsPath() ) return fullNameC(mfi);
     else if ( m_dirDestInternal.prettyAbsPath() == m_dirB.prettyAbsPath() ) return fullNameB(mfi);
     else return m_dirDestInternal.absoluteFilePath() + "/" + mfi.subPath(); 
   }

   FileAccess m_dirA;
   FileAccess m_dirB;
   FileAccess m_dirC;
   FileAccess m_dirDest;
   FileAccess m_dirDestInternal;
   Options* m_pOptions;

   void calcDirStatus( bool bThreeDirs, const QModelIndex& mi, 
      int& nofFiles, int& nofDirs, int& nofEqualFiles, int& nofManualMerges );

   void mergeContinue( bool bStart, bool bVerbose );

   void prepareListView(ProgressProxy& pp);
   void calcSuggestedOperation( const QModelIndex& mi, e_MergeOperation eDefaultOperation );
   void setAllMergeOperations( e_MergeOperation eDefaultOperation );
   friend class MergeFileInfos;

   bool canContinue();
   QModelIndex treeIterator( QModelIndex mi, bool bVisitChildren=true, bool bFindInvisible=false );
   void prepareMergeStart( const QModelIndex& miBegin, const QModelIndex& miEnd, bool bVerbose );
   bool executeMergeOperation( MergeFileInfos& mfi, bool& bSingleFileMerge );

   void scanDirectory( const QString& dirName, t_DirectoryList& dirList );
   void scanLocalDirectory( const QString& dirName, t_DirectoryList& dirList );
   bool fastFileComparison( FileAccess& fi1, FileAccess& fi2,
                            bool& bError, QString& status );
   void compareFilesAndCalcAges( MergeFileInfos& mfi );

   void setMergeOperation( const QModelIndex& mi, e_MergeOperation eMergeOp, bool bRecursive = true );
   bool isDir( const QModelIndex& mi );
   QString getFileName( const QModelIndex& mi );

   bool copyFLD( const QString& srcName, const QString& destName );
   bool deleteFLD( const QString& name, bool bCreateBackup );
   bool makeDir( const QString& name, bool bQuiet=false );
   bool renameFLD( const QString& srcName, const QString& destName );
   bool mergeFLD( const QString& nameA,const QString& nameB,const QString& nameC,
                  const QString& nameDest, bool& bSingleFileMerge );

   t_DirectoryList m_dirListA;
   t_DirectoryList m_dirListB;
   t_DirectoryList m_dirListC;

   QString m_dirMergeStateFilename;

   class FileKey
   {
   public:
      const FileAccess* m_pFA;
      FileKey( const FileAccess& fa ) : m_pFA( &fa ){}

      int getParents( const FileAccess* pFA, const FileAccess* v[] ) const
      {
         int s = 0;
         for(s=0; pFA->parent() != 0 ; pFA=pFA->parent(), ++s )
            v[s] = pFA ;
         return s;
      }

      // This is essentially the same as 
      // int r = filePath().compare( fa.filePath() )
      // if ( r<0 ) return true;
      // if ( r==0 ) return m_col < fa.m_col;
      // return false;
      bool operator< ( const FileKey& fk ) const
      {
         const FileAccess* v1[100];
         const FileAccess* v2[100];
         int v1Size = getParents(m_pFA, v1);
         int v2Size = getParents(fk.m_pFA, v2);
        
         for( int i=0; i<v1Size && i<v2Size; ++i )
         {
            int r = v1[v1Size-i-1]->fileName().compare( v2[v2Size-i-1]->fileName() );
            if (  r < 0  )
               return true;
            else if ( r > 0  )
               return false;
         }

         if ( v1Size < v2Size )
            return true;
         return false;
      }
   };
   typedef QMap<FileKey, MergeFileInfos> t_fileMergeMap;
   t_fileMergeMap m_fileMergeMap;

   bool m_bFollowDirLinks;
   bool m_bFollowFileLinks;
   bool m_bSimulatedMergeStarted;
   bool m_bRealMergeStarted;
   bool m_bError;
   bool m_bSyncMode;
   bool m_bDirectoryMerge; // if true, then merge is the default operation, otherwise it's diff.
   bool m_bCaseSensitive;
   bool m_bUnfoldSubdirs;
   bool m_bSkipDirStatus;   
   bool m_bScanning; // true while in init()

   KIconLoader* m_pIconLoader;
   DirectoryMergeInfo* m_pDirectoryMergeInfo;
   StatusInfo* m_pStatusInfo;

   typedef std::list< QModelIndex > MergeItemList; // linked list
   MergeItemList m_mergeItemList;
   MergeItemList::iterator m_currentIndexForOperation;

   QModelIndex m_selection1Index;
   QModelIndex m_selection2Index;
   QModelIndex m_selection3Index;
   void selectItemAndColumn( const QModelIndex& mi, bool bContextMenu);
   friend class DirMergeItem;

   KAction* m_pDirStartOperation;
   KAction* m_pDirRunOperationForCurrentItem;
   KAction* m_pDirCompareCurrent;
   KAction* m_pDirMergeCurrent;
   KAction* m_pDirRescan;
   KAction* m_pDirChooseAEverywhere;
   KAction* m_pDirChooseBEverywhere;
   KAction* m_pDirChooseCEverywhere;
   KAction* m_pDirAutoChoiceEverywhere;
   KAction* m_pDirDoNothingEverywhere;
   KAction* m_pDirFoldAll;
   KAction* m_pDirUnfoldAll;

   KToggleAction* m_pDirShowIdenticalFiles;
   KToggleAction* m_pDirShowDifferentFiles;
   KToggleAction* m_pDirShowFilesOnlyInA;
   KToggleAction* m_pDirShowFilesOnlyInB;
   KToggleAction* m_pDirShowFilesOnlyInC;

   KToggleAction* m_pDirSynchronizeDirectories;
   KToggleAction* m_pDirChooseNewerFiles;

   KAction* m_pDirCompareExplicit;
   KAction* m_pDirMergeExplicit;

   KAction* m_pDirCurrentDoNothing;
   KAction* m_pDirCurrentChooseA;
   KAction* m_pDirCurrentChooseB;
   KAction* m_pDirCurrentChooseC;
   KAction* m_pDirCurrentMerge;
   KAction* m_pDirCurrentDelete;

   KAction* m_pDirCurrentSyncDoNothing;
   KAction* m_pDirCurrentSyncCopyAToB;
   KAction* m_pDirCurrentSyncCopyBToA;
   KAction* m_pDirCurrentSyncDeleteA;
   KAction* m_pDirCurrentSyncDeleteB;
   KAction* m_pDirCurrentSyncDeleteAAndB;
   KAction* m_pDirCurrentSyncMergeToA;
   KAction* m_pDirCurrentSyncMergeToB;
   KAction* m_pDirCurrentSyncMergeToAAndB;

   KAction* m_pDirSaveMergeState;
   KAction* m_pDirLoadMergeState;

   bool init( FileAccess& dirA, FileAccess& dirB, FileAccess& dirC, FileAccess& dirDest, bool bDirectoryMerge, bool bReload );
   void setOpStatus( const QModelIndex& mi, e_OperationStatus eOpStatus )
   {
      if ( MergeFileInfos* pMFI = getMFI(mi) )
      {
         pMFI->m_eOpStatus = eOpStatus;
         emit dataChanged( mi, mi );
      }
   }
};

QVariant DirectoryMergeWindow::Data::data( const QModelIndex & index, int role ) const
{
   MergeFileInfos* pMFI = getMFI( index );
   if ( pMFI )
   {
      if ( role == Qt::DisplayRole )
      {
         switch ( index.column() )
         {
         case s_NameCol:     return QFileInfo(pMFI->subPath()).fileName();
         case s_ACol:        return "A";
         case s_BCol:        return "B";
         case s_CCol:        return "C";
         //case s_OpCol:       return i18n("Operation");
         //case s_OpStatusCol: return i18n("Status");
         case s_UnsolvedCol: return i18n("Unsolved");
         case s_SolvedCol:   return i18n("Solved");
         case s_NonWhiteCol: return i18n("Nonwhite");
         case s_WhiteCol:    return i18n("White");
         //default :           return QVariant();
         }

         if ( s_OpCol == index.column() )
         {            
            bool bDir = pMFI->dirA() || pMFI->dirB() || pMFI->dirC();
            switch( pMFI->m_eMergeOperation )
            {
            case eNoOperation:      return ""; break;
            case eCopyAToB:         return i18n("Copy A to B");     break;
            case eCopyBToA:         return i18n("Copy B to A");     break;
            case eDeleteA:          return i18n("Delete A");        break;
            case eDeleteB:          return i18n("Delete B");        break;
            case eDeleteAB:         return i18n("Delete A & B");    break;
            case eMergeToA:         return i18n("Merge to A");      break;
            case eMergeToB:         return i18n("Merge to B");      break;
            case eMergeToAB:        return i18n("Merge to A & B");  break;
            case eCopyAToDest:      return "A";    break;
            case eCopyBToDest:      return "B";    break;
            case eCopyCToDest:      return "C";    break;
            case eDeleteFromDest:   return i18n("Delete (if exists)");  break;
            case eMergeABCToDest:   return bDir ? i18n("Merge") : i18n("Merge (manual)");    break;
            case eMergeABToDest:    return bDir ? i18n("Merge") : i18n("Merge (manual)");    break;
            case eConflictingFileTypes: return i18n("Error: Conflicting File Types");         break;
            case eChangedAndDeleted: return i18n("Error: Changed and Deleted");               break;
            case eConflictingAges:  return i18n("Error: Dates are equal but files are not."); break;
            default:                assert(false); break;
            }
         }
         if ( s_OpStatusCol == index.column() )
         {
            switch( pMFI->m_eOpStatus )
            {
            case eOpStatusNone: return "";
            case eOpStatusDone: return i18n("Done");
            case eOpStatusError: return i18n("Error");
            case eOpStatusSkipped: return i18n("Skipped.");
            case eOpStatusNotSaved:return i18n("Not saved.");
            case eOpStatusInProgress: return i18n("In progress...");
            case eOpStatusToDo: return i18n("To do.");
            }
         }
      }
      else if ( role == Qt::DecorationRole )
      {
         if ( s_NameCol == index.column() )
         {
            return getOnePixmap( eAgeEnd, pMFI->isLinkA() || pMFI->isLinkB() || pMFI->isLinkC(),
                                 pMFI->dirA()  || pMFI->dirB()  || pMFI->dirC() );
         }

         if ( s_ACol == index.column() )
         {
            return getOnePixmap( pMFI->m_ageA, pMFI->isLinkA(), pMFI->dirA() );
         }
         if ( s_BCol == index.column() )
         {
            return getOnePixmap( pMFI->m_ageB, pMFI->isLinkB(), pMFI->dirB() );
         }
         if ( s_CCol == index.column() )
         {
            return getOnePixmap( pMFI->m_ageC, pMFI->isLinkC(), pMFI->dirC() );
         }
      }
      else if ( role == Qt::TextAlignmentRole )
      {
         if ( s_UnsolvedCol == index.column() || s_SolvedCol == index.column() 
            || s_NonWhiteCol == index.column() || s_WhiteCol == index.column() )
            return Qt::AlignRight;
      }
   }
   return QVariant();
}

QVariant DirectoryMergeWindow::Data::headerData ( int section, Qt::Orientation orientation, int role ) const
{
   if ( orientation == Qt::Horizontal && section>=0 && section<columnCount(QModelIndex()) && role==Qt::DisplayRole )
   {
      switch ( section )
      {
      case s_NameCol:     return i18n("Name");
      case s_ACol:        return "A";
      case s_BCol:        return "B";
      case s_CCol:        return "C";
      case s_OpCol:       return i18n("Operation");
      case s_OpStatusCol: return i18n("Status");
      case s_UnsolvedCol: return i18n("Unsolved");
      case s_SolvedCol:   return i18n("Solved");
      case s_NonWhiteCol: return i18n("Nonwhite");
      case s_WhiteCol:    return i18n("White");
      default :           return QVariant();
      }
   }
   return QVariant();
}

// Previously  Q3ListViewItem::paintCell(p,cg,column,width,align);
class DirectoryMergeWindow::DirMergeItemDelegate : public QItemDelegate
{
   DirectoryMergeWindow* m_pDMW;
   DirectoryMergeWindow::Data* d;
public:
   DirMergeItemDelegate(DirectoryMergeWindow* pParent) 
      : QItemDelegate(pParent), m_pDMW(pParent), d(pParent->d)
   {
   }
   void paint( QPainter * p, const QStyleOptionViewItem & option, const QModelIndex & index ) const 
   {
      int column = index.column();
      if (column == s_ACol || column == s_BCol || column == s_CCol )
      {
         QVariant value = index.data( Qt::DecorationRole );
         QPixmap icon;
         if ( value.isValid() ) 
         {
            if (value.type() == QVariant::Icon) 
            {
               icon = qvariant_cast<QIcon>(value).pixmap(16,16);
               //icon = qvariant_cast<QIcon>(value);
               //decorationRect = QRect(QPoint(0, 0), icon.actualSize(option.decorationSize, iconMode, iconState));
            } 
            else 
            {
               icon = qvariant_cast<QPixmap>(value);
               //decorationRect = QRect(QPoint(0, 0), option.decorationSize).intersected(pixmap.rect());
            }
         }

         int x = option.rect.left();
         int y = option.rect.top();
         //QPixmap icon = value.value<QPixmap>(); //pixmap(column);
         if ( !icon.isNull() )
         {
            int yOffset = (sizeHint(option,index).height() - icon.height()) / 2;
            p->drawPixmap( x+2, y+yOffset, icon );

            int i = index==d->m_selection1Index ? 1 :
                    index==d->m_selection2Index ? 2 :
                    index==d->m_selection3Index ? 3 :
                                                   0 ;
            if ( i!=0 )
            {
               Options* pOpts = d->m_pOptions;
               QColor c ( i==1 ? pOpts->m_colorA : i==2 ? pOpts->m_colorB : pOpts->m_colorC );
               p->setPen( c );// highlight() );
               p->drawRect( x+2, y+yOffset, icon.width(), icon.height());
               p->setPen( QPen( c, 0, Qt::DotLine) );
               p->drawRect( x+1, y+yOffset-1, icon.width()+2, icon.height()+2);
               p->setPen( Qt::white );
               QString s( QChar('A'+i-1) );
               p->drawText( x+2 + (icon.width() - p->fontMetrics().width(s))/2,
                           y+yOffset + (icon.height() + p->fontMetrics().ascent())/2-1,
                           s );
            }
            else
            {
               p->setPen( m_pDMW->palette().background().color() );
               p->drawRect( x+1, y+yOffset-1, icon.width()+2, icon.height()+2);
            }
            return;
         }
      }
      QStyleOptionViewItem option2 = option;
      if ( column>=s_UnsolvedCol )
      {
         option2.displayAlignment = Qt::AlignRight;
      }
      QItemDelegate::paint( p, option2, index );
   }
   QSize sizeHint( const QStyleOptionViewItem & option, const QModelIndex & index ) const 
   {
      QSize sz = QItemDelegate::sizeHint( option, index );
      return sz.expandedTo( QSize(0,18) );
   }
};


DirectoryMergeWindow::DirectoryMergeWindow( QWidget* pParent, Options* pOptions, KIconLoader* pIconLoader )
   : QTreeView( pParent )
{
   d = new Data(this);
   setModel( d );
   setItemDelegate( new DirMergeItemDelegate(this) );
   connect( this, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(onDoubleClick(const QModelIndex&)));
   connect( this, SIGNAL(expanded(const QModelIndex&)), this, SLOT(onExpanded()));

   d->m_pOptions = pOptions;
   d->m_pIconLoader = pIconLoader;

   setSortingEnabled(true);
}

DirectoryMergeWindow::~DirectoryMergeWindow()
{
   delete d;
}

QString DirectoryMergeWindow::getDirNameA(){ return d->m_dirA.prettyAbsPath(); }
QString DirectoryMergeWindow::getDirNameB(){ return d->m_dirB.prettyAbsPath(); }
QString DirectoryMergeWindow::getDirNameC(){ return d->m_dirC.prettyAbsPath(); }
QString DirectoryMergeWindow::getDirNameDest(){ return d->m_dirDest.prettyAbsPath(); }
void DirectoryMergeWindow::setDirectoryMergeInfo(DirectoryMergeInfo* p){ d->m_pDirectoryMergeInfo=p; }
bool DirectoryMergeWindow::isDirectoryMergeInProgress() { return d->m_bRealMergeStarted; }
bool DirectoryMergeWindow::isSyncMode() { return d->m_bSyncMode; }
bool DirectoryMergeWindow::isScanning() { return d->m_bScanning; }

bool DirectoryMergeWindow::Data::fastFileComparison(
   FileAccess& fi1, FileAccess& fi2,
   bool& bError, QString& status )
{
   ProgressProxy pp;
   status = "";
   bool bEqual = false;
   bError = true;

   if ( !m_bFollowFileLinks )
   {
      if ( fi1.isSymLink() != fi2.isSymLink() )
      {
         status = i18n("Mix of links and normal files.");
         return bEqual;
      }
      else if ( fi1.isSymLink() && fi2.isSymLink() )
      {
         bError = false;
         bEqual = fi1.readLink() == fi2.readLink();
         status = i18n("Link: ");
         return bEqual;
      }
   }

   if ( fi1.size()!=fi2.size() )
   {
      bEqual = false;
      status = i18n("Size. ");
      return bEqual;
   }
   else if ( m_pOptions->m_bDmTrustSize )
   {
      bEqual = true;
      return bEqual;
   }

   if ( m_pOptions->m_bDmTrustDate )
   {
      bEqual = ( fi1.lastModified() == fi2.lastModified()  &&  fi1.size()==fi2.size() );
      bError = false;
      status = i18n("Date & Size: ");
      return bEqual;
   }

   if ( m_pOptions->m_bDmTrustDateFallbackToBinary )
   {
      bEqual = ( fi1.lastModified() == fi2.lastModified()  &&  fi1.size()==fi2.size() );
      if ( bEqual )
      {
         bError = false;
         status = i18n("Date & Size: ");
         return bEqual;
      }
   }

   QString fileName1 = fi1.absoluteFilePath();
   QString fileName2 = fi2.absoluteFilePath();
   TempRemover tr1( fileName1, fi1 );
   if ( !tr1.success() )
   {
      status = i18n("Creating temp copy of %1 failed.",fileName1);
      return bEqual;
   }
   TempRemover tr2( fileName2, fi2 );
   if ( !tr2.success() )
   {
      status = i18n("Creating temp copy of %1 failed.",fileName2);
      return bEqual;
   }

   std::vector<char> buf1(100000);
   std::vector<char> buf2(buf1.size());

   QFile file1( tr1.name() );

   if ( ! file1.open(QIODevice::ReadOnly) )
   {
      status = i18n("Opening %1 failed.",fileName1);
      return bEqual;
   }

   QFile file2( tr2.name() );

   if ( ! file2.open(QIODevice::ReadOnly) )
   {
      status = i18n("Opening %1 failed.",fileName2);
      return bEqual;
   }

   pp.setInformation( i18n("Comparing file..."), 0, false );
   typedef qint64 t_FileSize;
   t_FileSize fullSize = file1.size();
   t_FileSize sizeLeft = fullSize;

   while( sizeLeft>0 && ! pp.wasCancelled() )
   {
      int len = min2( sizeLeft, (t_FileSize)buf1.size() );
      if( len != file1.read( &buf1[0], len ) )
      {
         status = i18n("Error reading from %1",fileName1);
         return bEqual;
      }

      if( len != file2.read( &buf2[0], len ) )
      {
         status = i18n("Error reading from %1",fileName2);
         return bEqual;
      }

      if ( memcmp( &buf1[0], &buf2[0], len ) != 0 )
      {
         bError = false;
         return bEqual;
      }
      sizeLeft-=len;
      pp.setCurrent(double(fullSize-sizeLeft)/fullSize, false );
   }

   // If the program really arrives here, then the files are really equal.
   bError = false;
   bEqual = true;
   return bEqual;
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
         i18n("Warning"), 
         KGuiItem( i18n("Rescan") ), 
         KGuiItem( i18n("Continue Merging") ) );
      if ( result!=KMessageBox::Yes )
         return;
   }

   init( d->m_dirA, d->m_dirB, d->m_dirC, d->m_dirDest, d->m_bDirectoryMerge, true );
}

// Copy pm2 onto pm1, but preserve the alpha value from pm1 where pm2 is transparent.
static QPixmap pixCombiner( const QPixmap* pm1, const QPixmap* pm2 )
{
   QImage img1 = pm1->toImage().convertToFormat(QImage::Format_ARGB32);
   QImage img2 = pm2->toImage().convertToFormat(QImage::Format_ARGB32);

   for (int y = 0; y < img1.height(); y++)
   {
      quint32 *line1 = reinterpret_cast<quint32 *>(img1.scanLine(y));
      quint32 *line2 = reinterpret_cast<quint32 *>(img2.scanLine(y));
      for (int x = 0; x < img1.width();  x++)
      {
         if ( qAlpha( line2[x] ) >0 )
            line1[x] = (line2[x] | 0xff000000);
      }
   }
   return QPixmap::fromImage(img1);
}

// like pixCombiner but let the pm1 color shine through
static QPixmap pixCombiner2( const QPixmap* pm1, const QPixmap* pm2 )
{
   QPixmap pix=*pm1;
   QPainter p(&pix);
   p.setOpacity(0.5);
   p.drawPixmap( 0,0,*pm2 );
   p.end();

   return pix;
}

void DirectoryMergeWindow::Data::calcDirStatus( bool bThreeDirs, const QModelIndex& mi, 
   int& nofFiles, int& nofDirs, int& nofEqualFiles, int& nofManualMerges )
{
   MergeFileInfos* pMFI = getMFI(mi);
   if ( pMFI->dirA() || pMFI->dirB() || pMFI->dirC() )
   {
      ++nofDirs;
   }
   else
   {
      ++nofFiles;
      if ( pMFI->m_bEqualAB && (!bThreeDirs || pMFI->m_bEqualAC ))
      {
         ++nofEqualFiles;
      }
      else
      {
         if ( pMFI->m_eMergeOperation==eMergeABCToDest || pMFI->m_eMergeOperation==eMergeABToDest )
            ++nofManualMerges;
      }
   }
   for( int childIdx=0; childIdx<rowCount(mi); ++childIdx )
      calcDirStatus( bThreeDirs, index(childIdx,0,mi), nofFiles, nofDirs, nofEqualFiles, nofManualMerges );
}

struct t_ItemInfo 
{
   bool bExpanded; 
   bool bOperationComplete; 
   QString status; 
   e_MergeOperation eMergeOperation; 
};

bool DirectoryMergeWindow::init
   (
   FileAccess& dirA,
   FileAccess& dirB,
   FileAccess& dirC,
   FileAccess& dirDest,
   bool bDirectoryMerge,
   bool bReload
   )
{
   return d->init( dirA, dirB, dirC, dirDest, bDirectoryMerge, bReload );
}

bool DirectoryMergeWindow::Data::init
   (
   FileAccess& dirA,
   FileAccess& dirB,
   FileAccess& dirC,
   FileAccess& dirDest,
   bool bDirectoryMerge,
   bool bReload
   )
{
   if ( m_pOptions->m_bDmFullAnalysis )
   {
      // A full analysis uses the same ressources that a normal text-diff/merge uses.
      // So make sure that the user saves his data first.
      bool bCanContinue=false;
      emit q->checkIfCanContinue( &bCanContinue );
      if ( !bCanContinue )
         return false;
      emit q->startDiffMerge("","","","","","","",0);  // hide main window
   }

   q->show();
   q->setUpdatesEnabled(true);

   std::map<QString,t_ItemInfo> expandedDirsMap;

   if ( bReload )
   {
      // Remember expanded items TODO
      //QTreeWidgetItemIterator it( this );
      //while ( *it )
      //{
      //   DirMergeItem* pDMI = static_cast<DirMergeItem*>( *it );
      //   t_ItemInfo& ii = expandedDirsMap[ pDMI->m_pMFI->subPath() ];
      //   ii.bExpanded = pDMI->isExpanded();
      //   ii.bOperationComplete = pDMI->m_pMFI->m_bOperationComplete;
      //   ii.status = pDMI->text( s_OpStatusCol );
      //   ii.eMergeOperation = pDMI->m_pMFI->m_eMergeOperation;
      //   ++it;
      //}
   }

   ProgressProxy pp;
   m_bFollowDirLinks = m_pOptions->m_bDmFollowDirLinks;
   m_bFollowFileLinks = m_pOptions->m_bDmFollowFileLinks;
   m_bSimulatedMergeStarted=false;
   m_bRealMergeStarted=false;
   m_bError=false;
   m_bDirectoryMerge = bDirectoryMerge;
   m_selection1Index = QModelIndex();
   m_selection2Index = QModelIndex();
   m_selection3Index = QModelIndex();
   m_bCaseSensitive = m_pOptions->m_bDmCaseSensitiveFilenameComparison;
   m_bUnfoldSubdirs = m_pOptions->m_bDmUnfoldSubdirs;
   m_bSkipDirStatus = m_pOptions->m_bDmSkipDirStatus;

   m_pRoot->m_children.clear();

   m_mergeItemList.clear();
   reset();
   
   m_currentIndexForOperation = m_mergeItemList.end();

   m_dirA = dirA;
   m_dirB = dirB;
   m_dirC = dirC;
   m_dirDest = dirDest;

   if ( !bReload )
   {
      m_pDirShowIdenticalFiles->setChecked(true);
      m_pDirShowDifferentFiles->setChecked(true);
      m_pDirShowFilesOnlyInA->setChecked(true);
      m_pDirShowFilesOnlyInB->setChecked(true);
      m_pDirShowFilesOnlyInC->setChecked(true);
   }

   // Check if all input directories exist and are valid. The dest dir is not tested now.
   // The test will happen only when we are going to write to it.
   if ( !m_dirA.isDir() || !m_dirB.isDir() ||
        (m_dirC.isValid() && !m_dirC.isDir()) )
   {
       QString text( i18n("Opening of directories failed:") );
       text += "\n\n";
       if ( !dirA.isDir() )
       {  text += i18n("Dir A \"%1\" does not exist or is not a directory.\n",m_dirA.prettyAbsPath()); }

       if ( !dirB.isDir() )
       {  text += i18n("Dir B \"%1\" does not exist or is not a directory.\n",m_dirB.prettyAbsPath()); }

       if ( m_dirC.isValid() && !m_dirC.isDir() )
       {  text += i18n("Dir C \"%1\" does not exist or is not a directory.\n",m_dirC.prettyAbsPath()); }

       KMessageBox::sorry( q, text, i18n("Directory Open Error") );
       return false;
   }

   if ( m_dirC.isValid() &&
        (m_dirDest.prettyAbsPath() == m_dirA.prettyAbsPath()  ||  m_dirDest.prettyAbsPath()==m_dirB.prettyAbsPath() ) )
   {
      KMessageBox::error(q,
         i18n( "The destination directory must not be the same as A or B when "
         "three directories are merged.\nCheck again before continuing."),
         i18n("Parameter Warning"));
      return false;
   }

   m_bScanning = true;
   emit q->statusBarMessage(i18n("Scanning directories..."));

   m_bSyncMode = m_pOptions->m_bDmSyncMode && !m_dirC.isValid() && !m_dirDest.isValid();

   if ( m_dirDest.isValid() )
      m_dirDestInternal = m_dirDest;
   else
      m_dirDestInternal = m_dirC.isValid() ? m_dirC : m_dirB;

   QString origCurrentDirectory = QDir::currentPath();

   m_fileMergeMap.clear();
   t_DirectoryList::iterator i;

   // calc how many directories will be read:
   double nofScans = ( m_dirA.isValid() ? 1 : 0 )+( m_dirB.isValid() ? 1 : 0 )+( m_dirC.isValid() ? 1 : 0 );
   int currentScan = 0;

//TODO   setColumnWidthMode(s_UnsolvedCol, Q3ListView::Manual);
//   setColumnWidthMode(s_SolvedCol,   Q3ListView::Manual);
//   setColumnWidthMode(s_WhiteCol,    Q3ListView::Manual);
//   setColumnWidthMode(s_NonWhiteCol, Q3ListView::Manual);
   q->setColumnHidden( s_CCol,    !m_dirC.isValid() );
   q->setColumnHidden( s_WhiteCol,    !m_pOptions->m_bDmFullAnalysis );
   q->setColumnHidden( s_NonWhiteCol, !m_pOptions->m_bDmFullAnalysis );
   q->setColumnHidden( s_UnsolvedCol, !m_pOptions->m_bDmFullAnalysis );
   q->setColumnHidden( s_SolvedCol,   !( m_pOptions->m_bDmFullAnalysis && m_dirC.isValid() ) );

   bool bListDirSuccessA = true;
   bool bListDirSuccessB = true;
   bool bListDirSuccessC = true;
   m_dirListA.clear();
   m_dirListB.clear();
   m_dirListC.clear();
   if ( m_dirA.isValid() )
   {
      pp.setInformation(i18n("Reading Directory A"));
      pp.setSubRangeTransformation(currentScan/nofScans, (currentScan+1)/nofScans);
      ++currentScan;

      bListDirSuccessA = m_dirA.listDir( &m_dirListA,
         m_pOptions->m_bDmRecursiveDirs, m_pOptions->m_bDmFindHidden,
         m_pOptions->m_DmFilePattern, m_pOptions->m_DmFileAntiPattern,
         m_pOptions->m_DmDirAntiPattern, m_pOptions->m_bDmFollowDirLinks,
         m_pOptions->m_bDmUseCvsIgnore);

      for (i=m_dirListA.begin(); i!=m_dirListA.end();++i )
      {
         MergeFileInfos& mfi = m_fileMergeMap[FileKey(*i)];
         //std::cout <<i->filePath()<<std::endl;
         mfi.m_pFileInfoA = &(*i);
      }
   }

   if ( m_dirB.isValid() )
   {
      pp.setInformation(i18n("Reading Directory B"));
      pp.setSubRangeTransformation(currentScan/nofScans, (currentScan+1)/nofScans);
      ++currentScan;

      bListDirSuccessB =  m_dirB.listDir( &m_dirListB,
         m_pOptions->m_bDmRecursiveDirs, m_pOptions->m_bDmFindHidden,
         m_pOptions->m_DmFilePattern, m_pOptions->m_DmFileAntiPattern,
         m_pOptions->m_DmDirAntiPattern, m_pOptions->m_bDmFollowDirLinks,
         m_pOptions->m_bDmUseCvsIgnore);

      for (i=m_dirListB.begin(); i!=m_dirListB.end();++i )
      {
         MergeFileInfos& mfi = m_fileMergeMap[FileKey(*i)];
         mfi.m_pFileInfoB = &(*i);
         if ( mfi.m_pFileInfoA && mfi.m_pFileInfoA->fileName() == mfi.m_pFileInfoB->fileName() )
            mfi.m_pFileInfoB->setSharedName(mfi.m_pFileInfoA->fileName()); // Reduce memory by sharing the name.
      }
   }

   e_MergeOperation eDefaultMergeOp;
   if ( m_dirC.isValid() )
   {
      pp.setInformation(i18n("Reading Directory C"));
      pp.setSubRangeTransformation(currentScan/nofScans, (currentScan+1)/nofScans);
      ++currentScan;

      bListDirSuccessC = m_dirC.listDir( &m_dirListC,
         m_pOptions->m_bDmRecursiveDirs, m_pOptions->m_bDmFindHidden,
         m_pOptions->m_DmFilePattern, m_pOptions->m_DmFileAntiPattern,
         m_pOptions->m_DmDirAntiPattern, m_pOptions->m_bDmFollowDirLinks,
         m_pOptions->m_bDmUseCvsIgnore);

      for (i=m_dirListC.begin(); i!=m_dirListC.end();++i )
      {
         MergeFileInfos& mfi = m_fileMergeMap[FileKey(*i)];
         mfi.m_pFileInfoC = &(*i);
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
      bContinue = KMessageBox::Continue == KMessageBox::warningContinueCancel( q, s );
   }

   if ( bContinue )
   {
      prepareListView(pp);

      q->updateFileVisibilities();

      for( int childIdx = 0; childIdx<rowCount(); ++childIdx )
      {
         QModelIndex mi = index( childIdx, 0, QModelIndex() );
         calcSuggestedOperation( mi, eDefaultMergeOp );
      }
   }

   q->sortByColumn(0,Qt::AscendingOrder);

   for (int i=0;i<columnCount(QModelIndex());++i)
      q->resizeColumnToContents(i);

   // Try to improve the view a little bit.
   QWidget* pParent = q->parentWidget();
   QSplitter* pSplitter = static_cast<QSplitter*>(pParent);
   if (pSplitter!=0)
   {
      QList<int> sizes = pSplitter->sizes();
      int total = sizes[0] + sizes[1];
      if ( total < 10 )
         total = 100;
      sizes[0]=total*6/10;
      sizes[1]=total - sizes[0];
      pSplitter->setSizes( sizes );
   }

   QDir::setCurrent(origCurrentDirectory);

   m_bScanning = false;
   emit q->statusBarMessage(i18n("Ready."));

   if ( bContinue && !m_bSkipDirStatus )
   {
      // Generate a status report
      int nofFiles=0;
      int nofDirs=0;
      int nofEqualFiles=0;
      int nofManualMerges=0;
//TODO
      for( int childIdx = 0; childIdx<rowCount(); ++childIdx )
         calcDirStatus( m_dirC.isValid(), index(childIdx,0,QModelIndex()),
                        nofFiles, nofDirs, nofEqualFiles, nofManualMerges );

      QString s;
      s = i18n("Directory Comparison Status") + "\n\n" +
          i18n("Number of subdirectories:") +" "+ QString::number(nofDirs)       + "\n"+
          i18n("Number of equal files:")     +" "+ QString::number(nofEqualFiles) + "\n"+
          i18n("Number of different files:") +" "+ QString::number(nofFiles-nofEqualFiles);

      if ( m_dirC.isValid() )
         s += "\n" + i18n("Number of manual merges:")   +" "+ QString::number(nofManualMerges);
      KMessageBox::information( q, s );
      //
      //TODO
      //if ( topLevelItemCount()>0 )
      //{
      //   topLevelItem(0)->setSelected(true);
      //   setCurrentItem( topLevelItem(0) );
      //}
   }

   if ( bReload )
   {
      // Remember expanded items
      //TODO
      //QTreeWidgetItemIterator it( this );
      //while ( *it )
      //{
      //   DirMergeItem* pDMI = static_cast<DirMergeItem*>( *it );
      //   std::map<QString,t_ItemInfo>::iterator i = expandedDirsMap.find( pDMI->m_pMFI->subPath() );
      //   if ( i!=expandedDirsMap.end() )
      //   {
      //      t_ItemInfo& ii = i->second;
      //      pDMI->setExpanded( ii.bExpanded );
      //      //pDMI->m_pMFI->setMergeOperation( ii.eMergeOperation, false ); unsafe, might have changed
      //      pDMI->m_pMFI->m_bOperationComplete = ii.bOperationComplete;
      //      pDMI->setText( s_OpStatusCol, ii.status );
      //   }
      //   ++it;
      //}
   }
   else if (m_bUnfoldSubdirs)
   {
      m_pDirUnfoldAll->trigger();
   }

   return true;
}

void DirectoryMergeWindow::onExpanded()
{
   resizeColumnToContents(s_NameCol);
}


void DirectoryMergeWindow::slotChooseAEverywhere(){ d->setAllMergeOperations( eCopyAToDest ); }

void DirectoryMergeWindow::slotChooseBEverywhere(){ d->setAllMergeOperations( eCopyBToDest ); }

void DirectoryMergeWindow::slotChooseCEverywhere(){ d->setAllMergeOperations( eCopyCToDest ); }

void DirectoryMergeWindow::slotAutoChooseEverywhere()
{
   e_MergeOperation eDefaultMergeOp = d->m_dirC.isValid() ?  eMergeABCToDest :
                                           d->m_bSyncMode ?  eMergeToAB      : eMergeABToDest;
   d->setAllMergeOperations(eDefaultMergeOp );
}

void DirectoryMergeWindow::slotNoOpEverywhere(){ d->setAllMergeOperations(eNoOperation); }

void DirectoryMergeWindow::slotFoldAllSubdirs()
{
   collapseAll();
}

void DirectoryMergeWindow::slotUnfoldAllSubdirs()
{
   expandAll();
}

// Merge current item (merge mode)
void DirectoryMergeWindow::slotCurrentDoNothing() { d->setMergeOperation(currentIndex(), eNoOperation ); }
void DirectoryMergeWindow::slotCurrentChooseA()   { d->setMergeOperation(currentIndex(), d->m_bSyncMode ? eCopyAToB : eCopyAToDest ); }
void DirectoryMergeWindow::slotCurrentChooseB()   { d->setMergeOperation(currentIndex(), d->m_bSyncMode ? eCopyBToA : eCopyBToDest ); }
void DirectoryMergeWindow::slotCurrentChooseC()   { d->setMergeOperation(currentIndex(), eCopyCToDest ); }
void DirectoryMergeWindow::slotCurrentMerge()
{
   bool bThreeDirs = d->m_dirC.isValid();
   d->setMergeOperation(currentIndex(), bThreeDirs ? eMergeABCToDest : eMergeABToDest );
}
void DirectoryMergeWindow::slotCurrentDelete()    { d->setMergeOperation(currentIndex(), eDeleteFromDest ); }
// Sync current item
void DirectoryMergeWindow::slotCurrentCopyAToB()     { d->setMergeOperation(currentIndex(), eCopyAToB ); }
void DirectoryMergeWindow::slotCurrentCopyBToA()     { d->setMergeOperation(currentIndex(), eCopyBToA ); }
void DirectoryMergeWindow::slotCurrentDeleteA()      { d->setMergeOperation(currentIndex(), eDeleteA );  }
void DirectoryMergeWindow::slotCurrentDeleteB()      { d->setMergeOperation(currentIndex(), eDeleteB );  }
void DirectoryMergeWindow::slotCurrentDeleteAAndB()  { d->setMergeOperation(currentIndex(), eDeleteAB ); }
void DirectoryMergeWindow::slotCurrentMergeToA()     { d->setMergeOperation(currentIndex(), eMergeToA ); }
void DirectoryMergeWindow::slotCurrentMergeToB()     { d->setMergeOperation(currentIndex(), eMergeToB ); }
void DirectoryMergeWindow::slotCurrentMergeToAAndB() { d->setMergeOperation(currentIndex(), eMergeToAB ); }


void DirectoryMergeWindow::keyPressEvent( QKeyEvent* e )
{
   if ( (e->QInputEvent::modifiers() & Qt::ControlModifier)!=0 )
   {
      bool bThreeDirs = d->m_dirC.isValid();
      
      MergeFileInfos* pMFI = d->getMFI( currentIndex() );

      if ( pMFI==0 ) 
         return;
      bool bMergeMode = bThreeDirs || !d->m_bSyncMode;
      bool bFTConflict = pMFI==0 ? false : conflictingFileTypes(*pMFI);

      if ( bMergeMode )
      {
         switch(e->key())
         {
         case Qt::Key_1:      if(pMFI->existsInA()){ slotCurrentChooseA(); }  return;
         case Qt::Key_2:      if(pMFI->existsInB()){ slotCurrentChooseB(); }  return;
         case Qt::Key_3:      if(pMFI->existsInC()){ slotCurrentChooseC(); }  return;
         case Qt::Key_Space:  slotCurrentDoNothing();                          return;
         case Qt::Key_4:      if ( !bFTConflict )   { slotCurrentMerge();   }  return;
         case Qt::Key_Delete: slotCurrentDelete();                             return;
         default: break;
         }
      }
      else
      {
         switch(e->key())
         {
         case Qt::Key_1:      if(pMFI->existsInA()){ slotCurrentCopyAToB(); }  return;
         case Qt::Key_2:      if(pMFI->existsInB()){ slotCurrentCopyBToA(); }  return;
         case Qt::Key_Space:  slotCurrentDoNothing();                           return;
         case Qt::Key_4:      if ( !bFTConflict ) { slotCurrentMergeToAAndB(); }  return;
         case Qt::Key_Delete: if( pMFI->existsInA() && pMFI->existsInB() ) slotCurrentDeleteAAndB();
                          else if( pMFI->existsInA() ) slotCurrentDeleteA();
                          else if( pMFI->existsInB() ) slotCurrentDeleteB();
                          return;
         default: break;
         }
      }
   }
   else if ( e->key()==Qt::Key_Return || e->key()==Qt::Key_Enter )
   {
      onDoubleClick( currentIndex() );
      return;
   }

   QTreeView::keyPressEvent(e);
}

void DirectoryMergeWindow::focusInEvent(QFocusEvent*)
{
   updateAvailabilities();
}
void DirectoryMergeWindow::focusOutEvent(QFocusEvent*)
{
   updateAvailabilities();
}

void DirectoryMergeWindow::Data::setAllMergeOperations( e_MergeOperation eDefaultOperation )
{
   if ( KMessageBox::Yes == KMessageBox::warningYesNo(q,
        i18n("This affects all merge operations."),
        i18n("Changing All Merge Operations"),
        KStandardGuiItem::cont(), 
        KStandardGuiItem::cancel() ) )
   {
      for( int i=0; i<rowCount(); ++i )
      {
         calcSuggestedOperation( index(i,0,QModelIndex()) , eDefaultOperation );
      }
   }
}


void DirectoryMergeWindow::Data::compareFilesAndCalcAges( MergeFileInfos& mfi )
{
   std::map<QDateTime,int> dateMap;

   if( mfi.existsInA() )
   {
      dateMap[ mfi.m_pFileInfoA->lastModified() ] = 0;
   }
   if( mfi.existsInB() )
   {
      dateMap[ mfi.m_pFileInfoB->lastModified() ] = 1;
   }
   if( mfi.existsInC() )
   {
      dateMap[ mfi.m_pFileInfoC->lastModified() ] = 2;
   }

   if ( m_pOptions->m_bDmFullAnalysis )
   {
      if( (mfi.existsInA() && mfi.dirA()) || (mfi.existsInB() && mfi.dirB()) || (mfi.existsInC() && mfi.dirC()) )
      {
         // If any input is a directory, don't start any comparison.
         mfi.m_bEqualAB=mfi.existsInA() && mfi.existsInB();
         mfi.m_bEqualAC=mfi.existsInA() && mfi.existsInC();
         mfi.m_bEqualBC=mfi.existsInB() && mfi.existsInC();
      }
      else
      {
         emit q->startDiffMerge(
            mfi.existsInA() ? mfi.m_pFileInfoA->absoluteFilePath() : QString(""),
            mfi.existsInB() ? mfi.m_pFileInfoB->absoluteFilePath() : QString(""),
            mfi.existsInC() ? mfi.m_pFileInfoC->absoluteFilePath() : QString(""),
            "",
            "","","",&mfi.m_totalDiffStatus
            );
         int nofNonwhiteConflicts = mfi.m_totalDiffStatus.nofUnsolvedConflicts + 
            mfi.m_totalDiffStatus.nofSolvedConflicts - mfi.m_totalDiffStatus.nofWhitespaceConflicts;

         if (m_pOptions->m_bDmWhiteSpaceEqual && nofNonwhiteConflicts == 0)
         {
            mfi.m_bEqualAB = mfi.existsInA() && mfi.existsInB();
            mfi.m_bEqualAC = mfi.existsInA() && mfi.existsInC();
            mfi.m_bEqualBC = mfi.existsInB() && mfi.existsInC();
         }
         else
         {
            mfi.m_bEqualAB = mfi.m_totalDiffStatus.bBinaryAEqB;
            mfi.m_bEqualBC = mfi.m_totalDiffStatus.bBinaryBEqC;
            mfi.m_bEqualAC = mfi.m_totalDiffStatus.bBinaryAEqC;
         }
      }
   }
   else
   {
      bool bError;
      QString eqStatus;
      if( mfi.existsInA() && mfi.existsInB() )
      {
         if( mfi.dirA() ) mfi.m_bEqualAB=true;
         else mfi.m_bEqualAB = fastFileComparison( *mfi.m_pFileInfoA, *mfi.m_pFileInfoB, bError, eqStatus );
      }
      if( mfi.existsInA() && mfi.existsInC() )
      {
         if( mfi.dirA() ) mfi.m_bEqualAC=true;
         else mfi.m_bEqualAC = fastFileComparison( *mfi.m_pFileInfoA, *mfi.m_pFileInfoC, bError, eqStatus );
      }
      if( mfi.existsInB() && mfi.existsInC() )
      {
         if (mfi.m_bEqualAB && mfi.m_bEqualAC)
            mfi.m_bEqualBC = true;
         else
         {
            if( mfi.dirB() ) mfi.m_bEqualBC=true;
            else mfi.m_bEqualBC = fastFileComparison( *mfi.m_pFileInfoB, *mfi.m_pFileInfoC, bError, eqStatus );
         }
      }
   }

   if (mfi.isLinkA()!=mfi.isLinkB()) mfi.m_bEqualAB=false;
   if (mfi.isLinkA()!=mfi.isLinkC()) mfi.m_bEqualAC=false;
   if (mfi.isLinkB()!=mfi.isLinkC()) mfi.m_bEqualBC=false;

   if (mfi.dirA()!=mfi.dirB()) mfi.m_bEqualAB=false;
   if (mfi.dirA()!=mfi.dirC()) mfi.m_bEqualAC=false;
   if (mfi.dirB()!=mfi.dirC()) mfi.m_bEqualBC=false;

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
   if ( mfi.existsInC() && mfi.m_ageC==eNotThere )
   {
      mfi.m_ageC = (e_Age)age; ++age;
      mfi.m_bConflictingAges = true;
   }
   if ( mfi.existsInB() && mfi.m_ageB==eNotThere )
   {
      mfi.m_ageB = (e_Age)age; ++age;
      mfi.m_bConflictingAges = true;
   }
   if ( mfi.existsInA() && mfi.m_ageA==eNotThere )
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

static QPixmap* pmNotThere;
static QPixmap* pmNew;
static QPixmap* pmOld;
static QPixmap* pmMiddle;

static QPixmap* pmLink;

static QPixmap* pmDirLink;
static QPixmap* pmFileLink;

static QPixmap* pmNewLink;
static QPixmap* pmOldLink;
static QPixmap* pmMiddleLink;

static QPixmap* pmNewDir;
static QPixmap* pmMiddleDir;
static QPixmap* pmOldDir;

static QPixmap* pmNewDirLink;
static QPixmap* pmMiddleDirLink;
static QPixmap* pmOldDirLink;


static QPixmap colorToPixmap(QColor c)
{
   QPixmap pm(16,16);
   QPainter p(&pm);
   p.setPen( Qt::black );
   p.setBrush( c );
   p.drawRect(0,0,pm.width(),pm.height());
   return pm;
}

static void initPixmaps( QColor newest, QColor oldest, QColor middle, QColor notThere )
{
   if (pmNew==0)
   {
      pmNotThere = new QPixmap;
      pmNew = new QPixmap;
      pmOld = new QPixmap;
      pmMiddle = new QPixmap;

      #include "xpm/link_arrow.xpm"
      pmLink = new QPixmap(link_arrow);

      pmDirLink = new QPixmap;
      pmFileLink = new QPixmap;

      pmNewLink = new QPixmap;
      pmOldLink = new QPixmap;
      pmMiddleLink = new QPixmap;

      pmNewDir = new QPixmap;
      pmMiddleDir = new QPixmap;
      pmOldDir = new QPixmap;

      pmNewDirLink = new QPixmap;
      pmMiddleDirLink = new QPixmap;
      pmOldDirLink = new QPixmap;
   }


   *pmNotThere = colorToPixmap(notThere);
   *pmNew = colorToPixmap(newest);
   *pmOld = colorToPixmap(oldest);
   *pmMiddle = colorToPixmap(middle);

   *pmDirLink = pixCombiner( s_pm_dir, pmLink);
   *pmFileLink = pixCombiner( s_pm_file, pmLink );

   *pmNewLink = pixCombiner( pmNew, pmLink);
   *pmOldLink = pixCombiner( pmOld, pmLink);
   *pmMiddleLink = pixCombiner( pmMiddle, pmLink);

   *pmNewDir = pixCombiner2( pmNew, s_pm_dir);
   *pmMiddleDir = pixCombiner2( pmMiddle, s_pm_dir);
   *pmOldDir = pixCombiner2( pmOld, s_pm_dir);

   *pmNewDirLink = pixCombiner( pmNewDir, pmLink);
   *pmMiddleDirLink = pixCombiner( pmMiddleDir, pmLink);
   *pmOldDirLink = pixCombiner( pmOldDir, pmLink);
}

static QPixmap getOnePixmap( e_Age eAge, bool bLink, bool bDir )
{
   static QPixmap* ageToPm[]=       { pmNew,     pmMiddle,     pmOld,     pmNotThere, s_pm_file  };
   static QPixmap* ageToPmLink[]=   { pmNewLink, pmMiddleLink, pmOldLink, pmNotThere, pmFileLink };
   static QPixmap* ageToPmDir[]=    { pmNewDir,  pmMiddleDir,  pmOldDir,  pmNotThere, s_pm_dir   };
   static QPixmap* ageToPmDirLink[]={ pmNewDirLink, pmMiddleDirLink, pmOldDirLink, pmNotThere, pmDirLink };

   QPixmap** ppPm = bDir ? ( bLink ? ageToPmDirLink : ageToPmDir ):
                           ( bLink ? ageToPmLink    : ageToPm    );

   return *ppPm[eAge];
}

static void setPixmaps( MergeFileInfos& mfi, bool )
{
   if ( mfi.dirA()  || mfi.dirB()  || mfi.dirC() )
   {
      mfi.m_ageA=eNotThere;
      mfi.m_ageB=eNotThere;
      mfi.m_ageC=eNotThere;
      int age = eNew;
      if ( mfi.existsInC() )
      {
         mfi.m_ageC = (e_Age)age;
         if (mfi.m_bEqualAC) mfi.m_ageA = (e_Age)age;
         if (mfi.m_bEqualBC) mfi.m_ageB = (e_Age)age;
         ++age;
      }
      if ( mfi.existsInB() && mfi.m_ageB==eNotThere )
      {
         mfi.m_ageB = (e_Age)age;
         if (mfi.m_bEqualAB) mfi.m_ageA = (e_Age)age;
         ++age;
      }
      if ( mfi.existsInA() && mfi.m_ageA==eNotThere )
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
}

static QModelIndex nextSibling(const QModelIndex& mi)
{
   QModelIndex miParent = mi.parent();
   int currentIdx = mi.row();
   if ( currentIdx+1 < mi.model()->rowCount(miParent) ) 
      return mi.model()->index(mi.row()+1,0,miParent); // next child of parent
   return QModelIndex();
}

// Iterate through the complete tree. Start by specifying QListView::firstChild().
QModelIndex DirectoryMergeWindow::Data::treeIterator( QModelIndex mi, bool bVisitChildren, bool bFindInvisible )
{
   if( mi.isValid() )
   {
      do
      {
         if ( bVisitChildren && mi.model()->rowCount(mi) != 0 )      
            mi = mi.model()->index(0,0,mi);
         else 
         {
            QModelIndex miNextSibling = nextSibling(mi);
            if ( miNextSibling.isValid() )
               mi = miNextSibling;
            else
            {
               mi = mi.parent();
               while ( mi.isValid() )
               {
                  QModelIndex miNextSibling = nextSibling(mi);
                  if( miNextSibling.isValid() ) 
                  { 
                     mi = miNextSibling; 
                     break; 
                  }
                  else
                  { 
                     mi = mi.parent();             
                  }
               }
            }
         }
      }
      while( mi.isValid() && q->isRowHidden(mi.row(),mi.parent()) && !bFindInvisible );
   }
   return mi;
}

void DirectoryMergeWindow::Data::prepareListView( ProgressProxy& pp )
{
   static bool bFirstTime = true;
   if (bFirstTime)
   {
      #include "xpm/file.xpm"
      #include "xpm/folder.xpm"
      // FIXME specify correct icon loader group
      s_pm_dir = new QPixmap( m_pIconLoader->loadIcon("folder", KIconLoader::NoGroup, KIconLoader::Small ) );
      if (s_pm_dir->size()!=QSize(16,16))
      {
         delete s_pm_dir;
         s_pm_dir = new QPixmap( folder_pm );
      }
      s_pm_file= new QPixmap( file_pm );
      bFirstTime=false;
   }

//TODO   clear();
   initPixmaps( m_pOptions->m_newestFileColor, m_pOptions->m_oldestFileColor,
                m_pOptions->m_midAgeFileColor, m_pOptions->m_missingFileColor );

   q->setRootIsDecorated( true );

   bool bCheckC = m_dirC.isValid();

   t_fileMergeMap::iterator j;
   int nrOfFiles = m_fileMergeMap.size();
   int currentIdx = 1;
   QTime t;
   t.start();
   for( j=m_fileMergeMap.begin(); j!=m_fileMergeMap.end(); ++j )
   {
      MergeFileInfos& mfi = j.value();

      // const QString& fileName = j->first;
      const QString& fileName = mfi.subPath();

      pp.setInformation(
         i18n("Processing ") + QString::number(currentIdx) +" / "+ QString::number(nrOfFiles)
         +"\n" + fileName, double(currentIdx) / nrOfFiles, false );
      if ( pp.wasCancelled() ) break;
      ++currentIdx;

      // The comparisons and calculations for each file take place here.
      compareFilesAndCalcAges( mfi );

      // Get dirname from fileName: Search for "/" from end:
      int pos = fileName.lastIndexOf('/');
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
         m_pRoot->m_children.push_back(&mfi); //new DirMergeItem( this, filePart, &mfi );
         mfi.m_pParent = m_pRoot;
      }
      else
      {
         FileAccess* pFA = mfi.m_pFileInfoA ? mfi.m_pFileInfoA : mfi.m_pFileInfoB ? mfi.m_pFileInfoB : mfi.m_pFileInfoC;
         MergeFileInfos& dirMfi = pFA->parent() ? m_fileMergeMap[FileKey(*pFA->parent())] : *m_pRoot; // parent
      
         dirMfi.m_children.push_back(&mfi);//new DirMergeItem( dirMfi.m_pDMI, filePart, &mfi );
         mfi.m_pParent = &dirMfi;

      //   // Equality for parent dirs is set in updateFileVisibilities()
      }

      setPixmaps( mfi, bCheckC );
   }
   reset();
}

static bool conflictingFileTypes(MergeFileInfos& mfi)
{
   // Now check if file/dir-types fit.
   if ( mfi.isLinkA() || mfi.isLinkB() || mfi.isLinkC() )
   {
      if ( (mfi.existsInA() && ! mfi.isLinkA())  ||
           (mfi.existsInB() && ! mfi.isLinkB())  ||
           (mfi.existsInC() && ! mfi.isLinkC()) )
      {
         return true;
      }
   }

   if ( mfi.dirA() || mfi.dirB() || mfi.dirC() )
   {
      if ( (mfi.existsInA() && ! mfi.dirA())  ||
           (mfi.existsInB() && ! mfi.dirB())  ||
           (mfi.existsInC() && ! mfi.dirC()) )
      {
         return true;
      }
   }
   return false;
}

void DirectoryMergeWindow::Data::calcSuggestedOperation( const QModelIndex& mi, e_MergeOperation eDefaultMergeOp )
{
   MergeFileInfos* pMFI = getMFI(mi);
   if ( pMFI==0 )
      return;
   MergeFileInfos& mfi = *pMFI;
   bool bCheckC = m_dirC.isValid();
   bool bCopyNewer = m_pOptions->m_bDmCopyNewer;
   bool bOtherDest = !( (m_dirDestInternal.absoluteFilePath() == m_dirA.absoluteFilePath()) || 
                        (m_dirDestInternal.absoluteFilePath() == m_dirB.absoluteFilePath()) ||
                        (bCheckC && m_dirDestInternal.absoluteFilePath() == m_dirC.absoluteFilePath()) );
                     

   if ( eDefaultMergeOp == eMergeABCToDest && !bCheckC ) { eDefaultMergeOp = eMergeABToDest; }
   if ( eDefaultMergeOp == eMergeToAB      &&  bCheckC ) { assert(false); }

   if ( eDefaultMergeOp == eMergeToA || eDefaultMergeOp == eMergeToB ||
        eDefaultMergeOp == eMergeABCToDest || eDefaultMergeOp == eMergeABToDest || eDefaultMergeOp == eMergeToAB )
   {
      if ( !bCheckC )
      {
         if ( mfi.m_bEqualAB )
         {
            setMergeOperation( mi, bOtherDest ? eCopyBToDest : eNoOperation );
         }
         else if ( mfi.existsInA() && mfi.existsInB() )
         {
            if ( !bCopyNewer || mfi.dirA() )
               setMergeOperation( mi, eDefaultMergeOp );
            else if (  bCopyNewer && mfi.m_bConflictingAges )
            {
               setMergeOperation( mi, eConflictingAges );
            }
            else
            {
               if ( mfi.m_ageA == eNew )
                  setMergeOperation( mi, eDefaultMergeOp == eMergeToAB ?  eCopyAToB : eCopyAToDest );
               else
                  setMergeOperation( mi, eDefaultMergeOp == eMergeToAB ?  eCopyBToA : eCopyBToDest );
            }
         }
         else if ( !mfi.existsInA() && mfi.existsInB() )
         {
            if ( eDefaultMergeOp==eMergeABToDest  ) setMergeOperation( mi, eCopyBToDest );
            else if ( eDefaultMergeOp==eMergeToB )  setMergeOperation( mi, eNoOperation );
            else                                    setMergeOperation( mi, eCopyBToA );
         }
         else if ( mfi.existsInA() && !mfi.existsInB() )
         {
            if ( eDefaultMergeOp==eMergeABToDest  ) setMergeOperation( mi, eCopyAToDest );
            else if ( eDefaultMergeOp==eMergeToA )  setMergeOperation( mi, eNoOperation );
            else                                    setMergeOperation( mi, eCopyAToB );
         }
         else //if ( !mfi.existsInA() && !mfi.existsInB() )
         {
            setMergeOperation( mi, eNoOperation ); assert(false);
         }
      }
      else
      {
         if ( mfi.m_bEqualAB && mfi.m_bEqualAC )
         {
            setMergeOperation( mi, bOtherDest ? eCopyCToDest : eNoOperation );
         }
         else if ( mfi.existsInA() && mfi.existsInB() && mfi.existsInC())
         {
            if ( mfi.m_bEqualAB )
               setMergeOperation( mi, eCopyCToDest );
            else if ( mfi.m_bEqualAC )
               setMergeOperation( mi, eCopyBToDest );
            else if ( mfi.m_bEqualBC )
               setMergeOperation( mi, eCopyCToDest );
            else
               setMergeOperation( mi, eMergeABCToDest );
         }
         else if ( mfi.existsInA() && mfi.existsInB() && !mfi.existsInC() )
         {
            if ( mfi.m_bEqualAB )
               setMergeOperation( mi, eDeleteFromDest );
            else
               setMergeOperation( mi, eChangedAndDeleted );
         }
         else if ( mfi.existsInA() && !mfi.existsInB() && mfi.existsInC() )
         {
            if ( mfi.m_bEqualAC )
               setMergeOperation( mi, eDeleteFromDest );
            else
               setMergeOperation( mi, eChangedAndDeleted );
         }
         else if ( !mfi.existsInA() && mfi.existsInB() && mfi.existsInC() )
         {
            if ( mfi.m_bEqualBC )
               setMergeOperation( mi, eCopyCToDest );
            else
               setMergeOperation( mi, eMergeABCToDest );
         }
         else if ( !mfi.existsInA() && !mfi.existsInB() && mfi.existsInC() )
         {
            setMergeOperation( mi, eCopyCToDest );
         }
         else if ( !mfi.existsInA() && mfi.existsInB() && !mfi.existsInC() )
         {
            setMergeOperation( mi, eCopyBToDest );
         }
         else if ( mfi.existsInA() && !mfi.existsInB() && !mfi.existsInC())
         {
            setMergeOperation( mi, eDeleteFromDest );
         }
         else //if ( !mfi.existsInA() && !mfi.existsInB() && !mfi.existsInC() )
         {
            setMergeOperation( mi, eNoOperation ); assert(false);
         }
      }

      // Now check if file/dir-types fit.
      if ( conflictingFileTypes(mfi) )
      {
         setMergeOperation( mi, eConflictingFileTypes );
      }
   }
   else
   {
      e_MergeOperation eMO = eDefaultMergeOp;
      switch ( eDefaultMergeOp )
      {
      case eConflictingFileTypes:
      case eChangedAndDeleted:
      case eConflictingAges:
      case eDeleteA:
      case eDeleteB:
      case eDeleteAB:
      case eDeleteFromDest:
      case eNoOperation: break;
      case eCopyAToB:    if ( !mfi.existsInA() ) { eMO = eDeleteB; }        break;
      case eCopyBToA:    if ( !mfi.existsInB() ) { eMO = eDeleteA; }        break;
      case eCopyAToDest: if ( !mfi.existsInA() ) { eMO = eDeleteFromDest; } break;
      case eCopyBToDest: if ( !mfi.existsInB() ) { eMO = eDeleteFromDest; } break;
      case eCopyCToDest: if ( !mfi.existsInC() ) { eMO = eDeleteFromDest; } break;

      case eMergeToA:
      case eMergeToB:
      case eMergeToAB:
      case eMergeABCToDest:
      case eMergeABToDest:
      default:
         assert(false);
      }
      setMergeOperation( mi, eMO );
   }
}

void DirectoryMergeWindow::onDoubleClick( const QModelIndex& mi )
{
   if ( ! mi.isValid() ) 
      return;

   d->m_bSimulatedMergeStarted = false;
   if ( d->m_bDirectoryMerge )
      mergeCurrentFile();
   else
      compareCurrentFile();
}

void DirectoryMergeWindow::currentChanged ( const QModelIndex & current, const QModelIndex & previous )
{
   QTreeView::currentChanged( current, previous );
   MergeFileInfos* pMFI = d->getMFI( current );
   if ( pMFI==0 ) 
      return;

   d->m_pDirectoryMergeInfo->setInfo( d->m_dirA, d->m_dirB, d->m_dirC, d->m_dirDestInternal, *pMFI );
}

void DirectoryMergeWindow::mousePressEvent( QMouseEvent* e )
{
   QTreeView::mousePressEvent(e);
   QModelIndex mi = indexAt( e->pos() );
   int c = mi.column();
   QPoint p = e->globalPos();
   MergeFileInfos* pMFI = d->getMFI( mi );
   if ( pMFI==0 )
      return;
   MergeFileInfos& mfi = *pMFI;

   if ( c==s_OpCol )
   {
      bool bThreeDirs = d->m_dirC.isValid();

      KMenu m(this);
      if ( bThreeDirs )
      {
         m.addAction( d->m_pDirCurrentDoNothing );
         int count=0;
         if ( mfi.existsInA() ) { m.addAction( d->m_pDirCurrentChooseA ); ++count;  }
         if ( mfi.existsInB() ) { m.addAction( d->m_pDirCurrentChooseB ); ++count;  }
         if ( mfi.existsInC() ) { m.addAction( d->m_pDirCurrentChooseC ); ++count;  }
         if ( !conflictingFileTypes(mfi) && count>1 ) m.addAction( d->m_pDirCurrentMerge );
         m.addAction( d->m_pDirCurrentDelete );
      }
      else if ( d->m_bSyncMode )
      {
         m.addAction( d->m_pDirCurrentSyncDoNothing );
         if ( mfi.existsInA() ) m.addAction( d->m_pDirCurrentSyncCopyAToB );
         if ( mfi.existsInB() ) m.addAction( d->m_pDirCurrentSyncCopyBToA );
         if ( mfi.existsInA() ) m.addAction( d->m_pDirCurrentSyncDeleteA );
         if ( mfi.existsInB() ) m.addAction( d->m_pDirCurrentSyncDeleteB );
         if ( mfi.existsInA() && mfi.existsInB() )
         {
            m.addAction( d->m_pDirCurrentSyncDeleteAAndB );
            if ( !conflictingFileTypes(mfi))
            {
               m.addAction( d->m_pDirCurrentSyncMergeToA );
               m.addAction( d->m_pDirCurrentSyncMergeToB );
               m.addAction( d->m_pDirCurrentSyncMergeToAAndB );
            }
         }
      }
      else
      {
         m.addAction( d->m_pDirCurrentDoNothing );
         if ( mfi.existsInA() ) { m.addAction( d->m_pDirCurrentChooseA ); }
         if ( mfi.existsInB() ) { m.addAction( d->m_pDirCurrentChooseB ); }
         if ( !conflictingFileTypes(mfi) && mfi.existsInA()  &&  mfi.existsInB() ) m.addAction( d->m_pDirCurrentMerge );
         m.addAction( d->m_pDirCurrentDelete );
      }

      m.exec( p );
   }
   else if ( c == s_ACol || c==s_BCol || c==s_CCol )
   {
      QString itemPath;
      if      ( c == s_ACol && mfi.existsInA() ){ itemPath = d->fullNameA(mfi); }
      else if ( c == s_BCol && mfi.existsInB() ){ itemPath = d->fullNameB(mfi); }
      else if ( c == s_CCol && mfi.existsInC() ){ itemPath = d->fullNameC(mfi); }

      if (!itemPath.isEmpty())
      {
         d->selectItemAndColumn( mi, e->button()==Qt::RightButton );
      }
   }
}

void DirectoryMergeWindow::contextMenuEvent(QContextMenuEvent* e)
{
   QModelIndex mi = indexAt( e->pos() );
   int c = mi.column();
   QPoint p = e->globalPos();

   MergeFileInfos* pMFI = d->getMFI(mi);
   if (pMFI==0)
      return;
   if ( c == s_ACol || c==s_BCol || c==s_CCol )
   {
      QString itemPath;
      if      ( c == s_ACol && pMFI->existsInA() ){ itemPath = d->fullNameA(*pMFI); }
      else if ( c == s_BCol && pMFI->existsInB() ){ itemPath = d->fullNameB(*pMFI); }
      else if ( c == s_CCol && pMFI->existsInC() ){ itemPath = d->fullNameC(*pMFI); }

      if (!itemPath.isEmpty())
      {
         d->selectItemAndColumn(mi, true);
         KMenu m(this);
         m.addAction( d->m_pDirCompareExplicit );
         m.addAction( d->m_pDirMergeExplicit );

#ifndef _WIN32
         m.exec( p );
#else
         void showShellContextMenu( const QString&, QPoint, QWidget*, QMenu* );
         showShellContextMenu( itemPath, p, this, &m );
#endif
      }
   }
}

QString DirectoryMergeWindow::Data::getFileName( const QModelIndex& mi )
{
   MergeFileInfos* pMFI = getMFI( mi );
   if ( pMFI != 0 )
   {
      return mi.column() == s_ACol ? pMFI->m_pFileInfoA->absoluteFilePath() :
             mi.column() == s_BCol ? pMFI->m_pFileInfoB->absoluteFilePath() :
             mi.column() == s_CCol ? pMFI->m_pFileInfoC->absoluteFilePath() :
             QString("");
   }
   return "";
}

bool DirectoryMergeWindow::Data::isDir( const QModelIndex& mi )
{
   MergeFileInfos* pMFI = getMFI( mi );
   if ( pMFI != 0 )
   {
      return mi.column() == s_ACol ? pMFI->dirA() :
             mi.column() == s_BCol ? pMFI->dirB() :
                                     pMFI->dirC();
   }
   return false;
}


void DirectoryMergeWindow::Data::selectItemAndColumn(const QModelIndex& mi, bool bContextMenu)
{
   if ( bContextMenu && ( mi==m_selection1Index  ||  mi==m_selection2Index  ||  mi==m_selection3Index ) ) 
      return;

   QModelIndex old1=m_selection1Index;
   QModelIndex old2=m_selection2Index;
   QModelIndex old3=m_selection3Index;

   bool bReset = false;

   if ( m_selection1Index.isValid() )
   {
      if (isDir( m_selection1Index )!=isDir( mi ))
         bReset = true;
   }

   if ( bReset || m_selection3Index.isValid() || mi==m_selection1Index || mi==m_selection2Index || mi==m_selection3Index )
   {
      // restart
      m_selection1Index = QModelIndex();
      m_selection2Index = QModelIndex();
      m_selection3Index = QModelIndex();
   }
   else if ( !m_selection1Index.isValid() )
   {
      m_selection1Index = mi;
      m_selection2Index = QModelIndex();
      m_selection3Index = QModelIndex();
   }
   else if ( !m_selection2Index.isValid() )
   {
      m_selection2Index = mi;
      m_selection3Index = QModelIndex();
   }
   else if ( !m_selection3Index.isValid() )
   {
      m_selection3Index = mi;
   }
   if (old1.isValid()) dataChanged( old1, old1 );
   if (old2.isValid()) dataChanged( old2, old2 );
   if (old3.isValid()) dataChanged( old3, old3 );
   if (m_selection1Index.isValid()) dataChanged( m_selection1Index, m_selection1Index );
   if (m_selection2Index.isValid()) dataChanged( m_selection2Index, m_selection2Index );
   if (m_selection3Index.isValid()) dataChanged( m_selection3Index, m_selection3Index );
   emit q->updateAvailabilities();
}

//TODO
//void DirMergeItem::init(MergeFileInfos* pMFI)
//{
//   pMFI->m_pDMI = this;
//   m_pMFI = pMFI;
//   TotalDiffStatus& tds = pMFI->m_totalDiffStatus;
//   if ( m_pMFI->dirA() || m_pMFI->dirB() || m_pMFI->dirC() )
//   {
//   }
//   else
//   {
//      setText( s_UnsolvedCol, QString::number( tds.nofUnsolvedConflicts ) );
//      setText( s_SolvedCol,   QString::number( tds.nofSolvedConflicts ) );
//      setText( s_NonWhiteCol, QString::number( tds.nofUnsolvedConflicts + tds.nofSolvedConflicts - tds.nofWhitespaceConflicts ) );
//      setText( s_WhiteCol,    QString::number( tds.nofWhitespaceConflicts ) );
//   }
//   setSizeHint( s_ACol, QSize(17,17) ); // Iconsize
//   setSizeHint( s_BCol, QSize(17,17) ); // Iconsize
//   setSizeHint( s_CCol, QSize(17,17) ); // Iconsize
//}

class MfiLessThan
{
   int m_sortColumn;
public:
   MfiLessThan( int sortColumn )
   : m_sortColumn( sortColumn )
   {
   }
   bool operator()( MergeFileInfos* pMFI1, MergeFileInfos* pMFI2 )
   {
      bool bDir1 =  pMFI1->dirA() || pMFI1->dirB() || pMFI1->dirC();
      bool bDir2 =  pMFI2->dirA() || pMFI2->dirB() || pMFI2->dirC();
      if ( bDir1 == bDir2 )
      {
         //if(col==s_UnsolvedCol || col==s_SolvedCol || col==s_NonWhiteCol || col==s_WhiteCol)
         //   return text(col).toInt() > i.text(col).toInt();
         //if ( s_sortColumn == s_NameCol )
            return pMFI1->fileName().compare( pMFI2->fileName(), Qt::CaseInsensitive )<0;
         //else
         //   return QTreeWidgetItem::operator<(i);      
      }
      else
         return bDir1;
   }
};

static void sortHelper( MergeFileInfos* pMFI, int sortColumn, Qt::SortOrder order )
{
   qSort( pMFI->m_children.begin(), pMFI->m_children.end(), MfiLessThan(sortColumn) );
   
   if ( order == Qt::DescendingOrder )
      std::reverse( pMFI->m_children.begin(), pMFI->m_children.end() );
   
   for( int i=0; i<pMFI->m_children.count(); ++i )
      sortHelper( pMFI->m_children[i], sortColumn, order );
}

void DirectoryMergeWindow::Data::sort( int column, Qt::SortOrder order )
{
   sortHelper( m_pRoot, column, order );
   reset();
}

//
//DirMergeItem::~DirMergeItem()
//{
//   m_pMFI->m_pDMI = 0;
//}

void DirectoryMergeWindow::Data::setMergeOperation( const QModelIndex& mi, e_MergeOperation eMOp, bool bRecursive )
{
   MergeFileInfos* pMFI = getMFI( mi );
   if ( pMFI == 0 )
      return;

   MergeFileInfos& mfi = *pMFI;

   if ( eMOp != mfi.m_eMergeOperation )
   {
      mfi.m_bOperationComplete = false;
      setOpStatus( mi, eOpStatusNone );
   }

   mfi.m_eMergeOperation = eMOp;
   QString s;
   if ( bRecursive )
   {
      e_MergeOperation eChildrenMergeOp = mfi.m_eMergeOperation;
      if ( eChildrenMergeOp == eConflictingFileTypes ) eChildrenMergeOp = eMergeABCToDest;
      for( int childIdx=0; childIdx<mfi.m_children.count(); ++childIdx )
      {
         calcSuggestedOperation( index( childIdx, 0, mi ), eChildrenMergeOp );
      }
   }
}

void DirectoryMergeWindow::compareCurrentFile()
{
   if (!d->canContinue()) return;

   if ( d->m_bRealMergeStarted )
   {
      KMessageBox::sorry(this,i18n("This operation is currently not possible."),i18n("Operation Not Possible"));
      return;
   }

   if ( MergeFileInfos* pMFI = d->getMFI(currentIndex()) )
   {
      if ( !(pMFI->dirA() || pMFI->dirB() || pMFI->dirC()) )
      {
         emit startDiffMerge(
            pMFI->existsInA() ? pMFI->m_pFileInfoA->absoluteFilePath() : QString(""),
            pMFI->existsInB() ? pMFI->m_pFileInfoB->absoluteFilePath() : QString(""),
            pMFI->existsInC() ? pMFI->m_pFileInfoC->absoluteFilePath() : QString(""),
            "",
            "","","",0
            );
      }
   }
   emit updateAvailabilities();
}


void DirectoryMergeWindow::slotCompareExplicitlySelectedFiles()
{
   if ( ! d->isDir(d->m_selection1Index) && !d->canContinue() ) return;

   if ( d->m_bRealMergeStarted )
   {
      KMessageBox::sorry(this,i18n("This operation is currently not possible."),i18n("Operation Not Possible"));
      return;
   }

   emit startDiffMerge(
      d->getFileName( d->m_selection1Index ),
      d->getFileName( d->m_selection2Index ),
      d->getFileName( d->m_selection3Index ),
      "",
      "","","",0
      );
   d->m_selection1Index=QModelIndex();
   d->m_selection2Index=QModelIndex();
   d->m_selection3Index=QModelIndex();

   emit updateAvailabilities();
   update();
}

void DirectoryMergeWindow::slotMergeExplicitlySelectedFiles()
{
   if ( ! d->isDir(d->m_selection1Index) && !d->canContinue() ) return;

   if ( d->m_bRealMergeStarted )
   {
      KMessageBox::sorry(this,i18n("This operation is currently not possible."),i18n("Operation Not Possible"));
      return;
   }

   QString fn1 = d->getFileName( d->m_selection1Index );
   QString fn2 = d->getFileName( d->m_selection2Index );
   QString fn3 = d->getFileName( d->m_selection3Index );

   emit startDiffMerge( fn1, fn2, fn3, 
      fn3.isEmpty() ? fn2 : fn3,
      "","","",0
      );
   d->m_selection1Index=QModelIndex();
   d->m_selection2Index=QModelIndex();
   d->m_selection3Index=QModelIndex();

   emit updateAvailabilities();
   update();
}

bool DirectoryMergeWindow::isFileSelected()
{
   if ( MergeFileInfos* pMFI = d->getMFI(currentIndex()) )
   {
      return ! (pMFI->dirA() || pMFI->dirB() || pMFI->dirC() || conflictingFileTypes(*pMFI) );
   }
   return false;
}

void DirectoryMergeWindow::mergeResultSaved(const QString& fileName)
{
   QModelIndex mi = (d->m_mergeItemList.empty() || d->m_currentIndexForOperation==d->m_mergeItemList.end() )
                                               ? QModelIndex()
                                               : *d->m_currentIndexForOperation;

   MergeFileInfos* pMFI = d->getMFI(mi);
   if ( pMFI==0 )
   {
      KMessageBox::error( this, i18n("This should never happen: \n\nmergeResultSaved: m_pMFI=0\n\nIf you know how to reproduce this, please contact the program author."),i18n("Program Error") );
      return;
   }
   if ( fileName == d->fullNameDest(*pMFI) )
   {
      MergeFileInfos& mfi = *pMFI;
      if ( mfi.m_eMergeOperation==eMergeToAB )
      {
         bool bSuccess = d->copyFLD( d->fullNameB(mfi), d->fullNameA(mfi) );
         if (!bSuccess)
         {
            KMessageBox::error(this, i18n("An error occurred while copying.\n"), i18n("Error") );
            d->m_pStatusInfo->setWindowTitle(i18n("Merge Error"));
            d->m_pStatusInfo->exec();
            //if ( m_pStatusInfo->firstChild()!=0 )
            //   m_pStatusInfo->ensureItemVisible( m_pStatusInfo->last() );
            d->m_bError = true;
            d->setOpStatus( mi, eOpStatusError );
            mfi.m_eMergeOperation = eCopyBToA;
            return;
         }
      }
      d->setOpStatus( mi, eOpStatusDone );
      pMFI->m_bOperationComplete = true;
      if ( d->m_mergeItemList.size()==1 )
      {
         d->m_mergeItemList.clear();
         d->m_bRealMergeStarted=false;
      }
   }

   emit updateAvailabilities();
}

bool DirectoryMergeWindow::Data::canContinue()
{
   bool bCanContinue=false;
   q->checkIfCanContinue( &bCanContinue );
   if ( bCanContinue && !m_bError )
   {
      QModelIndex mi = (m_mergeItemList.empty() || m_currentIndexForOperation==m_mergeItemList.end() ) ? QModelIndex() : *m_currentIndexForOperation;
      MergeFileInfos* pMFI = getMFI(mi);
      if ( pMFI  && ! pMFI->m_bOperationComplete )
      {
         setOpStatus( mi, eOpStatusNotSaved );
         pMFI->m_bOperationComplete = true;
         if ( m_mergeItemList.size()==1 )
         {
            m_mergeItemList.clear();
            m_bRealMergeStarted=false;
         }
      }
   }
   return bCanContinue;
}

bool DirectoryMergeWindow::Data::executeMergeOperation( MergeFileInfos& mfi, bool& bSingleFileMerge )
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
      KMessageBox::error( q, i18n("Unknown merge operation. (This must never happen!)"), i18n("Error") );
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
                                    mfi.existsInA() ? fullNameA(mfi) : QString(""),
                                    mfi.existsInB() ? fullNameB(mfi) : QString(""),
                                    mfi.existsInC() ? fullNameC(mfi) : QString(""),
                                    destName, bSingleFileMerge );
                        break;
   default:
      KMessageBox::error( q, i18n("Unknown merge operation."), i18n("Error") );
      assert(false);
   }

   return bSuccess;
}


// Check if the merge can start, and prepare the m_mergeItemList which then contains all
// items that must be merged.
void DirectoryMergeWindow::Data::prepareMergeStart( const QModelIndex& miBegin, const QModelIndex& miEnd, bool bVerbose )
{
   if ( bVerbose )
   {
      int status = KMessageBox::warningYesNoCancel(q,
         i18n("The merge is about to begin.\n\n"
         "Choose \"Do it\" if you have read the instructions and know what you are doing.\n"
         "Choosing \"Simulate it\" will tell you what would happen.\n\n"
         "Be aware that this program still has beta status "
         "and there is NO WARRANTY whatsoever! Make backups of your vital data!"),
         i18n("Starting Merge"), 
         KGuiItem( i18n("Do It") ), 
         KGuiItem( i18n("Simulate It") ) );
      if (status==KMessageBox::Yes)      m_bRealMergeStarted = true;
      else if (status==KMessageBox::No ) m_bSimulatedMergeStarted = true;
      else return;
   }
   else
   {
      m_bRealMergeStarted = true;
   }

   m_mergeItemList.clear();
   if ( !miBegin.isValid() )
      return;

   for( QModelIndex mi = miBegin; mi!=miEnd; mi = treeIterator( mi ) )
   {
      MergeFileInfos* pMFI = getMFI( mi );
      if ( pMFI && ! pMFI->m_bOperationComplete )
      {
         m_mergeItemList.push_back(mi);
         QString errorText;
         if (pMFI->m_eMergeOperation == eConflictingFileTypes )
         {
            errorText = i18n("The highlighted item has a different type in the different directories. Select what to do.");
         }
         if (pMFI->m_eMergeOperation == eConflictingAges )
         {
            errorText = i18n("The modification dates of the file are equal but the files are not. Select what to do.");
         }
         if (pMFI->m_eMergeOperation == eChangedAndDeleted )
         {
            errorText = i18n("The highlighted item was changed in one directory and deleted in the other. Select what to do.");
         }
         if ( !errorText.isEmpty() )
         {
            q->scrollTo( mi, QAbstractItemView::EnsureVisible );
            q->setCurrentIndex( mi );
            KMessageBox::error(q, errorText, i18n("Error"));
            m_mergeItemList.clear();
            m_bRealMergeStarted=false;
            return;
         }
      }
   }

   m_currentIndexForOperation = m_mergeItemList.begin();
   return;
}

void DirectoryMergeWindow::slotRunOperationForCurrentItem()
{
   if ( ! d->canContinue() ) return;

   bool bVerbose = false;
   if ( d->m_mergeItemList.empty() )
   {
      QModelIndex miBegin = currentIndex();
      QModelIndex miEnd = d->treeIterator(miBegin,false,false); // find next visible sibling (no children)

      d->prepareMergeStart( miBegin, miEnd, bVerbose );
      d->mergeContinue(true, bVerbose);
   }
   else
      d->mergeContinue(false, bVerbose);
}

void DirectoryMergeWindow::slotRunOperationForAllItems()
{
   if ( ! d->canContinue() ) return;

   bool bVerbose = true;
   if ( d->m_mergeItemList.empty() )
   {
      QModelIndex miBegin = d->rowCount()>0 ? d->index(0,0,QModelIndex()) : QModelIndex();

      d->prepareMergeStart( miBegin, QModelIndex(), bVerbose );
      d->mergeContinue(true, bVerbose);
   }
   else
      d->mergeContinue(false, bVerbose);
}

void DirectoryMergeWindow::mergeCurrentFile()
{
   if (!d->canContinue()) return;

   if ( d->m_bRealMergeStarted )
   {
      KMessageBox::sorry(this,i18n("This operation is currently not possible because directory merge is currently running."),i18n("Operation Not Possible"));
      return;
   }

   if ( isFileSelected() )
   {
      MergeFileInfos* pMFI = d->getMFI( currentIndex() );
      if ( pMFI != 0 )
      {
         MergeFileInfos& mfi = *pMFI;
         d->m_mergeItemList.clear();
         d->m_mergeItemList.push_back( currentIndex() );
         d->m_currentIndexForOperation = d->m_mergeItemList.begin();
         bool bDummy=false;
         d->mergeFLD(
            mfi.existsInA() ? mfi.m_pFileInfoA->absoluteFilePath() : QString(""),
            mfi.existsInB() ? mfi.m_pFileInfoB->absoluteFilePath() : QString(""),
            mfi.existsInC() ? mfi.m_pFileInfoC->absoluteFilePath() : QString(""),
            d->fullNameDest(mfi),
            bDummy
            );
      }
   }
   emit updateAvailabilities();
}


// When bStart is true then m_currentIndexForOperation must still be processed.
// When bVerbose is true then a messagebox will tell when the merge is complete.
void DirectoryMergeWindow::Data::mergeContinue(bool bStart, bool bVerbose)
{
   ProgressProxy pp;
   if ( m_mergeItemList.empty() )
      return;

   int nrOfItems = 0;
   int nrOfCompletedItems = 0;
   int nrOfCompletedSimItems = 0;

   // Count the number of completed items (for the progress bar).
   for( MergeItemList::iterator i = m_mergeItemList.begin(); i!=m_mergeItemList.end(); ++i )
   {
      MergeFileInfos* pMFI = getMFI( *i );
      ++nrOfItems;
      if ( pMFI->m_bOperationComplete )
         ++nrOfCompletedItems;
      if ( pMFI->m_bSimOpComplete )
         ++nrOfCompletedSimItems;
   }

   m_pStatusInfo->hide();
   m_pStatusInfo->clear();

   QModelIndex miCurrent = m_currentIndexForOperation==m_mergeItemList.end() ? QModelIndex() : *m_currentIndexForOperation;

   bool bContinueWithCurrentItem = bStart;  // true for first item, else false
   bool bSkipItem = false;
   if ( !bStart && m_bError && miCurrent.isValid() )
   {
      int status = KMessageBox::warningYesNoCancel(q,
         i18n("There was an error in the last step.\n"
         "Do you want to continue with the item that caused the error or do you want to skip this item?"),
         i18n("Continue merge after an error"), 
         KGuiItem( i18n("Continue With Last Item") ), 
         KGuiItem( i18n("Skip Item") ) );
      if      (status==KMessageBox::Yes) bContinueWithCurrentItem = true;
      else if (status==KMessageBox::No ) bSkipItem = true;
      else return;
      m_bError = false;
   }

   bool bSuccess = true;
   bool bSingleFileMerge = false;
   bool bSim = m_bSimulatedMergeStarted;
   while( bSuccess )
   {
      MergeFileInfos* pMFI = getMFI(miCurrent);
      if ( pMFI==0 )
      {
         m_mergeItemList.clear();
         m_bRealMergeStarted=false;
         break;
      }

      if ( pMFI!=0 && !bContinueWithCurrentItem )
      {
         if ( bSim )
         {
            if( rowCount(miCurrent)==0 )
            {
               pMFI->m_bSimOpComplete = true;
            }
         }
         else
         {
            if( rowCount( miCurrent )==0 )
            {
               if( !pMFI->m_bOperationComplete )
               {
                  setOpStatus( miCurrent, bSkipItem ? eOpStatusSkipped : eOpStatusDone );
                  pMFI->m_bOperationComplete = true;
                  bSkipItem = false;
               }
            }
            else
            {
               setOpStatus( miCurrent, eOpStatusInProgress );
            }
         }
      }

      if ( ! bContinueWithCurrentItem )
      {
         // Depth first
         QModelIndex miPrev = miCurrent;
         ++m_currentIndexForOperation;
         miCurrent = m_currentIndexForOperation==m_mergeItemList.end() ? QModelIndex() : *m_currentIndexForOperation;
         if ( (!miCurrent.isValid() || miCurrent.parent()!=miPrev.parent()) && miPrev.parent().isValid() )
         {
            // Check if the parent may be set to "Done"
            QModelIndex miParent = miPrev.parent();
            bool bDone = true;
            while ( bDone && miParent.isValid() )
            {
               for( int childIdx = 0; childIdx<rowCount(miParent); ++childIdx )
               {
                  MergeFileInfos* pMFI = getMFI( index(childIdx,0,miParent));
                  if ( (!bSim && ! pMFI->m_bOperationComplete)   ||  (bSim && pMFI->m_bSimOpComplete) )
                  {
                     bDone=false;
                     break;
                  }
               }
               if ( bDone )
               {
                  MergeFileInfos* pMFI = getMFI(miParent);
                  if (bSim)
                     pMFI->m_bSimOpComplete = bDone;
                  else
                  {
                     setOpStatus( miParent, eOpStatusDone );
                     pMFI->m_bOperationComplete = bDone;
                  }
               }
               miParent = miParent.parent();
            }
         }
      }

      if ( !miCurrent.isValid() ) // end?
      {
         if ( m_bRealMergeStarted )
         {
            if (bVerbose)
            {
               KMessageBox::information( q, i18n("Merge operation complete."), i18n("Merge Complete") );
            }
            m_bRealMergeStarted = false;
            m_pStatusInfo->setWindowTitle(i18n("Merge Complete"));
         }
         if ( m_bSimulatedMergeStarted )
         {
            m_bSimulatedMergeStarted = false;
            QModelIndex mi = rowCount()>0 ? index(0,0,QModelIndex()) : QModelIndex();
            for( ; mi.isValid(); mi=treeIterator(mi) )
            {
               getMFI( mi )->m_bSimOpComplete = false;
            }
            m_pStatusInfo->setWindowTitle(i18n("Simulated merge complete: Check if you agree with the proposed operations."));
            m_pStatusInfo->exec();
         }
         m_mergeItemList.clear();
         m_bRealMergeStarted=false;
         return;
      }

      MergeFileInfos& mfi = *getMFI(miCurrent);

      pp.setInformation( mfi.subPath(),
         bSim ? double(nrOfCompletedSimItems)/nrOfItems : double(nrOfCompletedItems)/nrOfItems,
         false // bRedrawUpdate
         );

      bSuccess = executeMergeOperation( mfi, bSingleFileMerge );  // Here the real operation happens.

      if ( bSuccess )
      {
         if(bSim) ++nrOfCompletedSimItems;
         else     ++nrOfCompletedItems;
         bContinueWithCurrentItem = false;
      }

      if( pp.wasCancelled() )
         break;
   }  // end while

   //g_pProgressDialog->hide();

   q->setCurrentIndex( miCurrent );
   q->scrollTo( miCurrent, EnsureVisible );
   if ( !bSuccess &&  !bSingleFileMerge )
   {
      KMessageBox::error(q, i18n("An error occurred. Press OK to see detailed information.\n"), i18n("Error") );
      m_pStatusInfo->setWindowTitle(i18n("Merge Error"));
      m_pStatusInfo->exec();
      //if ( m_pStatusInfo->firstChild()!=0 )
      //   m_pStatusInfo->ensureItemVisible( m_pStatusInfo->last() );
      m_bError = true;
      
      setOpStatus( miCurrent, eOpStatusError );
   }
   else
   {
      m_bError = false;
   }
   emit q->updateAvailabilities();

   if ( m_currentIndexForOperation==m_mergeItemList.end() )
   {
      m_mergeItemList.clear();
      m_bRealMergeStarted=false;
   }
}

bool DirectoryMergeWindow::Data::deleteFLD( const QString& name, bool bCreateBackup )
{
   FileAccess fi(name, true);
   if ( !fi.exists() )
      return true;

   if ( bCreateBackup )
   {
      bool bSuccess = renameFLD( name, name+".orig" );
      if (!bSuccess)
      {
         m_pStatusInfo->addText( i18n("Error: While deleting %1: Creating backup failed.",name) );
         return false;
      }
   }
   else
   {
      if ( fi.isDir() && !fi.isSymLink() )
         m_pStatusInfo->addText(i18n("delete directory recursively( %1 )",name));
      else
         m_pStatusInfo->addText(i18n("delete( %1 )",name));

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
            bSuccess = deleteFLD( fi2.absoluteFilePath(), false );
            if (!bSuccess) break;
         }
         if (bSuccess)
         {
            bSuccess = FileAccess::removeDir( name );
            if ( !bSuccess )
            {
               m_pStatusInfo->addText( i18n("Error: rmdir( %1 ) operation failed.",name));
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

bool DirectoryMergeWindow::Data::mergeFLD( const QString& nameA,const QString& nameB,const QString& nameC,const QString& nameDest, bool& bSingleFileMerge )
{
   FileAccess fi(nameA);
   if (fi.isDir())
   {
      return makeDir(nameDest);
   }

   // Make sure that the dir exists, into which we will save the file later.
   int pos=nameDest.lastIndexOf('/');
   if ( pos>0 )
   {
      QString parentName = nameDest.left(pos);
      bool bSuccess = makeDir(parentName, true /*quiet*/);
      if (!bSuccess)
         return false;
   }

   m_pStatusInfo->addText(i18n("manual merge( %1, %2, %3 -> %4)",nameA,nameB,nameC,nameDest));
   if ( m_bSimulatedMergeStarted )
   {
      m_pStatusInfo->addText(i18n("     Note: After a manual merge the user should continue by pressing F7.") );
      return true;
   }

   bSingleFileMerge = true;
   setOpStatus(*m_currentIndexForOperation, eOpStatusInProgress );
   q->scrollTo( *m_currentIndexForOperation, EnsureVisible );

   emit q->startDiffMerge( nameA, nameB, nameC, nameDest, "","","",0 );

   return false;
}

bool DirectoryMergeWindow::Data::copyFLD( const QString& srcName, const QString& destName )
{
   if ( srcName == destName )
      return true;

   FileAccess fi( srcName );
   FileAccess faDest(destName, true);
   if ( faDest.exists() && !( fi.isDir() && faDest.isDir() && (fi.isSymLink()==faDest.isSymLink())) )
   {
      bool bSuccess = deleteFLD( destName, m_pOptions->m_bDmCreateBakFiles );
      if ( !bSuccess )
      {
         m_pStatusInfo->addText(i18n("Error: copy( %1 -> %2 ) failed."
            "Deleting existing destination failed.",srcName,destName));
         return false;
      }
   }


   if ( fi.isSymLink() && ((fi.isDir() && !m_bFollowDirLinks)  ||  (!fi.isDir() && !m_bFollowFileLinks)) )
   {
      m_pStatusInfo->addText(i18n("copyLink( %1 -> %2 )",srcName,destName));
#if defined(_WIN32) || defined(Q_OS_OS2)
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
      if ( faDest.exists() )
	 return true;
      else
      {
         bool bSuccess = makeDir( destName );
         return bSuccess;
      }
   }

   int pos=destName.lastIndexOf('/');
   if ( pos>0 )
   {
      QString parentName = destName.left(pos);
      bool bSuccess = makeDir(parentName, true /*quiet*/);
      if (!bSuccess)
         return false;
   }

   m_pStatusInfo->addText(i18n("copy( %1 -> %2 )",srcName,destName));

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
bool DirectoryMergeWindow::Data::renameFLD( const QString& srcName, const QString& destName )
{
   if ( srcName == destName )
      return true;

   if ( FileAccess(destName, true).exists() )
   {
      bool bSuccess = deleteFLD( destName, false /*no backup*/ );
      if (!bSuccess)
      {
         m_pStatusInfo->addText( i18n("Error during rename( %1 -> %2 ): "
                             "Cannot delete existing destination." ,srcName,destName));
         return false;
      }
   }

   m_pStatusInfo->addText(i18n("rename( %1 -> %2 )",srcName,destName));
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

bool DirectoryMergeWindow::Data::makeDir( const QString& name, bool bQuiet )
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
                             "Cannot delete existing file." ,name));
         return false;
      }
   }

   int pos=name.lastIndexOf('/');
   if ( pos>0 )
   {
      QString parentName = name.left(pos);
      bool bSuccess = makeDir(parentName,true);
      if (!bSuccess)
         return false;
   }

   if ( ! bQuiet )
      m_pStatusInfo->addText(i18n("makeDir( %1 )",name));

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
   topLayout->setMargin(0);

   QGridLayout *grid = new QGridLayout();
   topLayout->addLayout(grid);
   grid->setColumnStretch(1,10);

   int line=0;

   m_pA = new QLabel("A",this);        grid->addWidget( m_pA,line, 0 );
   m_pInfoA = new QLabel(this);        grid->addWidget( m_pInfoA,line,1 ); ++line;
   m_pB = new QLabel("B",this);        grid->addWidget( m_pB,line, 0 );
   m_pInfoB = new QLabel(this);        grid->addWidget( m_pInfoB,line,1 ); ++line;
   m_pC = new QLabel("C",this);        grid->addWidget( m_pC,line, 0 );
   m_pInfoC = new QLabel(this);        grid->addWidget( m_pInfoC,line,1 ); ++line;
   m_pDest = new QLabel(i18n("Dest"),this);  grid->addWidget( m_pDest,line, 0 );
   m_pInfoDest = new QLabel(this);     grid->addWidget( m_pInfoDest,line,1 ); ++line;

   m_pInfoList = new QTreeWidget(this);  topLayout->addWidget( m_pInfoList );
   m_pInfoList->setHeaderLabels( QStringList() << i18n("Dir") << i18n("Type") << i18n("Size")
      << i18n("Attr") << i18n("Last Modification") << i18n("Link-Destination") );
   setMinimumSize( 100,100 );

   m_pInfoList->installEventFilter(this);
   m_pInfoList->setRootIsDecorated( false );
}

bool DirectoryMergeInfo::eventFilter(QObject*o, QEvent* e)
{
   if ( e->type()==QEvent::FocusIn && o==m_pInfoList )
      emit gotFocus();
   return false;
}

static void addListViewItem( QTreeWidget* pListView, const QString& dir,
   const QString& basePath, FileAccess* fi )
{
   if ( basePath.isEmpty() )
   {
      return;
   }
   else
   {
      if ( fi!=0 && fi->exists() )
      {
         QString dateString = fi->lastModified().toString("yyyy-MM-dd hh:mm:ss");

         new QTreeWidgetItem(
            pListView,
            QStringList() << dir <<
            QString( fi->isDir() ? i18n("Dir") : i18n("File") ) + (fi->isSymLink() ? "-Link" : "") <<
            QString::number(fi->size()) <<
            QString(fi->isReadable() ? "r" : " ") + (fi->isWritable()?"w" : " ")
#ifdef _WIN32
            /*Future: Use GetFileAttributes()*/ <<
#else
            + (fi->isExecutable()?"x" : " ") <<
#endif
            dateString <<
            QString(fi->isSymLink() ? (" -> " + fi->readLink()) : QString(""))
            );
      }
      else
      {
         new QTreeWidgetItem(
            pListView,
            QStringList() << dir <<
            i18n("not available") <<
            "" <<
            "" <<
            "" <<
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
   if ( dirA.absoluteFilePath()==dirDest.absoluteFilePath() )
   {
      m_pA->setText( i18n("A (Dest): ") );  bHideDest=true;
   }
   else
      m_pA->setText( !dirC.isValid() ? QString("A:    ") : i18n("A (Base): "));

   m_pInfoA->setText( dirA.prettyAbsPath() );

   if ( dirB.absoluteFilePath()==dirDest.absoluteFilePath() )
   {
      m_pB->setText( i18n("B (Dest): ") );  bHideDest=true;
   }
   else
      m_pB->setText( "B:    " );
   m_pInfoB->setText( dirB.prettyAbsPath() );

   if ( dirC.absoluteFilePath()==dirDest.absoluteFilePath() )
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
   addListViewItem( m_pInfoList, "A", dirA.prettyAbsPath(), mfi.m_pFileInfoA );
   addListViewItem( m_pInfoList, "B", dirB.prettyAbsPath(), mfi.m_pFileInfoB );
   addListViewItem( m_pInfoList, "C", dirC.prettyAbsPath(), mfi.m_pFileInfoC );
   if (!bHideDest)
   {
      FileAccess fiDest( dirDest.prettyAbsPath() + "/" + mfi.subPath(), true );
      addListViewItem( m_pInfoList, i18n("Dest"), dirDest.prettyAbsPath(), &fiDest );
   }
   for (int i=0;i<m_pInfoList->columnCount();++i)
      m_pInfoList->resizeColumnToContents ( i );
}

QTextStream& operator<<( QTextStream& ts, MergeFileInfos& mfi )
{
   ts << "{\n";
   ValueMap vm;
   vm.writeEntry( "SubPath", mfi.subPath() );
   vm.writeEntry( "ExistsInA", mfi.existsInA() );
   vm.writeEntry( "ExistsInB",  mfi.existsInB() );
   vm.writeEntry( "ExistsInC",  mfi.existsInC() );
   vm.writeEntry( "EqualAB",  mfi.m_bEqualAB );
   vm.writeEntry( "EqualAC",  mfi.m_bEqualAC );
   vm.writeEntry( "EqualBC",  mfi.m_bEqualBC );
   //DirMergeItem* m_pDMI;
   //MergeFileInfos* m_pParent;
   vm.writeEntry( "MergeOperation", (int) mfi.m_eMergeOperation );
   vm.writeEntry( "DirA",  mfi.dirA() );
   vm.writeEntry( "DirB",  mfi.dirB() );
   vm.writeEntry( "DirC",  mfi.dirC() );
   vm.writeEntry( "LinkA",  mfi.isLinkA() );
   vm.writeEntry( "LinkB",  mfi.isLinkB() );
   vm.writeEntry( "LinkC",  mfi.isLinkC() );
   vm.writeEntry( "OperationComplete", mfi.m_bOperationComplete );
   //bool m_bSimOpComplete );

   vm.writeEntry( "AgeA", (int) mfi.m_ageA );
   vm.writeEntry( "AgeB", (int) mfi.m_ageB );
   vm.writeEntry( "AgeC", (int) mfi.m_ageC );
   vm.writeEntry( "ConflictingAges", mfi.m_bConflictingAges );       // Equal age but files are not!

   //FileAccess m_fileInfoA;
   //FileAccess m_fileInfoB;
   //FileAccess m_fileInfoC;

   //TotalDiffStatus m_totalDiffStatus;
   
   vm.save(ts);
   
   ts << "}\n";

   return ts;
}

void DirectoryMergeWindow::slotSaveMergeState()
{
   //slotStatusMsg(i18n("Saving Directory Merge State ..."));

   //QString s = KFileDialog::getSaveUrl( QDir::currentPath(), 0, this, i18n("Save As...") ).url();
   QString s = KFileDialog::getSaveFileName( QDir::currentPath(), 0, this, i18n("Save Directory Merge State As...") );
   if(!s.isEmpty())
   {
      d->m_dirMergeStateFilename = s;


      QFile file(d->m_dirMergeStateFilename);
      bool bSuccess = file.open( QIODevice::WriteOnly );
      if ( bSuccess )
      {
         QTextStream ts( &file );

         QModelIndex mi( d->index(0,0,QModelIndex()) );
         while ( mi.isValid() ) {
            MergeFileInfos* pMFI = d->getMFI(mi);
            ts << *pMFI;
            mi = d->treeIterator(mi,true,true);
         }
      }
   }

   //slotStatusMsg(i18n("Ready."));

}

void DirectoryMergeWindow::slotLoadMergeState()
{
}

void DirectoryMergeWindow::updateFileVisibilities()
{
   bool bShowIdentical = d->m_pDirShowIdenticalFiles->isChecked();
   bool bShowDifferent = d->m_pDirShowDifferentFiles->isChecked();
   bool bShowOnlyInA   = d->m_pDirShowFilesOnlyInA->isChecked();
   bool bShowOnlyInB   = d->m_pDirShowFilesOnlyInB->isChecked();
   bool bShowOnlyInC   = d->m_pDirShowFilesOnlyInC->isChecked();
   bool bThreeDirs = d->m_dirC.isValid();
   d->m_selection1Index = QModelIndex();
   d->m_selection2Index = QModelIndex();
   d->m_selection3Index = QModelIndex();

   // in first run set all dirs to equal and determine if they are not equal.
   // on second run don't change the equal-status anymore; it is needed to
   // set the visibility (when bShowIdentical is false).
   for( int loop=0; loop<2; ++loop )
   {
      QModelIndex mi = d->rowCount()>0 ? d->index(0,0,QModelIndex()) : QModelIndex();
      while( mi.isValid() )
      {
         MergeFileInfos* pMFI = d->getMFI(mi);
         bool bDir = pMFI->dirA() || pMFI->dirB() || pMFI->dirC();
         if ( loop==0 && bDir )
         {
            bool bChange = false;
            if ( !pMFI->m_bEqualAB && pMFI->dirA() == pMFI->dirB() && pMFI->isLinkA()== pMFI->isLinkB() )
            {
               pMFI->m_bEqualAB = true; 
               bChange=true;                
            }
            if ( !pMFI->m_bEqualBC && pMFI->dirC() == pMFI->dirB() && pMFI->isLinkC()== pMFI->isLinkB() )
            { 
               pMFI->m_bEqualBC = true; 
               bChange=true; 
            }
            if ( !pMFI->m_bEqualAC && pMFI->dirA() == pMFI->dirC() && pMFI->isLinkA()== pMFI->isLinkC() )
            { 
               pMFI->m_bEqualAC = true;
               bChange=true; 
            }

            if ( bChange )
               setPixmaps( *pMFI, bThreeDirs );
         }
         bool bExistsEverywhere = pMFI->existsInA() && pMFI->existsInB() && (pMFI->existsInC() || !bThreeDirs);
         int existCount = int(pMFI->existsInA()) + int(pMFI->existsInB()) + int(pMFI->existsInC());
         bool bVisible =
                  ( bShowIdentical && bExistsEverywhere && pMFI->m_bEqualAB && (pMFI->m_bEqualAC || !bThreeDirs) )
               || ( (bShowDifferent||bDir) && existCount>=2 && (!pMFI->m_bEqualAB || !(pMFI->m_bEqualAC || !bThreeDirs)))
               || ( bShowOnlyInA &&  pMFI->existsInA() && !pMFI->existsInB() && !pMFI->existsInC() )
               || ( bShowOnlyInB && !pMFI->existsInA() &&  pMFI->existsInB() && !pMFI->existsInC() )
               || ( bShowOnlyInC && !pMFI->existsInA() && !pMFI->existsInB() &&  pMFI->existsInC() );

         QString fileName = pMFI->fileName();
         bVisible = bVisible && (
               (bDir && ! wildcardMultiMatch( d->m_pOptions->m_DmDirAntiPattern, fileName, d->m_bCaseSensitive ))
               || (wildcardMultiMatch( d->m_pOptions->m_DmFilePattern, fileName, d->m_bCaseSensitive )
                  && !wildcardMultiMatch( d->m_pOptions->m_DmFileAntiPattern, fileName, d->m_bCaseSensitive )) );

         setRowHidden(mi.row(),mi.parent(),!bVisible);

         bool bEqual = bThreeDirs ? pMFI->m_bEqualAB && pMFI->m_bEqualAC : pMFI->m_bEqualAB;
         if ( !bEqual && bVisible && loop==0 )  // Set all parents to "not equal"
         {
            MergeFileInfos* p2 = pMFI->m_pParent;
            while(p2!=0)
            {
               bool bChange = false;
               if ( !pMFI->m_bEqualAB && p2->m_bEqualAB ){ p2->m_bEqualAB = false; bChange=true; }
               if ( !pMFI->m_bEqualAC && p2->m_bEqualAC ){ p2->m_bEqualAC = false; bChange=true; }
               if ( !pMFI->m_bEqualBC && p2->m_bEqualBC ){ p2->m_bEqualBC = false; bChange=true; }

               if ( bChange )
                  setPixmaps( *p2, bThreeDirs );
               else
                  break;

               p2 = p2->m_pParent;
            }
         }
         mi = d->treeIterator( mi, true, true );
      }
   }
}

void DirectoryMergeWindow::slotShowIdenticalFiles() {
   d->m_pOptions->m_bDmShowIdenticalFiles=d->m_pDirShowIdenticalFiles->isChecked();
   updateFileVisibilities(); 
}
void DirectoryMergeWindow::slotShowDifferentFiles() { updateFileVisibilities(); }
void DirectoryMergeWindow::slotShowFilesOnlyInA()   { updateFileVisibilities(); }
void DirectoryMergeWindow::slotShowFilesOnlyInB()   { updateFileVisibilities(); }
void DirectoryMergeWindow::slotShowFilesOnlyInC()   { updateFileVisibilities(); }

void DirectoryMergeWindow::slotSynchronizeDirectories()   {  }
void DirectoryMergeWindow::slotChooseNewerFiles()   {  }

void DirectoryMergeWindow::initDirectoryMergeActions( QObject* pKDiff3App, KActionCollection* ac )
{
#include "xpm/startmerge.xpm"
#include "xpm/showequalfiles.xpm"
#include "xpm/showfilesonlyina.xpm"
#include "xpm/showfilesonlyinb.xpm"
#include "xpm/showfilesonlyinc.xpm"
   DirectoryMergeWindow* p = this;

   d->m_pDirStartOperation = KDiff3::createAction< KAction >(i18n("Start/Continue Directory Merge"), KShortcut( Qt::Key_F7 ), p, SLOT(slotRunOperationForAllItems()), ac, "dir_start_operation");
   d->m_pDirRunOperationForCurrentItem = KDiff3::createAction< KAction >(i18n("Run Operation for Current Item"), KShortcut( Qt::Key_F6 ), p, SLOT(slotRunOperationForCurrentItem()), ac, "dir_run_operation_for_current_item");
   d->m_pDirCompareCurrent = KDiff3::createAction< KAction >(i18n("Compare Selected File"), p, SLOT(compareCurrentFile()), ac, "dir_compare_current");
   d->m_pDirMergeCurrent = KDiff3::createAction< KAction >(i18n("Merge Current File"), QIcon(QPixmap(startmerge)), i18n("Merge\nFile"), pKDiff3App, SLOT(slotMergeCurrentFile()), ac, "merge_current");
   d->m_pDirFoldAll = KDiff3::createAction< KAction >(i18n("Fold All Subdirs"), p, SLOT(collapseAll()), ac, "dir_fold_all");
   d->m_pDirUnfoldAll = KDiff3::createAction< KAction >(i18n("Unfold All Subdirs"), p, SLOT(expandAll()), ac, "dir_unfold_all");
   d->m_pDirRescan = KDiff3::createAction< KAction >(i18n("Rescan"), KShortcut( Qt::SHIFT+Qt::Key_F5 ), p, SLOT(reload()), ac, "dir_rescan");
   d->m_pDirSaveMergeState = 0; //KDiff3::createAction< KAction >(i18n("Save Directory Merge State ..."), 0, p, SLOT(slotSaveMergeState()), ac, "dir_save_merge_state");
   d->m_pDirLoadMergeState = 0; //KDiff3::createAction< KAction >(i18n("Load Directory Merge State ..."), 0, p, SLOT(slotLoadMergeState()), ac, "dir_load_merge_state");
   d->m_pDirChooseAEverywhere = KDiff3::createAction< KAction >(i18n("Choose A for All Items"), p, SLOT(slotChooseAEverywhere()), ac, "dir_choose_a_everywhere");
   d->m_pDirChooseBEverywhere = KDiff3::createAction< KAction >(i18n("Choose B for All Items"), p, SLOT(slotChooseBEverywhere()), ac, "dir_choose_b_everywhere");
   d->m_pDirChooseCEverywhere = KDiff3::createAction< KAction >(i18n("Choose C for All Items"), p, SLOT(slotChooseCEverywhere()), ac, "dir_choose_c_everywhere");
   d->m_pDirAutoChoiceEverywhere = KDiff3::createAction< KAction >(i18n("Auto-Choose Operation for All Items"), p, SLOT(slotAutoChooseEverywhere()), ac, "dir_autochoose_everywhere");
   d->m_pDirDoNothingEverywhere = KDiff3::createAction< KAction >(i18n("No Operation for All Items"), p, SLOT(slotNoOpEverywhere()), ac, "dir_nothing_everywhere");

//   d->m_pDirSynchronizeDirectories = KDiff3::createAction< KToggleAction >(i18n("Synchronize Directories"), 0, this, SLOT(slotSynchronizeDirectories()), ac, "dir_synchronize_directories");
//   d->m_pDirChooseNewerFiles = KDiff3::createAction< KToggleAction >(i18n("Copy Newer Files Instead of Merging"), 0, this, SLOT(slotChooseNewerFiles()), ac, "dir_choose_newer_files");

   d->m_pDirShowIdenticalFiles = KDiff3::createAction< KToggleAction >(i18n("Show Identical Files"), QIcon(QPixmap(showequalfiles)), i18n("Identical\nFiles"), this, SLOT(slotShowIdenticalFiles()), ac, "dir_show_identical_files");
   d->m_pDirShowDifferentFiles = KDiff3::createAction< KToggleAction >(i18n("Show Different Files"), this, SLOT(slotShowDifferentFiles()), ac, "dir_show_different_files");
   d->m_pDirShowFilesOnlyInA   = KDiff3::createAction< KToggleAction >(i18n("Show Files only in A"), QIcon(QPixmap(showfilesonlyina)), i18n("Files\nonly in A"), this, SLOT(slotShowFilesOnlyInA()), ac, "dir_show_files_only_in_a");
   d->m_pDirShowFilesOnlyInB   = KDiff3::createAction< KToggleAction >(i18n("Show Files only in B"), QIcon(QPixmap(showfilesonlyinb)), i18n("Files\nonly in B"), this, SLOT(slotShowFilesOnlyInB()), ac, "dir_show_files_only_in_b");
   d->m_pDirShowFilesOnlyInC   = KDiff3::createAction< KToggleAction >(i18n("Show Files only in C"), QIcon(QPixmap(showfilesonlyinc)), i18n("Files\nonly in C"), this, SLOT(slotShowFilesOnlyInC()), ac, "dir_show_files_only_in_c");

   d->m_pDirShowIdenticalFiles->setChecked( d->m_pOptions->m_bDmShowIdenticalFiles );

   d->m_pDirCompareExplicit = KDiff3::createAction< KAction >(i18n("Compare Explicitly Selected Files"), p, SLOT(slotCompareExplicitlySelectedFiles()), ac, "dir_compare_explicitly_selected_files");
   d->m_pDirMergeExplicit = KDiff3::createAction< KAction >(i18n("Merge Explicitly Selected Files"), p, SLOT(slotMergeExplicitlySelectedFiles()), ac, "dir_merge_explicitly_selected_files");

   d->m_pDirCurrentDoNothing = KDiff3::createAction< KAction >(i18n("Do Nothing"), p, SLOT(slotCurrentDoNothing()), ac, "dir_current_do_nothing");
   d->m_pDirCurrentChooseA = KDiff3::createAction< KAction >(i18n("A"), p, SLOT(slotCurrentChooseA()), ac, "dir_current_choose_a");
   d->m_pDirCurrentChooseB = KDiff3::createAction< KAction >(i18n("B"), p, SLOT(slotCurrentChooseB()), ac, "dir_current_choose_b");
   d->m_pDirCurrentChooseC = KDiff3::createAction< KAction >(i18n("C"), p, SLOT(slotCurrentChooseC()), ac, "dir_current_choose_c");
   d->m_pDirCurrentMerge   = KDiff3::createAction< KAction >(i18n("Merge"), p, SLOT(slotCurrentMerge()), ac, "dir_current_merge");
   d->m_pDirCurrentDelete  = KDiff3::createAction< KAction >(i18n("Delete (if exists)"), p, SLOT(slotCurrentDelete()), ac, "dir_current_delete");

   d->m_pDirCurrentSyncDoNothing = KDiff3::createAction< KAction >(i18n("Do Nothing"), p, SLOT(slotCurrentDoNothing()), ac, "dir_current_sync_do_nothing");
   d->m_pDirCurrentSyncCopyAToB = KDiff3::createAction< KAction >(i18n("Copy A to B"), p, SLOT(slotCurrentCopyAToB()), ac, "dir_current_sync_copy_a_to_b" );
   d->m_pDirCurrentSyncCopyBToA = KDiff3::createAction< KAction >(i18n("Copy B to A"), p, SLOT(slotCurrentCopyBToA()), ac, "dir_current_sync_copy_b_to_a" );
   d->m_pDirCurrentSyncDeleteA  = KDiff3::createAction< KAction >(i18n("Delete A"), p, SLOT(slotCurrentDeleteA()), ac,"dir_current_sync_delete_a");
   d->m_pDirCurrentSyncDeleteB  = KDiff3::createAction< KAction >(i18n("Delete B"), p, SLOT(slotCurrentDeleteB()), ac,"dir_current_sync_delete_b");
   d->m_pDirCurrentSyncDeleteAAndB  = KDiff3::createAction< KAction >(i18n("Delete A && B"), p, SLOT(slotCurrentDeleteAAndB()), ac,"dir_current_sync_delete_a_and_b");
   d->m_pDirCurrentSyncMergeToA   = KDiff3::createAction< KAction >(i18n("Merge to A"), p, SLOT(slotCurrentMergeToA()), ac,"dir_current_sync_merge_to_a");
   d->m_pDirCurrentSyncMergeToB   = KDiff3::createAction< KAction >(i18n("Merge to B"), p, SLOT(slotCurrentMergeToB()), ac,"dir_current_sync_merge_to_b");
   d->m_pDirCurrentSyncMergeToAAndB   = KDiff3::createAction< KAction >(i18n("Merge to A && B"), p, SLOT(slotCurrentMergeToAAndB()), ac,"dir_current_sync_merge_to_a_and_b");
}


void DirectoryMergeWindow::updateAvailabilities( bool bDirCompare, bool bDiffWindowVisible,
   KToggleAction* chooseA, KToggleAction* chooseB, KToggleAction* chooseC )
{
   d->m_pDirStartOperation->setEnabled( bDirCompare );
   d->m_pDirRunOperationForCurrentItem->setEnabled( bDirCompare );
   d->m_pDirFoldAll->setEnabled( bDirCompare );
   d->m_pDirUnfoldAll->setEnabled( bDirCompare );

   d->m_pDirCompareCurrent->setEnabled( bDirCompare  &&  isVisible()  &&  isFileSelected() );

   d->m_pDirMergeCurrent->setEnabled( (bDirCompare  &&  isVisible()  &&  isFileSelected())
                                || bDiffWindowVisible );

   d->m_pDirRescan->setEnabled( bDirCompare );

   d->m_pDirAutoChoiceEverywhere->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirDoNothingEverywhere->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirChooseAEverywhere->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirChooseBEverywhere->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirChooseCEverywhere->setEnabled( bDirCompare &&  isVisible() );

   bool bThreeDirs = d->m_dirC.isValid();
   
   MergeFileInfos* pMFI = d->getMFI( currentIndex() );

   bool bItemActive = bDirCompare &&  isVisible() && pMFI!=0;//  &&  hasFocus();
   bool bMergeMode = bThreeDirs || !d->m_bSyncMode;
   bool bFTConflict = pMFI==0 ? false : conflictingFileTypes(*pMFI);

   bool bDirWindowHasFocus = isVisible() && hasFocus();

   d->m_pDirShowIdenticalFiles->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirShowDifferentFiles->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirShowFilesOnlyInA->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirShowFilesOnlyInB->setEnabled( bDirCompare &&  isVisible() );
   d->m_pDirShowFilesOnlyInC->setEnabled( bDirCompare &&  isVisible() && bThreeDirs );

   d->m_pDirCompareExplicit->setEnabled( bDirCompare &&  isVisible() && d->m_selection2Index.isValid() );
   d->m_pDirMergeExplicit->setEnabled( bDirCompare &&  isVisible() && d->m_selection2Index.isValid() );

   d->m_pDirCurrentDoNothing->setEnabled( bItemActive && bMergeMode );
   d->m_pDirCurrentChooseA->setEnabled( bItemActive && bMergeMode && pMFI->existsInA() );
   d->m_pDirCurrentChooseB->setEnabled( bItemActive && bMergeMode && pMFI->existsInB() );
   d->m_pDirCurrentChooseC->setEnabled( bItemActive && bMergeMode && pMFI->existsInC() );
   d->m_pDirCurrentMerge->setEnabled( bItemActive && bMergeMode && !bFTConflict );
   d->m_pDirCurrentDelete->setEnabled( bItemActive && bMergeMode );
   if ( bDirWindowHasFocus )
   {
      chooseA->setEnabled( bItemActive && pMFI->existsInA() );
      chooseB->setEnabled( bItemActive && pMFI->existsInB() );
      chooseC->setEnabled( bItemActive && pMFI->existsInC() );
      chooseA->setChecked( false );
      chooseB->setChecked( false );
      chooseC->setChecked( false );
   }

   d->m_pDirCurrentSyncDoNothing->setEnabled( bItemActive && !bMergeMode );
   d->m_pDirCurrentSyncCopyAToB->setEnabled( bItemActive && !bMergeMode && pMFI->existsInA() );
   d->m_pDirCurrentSyncCopyBToA->setEnabled( bItemActive && !bMergeMode && pMFI->existsInB() );
   d->m_pDirCurrentSyncDeleteA->setEnabled( bItemActive && !bMergeMode && pMFI->existsInA() );
   d->m_pDirCurrentSyncDeleteB->setEnabled( bItemActive && !bMergeMode && pMFI->existsInB() );
   d->m_pDirCurrentSyncDeleteAAndB->setEnabled( bItemActive && !bMergeMode && pMFI->existsInB() && pMFI->existsInB() );
   d->m_pDirCurrentSyncMergeToA->setEnabled( bItemActive && !bMergeMode && !bFTConflict );
   d->m_pDirCurrentSyncMergeToB->setEnabled( bItemActive && !bMergeMode && !bFTConflict );
   d->m_pDirCurrentSyncMergeToAAndB->setEnabled( bItemActive && !bMergeMode && !bFTConflict );
}


//#include "directorymergewindow.moc"
