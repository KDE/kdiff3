/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/


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
class RecalcWordWrapRunnable;
class QScrollBar;
class QStatusBar;
class Options;
class DiffTextWindowData;
class DiffTextWindowFrame;
class EncodingLabel;
class RLPainter;
class SourceData;

class KDiff3App;

class DiffTextWindow : public QWidget
{
    Q_OBJECT
  public:
    //Using this as a scoped global
    static QScrollBar* mVScrollBar;

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
    void convertToLinePos(int x, int y, LineRef& line, QtNumberType& pos);

    [[nodiscard]] QString getSelection() const;
    [[nodiscard]] int getFirstLine() const;
    LineRef calcTopLineInFile(const LineRef firstLine);

    [[nodiscard]] int getMaxTextWidth() const;
    [[nodiscard]] LineCount getNofLines() const;
    [[nodiscard]] LineCount getNofVisibleLines() const;
    [[nodiscard]] int getVisibleTextAreaWidth() const;

    LineIndex convertLineToDiff3LineIdx(LineRef line);
    LineRef convertDiff3LineIdxToLine(LineIndex d3lIdx);

    void convertD3LCoordsToLineCoords(LineIndex d3LIdx, int d3LPos, LineRef& line, int& pos);
    void convertLineCoordsToD3LCoords(LineRef line, int pos, LineIndex& d3LIdx, int& d3LPos);

    void convertSelectionToD3LCoords();

    bool findString(const QString& s, LineRef& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive);
    void setSelection(LineRef firstLine, int startPos, LineRef lastLine, int endPos, LineRef& l, int& p);
    void getSelectionRange(LineRef* firstLine, LineRef* lastLine, e_CoordType coordType);

    void setPaintingAllowed(bool bAllowPainting);
    void recalcWordWrap(bool bWordWrap, QtSizeType wrapLineVectorSize, int visibleTextWidth);
    void recalcWordWrapHelper(QtSizeType wrapLineVectorSize, int visibleTextWidth, QtSizeType cacheListIdx);

    void printWindow(RLPainter& painter, const QRect& view, const QString& headerText, int line, const LineCount linesPerPage, const QColor& fgColor);
    void print(RLPainter& painter, const QRect& r, int firstLine, const LineCount nofLinesPerPage);

    static bool startRunnables();

    [[nodiscard]] bool isThreeWay() const;
    [[nodiscard]] const QString& getFileName() const;

    [[nodiscard]] e_SrcSelector getWindowIndex() const;

    [[nodiscard]] const QString getEncodingDisplayString() const;
    [[nodiscard]] e_LineEndStyle getLineEndStyle() const;
    [[nodiscard]] const Diff3LineVector* getDiff3LineVector() const;

    [[nodiscard]] qint32 getLineNumberWidth() const;

    void setSourceData(const QSharedPointer<SourceData>& inData);

    void scrollVertically(QtNumberType deltaY);
  Q_SIGNALS:
    void statusBarMessage(const QString& message);
    void resizeHeightChangedSignal(int nofVisibleLines);
    void resizeWidthChangedSignal(int nofVisibleColumns);
    void scrollDiffTextWindow(int deltaX, int deltaY);
    void newSelection();
    void selectionEnd();
    void setFastSelectorLine(LineIndex line);
    void gotFocus();
    void lineClicked(e_SrcSelector winIdx, LineRef line);

    void finishRecalcWordWrap(int visibleTextWidthForPrinting);

    void finishDrop();

    void firstLineChanged(QtNumberType firstLine);
  public Q_SLOTS:
    void setFirstLine(QtNumberType line);
    void setHorizScrollOffset(int horizScrollOffset);
    void resetSelection();
    void setFastSelectorRange(int line1, int nofLines);
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
    static QList<RecalcWordWrapRunnable*> s_runnables;
    static constexpr int s_linesPerRunnable = 2000;

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
    DiffTextWindow* getDiffTextWindow();
    void init();

    void setupConnections(const KDiff3App *app);

  Q_SIGNALS:
    void fileNameChanged(const QString&, e_SrcSelector);
    void encodingChanged(QTextCodec*);

  public Q_SLOTS:
    void setFirstLine(QtNumberType firstLine);

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
    static const int m_maxRecentEncodings = 5;
    QSharedPointer<Options> m_pOptions;

    void insertCodec(const QString& visibleCodecName, QTextCodec* pCodec, QList<int>& CodecEnumList, QMenu* pMenu, int currentTextCodecEnum) const;
};

#endif
