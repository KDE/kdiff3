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

#include "SourceData.h"
#include "diff.h"

#include "LineRef.h"
#include "TypeUtils.h"

#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QSharedPointer>
#include <QString>
#include <QWheelEvent>

#include <boost/signals2.hpp>
#include <list>
#include <memory>

class QMenu;
class RecalcWordWrapThread;
class QScrollBar;
class QStatusBar;
class Options;
class DiffTextWindowData;
class DiffTextWindowFrame;
class EncodingLabel;
class RLPainter;
class SourceData;

class KDiff3App;

class DiffTextWindow: public QWidget
{
    Q_OBJECT
  public:
    //Using this as a scoped global
    static QScrollBar* mVScrollBar;
    static QAtomicInteger<size_t> maxThreads();

    DiffTextWindow(DiffTextWindowFrame* pParent, const QSharedPointer<Options> &pOptions, e_SrcSelector winIdx, KDiff3App &app);
    ~DiffTextWindow() override;
    void init(
        const QString& fileName,
        QTextCodec* pTextCodec,
        e_LineEndStyle eLineEndStyle,
        const std::shared_ptr<LineDataVector> &pLineData,
        LineCount size,
        const Diff3LineVector* pDiff3LineVector,
        const ManualDiffHelpList* pManualDiffHelpList
    );

    void setupConnections(const KDiff3App *app);

    void reset();
    void convertToLinePos(qint32 x, qint32 y, LineRef& line, qint32& pos);

    [[nodiscard]] QString getSelection() const;
    [[nodiscard]] qint32 getFirstLine() const;
    LineRef calcTopLineInFile(const LineRef firstLine);

    [[nodiscard]] qint32 getMaxTextWidth() const;
    [[nodiscard]] LineCount getNofLines() const;
    [[nodiscard]] LineCount getNofVisibleLines() const;
    [[nodiscard]] qint32 getVisibleTextAreaWidth() const;

    LineIndex convertLineToDiff3LineIdx(LineRef line);
    LineRef convertDiff3LineIdxToLine(LineIndex d3lIdx);

    void convertD3LCoordsToLineCoords(LineIndex d3LIdx, qint32 d3LPos, LineRef& line, qint32& pos);
    void convertLineCoordsToD3LCoords(LineRef line, qint32 pos, LineIndex& d3LIdx, qint32& d3LPos);

    void convertSelectionToD3LCoords();

    bool findString(const QString& s, LineRef& d3vLine, qint32& posInLine, bool bDirDown, bool bCaseSensitive);
    void setSelection(LineRef firstLine, qint32 startPos, LineRef lastLine, qint32 endPos, LineRef& l, qint32& p);
    void getSelectionRange(LineRef* firstLine, LineRef* lastLine, e_CoordType coordType);

    void setPaintingAllowed(bool bAllowPainting);
    void recalcWordWrap(bool bWordWrap, QtSizeType wrapLineVectorSize, qint32 visibleTextWidth);
    void recalcWordWrapHelper(QtSizeType wrapLineVectorSize, qint32 visibleTextWidth, QtSizeType cacheListIdx);

    void printWindow(RLPainter& painter, const QRect& view, const QString& headerText, qint32 line, const LineCount linesPerPage, const QColor& fgColor);
    void print(RLPainter& painter, const QRect& r, qint32 firstLine, const LineCount nofLinesPerPage);

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
    void setFastSelectorLine(LineIndex line);
    void gotFocus();
    void lineClicked(e_SrcSelector winIdx, LineRef line);

    void finishRecalcWordWrap(qint32 visibleTextWidthForPrinting);

    void finishDrop();

    void firstLineChanged(qint32 firstLine);
  public Q_SLOTS:
    void setFirstLine(qint32 line);
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
    static QList<RecalcWordWrapThread*> s_runnables;
    static constexpr qint32 s_linesPerRunnable = 2000;

    /*
      This list exists solely to auto disconnect boost signals.
    */
    std::list<boost::signals2::scoped_connection> connections;

    KDiff3App &m_app;
    std::unique_ptr<DiffTextWindowData> d;

    void showStatusLine(const LineRef lineFromPos);

    bool canCopy() { return hasFocus() && !getSelection().isEmpty(); }
};

class DiffTextWindowFrameData;

class DiffTextWindowFrame : public QWidget
{
    Q_OBJECT
  public:
    DiffTextWindowFrame(QWidget* pParent, const QSharedPointer<Options> &pOptions, e_SrcSelector winIdx, const QSharedPointer<SourceData> &psd, KDiff3App &app);
    ~DiffTextWindowFrame() override;
    QPointer<DiffTextWindow> getDiffTextWindow();
    void init();

    void setupConnections(const KDiff3App *app);

  Q_SIGNALS:
    void fileNameChanged(const QString&, e_SrcSelector);
    void encodingChanged(QTextCodec*);

  public Q_SLOTS:
    void setFirstLine(qint32 firstLine);

  protected:
    bool eventFilter(QObject*, QEvent*) override;
    //void paintEvent(QPaintEvent*);
  private Q_SLOTS:
    void slotReturnPressed();
    void slotBrowseButtonClicked();

    void slotEncodingChanged(QTextCodec* c);

  private:
    std::unique_ptr<DiffTextWindowFrameData> d;
};

class EncodingLabel : public QLabel
{
    Q_OBJECT
  public:
    EncodingLabel(const QString& text, const QSharedPointer<SourceData> &psd, const QSharedPointer<Options> &pOptions);

  protected:
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;

  Q_SIGNALS:
    void encodingChanged(QTextCodec*);

  private Q_SLOTS:
    void slotSelectEncoding();

  private:
    QMenu* m_pContextEncodingMenu;
    QSharedPointer<SourceData> m_pSourceData; //SourceData to get access to "isEmpty()" and "isFromBuffer()" functions
    static const qint32 m_maxRecentEncodings = 5;
    QSharedPointer<Options> m_pOptions;

    void insertCodec(const QString& visibleCodecName, QTextCodec* pCodec, QList<qint32>& CodecEnumList, QMenu* pMenu, qint32 currentTextCodecEnum) const;
};

#endif
