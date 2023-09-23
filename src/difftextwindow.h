// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#ifndef DIFFTEXTWINDOW_H
#define DIFFTEXTWINDOW_H

#include "diff.h"
#include "SourceData.h"

#include "LineRef.h"
#include "TypeUtils.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPointer>
#include <QSharedPointer>
#include <QString>
#include <QWheelEvent>

#include <boost/signals2.hpp>
#include <list>
#include <memory>
#include <vector>

class QMenu;
class QPushButton;
class QScrollBar;
class QStatusBar;
class RecalcWordWrapThread;
class Options;
class DiffTextWindowData;
class DiffTextWindowFrame;
class EncodingLabel;
class RLPainter;
class SourceData;
class FileNameLineEdit;

class KDiff3App;

class DiffTextWindow: public QWidget
{
    Q_OBJECT
  public:
    //Using this as a scoped global
    static QPointer<QScrollBar> mVScrollBar;
    static QAtomicInteger<size_t> maxThreads();

    DiffTextWindow(DiffTextWindowFrame* pParent, e_SrcSelector winIdx, KDiff3App& app);
    ~DiffTextWindow() override;
    void init(
        const QString& fileName,
        const char* pTextCodec,
        e_LineEndStyle eLineEndStyle,
        const std::shared_ptr<LineDataVector>& pLineData,
        LineType size,
        const Diff3LineVector* pDiff3LineVector,
        const ManualDiffHelpList* pManualDiffHelpList);

    void setupConnections(const KDiff3App* app);

    void reset();
    void convertToLinePos(qint32 x, qint32 y, LineRef& line, qint32& pos);

    [[nodiscard]] QString getSelection() const;
    [[nodiscard]] LineRef getFirstLine() const;
    LineRef calcTopLineInFile(const LineRef firstLine) const;

    [[nodiscard]] qint32 getMaxTextWidth() const;
    [[nodiscard]] LineType getNofLines() const;
    [[nodiscard]] LineType getNofVisibleLines() const;
    [[nodiscard]] qint32 getVisibleTextAreaWidth() const;

    LineType convertLineToDiff3LineIdx(const LineRef line) const;
    LineRef convertDiff3LineIdxToLine(const LineType d3lIdx) const;

    void convertD3LCoordsToLineCoords(LineType d3LIdx, QtSizeType d3LPos, LineRef& line, QtSizeType& pos) const;
    void convertLineCoordsToD3LCoords(LineRef line, QtSizeType pos, LineType& d3LIdx, QtSizeType& d3LPos) const;

    void convertSelectionToD3LCoords() const;

    bool findString(const QString& s, LineRef& d3vLine, QtSizeType& posInLine, bool bDirDown, bool bCaseSensitive);
    void setSelection(LineRef firstLine, QtSizeType startPos, LineRef lastLine, QtSizeType endPos, LineRef& l, QtSizeType& p);
    void getSelectionRange(LineRef* firstLine, LineRef* lastLine, e_CoordType coordType) const;

    void setPaintingAllowed(bool bAllowPainting);
    void recalcWordWrap(bool bWordWrap, QtSizeType wrapLineVectorSize, qint32 visibleTextWidth);
    void recalcWordWrapHelper(QtSizeType wrapLineVectorSize, qint32 visibleTextWidth, size_t cacheListIdx);

    void printWindow(RLPainter& painter, const QRect& view, const QString& headerText, qint32 line, const LineType linesPerPage, const QColor& fgColor);
    void print(RLPainter& painter, const QRect& r, qint32 firstLine, const LineType nofLinesPerPage);

    static bool startRunnables();

    [[nodiscard]] bool isThreeWay() const;
    [[nodiscard]] const QString& getFileName() const;

    [[nodiscard]] e_SrcSelector getWindowIndex() const;

    [[nodiscard]] const QString getEncodingDisplayString() const;
    [[nodiscard]] e_LineEndStyle getLineEndStyle() const;
    [[nodiscard]] const Diff3LineVector* getDiff3LineVector() const;

    [[nodiscard]] qint32 getLineNumberWidth() const;

    void setSourceData(const QSharedPointer<SourceData>& inData);

