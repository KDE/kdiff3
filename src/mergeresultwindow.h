// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef MERGERESULTWINDOW_H
#define MERGERESULTWINDOW_H

#include "diff.h"
#include "FileNameLineEdit.h"
#include "MergeEditLine.h"
#include "options.h"
#include "Overview.h"
#include "selection.h"
#include "LineRef.h"
#include "TypeUtils.h"

#include <boost/signals2.hpp>
#include <memory>

#include <QLineEdit>
#include <QPointer>
#include <QSharedPointer>
#include <QStatusBar>
#include <QTextLayout>
#include <QTimer>
#include <QWidget>

class QPainter;
class RLPainter;
class QScrollBar;
class KActionCollection;
class KToggleAction;

class KDiff3App;
class UndoRecord;

class MergeResultWindow: public QWidget
{
    Q_OBJECT
  public:
    inline static QPointer<QScrollBar> mVScrollBar = nullptr;

    MergeResultWindow(QWidget* pParent, QStatusBar* pStatusBar);

    void init(
        const std::shared_ptr<LineDataVector> &pLineDataA, LineRef sizeA,
        const std::shared_ptr<LineDataVector> &pLineDataB, LineRef sizeB,
        const std::shared_ptr<LineDataVector> &pLineDataC, LineRef sizeC,
        const Diff3LineList* pDiff3LineList,
        TotalDiffStatus* pTotalDiffStatus,
        bool bAutoSolve
    );

    void setupConnections(const KDiff3App* app);

    void initActions(KActionCollection* ac);

    void reset();

    bool saveDocument(const QString& fileName, const char* encoding, e_LineEndStyle eLineEndStyle);
    [[nodiscard]] qint32 getNumberOfUnsolvedConflicts(qint32* pNrOfWhiteSpaceConflicts = nullptr) const;
    void choose(const e_SrcSelector selector);
    void chooseGlobal(e_SrcSelector selector, bool bConflictsOnly, bool bWhiteSpaceOnly);

    [[nodiscard]] qint32 getMaxTextWidth(); // width of longest text line
    [[nodiscard]] LineType getNofLines() const;
    [[nodiscard]] qint32 getVisibleTextAreaWidth() const; // text area width without the border
    [[nodiscard]] qint32 getNofVisibleLines() const;
    [[nodiscard]] QString getSelection() const;
    void resetSelection();
    void showNumberOfConflicts(bool showIfNone = false);
    [[nodiscard]] bool isDeltaAboveCurrent() const;
    [[nodiscard]] bool isDeltaBelowCurrent() const;
    [[nodiscard]] bool isConflictAboveCurrent() const;
    [[nodiscard]] bool isConflictBelowCurrent() const;
    [[nodiscard]] bool isUnsolvedConflictAtCurrent() const;
    [[nodiscard]] bool isUnsolvedConflictAboveCurrent() const;
    [[nodiscard]] bool isUnsolvedConflictBelowCurrent() const;
    bool findString(const QString& s, LineRef& d3vLine, qsizetype& posInLine, bool bDirDown, bool bCaseSensitive);
    void setSelection(LineType firstLine, qsizetype startPos, LineType lastLine, qsizetype endPos);
    [[nodiscard]] e_OverviewMode getOverviewMode() const;

    void slotUpdateAvailabilities();

  private:
    inline static QPointer<QAction> chooseAEverywhere;
    inline static QPointer<QAction> chooseBEverywhere;
    inline static QPointer<QAction> chooseCEverywhere;
    inline static QPointer<QAction> chooseAForUnsolvedConflicts;
    inline static QPointer<QAction> chooseBForUnsolvedConflicts;
    inline static QPointer<QAction> chooseCForUnsolvedConflicts;
    inline static QPointer<QAction> chooseAForUnsolvedWhiteSpaceConflicts;
    inline static QPointer<QAction> chooseBForUnsolvedWhiteSpaceConflicts;
    inline static QPointer<QAction> chooseCForUnsolvedWhiteSpaceConflicts;

    struct HistoryMapEntry {
        MergeEditLineList mellA;
        MergeEditLineList mellB;
        MergeEditLineList mellC;
        MergeEditLineList& choice(bool bThreeInputs);
        bool staysInPlace(bool bThreeInputs, Diff3LineList::const_iterator& iHistoryEnd);
    };
    typedef std::map<QString, HistoryMapEntry> HistoryMap;

