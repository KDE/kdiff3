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

class MergeResultWindow: public QWidget
{
    Q_OBJECT
  public:
    static QScrollBar* mVScrollBar;

    MergeResultWindow(QWidget* pParent, const QSharedPointer<Options>& pOptions, QStatusBar* pStatusBar);

    void init(
        const std::shared_ptr<LineDataVector> &pLineDataA, LineRef sizeA,
        const std::shared_ptr<LineDataVector> &pLineDataB, LineRef sizeB,
        const std::shared_ptr<LineDataVector> &pLineDataC, LineRef sizeC,
        const Diff3LineList* pDiff3LineList,
        TotalDiffStatus* pTotalDiffStatus,
        bool bAutoSolve
    );

    void setupConnections(const KDiff3App* app);

    static void initActions(KActionCollection* ac);

    void connectActions() const;
    void reset();

    bool saveDocument(const QString& fileName, QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle);
    qint32 getNumberOfUnsolvedConflicts(qint32* pNrOfWhiteSpaceConflicts = nullptr) const;
    void choose(e_SrcSelector selector);
    void chooseGlobal(e_SrcSelector selector, bool bConflictsOnly, bool bWhiteSpaceOnly);

    qint32 getMaxTextWidth();         // width of longest text line
    [[nodiscard]] qint32 getNofLines() const;
    qint32 getVisibleTextAreaWidth() const; // text area width without the border
    qint32 getNofVisibleLines() const;
    QString getSelection() const;
    void resetSelection();
    void showNumberOfConflicts(bool showIfNone = false);
    bool isDeltaAboveCurrent() const;
    bool isDeltaBelowCurrent() const;
    bool isConflictAboveCurrent() const;
    bool isConflictBelowCurrent() const;
    bool isUnsolvedConflictAtCurrent() const;
    bool isUnsolvedConflictAboveCurrent() const;
    bool isUnsolvedConflictBelowCurrent() const;
    bool findString(const QString& s, LineRef& d3vLine, qint32& posInLine, bool bDirDown, bool bCaseSensitive);
    void setSelection(qint32 firstLine, qint32 startPos, qint32 lastLine, qint32 endPos);
    e_OverviewMode getOverviewMode() const;

    void slotUpdateAvailabilities();

  public Q_SLOTS:
    void setOverviewMode(e_OverviewMode eOverviewMode);
    void setFirstLine(qint32 firstLine);
    void setHorizScrollOffset(qint32 horizScrollOffset);

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
    void slotSplitDiff(LineIndex firstD3lLineIdx, LineIndex lastD3lLineIdx);
    void slotJoinDiffs(LineIndex firstD3lLineIdx, LineIndex lastD3lLineIdx);
    void slotSetFastSelectorLine(LineIndex);
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

  Q_SIGNALS:
    void statusBarMessage(const QString& message);
    void scrollMergeResultWindow(qint32 deltaX, qint32 deltaY);
    void modifiedChanged(bool bModified);
    void setFastSelectorRange(LineRef line1, LineCount nofLines);
    void sourceMask(qint32 srcMask, qint32 enabledMask);
    void resizeSignal();
    void selectionEnd();
    void newSelection();
    void updateAvailabilities();
    void showPopupMenu(const QPoint& point);
    void noRelevantChangesDetected();

  private:
    void merge(bool bAutoSolve, e_SrcSelector defaultSelector, bool bConflictsOnly = false, bool bWhiteSpaceOnly = false);
    QString getString(qint32 lineIdx);
    void showUnsolvedConflictsStatusMessage();

    static QPointer<QAction> chooseAEverywhere;
    static QPointer<QAction> chooseBEverywhere;
    static QPointer<QAction> chooseCEverywhere;
    static QPointer<QAction> chooseAForUnsolvedConflicts;
    static QPointer<QAction> chooseBForUnsolvedConflicts;
    static QPointer<QAction> chooseCForUnsolvedConflicts;
    static QPointer<QAction> chooseAForUnsolvedWhiteSpaceConflicts;
    static QPointer<QAction> chooseBForUnsolvedWhiteSpaceConflicts;
    static QPointer<QAction> chooseCForUnsolvedWhiteSpaceConflicts;

    QSharedPointer<Options> m_pOptions = nullptr;

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

  private:
    struct HistoryMapEntry {
        MergeEditLineList mellA;
        MergeEditLineList mellB;
        MergeEditLineList mellC;
        MergeEditLineList& choice(bool bThreeInputs);
        bool staysInPlace(bool bThreeInputs, Diff3LineList::const_iterator& iHistoryEnd);
    };
    typedef std::map<QString, HistoryMapEntry> HistoryMap;
    void collectHistoryInformation(e_SrcSelector src, const HistoryRange& historyRange, HistoryMap& historyMap, std::list<HistoryMap::iterator>& hitList);

