/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "directorymergewindow.h"

#include "CompositeIgnoreList.h"
#include "defmac.h"
#include "DirectoryInfo.h"
#include "guiutils.h"
#include "kdiff3.h"
#include "Logging.h"
#include "MergeFileInfos.h"
#include "options.h"
#include "PixMapUtils.h"
#include "progress.h"
#include "TypeUtils.h"
#include "Utils.h"

#include <map>
#include <memory>

#include <QAction>
#include <QApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QMenu>
#include <QPainter>
#include <QSplitter>
#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QTextStream>

#include <KLocalizedString>
#include <KMessageBox>
#include <KToggleAction>

struct DirectoryMergeWindow::ItemInfo {
    bool bExpanded;
    bool bOperationComplete;
    QString status;
    e_MergeOperation eMergeOperation;
};

class StatusInfo: public QDialog
{
  private:
    QTextEdit* m_pTextEdit;

  public:
    explicit StatusInfo(QWidget* pParent):
        QDialog(pParent)
    {
        QVBoxLayout* pVLayout = new QVBoxLayout(this);
        m_pTextEdit = new QTextEdit(this);
        pVLayout->addWidget(m_pTextEdit);
        setObjectName("StatusInfo");
        setWindowFlags(Qt::Dialog);
        m_pTextEdit->setWordWrapMode(QTextOption::NoWrap);
        m_pTextEdit->setReadOnly(true);
        QDialogButtonBox* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
        chk_connect(box, &QDialogButtonBox::rejected, this, &QDialog::accept);
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
    s_UnsolvedCol = 6, // Number of unsolved conflicts (for 3 input files)
    s_SolvedCol = 7,   // Number of auto-solvable conflicts (for 3 input files)
    s_NonWhiteCol = 8, // Number of nonwhite deltas (for 2 input files)
    s_WhiteCol = 9     // Number of white deltas (for 2 input files)
};

static Qt::CaseSensitivity s_eCaseSensitivity = Qt::CaseSensitive;

class DirectoryMergeWindow::DirectoryMergeWindowPrivate: public QAbstractItemModel
{
    friend class DirMergeItem;

  public:
    DirectoryMergeWindowPrivate(DirectoryMergeWindow* pDMW, KDiff3App& app):
        m_app(app)
    {
        mWindow = pDMW;
        m_pStatusInfo = new StatusInfo(mWindow);
        m_pStatusInfo->hide();
    }
    ~DirectoryMergeWindowPrivate() override
    {
        delete m_pRoot;
    }

    bool init(bool bDirectoryMerge, bool bReload);

    // Implement QAbstractItemModel
    [[nodiscard]] QVariant data(const QModelIndex& index, qint32 role = Qt::DisplayRole) const override;

    //Qt::ItemFlags flags ( const QModelIndex & index ) const
    [[nodiscard]] QModelIndex parent(const QModelIndex& index) const override
    {
        MergeFileInfos* pMFI = getMFI(index);
        if(pMFI == nullptr || pMFI == m_pRoot || pMFI->parent() == m_pRoot)
            return QModelIndex();

        MergeFileInfos* pParentsParent = pMFI->parent()->parent();
        return createIndex(pParentsParent->children().indexOf(pMFI->parent()), 0, pMFI->parent());
    }

    [[nodiscard]] qint32 rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        MergeFileInfos* pParentMFI = getMFI(parent);
        if(pParentMFI != nullptr)
            return pParentMFI->children().count();
        else
            return m_pRoot->children().count();
    }

    [[nodiscard]] qint32 columnCount(const QModelIndex& /*parent*/) const override
    {
        return 10;
    }

    [[nodiscard]] QModelIndex index(qint32 row, qint32 column, const QModelIndex& parent) const override
    {
        MergeFileInfos* pParentMFI = getMFI(parent);
        if(pParentMFI == nullptr && row < m_pRoot->children().count())
            return createIndex(row, column, m_pRoot->children()[row]);
        else if(pParentMFI != nullptr && row < pParentMFI->children().count())
            return createIndex(row, column, pParentMFI->children()[row]);
        else
            return QModelIndex();
    }

    [[nodiscard]] QVariant headerData(qint32 section, Qt::Orientation orientation, qint32 role = Qt::DisplayRole) const override;

    void sort(qint32 column, Qt::SortOrder order) override;

    void selectItemAndColumn(const QModelIndex& mi, bool bContextMenu);

    void setOpStatus(const QModelIndex& mi, e_OperationStatus eOpStatus)
    {
        if(MergeFileInfos* pMFI = getMFI(mi))
        {
            pMFI->setOpStatus(eOpStatus);
            Q_EMIT dataChanged(mi, mi);
        }
    }

    QModelIndex nextSibling(const QModelIndex& mi);

    // private data and helper methods
    [[nodiscard]] MergeFileInfos* getMFI(const QModelIndex& mi) const
    {
        if(mi.isValid())
            return (MergeFileInfos*)mi.internalPointer();
        else
            return nullptr;
    }

    /*
        returns whether or not we doing three way directory comparision

        This will return false when comparing three files.
        Use KDiff3App::isTripleDiff() if this is a problem.
    */
    [[nodiscard]] bool isDirThreeWay() const
    {
        if(rootMFI() == nullptr) return false;
        return rootMFI()->isThreeWay();
    }

    [[nodiscard]] MergeFileInfos* rootMFI() const { return m_pRoot; }

    void calcDirStatus(bool bThreeDirs, const QModelIndex& mi,
                       qint32& nofFiles, qint32& nofDirs, qint32& nofEqualFiles, qint32& nofManualMerges);

    void mergeContinue(bool bStart, bool bVerbose);

    void prepareListView();
    void calcSuggestedOperation(const QModelIndex& mi, e_MergeOperation eDefaultMergeOp);
    void setAllMergeOperations(e_MergeOperation eDefaultOperation);

    bool canContinue();
    QModelIndex treeIterator(QModelIndex mi, bool bVisitChildren = true, bool bFindInvisible = false);
    void prepareMergeStart(const QModelIndex& miBegin, const QModelIndex& miEnd, bool bVerbose);
    bool executeMergeOperation(const MergeFileInfos& mfi, bool& bSingleFileMerge);

    void scanDirectory(const QString& dirName, DirectoryList& dirList);
    void scanLocalDirectory(const QString& dirName, DirectoryList& dirList);