    void scrollVertically(qint32 deltaY);
  Q_SIGNALS:
    void statusBarMessage(const QString& message);
    void resizeHeightChangedSignal(qint32 nofVisibleLines);
    void resizeWidthChangedSignal(qint32 nofVisibleColumns);
    void scrollDiffTextWindow(qint32 deltaX, qint32 deltaY);
    void newSelection();
    void selectionEnd();
    void setFastSelectorLine(LineType line);
    void gotFocus();
    void lineClicked(e_SrcSelector winIdx, LineRef line);

    void finishRecalcWordWrap(qint32 visibleTextWidthForPrinting);

    void finishDrop();

    void firstLineChanged(const LineRef firstLine);
  public Q_SLOTS:
    void setFirstLine(LineRef line);
    void setHorizScrollOffset(qint32 horizScrollOffset);
    void resetSelection();
    void setFastSelectorRange(qint32 line1, qint32 nofLines);
    void slotRefresh();

    void slotSelectAll();

    void slotCopy();

  protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

    void wheelEvent(QWheelEvent*) override;

    void paintEvent(QPaintEvent*) override;
    void dragEnterEvent(QDragEnterEvent* dragEnterEvent) override;

    void dropEvent(QDropEvent* dropEvent) override;
    void focusInEvent(QFocusEvent* e) override;

    void resizeEvent(QResizeEvent*) override;
    void timerEvent(QTimerEvent*) override;

  private:
    static std::vector<RecalcWordWrapThread*> s_runnables;
    static constexpr qint32 s_linesPerRunnable = 2000;

    /*
      This list exists solely to auto disconnect boost signals.
    */
    std::list<boost::signals2::scoped_connection> connections;

    KDiff3App& m_app;
    e_SrcSelector mWinIdx = e_SrcSelector::None;
    std::unique_ptr<DiffTextWindowData> d;

    void showStatusLine(const LineRef lineFromPos);

    bool canCopy() { return hasFocus() && !getSelection().isEmpty(); }
};

class DiffTextWindowFrame: public QWidget
{
    Q_OBJECT
  public:
    DiffTextWindowFrame(QWidget* pParent, e_SrcSelector winIdx, const QSharedPointer<SourceData>& psd, KDiff3App& app);
    ~DiffTextWindowFrame() override;
    QPointer<DiffTextWindow> getDiffTextWindow();
    void init();

    void setupConnections(const KDiff3App* app);

  Q_SIGNALS:
    void fileNameChanged(const QString&, e_SrcSelector);
    void encodingChanged(const QByteArray&);

  public Q_SLOTS:
    void setFirstLine(LineRef firstLine);

  protected:
    bool eventFilter(QObject*, QEvent*) override;
    //void paintEvent(QPaintEvent*);
  private Q_SLOTS:
    void slotReturnPressed();
    void slotBrowseButtonClicked();

    void slotEncodingChanged(const QByteArray& name);

  private:
    QLabel* m_pLabel;
    QLabel* m_pTopLine;
    QLabel* m_pEncoding;
    QLabel* m_pLineEndStyle;
    QWidget* m_pTopLineWidget;
    FileNameLineEdit* m_pFileSelection;
    QPushButton* m_pBrowseButton;

    QPointer<DiffTextWindow> m_pDiffTextWindow;
    e_SrcSelector m_winIdx;

    QSharedPointer<SourceData> mSourceData;
};

class EncodingLabel: public QLabel
{
    Q_OBJECT
  public:
    EncodingLabel(const QString& text, const QSharedPointer<SourceData>& psd);

  protected:
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;

  Q_SIGNALS:
    void encodingChanged(const QByteArray&);

  private Q_SLOTS:
    void slotSelectEncoding();

  private:
    QMenu* m_pContextEncodingMenu;
    QSharedPointer<SourceData> m_pSourceData; //SourceData to get access to "isEmpty()" and "isFromBuffer()" functions
    static const qint32 m_maxRecentEncodings = 5;

    void insertCodec(const QString& visibleCodecName, const QByteArray& nameArray, QList<QByteArray>& codecEnumList, QMenu* pMenu, const QByteArray& currentTextCodecEnum) const;
};

#endif