    MergeLineList m_mergeLineList;
    MergeLineListImp::iterator m_currentMergeLineIt;
    bool isItAtEnd(bool bIncrement, MergeLineListImp::iterator i) const
    {
        if(bIncrement)
            return i != m_mergeLineList.list().end();
        else
            return i != m_mergeLineList.list().begin();
    }

    qint32 m_currentPos;
    bool checkOverviewIgnore(MergeLineListImp::iterator& i) const;

    enum e_Direction
    {
        eUp,
        eDown
    };
    enum e_EndPoint
    {
        eDelta,
        eConflict,
        eUnsolvedConflict,
        eLine,
        eEnd
    };
    void go(e_Direction eDir, e_EndPoint eEndPoint);
    bool calcIteratorFromLineNr(
        LineRef::LineType line,
        MergeLineListImp::iterator& mlIt,
        MergeEditLineList::iterator& melIt);

    void paintEvent(QPaintEvent* e) override;

    qint32 getTextXOffset() const;
    QVector<QTextLayout::FormatRange> getTextLayoutForLine(LineRef line, const QString& s, QTextLayout& textLayout);
    void myUpdate(qint32 afterMilliSecs);
    void timerEvent(QTimerEvent*) override;
    void writeLine(
        RLPainter& p, qint32 line, const QString& str,
        enum e_SrcSelector srcSelect, e_MergeDetails mergeDetails, qint32 rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict
    );
    void setFastSelector(MergeLineListImp::iterator i);
    LineRef convertToLine(qint32 y);
    bool event(QEvent*) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void resizeEvent(QResizeEvent* e) override;
    void keyPressEvent(QKeyEvent* e) override;
    void wheelEvent(QWheelEvent* pWheelEvent) override;
    void focusInEvent(QFocusEvent* e) override;

    bool canCut() { return hasFocus() && !getSelection().isEmpty(); }
    bool canCopy() { return hasFocus() && !getSelection().isEmpty(); }

    QPixmap m_pixmap;
    LineRef m_firstLine = 0;
    qint32 m_horizScrollOffset = 0;
    LineCount m_nofLines = 0;
    qint32 m_maxTextWidth = -1;
    bool m_bMyUpdate = false;
    bool m_bInsertMode = true;
    bool m_bModified = false;
    void setModified(bool bModified = true);

    qint32 m_scrollDeltaX = 0;
    qint32 m_scrollDeltaY = 0;
    qint32 m_cursorXPos = 0;
    qint32 m_cursorXPixelPos;
    qint32 m_cursorYPos = 0;
    qint32 m_cursorOldXPixelPos = 0;
    bool m_bCursorOn = true; // blinking on and off each second
    QTimer m_cursorTimer;
    bool m_bCursorUpdate = false;
    QStatusBar* m_pStatusBar;

    Selection m_selection;

    bool deleteSelection2(QString& str, qint32& x, qint32& y,
                          MergeLineListImp::iterator& mlIt, MergeEditLineList::iterator& melIt);
    bool doRelevantChangesExist();

    /*
      This list exists solely to auto disconnect boost signals.
    */
    std::list<boost::signals2::scoped_connection> connections;
  public Q_SLOTS:
    void deleteSelection();
    void pasteClipboard(bool bFromSelection);
  private Q_SLOTS:
    void slotCursorUpdate();
};

class QLineEdit;
class QTextCodec;
class QComboBox;
class QLabel;

class WindowTitleWidget: public QWidget
{
    Q_OBJECT
  private:
    QLabel*      m_pLabel;
    FileNameLineEdit*   m_pFileNameLineEdit;
    //QPushButton* m_pBrowseButton;
    QLabel*      m_pModifiedLabel;
    QLabel*      m_pLineEndStyleLabel;
    QComboBox*   m_pLineEndStyleSelector;
    QLabel*      m_pEncodingLabel;
    QComboBox*   m_pEncodingSelector;
    QSharedPointer<Options> m_pOptions;

  public:
    explicit WindowTitleWidget(const QSharedPointer<Options>& pOptions);
    QTextCodec* getEncoding();
    void setFileName(const QString& fileName);
    QString getFileName();
    void setEncodings(QTextCodec* pCodecForA, QTextCodec* pCodecForB, QTextCodec* pCodecForC);
    void setEncoding(QTextCodec* pEncoding);
    void setLineEndStyles(e_LineEndStyle eLineEndStyleA, e_LineEndStyle eLineEndStyleB, e_LineEndStyle eLineEndStyleC);
    e_LineEndStyle getLineEndStyle();

    bool eventFilter(QObject* o, QEvent* e) override;
  public Q_SLOTS:
    void slotSetModified(bool bModified);
    //private Q_SLOTS:
    //   void slotBrowseButtonClicked();
};

#endif
