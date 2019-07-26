/***************************************************************************
 *   Copyright (C) 2003-2007 by Joachim Eibl <joachim.eibl at gmx.de>      *
 *   Copyright (C) 2018 Michael Reeves reeves.87@gmail.com                 *
 *                                                                         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "directorymergewindow.h"

#include "DirectoryInfo.h"
#include "MergeFileInfos.h"
#include "PixMapUtils.h"
#include "Utils.h"
#include "guiutils.h"
#include "kdiff3.h"
#include "options.h"
#include "progress.h"

#include <algorithm>
#include <map>
#include <vector>

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPushButton>
#include <QRegExp>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTextStream>
#include <QPainter>

#include <KLocalizedString>
#include <KMessageBox>
#include <KTextEdit>
#include <KToggleAction>

class StatusInfo : public QDialog
{
    KTextEdit* m_pTextEdit;

  public:
    explicit StatusInfo(QWidget* pParent)
        : QDialog(pParent)
    {
        QVBoxLayout* pVLayout = new QVBoxLayout(this);
        m_pTextEdit = new KTextEdit(this);
        pVLayout->addWidget(m_pTextEdit);
        setObjectName("StatusInfo");
        setWindowFlags(Qt::Dialog);
        m_pTextEdit->setWordWrapMode(QTextOption::NoWrap);
        m_pTextEdit->setReadOnly(true);
        QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::accept);
        pVLayout->addWidget(box);
    }

    bool isEmpty()
    {
        return m_pTextEdit->toPlainText().isEmpty();
    }

    void addText(const QString& s)
    {
        m_pTextEdit->append(s);
    }

    void clear()
    {
        m_pTextEdit->clear();
    }

    void setVisible(bool bVisible) override
    {
        if(bVisible)
        {
            m_pTextEdit->moveCursor(QTextCursor::End);
            m_pTextEdit->moveCursor(QTextCursor::StartOfLine);
            m_pTextEdit->ensureCursorVisible();
        }

        QDialog::setVisible(bVisible);
        if(bVisible)
            setWindowState(windowState() | Qt::WindowMaximized);
    }
};

enum Columns
{
    s_NameCol = 0,
    s_ACol = 1,
    s_BCol = 2,
    s_CCol = 3,
    s_OpCol = 4,
    s_OpStatusCol = 5,
    s_UnsolvedCol = 6, // Nr of unsolved conflicts (for 3 input files)
    s_SolvedCol = 7,   // Nr of auto-solvable conflicts (for 3 input files)
    s_NonWhiteCol = 8, // Nr of nonwhite deltas (for 2 input files)
    s_WhiteCol = 9     // Nr of white deltas (for 2 input files)
};

static Qt::CaseSensitivity s_eCaseSensitivity = Qt::CaseSensitive;

//TODO: clean up this mess.
class DirectoryMergeWindow::DirectoryMergeWindowPrivate : public QAbstractItemModel
{
    friend class DirMergeItem;

  public:
    DirectoryMergeWindow* q;
    explicit DirectoryMergeWindowPrivate(DirectoryMergeWindow* pDMW)
    {
        q = pDMW;
        m_pOptions = nullptr;
        m_pDirectoryMergeInfo = nullptr;
        m_bSimulatedMergeStarted = false;
        m_bRealMergeStarted = false;
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
    ~DirectoryMergeWindowPrivate() override
    {
        delete m_pRoot;
    }
    // Implement QAbstractItemModel
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    //Qt::ItemFlags flags ( const QModelIndex & index ) const
    QModelIndex parent(const QModelIndex& index) const override
    {
        MergeFileInfos* pMFI = getMFI(index);
        if(pMFI == nullptr || pMFI == m_pRoot || pMFI->parent() == m_pRoot)
            return QModelIndex();
        else
        {
            MergeFileInfos* pParentsParent = pMFI->parent()->parent();
            return createIndex(pParentsParent->children().indexOf(pMFI->parent()), 0, pMFI->parent());
        }
    }
    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        MergeFileInfos* pParentMFI = getMFI(parent);
        if(pParentMFI != nullptr)
            return pParentMFI->children().count();
        else
            return m_pRoot->children().count();
    }
    int columnCount(const QModelIndex& /*parent*/) const override
    {
        return 10;
    }
    QModelIndex index(int row, int column, const QModelIndex& parent) const override
    {
        MergeFileInfos* pParentMFI = getMFI(parent);
        if(pParentMFI == nullptr && row < m_pRoot->children().count())
            return createIndex(row, column, m_pRoot->children()[row]);
        else if(pParentMFI != nullptr && row < pParentMFI->children().count())
            return createIndex(row, column, pParentMFI->children()[row]);
        else
            return QModelIndex();
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order) override;
    // private data and helper methods
    MergeFileInfos* getMFI(const QModelIndex& mi) const
    {
        if(mi.isValid())
            return (MergeFileInfos*)mi.internalPointer();
        else
            return nullptr;
    }

    bool isThreeWay() const
    {
        if(rootMFI() == nullptr || rootMFI()->getDirectoryInfo() == nullptr) return false;
        return rootMFI()->getDirectoryInfo()->dirC().isValid();
    }
    MergeFileInfos* rootMFI() const { return m_pRoot; }

    static void setPixmaps(MergeFileInfos& mfi, bool);

    Options* m_pOptions;

    void calcDirStatus(bool bThreeDirs, const QModelIndex& mi,
                       int& nofFiles, int& nofDirs, int& nofEqualFiles, int& nofManualMerges);

    void mergeContinue(bool bStart, bool bVerbose);

    void prepareListView(ProgressProxy& pp);
    void calcSuggestedOperation(const QModelIndex& mi, e_MergeOperation eDefaultMergeOp);
    void setAllMergeOperations(e_MergeOperation eDefaultOperation);

    bool canContinue();
    QModelIndex treeIterator(QModelIndex mi, bool bVisitChildren = true, bool bFindInvisible = false);
    void prepareMergeStart(const QModelIndex& miBegin, const QModelIndex& miEnd, bool bVerbose);
    bool executeMergeOperation(MergeFileInfos& mfi, bool& bSingleFileMerge);

    void scanDirectory(const QString& dirName, t_DirectoryList& dirList);
    void scanLocalDirectory(const QString& dirName, t_DirectoryList& dirList);
    bool fastFileComparison(FileAccess& fi1, FileAccess& fi2,
                            bool& bError, QString& status);
    bool compareFilesAndCalcAges(MergeFileInfos& mfi, QStringList& errors);

    void setMergeOperation(const QModelIndex& mi, e_MergeOperation eMergeOp, bool bRecursive = true);
    bool isDir(const QModelIndex& mi);
    QString getFileName(const QModelIndex& mi);

    bool copyFLD(const QString& srcName, const QString& destName);
    bool deleteFLD(const QString& name, bool bCreateBackup);
    bool makeDir(const QString& name, bool bQuiet = false);
    bool renameFLD(const QString& srcName, const QString& destName);
    bool mergeFLD(const QString& nameA, const QString& nameB, const QString& nameC,
                  const QString& nameDest, bool& bSingleFileMerge);


    void buildMergeMap(const QSharedPointer<DirectoryInfo>& dirInfo);

  private:
    class FileKey
    {
      private:
        const FileAccess* m_pFA;
      public:
        explicit FileKey(const FileAccess& fa)
            : m_pFA(&fa) {}

        quint32 getParents(const FileAccess* pFA, const FileAccess* v[], quint32 maxSize) const
        {
            quint32 s = 0;
            for(s = 0; pFA->parent() != nullptr; pFA = pFA->parent(), ++s)
            {
                if(s == maxSize)
                    break;
                v[s] = pFA;
            }
            return s;
        }

        // This is essentially the same as
        // int r = filePath().compare( fa.filePath() )
        // if ( r<0 ) return true;
        // if ( r==0 ) return m_col < fa.m_col;
        // return false;
        bool operator<(const FileKey& fk) const
        {
            const FileAccess* v1[100];
            const FileAccess* v2[100];
            quint32 v1Size = getParents(m_pFA, v1, 100);
            quint32 v2Size = getParents(fk.m_pFA, v2, 100);

            for(quint32 i = 0; i < v1Size && i < v2Size; ++i)
            {
                int r = v1[v1Size - i - 1]->fileName().compare(v2[v2Size - i - 1]->fileName(), s_eCaseSensitivity);
                if(r < 0)
                    return true;
                else if(r > 0)
                    return false;
            }

            return v1Size < v2Size;
        }
    };

    typedef QMap<FileKey, MergeFileInfos> t_fileMergeMap;

    MergeFileInfos* m_pRoot;

    t_fileMergeMap m_fileMergeMap;
  public:

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

    DirectoryMergeInfo* m_pDirectoryMergeInfo;
    StatusInfo* m_pStatusInfo;

    typedef std::list<QModelIndex> MergeItemList; // linked list
    MergeItemList m_mergeItemList;
    MergeItemList::iterator m_currentIndexForOperation;

    QModelIndex m_selection1Index;
    QModelIndex m_selection2Index;
    QModelIndex m_selection3Index;
    void selectItemAndColumn(const QModelIndex& mi, bool bContextMenu);

    QAction* m_pDirStartOperation;
    QAction* m_pDirRunOperationForCurrentItem;
    QAction* m_pDirCompareCurrent;
    QAction* m_pDirMergeCurrent;
    QAction* m_pDirRescan;
    QAction* m_pDirChooseAEverywhere;
    QAction* m_pDirChooseBEverywhere;
    QAction* m_pDirChooseCEverywhere;
    QAction* m_pDirAutoChoiceEverywhere;
    QAction* m_pDirDoNothingEverywhere;
    QAction* m_pDirFoldAll;
    QAction* m_pDirUnfoldAll;

    KToggleAction* m_pDirShowIdenticalFiles;
    KToggleAction* m_pDirShowDifferentFiles;
    KToggleAction* m_pDirShowFilesOnlyInA;
    KToggleAction* m_pDirShowFilesOnlyInB;
    KToggleAction* m_pDirShowFilesOnlyInC;

    KToggleAction* m_pDirSynchronizeDirectories;
    KToggleAction* m_pDirChooseNewerFiles;

    QAction* m_pDirCompareExplicit;
    QAction* m_pDirMergeExplicit;

    QAction* m_pDirCurrentDoNothing;
    QAction* m_pDirCurrentChooseA;
    QAction* m_pDirCurrentChooseB;
    QAction* m_pDirCurrentChooseC;
    QAction* m_pDirCurrentMerge;
    QAction* m_pDirCurrentDelete;

    QAction* m_pDirCurrentSyncDoNothing;
    QAction* m_pDirCurrentSyncCopyAToB;
    QAction* m_pDirCurrentSyncCopyBToA;
    QAction* m_pDirCurrentSyncDeleteA;
    QAction* m_pDirCurrentSyncDeleteB;
    QAction* m_pDirCurrentSyncDeleteAAndB;
    QAction* m_pDirCurrentSyncMergeToA;
    QAction* m_pDirCurrentSyncMergeToB;
    QAction* m_pDirCurrentSyncMergeToAAndB;

    QAction* m_pDirSaveMergeState;
    QAction* m_pDirLoadMergeState;

    bool init(const QSharedPointer<DirectoryInfo> &dirInfo, bool bDirectoryMerge, bool bReload);
    void setOpStatus(const QModelIndex& mi, e_OperationStatus eOpStatus)
    {
        if(MergeFileInfos* pMFI = getMFI(mi))
        {
            pMFI->setOpStatus(eOpStatus);
            emit dataChanged(mi, mi);
        }
    }

    QModelIndex nextSibling(const QModelIndex& mi);
};

QVariant DirectoryMergeWindow::DirectoryMergeWindowPrivate::data(const QModelIndex& index, int role) const
{
    MergeFileInfos* pMFI = getMFI(index);
    if(pMFI)
    {
        if(role == Qt::DisplayRole)
        {
            switch(index.column())
            {
                case s_NameCol:
                    return QFileInfo(pMFI->subPath()).fileName();
                case s_ACol:
                    return i18n("A");
                case s_BCol:
                    return i18n("B");
                case s_CCol:
                    return i18n("C");
                //case s_OpCol:       return i18n("Operation");
                //case s_OpStatusCol: return i18n("Status");
                case s_UnsolvedCol:
                    return pMFI->diffStatus().getUnsolvedConflicts();
                case s_SolvedCol:
                    return pMFI->diffStatus().getSolvedConflicts();
                case s_NonWhiteCol:
                    return pMFI->diffStatus().getNonWhitespaceConflicts();
                case s_WhiteCol:
                    return pMFI->diffStatus().getWhitespaceConflicts();
                    //default :           return QVariant();
            }

            if(s_OpCol == index.column())
            {
                bool bDir = pMFI->isDirA() || pMFI->isDirB() || pMFI->isDirC();
                switch(pMFI->getOperation())
                {
                    case eNoOperation:
                        return "";
                        break;
                    case eCopyAToB:
                        return i18n("Copy A to B");
                        break;
                    case eCopyBToA:
                        return i18n("Copy B to A");
                        break;
                    case eDeleteA:
                        return i18n("Delete A");
                        break;
                    case eDeleteB:
                        return i18n("Delete B");
                        break;
                    case eDeleteAB:
                        return i18n("Delete A & B");
                        break;
                    case eMergeToA:
                        return i18n("Merge to A");
                        break;
                    case eMergeToB:
                        return i18n("Merge to B");
                        break;
                    case eMergeToAB:
                        return i18n("Merge to A & B");
                        break;
                    case eCopyAToDest:
                        return i18n("A");
                        break;
                    case eCopyBToDest:
                        return i18n("B");
                        break;
                    case eCopyCToDest:
                        return i18n("C");
                        break;
                    case eDeleteFromDest:
                        return i18n("Delete (if exists)");
                        break;
                    case eMergeABCToDest:
                        return bDir ? i18n("Merge") : i18n("Merge (manual)");
                        break;
                    case eMergeABToDest:
                        return bDir ? i18n("Merge") : i18n("Merge (manual)");
                        break;
                    case eConflictingFileTypes:
                        return i18n("Error: Conflicting File Types");
                        break;
                    case eChangedAndDeleted:
                        return i18n("Error: Changed and Deleted");
                        break;
                    case eConflictingAges:
                        return i18n("Error: Dates are equal but files are not.");
                        break;
                    default:
                        Q_ASSERT(true);
                        break;
                }
            }
            if(s_OpStatusCol == index.column())
            {
                switch(pMFI->getOpStatus())
                {
                    case eOpStatusNone:
                        return "";
                    case eOpStatusDone:
                        return i18n("Done");
                    case eOpStatusError:
                        return i18n("Error");
                    case eOpStatusSkipped:
                        return i18n("Skipped.");
                    case eOpStatusNotSaved:
                        return i18n("Not saved.");
                    case eOpStatusInProgress:
                        return i18n("In progress...");
                    case eOpStatusToDo:
                        return i18n("To do.");
                }
            }
        }
        else if(role == Qt::DecorationRole)
        {
            if(s_NameCol == index.column())
            {
                return PixMapUtils::getOnePixmap(eAgeEnd, pMFI->isLinkA() || pMFI->isLinkB() || pMFI->isLinkC(),
                                                 pMFI->isDirA() || pMFI->isDirB() || pMFI->isDirC());
            }

            if(s_ACol == index.column())
            {
                return PixMapUtils::getOnePixmap(pMFI->getAgeA(), pMFI->isLinkA(), pMFI->isDirA());
            }
            if(s_BCol == index.column())
            {
                return PixMapUtils::getOnePixmap(pMFI->getAgeB(), pMFI->isLinkB(), pMFI->isDirB());
            }
            if(s_CCol == index.column())
            {
                return PixMapUtils::getOnePixmap(pMFI->getAgeC(), pMFI->isLinkC(), pMFI->isDirC());
            }
        }
        else if(role == Qt::TextAlignmentRole)
        {
            if(s_UnsolvedCol == index.column() || s_SolvedCol == index.column() || s_NonWhiteCol == index.column() || s_WhiteCol == index.column())
                return Qt::AlignRight;
        }
    }
    return QVariant();
}

