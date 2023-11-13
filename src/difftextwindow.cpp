// clang-format off
/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
// clang-format on

#include "difftextwindow.h"

#include "FileNameLineEdit.h"
#include "LineRef.h"
#include "RLPainter.h"
#include "SourceData.h"
#include "TypeUtils.h"
#include "Utils.h"
#include "common.h"     // for getAtomic, max3, min3
#include "defmac.h"
#include "kdiff3.h"
#include "Logging.h"
#include "merger.h"
#include "options.h"
#include "progress.h"
#include "selection.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <utility>
#include <vector>

#include <KLocalizedString>
#include <KMessageBox>

#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QRunnable>
#include <QScrollBar>
#include <QStatusBar>
#include <QTextCodec>
#include <QTextLayout>
#include <QThread>
#include <QToolTip>
#include <QUrl>
#include <QtMath>

QScrollBar* DiffTextWindow::mVScrollBar = nullptr;
QList<RecalcWordWrapThread*> DiffTextWindow::s_runnables; //Used in startRunables and recalWordWrap

/*
    QRunnable is not enough here be may appear to work depending on configuration.
    That is an artifact of the threads being short-lived. Never the less such code is
    not safe because of a potenial race condition on exit.

    The use of Qt's parent system establishes order of destruction for QObjects.
    Allowing us to guarantee clearance of these helper threads and their accompanying
    objects before the associated DiffTextWindow begins teardown.
*/
class RecalcWordWrapThread: public QThread
{
  private:
    static QAtomicInt s_runnableCount;

    qint32 m_visibleTextWidth;
    QtSizeType m_cacheIdx;

  public:
    static QAtomicInt s_maxNofRunnables;
    RecalcWordWrapThread(DiffTextWindow *parent, qint32 visibleTextWidth, QtSizeType cacheIdx)
        : QThread(parent), m_visibleTextWidth(visibleTextWidth), m_cacheIdx(cacheIdx)
    {
        setTerminationEnabled(true);
        s_runnableCount.fetchAndAddOrdered(1);
    }
    void run() override
    {
        //safe thanks to Qt memmory mangement
        DiffTextWindow* pDTW = qobject_cast<DiffTextWindow*>(parent());
        pDTW->recalcWordWrapHelper(0, m_visibleTextWidth, m_cacheIdx);
        qint32 newValue = s_runnableCount.fetchAndAddOrdered(-1) - 1;
        ProgressProxy::setCurrent(s_maxNofRunnables - s_runnableCount.loadRelaxed());
        if(newValue == 0)
        {
            Q_EMIT pDTW->finishRecalcWordWrap(m_visibleTextWidth);

            s_maxNofRunnables.storeRelease(0);
        }
        //Cleanup our object to avoid a memmory leak
        deleteLater();
    }

    ~RecalcWordWrapThread() override
    {
        //Wait for thread to finish.
        if(isRunning())
            wait();
    }
};

QAtomicInt RecalcWordWrapThread::s_runnableCount = 0;
QAtomicInt RecalcWordWrapThread::s_maxNofRunnables = 0;

class WrapLineCacheData
{
  public:
    WrapLineCacheData() = default;
    WrapLineCacheData(qint32 d3LineIdx, qint32 textStart, qint32 textLength)
        : m_d3LineIdx(d3LineIdx), m_textStart(textStart), m_textLength(textLength) {}
    [[nodiscard]] qint32 d3LineIdx() const { return m_d3LineIdx; }
    [[nodiscard]] qint32 textStart() const { return m_textStart; }
    [[nodiscard]] qint32 textLength() const { return m_textLength; }

  private:
    qint32 m_d3LineIdx = 0;
    qint32 m_textStart = 0;
    qint32 m_textLength = 0;
};

class DiffTextWindowData
{
  public:
    explicit DiffTextWindowData(QPointer<DiffTextWindow> p)
    {
        m_pDiffTextWindow = p;
#if defined(Q_OS_WIN)
        m_eLineEndStyle = eLineEndStyleDos;
#else
        m_eLineEndStyle = eLineEndStyleUnix;
#endif
    }

    QString getString(LineIndex d3lIdx);
    QString getLineString(qint32 line);

    void writeLine(
        RLPainter& p, const LineData* pld,
        const std::shared_ptr<DiffList>& pLineDiff1, const std::shared_ptr<DiffList>& pLineDiff2, const LineRef& line,
        const ChangeFlags whatChanged, const ChangeFlags whatChanged2, const LineRef& srcLineIdx,
        qint32 wrapLineOffset, qint32 wrapLineLength, bool bWrapLine, const QRect& invalidRect);

    void draw(RLPainter& p, const QRect& invalidRect, const qint32 beginLine, const LineRef& endLine);

    void myUpdate(qint32 afterMilliSecs);

    [[nodiscard]] qint32 leftInfoWidth() const { return 4 + m_lineNumberWidth; } // Number of information columns on left side
    qint32 convertLineOnScreenToLineInSource(qint32 lineOnScreen, e_CoordType coordType, bool bFirstLine);

    void prepareTextLayout(QTextLayout& textLayout, qint32 visibleTextWidth = -1);

    [[nodiscard]] bool isThreeWay() const { return KDiff3App::isTripleDiff(); };
    const QString& getFileName() { return m_filename; }

    const Diff3LineVector* getDiff3LineVector() { return mDiff3LineVector; }

    const QSharedPointer<Options>& getOptions() { return m_pOptions; }

  private:
    //TODO: Remove friend classes after creating accessors. Please don't add new classes here
    friend DiffTextWindow;
    QPointer<DiffTextWindow> m_pDiffTextWindow;
    QTextCodec* m_pTextCodec = nullptr;
    e_LineEndStyle m_eLineEndStyle;

    std::shared_ptr<LineDataVector> m_pLineData;
    LineCount m_size = 0;
    QString m_filename;
    bool m_bWordWrap = false;
    qint32 m_delayedDrawTimer = 0;

    const Diff3LineVector* mDiff3LineVector = nullptr;
    Diff3WrapLineVector m_diff3WrapLineVector;
    const ManualDiffHelpList* m_pManualDiffHelpList = nullptr;
    QList<QVector<WrapLineCacheData>> m_wrapLineCacheList;

    QSharedPointer<Options> m_pOptions;
    QColor m_cThis;
    QColor m_cDiff1;
    QColor m_cDiff2;
    QColor m_cDiffBoth;

    qint32 m_fastSelectorLine1 = 0;
    qint32 m_fastSelectorNofLines = 0;

    e_SrcSelector m_winIdx = e_SrcSelector::None;
    qint32 m_firstLine = 0;
    qint32 m_oldFirstLine = -1;
    qint32 m_horizScrollOffset = 0;
    qint32 m_lineNumberWidth = 0;
    QAtomicInt m_maxTextWidth = -1;

    Selection m_selection;

    qint32 m_scrollDeltaX = 0;
    qint32 m_scrollDeltaY = 0;

    bool m_bMyUpdate = false;

    bool m_bSelectionInProgress = false;
    QPoint m_lastKnownMousePos;

    QSharedPointer<SourceData> sourceData;
};

QAtomicInteger<size_t> DiffTextWindow::maxThreads()
{
    return RecalcWordWrapThread::s_maxNofRunnables.loadAcquire();
}

void DiffTextWindow::setSourceData(const QSharedPointer<SourceData>& inData) { d->sourceData = inData; }
bool DiffTextWindow::isThreeWay() const { return d->isThreeWay(); };
const QString& DiffTextWindow::getFileName() const { return d->getFileName(); }

e_SrcSelector DiffTextWindow::getWindowIndex() const { return d->m_winIdx; };

const QString DiffTextWindow::getEncodingDisplayString() const { return d->m_pTextCodec != nullptr ? QLatin1String(d->m_pTextCodec->name()) : QString(); }
e_LineEndStyle DiffTextWindow::getLineEndStyle() const { return d->m_eLineEndStyle; }

const Diff3LineVector* DiffTextWindow::getDiff3LineVector() const { return d->getDiff3LineVector(); }

qint32 DiffTextWindow::getLineNumberWidth() const { return (qint32)floor(log10((double)std::max(d->m_size, 1))) + 1; }

DiffTextWindow::DiffTextWindow(DiffTextWindowFrame* pParent,
    const QSharedPointer<Options>& pOptions,
    e_SrcSelector winIdx,
    KDiff3App &app)
    : QWidget(pParent)
    , m_app(app)
{
    setObjectName(QString("DiffTextWindow%1").arg((qint32)winIdx));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setUpdatesEnabled(false);

    d = std::make_unique<DiffTextWindowData>(this);
    setFocusPolicy(Qt::ClickFocus);
    setAcceptDrops(true);

    d->m_pOptions = pOptions;
    init(QString(""), nullptr, d->m_eLineEndStyle, nullptr, 0, nullptr, nullptr);

    setMinimumSize(QSize(20, 20));

    setUpdatesEnabled(true);
    d->m_bWordWrap = false;
    d->m_winIdx = winIdx;

    setFont(d->getOptions()->defaultFont());
}

DiffTextWindow::~DiffTextWindow() = default;

void DiffTextWindow::init(
    const QString& filename,
    QTextCodec* pTextCodec,
    e_LineEndStyle eLineEndStyle,
    const std::shared_ptr<LineDataVector> &pLineData,
    LineCount size,
    const Diff3LineVector* pDiff3LineVector,
    const ManualDiffHelpList* pManualDiffHelpList)
{
    reset();

    d->m_filename = filename;
    d->m_pLineData = pLineData;
    d->m_size = size;
    d->mDiff3LineVector = pDiff3LineVector;
    d->m_diff3WrapLineVector.clear();
    d->m_pManualDiffHelpList = pManualDiffHelpList;

    d->m_firstLine = 0;
    d->m_oldFirstLine = -1;
    d->m_horizScrollOffset = 0;
    d->m_scrollDeltaX = 0;
    d->m_scrollDeltaY = 0;
    d->m_bMyUpdate = false;
    d->m_fastSelectorLine1 = 0;
    d->m_fastSelectorNofLines = 0;
    d->m_lineNumberWidth = 0;
    d->m_maxTextWidth = -1;

    d->m_pTextCodec = pTextCodec;
    d->m_eLineEndStyle = eLineEndStyle;

    update();
}