    enum class Direction
    {
        eUp,
        eDown
    };

    enum class EndPoint
    {
        eDelta,
        eConflict,
        eUnsolvedConflict,
        eLine,
        eEnd
    };

    enum class RangeMark
    {
        none = 0,
        begin = 1,
        end = 2,
        current = 4
    };
    Q_DECLARE_FLAGS(RangeFlags, RangeMark);

    std::shared_ptr<UndoRecord> mUndoRec;

    std::shared_ptr<LineDataVector> m_pldA = nullptr;
    std::shared_ptr<LineDataVector> m_pldB = nullptr;
    std::shared_ptr<LineDataVector> m_pldC = nullptr;
    LineRef m_sizeA = 0;
    LineRef m_sizeB = 0;
    LineRef m_sizeC = 0;

    const Diff3LineList* m_pDiff3LineList = nullptr;
    TotalDiffStatus* m_pTotalDiffStatus = nullptr;

    qint32 m_delayedDrawTimer = 0;
    e_OverviewMode mOverviewMode = e_OverviewMode::eOMNormal;
    QString m_persistentStatusMessage;

    MergeBlockList m_mergeBlockList;
    MergeBlockList::iterator m_currentMergeBlockIt;

    qint32 m_currentPos;

    QPixmap m_pixmap;
    LineRef m_firstLine = 0;
    qint32 m_horizScrollOffset = 0;
    LineType m_nofLines = 0;
    qint32 m_maxTextWidth = -1;
    bool m_bMyUpdate = false;
    bool m_bInsertMode = true;
    bool m_bModified = false;
    void setModified(bool bModified = true);

    qint32 m_scrollDeltaX = 0;
    qint32 m_scrollDeltaY = 0;
    SafeInt<qint32> m_cursorXPos = 0;
    SafeInt<qint32> m_cursorXPixelPos = 0;
    LineRef m_cursorYPos = 0;
    qint32 m_cursorOldXPixelPos = 0;
    bool m_bCursorOn = true; // blinking on and off each second
    QTimer m_cursorTimer;
    bool m_bCursorUpdate = false;
    QStatusBar* m_pStatusBar;

    Selection m_selection;
    /*
      This list exists solely to auto disconnect boost signals.
    */
    std::list<boost::signals2::scoped_connection> connections;
    // Overrides
    void paintEvent(QPaintEvent* e) override;
    void timerEvent(QTimerEvent*) override;
    bool event(QEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void wheelEvent(QWheelEvent* pWheelEvent) override;
    void focusInEvent(QFocusEvent* e) override;
    //Costum functions
    void merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly = false, bool bWhiteSpaceOnly = false);
    QString getString(qint32 lineIdx);
    void showUnsolvedConflictsStatusMessage();

    void collectHistoryInformation(e_SrcSelector src, const HistoryRange& historyRange, HistoryMap& historyMap, std::list<HistoryMap::iterator>& hitList);

    bool isItAtEnd(bool bIncrement, const MergeBlockList::const_iterator i) const
    {
        if(bIncrement)
            return i != m_mergeBlockList.end();
        else
            return i != m_mergeBlockList.begin();
    }

    bool checkOverviewIgnore(const MergeBlockList::const_iterator i) const;

    void go(Direction eDir, EndPoint eEndPoint);
    bool calcIteratorFromLineNr(
        LineType line,
        MergeBlockList::iterator& mbIt,
        MergeEditLineList::iterator& melIt);

    qint32 getTextXOffset() const;
    QVector<QTextLayout::FormatRange> getTextLayoutForLine(LineRef line, const QString& s, QTextLayout& textLayout);
    void myUpdate(qint32 afterMilliSecs);
    void writeLine(
        RLPainter& p, LineRef line, const QString& str,
        enum e_SrcSelector srcSelect, e_MergeDetails mergeDetails, RangeFlags rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict);
    void setFastSelector(MergeBlockList::iterator i);
    LineRef convertToLine(qint32 y);

    bool canCut() { return hasFocus() && !getSelection().isEmpty(); }
    bool canCopy() { return hasFocus() && !getSelection().isEmpty(); }

    bool deleteSelection2(QString& str, SafeInt<qint32>& x, qint32& y,
                          MergeBlockList::iterator& mbIt, MergeEditLineList::iterator& melIt);
    bool doRelevantChangesExist();