QVariant DirectoryMergeWindow::DirectoryMergeWindowPrivate::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation == Qt::Horizontal && section >= 0 && section < columnCount(QModelIndex()) && role == Qt::DisplayRole)
    {
        switch(section)
        {
            case s_NameCol:
                return i18n("Name");
            case s_ACol:
                return i18n("A");
            case s_BCol:
                return i18n("B");
            case s_CCol:
                return i18n("C");
            case s_OpCol:
                return i18n("Operation");
            case s_OpStatusCol:
                return i18n("Status");
            case s_UnsolvedCol:
                return i18n("Unsolved");
            case s_SolvedCol:
                return i18n("Solved");
            case s_NonWhiteCol:
                return i18n("Nonwhite");
            case s_WhiteCol:
                return i18n("White");
            default:
                return QVariant();
        }
    }
    return QVariant();
}

// Previously  Q3ListViewItem::paintCell(p,cg,column,width,align);
class DirectoryMergeWindow::DirMergeItemDelegate : public QStyledItemDelegate
{
    DirectoryMergeWindow* m_pDMW;
    DirectoryMergeWindow::DirectoryMergeWindowPrivate* d;

  public:
    explicit DirMergeItemDelegate(DirectoryMergeWindow* pParent)
        : QStyledItemDelegate(pParent), m_pDMW(pParent), d(pParent->d)
    {
    }
    void paint(QPainter* p, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        int column = index.column();
        if(column == s_ACol || column == s_BCol || column == s_CCol)
        {
            QVariant value = index.data(Qt::DecorationRole);
            QPixmap icon;
            if(value.isValid())
            {
                if(value.type() == QVariant::Icon)
                {
                    icon = qvariant_cast<QIcon>(value).pixmap(16, 16);
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
            if(!icon.isNull())
            {
                int yOffset = (sizeHint(option, index).height() - icon.height()) / 2;
                p->drawPixmap(x + 2, y + yOffset, icon);

                int i = index == d->m_selection1Index ? 1 : index == d->m_selection2Index ? 2 : index == d->m_selection3Index ? 3 : 0;
                if(i != 0)
                {
                    Options* pOpts = d->m_pOptions;
                    QColor c(i == 1 ? pOpts->m_colorA : i == 2 ? pOpts->m_colorB : pOpts->m_colorC);
                    p->setPen(c); // highlight() );
                    p->drawRect(x + 2, y + yOffset, icon.width(), icon.height());
                    p->setPen(QPen(c, 0, Qt::DotLine));
                    p->drawRect(x + 1, y + yOffset - 1, icon.width() + 2, icon.height() + 2);
                    p->setPen(Qt::white);
                    QString s(QChar('A' + i - 1));
                    p->drawText(x + 2 + (icon.width() - p->fontMetrics().width(s)) / 2,
                                y + yOffset + (icon.height() + p->fontMetrics().ascent()) / 2 - 1,
                                s);
                }
                else
                {
                    p->setPen(m_pDMW->palette().background().color());
                    p->drawRect(x + 1, y + yOffset - 1, icon.width() + 2, icon.height() + 2);
                }
                return;
            }
        }
        QStyleOptionViewItem option2 = option;
        if(column >= s_UnsolvedCol)
        {
            option2.displayAlignment = Qt::AlignRight;
        }
        QStyledItemDelegate::paint(p, option2, index);
    }
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize sz = QStyledItemDelegate::sizeHint(option, index);
        return sz.expandedTo(QSize(0, 18));
    }
};

DirectoryMergeWindow::DirectoryMergeWindow(QWidget* pParent, Options* pOptions)
    : QTreeView(pParent)
{
    d = new DirectoryMergeWindowPrivate(this);
    setModel(d);
    setItemDelegate(new DirMergeItemDelegate(this));
    connect(this, &DirectoryMergeWindow::doubleClicked, this, &DirectoryMergeWindow::onDoubleClick);
    connect(this, &DirectoryMergeWindow::expanded, this, &DirectoryMergeWindow::onExpanded);

    d->m_pOptions = pOptions;

    setSortingEnabled(true);
}

DirectoryMergeWindow::~DirectoryMergeWindow()
{
    delete d;
}

void DirectoryMergeWindow::setDirectoryMergeInfo(DirectoryMergeInfo* p)
{
    d->m_pDirectoryMergeInfo = p;
}
bool DirectoryMergeWindow::isDirectoryMergeInProgress()
{
    return d->m_bRealMergeStarted;
}
bool DirectoryMergeWindow::isSyncMode()
{
    return d->m_bSyncMode;
}
bool DirectoryMergeWindow::isScanning()
{
    return d->m_bScanning;
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::fastFileComparison(
    FileAccess& fi1, FileAccess& fi2,
    bool& bError, QString& status)
{
    ProgressProxy pp;
    bool bEqual = false;

    status = "";
    bError = true;

    if(fi1.isNormal() != fi2.isNormal())
    {
        status = i18n("Unable to compare non-normal file with normal file.");
        return false;
    }

    if(!fi1.isNormal())
    {
        bError = false;
        return false;
    }

    if(!m_bFollowFileLinks)
    {
        if(fi1.isSymLink() != fi2.isSymLink())
        {
            status = i18n("Mix of links and normal files.");
            return bEqual;
        }
        else if(fi1.isSymLink() && fi2.isSymLink())
        {
            bError = false;
            bEqual = fi1.readLink() == fi2.readLink();
            status = i18n("Link: ");
            return bEqual;
        }
    }

    if(fi1.size() != fi2.size())
    {
        bError = false;
        bEqual = false;
        status = i18n("Size. ");
        return bEqual;
    }
    else if(m_pOptions->m_bDmTrustSize)
    {
        bEqual = true;
        bError = false;
        return bEqual;
    }

    if(m_pOptions->m_bDmTrustDate)
    {
        bEqual = (fi1.lastModified() == fi2.lastModified() && fi1.size() == fi2.size());
        bError = false;
        status = i18n("Date & Size: ");
        return bEqual;
    }

    if(m_pOptions->m_bDmTrustDateFallbackToBinary)
    {
        bEqual = (fi1.lastModified() == fi2.lastModified() && fi1.size() == fi2.size());
        if(bEqual)
        {
            bError = false;
            status = i18n("Date & Size: ");
            return bEqual;
        }
    }

    std::vector<char> buf1(100000);
    std::vector<char> buf2(buf1.size());

    if(!fi1.open(QIODevice::ReadOnly))
    {
        status = fi1.errorString();
        return bEqual;
    }

    if(!fi2.open(QIODevice::ReadOnly))
    {
        fi1.close();
        status = fi2.errorString();
        return bEqual;
    }

    pp.setInformation(i18n("Comparing file..."), 0, false);
    typedef qint64 t_FileSize;
    t_FileSize fullSize = fi1.size();
    t_FileSize sizeLeft = fullSize;

    pp.setMaxNofSteps(fullSize / buf1.size());

    while(sizeLeft > 0 && !pp.wasCancelled())
    {
        qint64 len = std::min(sizeLeft, (t_FileSize)buf1.size());
        if(len != fi1.read(&buf1[0], len))
        {
            status = fi1.errorString();
            fi1.close();
            fi2.close();
            return bEqual;
        }

        if(len != fi2.read(&buf2[0], len))
        {
            status = fi2.errorString();
            fi1.close();
            fi2.close();
            return bEqual;
        }

        if(memcmp(&buf1[0], &buf2[0], len) != 0)
        {
            bError = false;
            return bEqual;
        }
        sizeLeft -= len;
        //pp.setCurrent(double(fullSize-sizeLeft)/fullSize, false );
        pp.step();
    }

    // If the program really arrives here, then the files are really equal.
    bError = false;
    bEqual = true;
    return bEqual;
}

int DirectoryMergeWindow::totalColumnWidth()
{
    int w = 0;
    for(int i = 0; i < s_OpStatusCol; ++i)
    {
        w += columnWidth(i);
    }
    return w;
}

void DirectoryMergeWindow::reload()
{
    if(isDirectoryMergeInProgress())
    {
        int result = KMessageBox::warningYesNo(this,
                                               i18n("You are currently doing a directory merge. Are you sure, you want to abort the merge and rescan the directory?"),
                                               i18n("Warning"),
                                               KGuiItem(i18n("Rescan")),
                                               KGuiItem(i18n("Continue Merging")));
        if(result != KMessageBox::Yes)
            return;
    }

    init(d->rootMFI()->getDirectoryInfo(), true);
    //fix file visibilities after reload or menu will be out of sync with display if changed from defaults.
    updateFileVisibilities();
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::calcDirStatus(bool bThreeDirs, const QModelIndex& mi,
                                                                      int& nofFiles, int& nofDirs, int& nofEqualFiles, int& nofManualMerges)
{
    MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI->isDirA() || pMFI->isDirB() || pMFI->isDirC())
    {
        ++nofDirs;
    }
    else
    {
        ++nofFiles;
        if(pMFI->m_bEqualAB && (!bThreeDirs || pMFI->m_bEqualAC))
        {
            ++nofEqualFiles;
        }
        else
        {
            if(pMFI->getOperation() == eMergeABCToDest || pMFI->getOperation() == eMergeABToDest)
                ++nofManualMerges;
        }
    }
    for(int childIdx = 0; childIdx < rowCount(mi); ++childIdx)
        calcDirStatus(bThreeDirs, index(childIdx, 0, mi), nofFiles, nofDirs, nofEqualFiles, nofManualMerges);
}

struct t_ItemInfo {
    bool bExpanded;
    bool bOperationComplete;
    QString status;
    e_MergeOperation eMergeOperation;
};

bool DirectoryMergeWindow::init(
    const QSharedPointer<DirectoryInfo> &dirInfo,
    bool bDirectoryMerge,
    bool bReload)
{
    return d->init(dirInfo, bDirectoryMerge, bReload);
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::buildMergeMap(const QSharedPointer<DirectoryInfo>& dirInfo)
{
    t_DirectoryList::iterator dirIterator;

    if(dirInfo->dirA().isValid())
    {
        for(dirIterator = dirInfo->getDirListA().begin(); dirIterator != dirInfo->getDirListA().end(); ++dirIterator)
        {
            MergeFileInfos& mfi = m_fileMergeMap[FileKey(*dirIterator)];

            mfi.setFileInfoA(&(*dirIterator));
            mfi.setDirectoryInfo(dirInfo);
        }
    }

    if(dirInfo->dirB().isValid())
    {
        for(dirIterator = dirInfo->getDirListB().begin(); dirIterator != dirInfo->getDirListB().end(); ++dirIterator)
        {
            MergeFileInfos& mfi = m_fileMergeMap[FileKey(*dirIterator)];

            mfi.setFileInfoB(&(*dirIterator));
            mfi.setDirectoryInfo(dirInfo);
        }
    }

    if(dirInfo->dirC().isValid())
    {
        for(dirIterator = dirInfo->getDirListC().begin(); dirIterator != dirInfo->getDirListC().end(); ++dirIterator)
        {
            MergeFileInfos& mfi = m_fileMergeMap[FileKey(*dirIterator)];

            mfi.setFileInfoC(&(*dirIterator));
            mfi.setDirectoryInfo(dirInfo);
        }
    }
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::init(
    const QSharedPointer<DirectoryInfo> &dirInfo,
    bool bDirectoryMerge,
    bool bReload)
{
    //set root data now that we have the directory info.
    rootMFI()->setDirectoryInfo(dirInfo);

    if(m_pOptions->m_bDmFullAnalysis)
    {
        // A full analysis uses the same resources that a normal text-diff/merge uses.
        // So make sure that the user saves his data first.
        bool bCanContinue = false;
        emit q->checkIfCanContinue(&bCanContinue);
        if(!bCanContinue)
            return false;
        emit q->startDiffMerge("", "", "", "", "", "", "", nullptr); // hide main window
    }

    q->show();
    q->setUpdatesEnabled(true);

    std::map<QString, t_ItemInfo> expandedDirsMap;

    if(bReload)
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
        //   ii.eMergeOperation = pDMI->m_pMFI->getOperation();
        //   ++it;
        //}
    }

    ProgressProxy pp;
    m_bFollowDirLinks = m_pOptions->m_bDmFollowDirLinks;
    m_bFollowFileLinks = m_pOptions->m_bDmFollowFileLinks;
    m_bSimulatedMergeStarted = false;
    m_bRealMergeStarted = false;
    m_bError = false;
    m_bDirectoryMerge = bDirectoryMerge;
    m_selection1Index = QModelIndex();
    m_selection2Index = QModelIndex();
    m_selection3Index = QModelIndex();
    m_bCaseSensitive = m_pOptions->m_bDmCaseSensitiveFilenameComparison;
    m_bUnfoldSubdirs = m_pOptions->m_bDmUnfoldSubdirs;
    m_bSkipDirStatus = m_pOptions->m_bDmSkipDirStatus;

    beginResetModel();
    m_pRoot->clear();
    m_mergeItemList.clear();
    endResetModel();

    m_currentIndexForOperation = m_mergeItemList.end();

    if(!bReload)
    {
        m_pDirShowIdenticalFiles->setChecked(true);
        m_pDirShowDifferentFiles->setChecked(true);
        m_pDirShowFilesOnlyInA->setChecked(true);
        m_pDirShowFilesOnlyInB->setChecked(true);
        m_pDirShowFilesOnlyInC->setChecked(true);
    }
    Q_ASSERT(dirInfo != nullptr);
    FileAccess dirA = dirInfo->dirA();
    FileAccess dirB = dirInfo->dirB();
    FileAccess dirC = dirInfo->dirC();
    const FileAccess dirDest = dirInfo->destDir();
    // Check if all input directories exist and are valid. The dest dir is not tested now.
    // The test will happen only when we are going to write to it.
    if(!dirA.isDir() || !dirB.isDir() ||
       (dirC.isValid() && !dirC.isDir()))
    {
        QString text(i18n("Opening of directories failed:"));
        text += "\n\n";
        if(!dirA.isDir())
        {
            text += i18n("Dir A \"%1\" does not exist or is not a directory.\n", dirA.prettyAbsPath());
        }

        if(!dirB.isDir())
        {
            text += i18n("Dir B \"%1\" does not exist or is not a directory.\n", dirB.prettyAbsPath());
        }

        if(dirC.isValid() && !dirC.isDir())
        {
            text += i18n("Dir C \"%1\" does not exist or is not a directory.\n", dirC.prettyAbsPath());
        }

        KMessageBox::sorry(q, text, i18n("Directory Open Error"));
        return false;
    }

    if(dirC.isValid() &&
       (dirDest.prettyAbsPath() == dirA.prettyAbsPath() || dirDest.prettyAbsPath() == dirB.prettyAbsPath()))
    {
        KMessageBox::error(q,
                           i18n("The destination directory must not be the same as A or B when "
                                "three directories are merged.\nCheck again before continuing."),
                           i18n("Parameter Warning"));
        return false;
    }

    m_bScanning = true;
    emit q->statusBarMessage(i18n("Scanning directories..."));

    m_bSyncMode = m_pOptions->m_bDmSyncMode && !dirC.isValid() && !dirDest.isValid();

    m_fileMergeMap.clear();
    s_eCaseSensitivity = m_bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    // calc how many directories will be read:
    double nofScans = (dirA.isValid() ? 1 : 0) + (dirB.isValid() ? 1 : 0) + (dirC.isValid() ? 1 : 0);
    int currentScan = 0;

    //TODO   setColumnWidthMode(s_UnsolvedCol, Q3ListView::Manual);
    //   setColumnWidthMode(s_SolvedCol,   Q3ListView::Manual);
    //   setColumnWidthMode(s_WhiteCol,    Q3ListView::Manual);
    //   setColumnWidthMode(s_NonWhiteCol, Q3ListView::Manual);
    q->setColumnHidden(s_CCol, !dirC.isValid());
    q->setColumnHidden(s_WhiteCol, !m_pOptions->m_bDmFullAnalysis);
    q->setColumnHidden(s_NonWhiteCol, !m_pOptions->m_bDmFullAnalysis);
    q->setColumnHidden(s_UnsolvedCol, !m_pOptions->m_bDmFullAnalysis);
    q->setColumnHidden(s_SolvedCol, !(m_pOptions->m_bDmFullAnalysis && dirC.isValid()));

    bool bListDirSuccessA = true;
    bool bListDirSuccessB = true;
    bool bListDirSuccessC = true;

    if(dirA.isValid())
    {
        pp.setInformation(i18n("Reading Directory A"));
        pp.setSubRangeTransformation(currentScan / nofScans, (currentScan + 1) / nofScans);
        ++currentScan;

        bListDirSuccessA = dirInfo->listDirA(*m_pOptions);
    }

    if(dirB.isValid())
    {
        pp.setInformation(i18n("Reading Directory B"));
        pp.setSubRangeTransformation(currentScan / nofScans, (currentScan + 1) / nofScans);
        ++currentScan;

        bListDirSuccessB = dirInfo->listDirB(*m_pOptions);
    }

    e_MergeOperation eDefaultMergeOp;
    if(dirC.isValid())
    {
        pp.setInformation(i18n("Reading Directory C"));
        pp.setSubRangeTransformation(currentScan / nofScans, (currentScan + 1) / nofScans);
        ++currentScan;

        bListDirSuccessC = dirInfo->listDirC(*m_pOptions);

        eDefaultMergeOp = eMergeABCToDest;
    }
    else
        eDefaultMergeOp = m_bSyncMode ? eMergeToAB : eMergeABToDest;

    buildMergeMap(dirInfo);

    bool bContinue = true;
    if(!bListDirSuccessA || !bListDirSuccessB || !bListDirSuccessC)
    {
        QString s = i18n("Some subdirectories were not readable in");
        if(!bListDirSuccessA) s += "\nA: " + dirA.prettyAbsPath();
        if(!bListDirSuccessB) s += "\nB: " + dirB.prettyAbsPath();
        if(!bListDirSuccessC) s += "\nC: " + dirC.prettyAbsPath();
        s += '\n';
        s += i18n("Check the permissions of the subdirectories.");
        bContinue = KMessageBox::Continue == KMessageBox::warningContinueCancel(q, s);
    }

    if(bContinue)
    {
        prepareListView(pp);

        q->updateFileVisibilities();

        for(int childIdx = 0; childIdx < rowCount(); ++childIdx)
        {
            QModelIndex mi = index(childIdx, 0, QModelIndex());
            calcSuggestedOperation(mi, eDefaultMergeOp);
        }
    }

    q->sortByColumn(0, Qt::AscendingOrder);

    for(int column = 0; column < columnCount(QModelIndex()); ++column)
        q->resizeColumnToContents(column);

    // Try to improve the view a little bit.
    QWidget* pParent = q->parentWidget();
    QSplitter* pSplitter = static_cast<QSplitter*>(pParent);
    if(pSplitter != nullptr)
    {
        QList<int> sizes = pSplitter->sizes();
        int total = sizes[0] + sizes[1];
        if(total < 10)
            total = 100;
        sizes[0] = total * 6 / 10;
        sizes[1] = total - sizes[0];
        pSplitter->setSizes(sizes);
    }

    m_bScanning = false;
    emit q->statusBarMessage(i18n("Ready."));

    if(bContinue && !m_bSkipDirStatus)
    {
        // Generate a status report
        int nofFiles = 0;
        int nofDirs = 0;
        int nofEqualFiles = 0;
        int nofManualMerges = 0;
        //TODO
        for(int childIdx = 0; childIdx < rowCount(); ++childIdx)
            calcDirStatus(dirC.isValid(), index(childIdx, 0, QModelIndex()),
                          nofFiles, nofDirs, nofEqualFiles, nofManualMerges);

        QString s;
        s = i18n("Directory Comparison Status\n\n"
                 "Number of subdirectories: %1\n"
                 "Number of equal files: %2\n"
                 "Number of different files: %3",
                 nofDirs, nofEqualFiles, nofFiles - nofEqualFiles);

        if(dirC.isValid())
            s += '\n' + i18n("Number of manual merges: %1", nofManualMerges);
        KMessageBox::information(q, s);
        //
        //TODO
        //if ( topLevelItemCount()>0 )
        //{
        //   topLevelItem(0)->setSelected(true);
        //   setCurrentItem( topLevelItem(0) );
        //}
    }

    if(bReload)
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
    else if(m_bUnfoldSubdirs)
    {
        m_pDirUnfoldAll->trigger();
    }

    return true;
}

inline QString DirectoryMergeWindow::getDirNameA() const
{
    return d->rootMFI()->getDirectoryInfo()->dirA().prettyAbsPath();
}
inline QString DirectoryMergeWindow::getDirNameB() const
{
    return d->rootMFI()->getDirectoryInfo()->dirB().prettyAbsPath();
}
inline QString DirectoryMergeWindow::getDirNameC() const
{
    return d->rootMFI()->getDirectoryInfo()->dirC().prettyAbsPath();
}
inline QString DirectoryMergeWindow::getDirNameDest() const
{
    return d->rootMFI()->getDirectoryInfo()->destDir().prettyAbsPath();
}

void DirectoryMergeWindow::onExpanded()
{
    resizeColumnToContents(s_NameCol);
}

void DirectoryMergeWindow::slotChooseAEverywhere()
{
    d->setAllMergeOperations(eCopyAToDest);
}

void DirectoryMergeWindow::slotChooseBEverywhere()
{
    d->setAllMergeOperations(eCopyBToDest);
}

void DirectoryMergeWindow::slotChooseCEverywhere()
{
    d->setAllMergeOperations(eCopyCToDest);
}

void DirectoryMergeWindow::slotAutoChooseEverywhere()
{
    e_MergeOperation eDefaultMergeOp = d->isThreeWay() ? eMergeABCToDest : d->m_bSyncMode ? eMergeToAB : eMergeABToDest;
    d->setAllMergeOperations(eDefaultMergeOp);
}

void DirectoryMergeWindow::slotNoOpEverywhere()
{
    d->setAllMergeOperations(eNoOperation);
}

void DirectoryMergeWindow::slotFoldAllSubdirs()
{
    collapseAll();
}

void DirectoryMergeWindow::slotUnfoldAllSubdirs()
{
    expandAll();
}

// Merge current item (merge mode)
void DirectoryMergeWindow::slotCurrentDoNothing()
{
    d->setMergeOperation(currentIndex(), eNoOperation);
}
void DirectoryMergeWindow::slotCurrentChooseA()
{
    d->setMergeOperation(currentIndex(), d->m_bSyncMode ? eCopyAToB : eCopyAToDest);
}
void DirectoryMergeWindow::slotCurrentChooseB()
{
    d->setMergeOperation(currentIndex(), d->m_bSyncMode ? eCopyBToA : eCopyBToDest);
}
void DirectoryMergeWindow::slotCurrentChooseC()
{
    d->setMergeOperation(currentIndex(), eCopyCToDest);
}
void DirectoryMergeWindow::slotCurrentMerge()
{
    bool bThreeDirs = d->isThreeWay();
    d->setMergeOperation(currentIndex(), bThreeDirs ? eMergeABCToDest : eMergeABToDest);
}
void DirectoryMergeWindow::slotCurrentDelete()
{
    d->setMergeOperation(currentIndex(), eDeleteFromDest);
}
// Sync current item
void DirectoryMergeWindow::slotCurrentCopyAToB()
{
    d->setMergeOperation(currentIndex(), eCopyAToB);
}
void DirectoryMergeWindow::slotCurrentCopyBToA()
{
    d->setMergeOperation(currentIndex(), eCopyBToA);
}
void DirectoryMergeWindow::slotCurrentDeleteA()
{
    d->setMergeOperation(currentIndex(), eDeleteA);
}
void DirectoryMergeWindow::slotCurrentDeleteB()
{
    d->setMergeOperation(currentIndex(), eDeleteB);
}
void DirectoryMergeWindow::slotCurrentDeleteAAndB()
{
    d->setMergeOperation(currentIndex(), eDeleteAB);
}
void DirectoryMergeWindow::slotCurrentMergeToA()
{
    d->setMergeOperation(currentIndex(), eMergeToA);
}
void DirectoryMergeWindow::slotCurrentMergeToB()
{
    d->setMergeOperation(currentIndex(), eMergeToB);
}
void DirectoryMergeWindow::slotCurrentMergeToAAndB()
{
    d->setMergeOperation(currentIndex(), eMergeToAB);
}

void DirectoryMergeWindow::keyPressEvent(QKeyEvent* e)
{
    if((e->QInputEvent::modifiers() & Qt::ControlModifier) != 0)
    {
        MergeFileInfos* pMFI = d->getMFI(currentIndex());
        if(pMFI == nullptr)
            return;

        bool bThreeDirs = pMFI->getDirectoryInfo()->dirC().isValid();
        bool bMergeMode = bThreeDirs || !d->m_bSyncMode;
        bool bFTConflict = pMFI == nullptr ? false : pMFI->conflictingFileTypes();

        if(bMergeMode)
        {
            switch(e->key())
            {
                case Qt::Key_1:
                    if(pMFI->existsInA())
                    {
                        slotCurrentChooseA();
                    }
                    return;
                case Qt::Key_2:
                    if(pMFI->existsInB())
                    {
                        slotCurrentChooseB();
                    }
                    return;
                case Qt::Key_3:
                    if(pMFI->existsInC())
                    {
                        slotCurrentChooseC();
                    }
                    return;
                case Qt::Key_Space:
                    slotCurrentDoNothing();
                    return;
                case Qt::Key_4:
                    if(!bFTConflict)
                    {
                        slotCurrentMerge();
                    }
                    return;
                case Qt::Key_Delete:
                    slotCurrentDelete();
                    return;
                default:
                    break;
            }
        }
        else
        {
            switch(e->key())
            {
                case Qt::Key_1:
                    if(pMFI->existsInA())
                    {
                        slotCurrentCopyAToB();
                    }
                    return;
                case Qt::Key_2:
                    if(pMFI->existsInB())
                    {
                        slotCurrentCopyBToA();
                    }
                    return;
                case Qt::Key_Space:
                    slotCurrentDoNothing();
                    return;
                case Qt::Key_4:
                    if(!bFTConflict)
                    {
                        slotCurrentMergeToAAndB();
                    }
                    return;
                case Qt::Key_Delete:
                    if(pMFI->existsInA() && pMFI->existsInB())
                        slotCurrentDeleteAAndB();
                    else if(pMFI->existsInA())
                        slotCurrentDeleteA();
                    else if(pMFI->existsInB())
                        slotCurrentDeleteB();
                    return;
                default:
                    break;
            }
        }
    }
    else if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
        onDoubleClick(currentIndex());
        return;
    }

    QTreeView::keyPressEvent(e);
}

void DirectoryMergeWindow::focusInEvent(QFocusEvent*)
{
    emit updateAvailabilities();
}
void DirectoryMergeWindow::focusOutEvent(QFocusEvent*)
{
    emit updateAvailabilities();
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::setAllMergeOperations(e_MergeOperation eDefaultOperation)
{
    if(KMessageBox::Yes == KMessageBox::warningYesNo(q,
                                                     i18n("This affects all merge operations."),
                                                     i18n("Changing All Merge Operations"),
                                                     KStandardGuiItem::cont(),
                                                     KStandardGuiItem::cancel()))
    {
        for(int i = 0; i < rowCount(); ++i)
        {
            calcSuggestedOperation(index(i, 0, QModelIndex()), eDefaultOperation);
        }
    }
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::compareFilesAndCalcAges(MergeFileInfos& mfi, QStringList& errors)
{
    std::map<QDateTime, int> dateMap;

    if(mfi.existsInA())
    {
        dateMap[mfi.getFileInfoA()->lastModified()] = 0;
    }
    if(mfi.existsInB())
    {
        dateMap[mfi.getFileInfoB()->lastModified()] = 1;
    }
    if(mfi.existsInC())
    {
        dateMap[mfi.getFileInfoC()->lastModified()] = 2;
    }

    if(m_pOptions->m_bDmFullAnalysis)
    {
        if((mfi.existsInA() && mfi.isDirA()) || (mfi.existsInB() && mfi.isDirB()) || (mfi.existsInC() && mfi.isDirC()))
        {
            // If any input is a directory, don't start any comparison.
            mfi.m_bEqualAB = mfi.existsInA() && mfi.existsInB();
            mfi.m_bEqualAC = mfi.existsInA() && mfi.existsInC();
            mfi.m_bEqualBC = mfi.existsInB() && mfi.existsInC();
        }
        else
        {
            emit q->startDiffMerge(
                mfi.existsInA() ? mfi.getFileInfoA()->absoluteFilePath() : QString(""),
                mfi.existsInB() ? mfi.getFileInfoB()->absoluteFilePath() : QString(""),
                mfi.existsInC() ? mfi.getFileInfoC()->absoluteFilePath() : QString(""),
                "",
                "", "", "", &mfi.diffStatus());
            int nofNonwhiteConflicts = mfi.diffStatus().getNonWhitespaceConflicts();

            if(m_pOptions->m_bDmWhiteSpaceEqual && nofNonwhiteConflicts == 0)
            {
                mfi.m_bEqualAB = mfi.existsInA() && mfi.existsInB();
                mfi.m_bEqualAC = mfi.existsInA() && mfi.existsInC();
                mfi.m_bEqualBC = mfi.existsInB() && mfi.existsInC();
            }
            else
            {
                mfi.m_bEqualAB = mfi.diffStatus().isBinaryEqualAB();
                mfi.m_bEqualBC = mfi.diffStatus().isBinaryEqualBC();
                mfi.m_bEqualAC = mfi.diffStatus().isBinaryEqualAC();
            }
        }
    }
    else
    {
        bool bError = false;
        QString eqStatus;
        if(mfi.existsInA() && mfi.existsInB())
        {
            if(mfi.isDirA())
                mfi.m_bEqualAB = true;
            else
                mfi.m_bEqualAB = fastFileComparison(*mfi.getFileInfoA(), *mfi.getFileInfoB(), bError, eqStatus);
        }
        if(mfi.existsInA() && mfi.existsInC())
        {
            if(mfi.isDirA())
                mfi.m_bEqualAC = true;
            else
                mfi.m_bEqualAC = fastFileComparison(*mfi.getFileInfoA(), *mfi.getFileInfoC(), bError, eqStatus);
        }
        if(mfi.existsInB() && mfi.existsInC())
        {
            if(mfi.m_bEqualAB && mfi.m_bEqualAC)
                mfi.m_bEqualBC = true;
            else
            {
                if(mfi.isDirB())
                    mfi.m_bEqualBC = true;
                else
                    mfi.m_bEqualBC = fastFileComparison(*mfi.getFileInfoB(), *mfi.getFileInfoC(), bError, eqStatus);
            }
        }
        if(bError)
        {
            //Limit size of error list in memmory.
            if(errors.size() < 30)
                errors.append(eqStatus);
            return false;
        }
    }

    if(mfi.isLinkA() != mfi.isLinkB()) mfi.m_bEqualAB = false;
    if(mfi.isLinkA() != mfi.isLinkC()) mfi.m_bEqualAC = false;
    if(mfi.isLinkB() != mfi.isLinkC()) mfi.m_bEqualBC = false;

    if(mfi.isDirA() != mfi.isDirB()) mfi.m_bEqualAB = false;
    if(mfi.isDirA() != mfi.isDirC()) mfi.m_bEqualAC = false;
    if(mfi.isDirB() != mfi.isDirC()) mfi.m_bEqualBC = false;

    Q_ASSERT(eNew == 0 && eMiddle == 1 && eOld == 2);

    // The map automatically sorts the keys.
    int age = eNew;
    std::map<QDateTime, int>::reverse_iterator i;
    for(i = dateMap.rbegin(); i != dateMap.rend(); ++i)
    {
        int n = i->second;
        if(n == 0 && mfi.getAgeA() == eNotThere)
        {
            mfi.setAgeA((e_Age)age);
            ++age;
            if(mfi.m_bEqualAB)
            {
                mfi.setAgeB(mfi.getAgeA());
                ++age;
            }
            if(mfi.m_bEqualAC)
            {
                mfi.setAgeC(mfi.getAgeA());
                ++age;
            }
        }
        else if(n == 1 && mfi.getAgeB() == eNotThere)
        {
            mfi.setAgeB((e_Age)age);
            ++age;
            if(mfi.m_bEqualAB)
            {
                mfi.setAgeA(mfi.getAgeB());
                ++age;
            }
            if(mfi.m_bEqualBC)
            {
                mfi.setAgeC(mfi.getAgeB());
                ++age;
            }
        }
        else if(n == 2 && mfi.getAgeC() == eNotThere)
        {
            mfi.setAgeC((e_Age)age);
            ++age;
            if(mfi.m_bEqualAC)
            {
                mfi.setAgeA(mfi.getAgeC());
                ++age;
            }
            if(mfi.m_bEqualBC)
            {
                mfi.setAgeB(mfi.getAgeC());
                ++age;
            }
        }
    }

    // The checks below are necessary when the dates of the file are equal but the
    // files are not. One wouldn't expect this to happen, yet it happens sometimes.
    if(mfi.existsInC() && mfi.getAgeC() == eNotThere)
    {
        mfi.setAgeC((e_Age)age);
        ++age;
        mfi.m_bConflictingAges = true;
    }
    if(mfi.existsInB() && mfi.getAgeB() == eNotThere)
    {
        mfi.setAgeB((e_Age)age);
        ++age;
        mfi.m_bConflictingAges = true;
    }
    if(mfi.existsInA() && mfi.getAgeA() == eNotThere)
    {
        mfi.setAgeA((e_Age)age);
        ++age;
        mfi.m_bConflictingAges = true;
    }

    if(mfi.getAgeA() != eOld && mfi.getAgeB() != eOld && mfi.getAgeC() != eOld)
    {
        if(mfi.getAgeA() == eMiddle) mfi.setAgeA(eOld);
        if(mfi.getAgeB() == eMiddle) mfi.setAgeB(eOld);
        if(mfi.getAgeC() == eMiddle) mfi.setAgeC(eOld);
    }

    return true;
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::setPixmaps(MergeFileInfos& mfi, bool)
{
    if(mfi.isDirA() || mfi.isDirB() || mfi.isDirC())
    {
        mfi.setAgeA(eNotThere);
        mfi.setAgeB(eNotThere);
        mfi.setAgeC(eNotThere);
        int age = eNew;
        if(mfi.existsInC())
        {
            mfi.setAgeC((e_Age)age);
            if(mfi.m_bEqualAC) mfi.setAgeA((e_Age)age);
            if(mfi.m_bEqualBC) mfi.setAgeB((e_Age)age);
            ++age;
        }
        if(mfi.existsInB() && mfi.getAgeB() == eNotThere)
        {
            mfi.setAgeB((e_Age)age);
            if(mfi.m_bEqualAB) mfi.setAgeA((e_Age)age);
            ++age;
        }
        if(mfi.existsInA() && mfi.getAgeA() == eNotThere)
        {
            mfi.setAgeA((e_Age)age);
        }
        if(mfi.getAgeA() != eOld && mfi.getAgeB() != eOld && mfi.getAgeC() != eOld)
        {
            if(mfi.getAgeA() == eMiddle) mfi.setAgeA(eOld);
            if(mfi.getAgeB() == eMiddle) mfi.setAgeB(eOld);
            if(mfi.getAgeC() == eMiddle) mfi.setAgeC(eOld);
        }
    }
}

QModelIndex DirectoryMergeWindow::DirectoryMergeWindowPrivate::nextSibling(const QModelIndex& mi)
{
    QModelIndex miParent = mi.parent();
    int currentIdx = mi.row();
    if(currentIdx + 1 < mi.model()->rowCount(miParent))
        return mi.model()->index(mi.row() + 1, 0, miParent); // next child of parent
    return QModelIndex();
}

// Iterate through the complete tree. Start by specifying QListView::firstChild().
QModelIndex DirectoryMergeWindow::DirectoryMergeWindowPrivate::treeIterator(QModelIndex mi, bool bVisitChildren, bool bFindInvisible)
{
    if(mi.isValid())
    {
        do
        {
            if(bVisitChildren && mi.model()->rowCount(mi) != 0)
                mi = mi.model()->index(0, 0, mi);
            else
            {
                QModelIndex miNextSibling = nextSibling(mi);
                if(miNextSibling.isValid())
                    mi = miNextSibling;
                else
                {
                    mi = mi.parent();
                    while(mi.isValid())
                    {
                        miNextSibling = nextSibling(mi);
                        if(miNextSibling.isValid())
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
        } while(mi.isValid() && q->isRowHidden(mi.row(), mi.parent()) && !bFindInvisible);
    }
    return mi;
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::prepareListView(ProgressProxy& pp)
{
    QStringList errors;
    //TODO   clear();
    PixMapUtils::initPixmaps(m_pOptions->m_newestFileColor, m_pOptions->m_oldestFileColor,
                             m_pOptions->m_midAgeFileColor, m_pOptions->m_missingFileColor);

    q->setRootIsDecorated(true);

    bool bCheckC = isThreeWay();

    t_fileMergeMap::iterator j;
    int nrOfFiles = m_fileMergeMap.size();
    int currentIdx = 1;
    QTime t;
    t.start();
    pp.setMaxNofSteps(nrOfFiles);

    for(j = m_fileMergeMap.begin(); j != m_fileMergeMap.end(); ++j)
    {
        MergeFileInfos& mfi = j.value();

        // const QString& fileName = j->first;
        const QString& fileName = mfi.subPath();

        pp.setInformation(
            i18n("Processing %1 / %2\n%3", currentIdx, nrOfFiles, fileName), currentIdx, false);
        if(pp.wasCancelled()) break;
        ++currentIdx;

        // The comparisons and calculations for each file take place here.
        compareFilesAndCalcAges(mfi, errors);
        // Get dirname from fileName: Search for "/" from end:
        int pos = fileName.lastIndexOf('/');
        QString dirPart;
        QString filePart;
        if(pos == -1)
        {
            // Top dir
            filePart = fileName;
        }
        else
        {
            dirPart = fileName.left(pos);
            filePart = fileName.mid(pos + 1);
        }
        if(dirPart.isEmpty()) // Top level
        {
            m_pRoot->addChild(&mfi); //new DirMergeItem( this, filePart, &mfi );
            mfi.setParent(m_pRoot);
        }
        else
        {
            FileAccess* pFA = mfi.getFileInfoA() ? mfi.getFileInfoA() : mfi.getFileInfoB() ? mfi.getFileInfoB() : mfi.getFileInfoC();
            MergeFileInfos& dirMfi = pFA->parent() ? m_fileMergeMap[FileKey(*pFA->parent())] : *m_pRoot; // parent

            dirMfi.addChild(&mfi); //new DirMergeItem( dirMfi.m_pDMI, filePart, &mfi );
            mfi.setParent(&dirMfi);

            //   // Equality for parent dirs is set in updateFileVisibilities()
        }

        setPixmaps(mfi, bCheckC);
    }

    if(errors.size() > 0)
    {
        if(errors.size() < 15)
        {
            KMessageBox::errorList(q, i18n("Some files could not be processed."), errors);
        }
        else
        {
            KMessageBox::error(q, i18n("Some files could not be processed."));
        }
    }

    beginResetModel();
    endResetModel();
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::calcSuggestedOperation(const QModelIndex& mi, e_MergeOperation eDefaultMergeOp)
{
    MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI == nullptr)
        return;

    bool bCheckC = pMFI->getDirectoryInfo()->dirC().isValid();
    bool bCopyNewer = m_pOptions->m_bDmCopyNewer;
    bool bOtherDest = !((pMFI->getDirectoryInfo()->destDir().absoluteFilePath() == pMFI->getDirectoryInfo()->dirA().absoluteFilePath()) ||
                        (pMFI->getDirectoryInfo()->destDir().absoluteFilePath() == pMFI->getDirectoryInfo()->dirB().absoluteFilePath()) ||
                        (bCheckC && pMFI->getDirectoryInfo()->destDir().absoluteFilePath() == pMFI->getDirectoryInfo()->dirC().absoluteFilePath()));

    if(eDefaultMergeOp == eMergeABCToDest && !bCheckC)
    {
        eDefaultMergeOp = eMergeABToDest;
    }
    if(eDefaultMergeOp == eMergeToAB && bCheckC)
    {
        Q_ASSERT(true);
    }

    if(eDefaultMergeOp == eMergeToA || eDefaultMergeOp == eMergeToB ||
       eDefaultMergeOp == eMergeABCToDest || eDefaultMergeOp == eMergeABToDest || eDefaultMergeOp == eMergeToAB)
    {
        if(!bCheckC)
        {
            if(pMFI->m_bEqualAB)
            {
                setMergeOperation(mi, bOtherDest ? eCopyBToDest : eNoOperation);
            }
            else if(pMFI->existsInA() && pMFI->existsInB())
            {
                if(!bCopyNewer || pMFI->isDirA())
                    setMergeOperation(mi, eDefaultMergeOp);
                else if(bCopyNewer && pMFI->m_bConflictingAges)
                {
                    setMergeOperation(mi, eConflictingAges);
                }
                else
                {
                    if(pMFI->getAgeA() == eNew)
                        setMergeOperation(mi, eDefaultMergeOp == eMergeToAB ? eCopyAToB : eCopyAToDest);
                    else
                        setMergeOperation(mi, eDefaultMergeOp == eMergeToAB ? eCopyBToA : eCopyBToDest);
                }
            }
            else if(!pMFI->existsInA() && pMFI->existsInB())
            {
                if(eDefaultMergeOp == eMergeABToDest)
                    setMergeOperation(mi, eCopyBToDest);
                else if(eDefaultMergeOp == eMergeToB)
                    setMergeOperation(mi, eNoOperation);
                else
                    setMergeOperation(mi, eCopyBToA);
            }
            else if(pMFI->existsInA() && !pMFI->existsInB())
            {
                if(eDefaultMergeOp == eMergeABToDest)
                    setMergeOperation(mi, eCopyAToDest);
                else if(eDefaultMergeOp == eMergeToA)
                    setMergeOperation(mi, eNoOperation);
                else
                    setMergeOperation(mi, eCopyAToB);
            }
            else //if ( !pMFI->existsInA() && !pMFI->existsInB() )
            {
                setMergeOperation(mi, eNoOperation);
            }
        }
        else
        {
            if(pMFI->m_bEqualAB && pMFI->m_bEqualAC)
            {
                setMergeOperation(mi, bOtherDest ? eCopyCToDest : eNoOperation);
            }
            else if(pMFI->existsInA() && pMFI->existsInB() && pMFI->existsInC())
            {
                if(pMFI->m_bEqualAB)
                    setMergeOperation(mi, eCopyCToDest);
                else if(pMFI->m_bEqualAC)
                    setMergeOperation(mi, eCopyBToDest);
                else if(pMFI->m_bEqualBC)
                    setMergeOperation(mi, eCopyCToDest);
                else
                    setMergeOperation(mi, eMergeABCToDest);
            }
            else if(pMFI->existsInA() && pMFI->existsInB() && !pMFI->existsInC())
            {
                if(pMFI->m_bEqualAB)
                    setMergeOperation(mi, eDeleteFromDest);
                else
                    setMergeOperation(mi, eChangedAndDeleted);
            }
            else if(pMFI->existsInA() && !pMFI->existsInB() && pMFI->existsInC())
            {
                if(pMFI->m_bEqualAC)
                    setMergeOperation(mi, eDeleteFromDest);
                else
                    setMergeOperation(mi, eChangedAndDeleted);
            }
            else if(!pMFI->existsInA() && pMFI->existsInB() && pMFI->existsInC())
            {
                if(pMFI->m_bEqualBC)
                    setMergeOperation(mi, eCopyCToDest);
                else
                    setMergeOperation(mi, eMergeABCToDest);
            }
            else if(!pMFI->existsInA() && !pMFI->existsInB() && pMFI->existsInC())
            {
                setMergeOperation(mi, eCopyCToDest);
            }
            else if(!pMFI->existsInA() && pMFI->existsInB() && !pMFI->existsInC())
            {
                setMergeOperation(mi, eCopyBToDest);
            }
            else if(pMFI->existsInA() && !pMFI->existsInB() && !pMFI->existsInC())
            {
                setMergeOperation(mi, eDeleteFromDest);
            }
            else //if ( !pMFI->existsInA() && !pMFI->existsInB() && !pMFI->existsInC() )
            {
                setMergeOperation(mi, eNoOperation);
            }
        }

        // Now check if file/dir-types fit.
        if(pMFI->conflictingFileTypes())
        {
            setMergeOperation(mi, eConflictingFileTypes);
        }
    }
    else
    {
        e_MergeOperation eMO = eDefaultMergeOp;
        switch(eDefaultMergeOp)
        {
            case eConflictingFileTypes:
            case eChangedAndDeleted:
            case eConflictingAges:
            case eDeleteA:
            case eDeleteB:
            case eDeleteAB:
            case eDeleteFromDest:
            case eNoOperation:
                break;
            case eCopyAToB:
                if(!pMFI->existsInA())
                {
                    eMO = eDeleteB;
                }
                break;
            case eCopyBToA:
                if(!pMFI->existsInB())
                {
                    eMO = eDeleteA;
                }
                break;
            case eCopyAToDest:
                if(!pMFI->existsInA())
                {
                    eMO = eDeleteFromDest;
                }
                break;
            case eCopyBToDest:
                if(!pMFI->existsInB())
                {
                    eMO = eDeleteFromDest;
                }
                break;
            case eCopyCToDest:
                if(!pMFI->existsInC())
                {
                    eMO = eDeleteFromDest;
                }
                break;

            case eMergeToA:
            case eMergeToB:
            case eMergeToAB:
            case eMergeABCToDest:
            case eMergeABToDest:
                break;
            default:
                Q_ASSERT(true);
                break;
        }
        setMergeOperation(mi, eMO);
    }
}

void DirectoryMergeWindow::onDoubleClick(const QModelIndex& mi)
{
    if(!mi.isValid())
        return;

    d->m_bSimulatedMergeStarted = false;
    if(d->m_bDirectoryMerge)
        mergeCurrentFile();
    else
        compareCurrentFile();
}

void DirectoryMergeWindow::currentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    QTreeView::currentChanged(current, previous);
    MergeFileInfos* pMFI = d->getMFI(current);
    if(pMFI == nullptr)
        return;

    d->m_pDirectoryMergeInfo->setInfo(pMFI->getDirectoryInfo()->dirA(), pMFI->getDirectoryInfo()->dirB(), pMFI->getDirectoryInfo()->dirC(), pMFI->getDirectoryInfo()->destDir(), *pMFI);
}

void DirectoryMergeWindow::mousePressEvent(QMouseEvent* e)
{
    QTreeView::mousePressEvent(e);
    QModelIndex mi = indexAt(e->pos());
    int c = mi.column();
    QPoint p = e->globalPos();
    MergeFileInfos* pMFI = d->getMFI(mi);
    if(pMFI == nullptr)
        return;

    if(c == s_OpCol)
    {
        bool bThreeDirs = d->isThreeWay();

        QMenu m(this);
        if(bThreeDirs)
        {
            m.addAction(d->m_pDirCurrentDoNothing);
            int count = 0;
            if(pMFI->existsInA())
            {
                m.addAction(d->m_pDirCurrentChooseA);
                ++count;
            }
            if(pMFI->existsInB())
            {
                m.addAction(d->m_pDirCurrentChooseB);
                ++count;
            }
            if(pMFI->existsInC())
            {
                m.addAction(d->m_pDirCurrentChooseC);
                ++count;
            }
            if(!pMFI->conflictingFileTypes() && count > 1) m.addAction(d->m_pDirCurrentMerge);
            m.addAction(d->m_pDirCurrentDelete);
        }
        else if(d->m_bSyncMode)
        {
            m.addAction(d->m_pDirCurrentSyncDoNothing);
            if(pMFI->existsInA()) m.addAction(d->m_pDirCurrentSyncCopyAToB);
            if(pMFI->existsInB()) m.addAction(d->m_pDirCurrentSyncCopyBToA);
            if(pMFI->existsInA()) m.addAction(d->m_pDirCurrentSyncDeleteA);
            if(pMFI->existsInB()) m.addAction(d->m_pDirCurrentSyncDeleteB);
            if(pMFI->existsInA() && pMFI->existsInB())
            {
                m.addAction(d->m_pDirCurrentSyncDeleteAAndB);
                if(!pMFI->conflictingFileTypes())
                {
                    m.addAction(d->m_pDirCurrentSyncMergeToA);
                    m.addAction(d->m_pDirCurrentSyncMergeToB);
                    m.addAction(d->m_pDirCurrentSyncMergeToAAndB);
                }
            }
        }
        else
        {
            m.addAction(d->m_pDirCurrentDoNothing);
            if(pMFI->existsInA())
            {
                m.addAction(d->m_pDirCurrentChooseA);
            }
            if(pMFI->existsInB())
            {
                m.addAction(d->m_pDirCurrentChooseB);
            }
            if(!pMFI->conflictingFileTypes() && pMFI->existsInA() && pMFI->existsInB()) m.addAction(d->m_pDirCurrentMerge);
            m.addAction(d->m_pDirCurrentDelete);
        }

        m.exec(p);
    }
    else if(c == s_ACol || c == s_BCol || c == s_CCol)
    {
        QString itemPath;
        if(c == s_ACol && pMFI->existsInA())
        {
            itemPath = pMFI->fullNameA();
        }
        else if(c == s_BCol && pMFI->existsInB())
        {
            itemPath = pMFI->fullNameB();
        }
        else if(c == s_CCol && pMFI->existsInC())
        {
            itemPath = pMFI->fullNameC();
        }

        if(!itemPath.isEmpty())
        {
            d->selectItemAndColumn(mi, e->button() == Qt::RightButton);
        }
    }
}

#ifndef QT_NO_CONTEXTMENU
void DirectoryMergeWindow::contextMenuEvent(QContextMenuEvent* e)
{
    QModelIndex mi = indexAt(e->pos());
    int c = mi.column();

    MergeFileInfos* pMFI = d->getMFI(mi);
    if(pMFI == nullptr)
        return;
    if(c == s_ACol || c == s_BCol || c == s_CCol)
    {
        QString itemPath;
        if(c == s_ACol && pMFI->existsInA())
        {
            itemPath = pMFI->fullNameA();
        }
        else if(c == s_BCol && pMFI->existsInB())
        {
            itemPath = pMFI->fullNameB();
        }
        else if(c == s_CCol && pMFI->existsInC())
        {
            itemPath = pMFI->fullNameC();
        }

        if(!itemPath.isEmpty())
        {
            d->selectItemAndColumn(mi, true);
            QMenu m(this);
            m.addAction(d->m_pDirCompareExplicit);
            m.addAction(d->m_pDirMergeExplicit);

            m.popup(e->globalPos());
        }
    }
}
#endif

QString DirectoryMergeWindow::DirectoryMergeWindowPrivate::getFileName(const QModelIndex& mi)
{
    MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI != nullptr)
    {
        return mi.column() == s_ACol ? pMFI->getFileInfoA()->absoluteFilePath() : mi.column() == s_BCol ? pMFI->getFileInfoB()->absoluteFilePath() : mi.column() == s_CCol ? pMFI->getFileInfoC()->absoluteFilePath() : QString("");
    }
    return QString();
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::isDir(const QModelIndex& mi)
{
    MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI != nullptr)
    {
        return mi.column() == s_ACol ? pMFI->isDirA() : mi.column() == s_BCol ? pMFI->isDirB() : pMFI->isDirC();
    }
    return false;
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::selectItemAndColumn(const QModelIndex& mi, bool bContextMenu)
{
    if(bContextMenu && (mi == m_selection1Index || mi == m_selection2Index || mi == m_selection3Index))
        return;

    QModelIndex old1 = m_selection1Index;
    QModelIndex old2 = m_selection2Index;
    QModelIndex old3 = m_selection3Index;

    bool bReset = false;

    if(m_selection1Index.isValid())
    {
        if(isDir(m_selection1Index) != isDir(mi))
            bReset = true;
    }

    if(bReset || m_selection3Index.isValid() || mi == m_selection1Index || mi == m_selection2Index || mi == m_selection3Index)
    {
        // restart
        m_selection1Index = QModelIndex();
        m_selection2Index = QModelIndex();
        m_selection3Index = QModelIndex();
    }
    else if(!m_selection1Index.isValid())
    {
        m_selection1Index = mi;
        m_selection2Index = QModelIndex();
        m_selection3Index = QModelIndex();
    }
    else if(!m_selection2Index.isValid())
    {
        m_selection2Index = mi;
        m_selection3Index = QModelIndex();
    }
    else if(!m_selection3Index.isValid())
    {
        m_selection3Index = mi;
    }
    if(old1.isValid()) emit dataChanged(old1, old1);
    if(old2.isValid()) emit dataChanged(old2, old2);
    if(old3.isValid()) emit dataChanged(old3, old3);
    if(m_selection1Index.isValid()) emit dataChanged(m_selection1Index, m_selection1Index);
    if(m_selection2Index.isValid()) emit dataChanged(m_selection2Index, m_selection2Index);
    if(m_selection3Index.isValid()) emit dataChanged(m_selection3Index, m_selection3Index);
    emit q->updateAvailabilities();
}

//TODO
//void DirMergeItem::init(MergeFileInfos* pMFI)
//{
//   pMFI->m_pDMI = this; //no not here
//   m_pMFI = pMFI;
//   TotalDiffStatus& tds = pMFI->m_totalDiffStatus;
//   if ( m_pMFI->dirA() || m_pMFI->dirB() || m_pMFI->isDirC() )
//   {
//   }
//   else
//   {
//      setText( s_UnsolvedCol, QString::number( tds.getUnsolvedConflicts() ) );
//      setText( s_SolvedCol,   QString::number( tds.getSolvedConflicts() ) );
//      setText( s_NonWhiteCol, QString::number( tds.getUnsolvedConflicts() + tds.getSolvedConflicts() - tds.getWhitespaceConflicts() ) );
//      setText( s_WhiteCol,    QString::number( tds.getWhitespaceConflicts() ) );
//   }
//   setSizeHint( s_ACol, QSize(17,17) ); // Iconsize
//   setSizeHint( s_BCol, QSize(17,17) ); // Iconsize
//   setSizeHint( s_CCol, QSize(17,17) ); // Iconsize
//}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::sort(int column, Qt::SortOrder order)
{
    Q_UNUSED(column);
    beginResetModel();
    m_pRoot->sort(order);
    endResetModel();
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::setMergeOperation(const QModelIndex& mi, e_MergeOperation eMergeOp, bool bRecursive)
{
    MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI == nullptr)
        return;

    if(eMergeOp != pMFI->getOperation())
    {
        pMFI->m_bOperationComplete = false;
        setOpStatus(mi, eOpStatusNone);
    }

    pMFI->setOperation(eMergeOp);
    if(bRecursive)
    {
        e_MergeOperation eChildrenMergeOp = pMFI->getOperation();
        if(eChildrenMergeOp == eConflictingFileTypes) eChildrenMergeOp = eMergeABCToDest;
        for(int childIdx = 0; childIdx < pMFI->children().count(); ++childIdx)
        {
            calcSuggestedOperation(index(childIdx, 0, mi), eChildrenMergeOp);
        }
    }
}

void DirectoryMergeWindow::compareCurrentFile()
{
    if(!d->canContinue()) return;

    if(d->m_bRealMergeStarted)
    {
        KMessageBox::sorry(this, i18n("This operation is currently not possible."), i18n("Operation Not Possible"));
        return;
    }

    if(MergeFileInfos* pMFI = d->getMFI(currentIndex()))
    {
        if(!(pMFI->isDirA() || pMFI->isDirB() || pMFI->isDirC()))
        {
            emit startDiffMerge(
                pMFI->existsInA() ? pMFI->getFileInfoA()->absoluteFilePath() : QString(""),
                pMFI->existsInB() ? pMFI->getFileInfoB()->absoluteFilePath() : QString(""),
                pMFI->existsInC() ? pMFI->getFileInfoC()->absoluteFilePath() : QString(""),
                "",
                "", "", "", nullptr);
        }
    }
    emit updateAvailabilities();
}

void DirectoryMergeWindow::slotCompareExplicitlySelectedFiles()
{
    if(!d->isDir(d->m_selection1Index) && !d->canContinue()) return;

    if(d->m_bRealMergeStarted)
    {
        KMessageBox::sorry(this, i18n("This operation is currently not possible."), i18n("Operation Not Possible"));
        return;
    }

    emit startDiffMerge(
        d->getFileName(d->m_selection1Index),
        d->getFileName(d->m_selection2Index),
        d->getFileName(d->m_selection3Index),
        "",
        "", "", "", nullptr);
    d->m_selection1Index = QModelIndex();
    d->m_selection2Index = QModelIndex();
    d->m_selection3Index = QModelIndex();

    emit updateAvailabilities();
    update();
}

void DirectoryMergeWindow::slotMergeExplicitlySelectedFiles()
{
    if(!d->isDir(d->m_selection1Index) && !d->canContinue()) return;

    if(d->m_bRealMergeStarted)
    {
        KMessageBox::sorry(this, i18n("This operation is currently not possible."), i18n("Operation Not Possible"));
        return;
    }

    QString fn1 = d->getFileName(d->m_selection1Index);
    QString fn2 = d->getFileName(d->m_selection2Index);
    QString fn3 = d->getFileName(d->m_selection3Index);

    emit startDiffMerge(fn1, fn2, fn3,
                        fn3.isEmpty() ? fn2 : fn3,
                        "", "", "", nullptr);
    d->m_selection1Index = QModelIndex();
    d->m_selection2Index = QModelIndex();
    d->m_selection3Index = QModelIndex();

    emit updateAvailabilities();
    update();
}

bool DirectoryMergeWindow::isFileSelected()
{
    if(MergeFileInfos* pMFI = d->getMFI(currentIndex()))
    {
        return !(pMFI->isDirA() || pMFI->isDirB() || pMFI->isDirC() || pMFI->conflictingFileTypes());
    }
    return false;
}

void DirectoryMergeWindow::mergeResultSaved(const QString& fileName)
{
    QModelIndex mi = (d->m_mergeItemList.empty() || d->m_currentIndexForOperation == d->m_mergeItemList.end())
                         ? QModelIndex()
                         : *d->m_currentIndexForOperation;

    MergeFileInfos* pMFI = d->getMFI(mi);
    if(pMFI == nullptr)
    {
        // This can happen if the same file is saved and modified and saved again. Nothing to do then.
        return;
    }
    if(fileName == pMFI->fullNameDest())
    {
        if(pMFI->getOperation() == eMergeToAB)
        {
            bool bSuccess = d->copyFLD(pMFI->fullNameB(), pMFI->fullNameA());
            if(!bSuccess)
            {
                KMessageBox::error(this, i18n("An error occurred while copying."));
                d->m_pStatusInfo->setWindowTitle(i18n("Merge Error"));
                d->m_pStatusInfo->exec();
                //if ( m_pStatusInfo->firstChild()!=0 )
                //   m_pStatusInfo->ensureItemVisible( m_pStatusInfo->last() );
                d->m_bError = true;
                d->setOpStatus(mi, eOpStatusError);
                pMFI->setOperation(eCopyBToA);
                return;
            }
        }
        d->setOpStatus(mi, eOpStatusDone);
        pMFI->m_bOperationComplete = true;
        if(d->m_mergeItemList.size() == 1)
        {
            d->m_mergeItemList.clear();
            d->m_bRealMergeStarted = false;
        }
    }

    emit updateAvailabilities();
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::canContinue()
{
    bool bCanContinue = false;

    emit q->checkIfCanContinue(&bCanContinue);

    if(bCanContinue && !m_bError)
    {
        QModelIndex mi = (m_mergeItemList.empty() || m_currentIndexForOperation == m_mergeItemList.end()) ? QModelIndex() : *m_currentIndexForOperation;
        MergeFileInfos* pMFI = getMFI(mi);
        if(pMFI && !pMFI->m_bOperationComplete)
        {
            setOpStatus(mi, eOpStatusNotSaved);
            pMFI->m_bOperationComplete = true;
            if(m_mergeItemList.size() == 1)
            {
                m_mergeItemList.clear();
                m_bRealMergeStarted = false;
            }
        }
    }
    return bCanContinue;
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::executeMergeOperation(MergeFileInfos& mfi, bool& bSingleFileMerge)
{
    bool bCreateBackups = m_pOptions->m_bDmCreateBakFiles;
    // First decide destname
    QString destName;
    switch(mfi.getOperation())
    {
        case eNoOperation:
            break;
        case eDeleteAB:
            break;
        case eMergeToAB: // let the user save in B. In mergeResultSaved() the file will be copied to A.
        case eMergeToB:
        case eDeleteB:
        case eCopyAToB:
            destName = mfi.fullNameB();
            break;
        case eMergeToA:
        case eDeleteA:
        case eCopyBToA:
            destName = mfi.fullNameA();
            break;
        case eMergeABToDest:
        case eMergeABCToDest:
        case eCopyAToDest:
        case eCopyBToDest:
        case eCopyCToDest:
        case eDeleteFromDest:
            destName = mfi.fullNameDest();
            break;
        default:
            KMessageBox::error(q, i18n("Unknown merge operation. (This must never happen!)"));
    }

    bool bSuccess = false;
    bSingleFileMerge = false;
    switch(mfi.getOperation())
    {
        case eNoOperation:
            bSuccess = true;
            break;
        case eCopyAToDest:
        case eCopyAToB:
            bSuccess = copyFLD(mfi.fullNameA(), destName);
            break;
        case eCopyBToDest:
        case eCopyBToA:
            bSuccess = copyFLD(mfi.fullNameB(), destName);
            break;
        case eCopyCToDest:
            bSuccess = copyFLD(mfi.fullNameC(), destName);
            break;
        case eDeleteFromDest:
        case eDeleteA:
        case eDeleteB:
            bSuccess = deleteFLD(destName, bCreateBackups);
            break;
        case eDeleteAB:
            bSuccess = deleteFLD(mfi.fullNameA(), bCreateBackups) &&
                       deleteFLD(mfi.fullNameB(), bCreateBackups);
            break;
        case eMergeABToDest:
        case eMergeToA:
        case eMergeToAB:
        case eMergeToB:
            bSuccess = mergeFLD(mfi.fullNameA(), mfi.fullNameB(), "",
                                destName, bSingleFileMerge);
            break;
        case eMergeABCToDest:
            bSuccess = mergeFLD(
                mfi.existsInA() ? mfi.fullNameA() : QString(""),
                mfi.existsInB() ? mfi.fullNameB() : QString(""),
                mfi.existsInC() ? mfi.fullNameC() : QString(""),
                destName, bSingleFileMerge);
            break;
        default:
            KMessageBox::error(q, i18n("Unknown merge operation."));
    }

    return bSuccess;
}

// Check if the merge can start, and prepare the m_mergeItemList which then contains all
// items that must be merged.
void DirectoryMergeWindow::DirectoryMergeWindowPrivate::prepareMergeStart(const QModelIndex& miBegin, const QModelIndex& miEnd, bool bVerbose)
{
    if(bVerbose)
    {
        int status = KMessageBox::warningYesNoCancel(q,
                                                     i18n("The merge is about to begin.\n\n"
                                                          "Choose \"Do it\" if you have read the instructions and know what you are doing.\n"
                                                          "Choosing \"Simulate it\" will tell you what would happen.\n\n"
                                                          "Be aware that this program still has beta status "
                                                          "and there is NO WARRANTY whatsoever! Make backups of your vital data!"),
                                                     i18n("Starting Merge"),
                                                     KGuiItem(i18n("Do It")),
                                                     KGuiItem(i18n("Simulate It")));
        if(status == KMessageBox::Yes)
            m_bRealMergeStarted = true;
        else if(status == KMessageBox::No)
            m_bSimulatedMergeStarted = true;
        else
            return;
    }
    else
    {
        m_bRealMergeStarted = true;
    }

    m_mergeItemList.clear();
    if(!miBegin.isValid())
        return;

    for(QModelIndex mi = miBegin; mi != miEnd; mi = treeIterator(mi))
    {
        MergeFileInfos* pMFI = getMFI(mi);
        if(pMFI && !pMFI->m_bOperationComplete)
        {
            m_mergeItemList.push_back(mi);
            QString errorText;
            if(pMFI->getOperation() == eConflictingFileTypes)
            {
                errorText = i18n("The highlighted item has a different type in the different directories. Select what to do.");
            }
            if(pMFI->getOperation() == eConflictingAges)
            {
                errorText = i18n("The modification dates of the file are equal but the files are not. Select what to do.");
            }
            if(pMFI->getOperation() == eChangedAndDeleted)
            {
                errorText = i18n("The highlighted item was changed in one directory and deleted in the other. Select what to do.");
            }
            if(!errorText.isEmpty())
            {
                q->scrollTo(mi, QAbstractItemView::EnsureVisible);
                q->setCurrentIndex(mi);
                KMessageBox::error(q, errorText);
                m_mergeItemList.clear();
                m_bRealMergeStarted = false;
                return;
            }
        }
    }

    m_currentIndexForOperation = m_mergeItemList.begin();
    return;
}

void DirectoryMergeWindow::slotRunOperationForCurrentItem()
{
    if(!d->canContinue()) return;

    bool bVerbose = false;
    if(d->m_mergeItemList.empty())
    {
        QModelIndex miBegin = currentIndex();
        QModelIndex miEnd = d->treeIterator(miBegin, false, false); // find next visible sibling (no children)

        d->prepareMergeStart(miBegin, miEnd, bVerbose);
        d->mergeContinue(true, bVerbose);
    }
    else
        d->mergeContinue(false, bVerbose);
}

void DirectoryMergeWindow::slotRunOperationForAllItems()
{
    if(!d->canContinue()) return;

    bool bVerbose = true;
    if(d->m_mergeItemList.empty())
    {
        QModelIndex miBegin = d->rowCount() > 0 ? d->index(0, 0, QModelIndex()) : QModelIndex();

        d->prepareMergeStart(miBegin, QModelIndex(), bVerbose);
        d->mergeContinue(true, bVerbose);
    }
    else
        d->mergeContinue(false, bVerbose);
}

void DirectoryMergeWindow::mergeCurrentFile()
{
    if(!d->canContinue()) return;

    if(d->m_bRealMergeStarted)
    {
        KMessageBox::sorry(this, i18n("This operation is currently not possible because directory merge is currently running."), i18n("Operation Not Possible"));
        return;
    }

    if(isFileSelected())
    {
        MergeFileInfos* pMFI = d->getMFI(currentIndex());
        if(pMFI != nullptr)
        {
            d->m_mergeItemList.clear();
            d->m_mergeItemList.push_back(currentIndex());
            d->m_currentIndexForOperation = d->m_mergeItemList.begin();
            bool bDummy = false;
            d->mergeFLD(
                pMFI->existsInA() ? pMFI->getFileInfoA()->absoluteFilePath() : QString(""),
                pMFI->existsInB() ? pMFI->getFileInfoB()->absoluteFilePath() : QString(""),
                pMFI->existsInC() ? pMFI->getFileInfoC()->absoluteFilePath() : QString(""),
                pMFI->fullNameDest(),
                bDummy);
        }
    }
    emit updateAvailabilities();
}

// When bStart is true then m_currentIndexForOperation must still be processed.
// When bVerbose is true then a messagebox will tell when the merge is complete.
void DirectoryMergeWindow::DirectoryMergeWindowPrivate::mergeContinue(bool bStart, bool bVerbose)
{
    ProgressProxy pp;
    if(m_mergeItemList.empty())
        return;

    int nrOfItems = 0;
    int nrOfCompletedItems = 0;
    int nrOfCompletedSimItems = 0;

    // Count the number of completed items (for the progress bar).
    for(MergeItemList::iterator i = m_mergeItemList.begin(); i != m_mergeItemList.end(); ++i)
    {
        MergeFileInfos* pMFI = getMFI(*i);
        ++nrOfItems;
        if(pMFI->m_bOperationComplete)
            ++nrOfCompletedItems;
        if(pMFI->m_bSimOpComplete)
            ++nrOfCompletedSimItems;
    }

    m_pStatusInfo->hide();
    m_pStatusInfo->clear();

    QModelIndex miCurrent = m_currentIndexForOperation == m_mergeItemList.end() ? QModelIndex() : *m_currentIndexForOperation;

    bool bContinueWithCurrentItem = bStart; // true for first item, else false
    bool bSkipItem = false;
    if(!bStart && m_bError && miCurrent.isValid())
    {
        int status = KMessageBox::warningYesNoCancel(q,
                                                     i18n("There was an error in the last step.\n"
                                                          "Do you want to continue with the item that caused the error or do you want to skip this item?"),
                                                     i18n("Continue merge after an error"),
                                                     KGuiItem(i18n("Continue With Last Item")),
                                                     KGuiItem(i18n("Skip Item")));
        if(status == KMessageBox::Yes)
            bContinueWithCurrentItem = true;
        else if(status == KMessageBox::No)
            bSkipItem = true;
        else
            return;
        m_bError = false;
    }

    pp.setMaxNofSteps(nrOfItems);

    bool bSuccess = true;
    bool bSingleFileMerge = false;
    bool bSim = m_bSimulatedMergeStarted;
    while(bSuccess)
    {
        MergeFileInfos* pMFI = getMFI(miCurrent);
        if(pMFI == nullptr)
        {
            m_mergeItemList.clear();
            m_bRealMergeStarted = false;
            break;
        }

        if(pMFI != nullptr && !bContinueWithCurrentItem)
        {
            if(bSim)
            {
                if(rowCount(miCurrent) == 0)
                {
                    pMFI->m_bSimOpComplete = true;
                }
            }
            else
            {
                if(rowCount(miCurrent) == 0)
                {
                    if(!pMFI->m_bOperationComplete)
                    {
                        setOpStatus(miCurrent, bSkipItem ? eOpStatusSkipped : eOpStatusDone);
                        pMFI->m_bOperationComplete = true;
                        bSkipItem = false;
                    }
                }
                else
                {
                    setOpStatus(miCurrent, eOpStatusInProgress);
                }
            }
        }

        if(!bContinueWithCurrentItem)
        {
            // Depth first
            QModelIndex miPrev = miCurrent;
            ++m_currentIndexForOperation;
            miCurrent = m_currentIndexForOperation == m_mergeItemList.end() ? QModelIndex() : *m_currentIndexForOperation;
            if((!miCurrent.isValid() || miCurrent.parent() != miPrev.parent()) && miPrev.parent().isValid())
            {
                // Check if the parent may be set to "Done"
                QModelIndex miParent = miPrev.parent();
                bool bDone = true;
                while(bDone && miParent.isValid())
                {
                    for(int childIdx = 0; childIdx < rowCount(miParent); ++childIdx)
                    {
                        pMFI = getMFI(index(childIdx, 0, miParent));
                        if((!bSim && !pMFI->m_bOperationComplete) || (bSim && pMFI->m_bSimOpComplete))
                        {
                            bDone = false;
                            break;
                        }
                    }
                    if(bDone)
                    {
                        pMFI = getMFI(miParent);
                        if(bSim)
                            pMFI->m_bSimOpComplete = bDone;
                        else
                        {
                            setOpStatus(miParent, eOpStatusDone);
                            pMFI->m_bOperationComplete = bDone;
                        }
                    }
                    miParent = miParent.parent();
                }
            }
        }

        if(!miCurrent.isValid()) // end?
        {
            if(m_bRealMergeStarted)
            {
                if(bVerbose)
                {
                    KMessageBox::information(q, i18n("Merge operation complete."), i18n("Merge Complete"));
                }
                m_bRealMergeStarted = false;
                m_pStatusInfo->setWindowTitle(i18n("Merge Complete"));
            }
            if(m_bSimulatedMergeStarted)
            {
                m_bSimulatedMergeStarted = false;
                QModelIndex mi = rowCount() > 0 ? index(0, 0, QModelIndex()) : QModelIndex();
                for(; mi.isValid(); mi = treeIterator(mi))
                {
                    getMFI(mi)->m_bSimOpComplete = false;
                }
                m_pStatusInfo->setWindowTitle(i18n("Simulated merge complete: Check if you agree with the proposed operations."));
                m_pStatusInfo->exec();
            }
            m_mergeItemList.clear();
            m_bRealMergeStarted = false;
            return;
        }

        pMFI = getMFI(miCurrent);

        pp.setInformation(pMFI->subPath(),
                          bSim ? nrOfCompletedSimItems : nrOfCompletedItems,
                          false // bRedrawUpdate
        );

        bSuccess = executeMergeOperation(*pMFI, bSingleFileMerge); // Here the real operation happens.

        if(bSuccess)
        {
            if(bSim)
                ++nrOfCompletedSimItems;
            else
                ++nrOfCompletedItems;
            bContinueWithCurrentItem = false;
        }

        if(pp.wasCancelled())
            break;
    } // end while

    //g_pProgressDialog->hide();

    q->setCurrentIndex(miCurrent);
    q->scrollTo(miCurrent, EnsureVisible);
    if(!bSuccess && !bSingleFileMerge)
    {
        KMessageBox::error(q, i18n("An error occurred. Press OK to see detailed information."));
        m_pStatusInfo->setWindowTitle(i18n("Merge Error"));
        m_pStatusInfo->exec();
        //if ( m_pStatusInfo->firstChild()!=0 )
        //   m_pStatusInfo->ensureItemVisible( m_pStatusInfo->last() );
        m_bError = true;

        setOpStatus(miCurrent, eOpStatusError);
    }
    else
    {
        m_bError = false;
    }
    emit q->updateAvailabilities();

    if(m_currentIndexForOperation == m_mergeItemList.end())
    {
        m_mergeItemList.clear();
        m_bRealMergeStarted = false;
    }
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::deleteFLD(const QString& name, bool bCreateBackup)
{
    FileAccess fi(name, true);
    if(!fi.exists())
        return true;

    if(bCreateBackup)
    {
        bool bSuccess = renameFLD(name, name + ".orig");
        if(!bSuccess)
        {
            m_pStatusInfo->addText(i18n("Error: While deleting %1: Creating backup failed.", name));
            return false;
        }
    }
    else
    {
        if(fi.isDir() && !fi.isSymLink())
            m_pStatusInfo->addText(i18n("delete directory recursively( %1 )", name));
        else
            m_pStatusInfo->addText(i18n("delete( %1 )", name));

        if(m_bSimulatedMergeStarted)
        {
            return true;
        }

        if(fi.isDir() && !fi.isSymLink()) // recursive directory delete only for real dirs, not symlinks
        {
            t_DirectoryList dirList;
            bool bSuccess = fi.listDir(&dirList, false, true, "*", "", "", false, false); // not recursive, find hidden files

            if(!bSuccess)
            {
                // No Permission to read directory or other error.
                m_pStatusInfo->addText(i18n("Error: delete dir operation failed while trying to read the directory."));
                return false;
            }

            t_DirectoryList::iterator it; // create list iterator

            for(it = dirList.begin(); it != dirList.end(); ++it) // for each file...
            {
                FileAccess& fi2 = *it;
                Q_ASSERT(fi2.fileName() != "." && fi2.fileName() != "..");

                bSuccess = deleteFLD(fi2.absoluteFilePath(), false);
                if(!bSuccess) break;
            }
            if(bSuccess)
            {
                bSuccess = FileAccess::removeDir(name);
                if(!bSuccess)
                {
                    m_pStatusInfo->addText(i18n("Error: rmdir( %1 ) operation failed.", name)); // krazy:exclude=syscalls
                    return false;
                }
            }
        }
        else
        {
            bool bSuccess = fi.removeFile();
            if(!bSuccess)
            {
                m_pStatusInfo->addText(i18n("Error: delete operation failed."));
                return false;
            }
        }
    }
    return true;
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::mergeFLD(const QString& nameA, const QString& nameB, const QString& nameC, const QString& nameDest, bool& bSingleFileMerge)
{
    FileAccess fi(nameA);
    if(fi.isDir())
    {
        return makeDir(nameDest);
    }

    // Make sure that the dir exists, into which we will save the file later.
    int pos = nameDest.lastIndexOf('/');
    if(pos > 0)
    {
        QString parentName = nameDest.left(pos);
        bool bSuccess = makeDir(parentName, true /*quiet*/);
        if(!bSuccess)
            return false;
    }

    m_pStatusInfo->addText(i18n("manual merge( %1, %2, %3 -> %4)", nameA, nameB, nameC, nameDest));
    if(m_bSimulatedMergeStarted)
    {
        m_pStatusInfo->addText(i18n("     Note: After a manual merge the user should continue by pressing F7."));
        return true;
    }

    bSingleFileMerge = true;
    setOpStatus(*m_currentIndexForOperation, eOpStatusInProgress);
    q->scrollTo(*m_currentIndexForOperation, EnsureVisible);

    emit q->startDiffMerge(nameA, nameB, nameC, nameDest, "", "", "", nullptr);

    return false;
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::copyFLD(const QString& srcName, const QString& destName)
{
    bool bSuccess = false;

    if(srcName == destName)
        return true;

    FileAccess fi(srcName);
    FileAccess faDest(destName, true);
    if(faDest.exists() && !(fi.isDir() && faDest.isDir() && (fi.isSymLink() == faDest.isSymLink())))
    {
        bSuccess = deleteFLD(destName, m_pOptions->m_bDmCreateBakFiles);
        if(!bSuccess)
        {
            m_pStatusInfo->addText(i18n("Error: copy( %1 -> %2 ) failed."
                                        "Deleting existing destination failed.",
                                        srcName, destName));
            return bSuccess;
        }
    }

    if(fi.isSymLink() && ((fi.isDir() && !m_bFollowDirLinks) || (!fi.isDir() && !m_bFollowFileLinks)))
    {
        m_pStatusInfo->addText(i18n("copyLink( %1 -> %2 )", srcName, destName));

        if(m_bSimulatedMergeStarted)
        {
            return true;
        }
        FileAccess destFi(destName);
        if(!destFi.isLocal() || !fi.isLocal())
        {
            m_pStatusInfo->addText(i18n("Error: copyLink failed: Remote links are not yet supported."));
            return false;
        }

        bSuccess = false;
        QString linkTarget = fi.readLink();
        if(!linkTarget.isEmpty())
        {
            bSuccess = FileAccess::symLink(linkTarget, destName);
            if(!bSuccess)
                m_pStatusInfo->addText(i18n("Error: copyLink failed."));
        }
        return bSuccess;
    }

    if(fi.isDir())
    {
        if(faDest.exists())
            return true;
        else
        {
            bSuccess = makeDir(destName);
            return bSuccess;
        }
    }

    int pos = destName.lastIndexOf('/');
    if(pos > 0)
    {
        QString parentName = destName.left(pos);
        bSuccess = makeDir(parentName, true /*quiet*/);
        if(!bSuccess)
            return false;
    }

    m_pStatusInfo->addText(i18n("copy( %1 -> %2 )", srcName, destName));

    if(m_bSimulatedMergeStarted)
    {
        return true;
    }

    FileAccess faSrc(srcName);
    bSuccess = faSrc.copyFile(destName);
    if(!bSuccess) m_pStatusInfo->addText(faSrc.getStatusText());
    return bSuccess;
}

// Rename is not an operation that can be selected by the user.
// It will only be used to create backups.
// Hence it will delete an existing destination without making a backup (of the old backup.)
bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::renameFLD(const QString& srcName, const QString& destName)
{
    if(srcName == destName)
        return true;
    FileAccess destFile = FileAccess(destName, true);
    if(destFile.exists())
    {
        bool bSuccess = deleteFLD(destName, false /*no backup*/);
        if(!bSuccess)
        {
            m_pStatusInfo->addText(i18n("Error during rename( %1 -> %2 ): "
                                        "Cannot delete existing destination.",
                                        srcName, destName));
            return false;
        }
    }

    m_pStatusInfo->addText(i18n("rename( %1 -> %2 )", srcName, destName));
    if(m_bSimulatedMergeStarted)
    {
        return true;
    }

    bool bSuccess = FileAccess(srcName).rename(destFile);
    if(!bSuccess)
    {
        m_pStatusInfo->addText(i18n("Error: Rename failed."));
        return false;
    }

    return true;
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::makeDir(const QString& name, bool bQuiet)
{
    FileAccess fi(name, true);
    if(fi.exists() && fi.isDir())
        return true;

    if(fi.exists() && !fi.isDir())
    {
        bool bSuccess = deleteFLD(name, true);
        if(!bSuccess)
        {
            m_pStatusInfo->addText(i18n("Error during makeDir of %1. "
                                        "Cannot delete existing file.",
                                        name));
            return false;
        }
    }

    int pos = name.lastIndexOf('/');
    if(pos > 0)
    {
        QString parentName = name.left(pos);
        bool bSuccess = makeDir(parentName, true);
        if(!bSuccess)
            return false;
    }

    if(!bQuiet)
        m_pStatusInfo->addText(i18n("makeDir( %1 )", name));

    if(m_bSimulatedMergeStarted)
    {
        return true;
    }

    bool bSuccess = FileAccess::makeDir(name);
    if(!bSuccess)
    {
        m_pStatusInfo->addText(i18n("Error while creating directory."));
        return false;
    }
    return true;
}

DirectoryMergeInfo::DirectoryMergeInfo(QWidget* pParent)
    : QFrame(pParent)
{
    QVBoxLayout* topLayout = new QVBoxLayout(this);
    topLayout->setMargin(0);

    QGridLayout* grid = new QGridLayout();
    topLayout->addLayout(grid);
    grid->setColumnStretch(1, 10);

    int line = 0;

    m_pA = new QLabel(i18n("A"), this);
    grid->addWidget(m_pA, line, 0);
    m_pInfoA = new QLabel(this);
    grid->addWidget(m_pInfoA, line, 1);
    ++line;

    m_pB = new QLabel(i18n("B"), this);
    grid->addWidget(m_pB, line, 0);
    m_pInfoB = new QLabel(this);
    grid->addWidget(m_pInfoB, line, 1);
    ++line;

    m_pC = new QLabel(i18n("C"), this);
    grid->addWidget(m_pC, line, 0);
    m_pInfoC = new QLabel(this);
    grid->addWidget(m_pInfoC, line, 1);
    ++line;

    m_pDest = new QLabel(i18n("Dest"), this);
    grid->addWidget(m_pDest, line, 0);
    m_pInfoDest = new QLabel(this);
    grid->addWidget(m_pInfoDest, line, 1);
    ++line;

    m_pInfoList = new QTreeWidget(this);
    topLayout->addWidget(m_pInfoList);
    m_pInfoList->setHeaderLabels(QStringList() << i18n("Dir") << i18n("Type") << i18n("Size")
                                               << i18n("Attr") << i18n("Last Modification") << i18n("Link-Destination"));
    setMinimumSize(100, 100);

    m_pInfoList->installEventFilter(this);
    m_pInfoList->setRootIsDecorated(false);
}

bool DirectoryMergeInfo::eventFilter(QObject* o, QEvent* e)
{
    if(e->type() == QEvent::FocusIn && o == m_pInfoList)
        emit gotFocus();
    return false;
}

void DirectoryMergeInfo::addListViewItem(const QString& dir, const QString& basePath, FileAccess* fi)
{
    if(basePath.isEmpty())
    {
        return;
    }
    else
    {
        if(fi != nullptr && fi->exists())
        {
            QString dateString = fi->lastModified().toString("yyyy-MM-dd hh:mm:ss");
            //TODO: Move logic to FileAccess
            m_pInfoList->addTopLevelItem(new QTreeWidgetItem(
                m_pInfoList,
                QStringList() << dir << QString(fi->isDir() ? i18n("Dir") : i18n("File")) + (fi->isSymLink() ? i18n("-Link") : "") << QString::number(fi->size()) << QLatin1String(fi->isReadable() ? "r" : " ") + QLatin1String(fi->isWritable() ? "w" : " ") + QLatin1String((fi->isExecutable() ? "x" : " ")) << dateString << QString(fi->isSymLink() ? (" -> " + fi->readLink()) : QString(""))));
        }
        else
        {
            m_pInfoList->addTopLevelItem(new QTreeWidgetItem(
                m_pInfoList,
                QStringList() << dir << i18n("not available") << ""
                              << ""
                              << ""
                              << ""));
        }
    }
}

void DirectoryMergeInfo::setInfo(
    const FileAccess& dirA,
    const FileAccess& dirB,
    const FileAccess& dirC,
    const FileAccess& dirDest,
    MergeFileInfos& mfi)
{
    bool bHideDest = false;
    if(dirA.absoluteFilePath() == dirDest.absoluteFilePath())
    {
        m_pA->setText(i18n("A (Dest): "));
        bHideDest = true;
    }
    else
        m_pA->setText(!dirC.isValid() ? i18n("A:    ") : i18n("A (Base): "));

    m_pInfoA->setText(dirA.prettyAbsPath());

    if(dirB.absoluteFilePath() == dirDest.absoluteFilePath())
    {
        m_pB->setText(i18n("B (Dest): "));
        bHideDest = true;
    }
    else
        m_pB->setText(i18n("B:    "));
    m_pInfoB->setText(dirB.prettyAbsPath());

    if(dirC.absoluteFilePath() == dirDest.absoluteFilePath())
    {
        m_pC->setText(i18n("C (Dest): "));
        bHideDest = true;
    }
    else
        m_pC->setText(i18n("C:    "));
    m_pInfoC->setText(dirC.prettyAbsPath());

    m_pDest->setText(i18n("Dest: "));
    m_pInfoDest->setText(dirDest.prettyAbsPath());

    if(!dirC.isValid())
    {
        m_pC->hide();
        m_pInfoC->hide();
    }
    else
    {
        m_pC->show();
        m_pInfoC->show();
    }

    if(!dirDest.isValid() || bHideDest)
    {
        m_pDest->hide();
        m_pInfoDest->hide();
    }
    else
    {
        m_pDest->show();
        m_pInfoDest->show();
    }

    m_pInfoList->clear();
    addListViewItem(i18n("A"), dirA.prettyAbsPath(), mfi.getFileInfoA());
    addListViewItem(i18n("B"), dirB.prettyAbsPath(), mfi.getFileInfoB());
    addListViewItem(i18n("C"), dirC.prettyAbsPath(), mfi.getFileInfoC());
    if(!bHideDest)
    {
        FileAccess fiDest(dirDest.prettyAbsPath() + '/' + mfi.subPath(), true);
        addListViewItem(i18n("Dest"), dirDest.prettyAbsPath(), &fiDest);
    }
    for(int i = 0; i < m_pInfoList->columnCount(); ++i)
        m_pInfoList->resizeColumnToContents(i);
}

QTextStream& operator<<(QTextStream& ts, MergeFileInfos& mfi)
{
    ts << "{\n";
    ValueMap vm;
    vm.writeEntry("SubPath", mfi.subPath());
    vm.writeEntry("ExistsInA", mfi.existsInA());
    vm.writeEntry("ExistsInB", mfi.existsInB());
    vm.writeEntry("ExistsInC", mfi.existsInC());
    vm.writeEntry("EqualAB", mfi.m_bEqualAB);
    vm.writeEntry("EqualAC", mfi.m_bEqualAC);
    vm.writeEntry("EqualBC", mfi.m_bEqualBC);

    vm.writeEntry("MergeOperation", (int)mfi.getOperation());
    vm.writeEntry("DirA", mfi.isDirA());
    vm.writeEntry("DirB", mfi.isDirB());
    vm.writeEntry("DirC", mfi.isDirC());
    vm.writeEntry("LinkA", mfi.isLinkA());
    vm.writeEntry("LinkB", mfi.isLinkB());
    vm.writeEntry("LinkC", mfi.isLinkC());
    vm.writeEntry("OperationComplete", mfi.m_bOperationComplete);

    vm.writeEntry("AgeA", (int)mfi.getAgeA());
    vm.writeEntry("AgeB", (int)mfi.getAgeB());
    vm.writeEntry("AgeC", (int)mfi.getAgeC());
    vm.writeEntry("ConflictingAges", mfi.m_bConflictingAges); // Equal age but files are not!

    vm.save(ts);

    ts << "}\n";

    return ts;
}

void DirectoryMergeWindow::slotSaveMergeState()
{
    //slotStatusMsg(i18n("Saving Directory Merge State ..."));

    QString dirMergeStateFilename = QFileDialog::getSaveFileName(this, i18n("Save Directory Merge State As..."), QDir::currentPath());
    if(!dirMergeStateFilename.isEmpty())
    {
        QFile file(dirMergeStateFilename);
        bool bSuccess = file.open(QIODevice::WriteOnly);
        if(bSuccess)
        {
            QTextStream ts(&file);

            QModelIndex mi(d->index(0, 0, QModelIndex()));
            while(mi.isValid())
            {
                MergeFileInfos* pMFI = d->getMFI(mi);
                ts << *pMFI;
                mi = d->treeIterator(mi, true, true);
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
    bool bShowOnlyInA = d->m_pDirShowFilesOnlyInA->isChecked();
    bool bShowOnlyInB = d->m_pDirShowFilesOnlyInB->isChecked();
    bool bShowOnlyInC = d->m_pDirShowFilesOnlyInC->isChecked();
    bool bThreeDirs = d->isThreeWay();
    d->m_selection1Index = QModelIndex();
    d->m_selection2Index = QModelIndex();
    d->m_selection3Index = QModelIndex();

    // in first run set all dirs to equal and determine if they are not equal.
    // on second run don't change the equal-status anymore; it is needed to
    // set the visibility (when bShowIdentical is false).
    for(int loop = 0; loop < 2; ++loop)
    {
        QModelIndex mi = d->rowCount() > 0 ? d->index(0, 0, QModelIndex()) : QModelIndex();
        while(mi.isValid())
        {
            MergeFileInfos* pMFI = d->getMFI(mi);
            bool bDir = pMFI->isDirA() || pMFI->isDirB() || pMFI->isDirC();
            if(loop == 0 && bDir)
            {
                bool bChange = false;
                if(!pMFI->m_bEqualAB && pMFI->isDirA() == pMFI->isDirB() && pMFI->isLinkA() == pMFI->isLinkB())
                {
                    pMFI->m_bEqualAB = true;
                    bChange = true;
                }
                if(!pMFI->m_bEqualBC && pMFI->isDirC() == pMFI->isDirB() && pMFI->isLinkC() == pMFI->isLinkB())
                {
                    pMFI->m_bEqualBC = true;
                    bChange = true;
                }
                if(!pMFI->m_bEqualAC && pMFI->isDirA() == pMFI->isDirC() && pMFI->isLinkA() == pMFI->isLinkC())
                {
                    pMFI->m_bEqualAC = true;
                    bChange = true;
                }

                if(bChange)
                    DirectoryMergeWindow::DirectoryMergeWindowPrivate::setPixmaps(*pMFI, bThreeDirs);
            }
            bool bExistsEverywhere = pMFI->existsInA() && pMFI->existsInB() && (pMFI->existsInC() || !bThreeDirs);
            int existCount = int(pMFI->existsInA()) + int(pMFI->existsInB()) + int(pMFI->existsInC());
            bool bVisible =
                (bShowIdentical && bExistsEverywhere && pMFI->m_bEqualAB && (pMFI->m_bEqualAC || !bThreeDirs)) || ((bShowDifferent || bDir) && existCount >= 2 && (!pMFI->m_bEqualAB || !(pMFI->m_bEqualAC || !bThreeDirs))) || (bShowOnlyInA && pMFI->existsInA() && !pMFI->existsInB() && !pMFI->existsInC()) || (bShowOnlyInB && !pMFI->existsInA() && pMFI->existsInB() && !pMFI->existsInC()) || (bShowOnlyInC && !pMFI->existsInA() && !pMFI->existsInB() && pMFI->existsInC());

            QString fileName = pMFI->fileName();
            bVisible = bVisible && ((bDir && !Utils::wildcardMultiMatch(d->m_pOptions->m_DmDirAntiPattern, fileName, d->m_bCaseSensitive)) || (Utils::wildcardMultiMatch(d->m_pOptions->m_DmFilePattern, fileName, d->m_bCaseSensitive) && !Utils::wildcardMultiMatch(d->m_pOptions->m_DmFileAntiPattern, fileName, d->m_bCaseSensitive)));

            setRowHidden(mi.row(), mi.parent(), !bVisible);

            bool bEqual = bThreeDirs ? pMFI->m_bEqualAB && pMFI->m_bEqualAC : pMFI->m_bEqualAB;
            if(!bEqual && bVisible && loop == 0) // Set all parents to "not equal"
            {
                MergeFileInfos* p2 = pMFI->parent();
                while(p2 != nullptr)
                {
                    bool bChange = false;
                    if(!pMFI->m_bEqualAB && p2->m_bEqualAB)
                    {
                        p2->m_bEqualAB = false;
                        bChange = true;
                    }
                    if(!pMFI->m_bEqualAC && p2->m_bEqualAC)
                    {
                        p2->m_bEqualAC = false;
                        bChange = true;
                    }
                    if(!pMFI->m_bEqualBC && p2->m_bEqualBC)
                    {
                        p2->m_bEqualBC = false;
                        bChange = true;
                    }

                    if(bChange)
                        DirectoryMergeWindow::DirectoryMergeWindowPrivate::setPixmaps(*p2, bThreeDirs);
                    else
                        break;

                    p2 = p2->parent();
                }
            }
            mi = d->treeIterator(mi, true, true);
        }
    }
}

void DirectoryMergeWindow::slotShowIdenticalFiles()
{
    d->m_pOptions->m_bDmShowIdenticalFiles = d->m_pDirShowIdenticalFiles->isChecked();
    updateFileVisibilities();
}
void DirectoryMergeWindow::slotShowDifferentFiles()
{
    updateFileVisibilities();
}
void DirectoryMergeWindow::slotShowFilesOnlyInA()
{
    updateFileVisibilities();
}
void DirectoryMergeWindow::slotShowFilesOnlyInB()
{
    updateFileVisibilities();
}
void DirectoryMergeWindow::slotShowFilesOnlyInC()
{
    updateFileVisibilities();
}

void DirectoryMergeWindow::slotSynchronizeDirectories() {}
void DirectoryMergeWindow::slotChooseNewerFiles() {}

void DirectoryMergeWindow::initDirectoryMergeActions(KDiff3App* pKDiff3App, KActionCollection* ac)
{
#include "xpm/showequalfiles.xpm"
#include "xpm/showfilesonlyina.xpm"
#include "xpm/showfilesonlyinb.xpm"
#include "xpm/showfilesonlyinc.xpm"
#include "xpm/startmerge.xpm"

    d->m_pDirStartOperation = GuiUtils::createAction<QAction>(i18n("Start/Continue Directory Merge"), QKeySequence(Qt::Key_F7), this, &DirectoryMergeWindow::slotRunOperationForAllItems, ac, "dir_start_operation");
    d->m_pDirRunOperationForCurrentItem = GuiUtils::createAction<QAction>(i18n("Run Operation for Current Item"), QKeySequence(Qt::Key_F6), this, &DirectoryMergeWindow::slotRunOperationForCurrentItem, ac, "dir_run_operation_for_current_item");
    d->m_pDirCompareCurrent = GuiUtils::createAction<QAction>(i18n("Compare Selected File"), this, &DirectoryMergeWindow::compareCurrentFile, ac, "dir_compare_current");
    d->m_pDirMergeCurrent = GuiUtils::createAction<QAction>(i18n("Merge Current File"), QIcon(QPixmap(startmerge)), i18n("Merge\nFile"), pKDiff3App, &KDiff3App::slotMergeCurrentFile, ac, "merge_current");
    d->m_pDirFoldAll = GuiUtils::createAction<QAction>(i18n("Fold All Subdirs"), this, &DirectoryMergeWindow::collapseAll, ac, "dir_fold_all");
    d->m_pDirUnfoldAll = GuiUtils::createAction<QAction>(i18n("Unfold All Subdirs"), this, &DirectoryMergeWindow::expandAll, ac, "dir_unfold_all");
    d->m_pDirRescan = GuiUtils::createAction<QAction>(i18n("Rescan"), QKeySequence(Qt::SHIFT + Qt::Key_F5), this, &DirectoryMergeWindow::reload, ac, "dir_rescan");
    d->m_pDirSaveMergeState = nullptr; //GuiUtils::createAction< QAction >(i18n("Save Directory Merge State ..."), 0, this, &DirectoryMergeWindow::slotSaveMergeState, ac, "dir_save_merge_state");
    d->m_pDirLoadMergeState = nullptr; //GuiUtils::createAction< QAction >(i18n("Load Directory Merge State ..."), 0, this, &DirectoryMergeWindow::slotLoadMergeState, ac, "dir_load_merge_state");
    d->m_pDirChooseAEverywhere = GuiUtils::createAction<QAction>(i18n("Choose A for All Items"), this, &DirectoryMergeWindow::slotChooseAEverywhere, ac, "dir_choose_a_everywhere");
    d->m_pDirChooseBEverywhere = GuiUtils::createAction<QAction>(i18n("Choose B for All Items"), this, &DirectoryMergeWindow::slotChooseBEverywhere, ac, "dir_choose_b_everywhere");
    d->m_pDirChooseCEverywhere = GuiUtils::createAction<QAction>(i18n("Choose C for All Items"), this, &DirectoryMergeWindow::slotChooseCEverywhere, ac, "dir_choose_c_everywhere");
    d->m_pDirAutoChoiceEverywhere = GuiUtils::createAction<QAction>(i18n("Auto-Choose Operation for All Items"), this, &DirectoryMergeWindow::slotAutoChooseEverywhere, ac, "dir_autochoose_everywhere");
    d->m_pDirDoNothingEverywhere = GuiUtils::createAction<QAction>(i18n("No Operation for All Items"), this, &DirectoryMergeWindow::slotNoOpEverywhere, ac, "dir_nothing_everywhere");

    //   d->m_pDirSynchronizeDirectories = GuiUtils::createAction< KToggleAction >(i18n("Synchronize Directories"), 0, this, &DirectoryMergeWindow::slotSynchronizeDirectories, ac, "dir_synchronize_directories");
    //   d->m_pDirChooseNewerFiles = GuiUtils::createAction< KToggleAction >(i18n("Copy Newer Files Instead of Merging"), 0, this, &DirectoryMergeWindow::slotChooseNewerFiles, ac, "dir_choose_newer_files");

    d->m_pDirShowIdenticalFiles = GuiUtils::createAction<KToggleAction>(i18n("Show Identical Files"), QIcon(QPixmap(showequalfiles)), i18n("Identical\nFiles"), this, &DirectoryMergeWindow::slotShowIdenticalFiles, ac, "dir_show_identical_files");
    d->m_pDirShowDifferentFiles = GuiUtils::createAction<KToggleAction>(i18n("Show Different Files"), this, &DirectoryMergeWindow::slotShowDifferentFiles, ac, "dir_show_different_files");
    d->m_pDirShowFilesOnlyInA = GuiUtils::createAction<KToggleAction>(i18n("Show Files only in A"), QIcon(QPixmap(showfilesonlyina)), i18n("Files\nonly in A"), this, &DirectoryMergeWindow::slotShowFilesOnlyInA, ac, "dir_show_files_only_in_a");
    d->m_pDirShowFilesOnlyInB = GuiUtils::createAction<KToggleAction>(i18n("Show Files only in B"), QIcon(QPixmap(showfilesonlyinb)), i18n("Files\nonly in B"), this, &DirectoryMergeWindow::slotShowFilesOnlyInB, ac, "dir_show_files_only_in_b");
    d->m_pDirShowFilesOnlyInC = GuiUtils::createAction<KToggleAction>(i18n("Show Files only in C"), QIcon(QPixmap(showfilesonlyinc)), i18n("Files\nonly in C"), this, &DirectoryMergeWindow::slotShowFilesOnlyInC, ac, "dir_show_files_only_in_c");

    d->m_pDirShowIdenticalFiles->setChecked(d->m_pOptions->m_bDmShowIdenticalFiles);

    d->m_pDirCompareExplicit = GuiUtils::createAction<QAction>(i18n("Compare Explicitly Selected Files"), this, &DirectoryMergeWindow::slotCompareExplicitlySelectedFiles, ac, "dir_compare_explicitly_selected_files");
    d->m_pDirMergeExplicit = GuiUtils::createAction<QAction>(i18n("Merge Explicitly Selected Files"), this, &DirectoryMergeWindow::slotMergeExplicitlySelectedFiles, ac, "dir_merge_explicitly_selected_files");

    d->m_pDirCurrentDoNothing = GuiUtils::createAction<QAction>(i18n("Do Nothing"), this, &DirectoryMergeWindow::slotCurrentDoNothing, ac, "dir_current_do_nothing");
    d->m_pDirCurrentChooseA = GuiUtils::createAction<QAction>(i18n("A"), this, &DirectoryMergeWindow::slotCurrentChooseA, ac, "dir_current_choose_a");
    d->m_pDirCurrentChooseB = GuiUtils::createAction<QAction>(i18n("B"), this, &DirectoryMergeWindow::slotCurrentChooseB, ac, "dir_current_choose_b");
    d->m_pDirCurrentChooseC = GuiUtils::createAction<QAction>(i18n("C"), this, &DirectoryMergeWindow::slotCurrentChooseC, ac, "dir_current_choose_c");
    d->m_pDirCurrentMerge = GuiUtils::createAction<QAction>(i18n("Merge"), this, &DirectoryMergeWindow::slotCurrentMerge, ac, "dir_current_merge");
    d->m_pDirCurrentDelete = GuiUtils::createAction<QAction>(i18n("Delete (if exists)"), this, &DirectoryMergeWindow::slotCurrentDelete, ac, "dir_current_delete");

    d->m_pDirCurrentSyncDoNothing = GuiUtils::createAction<QAction>(i18n("Do Nothing"), this, &DirectoryMergeWindow::slotCurrentDoNothing, ac, "dir_current_sync_do_nothing");
    d->m_pDirCurrentSyncCopyAToB = GuiUtils::createAction<QAction>(i18n("Copy A to B"), this, &DirectoryMergeWindow::slotCurrentCopyAToB, ac, "dir_current_sync_copy_a_to_b");
    d->m_pDirCurrentSyncCopyBToA = GuiUtils::createAction<QAction>(i18n("Copy B to A"), this, &DirectoryMergeWindow::slotCurrentCopyBToA, ac, "dir_current_sync_copy_b_to_a");
    d->m_pDirCurrentSyncDeleteA = GuiUtils::createAction<QAction>(i18n("Delete A"), this, &DirectoryMergeWindow::slotCurrentDeleteA, ac, "dir_current_sync_delete_a");
    d->m_pDirCurrentSyncDeleteB = GuiUtils::createAction<QAction>(i18n("Delete B"), this, &DirectoryMergeWindow::slotCurrentDeleteB, ac, "dir_current_sync_delete_b");
    d->m_pDirCurrentSyncDeleteAAndB = GuiUtils::createAction<QAction>(i18n("Delete A && B"), this, &DirectoryMergeWindow::slotCurrentDeleteAAndB, ac, "dir_current_sync_delete_a_and_b");
    d->m_pDirCurrentSyncMergeToA = GuiUtils::createAction<QAction>(i18n("Merge to A"), this, &DirectoryMergeWindow::slotCurrentMergeToA, ac, "dir_current_sync_merge_to_a");
    d->m_pDirCurrentSyncMergeToB = GuiUtils::createAction<QAction>(i18n("Merge to B"), this, &DirectoryMergeWindow::slotCurrentMergeToB, ac, "dir_current_sync_merge_to_b");
    d->m_pDirCurrentSyncMergeToAAndB = GuiUtils::createAction<QAction>(i18n("Merge to A && B"), this, &DirectoryMergeWindow::slotCurrentMergeToAAndB, ac, "dir_current_sync_merge_to_a_and_b");
}

void DirectoryMergeWindow::updateAvailabilities(bool bDirCompare, bool bDiffWindowVisible,
                                                KToggleAction* chooseA, KToggleAction* chooseB, KToggleAction* chooseC)
{
    d->m_pDirStartOperation->setEnabled(bDirCompare);
    d->m_pDirRunOperationForCurrentItem->setEnabled(bDirCompare);
    d->m_pDirFoldAll->setEnabled(bDirCompare);
    d->m_pDirUnfoldAll->setEnabled(bDirCompare);

    d->m_pDirCompareCurrent->setEnabled(bDirCompare && isVisible() && isFileSelected());

    d->m_pDirMergeCurrent->setEnabled((bDirCompare && isVisible() && isFileSelected()) || bDiffWindowVisible);

    d->m_pDirRescan->setEnabled(bDirCompare);

    bool bThreeDirs = d->isThreeWay();
    d->m_pDirAutoChoiceEverywhere->setEnabled(bDirCompare && isVisible());
    d->m_pDirDoNothingEverywhere->setEnabled(bDirCompare && isVisible());
    d->m_pDirChooseAEverywhere->setEnabled(bDirCompare && isVisible());
    d->m_pDirChooseBEverywhere->setEnabled(bDirCompare && isVisible());
    d->m_pDirChooseCEverywhere->setEnabled(bDirCompare && isVisible() && bThreeDirs);


    MergeFileInfos* pMFI = d->getMFI(currentIndex());

    bool bItemActive = bDirCompare && isVisible() && pMFI != nullptr; //  &&  hasFocus();
    bool bMergeMode = bThreeDirs || !d->m_bSyncMode;
    bool bFTConflict = pMFI == nullptr ? false : pMFI->conflictingFileTypes();

    bool bDirWindowHasFocus = isVisible() && hasFocus();

    d->m_pDirShowIdenticalFiles->setEnabled(bDirCompare && isVisible());
    d->m_pDirShowDifferentFiles->setEnabled(bDirCompare && isVisible());
    d->m_pDirShowFilesOnlyInA->setEnabled(bDirCompare && isVisible());
    d->m_pDirShowFilesOnlyInB->setEnabled(bDirCompare && isVisible());
    d->m_pDirShowFilesOnlyInC->setEnabled(bDirCompare && isVisible() && bThreeDirs);

    d->m_pDirCompareExplicit->setEnabled(bDirCompare && isVisible() && d->m_selection2Index.isValid());
    d->m_pDirMergeExplicit->setEnabled(bDirCompare && isVisible() && d->m_selection2Index.isValid());

    d->m_pDirCurrentDoNothing->setEnabled(bItemActive && bMergeMode);
    d->m_pDirCurrentChooseA->setEnabled(bItemActive && bMergeMode && pMFI->existsInA());
    d->m_pDirCurrentChooseB->setEnabled(bItemActive && bMergeMode && pMFI->existsInB());
    d->m_pDirCurrentChooseC->setEnabled(bItemActive && bMergeMode && pMFI->existsInC());
    d->m_pDirCurrentMerge->setEnabled(bItemActive && bMergeMode && !bFTConflict);
    d->m_pDirCurrentDelete->setEnabled(bItemActive && bMergeMode);
    if(bDirWindowHasFocus)
    {
        chooseA->setEnabled(bItemActive && pMFI->existsInA());
        chooseB->setEnabled(bItemActive && pMFI->existsInB());
        chooseC->setEnabled(bItemActive && pMFI->existsInC());
        chooseA->setChecked(false);
        chooseB->setChecked(false);
        chooseC->setChecked(false);
    }

    d->m_pDirCurrentSyncDoNothing->setEnabled(bItemActive && !bMergeMode);
    d->m_pDirCurrentSyncCopyAToB->setEnabled(bItemActive && !bMergeMode && pMFI->existsInA());
    d->m_pDirCurrentSyncCopyBToA->setEnabled(bItemActive && !bMergeMode && pMFI->existsInB());
    d->m_pDirCurrentSyncDeleteA->setEnabled(bItemActive && !bMergeMode && pMFI->existsInA());
    d->m_pDirCurrentSyncDeleteB->setEnabled(bItemActive && !bMergeMode && pMFI->existsInB());
    d->m_pDirCurrentSyncDeleteAAndB->setEnabled(bItemActive && !bMergeMode && pMFI->existsInA() && pMFI->existsInB());
    d->m_pDirCurrentSyncMergeToA->setEnabled(bItemActive && !bMergeMode && !bFTConflict);
    d->m_pDirCurrentSyncMergeToB->setEnabled(bItemActive && !bMergeMode && !bFTConflict);
    d->m_pDirCurrentSyncMergeToAAndB->setEnabled(bItemActive && !bMergeMode && !bFTConflict);
}

//#include "directorymergewindow.moc"