    void setMergeOperation(const QModelIndex& mi, e_MergeOperation eMergeOp, bool bRecursive = true);
    [[nodiscard]] bool isDir(const QModelIndex& mi) const;
    [[nodiscard]] QString getFileName(const QModelIndex& mi) const;

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
        explicit FileKey(const FileAccess& fa):
            m_pFA(&fa) {}

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
        // qint32 r = filePath().compare( fa.filePath() )
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
                qint32 r = v1[v1Size - i - 1]->fileName().compare(v2[v2Size - i - 1]->fileName(), s_eCaseSensitivity);
                if(r < 0)
                    return true;
                else if(r > 0)
                    return false;
            }

            return v1Size < v2Size;
        }
    };

    typedef QMap<FileKey, MergeFileInfos> t_fileMergeMap;

    MergeFileInfos* m_pRoot = new MergeFileInfos();

    t_fileMergeMap m_fileMergeMap;

  public:
    DirectoryMergeWindow* mWindow;
    QSharedPointer<Options> m_pOptions = nullptr;
    KDiff3App& m_app;

    bool m_bFollowDirLinks = false;
    bool m_bFollowFileLinks = false;
    bool m_bSimulatedMergeStarted = false;
    bool m_bRealMergeStarted = false;
    bool m_bError = false;
    bool m_bSyncMode = false;
    bool m_bDirectoryMerge = false; // if true, then merge is the default operation, otherwise it's diff.
    bool m_bCaseSensitive = true;
    bool m_bUnfoldSubdirs = false;
    bool m_bSkipDirStatus = false;
    bool m_bScanning = false; // true while in init()

    DirectoryMergeInfo* m_pDirectoryMergeInfo = nullptr;
    StatusInfo* m_pStatusInfo = nullptr;

    typedef std::list<QModelIndex> MergeItemList; // linked list
    MergeItemList m_mergeItemList;
    MergeItemList::iterator m_currentIndexForOperation;

    QModelIndex m_selection1Index;
    QModelIndex m_selection2Index;
    QModelIndex m_selection3Index;

    QPointer<QAction> m_pDirStartOperation;
    QPointer<QAction> m_pDirRunOperationForCurrentItem;
    QPointer<QAction> m_pDirCompareCurrent;
    QPointer<QAction> m_pDirMergeCurrent;
    QPointer<QAction> m_pDirRescan;
    QPointer<QAction> m_pDirChooseAEverywhere;
    QPointer<QAction> m_pDirChooseBEverywhere;
    QPointer<QAction> m_pDirChooseCEverywhere;
    QPointer<QAction> m_pDirAutoChoiceEverywhere;
    QPointer<QAction> m_pDirDoNothingEverywhere;
    QPointer<QAction> m_pDirFoldAll;
    QPointer<QAction> m_pDirUnfoldAll;

    KToggleAction* m_pDirShowIdenticalFiles;
    KToggleAction* m_pDirShowDifferentFiles;
    KToggleAction* m_pDirShowFilesOnlyInA;
    KToggleAction* m_pDirShowFilesOnlyInB;
    KToggleAction* m_pDirShowFilesOnlyInC;

    KToggleAction* m_pDirSynchronizeDirectories;
    KToggleAction* m_pDirChooseNewerFiles;

    QPointer<QAction> m_pDirCompareExplicit;
    QPointer<QAction> m_pDirMergeExplicit;

    QPointer<QAction> m_pDirCurrentDoNothing;
    QPointer<QAction> m_pDirCurrentChooseA;
    QPointer<QAction> m_pDirCurrentChooseB;
    QPointer<QAction> m_pDirCurrentChooseC;
    QPointer<QAction> m_pDirCurrentMerge;
    QPointer<QAction> m_pDirCurrentDelete;

    QPointer<QAction> m_pDirCurrentSyncDoNothing;
    QPointer<QAction> m_pDirCurrentSyncCopyAToB;
    QPointer<QAction> m_pDirCurrentSyncCopyBToA;
    QPointer<QAction> m_pDirCurrentSyncDeleteA;
    QPointer<QAction> m_pDirCurrentSyncDeleteB;
    QPointer<QAction> m_pDirCurrentSyncDeleteAAndB;
    QPointer<QAction> m_pDirCurrentSyncMergeToA;
    QPointer<QAction> m_pDirCurrentSyncMergeToB;
    QPointer<QAction> m_pDirCurrentSyncMergeToAAndB;

    QPointer<QAction> m_pDirSaveMergeState;
    QPointer<QAction> m_pDirLoadMergeState;
};