  public Q_SLOTS:
    void setOverviewMode(e_OverviewMode eOverviewMode);
    void setFirstLine(LineRef firstLine);
    void setHorizScrollOffset(const qint32 horizScrollOffset);

    void slotGoCurrent();
    void slotGoTop();
    void slotGoBottom();
    void slotGoPrevDelta();
    void slotGoNextDelta();
    void slotGoPrevUnsolvedConflict();
    void slotGoNextUnsolvedConflict();
    void slotGoPrevConflict();
    void slotGoNextConflict();
    void slotAutoSolve();
    void slotUnsolve();
    void slotMergeHistory();
    void slotRegExpAutoMerge();
    void slotSplitDiff(LineType firstD3lLineIdx, LineType lastD3lLineIdx);
    void slotJoinDiffs(LineType firstD3lLineIdx, LineType lastD3lLineIdx);
    void slotSetFastSelectorLine(LineType);
    void setPaintingAllowed(bool);
    void updateSourceMask();
    void slotStatusMessageChanged(const QString&);

    void slotChooseAEverywhere() { chooseGlobal(e_SrcSelector::A, false, false); }
    void slotChooseBEverywhere() { chooseGlobal(e_SrcSelector::B, false, false); }
    void slotChooseCEverywhere() { chooseGlobal(e_SrcSelector::C, false, false); }

    void slotChooseAForUnsolvedConflicts() { chooseGlobal(e_SrcSelector::A, true, false); }
    void slotChooseBForUnsolvedConflicts() { chooseGlobal(e_SrcSelector::B, true, false); }
    void slotChooseCForUnsolvedConflicts() { chooseGlobal(e_SrcSelector::C, true, false); }

    void slotChooseAForUnsolvedWhiteSpaceConflicts() { chooseGlobal(e_SrcSelector::A, true, true); }
    void slotChooseBForUnsolvedWhiteSpaceConflicts() { chooseGlobal(e_SrcSelector::B, true, true); }
    void slotChooseCForUnsolvedWhiteSpaceConflicts() { chooseGlobal(e_SrcSelector::C, true, true); }

    void slotRefresh();

    void slotResize();

    void slotCut();
    void slotCopy();
    void slotSelectAll();

    void scrollVertically(qint32 deltaY);

    void deleteSelection();
    void pasteClipboard(bool bFromSelection);
  private Q_SLOTS:
    void slotCursorUpdate();

  Q_SIGNALS:
    void statusBarMessage(const QString& message);
    void scrollMergeResultWindow(qint32 deltaX, qint32 deltaY);
    void modifiedChanged(bool bModified);
    void setFastSelectorRange(LineRef line1, LineType nofLines);
    void sourceMask(qint32 srcMask, qint32 enabledMask);
    void resizeSignal();
    void selectionEnd();
    void newSelection();
    void updateAvailabilities();
    void showPopupMenu(const QPoint& point);
    void noRelevantChangesDetected();

    void scrollToH(qsizetype pos);
};

class QLineEdit;
class QComboBox;
class QLabel;

class WindowTitleWidget: public QWidget
{
    Q_OBJECT
  private:
    struct {
        QByteArray name;
        bool hasBOM = false;
    } EcodingItemData;

    QLabel*      m_pLabel;
    FileNameLineEdit*   m_pFileNameLineEdit;
    //QPushButton* m_pBrowseButton;
    QLabel*      m_pModifiedLabel;
    QLabel*      m_pLineEndStyleLabel;
    QComboBox*   m_pLineEndStyleSelector;
    QLabel*      m_pEncodingLabel;
    QComboBox* m_pEncodingSelector;

  public:
    WindowTitleWidget();
    const char* getEncoding();
    void setFileName(const QString& fileName);
    QString getFileName();
    void setEncodings(const QByteArray& pCodecForA, const QByteArray& pCodecForB, const QByteArray& pCodecForC);
    void setEncoding(const char* encoding);
    void setLineEndStyles(e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC);
    e_LineEndStyle getLineEndStyle();

    bool eventFilter(QObject* o, QEvent* e) override;
  public Q_SLOTS:
    void slotSetModified(bool bModified);
    //private Q_SLOTS:
    //   void slotBrowseButtonClicked();
};

#endif