void DiffTextWindow::setupConnections(const KDiff3App* app)
{
    assert(qobject_cast<DiffTextWindowFrame*>(parent()) != nullptr);

    chk_connect(this, &DiffTextWindow::firstLineChanged, dynamic_cast<DiffTextWindowFrame*>(parent()), &DiffTextWindowFrame::setFirstLine);
    chk_connect(this, &DiffTextWindow::newSelection, app, &KDiff3App::slotSelectionStart);
    chk_connect(this, &DiffTextWindow::selectionEnd, app, &KDiff3App::slotSelectionEnd);
    chk_connect(this, &DiffTextWindow::scrollDiffTextWindow, app, &KDiff3App::scrollDiffTextWindow);
    chk_connect_q(this, &DiffTextWindow::finishRecalcWordWrap, app, &KDiff3App::slotFinishRecalcWordWrap);

    chk_connect(this, &DiffTextWindow::finishDrop, app, &KDiff3App::slotFinishDrop);

    chk_connect(this, &DiffTextWindow::statusBarMessage, app, &KDiff3App::slotStatusMsg);

    chk_connect(app, &KDiff3App::showWhiteSpaceToggled, this, static_cast<void (DiffTextWindow::*)(void)>(&DiffTextWindow::update));
    chk_connect(app, &KDiff3App::showLineNumbersToggled, this, static_cast<void (DiffTextWindow::*)(void)>(&DiffTextWindow::update));
    chk_connect(app, &KDiff3App::doRefresh, this, &DiffTextWindow::slotRefresh);
    chk_connect(app, &KDiff3App::selectAll, this, &DiffTextWindow::slotSelectAll);
    chk_connect(app, &KDiff3App::copy, this, &DiffTextWindow::slotCopy);

    connections.push_back(KDiff3App::allowCopy.connect(boost::bind(&DiffTextWindow::canCopy, this)));
    connections.push_back(KDiff3App::getSelection.connect(boost::bind(&DiffTextWindow::getSelection, this)));
}

void DiffTextWindow::reset()
{
    while(DiffTextWindow::maxThreads() > 0) {} //Clear word wrap threads.

    d->m_pLineData = nullptr;
    d->m_size = 0;
    d->mDiff3LineVector = nullptr;
    d->m_filename = "";
    d->m_diff3WrapLineVector.clear();
}

void DiffTextWindow::slotRefresh()
{
    setFont(d->getOptions()->defaultFont());
    update();
}

void DiffTextWindow::slotSelectAll()
{
    LineRef l;
    qint32 p = 0; // needed as dummy return values

    if(hasFocus())
    {
        setSelection(0, 0, getNofLines(), 0, l, p);
    }
}

void DiffTextWindow::slotCopy()
{
    if(!hasFocus())
        return;

    const QString curSelection = getSelection();

    if(!curSelection.isEmpty())
    {
        QApplication::clipboard()->setText(curSelection, QClipboard::Clipboard);
    }
}

void DiffTextWindow::setPaintingAllowed(bool bAllowPainting)
{
    if(updatesEnabled() != bAllowPainting)
    {
        setUpdatesEnabled(bAllowPainting);
        if(bAllowPainting)
            update();
    }
}

void DiffTextWindow::dragEnterEvent(QDragEnterEvent* dragEnterEvent)
{
    dragEnterEvent->setAccepted(dragEnterEvent->mimeData()->hasUrls() || dragEnterEvent->mimeData()->hasText());
}

void DiffTextWindow::dropEvent(QDropEvent* dropEvent)
{
    dropEvent->accept();

    if(dropEvent->mimeData()->hasUrls())
    {
        QList<QUrl> urlList = dropEvent->mimeData()->urls();

        if(m_app.canContinue() && !urlList.isEmpty())
        {
            FileAccess fa(urlList.first());
            if(fa.isDir())
                return;

            d->sourceData->setFileAccess(fa);

            Q_EMIT finishDrop();
        }
    }
    else if(dropEvent->mimeData()->hasText())
    {
        QString text = dropEvent->mimeData()->text();

        if(m_app.canContinue())
        {
            QString error;

            error = d->sourceData->setData(text);

            if(!error.isEmpty())
            {
                KMessageBox::error(this, error);
            }

            Q_EMIT finishDrop();
        }
    }
}

void DiffTextWindow::printWindow(RLPainter& painter, const QRect& view, const QString& headerText, qint32 line, const LineCount linesPerPage, const QColor& fgColor)
{
    QRect clipRect = view;
    clipRect.setTop(0);
    painter.setClipRect(clipRect);
    painter.translate(view.left(), 0);
    QFontMetrics fm = painter.fontMetrics();
    {
        qint32 lineHeight = fm.height() + fm.ascent();
        const QRectF headerRect(0, 5, view.width(), 3 * (lineHeight));
        QTextOption options;
        options.setWrapMode(QTextOption::WordWrap);
        // TODO: transition to Qt::LayoutDirectionAuto
        options.setTextDirection(Qt::LeftToRight);
        static_cast<QPainter&>(painter).drawText(headerRect, headerText, options);

        painter.setPen(fgColor);
        painter.drawLine(0, view.top() - 2, view.width(), view.top() - 2);
    }

    painter.translate(0, view.top());
    print(painter, view, line, linesPerPage);
    painter.resetTransform();
}

void DiffTextWindow::setFirstLine(qint32 firstLine)
{
    qint32 fontHeight = fontMetrics().lineSpacing();

    LineRef newFirstLine = std::max(0, firstLine);

    qint32 deltaY = fontHeight * (d->m_firstLine - newFirstLine);

    d->m_firstLine = newFirstLine;

    if(d->m_bSelectionInProgress && d->m_selection.isValidFirstLine())
    {
        LineRef line;
        qint32 pos;
        convertToLinePos(d->m_lastKnownMousePos.x(), d->m_lastKnownMousePos.y(), line, pos);
        d->m_selection.end(line, pos);
        update();
    }
    else
    {
        scroll(0, deltaY);
    }

    Q_EMIT firstLineChanged(d->m_firstLine);
}

qint32 DiffTextWindow::getFirstLine() const
{
    return d->m_firstLine;
}

void DiffTextWindow::setHorizScrollOffset(qint32 horizScrollOffset)
{
    d->m_horizScrollOffset = std::max(0, horizScrollOffset);

    if(d->m_bSelectionInProgress && d->m_selection.isValidFirstLine())
    {
        LineRef line;
        qint32 pos;
        convertToLinePos(d->m_lastKnownMousePos.x(), d->m_lastKnownMousePos.y(), line, pos);
        d->m_selection.end(line, pos);
    }

    update();
}

qint32 DiffTextWindow::getMaxTextWidth() const
{
    if(d->m_bWordWrap)
    {
        return getVisibleTextAreaWidth();
    }
    else if(getAtomic(d->m_maxTextWidth) < 0)
    {
        d->m_maxTextWidth = 0;
        QTextLayout textLayout(QString(), font(), this);
        for(qint32 i = 0; i < d->m_size; ++i)
        {
            textLayout.clearLayout();
            textLayout.setText(d->getString(i));
            d->prepareTextLayout(textLayout);
            if(textLayout.maximumWidth() > getAtomic(d->m_maxTextWidth))
                d->m_maxTextWidth = qCeil(textLayout.maximumWidth());
        }
    }
    return getAtomic(d->m_maxTextWidth);
}

//FIXME:not 64-bit size safe
LineCount DiffTextWindow::getNofLines() const
{
    return d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : d->getDiff3LineVector()->size();
}

LineIndex DiffTextWindow::convertLineToDiff3LineIdx(LineRef line)
{
    if(line.isValid() && d->m_bWordWrap && d->m_diff3WrapLineVector.size() > 0)
        return d->m_diff3WrapLineVector[std::min((LineRef::LineType)line, d->m_diff3WrapLineVector.size() - 1)].diff3LineIndex;
    else
        return line;
}