QVariant DirectoryMergeWindow::DirectoryMergeWindowPrivate::data(const QModelIndex& index, qint32 role) const
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
                bool bDir = pMFI->hasDir();
                switch(pMFI->getOperation())
                {
                    case eNoOperation:
                        return "";
                        break;
                    case eCopyAToB:
                        return i18nc("Operation column message", "Copy A to B");
                        break;
                    case eCopyBToA:
                        return i18nc("Operation column message", "Copy B to A");
                        break;
                    case eDeleteA:
                        return i18nc("Operation column message", "Delete A");
                        break;
                    case eDeleteB:
                        return i18nc("Operation column message", "Delete B");
                        break;
                    case eDeleteAB:
                        return i18nc("Operation column message", "Delete A & B");
                        break;
                    case eMergeToA:
                        return i18nc("Operation column message", "Merge to A");
                        break;
                    case eMergeToB:
                        return i18nc("Operation column message", "Merge to B");
                        break;
                    case eMergeToAB:
                        return i18nc("Operation column message", "Merge to A & B");
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
                        return i18nc("Operation column message", "Delete (if exists)");
                        break;
                    case eMergeABCToDest:
                    case eMergeABToDest:
                        return bDir ? i18nc("Operation column message (Directory merge)", "Merge") : i18nc("Operation column message (File merge)", "Merge (manual)");
                        break;
                    case eConflictingFileTypes:
                        return i18nc("Operation column message", "Error: Conflicting File Types");
                        break;
                    case eChangedAndDeleted:
                        return i18nc("Operation column message", "Error: Changed and Deleted");
                        break;
                    case eConflictingAges:
                        return i18nc("Operation column message", "Error: Dates are equal but files are not.");
                        break;
                    default:
                        assert(false);
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
                        return i18nc("Status column message", "Done");
                    case eOpStatusError:
                        return i18nc("Status column message", "Error");
                    case eOpStatusSkipped:
                        return i18nc("Status column message", "Skipped.");
                    case eOpStatusNotSaved:
                        return i18nc("Status column message", "Not saved.");
                    case eOpStatusInProgress:
                        return i18nc("Status column message", "In progress...");
                    case eOpStatusToDo:
                        return i18nc("Status column message", "To do.");
                }
            }
        }
        else if(role == Qt::DecorationRole)
        {
            if(s_NameCol == index.column())
            {
                return PixMapUtils::getOnePixmap(eAgeEnd, pMFI->hasLink(), pMFI->hasDir());
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

QVariant DirectoryMergeWindow::DirectoryMergeWindowPrivate::headerData(qint32 section, Qt::Orientation orientation, qint32 role) const
{
    if(orientation == Qt::Horizontal && section >= 0 && section < columnCount(QModelIndex()) && role == Qt::DisplayRole)
    {
        switch(section)
        {
            case s_NameCol:
                return i18nc("Column title", "Name");
            case s_ACol:
                return i18n("A");
            case s_BCol:
                return i18n("B");
            case s_CCol:
                return i18n("C");
            case s_OpCol:
                return i18nc("Column title", "Operation");
            case s_OpStatusCol:
                return i18nc("Column title", "Status");
            case s_UnsolvedCol:
                return i18nc("Column title", "Unsolved");
            case s_SolvedCol:
                return i18nc("Column title", "Solved");
            case s_NonWhiteCol:
                return i18nc("Column title", "Nonwhite");
            case s_WhiteCol:
                return i18nc("Column title", "White");
            default:
                return QVariant();
        }
    }
    return QVariant();
}

qint32 DirectoryMergeWindow::getIntFromIndex(const QModelIndex& index) const
{
    return index == d->m_selection1Index ? 1 : index == d->m_selection2Index ? 2 : index == d->m_selection3Index ? 3 : 0;
}

const QSharedPointer<Options>& DirectoryMergeWindow::getOptions() const
{
    return d->m_pOptions;
}

// Previously  Q3ListViewItem::paintCell(p,cg,column,width,align);
class DirectoryMergeWindow::DirMergeItemDelegate: public QStyledItemDelegate
{
  private:
    DirectoryMergeWindow* m_pDMW;
    [[nodiscard]] const QSharedPointer<Options>& getOptions() const { return m_pDMW->getOptions(); }

  public:
    explicit DirMergeItemDelegate(DirectoryMergeWindow* pParent):
        QStyledItemDelegate(pParent), m_pDMW(pParent)
    {
    }

    void paint(QPainter* thePainter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        qint32 column = index.column();
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

            qint32 x = option.rect.left();
            qint32 y = option.rect.top();
            //QPixmap icon = value.value<QPixmap>(); //pixmap(column);
            if(!icon.isNull())
            {
                const auto dpr = thePainter->device()->devicePixelRatioF();
                const qint32 w = qRound(icon.width() / dpr);
                const qint32 h = qRound(icon.height() / dpr);
                qint32 yOffset = (sizeHint(option, index).height() - h) / 2;
                thePainter->drawPixmap(x + 2, y + yOffset, icon);

                qint32 i = m_pDMW->getIntFromIndex(index);
                if(i != 0)
                {
                    QColor c(i == 1 ? getOptions()->aColor() : i == 2 ? getOptions()->bColor() :
                                                                        getOptions()->cColor());
                    thePainter->setPen(c); // highlight() );
                    thePainter->drawRect(x + 2, y + yOffset, w, h);
                    thePainter->setPen(QPen(c, 0, Qt::DotLine));
                    thePainter->drawRect(x + 1, y + yOffset - 1, w + 2, h + 2);
                    thePainter->setPen(Qt::white);
                    QString s(QChar('A' + i - 1));

                    thePainter->drawText(x + 2 + (w - Utils::getHorizontalAdvance(thePainter->fontMetrics(), s)) / 2,
                                         y + yOffset + (h + thePainter->fontMetrics().ascent()) / 2 - 1,
                                         s);
                }
                else
                {
                    thePainter->setPen(m_pDMW->palette().window().color());
                    thePainter->drawRect(x + 1, y + yOffset - 1, w + 2, h + 2);
                }
                return;
            }
        }

        QStyleOptionViewItem option2 = option;
        if(column >= s_UnsolvedCol)
        {
            option2.displayAlignment = Qt::AlignRight;
        }
        QStyledItemDelegate::paint(thePainter, option2, index);
    }
    [[nodiscard]] QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override
    {
        QSize sz = QStyledItemDelegate::sizeHint(option, index);
        return sz.expandedTo(QSize(0, 18));
    }
};

DirectoryMergeWindow::DirectoryMergeWindow(QWidget* pParent, const QSharedPointer<Options>& pOptions, KDiff3App& app):
    QTreeView(pParent)
{
    d = std::make_unique<DirectoryMergeWindowPrivate>(this, app);
    setModel(d.get());
    setItemDelegate(new DirMergeItemDelegate(this));
    chk_connect(this, &DirectoryMergeWindow::doubleClicked, this, &DirectoryMergeWindow::onDoubleClick);
    chk_connect(this, &DirectoryMergeWindow::expanded, this, &DirectoryMergeWindow::onExpanded);

    d->m_pOptions = pOptions;

    setSortingEnabled(true);
}

DirectoryMergeWindow::~DirectoryMergeWindow() = default;

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

qint32 DirectoryMergeWindow::totalColumnWidth()
{
    qint32 w = 0;
    for(qint32 i = 0; i < s_OpStatusCol; ++i)
    {
        w += columnWidth(i);
    }
    return w;
}

void DirectoryMergeWindow::reload()
{
    if(isDirectoryMergeInProgress())
    {
        qint32 result = KMessageBox::warningYesNo(this,
                                               i18n("You are currently doing a folder merge. Are you sure, you want to abort the merge and rescan the folder?"),
                                               i18nc("Error dialog caption", "Warning"),
                                               KGuiItem(i18nc("Title for rescan button", "Rescan")),
                                               KGuiItem(i18nc("Title for continue button", "Continue Merging")));
        if(result != KMessageBox::Yes)
            return;
    }

    init(true);
    //fix file visibilities after reload or menu will be out of sync with display if changed from defaults.
    updateFileVisibilities();
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::calcDirStatus(bool bThreeDirs, const QModelIndex& mi,
                                                                      qint32& nofFiles, qint32& nofDirs, qint32& nofEqualFiles, qint32& nofManualMerges)
{
    const MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI->hasDir())
    {
        ++nofDirs;
    }
    else
    {
        ++nofFiles;
        if(pMFI->isEqualAB() && (!bThreeDirs || pMFI->isEqualAC()))
        {
            ++nofEqualFiles;
        }
        else
        {
            if(pMFI->getOperation() == eMergeABCToDest || pMFI->getOperation() == eMergeABToDest)
                ++nofManualMerges;
        }
    }
    for(qint32 childIdx = 0; childIdx < rowCount(mi); ++childIdx)
        calcDirStatus(bThreeDirs, index(childIdx, 0, mi), nofFiles, nofDirs, nofEqualFiles, nofManualMerges);
}

bool DirectoryMergeWindow::init(
    bool bDirectoryMerge,
    bool bReload)
{
    return d->init(bDirectoryMerge, bReload);
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::buildMergeMap(const QSharedPointer<DirectoryInfo>& dirInfo)
{
    if(dirInfo->dirA().isValid())
    {
        for(FileAccess& fileRecord: dirInfo->getDirListA())
        {
            MergeFileInfos& mfi = m_fileMergeMap[FileKey(fileRecord)];

            mfi.setFileInfoA(&fileRecord);
        }
    }

    if(dirInfo->dirB().isValid())
    {
        for(FileAccess& fileRecord: dirInfo->getDirListB())
        {
            MergeFileInfos& mfi = m_fileMergeMap[FileKey(fileRecord)];

            mfi.setFileInfoB(&(fileRecord));
        }
    }

    if(dirInfo->dirC().isValid())
    {
        for(FileAccess& fileRecord: dirInfo->getDirListC())
        {
            MergeFileInfos& mfi = m_fileMergeMap[FileKey(fileRecord)];

            mfi.setFileInfoC(&(fileRecord));
        }
    }
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::init(
    bool bDirectoryMerge,
    bool bReload)
{
    if(m_pOptions->m_bDmFullAnalysis)
    {
        QStringList errors;
        // A full analysis uses the same resources that a normal text-diff/merge uses.
        // So make sure that the user saves his data first.
        if(!m_app.canContinue())
            return false;
        Q_EMIT mWindow->startDiffMerge(errors, "", "", "", "", "", "", "", nullptr); // hide main window
    }

    mWindow->show();
    mWindow->setUpdatesEnabled(true);

    std::map<QString, ItemInfo> expandedDirsMap;

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

    ProgressScope pp;
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

    assert(gDirInfo != nullptr);
    const FileAccess& dirA = gDirInfo->dirA();
    const FileAccess& dirB = gDirInfo->dirB();
    const FileAccess& dirC = gDirInfo->dirC();
    const FileAccess& dirDest = gDirInfo->destDir();
    // Check if all input directories exist and are valid. The dest dir is not tested now.
    // The test will happen only when we are going to write to it.
    if(!dirA.isDir() || !dirB.isDir() ||
       (dirC.isValid() && !dirC.isDir()))
    {
        QString text(i18n("Opening of folders failed:"));
        text += "\n\n";
        if(!dirA.isDir())
        {
            text += i18n("Folder A \"%1\" does not exist or is not a folder.\n", dirA.prettyAbsPath());
        }

        if(!dirB.isDir())
        {
            text += i18n("Folder B \"%1\" does not exist or is not a folder.\n", dirB.prettyAbsPath());
        }

        if(dirC.isValid() && !dirC.isDir())
        {
            text += i18n("Folder C \"%1\" does not exist or is not a folder.\n", dirC.prettyAbsPath());
        }

        KMessageBox::error(mWindow, text, i18n("Folder Opening Error"));
        return false;
    }

    if(dirC.isValid() &&
       (dirDest.prettyAbsPath() == dirA.prettyAbsPath() || dirDest.prettyAbsPath() == dirB.prettyAbsPath()))
    {
        KMessageBox::error(mWindow,
                           i18n("The destination folder must not be the same as A or B when "
                                "three folders are merged.\nCheck again before continuing."),
                           i18n("Parameter Warning"));
        return false;
    }

    m_bScanning = true;
    Q_EMIT mWindow->statusBarMessage(i18n("Scanning folders..."));

    m_bSyncMode = m_pOptions->m_bDmSyncMode && gDirInfo->allowSyncMode();

    m_fileMergeMap.clear();
    s_eCaseSensitivity = m_bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive;
    // calc how many directories will be read:
    double nofScans = (dirA.isValid() ? 1 : 0) + (dirB.isValid() ? 1 : 0) + (dirC.isValid() ? 1 : 0);
    qint32 currentScan = 0;

    //TODO   setColumnWidthMode(s_UnsolvedCol, Q3ListView::Manual);
    //   setColumnWidthMode(s_SolvedCol,   Q3ListView::Manual);
    //   setColumnWidthMode(s_WhiteCol,    Q3ListView::Manual);
    //   setColumnWidthMode(s_NonWhiteCol, Q3ListView::Manual);
    mWindow->setColumnHidden(s_CCol, !dirC.isValid());
    mWindow->setColumnHidden(s_WhiteCol, !m_pOptions->m_bDmFullAnalysis);
    mWindow->setColumnHidden(s_NonWhiteCol, !m_pOptions->m_bDmFullAnalysis);
    mWindow->setColumnHidden(s_UnsolvedCol, !m_pOptions->m_bDmFullAnalysis);
    mWindow->setColumnHidden(s_SolvedCol, !(m_pOptions->m_bDmFullAnalysis && dirC.isValid()));

    bool bListDirSuccessA = true;
    bool bListDirSuccessB = true;
    bool bListDirSuccessC = true;

    if(dirA.isValid())
    {
        ProgressProxy::setInformation(i18n("Reading Folder A"));
        ProgressProxy::setSubRangeTransformation(currentScan / nofScans, (currentScan + 1) / nofScans);
        ++currentScan;

        bListDirSuccessA = gDirInfo->listDirA(m_pOptions);
    }

    if(dirB.isValid())
    {
        ProgressProxy::setInformation(i18n("Reading Folder B"));
        ProgressProxy::setSubRangeTransformation(currentScan / nofScans, (currentScan + 1) / nofScans);
        ++currentScan;

        bListDirSuccessB = gDirInfo->listDirB(m_pOptions);
    }

    e_MergeOperation eDefaultMergeOp;
    if(dirC.isValid())
    {
        ProgressProxy::setInformation(i18n("Reading Folder C"));
        ProgressProxy::setSubRangeTransformation(currentScan / nofScans, (currentScan + 1) / nofScans);
        ++currentScan;

        bListDirSuccessC = gDirInfo->listDirC(m_pOptions);

        eDefaultMergeOp = eMergeABCToDest;
    }
    else
        eDefaultMergeOp = m_bSyncMode ? eMergeToAB : eMergeABToDest;

    buildMergeMap(gDirInfo);

    bool bContinue = true;
    if(!bListDirSuccessA || !bListDirSuccessB || !bListDirSuccessC)
    {
        QString s = i18n("Some subfolders were not readable in");
        if(!bListDirSuccessA) s += "\nA: " + dirA.prettyAbsPath();
        if(!bListDirSuccessB) s += "\nB: " + dirB.prettyAbsPath();
        if(!bListDirSuccessC) s += "\nC: " + dirC.prettyAbsPath();
        s += '\n';
        s += i18n("Check the permissions of the subfolders.");
        bContinue = KMessageBox::Continue == KMessageBox::warningContinueCancel(mWindow, s);
    }

    if(bContinue)
    {
        prepareListView();

        mWindow->updateFileVisibilities();

        for(qint32 childIdx = 0; childIdx < rowCount(); ++childIdx)
        {
            QModelIndex mi = index(childIdx, 0, QModelIndex());
            calcSuggestedOperation(mi, eDefaultMergeOp);
        }
    }

    mWindow->sortByColumn(0, Qt::AscendingOrder);

    for(qint32 column = 0; column < columnCount(QModelIndex()); ++column)
        mWindow->resizeColumnToContents(column);

    m_bScanning = false;
    Q_EMIT mWindow->statusBarMessage(i18n("Ready."));

    if(bContinue && !m_bSkipDirStatus)
    {
        // Generate a status report
        qint32 nofFiles = 0;
        qint32 nofDirs = 0;
        qint32 nofEqualFiles = 0;
        qint32 nofManualMerges = 0;
        //TODO
        for(qint32 childIdx = 0; childIdx < rowCount(); ++childIdx)
            calcDirStatus(dirC.isValid(), index(childIdx, 0, QModelIndex()),
                          nofFiles, nofDirs, nofEqualFiles, nofManualMerges);

        QString s;
        s = i18n("Folder Comparison Status\n\n"
                 "Number of subfolders: %1\n"
                 "Number of equal files: %2\n"
                 "Number of different files: %3",
                 nofDirs, nofEqualFiles, nofFiles - nofEqualFiles);

        if(dirC.isValid())
            s += '\n' + i18n("Number of manual merges: %1", nofManualMerges);
        KMessageBox::information(mWindow, s);
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
    return gDirInfo->dirA().prettyAbsPath();
}
inline QString DirectoryMergeWindow::getDirNameB() const
{
    return gDirInfo->dirB().prettyAbsPath();
}
inline QString DirectoryMergeWindow::getDirNameC() const
{
    return gDirInfo->dirC().prettyAbsPath();
}
inline QString DirectoryMergeWindow::getDirNameDest() const
{
    return gDirInfo->destDir().prettyAbsPath();
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
    e_MergeOperation eDefaultMergeOp = d->isDirThreeWay() ? eMergeABCToDest : d->m_bSyncMode ? eMergeToAB :
                                                                                               eMergeABToDest;
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
    bool bThreeDirs = d->isDirThreeWay();
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
    if((e->modifiers() & Qt::ControlModifier) != 0)
    {
        MergeFileInfos* pMFI = d->getMFI(currentIndex());
        if(pMFI == nullptr)
            return;

        bool bThreeDirs = pMFI->isThreeWay();
        bool bMergeMode = bThreeDirs || !d->m_bSyncMode;
        bool bFTConflict = pMFI->conflictingFileTypes();

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
            case Qt::Key_Space:
                slotCurrentDoNothing();
                return;
        }

        if(bMergeMode)
        {
            switch(e->key())
            {
                case Qt::Key_3:
                    if(pMFI->existsInC())
                    {
                        slotCurrentChooseC();
                    }
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
    //Override Qt's default behavior for this key.
    else if(e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
    {
        onDoubleClick(currentIndex());
        return;
    }

    QTreeView::keyPressEvent(e);
}

void DirectoryMergeWindow::focusInEvent(QFocusEvent*)
{
    Q_EMIT updateAvailabilities();
}
void DirectoryMergeWindow::focusOutEvent(QFocusEvent*)
{
    Q_EMIT updateAvailabilities();
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::setAllMergeOperations(e_MergeOperation eDefaultOperation)
{
    if(KMessageBox::Yes == KMessageBox::warningYesNo(mWindow,
                                                     i18n("This affects all merge operations."),
                                                     i18n("Changing All Merge Operations"),
                                                     KStandardGuiItem::cont(),
                                                     KStandardGuiItem::cancel()))
    {
        for(qint32 i = 0; i < rowCount(); ++i)
        {
            calcSuggestedOperation(index(i, 0, QModelIndex()), eDefaultOperation);
        }
    }
}

QModelIndex DirectoryMergeWindow::DirectoryMergeWindowPrivate::nextSibling(const QModelIndex& mi)
{
    QModelIndex miParent = mi.parent();
    qint32 currentIdx = mi.row();
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
        } while(mi.isValid() && mWindow->isRowHidden(mi.row(), mi.parent()) && !bFindInvisible);
    }
    return mi;
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::prepareListView()
{
    QStringList errors;
    //TODO   clear();
    PixMapUtils::initPixmaps(m_pOptions->m_newestFileColor, m_pOptions->m_oldestFileColor,
                             m_pOptions->m_midAgeFileColor, m_pOptions->m_missingFileColor);

    mWindow->setRootIsDecorated(true);

    qint32 nrOfFiles = m_fileMergeMap.size();
    qint32 currentIdx = 1;
    QElapsedTimer t;
    t.start();
    ProgressProxy::setMaxNofSteps(nrOfFiles);

    for(MergeFileInfos& mfi: m_fileMergeMap)
    {
        const QString& fileName = mfi.subPath();

        ProgressProxy::setInformation(
            i18n("Processing %1 / %2\n%3", currentIdx, nrOfFiles, fileName), currentIdx, false);
        if(ProgressProxy::wasCancelled()) break;
        ++currentIdx;

        // The comparisons and calculations for each file take place here.
        if(!mfi.compareFilesAndCalcAges(errors, m_pOptions, mWindow) && errors.size() >= 30)
            break;

        // Get dirname from fileName: Search for "/" from end:
        qint32 pos = fileName.lastIndexOf('/');
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
            m_pRoot->addChild(&mfi); // new DirMergeItem( this, filePart, &mfi );
            mfi.setParent(m_pRoot);
        }
        else
        {
            const FileAccess* pFA = mfi.getFileInfoA() ? mfi.getFileInfoA() : mfi.getFileInfoB() ? mfi.getFileInfoB() :
                                                                                                   mfi.getFileInfoC();
            MergeFileInfos& dirMfi = pFA->parent() ? m_fileMergeMap[FileKey(*pFA->parent())] : *m_pRoot; // parent

            dirMfi.addChild(&mfi); // new DirMergeItem( dirMfi.m_pDMI, filePart, &mfi );
            mfi.setParent(&dirMfi);
            //   // Equality for parent dirs is set in updateFileVisibilities()
        }

        mfi.updateAge();
    }

    if(errors.size() > 0)
    {
        if(errors.size() < 15)
        {
            KMessageBox::errorList(mWindow, i18n("Some files could not be processed."), errors);
        }
        else if(errors.size() < 30)
        {
            KMessageBox::error(mWindow, i18n("Some files could not be processed."));
        }
        else
            KMessageBox::error(mWindow, i18n("Aborting due to too many errors."));
    }

    beginResetModel();
    endResetModel();
}

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::calcSuggestedOperation(const QModelIndex& mi, e_MergeOperation eDefaultMergeOp)
{
    const MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI == nullptr)
        return;

    bool bCheckC = pMFI->isThreeWay();
    bool bCopyNewer = m_pOptions->m_bDmCopyNewer;
    bool bOtherDest = !((gDirInfo->destDir().absoluteFilePath() == gDirInfo->dirA().absoluteFilePath()) ||
                        (gDirInfo->destDir().absoluteFilePath() == gDirInfo->dirB().absoluteFilePath()) ||
                        (bCheckC && gDirInfo->destDir().absoluteFilePath() == gDirInfo->dirC().absoluteFilePath()));

    //Crash and burn in debug mode these states are never valid.
    //The checks are duplicated here so they show in the assert text.
    assert(!(eDefaultMergeOp == eMergeABCToDest && !bCheckC));
    assert(!(eDefaultMergeOp == eMergeToAB && bCheckC));

    //Check for two bugged states that are recoverable. This should never happen!
    if(Q_UNLIKELY(eDefaultMergeOp == eMergeABCToDest && !bCheckC))
    {
        qCWarning(kdiffMain) << "Invalid State detected in DirectoryMergeWindow::DirectoryMergeWindowPrivate::calcSuggestedOperation";
        eDefaultMergeOp = eMergeABToDest;
    }
    if(Q_UNLIKELY(eDefaultMergeOp == eMergeToAB && bCheckC))
    {
        qCWarning(kdiffMain) << "Invalid State detected in DirectoryMergeWindow::DirectoryMergeWindowPrivate::calcSuggestedOperation";
        eDefaultMergeOp = eMergeABCToDest;
    }

    if(eDefaultMergeOp == eMergeToA || eDefaultMergeOp == eMergeToB ||
       eDefaultMergeOp == eMergeABCToDest || eDefaultMergeOp == eMergeABToDest || eDefaultMergeOp == eMergeToAB)
    {
        if(!bCheckC)
        {
            if(pMFI->isEqualAB())
            {
                setMergeOperation(mi, bOtherDest ? eCopyBToDest : eNoOperation);
            }
            else if(pMFI->existsInA() && pMFI->existsInB())
            {
                //TODO: verify conditions here
                if(!bCopyNewer || pMFI->isDirA())
                    setMergeOperation(mi, eDefaultMergeOp);
                else if(bCopyNewer && pMFI->conflictingAges())
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
            if(pMFI->isEqualAB() && pMFI->isEqualAC())
            {
                setMergeOperation(mi, bOtherDest ? eCopyCToDest : eNoOperation);
            }
            else if(pMFI->existsInA() && pMFI->existsInB() && pMFI->existsInC())
            {
                if(pMFI->isEqualAB() || pMFI->isEqualBC())
                    setMergeOperation(mi, eCopyCToDest);
                else if(pMFI->isEqualAC())
                    setMergeOperation(mi, eCopyBToDest);
                else
                    setMergeOperation(mi, eMergeABCToDest);
            }
            else if(pMFI->existsInA() && pMFI->existsInB() && !pMFI->existsInC())
            {
                if(pMFI->isEqualAB())
                    setMergeOperation(mi, eDeleteFromDest);
                else
                    setMergeOperation(mi, eChangedAndDeleted);
            }
            else if(pMFI->existsInA() && !pMFI->existsInB() && pMFI->existsInC())
            {
                if(pMFI->isEqualAC())
                    setMergeOperation(mi, eDeleteFromDest);
                else
                    setMergeOperation(mi, eChangedAndDeleted);
            }
            else if(!pMFI->existsInA() && pMFI->existsInB() && pMFI->existsInC())
            {
                if(pMFI->isEqualBC())
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
                assert(false);
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

    d->m_pDirectoryMergeInfo->setInfo(gDirInfo->dirA(), gDirInfo->dirB(), gDirInfo->dirC(), gDirInfo->destDir(), *pMFI);
}

void DirectoryMergeWindow::mousePressEvent(QMouseEvent* e)
{
    QTreeView::mousePressEvent(e);
    QModelIndex mi = indexAt(e->pos());
    qint32 c = mi.column();
    QPoint p = e->globalPos();
    MergeFileInfos* pMFI = d->getMFI(mi);
    if(pMFI == nullptr)
        return;

    if(c == s_OpCol)
    {
        bool bThreeDirs = d->isDirThreeWay();

        QMenu m(this);
        if(bThreeDirs)
        {
            m.addAction(d->m_pDirCurrentDoNothing);
            qint32 count = 0;
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
    qint32 c = mi.column();

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

QString DirectoryMergeWindow::DirectoryMergeWindowPrivate::getFileName(const QModelIndex& mi) const
{
    MergeFileInfos* pMFI = getMFI(mi);
    if(pMFI != nullptr)
    {
        return mi.column() == s_ACol ? pMFI->getFileInfoA()->absoluteFilePath() : mi.column() == s_BCol ? pMFI->getFileInfoB()->absoluteFilePath() : mi.column() == s_CCol ? pMFI->getFileInfoC()->absoluteFilePath() : QString("");
    }
    return QString();
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::isDir(const QModelIndex& mi) const
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
    if(old1.isValid()) Q_EMIT dataChanged(old1, old1);
    if(old2.isValid()) Q_EMIT dataChanged(old2, old2);
    if(old3.isValid()) Q_EMIT dataChanged(old3, old3);
    if(m_selection1Index.isValid()) Q_EMIT dataChanged(m_selection1Index, m_selection1Index);
    if(m_selection2Index.isValid()) Q_EMIT dataChanged(m_selection2Index, m_selection2Index);
    if(m_selection3Index.isValid()) Q_EMIT dataChanged(m_selection3Index, m_selection3Index);
    Q_EMIT mWindow->updateAvailabilities();
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

void DirectoryMergeWindow::DirectoryMergeWindowPrivate::sort(qint32 column, Qt::SortOrder order)
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
        pMFI->startOperation();
        setOpStatus(mi, eOpStatusNone);
    }

    pMFI->setOperation(eMergeOp);
    if(bRecursive)
    {
        e_MergeOperation eChildrenMergeOp = pMFI->getOperation();
        if(eChildrenMergeOp == eConflictingFileTypes)
            eChildrenMergeOp = isDirThreeWay() ? eMergeABCToDest : eMergeABToDest;

        for(qint32 childIdx = 0; childIdx < pMFI->children().count(); ++childIdx)
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
        KMessageBox::error(this, i18n("This operation is currently not possible."), i18n("Operation Not Possible"));
        return;
    }
    QStringList errors;
    if(MergeFileInfos* pMFI = d->getMFI(currentIndex()))
    {
        if(!(pMFI->hasDir()))
        {
            Q_EMIT startDiffMerge(errors,
                                  pMFI->existsInA() ? pMFI->getFileInfoA()->absoluteFilePath() : QString(""),
                                  pMFI->existsInB() ? pMFI->getFileInfoB()->absoluteFilePath() : QString(""),
                                  pMFI->existsInC() ? pMFI->getFileInfoC()->absoluteFilePath() : QString(""),
                                  "",
                                  "", "", "", nullptr);
        }
    }
    Q_EMIT updateAvailabilities();
}

void DirectoryMergeWindow::slotCompareExplicitlySelectedFiles()
{
    if(!d->isDir(d->m_selection1Index) && !d->canContinue()) return;

    if(d->m_bRealMergeStarted)
    {
        KMessageBox::error(this, i18n("This operation is currently not possible."), i18n("Operation Not Possible"));
        return;
    }

    QStringList errors;
    Q_EMIT startDiffMerge(errors,
                          d->getFileName(d->m_selection1Index),
                          d->getFileName(d->m_selection2Index),
                          d->getFileName(d->m_selection3Index),
                          "",
                          "", "", "", nullptr);
    d->m_selection1Index = QModelIndex();
    d->m_selection2Index = QModelIndex();
    d->m_selection3Index = QModelIndex();

    Q_EMIT updateAvailabilities();
    update();
}

void DirectoryMergeWindow::slotMergeExplicitlySelectedFiles()
{
    if(!d->isDir(d->m_selection1Index) && !d->canContinue()) return;

    if(d->m_bRealMergeStarted)
    {
        KMessageBox::error(this, i18n("This operation is currently not possible."), i18n("Operation Not Possible"));
        return;
    }
    QStringList errors;
    QString fn1 = d->getFileName(d->m_selection1Index);
    QString fn2 = d->getFileName(d->m_selection2Index);
    QString fn3 = d->getFileName(d->m_selection3Index);

    Q_EMIT startDiffMerge(errors, fn1, fn2, fn3,
                          fn3.isEmpty() ? fn2 : fn3,
                          "", "", "", nullptr);
    d->m_selection1Index = QModelIndex();
    d->m_selection2Index = QModelIndex();
    d->m_selection3Index = QModelIndex();

    Q_EMIT updateAvailabilities();
    update();
}

bool DirectoryMergeWindow::isFileSelected()
{
    if(MergeFileInfos* pMFI = d->getMFI(currentIndex()))
    {
        return !(pMFI->hasDir() || pMFI->conflictingFileTypes());
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
        pMFI->endOperation();
        if(d->m_mergeItemList.size() == 1)
        {
            d->m_mergeItemList.clear();
            d->m_bRealMergeStarted = false;
        }
    }

    Q_EMIT updateAvailabilities();
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::canContinue()
{
    if(m_app.canContinue() && !m_bError)
    {
        QModelIndex mi = (m_mergeItemList.empty() || m_currentIndexForOperation == m_mergeItemList.end()) ? QModelIndex() : *m_currentIndexForOperation;
        MergeFileInfos* pMFI = getMFI(mi);
        if(pMFI && pMFI->isOperationRunning())
        {
            setOpStatus(mi, eOpStatusNotSaved);
            pMFI->endOperation();
            if(m_mergeItemList.size() == 1)
            {
                m_mergeItemList.clear();
                m_bRealMergeStarted = false;
            }
        }

        return true;
    }
    return false;
}

bool DirectoryMergeWindow::DirectoryMergeWindowPrivate::executeMergeOperation(const MergeFileInfos& mfi, bool& bSingleFileMerge)
{
    bool bCreateBackups = m_pOptions->m_bDmCreateBakFiles;
    // First decide destname
    QString destName;
    switch(mfi.getOperation())
    {
        case eNoOperation:
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
            /*
                Do not replace with code that ignores gDirInfo->destDir().
                Any such patch will be rejected. KDiff3 intentionally supports custom destination directories.
            */
            destName = mfi.fullNameDest();
            break;
        default:
            KMessageBox::error(mWindow, i18n("Unknown merge operation. (This must never happen!)"));
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
            KMessageBox::error(mWindow, i18n("Unknown merge operation."));
    }

    return bSuccess;
}

// Check if the merge can start, and prepare the m_mergeItemList which then contains all
// items that must be merged.
void DirectoryMergeWindow::DirectoryMergeWindowPrivate::prepareMergeStart(const QModelIndex& miBegin, const QModelIndex& miEnd, bool bVerbose)
{
    if(bVerbose)
    {
        qint32 status = KMessageBox::warningYesNoCancel(mWindow,
                                                     i18n("The merge is about to begin.\n\n"
                                                          "Choose \"Do it\" if you have read the instructions and know what you are doing.\n"
                                                          "Choosing \"Simulate it\" will tell you what would happen.\n\n"
                                                          "Be aware that this program still has beta status "
                                                          "and there is NO WARRANTY whatsoever! Make backups of your vital data!"),
                                                     i18nc("Caption", "Starting Merge"),
                                                     KGuiItem(i18nc("Button title to confirm merge", "Do It")),
                                                     KGuiItem(i18nc("Button title to simulate merge", "Simulate It")));
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
        if(pMFI && pMFI->isOperationRunning())
        {
            m_mergeItemList.push_back(mi);
            QString errorText;
            if(pMFI->getOperation() == eConflictingFileTypes)
            {
                errorText = i18n("The highlighted item has a different type in the different folders. Select what to do.");
            }
            if(pMFI->getOperation() == eConflictingAges)
            {
                errorText = i18n("The modification dates of the file are equal but the files are not. Select what to do.");
            }
            if(pMFI->getOperation() == eChangedAndDeleted)
            {
                errorText = i18n("The highlighted item was changed in one folder and deleted in the other. Select what to do.");
            }
            if(!errorText.isEmpty())
            {
                mWindow->scrollTo(mi, QAbstractItemView::EnsureVisible);
                mWindow->setCurrentIndex(mi);
                KMessageBox::error(mWindow, errorText);
                m_mergeItemList.clear();
                m_bRealMergeStarted = false;
                return;
            }
        }
    }

    m_currentIndexForOperation = m_mergeItemList.begin();
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
        KMessageBox::error(this, i18n("This operation is currently not possible because folder merge is currently running."), i18n("Operation Not Possible"));
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
    Q_EMIT updateAvailabilities();
}

// When bStart is true then m_currentIndexForOperation must still be processed.
// When bVerbose is true then a messagebox will tell when the merge is complete.
void DirectoryMergeWindow::DirectoryMergeWindowPrivate::mergeContinue(bool bStart, bool bVerbose)
{
    ProgressScope pp;
    if(m_mergeItemList.empty())
        return;

    qint32 nrOfItems = 0;
    qint32 nrOfCompletedItems = 0;
    qint32 nrOfCompletedSimItems = 0;

    // Count the number of completed items (for the progress bar).
    for(const QModelIndex& i: m_mergeItemList)
    {
        MergeFileInfos* pMFI = getMFI(i);
        ++nrOfItems;
        if(!pMFI->isOperationRunning())
            ++nrOfCompletedItems;
        if(!pMFI->isSimOpRunning())
            ++nrOfCompletedSimItems;
    }

    m_pStatusInfo->hide();
    m_pStatusInfo->clear();

    QModelIndex miCurrent = m_currentIndexForOperation == m_mergeItemList.end() ? QModelIndex() : *m_currentIndexForOperation;

    bool bContinueWithCurrentItem = bStart; // true for first item, else false
    bool bSkipItem = false;
    if(!bStart && m_bError && miCurrent.isValid())
    {
        qint32 status = KMessageBox::warningYesNoCancel(mWindow,
                                                     i18n("There was an error in the last step.\n"
                                                          "Do you want to continue with the item that caused the error or do you want to skip this item?"),
                                                     i18nc("Caption for message dialog", "Continue merge after an error"),
                                                     KGuiItem(i18nc("Continue button title", "Continue With Last Item")),
                                                     KGuiItem(i18nc("Skip button title", "Skip Item")));
        if(status == KMessageBox::Yes)
            bContinueWithCurrentItem = true;
        else if(status == KMessageBox::No)
            bSkipItem = true;
        else
            return;
        m_bError = false;
    }

    ProgressProxy::setMaxNofSteps(nrOfItems);

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

        if(!bContinueWithCurrentItem)
        {
            if(bSim)
            {
                if(rowCount(miCurrent) == 0)
                {
                    pMFI->endSimOp();
                }
            }
            else
            {
                if(rowCount(miCurrent) == 0)
                {
                    if(pMFI->isOperationRunning())
                    {
                        setOpStatus(miCurrent, bSkipItem ? eOpStatusSkipped : eOpStatusDone);
                        pMFI->endOperation();
                        bSkipItem = false;
                    }
                }
                else
                {
                    setOpStatus(miCurrent, eOpStatusInProgress);
                }
            }

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
                    for(qint32 childIdx = 0; childIdx < rowCount(miParent); ++childIdx)
                    {
                        pMFI = getMFI(index(childIdx, 0, miParent));
                        if((!bSim && pMFI->isOperationRunning()) || (bSim && !pMFI->isSimOpRunning()))
                        {
                            bDone = false;
                            break;
                        }
                    }
                    if(bDone)
                    {
                        pMFI = getMFI(miParent);
                        if(bSim)
                            pMFI->endSimOp();
                        else
                        {
                            setOpStatus(miParent, eOpStatusDone);
                            pMFI->endOperation();
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
                    KMessageBox::information(mWindow, i18n("Merge operation complete."), i18n("Merge Complete"));
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
                    getMFI(mi)->startSimOp();
                }
                m_pStatusInfo->setWindowTitle(i18n("Simulated merge complete: Check if you agree with the proposed operations."));
                m_pStatusInfo->exec();
            }
            m_mergeItemList.clear();
            m_bRealMergeStarted = false;
            return;
        }

        pMFI = getMFI(miCurrent);

        ProgressProxy::setInformation(pMFI->subPath(),
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

        if(ProgressProxy::wasCancelled())
            break;
    } // end while

    //g_pProgressDialog->hide();

    mWindow->setCurrentIndex(miCurrent);
    mWindow->scrollTo(miCurrent, EnsureVisible);
    if(!bSuccess && !bSingleFileMerge)
    {
        KMessageBox::error(mWindow, i18n("An error occurred. Press OK to see detailed information."));
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
    Q_EMIT mWindow->updateAvailabilities();

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
            m_pStatusInfo->addText(i18n("delete folder recursively( %1 )", name));
        else
            m_pStatusInfo->addText(i18n("delete( %1 )", name));

        if(m_bSimulatedMergeStarted)
        {
            return true;
        }

        if(fi.isDir() && !fi.isSymLink()) // recursive directory delete only for real dirs, not symlinks
        {
            DirectoryList dirList;
            CompositeIgnoreList ignoreList;
            bool bSuccess = fi.listDir(&dirList, false, true, "*", "", "", false, ignoreList); // not recursive, find hidden files

            if(!bSuccess)
            {
                // No Permission to read directory or other error.
                m_pStatusInfo->addText(i18n("Error: delete folder operation failed while trying to read the folder."));
                return false;
            }

            for(const FileAccess& fi2: dirList) // for each file...
            {
                assert(fi2.fileName() != "." && fi2.fileName() != "..");

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

    QStringList errors;
    // Make sure that the dir exists, into which we will save the file later.
    qint32 pos = nameDest.lastIndexOf('/');
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
    mWindow->scrollTo(*m_currentIndexForOperation, EnsureVisible);

    Q_EMIT mWindow->startDiffMerge(errors, nameA, nameB, nameC, nameDest, "", "", "", nullptr);

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

        bSuccess = makeDir(destName);
        return bSuccess;
    }

    qint32 pos = destName.lastIndexOf('/');
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

    qint32 pos = name.lastIndexOf('/');
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
        m_pStatusInfo->addText(i18n("Error while creating folder."));
        return false;
    }
    return true;
}

DirectoryMergeInfo::DirectoryMergeInfo(QWidget* pParent):
    QFrame(pParent)
{
    QVBoxLayout* topLayout = new QVBoxLayout(this);
    topLayout->setContentsMargins(0, 0, 0, 0);

    QGridLayout* grid = new QGridLayout();
    topLayout->addLayout(grid);
    grid->setColumnStretch(1, 10);

    qint32 line = 0;

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
    m_pInfoList->setHeaderLabels({i18n("Folder"), i18n("Type"), i18n("Size"),
                                  i18n("Attr"), i18n("Last Modification"), i18n("Link-Destination")});
    setMinimumSize(100, 100);

    m_pInfoList->installEventFilter(this);
    m_pInfoList->setRootIsDecorated(false);
}

bool DirectoryMergeInfo::eventFilter(QObject* o, QEvent* e)
{
    if(e->type() == QEvent::FocusIn && o == m_pInfoList)
        Q_EMIT gotFocus();
    return false;
}

void DirectoryMergeInfo::addListViewItem(const QString& dir, const QString& basePath, FileAccess* fi)
{
    if(basePath.isEmpty())
    {
        return;
    }

    if(fi != nullptr && fi->exists())
    {
        QString dateString = fi->lastModified().toString(QLocale::system().dateTimeFormat());

        m_pInfoList->addTopLevelItem(new QTreeWidgetItem(
            m_pInfoList,
            {dir, QString(fi->isDir() ? i18n("Folder") : i18n("File")) + (fi->isSymLink() ? i18n("-Link") : ""), QString::number(fi->size()), QLatin1String(fi->isReadable() ? "r" : " ") + QLatin1String(fi->isWritable() ? "w" : " ") + QLatin1String((fi->isExecutable() ? "x" : " ")), dateString, QString(fi->isSymLink() ? (" -> " + fi->readLink()) : QString(""))}));
    }
    else
    {
        m_pInfoList->addTopLevelItem(new QTreeWidgetItem(
            m_pInfoList,
            {dir, i18n("not available"), "", "", "", ""}));
    }
}

void DirectoryMergeInfo::setInfo(
    const FileAccess& dirA,
    const FileAccess& dirB,
    const FileAccess& dirC,
    const FileAccess& dirDest,
    const MergeFileInfos& mfi)
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
    for(qint32 i = 0; i < m_pInfoList->columnCount(); ++i)
        m_pInfoList->resizeColumnToContents(i);
}

void DirectoryMergeWindow::slotSaveMergeState()
{
    //slotStatusMsg(i18n("Saving Directory Merge State ..."));

    QString dirMergeStateFilename = QFileDialog::getSaveFileName(this, i18n("Save Folder Merge State As..."), QDir::currentPath());
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
    bool bThreeDirs = d->isDirThreeWay();
    d->m_selection1Index = QModelIndex();
    d->m_selection2Index = QModelIndex();
    d->m_selection3Index = QModelIndex();

    // in first run set all dirs to equal and determine if they are not equal.
    // on second run don't change the equal-status anymore; it is needed to
    // set the visibility (when bShowIdentical is false).
    for(qint32 loop = 0; loop < 2; ++loop)
    {
        QModelIndex mi = d->rowCount() > 0 ? d->index(0, 0, QModelIndex()) : QModelIndex();
        while(mi.isValid())
        {
            MergeFileInfos* pMFI = d->getMFI(mi);
            bool bDir = pMFI->hasDir();
            if(loop == 0 && bDir)
            { //Treat all links and directories to equal by default.
                pMFI->updateDirectoryOrLink();
            }

            bool bVisible =
                (bShowIdentical && pMFI->existsEveryWhere() && pMFI->isEqualAB() && (pMFI->isEqualAC() || !bThreeDirs)) ||
                ((bShowDifferent || bDir) && pMFI->existsCount() >= 2 && (!pMFI->isEqualAB() || !(pMFI->isEqualAC() || !bThreeDirs))) ||
                (bShowOnlyInA && pMFI->onlyInA()) || (bShowOnlyInB && pMFI->onlyInB()) || (bShowOnlyInC && pMFI->onlyInC());

            QString fileName = pMFI->fileName();
            bVisible = bVisible && ((bDir && !Utils::wildcardMultiMatch(d->m_pOptions->m_DmDirAntiPattern, fileName, d->m_bCaseSensitive)) || (Utils::wildcardMultiMatch(d->m_pOptions->m_DmFilePattern, fileName, d->m_bCaseSensitive) && !Utils::wildcardMultiMatch(d->m_pOptions->m_DmFileAntiPattern, fileName, d->m_bCaseSensitive)));

            if(loop != 0)
                setRowHidden(mi.row(), mi.parent(), !bVisible);

            bool bEqual = bThreeDirs ? pMFI->isEqualAB() && pMFI->isEqualAC() : pMFI->isEqualAB();
            if(!bEqual && bVisible && loop == 0) // Set all parents to "not equal"
            {
                pMFI->updateParents();
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

    d->m_pDirStartOperation = GuiUtils::createAction<QAction>(i18n("Start/Continue Folder Merge"), QKeySequence(Qt::Key_F7), this, &DirectoryMergeWindow::slotRunOperationForAllItems, ac, "dir_start_operation");
    d->m_pDirRunOperationForCurrentItem = GuiUtils::createAction<QAction>(i18n("Run Operation for Current Item"), QKeySequence(Qt::Key_F6), this, &DirectoryMergeWindow::slotRunOperationForCurrentItem, ac, "dir_run_operation_for_current_item");
    d->m_pDirCompareCurrent = GuiUtils::createAction<QAction>(i18n("Compare Selected File"), this, &DirectoryMergeWindow::compareCurrentFile, ac, "dir_compare_current");
    d->m_pDirMergeCurrent = GuiUtils::createAction<QAction>(i18n("Merge Current File"), QIcon(QPixmap(startmerge)), i18n("Merge\nFile"), pKDiff3App, &KDiff3App::slotMergeCurrentFile, ac, "merge_current");
    d->m_pDirFoldAll = GuiUtils::createAction<QAction>(i18n("Fold All Subfolders"), this, &DirectoryMergeWindow::collapseAll, ac, "dir_fold_all");
    d->m_pDirUnfoldAll = GuiUtils::createAction<QAction>(i18n("Unfold All Subfolders"), this, &DirectoryMergeWindow::expandAll, ac, "dir_unfold_all");
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

void DirectoryMergeWindow::setupConnections(const KDiff3App* app)
{
    chk_connect(this, &DirectoryMergeWindow::startDiffMerge, app, &KDiff3App::slotFileOpen2);
    chk_connect(selectionModel(), &QItemSelectionModel::selectionChanged, app, &KDiff3App::slotUpdateAvailabilities);
    chk_connect(selectionModel(), &QItemSelectionModel::currentChanged, app, &KDiff3App::slotUpdateAvailabilities);
    chk_connect(this, static_cast<void (DirectoryMergeWindow::*)(void)>(&DirectoryMergeWindow::updateAvailabilities), app, &KDiff3App::slotUpdateAvailabilities);
    chk_connect(this, &DirectoryMergeWindow::statusBarMessage, app, &KDiff3App::slotStatusMsg);
    chk_connect(app, &KDiff3App::doRefresh, this, &DirectoryMergeWindow::slotRefresh);
}

void DirectoryMergeWindow::updateAvailabilities(bool bMergeEditorVisible, bool bDirCompare, bool bDiffWindowVisible,
                                                KToggleAction* chooseA, KToggleAction* chooseB, KToggleAction* chooseC)
{
    d->m_pDirStartOperation->setEnabled(bDirCompare);
    d->m_pDirRunOperationForCurrentItem->setEnabled(bDirCompare);
    d->m_pDirFoldAll->setEnabled(bDirCompare);
    d->m_pDirUnfoldAll->setEnabled(bDirCompare);

    d->m_pDirCompareCurrent->setEnabled(bDirCompare && isVisible() && isFileSelected());

    d->m_pDirMergeCurrent->setEnabled((bDirCompare && isVisible() && isFileSelected()) || bDiffWindowVisible);

    d->m_pDirRescan->setEnabled(bDirCompare);

    bool bThreeDirs = d->isDirThreeWay();
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
        chooseA->setEnabled(bItemActive && (bDirCompare ? pMFI->existsInA() : true));
        chooseB->setEnabled(bItemActive && (bDirCompare ? pMFI->existsInB() : true));
        chooseC->setEnabled(bItemActive && (bDirCompare ? pMFI->existsInC() : KDiff3App::isTripleDiff()));
        chooseA->setChecked(false);
        chooseB->setChecked(false);
        chooseC->setChecked(false);
    }
    else
    {
        chooseA->setEnabled(bMergeEditorVisible);
        chooseB->setEnabled(bMergeEditorVisible);
        chooseC->setEnabled(bMergeEditorVisible && KDiff3App::isTripleDiff());
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
