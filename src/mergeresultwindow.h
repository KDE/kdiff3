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

    static void initActions(KActionCollection* ac);

    void connectActions() const;
    void reset();

    bool saveDocument(const QString& fileName, QTextCodec* pEncoding, e_LineEndStyle eLineEndStyle);
    [[nodiscard]] int getNumberOfUnsolvedConflicts(int* pNrOfWhiteSpaceConflicts = nullptr) const;
    void choose(e_SrcSelector selector);
    void chooseGlobal(e_SrcSelector selector, bool bConflictsOnly, bool bWhiteSpaceOnly);

    [[nodiscard]] int getMaxTextWidth(); // width of longest text line
    [[nodiscard]] LineType getNofLines() const;
    [[nodiscard]] int getVisibleTextAreaWidth() const; // text area width without the border
    [[nodiscard]] int getNofVisibleLines() const;
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
    bool findString(const QString& s, LineRef& d3vLine, QtSizeType& posInLine, bool bDirDown, bool bCaseSensitive);
    void setSelection(LineType firstLine, QtSizeType startPos, LineType lastLine, QtSizeType endPos);
    [[nodiscard]] e_OverviewMode getOverviewMode() const;

    void slotUpdateAvailabilities();

  private:
    static QPointer<QAction> chooseAEverywhere;
    static QPointer<QAction> chooseBEverywhere;
    static QPointer<QAction> chooseCEverywhere;
    static QPointer<QAction> chooseAForUnsolvedConflicts;
    static QPointer<QAction> chooseBForUnsolvedConflicts;
    static QPointer<QAction> chooseCForUnsolvedConflicts;
    static QPointer<QAction> chooseAForUnsolvedWhiteSpaceConflicts;
    static QPointer<QAction> chooseBForUnsolvedWhiteSpaceConflicts;
    static QPointer<QAction> chooseCForUnsolvedWhiteSpaceConflicts;

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

    std::shared_ptr<LineDataVector> m_pldA = nullptr;
    std::shared_ptr<LineDataVector> m_pldB = nullptr;
    std::shared_ptr<LineDataVector> m_pldC = nullptr;
    LineRef m_sizeA = 0;
    LineRef m_sizeB = 0;
    LineRef m_sizeC = 0;

    const Diff3LineList* m_pDiff3LineList = nullptr;
    TotalDiffStatus* m_pTotalDiffStatus = nullptr;

    int m_delayedDrawTimer = 0;
    e_OverviewMode mOverviewMode = e_OverviewMode::eOMNormal;
    QString m_persistentStatusMessage;

    MergeBlockList m_mergeBlockList;
    MergeBlockListImp::iterator m_currentMergeBlockIt;

    int m_currentPos;

    QPixmap m_pixmap;
    LineRef m_firstLine = 0;
    int m_horizScrollOffset = 0;
    LineType m_nofLines = 0;
    int m_maxTextWidth = -1;
    bool m_bMyUpdate = false;
    bool m_bInsertMode = true;
    bool m_bModified = false;
    void setModified(bool bModified = true);

    int m_scrollDeltaX = 0;
    int m_scrollDeltaY = 0;
    QtNumberType m_cursorXPos = 0;
    int m_cursorXPixelPos;
    int m_cursorYPos = 0;
    int m_cursorOldXPixelPos = 0;
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
    QString getString(int lineIdx);
    void showUnsolvedConflictsStatusMessage();

    void collectHistoryInformation(e_SrcSelector src, const HistoryRange& historyRange, HistoryMap& historyMap, std::list<HistoryMap::iterator>& hitList);

    bool isItAtEnd(bool bIncrement, const MergeBlockListImp::const_iterator i) const
    {
        if(bIncrement)
            return i != m_mergeBlockList.list().end();
        else
            return i != m_mergeBlockList.list().begin();
    }

    bool checkOverviewIgnore(const MergeBlockListImp::const_iterator i) const;

    void go(Direction eDir, EndPoint eEndPoint);
    bool calcIteratorFromLineNr(
        LineType line,
        MergeBlockListImp::iterator& mbIt,
        MergeEditLineList::iterator& melIt);

    int getTextXOffset() const;
    QVector<QTextLayout::FormatRange> getTextLayoutForLine(LineRef line, const QString& s, QTextLayout& textLayout);
    void myUpdate(int afterMilliSecs);
    void writeLine(
        RLPainter& p, int line, const QString& str,
        enum e_SrcSelector srcSelect, e_MergeDetails mergeDetails, int rangeMark, bool bUserModified, bool bLineRemoved, bool bWhiteSpaceConflict);
    void setFastSelector(MergeBlockListImp::iterator i);
    LineRef convertToLine(QtNumberType y);

    bool canCut() { return hasFocus() && !getSelection().isEmpty(); }
    bool canCopy() { return hasFocus() && !getSelection().isEmpty(); }

    bool deleteSelection2(QString& str, int& x, int& y,
                          MergeBlockListImp::iterator& mbIt, MergeEditLineList::iterator& melIt);
    bool doRelevantChangesExist();

  public Q_SLOTS:
    void setOverviewMode(e_OverviewMode eOverviewMode);
    void setFirstLine(QtNumberType firstLine);
    void setHorizScrollOffset(const int horizScrollOffset);

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

    void scrollVertically(QtNumberType deltaY);

    void deleteSelection();
    void pasteClipboard(bool bFromSelection);
  private Q_SLOTS:
    void slotCursorUpdate();

  Q_SIGNALS:
    void statusBarMessage(const QString& message);
    void scrollMergeResultWindow(int deltaX, int deltaY);
    void modifiedChanged(bool bModified);
    void setFastSelectorRange(LineRef line1, LineType nofLines);
    void sourceMask(int srcMask, int enabledMask);
    void resizeSignal();
    void selectionEnd();
    void newSelection();
    void updateAvailabilities();
    void showPopupMenu(const QPoint& point);
    void noRelevantChangesDetected();
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
    QComboBox* m_pEncodingSelector;

  public:
    WindowTitleWidget();
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
