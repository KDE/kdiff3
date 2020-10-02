/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DIFFTEXTWINDOW_H
#define DIFFTEXTWINDOW_H

#include "diff.h"

#include <QLabel>
#include <QSharedPointer>  // for QSharedPointer
#include <QString>         // for QString

#include <boost/signals2.hpp>

class QMenu;
class RecalcWordWrapRunnable;
class QScrollBar;
class QStatusBar;
class Options;
class DiffTextWindowData;
class DiffTextWindowFrame;
class EncodingLabel;
class RLPainter;

class KDiff3App;

class DiffTextWindow : public QWidget
{
    Q_OBJECT
  public:
    //Using this as a scoped global
    static QScrollBar* mVScrollBar;

    DiffTextWindow(DiffTextWindowFrame* pParent, const QSharedPointer<Options> &pOptions, e_SrcSelector winIdx);
    ~DiffTextWindow() override;
    void init(
        const QString& fileName,
        QTextCodec* pTextCodec,
        e_LineEndStyle eLineEndStyle,
        const QVector<LineData>* pLineData,
        int size,
        const Diff3LineVector* pDiff3LineVector,
        const ManualDiffHelpList* pManualDiffHelpList
    );

    void setupConnections(const KDiff3App *app);

    void reset();
    void convertToLinePos(int x, int y, LineRef& line, int& pos);

    QString getSelection();
    int getFirstLine();
    LineRef calcTopLineInFile(const LineRef firstLine);

    int getMaxTextWidth();
    LineCount getNofLines();
    int getNofVisibleLines();
    int getVisibleTextAreaWidth();

    int convertLineToDiff3LineIdx(LineRef line);
    LineRef convertDiff3LineIdxToLine(int d3lIdx);

    void convertD3LCoordsToLineCoords(int d3LIdx, int d3LPos, int& line, int& pos);
    void convertLineCoordsToD3LCoords(int line, int pos, int& d3LIdx, int& d3LPos);

    void convertSelectionToD3LCoords();

    bool findString(const QString& s, LineRef& d3vLine, int& posInLine, bool bDirDown, bool bCaseSensitive);
    void setSelection(LineRef firstLine, int startPos, LineRef lastLine, int endPos, LineRef& l, int& p);
    void getSelectionRange(LineRef* firstLine, LineRef* lastLine, e_CoordType coordType);

    void setPaintingAllowed(bool bAllowPainting);
    void recalcWordWrap(bool bWordWrap, int wrapLineVectorSize, int visibleTextWidth);
    void recalcWordWrapHelper(int wrapLineVectorSize, int visibleTextWidth, int cacheListIdx);

    void printWindow(RLPainter& painter, const QRect& view, const QString& headerText, int line, int linesPerPage, const QColor& fgColor);
    void print(RLPainter& painter, const QRect& r, int firstLine, int nofLinesPerPage);

    static bool startRunnables();

    bool isThreeWay() const;
    const QString& getFileName() const;

    e_SrcSelector getWindowIndex() const;

    const QString getEncodingDisplayString() const;
    e_LineEndStyle getLineEndStyle() const;
    const Diff3LineVector* getDiff3LineVector() const;

    qint32 getLineNumberWidth() const;

    void setSourceData(const QSharedPointer<SourceData>& inData);
  Q_SIGNALS:
    void statusBarMessage(const QString& message);
    void scrollVertically(QtNumberType deltaY);
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

    DiffTextWindowData* d;
    void showStatusLine(const LineRef lineFromPos);

    bool canCopy() { return hasFocus() && !getSelection().isEmpty(); }
};

class DiffTextWindowFrameData;

class DiffTextWindowFrame : public QWidget
{
    Q_OBJECT
  public:
    DiffTextWindowFrame(QWidget* pParent, const QSharedPointer<Options> &pOptions, e_SrcSelector winIdx, QSharedPointer<SourceData> psd);
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

    void slotEncodingChanged(QTextCodec* c) { Q_EMIT encodingChanged(c); };//relay signal from encoding label


  private:
    DiffTextWindowFrameData* d;
};

class EncodingLabel : public QLabel
{
    Q_OBJECT
  public:
    EncodingLabel(const QString& text, QSharedPointer<SourceData> psd, const QSharedPointer<Options> &pOptions);

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