LineRef DiffTextWindow::convertDiff3LineIdxToLine(LineIndex d3lIdx)
{
    assert(d3lIdx >= 0);

    if(d->m_bWordWrap && d->getDiff3LineVector() != nullptr && d->getDiff3LineVector()->size() > 0)
        return (*d->getDiff3LineVector())[std::min(d3lIdx, d->getDiff3LineVector()->size() - 1)]->sumLinesNeededForDisplay();
    else
        return d3lIdx;
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
qint32 getBestFirstLine(qint32 line, qint32 nofLines, qint32 firstLine, qint32 visibleLines)
{
    qint32 newFirstLine = firstLine;
    if(line < firstLine || line + nofLines + 2 > firstLine + visibleLines)
    {
        if(nofLines > visibleLines || nofLines <= (2 * visibleLines / 3 - 1))
            newFirstLine = line - visibleLines / 3;
        else
            newFirstLine = line - (visibleLines - nofLines);
    }

    return newFirstLine;
}

void DiffTextWindow::setFastSelectorRange(qint32 line1, qint32 nofLines)
{
    d->m_fastSelectorLine1 = line1;
    d->m_fastSelectorNofLines = nofLines;
    if(isVisible())
    {
        qint32 newFirstLine = getBestFirstLine(
            convertDiff3LineIdxToLine(d->m_fastSelectorLine1),
            convertDiff3LineIdxToLine(d->m_fastSelectorLine1 + d->m_fastSelectorNofLines) - convertDiff3LineIdxToLine(d->m_fastSelectorLine1),
            d->m_firstLine,
            getNofVisibleLines());
        if(newFirstLine != d->m_firstLine)
        {
            scrollVertically(newFirstLine - d->m_firstLine);
        }

        update();
    }
}
/*
    Takes the line number estimated from mouse position and converts it to the actual line in
    the file. Then sets the status message accordingly.

    emits lineClicked signal.
*/
void DiffTextWindow::showStatusLine(const LineRef lineFromPos)
{
    qint32 d3lIdx = convertLineToDiff3LineIdx(lineFromPos);

    if(d->getDiff3LineVector() != nullptr && d3lIdx >= 0 && d3lIdx < (qint32)d->getDiff3LineVector()->size())
    {
        const Diff3Line* pD3l = (*d->getDiff3LineVector())[d3lIdx];
        if(pD3l != nullptr)
        {
            LineRef actualLine = pD3l->getLineInFile(d->m_winIdx);

            QString message;
            if(actualLine.isValid())
                message = i18n("File %1: Line %2", d->m_filename, actualLine + 1);
            else
                message = i18n("File %1: Line not available", d->m_filename);
            Q_EMIT statusBarMessage(message);

            Q_EMIT lineClicked(d->m_winIdx, actualLine);
        }
    }
}

void DiffTextWindow::scrollVertically(qint32 deltaY)
{
    mVScrollBar->setValue(mVScrollBar->value()  + deltaY);
}

void DiffTextWindow::focusInEvent(QFocusEvent* e)
{
    Q_EMIT gotFocus();
    QWidget::focusInEvent(e);
}

void DiffTextWindow::mousePressEvent(QMouseEvent* e)
{
    qCInfo(kdiffDiffTextWindow) << "mousePressEvent triggered";
    if(e->button() == Qt::LeftButton)
    {
        LineRef line;
        qint32 pos;
        convertToLinePos(e->x(), e->y(), line, pos);
        qCInfo(kdiffDiffTextWindow) << "Left Button detected,";
        qCDebug(kdiffDiffTextWindow) << "line = " << line << ", pos = " << pos;

        qint32 fontWidth = Utils::getHorizontalAdvance(fontMetrics(), '0');
        qint32 xOffset = d->leftInfoWidth() * fontWidth;

        if((!d->getOptions()->m_bRightToLeftLanguage && e->x() < xOffset) || (d->getOptions()->m_bRightToLeftLanguage && e->x() > width() - xOffset))
        {
            Q_EMIT setFastSelectorLine(convertLineToDiff3LineIdx(line));
            d->m_selection.reset(); // Disable current d->m_selection
        }
        else
        { // Selection
            resetSelection();
            d->m_selection.start(line, pos);
            d->m_selection.end(line, pos);
            d->m_bSelectionInProgress = true;
            d->m_lastKnownMousePos = e->pos();

            showStatusLine(line);
        }
    }
}

void DiffTextWindow::mouseDoubleClickEvent(QMouseEvent* e)
{
    qCInfo(kdiffDiffTextWindow) << "Mouse Double Clicked";
    qCDebug(kdiffDiffTextWindow) << "d->m_lastKnownMousePos = " << d->m_lastKnownMousePos << ", e->pos() = " << e->pos();
    qCDebug(kdiffDiffTextWindow) << "d->m_bSelectionInProgress = " << d->m_bSelectionInProgress;

    d->m_bSelectionInProgress = false;
    d->m_lastKnownMousePos = e->pos();
    if(e->button() == Qt::LeftButton)
    {
        LineRef line;
        qint32 pos;
        convertToLinePos(e->x(), e->y(), line, pos);
        qCInfo(kdiffDiffTextWindow) << "Left Button detected,";
        qCDebug(kdiffDiffTextWindow) << "line = " << line << ", pos = " << pos;

        // Get the string data of the current line
        QString s;
        if(d->m_bWordWrap)
        {
            if(!line.isValid() || line >= d->m_diff3WrapLineVector.size())
                return;
            const Diff3WrapLine& d3wl = d->m_diff3WrapLineVector[line];
            s = d->getString(d3wl.diff3LineIndex).mid(d3wl.wrapLineOffset, d3wl.wrapLineLength);
        }
        else
        {
            if(!line.isValid() || line >= d->getDiff3LineVector()->size())
                return;
            s = d->getString(line);
        }

        if(!s.isEmpty())
        {
            QtSizeType pos1, pos2;
            Utils::calcTokenPos(s, pos, pos1, pos2);

            resetSelection();
            d->m_selection.start(line, pos1);
            d->m_selection.end(line, pos2);
            update();
            // Q_EMIT d->m_selectionEnd() happens in the mouseReleaseEvent.
            showStatusLine(line);
        }
    }
}

void DiffTextWindow::mouseReleaseEvent(QMouseEvent* e)
{
    qCInfo(kdiffDiffTextWindow) << "Mouse Released";
    qCDebug(kdiffDiffTextWindow) << "d->m_lastKnownMousePos = " << d->m_lastKnownMousePos << ", e->pos() = " << e->pos();
    qCDebug(kdiffDiffTextWindow) << "d->m_bSelectionInProgress = " << d->m_bSelectionInProgress;

    d->m_bSelectionInProgress = false;
    d->m_lastKnownMousePos = e->pos();


    if(d->m_delayedDrawTimer)
        killTimer(d->m_delayedDrawTimer);
    d->m_delayedDrawTimer = 0;
    if(d->m_selection.isValidFirstLine())
    {
        qCInfo(kdiffDiffTextWindow) << "Ending selection.";
        Q_EMIT selectionEnd();
    }

    d->m_scrollDeltaX = 0;
    d->m_scrollDeltaY = 0;
}

void DiffTextWindow::mouseMoveEvent(QMouseEvent* e)
{//Handles selection highlighting.
    LineRef line;
    qint32 pos;

    qCInfo(kdiffDiffTextWindow) << "Mouse Moved";
    qCDebug(kdiffDiffTextWindow) << "d->m_lastKnownMousePos = " << d->m_lastKnownMousePos << ", e->pos() = " << e->pos();

    convertToLinePos(e->x(), e->y(), line, pos);
    d->m_lastKnownMousePos = e->pos();

    qCDebug(kdiffDiffTextWindow) << "line = " << line << ", pos = " << pos;

    if(d->m_selection.isValidFirstLine())
    {
        qCDebug(kdiffDiffTextWindow) << "d->m_selection.isValidFirstLine() = " << d->m_selection.isValidFirstLine();
        d->m_selection.end(line, pos);

        showStatusLine(line);

        // Scroll because mouse moved out of the window
        const QFontMetrics& fm = fontMetrics();
        qint32 fontWidth = Utils::getHorizontalAdvance(fm, '0');
        qint32 deltaX = 0;
        qint32 deltaY = 0;
        if(!d->getOptions()->m_bRightToLeftLanguage)
        {
            if(e->x() < d->leftInfoWidth() * fontWidth) deltaX = -1 - abs(e->x() - d->leftInfoWidth() * fontWidth) / fontWidth;
            if(e->x() > width()) deltaX = +1 + abs(e->x() - width()) / fontWidth;
        }
        else
        {
            if(e->x() > width() - 1 - d->leftInfoWidth() * fontWidth) deltaX = +1 + abs(e->x() - (width() - 1 - d->leftInfoWidth() * fontWidth)) / fontWidth;
            if(e->x() < fontWidth) deltaX = -1 - abs(e->x() - fontWidth) / fontWidth;
        }
        if(e->y() < 0) deltaY = -1 - (qint32)std::pow<qint32, qint32>(e->y(), 2) / (qint32)std::pow(fm.lineSpacing(), 2);
        if(e->y() > height()) deltaY = 1 + (qint32)std::pow(e->y() - height(), 2) / (qint32)std::pow(fm.lineSpacing(), 2);
        if((deltaX != 0 && d->m_scrollDeltaX != deltaX) || (deltaY != 0 && d->m_scrollDeltaY != deltaY))
        {
            d->m_scrollDeltaX = deltaX;
            d->m_scrollDeltaY = deltaY;
            Q_EMIT scrollDiffTextWindow(deltaX, deltaY);
            if(d->m_delayedDrawTimer)
                killTimer(d->m_delayedDrawTimer);
            d->m_delayedDrawTimer = startTimer(50);
        }
        else
        {
            d->m_scrollDeltaX = deltaX;
            d->m_scrollDeltaY = deltaY;
            d->myUpdate(0);
        }
    }
}

void DiffTextWindow::wheelEvent(QWheelEvent* pWheelEvent)
{
    QPoint delta = pWheelEvent->angleDelta();

    //Block diagonal scrolling easily generated unintentionally with track pads.
    if(delta.y() != 0 && abs(delta.y()) > abs(delta.x()) && mVScrollBar != nullptr)
    {
        pWheelEvent->accept();
        QCoreApplication::postEvent(mVScrollBar, new QWheelEvent(*pWheelEvent));
    }
}

void DiffTextWindowData::myUpdate(qint32 afterMilliSecs)
{
    if(m_delayedDrawTimer)
        m_pDiffTextWindow->killTimer(m_delayedDrawTimer);
    m_bMyUpdate = true;
    m_delayedDrawTimer = m_pDiffTextWindow->startTimer(afterMilliSecs);
}

void DiffTextWindow::timerEvent(QTimerEvent*)
{
    killTimer(d->m_delayedDrawTimer);
    d->m_delayedDrawTimer = 0;

    if(d->m_bMyUpdate)
    {
        qint32 fontHeight = fontMetrics().lineSpacing();

        if(d->m_selection.getOldLastLine().isValid())
        {
            qint32 lastLine;
            qint32 firstLine;
            if(d->m_selection.getOldFirstLine().isValid())
            {
                firstLine = min3(d->m_selection.getOldFirstLine(), d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
                lastLine = max3(d->m_selection.getOldFirstLine(), d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
            }
            else
            {
                firstLine = std::min(d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
                lastLine = std::max(d->m_selection.getLastLine(), d->m_selection.getOldLastLine());
            }
            qint32 y1 = (firstLine - d->m_firstLine) * fontHeight;
            qint32 y2 = std::min(height(), (lastLine - d->m_firstLine + 1) * fontHeight);

            if(y1 < height() && y2 > 0)
            {
                QRect invalidRect = QRect(0, y1 - 1, width(), y2 - y1 + fontHeight); // Some characters in exotic exceed the regular bottom.
                update(invalidRect);
            }
        }

        d->m_bMyUpdate = false;
    }

    if(d->m_scrollDeltaX != 0 || d->m_scrollDeltaY != 0)
    {
        QtSizeType newPos = d->m_selection.getLastPos() + d->m_scrollDeltaX;
        try
        {
            LineRef newLine = d->m_selection.getLastLine() + d->m_scrollDeltaY;

            d->m_selection.end(newLine, newPos > 0 ? newPos : 0);
        }
        catch(const std::system_error&)
        {
            d->m_selection.end(LineRef::invalid, newPos > 0 ? newPos : 0);
        }
        Q_EMIT scrollDiffTextWindow(d->m_scrollDeltaX, d->m_scrollDeltaY);
        killTimer(d->m_delayedDrawTimer);
        d->m_delayedDrawTimer = startTimer(50);
    }
}

void DiffTextWindow::resetSelection()
{
    qCInfo(kdiffDiffTextWindow) << "Resetting Selection";
    d->m_selection.reset();
    update();
}

void DiffTextWindow::convertToLinePos(qint32 x, qint32 y, LineRef& line, qint32& pos)
{
    const QFontMetrics& fm = fontMetrics();
    qint32 fontHeight = fm.lineSpacing();

    qint32 yOffset = d->m_firstLine * fontHeight;

    if((y + yOffset) >= 0)
        line = (y + yOffset) / fontHeight;
    else
        line = LineRef::invalid;

    if(line.isValid() && (!d->getOptions()->wordWrapOn() || line < d->m_diff3WrapLineVector.count()))
    {
        QString s = d->getLineString(line);
        QTextLayout textLayout(s, font(), this);
        d->prepareTextLayout(textLayout);
        pos = textLayout.lineAt(0).xToCursor(x - textLayout.position().x());
    }
    else
        pos = -1;
}

class FormatRangeHelper
{
  private:
    QFont m_font;
    QPen m_pen;
    QColor m_background;
    qint32 m_currentPos;

    QVector<QTextLayout::FormatRange> m_formatRanges;

  public:
    inline operator QVector<QTextLayout::FormatRange>() { return m_formatRanges; }
    FormatRangeHelper()
    {
        m_pen = QColor(Qt::black);
        m_background = QColor(Qt::white);
        m_currentPos = 0;
    }

    void setFont(const QFont& f)
    {
        m_font = f;
    }

    void setPen(const QPen& pen)
    {
        m_pen = pen;
    }

    void setBackground(const QColor& background)
    {
        m_background = background;
    }

    void next()
    {
        if(m_formatRanges.isEmpty() || m_formatRanges.back().format.foreground().color() != m_pen.color() || m_formatRanges.back().format.background().color() != m_background)
        {
            QTextLayout::FormatRange fr;
            fr.length = 1;
            fr.start = m_currentPos;
            fr.format.setForeground(m_pen.color());
            fr.format.setBackground(m_background);
            m_formatRanges.append(fr);
        }
        else
        {
            ++m_formatRanges.back().length;
        }
        ++m_currentPos;
    }
};

void DiffTextWindowData::prepareTextLayout(QTextLayout& textLayout, qint32 visibleTextWidth)
{
    QTextOption textOption;

    textOption.setTabStopDistance(QFontMetricsF(m_pDiffTextWindow->font()).horizontalAdvance(' ') * m_pOptions->m_tabSize);

    if(m_pOptions->m_bShowWhiteSpaceCharacters)
        textOption.setFlags(QTextOption::ShowTabsAndSpaces);
    if(m_pOptions->m_bRightToLeftLanguage)

    {
        textOption.setAlignment(Qt::AlignRight); // only relevant for multi line text layout
    }
    if(visibleTextWidth >= 0)
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    textLayout.setTextOption(textOption);

    if(m_pOptions->m_bShowWhiteSpaceCharacters)
    {
        // This additional format is only necessary for the tab arrow
        QVector<QTextLayout::FormatRange> formats;
        QTextLayout::FormatRange formatRange;
        formatRange.start = 0;
        formatRange.length = textLayout.text().length();
        formatRange.format.setFont(m_pDiffTextWindow->font());
        formats.append(formatRange);
        textLayout.setFormats(formats);
    }
    textLayout.beginLayout();

    qint32 leading = m_pDiffTextWindow->fontMetrics().leading();
    qint32 height = 0;

    qint32 fontWidth = Utils::getHorizontalAdvance(m_pDiffTextWindow->fontMetrics(), '0');
    qint32 xOffset = leftInfoWidth() * fontWidth - m_horizScrollOffset;
    qint32 textWidth = visibleTextWidth;
    if(textWidth < 0)
        textWidth = m_pDiffTextWindow->width() - xOffset;

    qint32 indentation = 0;
    while(true)
    {
        QTextLine line = textLayout.createLine();
        if(!line.isValid())
            break;

        height += leading;
        //if ( !bFirstLine )
        //   indentation =  Utils::getHorizontalAdvance(m_pDiffTextWindow->fontMetrics(), (' ')) * m_pOptions->m_tabSize;
        if(visibleTextWidth >= 0)
        {
            line.setLineWidth(visibleTextWidth - indentation);
            line.setPosition(QPointF(indentation, height));
            height += qCeil(line.height());
            //bFirstLine = false;
        }
        else // only one line
        {
            line.setPosition(QPointF(indentation, height));
            break;
        }
    }

    textLayout.endLayout();
    if(m_pOptions->m_bRightToLeftLanguage)
        textLayout.setPosition(QPointF(textWidth - textLayout.maximumWidth(), 0));
    else
        textLayout.setPosition(QPointF(xOffset, 0));
}

/*
    Don't try to use invalid rect to block drawing of lines based on there apparent horizontal dementions.
    This does not always work for very long lines being scrolled horizontally. (Causes blanking of diff text area)
*/
void DiffTextWindowData::writeLine(
    RLPainter& p,
    const LineData* pld,
    const std::shared_ptr<DiffList>& pLineDiff1,
    const std::shared_ptr<DiffList>& pLineDiff2,
    const LineRef& line,
    const ChangeFlags whatChanged,
    const ChangeFlags whatChanged2,
    const LineRef& srcLineIdx,
    qint32 wrapLineOffset,
    qint32 wrapLineLength,
    bool bWrapLine,
    const QRect& invalidRect)
{
    QFont normalFont = p.font();

    const QFontMetrics& fm = p.fontMetrics();
    qint32 fontHeight = fm.lineSpacing();
    qint32 fontAscent = fm.ascent();
    qint32 fontWidth = Utils::getHorizontalAdvance(fm, '0');

    qint32 xOffset = 0;
    qint32 yOffset = (line - m_firstLine) * fontHeight;

    qint32 fastSelectorLine1 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1);
    qint32 fastSelectorLine2 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1 + m_fastSelectorNofLines) - 1;

    bool bFastSelectionRange = (line >= fastSelectorLine1 && line <= fastSelectorLine2);
    QColor bgColor = m_pOptions->backgroundColor();
    QColor diffBgColor = m_pOptions->diffBackgroundColor();

    if(bFastSelectionRange)
    {
        bgColor = m_pOptions->m_currentRangeBgColor;
        diffBgColor = m_pOptions->m_currentRangeDiffBgColor;
    }

    if(yOffset + fontHeight < invalidRect.top() || invalidRect.bottom() < yOffset - fontHeight)
        return;

    ChangeFlags changed = whatChanged;
    if(pLineDiff1 != nullptr) changed |= AChanged;
    if(pLineDiff2 != nullptr) changed |= BChanged;

    QColor penColor = m_pOptions->forgroundColor();
    p.setPen(penColor);
    if(changed == BChanged)
    {
        penColor = m_cDiff2;
    }
    else if(changed == AChanged)
    {
        penColor = m_cDiff1;
    }
    else if(changed == Both)
    {
        penColor = m_cDiffBoth;
    }

    if(pld != nullptr)
    {
        // First calculate the "changed" information for each character.
        QtSizeType i = 0;
        QString lineString = pld->getLine();
        if(!lineString.isEmpty())
        {
            switch(lineString[lineString.length() - 1].unicode())
            {
                case '\n':
                    lineString[lineString.length() - 1] = 0x00B6;
                    break; // "Pilcrow", "paragraph mark"
                case '\r':
                    lineString[lineString.length() - 1] = 0x00A4;
                    break; // Currency sign ;0x2761 "curved stem paragraph sign ornament"
                //case '\0b' : lineString[lineString.length()-1] = 0x2756; break; // some other nice looking character
            }
        }
        QVector<ChangeFlags> charChanged(pld->size());
        Merger merger(pLineDiff1, pLineDiff2);
        while(!merger.isEndReached() && i < pld->size())
        {
            if(i < pld->size())
            {
                charChanged[i] = merger.whatChanged();
                ++i;
            }
            merger.next();
        }

        qint32 outPos = 0;

        QtSizeType lineLength = m_bWordWrap ? wrapLineOffset + wrapLineLength : lineString.length();

        FormatRangeHelper frh;

        for(i = wrapLineOffset; i < lineLength; ++i)
        {
            penColor = m_pOptions->forgroundColor();
            ChangeFlags cchanged = charChanged[i] | whatChanged;

            if(cchanged == BChanged)
            {
                penColor = m_cDiff2;
            }
            else if(cchanged == AChanged)
            {
                penColor = m_cDiff1;
            }
            else if(cchanged == Both)
            {
                penColor = m_cDiffBoth;
            }

            if(penColor != m_pOptions->forgroundColor() && whatChanged2 == NoChange && !m_pOptions->m_bShowWhiteSpace)
            {
                // The user doesn't want to see highlighted white space.
                penColor = m_pOptions->forgroundColor();
            }

            frh.setBackground(bgColor);
            if(!m_selection.within(line, outPos))
            {
                if(penColor != m_pOptions->forgroundColor())
                {
                    frh.setBackground(diffBgColor);
                    // Setting italic font here doesn't work: Changing the font only when drawing is too late
                }

                frh.setPen(penColor);
                frh.next();
                frh.setFont(normalFont);
            }
            else
            {
                frh.setBackground(m_pDiffTextWindow->palette().highlight().color());
                frh.setPen(m_pDiffTextWindow->palette().highlightedText().color());
                frh.next();

                m_selection.bSelectionContainsData = true;
            }

            ++outPos;
        } // end for

        QTextLayout textLayout(lineString.mid(wrapLineOffset, lineLength - wrapLineOffset), m_pDiffTextWindow->font(), m_pDiffTextWindow);
        prepareTextLayout(textLayout);
        textLayout.draw(&p, QPoint(0, yOffset), frh /*, const QRectF & clip = QRectF() */);
    }

    p.fillRect(0, yOffset, leftInfoWidth() * fontWidth, fontHeight, m_pOptions->backgroundColor());

    xOffset = (m_lineNumberWidth + 2) * fontWidth;
    qint32 xLeft = m_lineNumberWidth * fontWidth;
    p.setPen(m_pOptions->forgroundColor());
    if(pld != nullptr)
    {
        if(m_pOptions->m_bShowLineNumbers && !bWrapLine)
        {
            QString num = QString::number(srcLineIdx + 1);
            assert(!num.isEmpty());
            p.drawText(0, yOffset + fontAscent, num);
        }
        if(!bWrapLine || wrapLineLength > 0)
        {
            Qt::PenStyle wrapLinePenStyle = Qt::DotLine;

            p.setPen(QPen(m_pOptions->forgroundColor(), 0, bWrapLine ? wrapLinePenStyle : Qt::SolidLine));
            p.drawLine(xOffset + 1, yOffset, xOffset + 1, yOffset + fontHeight - 1);
            p.setPen(QPen(m_pOptions->forgroundColor(), 0, Qt::SolidLine));
        }
    }
    if(penColor != m_pOptions->forgroundColor() && whatChanged2 == NoChange)
    {
        if(m_pOptions->m_bShowWhiteSpace)
        {
            p.setBrushOrigin(0, 0);
            p.fillRect(xLeft, yOffset, fontWidth * 2 - 1, fontHeight, QBrush(penColor, Qt::Dense5Pattern));
        }
    }
    else
    {
        p.fillRect(xLeft, yOffset, fontWidth * 2 - 1, fontHeight, penColor == m_pOptions->forgroundColor() ? bgColor : penColor);
    }

    if(bFastSelectionRange)
    {
        p.fillRect(xOffset + fontWidth - 1, yOffset, 3, fontHeight, m_pOptions->forgroundColor());
    }

    // Check if line needs a manual diff help mark
    ManualDiffHelpList::const_iterator ci;
    for(ci = m_pManualDiffHelpList->begin(); ci != m_pManualDiffHelpList->end(); ++ci)
    {
        const ManualDiffHelpEntry& mdhe = *ci;
        LineRef rangeLine1;
        LineRef rangeLine2;

        mdhe.getRangeForUI(m_winIdx, &rangeLine1, &rangeLine2);
        if(rangeLine1.isValid() && rangeLine2.isValid() && srcLineIdx >= rangeLine1 && srcLineIdx <= rangeLine2)
        {
            p.fillRect(xOffset - fontWidth, yOffset, fontWidth - 1, fontHeight, m_pOptions->m_manualHelpRangeColor);
            break;
        }
    }
}

void DiffTextWindow::paintEvent(QPaintEvent* e)
{
    QRect invalidRect = e->rect();
    if(invalidRect.isEmpty())
        return;

    if(d->getDiff3LineVector() == nullptr || (d->m_diff3WrapLineVector.empty() && d->m_bWordWrap))
    {
        QPainter p(this);
        p.fillRect(invalidRect, d->getOptions()->backgroundColor());
        return;
    }

    bool bOldSelectionContainsData = d->m_selection.bSelectionContainsData;
    d->m_selection.bSelectionContainsData = false;

    LineRef endLine = std::min(d->m_firstLine + getNofVisibleLines() + 2, getNofLines());

    RLPainter p(this, d->getOptions()->m_bRightToLeftLanguage, width(), Utils::getHorizontalAdvance(fontMetrics(), '0'));

    p.setFont(font());
    p.QPainter::fillRect(invalidRect, d->getOptions()->backgroundColor());

    d->draw(p, invalidRect, d->m_firstLine, endLine);
    p.end();

    d->m_oldFirstLine = d->m_firstLine;
    d->m_selection.clearOldSelection();

    if(!bOldSelectionContainsData && d->m_selection.selectionContainsData())
        Q_EMIT newSelection();
}

void DiffTextWindow::print(RLPainter& p, const QRect&, qint32 firstLine, const LineCount nofLinesPerPage)
{
    if(d->getDiff3LineVector() == nullptr || !updatesEnabled() ||
       (d->m_diff3WrapLineVector.empty() && d->m_bWordWrap))
        return;
    resetSelection();
    qint32 oldFirstLine = d->m_firstLine;
    d->m_firstLine = firstLine;
    QRect invalidRect = QRect(0, 0, 1000000000, 1000000000);
    d->getOptions()->beginPrint();
    d->draw(p, invalidRect, firstLine, std::min(firstLine + nofLinesPerPage, getNofLines()));
    d->getOptions()->endPrint();
    d->m_firstLine = oldFirstLine;
}

void DiffTextWindowData::draw(RLPainter& p, const QRect& invalidRect, const qint32 beginLine, const LineRef& endLine)
{
    if(m_pLineData == nullptr || m_pLineData->empty()) return;
    m_lineNumberWidth = m_pOptions->m_bShowLineNumbers ? m_pDiffTextWindow->getLineNumberWidth() : 0;

    if(m_winIdx == e_SrcSelector::A)
    {
        m_cThis = m_pOptions->aColor();
        m_cDiff1 = m_pOptions->bColor();
        m_cDiff2 = m_pOptions->cColor();
    }
    else if(m_winIdx == e_SrcSelector::B)
    {
        m_cThis = m_pOptions->bColor();
        m_cDiff1 = m_pOptions->cColor();
        m_cDiff2 = m_pOptions->aColor();
    }
    else
        if(m_winIdx == e_SrcSelector::C)
    {
        m_cThis = m_pOptions->cColor();
        m_cDiff1 = m_pOptions->aColor();
        m_cDiff2 = m_pOptions->bColor();
    }
    m_cDiffBoth = m_pOptions->conflictColor(); // Conflict color

    p.setPen(m_cThis);

    for(qint32 line = beginLine; line < endLine; ++line)
    {
        qint32 wrapLineOffset = 0;
        qint32 wrapLineLength = 0;
        const Diff3Line* d3l = nullptr;
        bool bWrapLine = false;
        if(m_bWordWrap)
        {
            Diff3WrapLine& d3wl = m_diff3WrapLineVector[line];
            wrapLineOffset = d3wl.wrapLineOffset;
            wrapLineLength = d3wl.wrapLineLength;
            d3l = d3wl.pD3L;
            bWrapLine = line > 0 && m_diff3WrapLineVector[line - 1].pD3L == d3l;
        }
        else
        {
            d3l = (*mDiff3LineVector)[line];
        }
        std::shared_ptr<DiffList> pFineDiff1;
        std::shared_ptr<DiffList> pFineDiff2;
        ChangeFlags changed = NoChange;
        ChangeFlags changed2 = NoChange;

        LineRef srcLineIdx;
        d3l->getLineInfo(m_winIdx, KDiff3App::isTripleDiff(), srcLineIdx, pFineDiff1, pFineDiff2, changed, changed2);

        writeLine(
            p,                                                             // QPainter
            !srcLineIdx.isValid() ? nullptr : &(*m_pLineData)[srcLineIdx], // Text in this line
            pFineDiff1,
            pFineDiff2,
            line, // Line on the screen
            changed,
            changed2,
            srcLineIdx,
            wrapLineOffset,
            wrapLineLength,
            bWrapLine,
            invalidRect);
    }
}

QString DiffTextWindowData::getString(LineIndex d3lIdx)
{
    assert(!(m_pLineData != nullptr && m_pLineData->empty() && m_size != 0));

    if(m_pLineData == nullptr || m_pLineData->empty() || d3lIdx < 0 || d3lIdx >= mDiff3LineVector->size())
        return QString();

    const Diff3Line* d3l = (*mDiff3LineVector)[d3lIdx];
    const LineRef lineIdx = d3l->getLineIndex(m_winIdx);

    if(!lineIdx.isValid())
        return QString();

    return (*m_pLineData)[lineIdx].getLine();
}

QString DiffTextWindowData::getLineString(qint32 line)
{
    if(m_bWordWrap)
    {
        if(line < m_diff3WrapLineVector.count())
        {
            qint32 d3LIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx(line);
            return getString(d3LIdx).mid(m_diff3WrapLineVector[line].wrapLineOffset, m_diff3WrapLineVector[line].wrapLineLength);
        }
        else
            return QString();
    }
    else
    {
        return getString(line);
    }
}

void DiffTextWindow::resizeEvent(QResizeEvent* e)
{
    QSize newSize = e->size();
    QFontMetrics fm = fontMetrics();
    qint32 visibleLines = newSize.height() / fm.lineSpacing() - 2;
    qint32 visibleColumns = newSize.width() / Utils::getHorizontalAdvance(fm, '0') - d->leftInfoWidth();

    if(e->size().height() != e->oldSize().height())
        Q_EMIT resizeHeightChangedSignal(visibleLines);
    if(e->size().width() != e->oldSize().width())
        Q_EMIT resizeWidthChangedSignal(visibleColumns);
    QWidget::resizeEvent(e);
}

LineCount DiffTextWindow::getNofVisibleLines() const
{
    QFontMetrics fm = fontMetrics();

    return height() / fm.lineSpacing() - 1;
}

qint32 DiffTextWindow::getVisibleTextAreaWidth() const
{
    QFontMetrics fm = fontMetrics();

    return width() - d->leftInfoWidth() * Utils::getHorizontalAdvance(fm, '0');
}

QString DiffTextWindow::getSelection() const
{
    if(d->m_pLineData == nullptr)
        return QString();

    QString selectionString;

    qint32 line = 0;
    qint32 lineIdx = 0;

    qint32 it;
    qint32 vectorSize = d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : d->getDiff3LineVector()->size();
    for(it = 0; it < vectorSize; ++it)
    {
        const Diff3Line* d3l = d->m_bWordWrap ? d->m_diff3WrapLineVector[it].pD3L : (*d->getDiff3LineVector())[it];

        assert(d->m_winIdx >= e_SrcSelector::A && d->m_winIdx <= e_SrcSelector::C);

        if(d->m_winIdx == e_SrcSelector::A)
        {
            lineIdx = d3l->getLineA();
        }
        else if(d->m_winIdx == e_SrcSelector::B)
        {
            lineIdx = d3l->getLineB();
        }
        else if(d->m_winIdx == e_SrcSelector::C)
        {
            lineIdx = d3l->getLineC();
        }

        if(lineIdx != -1)
        {
            QtSizeType size = (*d->m_pLineData)[lineIdx].size();
            QString lineString = (*d->m_pLineData)[lineIdx].getLine();

            if(d->m_bWordWrap)
            {
                size = d->m_diff3WrapLineVector[it].wrapLineLength;
                lineString = lineString.mid(d->m_diff3WrapLineVector[it].wrapLineOffset, size);
            }

            for(QtSizeType i = 0; i < size; ++i)
            {
                if(d->m_selection.within(line, i))
                {
                    selectionString += lineString[i];
                }
            }

            if(d->m_selection.within(line, size) &&
               !(d->m_bWordWrap && it + 1 < vectorSize && d3l == d->m_diff3WrapLineVector[it + 1].pD3L))
            {
#if defined(Q_OS_WIN)
                selectionString += '\r';
#endif
                selectionString += '\n';
            }
        }

        ++line;
    }

    return selectionString;
}
//FIXME:64-bit size safe?
bool DiffTextWindow::findString(const QString& s, LineRef& d3vLine, qint32& posInLine, bool bDirDown, bool bCaseSensitive)
{
    LineRef it = d3vLine;
    QtSizeType endIt = bDirDown ? d->getDiff3LineVector()->size() : -1;
    qint32 step = bDirDown ? 1 : -1;
    qint32 startPos = posInLine;

    for(; it != endIt; it += step)
    {
        QString line = d->getString(it);
        if(!line.isEmpty())
        {
            QtSizeType pos = line.indexOf(s, startPos, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            if(pos != -1)
            {
                d3vLine = it;
                posInLine = pos;
                return true;
            }

            startPos = 0;
        }
    }
    return false;
}

void DiffTextWindow::convertD3LCoordsToLineCoords(LineIndex d3LIdx, qint32 d3LPos, LineRef& line, qint32& pos)
{
    if(d->m_bWordWrap)
    {
        qint32 wrapPos = d3LPos;
        LineRef wrapLine = convertDiff3LineIdxToLine(d3LIdx);
        while(wrapPos > d->m_diff3WrapLineVector[wrapLine].wrapLineLength)
        {
            wrapPos -= d->m_diff3WrapLineVector[wrapLine].wrapLineLength;
            ++wrapLine;
        }
        pos = wrapPos;
        line = wrapLine;
    }
    else
    {
        pos = d3LPos;
        line = d3LIdx;
    }
}

void DiffTextWindow::convertLineCoordsToD3LCoords(LineRef line, qint32 pos, LineIndex& d3LIdx, qint32& d3LPos)
{
    if(d->m_bWordWrap && !d->m_diff3WrapLineVector.empty())
    {
        d3LPos = pos;
        d3LIdx = convertLineToDiff3LineIdx(line);
        LineRef wrapLine = convertDiff3LineIdxToLine(d3LIdx); // First wrap line belonging to this d3LIdx
        while(wrapLine < line)
        {
            d3LPos += d->m_diff3WrapLineVector[wrapLine].wrapLineLength;
            ++wrapLine;
        }
    }
    else
    {
        d3LPos = pos;
        d3LIdx = line;
    }
}

void DiffTextWindow::setSelection(LineRef firstLine, qint32 startPos, LineRef lastLine, qint32 endPos, LineRef& l, qint32& p)
{
    d->m_selection.reset();
    if(lastLine >= getNofLines())
    {
        lastLine = getNofLines() - 1;

        const Diff3Line* d3l = (*d->getDiff3LineVector())[convertLineToDiff3LineIdx(lastLine)];
        LineRef line;
        if(d->m_winIdx == e_SrcSelector::A) line = d3l->getLineA();
        if(d->m_winIdx == e_SrcSelector::B) line = d3l->getLineB();
        if(d->m_winIdx == e_SrcSelector::C) line = d3l->getLineC();
        if(line.isValid())
            endPos = (*d->m_pLineData)[line].width(d->getOptions()->m_tabSize);
    }

    if(d->m_bWordWrap && d->getDiff3LineVector() != nullptr)
    {
        QString s1 = d->getString(firstLine);
        qint32 firstWrapLine = convertDiff3LineIdxToLine(firstLine);
        qint32 wrapStartPos = startPos;
        while(wrapStartPos > d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength)
        {
            wrapStartPos -= d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength;
            s1 = s1.mid(d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength);
            ++firstWrapLine;
        }

        QString s2 = d->getString(lastLine);
        qint32 lastWrapLine = convertDiff3LineIdxToLine(lastLine);
        qint32 wrapEndPos = endPos;
        while(wrapEndPos > d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength)
        {
            wrapEndPos -= d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength;
            s2 = s2.mid(d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength);
            ++lastWrapLine;
        }

        d->m_selection.start(firstWrapLine, wrapStartPos);
        d->m_selection.end(lastWrapLine, wrapEndPos);
        l = firstWrapLine;
        p = wrapStartPos;
    }
    else
    {
        if(d->getDiff3LineVector() != nullptr)
        {
            d->m_selection.start(firstLine, startPos);
            d->m_selection.end(lastLine, endPos);
            l = firstLine;
            p = startPos;
        }
    }
    update();
}
//FIXME: Comparison LineIndex and QtSizeType?
qint32 DiffTextWindowData::convertLineOnScreenToLineInSource(qint32 lineOnScreen, e_CoordType coordType, bool bFirstLine)
{
    LineRef line;
    if(lineOnScreen >= 0)
    {
        if(coordType == eWrapCoords) return lineOnScreen;
        LineIndex d3lIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx(lineOnScreen);
        if(!bFirstLine && d3lIdx >= mDiff3LineVector->size())
            d3lIdx = mDiff3LineVector->size() - 1;
        if(coordType == eD3LLineCoords) return d3lIdx;
        while(!line.isValid() && d3lIdx < mDiff3LineVector->size() && d3lIdx >= 0)
        {
            const Diff3Line* d3l = (*mDiff3LineVector)[d3lIdx];
            if(m_winIdx == e_SrcSelector::A) line = d3l->getLineA();
            if(m_winIdx == e_SrcSelector::B) line = d3l->getLineB();
            if(m_winIdx == e_SrcSelector::C) line = d3l->getLineC();
            if(bFirstLine)
                ++d3lIdx;
            else
                --d3lIdx;
        }
        assert(coordType == eFileCoords);
    }
    return line;
}

void DiffTextWindow::getSelectionRange(LineRef* pFirstLine, LineRef* pLastLine, e_CoordType coordType)
{
    if(pFirstLine)
        *pFirstLine = d->convertLineOnScreenToLineInSource(d->m_selection.beginLine(), coordType, true);
    if(pLastLine)
        *pLastLine = d->convertLineOnScreenToLineInSource(d->m_selection.endLine(), coordType, false);
}

void DiffTextWindow::convertSelectionToD3LCoords()
{
    if(d->getDiff3LineVector() == nullptr || !updatesEnabled() || !isVisible() || d->m_selection.isEmpty())
    {
        return;
    }

    // convert the d->m_selection to unwrapped coordinates: Later restore to new coords
    qint32 firstD3LIdx, firstD3LPos;
    QString s = d->getLineString(d->m_selection.beginLine());
    qint32 firstPosInText = d->m_selection.beginPos();
    convertLineCoordsToD3LCoords(d->m_selection.beginLine(), firstPosInText, firstD3LIdx, firstD3LPos);

    qint32 lastD3LIdx, lastD3LPos;
    s = d->getLineString(d->m_selection.endLine());
    qint32 lastPosInText = d->m_selection.endPos();
    convertLineCoordsToD3LCoords(d->m_selection.endLine(), lastPosInText, lastD3LIdx, lastD3LPos);

    d->m_selection.start(firstD3LIdx, firstD3LPos);
    d->m_selection.end(lastD3LIdx, lastD3LPos);
}

bool DiffTextWindow::startRunnables()
{
    assert(RecalcWordWrapThread::s_maxNofRunnables == 0);

    if(s_runnables.count() == 0)
    {
        return false;
    }
    else
    {
        g_pProgressDialog->setStayHidden(true);
        ProgressProxy::startBackgroundTask();
        g_pProgressDialog->setMaxNofSteps(s_runnables.count());
        RecalcWordWrapThread::s_maxNofRunnables = s_runnables.count();
        g_pProgressDialog->setCurrent(0);

        for(qint32 i = 0; i < s_runnables.count(); ++i)
        {
            s_runnables[i]->start();
        }

        s_runnables.clear();
        return true;
    }
}

void DiffTextWindow::recalcWordWrap(bool bWordWrap, QtSizeType wrapLineVectorSize, qint32 visibleTextWidth)
{
    if(d->getDiff3LineVector() == nullptr || !isVisible())
    {
        d->m_bWordWrap = bWordWrap;
        if(!bWordWrap) d->m_diff3WrapLineVector.resize(0);
        return;
    }

    d->m_bWordWrap = bWordWrap;

    if(bWordWrap)
    {
        d->m_lineNumberWidth = d->getOptions()->m_bShowLineNumbers ? getLineNumberWidth() : 0;

        d->m_diff3WrapLineVector.resize(wrapLineVectorSize);

        if(wrapLineVectorSize == 0)
        {
            d->m_wrapLineCacheList.clear();
            setUpdatesEnabled(false);
            for(QtSizeType i = 0, j = 0; i < d->getDiff3LineVector()->size(); i += s_linesPerRunnable, ++j)
            {
                d->m_wrapLineCacheList.append(QVector<WrapLineCacheData>());
                s_runnables.push_back(new RecalcWordWrapThread(this, visibleTextWidth, j));
            }
        }
        else
        {
            recalcWordWrapHelper(wrapLineVectorSize, visibleTextWidth, 0);
            setUpdatesEnabled(true);
        }
    }
    else
    {
        if(wrapLineVectorSize == 0 && getAtomic(d->m_maxTextWidth) < 0)
        {
            d->m_diff3WrapLineVector.resize(0);
            d->m_wrapLineCacheList.clear();
            setUpdatesEnabled(false);
            for(qint32 i = 0, j = 0; i < d->getDiff3LineVector()->size(); i += s_linesPerRunnable, ++j)
            {
                s_runnables.push_back(new RecalcWordWrapThread(this, visibleTextWidth, j));
            }
        }
        else
        {
            setUpdatesEnabled(true);
        }
    }
}

void DiffTextWindow::recalcWordWrapHelper(QtSizeType wrapLineVectorSize, qint32 visibleTextWidth, QtSizeType cacheListIdx)
{
    if(d->m_bWordWrap)
    {
        if(ProgressProxy::wasCancelled())
            return;
        if(visibleTextWidth < 0)
            visibleTextWidth = getVisibleTextAreaWidth();
        else
            visibleTextWidth -= d->leftInfoWidth() * Utils::getHorizontalAdvance(fontMetrics(), '0');
        LineIndex i;
        QtSizeType wrapLineIdx = 0;
        QtSizeType size = d->getDiff3LineVector()->size();
        //FIXME: Safe for 64-bit QtSizeType?
        LineIndex firstD3LineIdx = wrapLineVectorSize > 0 ? 0 : cacheListIdx * s_linesPerRunnable;
        LineIndex endIdx = wrapLineVectorSize > 0 ? size : std::min(firstD3LineIdx + s_linesPerRunnable, size);
        QVector<WrapLineCacheData>& wrapLineCache = d->m_wrapLineCacheList[cacheListIdx];
        QtSizeType cacheListIdx2 = 0;
        QTextLayout textLayout(QString(), font(), this);

        for(i = firstD3LineIdx; i < endIdx; ++i)
        {
            if(ProgressProxy::wasCancelled())
                return;

            LineCount linesNeeded = 0;
            if(wrapLineVectorSize == 0)
            {
                QString s = d->getString(i);
                textLayout.clearLayout();
                textLayout.setText(s);
                d->prepareTextLayout(textLayout, visibleTextWidth);
                linesNeeded = textLayout.lineCount();
                for(qint32 l = 0; l < linesNeeded; ++l)
                {
                    QTextLine line = textLayout.lineAt(l);
                    wrapLineCache.push_back(WrapLineCacheData(i, line.textStart(), line.textLength()));
                }
            }
            else if(wrapLineVectorSize > 0 && cacheListIdx2 < d->m_wrapLineCacheList.count())
            {
                WrapLineCacheData* pWrapLineCache = d->m_wrapLineCacheList[cacheListIdx2].data();
                QtSizeType cacheIdx = 0;
                QtSizeType clc = d->m_wrapLineCacheList.count() - 1;
                QtSizeType cllc = d->m_wrapLineCacheList.last().count();
                QtSizeType curCount = d->m_wrapLineCacheList[cacheListIdx2].count() - 1;
                LineCount l = 0;

                while(wrapLineIdx + l < d->m_diff3WrapLineVector.size() && (cacheListIdx2 < clc || (cacheListIdx2 == clc && cacheIdx < cllc)) && pWrapLineCache->d3LineIdx() <= i)
                {
                    if(pWrapLineCache->d3LineIdx() == i)
                    {
                        Diff3WrapLine* pDiff3WrapLine = &d->m_diff3WrapLineVector[wrapLineIdx + l];
                        pDiff3WrapLine->wrapLineOffset = pWrapLineCache->textStart();
                        pDiff3WrapLine->wrapLineLength = pWrapLineCache->textLength();
                        ++l;
                    }
                    if(cacheIdx < curCount)
                    {
                        ++cacheIdx;
                        ++pWrapLineCache;
                    }
                    else
                    {
                        ++cacheListIdx2;
                        if(cacheListIdx2 >= d->m_wrapLineCacheList.count())
                            break;
                        pWrapLineCache = d->m_wrapLineCacheList[cacheListIdx2].data();
                        curCount = d->m_wrapLineCacheList[cacheListIdx2].count();
                        cacheIdx = 0;
                    }
                }
                linesNeeded = l;
            }

            Diff3Line& d3l = *(*d->getDiff3LineVector())[i];
            if(d3l.linesNeededForDisplay() < linesNeeded)
            {
                assert(wrapLineVectorSize == 0);
                d3l.setLinesNeeded(linesNeeded);
            }

            if(wrapLineVectorSize > 0)
            {
                qint32 j;
                for(j = 0; wrapLineIdx < d->m_diff3WrapLineVector.size() && j < d3l.linesNeededForDisplay(); ++j, ++wrapLineIdx)
                {
                    Diff3WrapLine& d3wl = d->m_diff3WrapLineVector[wrapLineIdx];
                    d3wl.diff3LineIndex = i;
                    d3wl.pD3L = (*d->getDiff3LineVector())[i];
                    if(j >= linesNeeded)
                    {
                        d3wl.wrapLineOffset = 0;
                        d3wl.wrapLineLength = 0;
                    }
                }

                if(wrapLineIdx >= d->m_diff3WrapLineVector.size())
                    break;
            }
        }

        if(wrapLineVectorSize > 0)
        {
            d->m_firstLine = std::min(d->m_firstLine, wrapLineVectorSize - 1);
            d->m_horizScrollOffset = 0;

            Q_EMIT firstLineChanged(d->m_firstLine);
        }
    }
    else // no word wrap, just calc the maximum text width
    {
        if(ProgressProxy::wasCancelled())
            return;
        //FIXME: 64-bit size safe?
        QtSizeType size = d->getDiff3LineVector()->size();
        LineIndex firstD3LineIdx = cacheListIdx * s_linesPerRunnable;
        LineIndex endIdx = std::min(firstD3LineIdx + s_linesPerRunnable, size);

        qint32 maxTextWidth = getAtomic(d->m_maxTextWidth); // current value
        QTextLayout textLayout(QString(), font(), this);
        for(LineIndex i = firstD3LineIdx; i < endIdx; ++i)
        {
            if(ProgressProxy::wasCancelled())
                return;
            textLayout.clearLayout();
            textLayout.setText(d->getString(i));
            d->prepareTextLayout(textLayout);
            if(textLayout.maximumWidth() > maxTextWidth)
                maxTextWidth = qCeil(textLayout.maximumWidth());
        }

        for(;;)
        {
            qint32 prevMaxTextWidth = d->m_maxTextWidth.fetchAndStoreOrdered(maxTextWidth);
            if(prevMaxTextWidth <= maxTextWidth)
                break;
            maxTextWidth = prevMaxTextWidth;
        }
    }

    if(!d->m_selection.isEmpty() && (!d->m_bWordWrap || wrapLineVectorSize > 0))
    {
        // Assume unwrapped coordinates
        //( Why? ->Conversion to unwrapped coords happened a few lines above in this method.
        //  Also see KDiff3App::recalcWordWrap() on the role of wrapLineVectorSize)

        // Wrap them now.

        // convert the d->m_selection to unwrapped coordinates.
        LineRef firstLine;
        qint32  firstPos;
        convertD3LCoordsToLineCoords(d->m_selection.beginLine(), d->m_selection.beginPos(), firstLine, firstPos);

        LineRef lastLine;
        qint32  lastPos;
        convertD3LCoordsToLineCoords(d->m_selection.endLine(), d->m_selection.endPos(), lastLine, lastPos);

        d->m_selection.start(firstLine, firstPos);
        d->m_selection.end(lastLine, lastPos);
    }
}

class DiffTextWindowFrameData
{
  public:
    DiffTextWindowFrameData(DiffTextWindowFrame* frame, const QSharedPointer<Options>& pOptions, const e_SrcSelector winIdx)
    {
        m_winIdx = winIdx;

        m_pOptions = pOptions;
        m_pTopLineWidget = new QWidget(frame);
        m_pFileSelection = new FileNameLineEdit(m_pTopLineWidget);
        m_pBrowseButton = new QPushButton("...", m_pTopLineWidget);
        m_pBrowseButton->setFixedWidth(30);

        m_pFileSelection->setAcceptDrops(true);

        m_pLabel = new QLabel("A:", m_pTopLineWidget);
        m_pTopLine = new QLabel(m_pTopLineWidget);
    }

    [[nodiscard]] const QPushButton* getBrowseButton() const { return m_pBrowseButton; }
    [[nodiscard]] const FileNameLineEdit* getFileSelectionField() const { return m_pFileSelection; }
    [[nodiscard]] const QWidget* getTopLineWidget() const { return m_pTopLineWidget; }
    [[nodiscard]] const QLabel* getLabel() const { return m_pLabel; }

    const QSharedPointer<Options> getOptions() { return m_pOptions; }

  private:
    friend DiffTextWindowFrame;
    QPointer<DiffTextWindow> m_pDiffTextWindow;
    FileNameLineEdit* m_pFileSelection;
    QPushButton* m_pBrowseButton;
    QSharedPointer<Options> m_pOptions;
    QLabel* m_pLabel;
    QLabel* m_pTopLine;
    QLabel* m_pEncoding;
    QLabel* m_pLineEndStyle;
    QWidget* m_pTopLineWidget;
    e_SrcSelector m_winIdx;

    QSharedPointer<SourceData> mSourceData;
};

DiffTextWindowFrame::DiffTextWindowFrame(QWidget* pParent, const QSharedPointer<Options>& pOptions, e_SrcSelector winIdx, const QSharedPointer<SourceData>& psd, KDiff3App &app)
    : QWidget(pParent)
{
    d = std::make_unique<DiffTextWindowFrameData>(this, pOptions, winIdx);
    d->mSourceData = psd;
    setAutoFillBackground(true);
    chk_connect(d->getBrowseButton(), &QPushButton::clicked, this, &DiffTextWindowFrame::slotBrowseButtonClicked);
    chk_connect(d->getFileSelectionField(), &QLineEdit::returnPressed, this, &DiffTextWindowFrame::slotReturnPressed);

    d->m_pDiffTextWindow = new DiffTextWindow(this, pOptions, winIdx, app);
    d->m_pDiffTextWindow->setSourceData(psd);
    QVBoxLayout* pVTopLayout = new QVBoxLayout(const_cast<QWidget*>(d->getTopLineWidget()));
    pVTopLayout->setContentsMargins(2, 2, 2, 2);
    pVTopLayout->setSpacing(0);
    QHBoxLayout* pHL = new QHBoxLayout();
    QHBoxLayout* pHL2 = new QHBoxLayout();
    pVTopLayout->addLayout(pHL);
    pVTopLayout->addLayout(pHL2);

    // Upper line:
    pHL->setContentsMargins(0, 0, 0, 0);
    pHL->setSpacing(2);

    pHL->addWidget(const_cast<QLabel*>(d->getLabel()), 0);
    pHL->addWidget(const_cast<FileNameLineEdit*>(d->getFileSelectionField()), 1);
    pHL->addWidget(const_cast<QPushButton*>(d->getBrowseButton()), 0);
    pHL->addWidget(d->m_pTopLine, 0);

    // Lower line
    pHL2->setContentsMargins(0, 0, 0, 0);
    pHL2->setSpacing(2);
    pHL2->addWidget(d->m_pTopLine, 0);
    d->m_pEncoding = new EncodingLabel(i18n("Encoding:"), psd, pOptions);
    //EncodeLabel::EncodingChanged should be handled asyncroniously.
    chk_connect_q((EncodingLabel*)d->m_pEncoding, &EncodingLabel::encodingChanged, this, &DiffTextWindowFrame::slotEncodingChanged);

    d->m_pLineEndStyle = new QLabel(i18n("Line end style:"));
    pHL2->addWidget(d->m_pEncoding);
    pHL2->addWidget(d->m_pLineEndStyle);

    QVBoxLayout* pVL = new QVBoxLayout(this);
    pVL->setContentsMargins(0, 0, 0, 0);
    pVL->setSpacing(0);
    pVL->addWidget(const_cast<QWidget*>(d->getTopLineWidget()), 0);
    pVL->addWidget(d->m_pDiffTextWindow, 1);

    d->m_pDiffTextWindow->installEventFilter(this);
    d->m_pFileSelection->installEventFilter(this);
    d->m_pBrowseButton->installEventFilter(this);
    init();
}

DiffTextWindowFrame::~DiffTextWindowFrame() = default;

void DiffTextWindowFrame::init()
{
    DiffTextWindow* pDTW = d->m_pDiffTextWindow;
    if(pDTW)
    {
        QString s = QDir::toNativeSeparators(pDTW->getFileName());
        d->m_pFileSelection->setText(s);
        QString winId = pDTW->getWindowIndex() == e_SrcSelector::A ? (pDTW->isThreeWay() ? i18n("A (Base)") : i18n("A")) : (pDTW->getWindowIndex() == e_SrcSelector::B ? i18n("B") : i18n("C"));
        d->m_pLabel->setText(winId + ':');
        d->m_pEncoding->setText(i18n("Encoding: %1", pDTW->getEncodingDisplayString()));
        d->m_pLineEndStyle->setText(i18n("Line end style: %1", pDTW->getLineEndStyle() == eLineEndStyleDos ? i18n("DOS") : pDTW->getLineEndStyle() == eLineEndStyleUnix ? i18n("Unix") :
                                                                                                                                                                          i18n("Unknown")));
    }
}

void DiffTextWindowFrame::setupConnections(const KDiff3App* app)
{
    chk_connect(this, &DiffTextWindowFrame::fileNameChanged, app, &KDiff3App::slotFileNameChanged);
    chk_connect(this, &DiffTextWindowFrame::encodingChanged, app, &KDiff3App::slotEncodingChanged);
}

// Search for the first visible line (search loop needed when no line exists for this file.)
LineRef DiffTextWindow::calcTopLineInFile(const LineRef firstLine)
{
    LineRef currentLine;
    for(QtSizeType i = convertLineToDiff3LineIdx(firstLine); i < d->getDiff3LineVector()->size(); ++i)
    {
        const Diff3Line* d3l = (*d->getDiff3LineVector())[i];
        currentLine = d3l->getLineInFile(d->m_winIdx);
        if(currentLine.isValid()) break;
    }
    return currentLine;
}

void DiffTextWindowFrame::setFirstLine(qint32 firstLine)
{
    DiffTextWindow* pDTW = d->m_pDiffTextWindow;
    if(pDTW && pDTW->getDiff3LineVector())
    {
        QString s = i18n("Top line");
        qint32 lineNumberWidth = pDTW->getLineNumberWidth();

        LineRef topVisiableLine = pDTW->calcTopLineInFile(firstLine);

        qint32 w = Utils::getHorizontalAdvance(d->m_pTopLine->fontMetrics(), s + ' ' + QString().fill('0', lineNumberWidth));
        d->m_pTopLine->setMinimumWidth(w);

        if(!topVisiableLine.isValid())
            s = i18n("End");
        else
            s += ' ' + QString::number(topVisiableLine + 1);

        d->m_pTopLine->setText(s);
        d->m_pTopLine->repaint();
    }
}

QPointer<DiffTextWindow> DiffTextWindowFrame::getDiffTextWindow()
{
    return d->m_pDiffTextWindow;
}

bool DiffTextWindowFrame::eventFilter(QObject* o, QEvent* e)
{
    Q_UNUSED(o);
    if(e->type() == QEvent::FocusIn || e->type() == QEvent::FocusOut)
    {
        QColor c1 = d->getOptions()->backgroundColor();
        QColor c2;
        if(d->m_winIdx == e_SrcSelector::A)
            c2 = d->getOptions()->aColor();
        else if(d->m_winIdx == e_SrcSelector::B)
            c2 = d->getOptions()->bColor();
        else if(d->m_winIdx == e_SrcSelector::C)
            c2 = d->getOptions()->cColor();

        QPalette p = d->m_pTopLineWidget->palette();
        if(e->type() == QEvent::FocusOut)
            std::swap(c1, c2);

        p.setColor(QPalette::Window, c2);
        setPalette(p);

        p.setColor(QPalette::WindowText, c1);
        d->m_pLabel->setPalette(p);
        d->m_pTopLine->setPalette(p);
        d->m_pEncoding->setPalette(p);
        d->m_pLineEndStyle->setPalette(p);
    }

    return false;
}

void DiffTextWindowFrame::slotReturnPressed()
{
    DiffTextWindow* pDTW = d->m_pDiffTextWindow;
    if(pDTW->getFileName() != d->m_pFileSelection->text())
    {
        Q_EMIT fileNameChanged(d->m_pFileSelection->text(), pDTW->getWindowIndex());
    }
}

void DiffTextWindowFrame::slotBrowseButtonClicked()
{
    QString current = d->m_pFileSelection->text();

    QUrl newURL = QFileDialog::getOpenFileUrl(this, i18n("Open File"), QUrl::fromUserInput(current, QString(), QUrl::AssumeLocalFile));
    if(!newURL.isEmpty())
    {
        DiffTextWindow* pDTW = d->m_pDiffTextWindow;
        Q_EMIT fileNameChanged(newURL.url(), pDTW->getWindowIndex());
    }
}

void DiffTextWindowFrame::slotEncodingChanged(QTextCodec* c)
{
    Q_EMIT encodingChanged(c); //relay signal from encoding label
    d->mSourceData->setEncoding(c);
}

EncodingLabel::EncodingLabel(const QString& text, const QSharedPointer<SourceData>& pSD, const QSharedPointer<Options>& pOptions)
    : QLabel(text)
{
    m_pOptions = pOptions;
    m_pSourceData = pSD;
    m_pContextEncodingMenu = nullptr;
    setMouseTracking(true);
}

void EncodingLabel::mouseMoveEvent(QMouseEvent*)
{
    // When there is no data to display or it came from clipboard,
    // we will be use UTF-8 only,
    // in that case there is no possibility to change the encoding in the SourceData
    // so, we should hide the HandCursor and display usual ArrowCursor
    if(m_pSourceData->isFromBuffer() || m_pSourceData->isEmpty())
        setCursor(QCursor(Qt::ArrowCursor));
    else
        setCursor(QCursor(Qt::PointingHandCursor));
}

void EncodingLabel::mousePressEvent(QMouseEvent*)
{
    if(!(m_pSourceData->isFromBuffer() || m_pSourceData->isEmpty()))
    {
        delete m_pContextEncodingMenu;
        m_pContextEncodingMenu = new QMenu(this);
        QMenu* pContextEncodingSubMenu = new QMenu(m_pContextEncodingMenu);

        qint32 currentTextCodecEnum = m_pSourceData->getEncoding()->mibEnum(); // the codec that will be checked in the context menu
        const QList<qint32> mibs = QTextCodec::availableMibs();
        QList<qint32> codecEnumList;

        // Adding "main" encodings
        insertCodec(i18n("Unicode, 8 bit"), QTextCodec::codecForName("UTF-8"), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
        if(QTextCodec::codecForName("System"))
        {
            insertCodec(QString(), QTextCodec::codecForName("System"), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
        }

        // Adding recent encodings
        if(m_pOptions != nullptr)
        {
            const QStringList& recentEncodings = m_pOptions->m_recentEncodings;
            for(const QString& s : recentEncodings)
            {
                insertCodec("", QTextCodec::codecForName(s.toLatin1()), codecEnumList, m_pContextEncodingMenu, currentTextCodecEnum);
            }
        }
        // Submenu to add the rest of available encodings
        pContextEncodingSubMenu->setTitle(i18n("Other"));
        for(qint32 i : mibs)
        {
            QTextCodec* c = QTextCodec::codecForMib(i);
            if(c != nullptr)
                insertCodec("", c, codecEnumList, pContextEncodingSubMenu, currentTextCodecEnum);
        }

        m_pContextEncodingMenu->addMenu(pContextEncodingSubMenu);

        m_pContextEncodingMenu->exec(QCursor::pos());
    }
}

void EncodingLabel::insertCodec(const QString& visibleCodecName, QTextCodec* pCodec, QList<qint32>& codecEnumList, QMenu* pMenu, qint32 currentTextCodecEnum) const
{
    if(pCodec == nullptr)
        return;

    qint32 CodecMIBEnum = pCodec->mibEnum();
    if(!codecEnumList.contains(CodecMIBEnum))
    {
        QAction* pAction = new QAction(pMenu); // menu takes ownership, so deleting the menu deletes the action too.
        QByteArray nameArray = pCodec->name();
        QLatin1String codecName = QLatin1String(nameArray);

        pAction->setText(visibleCodecName.isEmpty() ? codecName : visibleCodecName + u8" (" + codecName + u8")");
        pAction->setData(CodecMIBEnum);
        pAction->setCheckable(true);
        if(currentTextCodecEnum == CodecMIBEnum)
            pAction->setChecked(true);
        pMenu->addAction(pAction);
        chk_connect(pAction, &QAction::triggered, this, &EncodingLabel::slotSelectEncoding);
        codecEnumList.append(CodecMIBEnum);
    }
}

void EncodingLabel::slotSelectEncoding()
{
    QAction* pAction = qobject_cast<QAction*>(sender());
    if(pAction)
    {
        QTextCodec* pCodec = QTextCodec::codecForMib(pAction->data().toInt());
        if(pCodec != nullptr)
        {
            QString s(QLatin1String(pCodec->name()));
            QStringList& recentEncodings = m_pOptions->m_recentEncodings;
            if(!recentEncodings.contains(s) && s != "UTF-8" && s != "System")
            {
                qint32 itemsToRemove = recentEncodings.size() - m_maxRecentEncodings + 1;
                for(qint32 i = 0; i < itemsToRemove; ++i)
                {
                    recentEncodings.removeFirst();
                }
                recentEncodings.append(s);
            }
        }

        Q_EMIT encodingChanged(pCodec);
    }
}
