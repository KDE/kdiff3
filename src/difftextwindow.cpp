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

#include "defmac.h"
#include "FileNameLineEdit.h"
#include "kdiff3.h"
#include "LineRef.h"
#include "Logging.h"
#include "merger.h"
#include "options.h"
#include "progress.h"
#include "RLPainter.h"
#include "selection.h"
#include "SourceData.h"
#include "TypeUtils.h"
#include "Utils.h"

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
#include <QTextLayout>
#include <QThread>
#include <QtMath>
#include <QToolTip>
#include <QUrl>

/*
    QRunnable is not enough here. It may appear to work depending on configuration.
    That is an artifact of the threads being short-lived. Never the less such code is
    not safe because of a potenial race condition on exit.

    The use of Qt's parent system establishes order of destruction for QObjects.
    Allowing us to guarantee clearance of these helper threads and their accompanying
    objects before the associated DiffTextWindow begins teardown.
*/
class RecalcWordWrapThread: public QThread
{
  private:
    static QAtomicInteger<size_t> s_runnableCount;

    qint32 m_visibleTextWidth;
    size_t m_cacheIdx;

  public:
    static QAtomicInteger<size_t> s_maxNofRunnables;
    RecalcWordWrapThread(DiffTextWindow* parent, qint32 visibleTextWidth, SafeInt<size_t> cacheIdx):
        QThread(parent), m_visibleTextWidth(visibleTextWidth), m_cacheIdx(cacheIdx)
    {
        setTerminationEnabled(true);
        s_runnableCount.fetchAndAddOrdered(1);
    }
    void run() override
    {
        //safe thanks to Qt memory mangement
        DiffTextWindow* pDTW = qobject_cast<DiffTextWindow*>(parent());

        pDTW->recalcWordWrapHelper(0, m_visibleTextWidth, m_cacheIdx);
        size_t newValue = s_runnableCount.fetchAndSubOrdered(1) - 1;
        ProgressProxy::setCurrent(s_maxNofRunnables - s_runnableCount.loadRelaxed());
        if(newValue == 0)
        {
            Q_EMIT pDTW->finishRecalcWordWrap(m_visibleTextWidth);

            s_maxNofRunnables.storeRelease(0);
        }
        //Cleanup our object to avoid a memory leak
        deleteLater();
    }

    ~RecalcWordWrapThread() override
    {
        //Wait for thread to finish.
        if(isRunning())
            wait();
    }
};

QAtomicInteger<size_t> RecalcWordWrapThread::s_runnableCount = 0;
QAtomicInteger<size_t> RecalcWordWrapThread::s_maxNofRunnables = 0;

class WrapLineCacheData
{
  public:
    WrapLineCacheData() = default;
    WrapLineCacheData(qint32 d3LineIdx, qint32 textStart, qint32 textLength):
        m_d3LineIdx(d3LineIdx), m_textStart(textStart), m_textLength(textLength) {}
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

    void init(
        const QString& filename,
        const char* encoding,
        e_LineEndStyle eLineEndStyle,
        const std::shared_ptr<LineDataVector>& pLineData,
        LineType size,
        const Diff3LineVector* pDiff3LineVector,
        const ManualDiffHelpList* pManualDiffHelpList)
    {
        reset();
        m_filename = filename;
        m_pLineData = pLineData;
        m_size = size;
        mDiff3LineVector = pDiff3LineVector;
        m_pManualDiffHelpList = pManualDiffHelpList;

        mTextEncoding = QString::fromLatin1(encoding);
        m_eLineEndStyle = eLineEndStyle;
    }

    void reset()
    {
        //wait for all helper threads to finish
        while(DiffTextWindow::maxThreads() > 0) {} //Clear word wrap threads.

        m_firstLine = 0;
        m_oldFirstLine = LineRef::invalid;
        m_horizScrollOffset = 0;
        m_scrollDeltaX = 0;
        m_scrollDeltaY = 0;
        m_bMyUpdate = false;
        m_fastSelectorLine1 = 0;
        m_fastSelectorNofLines = 0;
        m_maxTextWidth = -1;

        m_pLineData = nullptr;
        m_size = 0;
        mDiff3LineVector = nullptr;
        m_filename = "";
        m_diff3WrapLineVector.clear();
    }

    [[nodiscard]] QString getString(const LineType d3lIdx) const;
    [[nodiscard]] QString getLineString(const qint32 line) const;

    void writeLine(
        RLPainter& p,
        const std::shared_ptr<const DiffList>& pLineDiff1, const std::shared_ptr<const DiffList>& pLineDiff2, const LineRef& line,
        const ChangeFlags whatChanged, const ChangeFlags whatChanged2, const LineRef& srcLineIdx,
        qint32 wrapLineOffset, qint32 wrapLineLength, bool bWrapLine, const QRect& invalidRect);

    void myUpdate(qint32 afterMilliSecs);

    //TODO: Fix after line number area is converted to a QWidget.
    [[nodiscard]] qint32 leftInfoWidth() const { return 4 + (gOptions->m_bShowLineNumbers ? m_pDiffTextWindow->getLineNumberWidth() : 0); } // Number of information columns on left side
    [[nodiscard]] LineRef convertLineOnScreenToLineInSource(const qint32 lineOnScreen, const e_CoordType coordType, const bool bFirstLine) const;

    void prepareTextLayout(QTextLayout& textLayout, qint32 visibleTextWidth = -1);

    [[nodiscard]] bool isThreeWay() const { return KDiff3App::isTripleDiff(); };
    [[nodiscard]] const QString& getFileName() const { return m_filename; }

    [[nodiscard]] const Diff3LineVector* getDiff3LineVector() const { return mDiff3LineVector; }

    [[nodiscard]] const Diff3WrapLineVector& getDiff3WrapLineVector() { return m_diff3WrapLineVector; }
    [[nodiscard]] bool hasLineData() const { return m_pLineData != nullptr && !m_pLineData->empty(); }

    void initColors()
    {
        if(getWindowIndex() == e_SrcSelector::A)
        {
            m_cThis = gOptions->aColor();
            m_cDiff1 = gOptions->bColor();
            m_cDiff2 = gOptions->cColor();
        }
        else if(getWindowIndex() == e_SrcSelector::B)
        {
            m_cThis = gOptions->bColor();
            m_cDiff1 = gOptions->cColor();
            m_cDiff2 = gOptions->aColor();
        }
        else if(getWindowIndex() == e_SrcSelector::C)
        {
            m_cThis = gOptions->cColor();
            m_cDiff1 = gOptions->aColor();
            m_cDiff2 = gOptions->bColor();
        }
        m_cDiffBoth = gOptions->conflictColor(); // Conflict color
    }

    [[nodiscard]] QColor thisColor() const { return m_cThis; }
    [[nodiscard]] QColor diff1Color() const { return m_cDiff1; }
    [[nodiscard]] QColor diff2Color() const { return m_cDiff2; }
    [[nodiscard]] QColor diffBothColor() const { return m_cDiffBoth; }
    [[nodiscard]] const Diff3LineVector* diff3LineVector() { return mDiff3LineVector; }

  private:
    friend DiffTextWindow;
    [[nodiscard]] e_SrcSelector getWindowIndex() const { return m_pDiffTextWindow->getWindowIndex(); }

    QPointer<DiffTextWindow> m_pDiffTextWindow;
    QString mTextEncoding;
    e_LineEndStyle m_eLineEndStyle;

    std::shared_ptr<LineDataVector> m_pLineData;
    LineType m_size = 0;
    QString m_filename;
    bool m_bWordWrap = false;
    qint32 m_delayedDrawTimer = 0;

    const Diff3LineVector* mDiff3LineVector = nullptr;
    Diff3WrapLineVector m_diff3WrapLineVector;
    const ManualDiffHelpList* m_pManualDiffHelpList = nullptr;
    std::vector<std::vector<WrapLineCacheData>> m_wrapLineCacheList;

    QColor m_cThis;
    QColor m_cDiff1;
    QColor m_cDiff2;
    QColor m_cDiffBoth;

    LineRef m_fastSelectorLine1 = 0;
    LineType m_fastSelectorNofLines = 0;

    LineRef m_firstLine = 0;
    LineRef m_oldFirstLine;
    qint32 m_horizScrollOffset = 0;
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

void DiffTextWindow::setSourceData(const QSharedPointer<SourceData>& inData)
{
    d->sourceData = inData;
}

bool DiffTextWindow::isThreeWay() const
{
    return d->isThreeWay();
};

const QString& DiffTextWindow::getFileName() const
{
    return d->getFileName();
}

e_SrcSelector DiffTextWindow::getWindowIndex() const
{
    return mWinIdx;
};

const QString DiffTextWindow::getEncodingDisplayString() const
{
    return d->mTextEncoding;
}

e_LineEndStyle DiffTextWindow::getLineEndStyle() const
{
    return d->m_eLineEndStyle;
}

const Diff3LineVector* DiffTextWindow::getDiff3LineVector() const
{
    return d->getDiff3LineVector();
}

qint32 DiffTextWindow::getLineNumberWidth() const
{
    return std::floor(std::log10(std::max(d->m_size, 1))) + 1;
}

DiffTextWindow::DiffTextWindow(DiffTextWindowFrame* pParent,
                               e_SrcSelector winIdx,
                               KDiff3App& app):
    QWidget(pParent),
    m_app(app)
{
    setObjectName(QString("DiffTextWindow%1").arg((qint32)winIdx));
    setAttribute(Qt::WA_OpaquePaintEvent);
    setUpdatesEnabled(false);

    d = std::make_unique<DiffTextWindowData>(this);
    setFocusPolicy(Qt::ClickFocus);
    setAcceptDrops(true);
    mWinIdx = winIdx;

    init(QString(""), nullptr, d->m_eLineEndStyle, nullptr, 0, nullptr, nullptr);

    setMinimumSize(QSize(20, 20));

    setUpdatesEnabled(true);

    setFont(gOptions->defaultFont());
}

DiffTextWindow::~DiffTextWindow() = default;

void DiffTextWindow::init(
    const QString& filename,
    const char* encoding,
    e_LineEndStyle eLineEndStyle,
    const std::shared_ptr<LineDataVector>& pLineData,
    LineType size,
    const Diff3LineVector* pDiff3LineVector,
    const ManualDiffHelpList* pManualDiffHelpList)
{
    d->init(filename, encoding, eLineEndStyle, pLineData, size, pDiff3LineVector, pManualDiffHelpList);

    update();
}

void DiffTextWindow::reset()
{
    d->reset();
}

void DiffTextWindow::setupConnections(const KDiff3App* app)
{
    assert(qobject_cast<DiffTextWindowFrame*>(parent()) != nullptr);

    chk_connect_a(this, &DiffTextWindow::firstLineChanged, dynamic_cast<DiffTextWindowFrame*>(parent()), &DiffTextWindowFrame::setFirstLine);
    chk_connect_a(this, &DiffTextWindow::newSelection, app, &KDiff3App::slotSelectionStart);
    chk_connect_a(this, &DiffTextWindow::selectionEnd, app, &KDiff3App::slotSelectionEnd);
    chk_connect_a(this, &DiffTextWindow::scrollDiffTextWindow, app, &KDiff3App::scrollDiffTextWindow);
    chk_connect_q(this, &DiffTextWindow::finishRecalcWordWrap, app, &KDiff3App::slotFinishRecalcWordWrap);

    chk_connect_a(this, &DiffTextWindow::finishDrop, app, &KDiff3App::slotFinishDrop);

    chk_connect_a(this, &DiffTextWindow::statusBarMessage, app, &KDiff3App::slotStatusMsg);

    chk_connect_a(app, &KDiff3App::showWhiteSpaceToggled, this, static_cast<void (DiffTextWindow::*)(void)>(&DiffTextWindow::update));
    chk_connect_a(app, &KDiff3App::showLineNumbersToggled, this, static_cast<void (DiffTextWindow::*)(void)>(&DiffTextWindow::update));
    chk_connect_a(app, &KDiff3App::doRefresh, this, &DiffTextWindow::slotRefresh);
    chk_connect_a(app, &KDiff3App::selectAll, this, &DiffTextWindow::slotSelectAll);
    chk_connect_a(app, &KDiff3App::copy, this, &DiffTextWindow::slotCopy);

    chk_connect_a(this, &DiffTextWindow::scrollToH, app, &KDiff3App::slotScrollToH);

    connections.push_back(StandardMenus::allowCopy.connect(boost::bind(&DiffTextWindow::canCopy, this)));
    connections.push_back(KDiff3App::getSelection.connect(boost::bind(&DiffTextWindow::getSelection, this)));
}

void DiffTextWindow::slotRefresh()
{
    setFont(gOptions->defaultFont());
    update();
}

void DiffTextWindow::slotSelectAll()
{
    if(hasFocus())
    {
        setSelection(0, 0, getNofLines(), 0);
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

            d->sourceData->setData(text);
            const QStringList& errors = d->sourceData->getErrors();
            if(!errors.isEmpty())
                error = d->sourceData->getErrors()[0];

            if(!error.isEmpty())
            {
                KMessageBox::error(this, error);
            }

            Q_EMIT finishDrop();
        }
    }
}

void DiffTextWindow::printWindow(RLPainter& painter, const QRect& view, const QString& headerText, qint32 line, const LineType linesPerPage, const QColor& fgColor)
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

void DiffTextWindow::setFirstLine(LineRef firstLine)
{
    qint32 fontHeight = fontMetrics().lineSpacing();

    LineRef newFirstLine = std::max<LineRef>(0, firstLine);

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

LineRef DiffTextWindow::getFirstLine() const
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
    else if(d->m_maxTextWidth.loadRelaxed() < 0)
    {
        d->m_maxTextWidth = 0;
        QTextLayout textLayout(QString(), font(), this);
        for(qint32 i = 0; i < d->m_size; ++i)
        {
            textLayout.clearLayout();
            textLayout.setText(d->getString(i));
            d->prepareTextLayout(textLayout);
            if(textLayout.maximumWidth() > d->m_maxTextWidth.loadRelaxed())
                d->m_maxTextWidth = qCeil(textLayout.maximumWidth());
        }
    }
    return d->m_maxTextWidth.loadRelaxed();
}

LineType DiffTextWindow::getNofLines() const
{
    return static_cast<LineType>(d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : d->getDiff3LineVector()->size());
}

LineType DiffTextWindow::convertLineToDiff3LineIdx(const LineRef line) const
{
    if(line.isValid() && d->m_bWordWrap && d->m_diff3WrapLineVector.size() > 0)
        return d->m_diff3WrapLineVector[std::min<size_t>(line, d->m_diff3WrapLineVector.size() - 1)].diff3LineIndex;
    else
        return line;
}

LineRef DiffTextWindow::convertDiff3LineIdxToLine(const LineType d3lIdx) const
{
    assert(d3lIdx >= 0);

    if(d->m_bWordWrap && d->getDiff3LineVector() != nullptr && d->getDiff3LineVector()->size() > 0)
        return (*d->getDiff3LineVector())[std::min((size_t)d3lIdx, d->getDiff3LineVector()->size() - 1)]->sumLinesNeededForDisplay();
    else
        return d3lIdx;
}

/** Returns a line number where the linerange [line, line+nofLines] can
    be displayed best. If it fits into the currently visible range then
    the returned value is the current firstLine.
*/
LineRef getBestFirstLine(LineRef line, LineType nofLines, LineRef firstLine, LineType visibleLines)
{
    assert(visibleLines >= 0); // VisibleLines should not be < 0.

    if(line < visibleLines || visibleLines == 0) //well known result.
        return 0;

    LineRef newFirstLine = firstLine;
    if(line > firstLine && line + nofLines + 2 <= firstLine + visibleLines)
        return newFirstLine;

    if(nofLines < visibleLines)
        newFirstLine = std::max(0, (LineType)std::ceil(line - (visibleLines - nofLines) / 2));
    else
    {
        LineType numberPages = (LineType)std::floor(nofLines / visibleLines);
        newFirstLine = std::max(0, line - (visibleLines * numberPages) / 3);
    }
    return newFirstLine;
}

void DiffTextWindow::setFastSelectorRange(qint32 line1, qint32 nofLines)
{
    d->m_fastSelectorLine1 = line1;
    d->m_fastSelectorNofLines = nofLines;
    if(isVisible())
    {
        LineRef newFirstLine = getBestFirstLine(
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
    LineType d3lIdx = convertLineToDiff3LineIdx(lineFromPos);

    if(d->getDiff3LineVector() != nullptr && d3lIdx >= 0 && d3lIdx < (qint32)d->getDiff3LineVector()->size())
    {
        const Diff3Line* pD3l = (*d->getDiff3LineVector())[d3lIdx];
        if(pD3l != nullptr)
        {
            LineRef actualLine = pD3l->getLineInFile(getWindowIndex());

            QString message;
            if(actualLine.isValid())
                message = i18n("File %1: Line %2", getFileName(), actualLine + 1);
            else
                message = i18n("File %1: Line not available", getFileName());
            Q_EMIT statusBarMessage(message);

            Q_EMIT lineClicked(getWindowIndex(), actualLine);
        }
    }
}

void DiffTextWindow::scrollVertically(qint32 deltaY)
{
    mVScrollBar->setValue(mVScrollBar->value() + deltaY);
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
        convertToLinePos(e->pos().x(), e->pos().y(), line, pos);
        qCInfo(kdiffDiffTextWindow) << "Left Button detected,";
        qCDebug(kdiffDiffTextWindow) << "line = " << line << ", pos = " << pos;

        //TODO: Fix after line number area is converted to a QWidget.
        qint32 fontWidth = fontMetrics().horizontalAdvance('0');
        qint32 xOffset = d->leftInfoWidth() * fontWidth;

        if((!gOptions->m_bRightToLeftLanguage && e->pos().x() < xOffset) || (gOptions->m_bRightToLeftLanguage && e->pos().x() > width() - xOffset))
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
        convertToLinePos(e->pos().x(), e->pos().y(), line, pos);
        qCInfo(kdiffDiffTextWindow) << "Left Button detected,";
        qCDebug(kdiffDiffTextWindow) << "line = " << line << ", pos = " << pos;

        // Get the string data of the current line
        QString s;
        if(d->m_bWordWrap)
        {
            if(!line.isValid() || (size_t)line >= d->m_diff3WrapLineVector.size())
                return;
            const Diff3WrapLine& d3wl = d->m_diff3WrapLineVector[line];
            s = d->getString(d3wl.diff3LineIndex).mid(d3wl.wrapLineOffset, d3wl.wrapLineLength);
        }
        else
        {
            if(!line.isValid() || (size_t)line >= d->getDiff3LineVector()->size())
                return;
            s = d->getString(line);
        }

        if(!s.isEmpty())
        {
            const bool selectionWasEmpty = d->m_selection.isEmpty();
            qsizetype pos1, pos2;
            Utils::calcTokenPos(s, pos, pos1, pos2);

            resetSelection();
            d->m_selection.start(line, pos1);
            d->m_selection.end(line, pos2);
            if(!d->m_selection.isEmpty() && selectionWasEmpty)
                Q_EMIT newSelection();

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
{ //Handles selection highlighting.
    LineRef line;
    qint32 pos;

    qCInfo(kdiffDiffTextWindow) << "Mouse Moved";
    qCDebug(kdiffDiffTextWindow) << "d->m_lastKnownMousePos = " << d->m_lastKnownMousePos << ", e->pos() = " << e->pos();

    convertToLinePos(e->pos().x(), e->pos().y(), line, pos);
    d->m_lastKnownMousePos = e->pos();

    qCDebug(kdiffDiffTextWindow) << "line = " << line << ", pos = " << pos;

    if(d->m_selection.isValidFirstLine())
    {
        qCDebug(kdiffDiffTextWindow) << "d->m_selection.isValidFirstLine() = " << d->m_selection.isValidFirstLine();
        const bool selectionWasEmpty = d->m_selection.isEmpty();
        d->m_selection.end(line, pos);
        if(!d->m_selection.isEmpty() && selectionWasEmpty)
            Q_EMIT newSelection();

        showStatusLine(line);

        // Scroll because mouse moved out of the window
        const QFontMetrics& fm = fontMetrics();
        qint32 fontWidth = fm.horizontalAdvance('0');
        qint32 deltaX = 0;
        qint32 deltaY = 0;
        //TODO: Fix after line number area is converted to a QWidget.
        //FIXME: Why are we manually doing Layout adjustments?
        if(!gOptions->m_bRightToLeftLanguage)
        {
            if(e->pos().x() < d->leftInfoWidth() * fontWidth) deltaX = -1 - abs(e->pos().x() - d->leftInfoWidth() * fontWidth) / fontWidth;
            if(e->pos().x() > width()) deltaX = +1 + abs(e->pos().x() - width()) / fontWidth;
        }
        else
        {
            if(e->pos().x() > width() - 1 - d->leftInfoWidth() * fontWidth) deltaX = +1 + abs(e->pos().x() - (width() - 1 - d->leftInfoWidth() * fontWidth)) / fontWidth;
            if(e->pos().x() < fontWidth) deltaX = -1 - abs(e->pos().x() - fontWidth) / fontWidth;
        }
        if(e->pos().y() < 0) deltaY = -1 - (qint32)std::pow<qint32, qint32>(e->pos().y(), 2) / (qint32)std::pow(fm.lineSpacing(), 2);
        if(e->pos().y() > height()) deltaY = 1 + (qint32)std::pow(e->pos().y() - height(), 2) / (qint32)std::pow(fm.lineSpacing(), 2);
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
        QCoreApplication::sendEvent(mVScrollBar, pWheelEvent);
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
            LineRef lastLine;
            LineRef firstLine;
            if(d->m_selection.getOldFirstLine().isValid())
            {
                firstLine = std::min({d->m_selection.getOldFirstLine(), d->m_selection.getLastLine(), d->m_selection.getOldLastLine()});
                lastLine = std::max({d->m_selection.getOldFirstLine(), d->m_selection.getLastLine(), d->m_selection.getOldLastLine()});
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
        qsizetype newPos = d->m_selection.getLastPos() + d->m_scrollDeltaX;
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

    if(line.isValid() && (!gOptions->wordWrapOn() || (size_t)line < d->m_diff3WrapLineVector.size()))
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
    operator QVector<QTextLayout::FormatRange>() { return m_formatRanges; }
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

    textOption.setTabStopDistance(QFontMetricsF(m_pDiffTextWindow->font()).horizontalAdvance(' ') * gOptions->tabSize());

    if(gOptions->m_bShowWhiteSpaceCharacters)
        textOption.setFlags(QTextOption::ShowTabsAndSpaces);
    if(gOptions->m_bRightToLeftLanguage)

    {
        textOption.setAlignment(Qt::AlignRight); // only relevant for multi line text layout
    }
    if(visibleTextWidth >= 0)
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

    textLayout.setTextOption(textOption);

    if(gOptions->m_bShowWhiteSpaceCharacters)
    {
        // This additional format is only necessary for the tab arrow
        QVector<QTextLayout::FormatRange> formats;
        QTextLayout::FormatRange formatRange;
        formatRange.start = 0;
        formatRange.length = SafeInt<qint32>(textLayout.text().length());
        formatRange.format.setFont(m_pDiffTextWindow->font());
        formats.append(formatRange);
        textLayout.setFormats(formats);
    }
    textLayout.beginLayout();

    qint32 leading = m_pDiffTextWindow->fontMetrics().leading();
    qint32 height = 0;
    //TODO: Fix after line number area is converted to a QWidget.
    qint32 fontWidth = m_pDiffTextWindow->fontMetrics().horizontalAdvance('0');
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
        if(visibleTextWidth >= 0)
        {
            line.setLineWidth(visibleTextWidth - indentation);
            line.setPosition(QPointF(indentation, height));
            height += qCeil(line.height());
        }
        else // only one line
        {
            line.setPosition(QPointF(indentation, height));
            break;
        }
    }

    textLayout.endLayout();
    if(gOptions->m_bRightToLeftLanguage)
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
    const std::shared_ptr<const DiffList>& pLineDiff1,
    const std::shared_ptr<const DiffList>& pLineDiff2,
    const LineRef& line,
    const ChangeFlags whatChanged,
    const ChangeFlags whatChanged2,
    const LineRef& srcLineIdx,
    qint32 wrapLineOffset,
    qint32 wrapLineLength,
    bool bWrapLine,
    const QRect& invalidRect)
{
    //TODO: Fix after line number area is converted to a QWidget.
    const qint32 lineNumberWidth = gOptions->m_bShowLineNumbers ? m_pDiffTextWindow->getLineNumberWidth() : 0;

    const LineData* pld = !srcLineIdx.isValid() ? nullptr : &(*m_pLineData)[srcLineIdx]; // Text in this line;
    QFont normalFont = p.font();

    const QFontMetrics& fm = p.fontMetrics();
    qint32 fontHeight = fm.lineSpacing();
    qint32 fontAscent = fm.ascent();
    qint32 fontWidth = fm.horizontalAdvance('0');

    qint32 xOffset = 0;
    qint32 yOffset = (line - m_firstLine) * fontHeight;

    qint32 fastSelectorLine1 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1);
    qint32 fastSelectorLine2 = m_pDiffTextWindow->convertDiff3LineIdxToLine(m_fastSelectorLine1 + m_fastSelectorNofLines) - 1;

    bool bFastSelectionRange = (line >= fastSelectorLine1 && line <= fastSelectorLine2);
    QColor bgColor = gOptions->backgroundColor();
    QColor diffBgColor = gOptions->diffBackgroundColor();

    if(bFastSelectionRange)
    {
        bgColor = gOptions->getCurrentRangeBgColor();
        diffBgColor = gOptions->getCurrentRangeDiffBgColor();
    }

    if(yOffset + fontHeight < invalidRect.top() || invalidRect.bottom() < yOffset - fontHeight)
        return;

    ChangeFlags changed = whatChanged;
    if(pLineDiff1 != nullptr) changed |= AChanged;
    if(pLineDiff2 != nullptr) changed |= BChanged;

    QColor penColor = gOptions->foregroundColor();
    p.setPen(penColor);
    if(changed == BChanged)
    {
        penColor = diff2Color();
    }
    else if(changed == AChanged)
    {
        penColor = diff1Color();
    }
    else if(changed == Both)
    {
        penColor = diffBothColor();
    }

    if(pld != nullptr)
    {
        // First calculate the "changed" information for each character.
        qsizetype i = 0;
        QString lineString = pld->getLine();
        if(!lineString.isEmpty())
        {
            switch(lineString[lineString.length() - 1].unicode())
            {
                case '\n':
                    lineString[lineString.length() - 1] = QChar(0x00B6);
                    break; // "Pilcrow", "paragraph mark"
                case '\r':
                    lineString[lineString.length() - 1] = QChar(0x00A4);
                    break; // Currency sign ;0x2761 "curved stem paragraph sign ornament"
                           //case '\0b' : lineString[lineString.length()-1] = 0x2756; break; // some other nice looking character
            }
        }
        QVector<ChangeFlags> charChanged(pld->size());
        Merger merger(pLineDiff1, pLineDiff2);
        while(!merger.isEndReached() && i < pld->size())
        {
            charChanged[i] = merger.whatChanged();
            ++i;

            merger.next();
        }

        qint32 outPos = 0;

        qsizetype lineLength = m_bWordWrap ? wrapLineOffset + wrapLineLength : lineString.length();

        FormatRangeHelper frh;

        for(i = wrapLineOffset; i < lineLength; ++i)
        {
            QColor penColor2 = gOptions->foregroundColor();
            ChangeFlags cchanged = charChanged[i] | whatChanged;

            if(cchanged == BChanged)
            {
                penColor2 = m_cDiff2;
            }
            else if(cchanged == AChanged)
            {
                penColor2 = m_cDiff1;
            }
            else if(cchanged == Both)
            {
                penColor2 = m_cDiffBoth;
            }

            if(penColor2 != gOptions->foregroundColor() && whatChanged2 == NoChange && !gOptions->m_bShowWhiteSpace)
            {
                // The user doesn't want to see highlighted white space.
                penColor2 = gOptions->foregroundColor();
            }

            frh.setBackground(bgColor);
            if(!m_selection.within(line, outPos))
            {
                if(penColor2 != gOptions->foregroundColor())
                {
                    frh.setBackground(diffBgColor);
                    // Setting italic font here doesn't work: Changing the font only when drawing is too late
                }

                frh.setPen(penColor2);
                frh.next();
                frh.setFont(normalFont);
            }
            else
            {
                frh.setBackground(m_pDiffTextWindow->palette().highlight().color());
                frh.setPen(m_pDiffTextWindow->palette().highlightedText().color());
                frh.next();
            }

            ++outPos;
        } // end for

        QTextLayout textLayout(lineString.mid(wrapLineOffset, lineLength - wrapLineOffset), m_pDiffTextWindow->font(), m_pDiffTextWindow);
        prepareTextLayout(textLayout);
        textLayout.draw(&p, QPoint(0, yOffset), frh /*, const QRectF & clip = QRectF() */);
    }

    p.fillRect(0, yOffset, leftInfoWidth() * fontWidth, fontHeight, gOptions->backgroundColor());

    //TODO: Fix after line number area is converted to a QWidget.
    xOffset = (lineNumberWidth + 2) * fontWidth;
    qint32 xLeft = lineNumberWidth * fontWidth;
    p.setPen(gOptions->foregroundColor());
    if(pld != nullptr)
    {
        if(gOptions->m_bShowLineNumbers && !bWrapLine)
        {
            QString num = QString::number(srcLineIdx + 1);
            assert(!num.isEmpty());
            p.drawText(0, yOffset + fontAscent, num);
        }
        if(!bWrapLine || wrapLineLength > 0)
        {
            Qt::PenStyle wrapLinePenStyle = Qt::DotLine;

            p.setPen(QPen(gOptions->foregroundColor(), 0, bWrapLine ? wrapLinePenStyle : Qt::SolidLine));
            p.drawLine(xOffset + 1, yOffset, xOffset + 1, yOffset + fontHeight - 1);
            p.setPen(QPen(gOptions->foregroundColor(), 0, Qt::SolidLine));
        }
    }
    if(penColor != gOptions->foregroundColor() && whatChanged2 == NoChange)
    {
        if(gOptions->m_bShowWhiteSpace)
        {
            p.setBrushOrigin(0, 0);
            p.fillRect(xLeft, yOffset, fontWidth * 2 - 1, fontHeight, QBrush(penColor, Qt::Dense5Pattern));
        }
    }
    else
    {
        p.fillRect(xLeft, yOffset, fontWidth * 2 - 1, fontHeight, penColor == gOptions->foregroundColor() ? bgColor : penColor);
    }

    if(bFastSelectionRange)
    {
        p.fillRect(xOffset + fontWidth - 1, yOffset, 3, fontHeight, gOptions->foregroundColor());
    }

    // Check if line needs a manual diff help mark
    ManualDiffHelpList::const_iterator ci;
    for(ci = m_pManualDiffHelpList->begin(); ci != m_pManualDiffHelpList->end(); ++ci)
    {
        const ManualDiffHelpEntry& mdhe = *ci;
        LineRef rangeLine1;
        LineRef rangeLine2;

        mdhe.getRangeForUI(getWindowIndex(), &rangeLine1, &rangeLine2);
        if(rangeLine1.isValid() && rangeLine2.isValid() && srcLineIdx >= rangeLine1 && srcLineIdx <= rangeLine2)
        {
            p.fillRect(xOffset - fontWidth, yOffset, fontWidth - 1, fontHeight, gOptions->manualHelpRangeColor());
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
        p.fillRect(invalidRect, gOptions->backgroundColor());
        return;
    }

    LineRef endLine = std::min(d->m_firstLine + getNofVisibleLines() + 2, getNofLines());
    RLPainter p(this, gOptions->m_bRightToLeftLanguage, width(), fontMetrics().horizontalAdvance('0'));

    p.setFont(font());
    p.QPainter::fillRect(invalidRect, gOptions->backgroundColor());

    draw(p, invalidRect, d->m_firstLine, endLine);
    p.end();

    d->m_oldFirstLine = d->m_firstLine;
    d->m_selection.clearOldSelection();
}

void DiffTextWindow::print(RLPainter& p, const QRect&, qint32 firstLine, const LineType nofLinesPerPage)
{
    if(d->getDiff3LineVector() == nullptr || !updatesEnabled() ||
       (d->m_diff3WrapLineVector.empty() && d->m_bWordWrap))
        return;
    resetSelection();
    LineRef oldFirstLine = d->m_firstLine;
    d->m_firstLine = firstLine;
    QRect invalidRect = QRect(0, 0, 1000000000, 1000000000);
    gOptions->beginPrint();
    draw(p, invalidRect, firstLine, std::min(firstLine + nofLinesPerPage, getNofLines()));
    gOptions->endPrint();
    d->m_firstLine = oldFirstLine;
}

void DiffTextWindow::draw(RLPainter& p, const QRect& invalidRect, const qint32 beginLine, const LineRef& endLine)
{
    if(!d->hasLineData()) return;

    d->initColors();
    p.setPen(d->thisColor());

    for(qint32 line = beginLine; line < endLine; ++line)
    {
        qint32 wrapLineOffset = 0;
        qint32 wrapLineLength = 0;
        const Diff3Line* d3l = nullptr;
        bool bWrapLine = false;
        if(d->m_bWordWrap)
        {
            const Diff3WrapLine& d3wl = d->getDiff3WrapLineVector()[line];
            wrapLineOffset = d3wl.wrapLineOffset;
            wrapLineLength = d3wl.wrapLineLength;
            d3l = d3wl.pD3L;
            bWrapLine = line > 0 && d->getDiff3WrapLineVector()[line - 1].pD3L == d3l;
        }
        else
        {
            d3l = (*d->diff3LineVector())[line];
        }
        std::shared_ptr<const DiffList> pFineDiff1;
        std::shared_ptr<const DiffList> pFineDiff2;
        ChangeFlags changed = NoChange;
        ChangeFlags changed2 = NoChange;

        LineRef srcLineIdx;
        d3l->getLineInfo(getWindowIndex(), KDiff3App::isTripleDiff(), srcLineIdx, pFineDiff1, pFineDiff2, changed, changed2);

        d->writeLine(
            p, // QPainter
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

QString DiffTextWindowData::getString(const LineType d3lIdx) const
{
    assert(!(m_pLineData != nullptr && m_pLineData->empty() && m_size != 0));

    if(m_pLineData == nullptr || m_pLineData->empty() || d3lIdx < 0 || (size_t)d3lIdx >= mDiff3LineVector->size())
        return QString();

    const Diff3Line* d3l = (*mDiff3LineVector)[d3lIdx];
    const LineType lineIdx = d3l->getLineIndex(getWindowIndex());

    if(lineIdx == LineRef::invalid)
        return QString();

    return (*m_pLineData)[lineIdx].getLine();
}

QString DiffTextWindowData::getLineString(const LineType line) const
{
    //assert(line > 0);
    if(m_bWordWrap)
    {
        if((size_t)line < m_diff3WrapLineVector.size())
        {
            LineType d3LIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx(line);
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
    LineType visibleLines = newSize.height() / fm.lineSpacing() - 2;
    //TODO: Fix after line number area is converted to a QWidget.
    qint32 visibleColumns = newSize.width() / fm.horizontalAdvance('0') - d->leftInfoWidth();

    if(e->size().height() != e->oldSize().height())
        Q_EMIT resizeHeightChangedSignal(visibleLines);
    if(e->size().width() != e->oldSize().width())
        Q_EMIT resizeWidthChangedSignal(visibleColumns);
    QWidget::resizeEvent(e);
}

LineType DiffTextWindow::getNofVisibleLines() const
{
    QFontMetrics fm = fontMetrics();
    //QWidget::height() may return 0 with certian configurations with 0 length input files loaded.
    return std::max((LineType)ceil(height() / fm.lineSpacing()) - 1, 0);
}

qint32 DiffTextWindow::getVisibleTextAreaWidth() const
{
    //TODO: Check after line number area is converted to a QWidget.
    QFontMetrics fm = fontMetrics();

    return width() - d->leftInfoWidth() * fm.horizontalAdvance('0');
}

QString DiffTextWindow::getSelection() const
{
    if(d->m_pLineData == nullptr)
        return QString();

    QString selectionString;

    LineRef line = 0;
    LineType lineIdx = 0;

    qsizetype it;
    qsizetype vectorSize = d->m_bWordWrap ? d->m_diff3WrapLineVector.size() : d->getDiff3LineVector()->size();
    for(it = 0; it < vectorSize; ++it)
    {
        const Diff3Line* d3l = d->m_bWordWrap ? d->m_diff3WrapLineVector[it].pD3L : (*d->getDiff3LineVector())[it];

        assert(getWindowIndex() >= e_SrcSelector::A && getWindowIndex() <= e_SrcSelector::C);

        if(getWindowIndex() == e_SrcSelector::A)
        {
            lineIdx = d3l->getLineA();
        }
        else if(getWindowIndex() == e_SrcSelector::B)
        {
            lineIdx = d3l->getLineB();
        }
        else if(getWindowIndex() == e_SrcSelector::C)
        {
            lineIdx = d3l->getLineC();
        }

        if(lineIdx != LineRef::invalid)
        {
            qsizetype size = (*d->m_pLineData)[lineIdx].size();
            QString lineString = (*d->m_pLineData)[lineIdx].getLine();

            if(d->m_bWordWrap)
            {
                size = d->m_diff3WrapLineVector[it].wrapLineLength;
                lineString = lineString.mid(d->m_diff3WrapLineVector[it].wrapLineOffset, size);
            }

            for(qsizetype i = 0; i < size; ++i)
            {
                if(d->m_selection.within(line, i))
                {
                    selectionString += lineString[i];
                }
            }

            if(d->m_selection.within(line, size) &&
               (!d->m_bWordWrap || it + 1 >= vectorSize || d3l != d->m_diff3WrapLineVector[it + 1].pD3L))
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

bool DiffTextWindow::findString(const QString& s, LineRef& d3vLine, qsizetype& posInLine, bool bDirDown, bool bCaseSensitive)
{
    LineRef it = d3vLine;
    qsizetype endIt = bDirDown ? d->getDiff3LineVector()->size() : -1;
    qint32 step = bDirDown ? 1 : -1;
    qsizetype startPos = posInLine;

    for(; it != endIt; it += step)
    {
        QString line = d->getString(it);
        if(!line.isEmpty())
        {
            qsizetype pos = line.indexOf(s, startPos, bCaseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            //TODO: Provide error message when failsafe is triggered.
            if(Q_UNLIKELY(pos > limits<qint32>::max()))
            {
                qCWarning(kdiffMain) << "Skip possiable match line offset to large.";
                continue;
            }

            if(pos != -1)
            {
                d3vLine = it;
                posInLine = pos;

                setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length());
                mVScrollBar->setValue(d->m_selection.beginLine() - DiffTextWindow::mVScrollBar->pageStep() / 2);

                scrollToH(d->m_selection.beginPos());
                return true;
            }

            startPos = 0;
        }
    }
    return false;
}

void DiffTextWindow::convertD3LCoordsToLineCoords(LineType d3LIdx, qsizetype d3LPos, LineRef& line, qsizetype& pos) const
{
    if(d->m_bWordWrap)
    {
        qsizetype wrapPos = d3LPos;
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

void DiffTextWindow::convertLineCoordsToD3LCoords(LineRef line, qsizetype pos, LineType& d3LIdx, qsizetype& d3LPos) const
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

void DiffTextWindow::setSelection(LineRef firstLine, qsizetype startPos, LineRef lastLine, qsizetype endPos)
{
    d->m_selection.reset();
    if(lastLine >= getNofLines())
    {
        lastLine = getNofLines() - 1;

        const Diff3Line* d3l = (*d->getDiff3LineVector())[convertLineToDiff3LineIdx(lastLine)];
        LineRef line;
        if(getWindowIndex() == e_SrcSelector::A) line = d3l->getLineA();
        if(getWindowIndex() == e_SrcSelector::B) line = d3l->getLineB();
        if(getWindowIndex() == e_SrcSelector::C) line = d3l->getLineC();
        if(line.isValid())
            endPos = (*d->m_pLineData)[line].width(gOptions->tabSize());
    }

    if(d->m_bWordWrap && d->getDiff3LineVector() != nullptr)
    {
        QString s1 = d->getString(firstLine);
        LineRef firstWrapLine = convertDiff3LineIdxToLine(firstLine);
        qsizetype wrapStartPos = startPos;
        while(wrapStartPos > d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength)
        {
            wrapStartPos -= d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength;
            s1 = s1.mid(d->m_diff3WrapLineVector[firstWrapLine].wrapLineLength);
            ++firstWrapLine;
        }

        QString s2 = d->getString(lastLine);
        LineRef lastWrapLine = convertDiff3LineIdxToLine(lastLine);
        qsizetype wrapEndPos = endPos;
        while(wrapEndPos > d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength)
        {
            wrapEndPos -= d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength;
            s2 = s2.mid(d->m_diff3WrapLineVector[lastWrapLine].wrapLineLength);
            ++lastWrapLine;
        }

        d->m_selection.start(firstWrapLine, wrapStartPos);
        d->m_selection.end(lastWrapLine, wrapEndPos);
    }
    else
    {
        if(d->getDiff3LineVector() != nullptr)
        {
            d->m_selection.start(firstLine, startPos);
            d->m_selection.end(lastLine, endPos);
        }
    }

    update();
}

LineRef DiffTextWindowData::convertLineOnScreenToLineInSource(const qint32 lineOnScreen, const e_CoordType coordType, const bool bFirstLine) const
{
    LineRef line;
    if(lineOnScreen >= 0)
    {
        if(coordType == eWrapCoords) return lineOnScreen;
        LineType d3lIdx = m_pDiffTextWindow->convertLineToDiff3LineIdx(lineOnScreen);
        if(!bFirstLine && d3lIdx >= SafeInt<LineType>(mDiff3LineVector->size()))
            d3lIdx = SafeInt<LineType>(mDiff3LineVector->size() - 1);
        if(coordType == eD3LLineCoords) return d3lIdx;
        while(!line.isValid() && d3lIdx < SafeInt<LineType>(mDiff3LineVector->size()) && d3lIdx >= 0)
        {
            const Diff3Line* d3l = (*mDiff3LineVector)[d3lIdx];
            if(getWindowIndex() == e_SrcSelector::A) line = d3l->getLineA();
            if(getWindowIndex() == e_SrcSelector::B) line = d3l->getLineB();
            if(getWindowIndex() == e_SrcSelector::C) line = d3l->getLineC();
            if(bFirstLine)
                ++d3lIdx;
            else
                --d3lIdx;
        }
        assert(coordType == eFileCoords);
    }
    return line;
}

void DiffTextWindow::getSelectionRange(LineRef* pFirstLine, LineRef* pLastLine, e_CoordType coordType) const
{
    if(pFirstLine)
        *pFirstLine = d->convertLineOnScreenToLineInSource(d->m_selection.beginLine(), coordType, true);
    if(pLastLine)
        *pLastLine = d->convertLineOnScreenToLineInSource(d->m_selection.endLine(), coordType, false);
}

void DiffTextWindow::convertSelectionToD3LCoords() const
{
    if(d->getDiff3LineVector() == nullptr || !updatesEnabled() || !isVisible() || d->m_selection.isEmpty())
    {
        return;
    }

    // convert the d->m_selection to unwrapped coordinates: Later restore to new coords
    LineType firstD3LIdx;
    qsizetype firstD3LPos;
    qsizetype firstPosInText = d->m_selection.beginPos();
    convertLineCoordsToD3LCoords(d->m_selection.beginLine(), firstPosInText, firstD3LIdx, firstD3LPos);

    LineType lastD3LIdx;
    qsizetype lastD3LPos;
    qsizetype lastPosInText = d->m_selection.endPos();
    convertLineCoordsToD3LCoords(d->m_selection.endLine(), lastPosInText, lastD3LIdx, lastD3LPos);

    d->m_selection.start(firstD3LIdx, firstD3LPos);
    d->m_selection.end(lastD3LIdx, lastD3LPos);
}

bool DiffTextWindow::startRunnables()
{
    assert(RecalcWordWrapThread::s_maxNofRunnables == 0);

    if(s_runnables.size() == 0)
    {
        return false;
    }
    else
    {
        g_pProgressDialog->setStayHidden(true);
        ProgressProxy::startBackgroundTask();
        g_pProgressDialog->setMaxNofSteps(s_runnables.size());
        RecalcWordWrapThread::s_maxNofRunnables = s_runnables.size();
        g_pProgressDialog->setCurrent(0);

        for(size_t i = 0; i < s_runnables.size(); ++i)
        {
            s_runnables[i]->start();
        }

        s_runnables.clear();
        return true;
    }
}

void DiffTextWindow::recalcWordWrap(bool bWordWrap, size_t wrapLineVectorSize, qint32 visibleTextWidth)
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
        d->m_diff3WrapLineVector.resize(wrapLineVectorSize);

        if(wrapLineVectorSize == 0)
        {
            d->m_wrapLineCacheList.clear();
            setUpdatesEnabled(false);
            for(size_t i = 0, j = 0; i < d->getDiff3LineVector()->size(); i += s_linesPerRunnable, ++j)
            {
                d->m_wrapLineCacheList.push_back(std::vector<WrapLineCacheData>());
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
        if(wrapLineVectorSize == 0 && d->m_maxTextWidth.loadRelaxed() < 0)
        {
            d->m_diff3WrapLineVector.resize(0);
            d->m_wrapLineCacheList.clear();
            setUpdatesEnabled(false);
            for(size_t i = 0, j = 0; i < d->getDiff3LineVector()->size(); i += s_linesPerRunnable, ++j)
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

void DiffTextWindow::recalcWordWrapHelper(size_t wrapLineVectorSize, qint32 visibleTextWidth, size_t cacheListIdx)
{
    if(d->m_bWordWrap)
    {
        if(ProgressProxy::wasCancelled())
            return;
        if(visibleTextWidth < 0)
            visibleTextWidth = getVisibleTextAreaWidth();
        else //TODO: Drop after line number area is converted to a QWidget.
            visibleTextWidth -= d->leftInfoWidth() * fontMetrics().horizontalAdvance('0');
        LineType i;
        size_t wrapLineIdx = 0;
        size_t size = d->getDiff3LineVector()->size();
        LineType firstD3LineIdx = SafeInt<LineType>(wrapLineVectorSize > 0 ? 0 : cacheListIdx * s_linesPerRunnable);
        LineType endIdx = SafeInt<LineType>(wrapLineVectorSize > 0 ? size : std::min<size_t>(firstD3LineIdx + s_linesPerRunnable, size));
        std::vector<WrapLineCacheData>& wrapLineCache = d->m_wrapLineCacheList[cacheListIdx];
        size_t cacheListIdx2 = 0;
        QTextLayout textLayout(QString(), font(), this);

        for(i = firstD3LineIdx; i < endIdx; ++i)
        {
            if(ProgressProxy::wasCancelled())
                return;

            LineType linesNeeded = 0;
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
            else if(wrapLineVectorSize > 0 && cacheListIdx2 < d->m_wrapLineCacheList.size())
            {
                WrapLineCacheData* pWrapLineCache = d->m_wrapLineCacheList[cacheListIdx2].data();
                size_t cacheIdx = 0;
                size_t clc = d->m_wrapLineCacheList.size() - 1;
                size_t cllc = d->m_wrapLineCacheList.back().size();
                size_t curCount = d->m_wrapLineCacheList[cacheListIdx2].size() - 1;
                LineType l = 0;

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
                        if(cacheListIdx2 >= d->m_wrapLineCacheList.size())
                            break;
                        pWrapLineCache = d->m_wrapLineCacheList[cacheListIdx2].data();
                        curCount = d->m_wrapLineCacheList[cacheListIdx2].size();
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
            assert(wrapLineVectorSize <= limits<LineType>::max()); //Posiable but unlikely starting in Qt6
            d->m_firstLine = std::min<LineRef>(d->m_firstLine, wrapLineVectorSize - 1);
            d->m_horizScrollOffset = 0;

            Q_EMIT firstLineChanged(d->m_firstLine);
        }
    }
    else // no word wrap, just calc the maximum text width
    {
        if(ProgressProxy::wasCancelled())
            return;

        size_t size = d->getDiff3LineVector()->size();
        LineType firstD3LineIdx = SafeInt<LineType>(cacheListIdx * s_linesPerRunnable);
        LineType endIdx = std::min(firstD3LineIdx + s_linesPerRunnable, (LineType)size);

        qint32 maxTextWidth = d->m_maxTextWidth.loadRelaxed(); // current value
        QTextLayout textLayout(QString(), font(), this);
        for(LineType i = firstD3LineIdx; i < endIdx; ++i)
        {
            if(ProgressProxy::wasCancelled())
                return;
            textLayout.clearLayout();
            textLayout.setText(d->getString(i));
            d->prepareTextLayout(textLayout);
            if(textLayout.maximumWidth() > maxTextWidth)
                maxTextWidth = qCeil(textLayout.maximumWidth());
        }

        for(qint32 prevMaxTextWidth = d->m_maxTextWidth.fetchAndStoreOrdered(maxTextWidth);
            prevMaxTextWidth > maxTextWidth;
            prevMaxTextWidth = d->m_maxTextWidth.fetchAndStoreOrdered(maxTextWidth))
        {
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
        qsizetype firstPos;
        convertD3LCoordsToLineCoords(d->m_selection.beginLine(), d->m_selection.beginPos(), firstLine, firstPos);

        LineRef lastLine;
        qsizetype lastPos;
        convertD3LCoordsToLineCoords(d->m_selection.endLine(), d->m_selection.endPos(), lastLine, lastPos);

        d->m_selection.start(firstLine, firstPos);
        d->m_selection.end(lastLine, lastPos);
    }
}

DiffTextWindowFrame::DiffTextWindowFrame(QWidget* pParent, e_SrcSelector winIdx, const QSharedPointer<SourceData>& psd, KDiff3App& app):
    QWidget(pParent)
{
    m_winIdx = winIdx;

    m_pTopLineWidget = new QWidget(this);
    m_pFileSelection = new FileNameLineEdit(m_pTopLineWidget);
    m_pBrowseButton = new QPushButton("...", m_pTopLineWidget);
    m_pBrowseButton->setFixedWidth(30);

    m_pFileSelection->setAcceptDrops(true);

    m_pLabel = new QLabel("A:", m_pTopLineWidget);
    m_pTopLine = new QLabel(m_pTopLineWidget);

    mSourceData = psd;
    setAutoFillBackground(true);
    chk_connect_a(m_pBrowseButton, &QPushButton::clicked, this, &DiffTextWindowFrame::slotBrowseButtonClicked);
    chk_connect_a(m_pFileSelection, &QLineEdit::returnPressed, this, &DiffTextWindowFrame::slotReturnPressed);

    m_pDiffTextWindow = new DiffTextWindow(this, winIdx, app);
    m_pDiffTextWindow->setSourceData(psd);
    QVBoxLayout* pVTopLayout = new QVBoxLayout(m_pTopLineWidget);
    pVTopLayout->setContentsMargins(2, 2, 2, 2);
    pVTopLayout->setSpacing(0);
    QHBoxLayout* pHL = new QHBoxLayout();
    QHBoxLayout* pHL2 = new QHBoxLayout();
    pVTopLayout->addLayout(pHL);
    pVTopLayout->addLayout(pHL2);

    // Upper line:
    pHL->setContentsMargins(0, 0, 0, 0);
    pHL->setSpacing(2);

    pHL->addWidget(m_pLabel, 0);
    pHL->addWidget(m_pFileSelection, 1);
    pHL->addWidget(m_pBrowseButton, 0);
    pHL->addWidget(m_pTopLine, 0);

    // Lower line
    pHL2->setContentsMargins(0, 0, 0, 0);
    pHL2->setSpacing(2);
    pHL2->addWidget(m_pTopLine, 0);
    m_pEncoding = new EncodingLabel(i18n("Encoding:"), psd);
    //EncodeLabel::EncodingChanged should be handled asyncroniously.
    chk_connect_q((EncodingLabel*)m_pEncoding, &EncodingLabel::encodingChanged, this, &DiffTextWindowFrame::slotEncodingChanged);

    m_pLineEndStyle = new QLabel(i18n("Line end style:"));
    pHL2->addWidget(m_pEncoding);
    pHL2->addWidget(m_pLineEndStyle);

    QVBoxLayout* pVL = new QVBoxLayout(this);
    pVL->setContentsMargins(0, 0, 0, 0);
    pVL->setSpacing(0);
    pVL->addWidget(m_pTopLineWidget, 0);
    pVL->addWidget(m_pDiffTextWindow, 1);

    m_pDiffTextWindow->installEventFilter(this);
    m_pFileSelection->installEventFilter(this);
    m_pBrowseButton->installEventFilter(this);
    init();
}

DiffTextWindowFrame::~DiffTextWindowFrame() = default;

void DiffTextWindowFrame::init()
{
    QPointer<DiffTextWindow> pDTW = m_pDiffTextWindow;
    if(pDTW)
    {
        QString s = QDir::toNativeSeparators(pDTW->getFileName());
        m_pFileSelection->setText(s);
        QString winId = pDTW->getWindowIndex() == e_SrcSelector::A ? (pDTW->isThreeWay() ? i18n("A (Base)") : QStringLiteral("A")) : (pDTW->getWindowIndex() == e_SrcSelector::B ? QStringLiteral("B") : QStringLiteral("C"));
        m_pLabel->setText(winId + ':');
        m_pEncoding->setText(i18n("Encoding: %1", pDTW->getEncodingDisplayString()));
        m_pLineEndStyle->setText(i18n("Line end style: %1", pDTW->getLineEndStyle() == eLineEndStyleDos ? i18n("DOS") : pDTW->getLineEndStyle() == eLineEndStyleUnix ? i18n("Unix") :
                                                                                                                                                                       i18n("Unknown")));
    }
}

void DiffTextWindowFrame::setupConnections(const KDiff3App* app)
{
    chk_connect_a(this, &DiffTextWindowFrame::fileNameChanged, app, &KDiff3App::slotFileNameChanged);
    chk_connect_a(this, &DiffTextWindowFrame::encodingChanged, app, &KDiff3App::slotEncodingChanged);
}

// Search for the first visible line (search loop needed when no line exists for this file.)
LineRef DiffTextWindow::calcTopLineInFile(const LineRef firstLine) const
{
    LineRef currentLine;
    for(size_t i = convertLineToDiff3LineIdx(firstLine); i < d->getDiff3LineVector()->size(); ++i)
    {
        const Diff3Line* d3l = (*d->getDiff3LineVector())[i];
        currentLine = d3l->getLineInFile(getWindowIndex());
        if(currentLine.isValid()) break;
    }
    return currentLine;
}

void DiffTextWindowFrame::setFirstLine(const LineRef firstLine)
{
    QPointer<DiffTextWindow> pDTW = m_pDiffTextWindow;
    if(pDTW && pDTW->getDiff3LineVector())
    {
        QString s = i18n("Top line");
        qint32 lineNumberWidth = pDTW->getLineNumberWidth();

        LineRef topVisiableLine = pDTW->calcTopLineInFile(firstLine);

        const QString widthString = QString().fill('0', lineNumberWidth);
        qint32 w = m_pTopLine->fontMetrics().horizontalAdvance(s + ' ' + widthString);
        m_pTopLine->setMinimumWidth(w);

        if(!topVisiableLine.isValid())
            s = i18n("End");
        else
            s += ' ' + QString::number(topVisiableLine + 1);

        m_pTopLine->setText(s);
        m_pTopLine->repaint();
    }
}

QPointer<DiffTextWindow> DiffTextWindowFrame::getDiffTextWindow()
{
    return m_pDiffTextWindow;
}

bool DiffTextWindowFrame::eventFilter([[maybe_unused]] QObject* o, QEvent* e)
{
    if(e->type() == QEvent::FocusIn || e->type() == QEvent::FocusOut)
    {
        QColor c1 = gOptions->backgroundColor();
        QColor c2;
        if(m_winIdx == e_SrcSelector::A)
            c2 = gOptions->aColor();
        else if(m_winIdx == e_SrcSelector::B)
            c2 = gOptions->bColor();
        else if(m_winIdx == e_SrcSelector::C)
            c2 = gOptions->cColor();

        QPalette p = m_pTopLineWidget->palette();
        if(e->type() == QEvent::FocusOut)
            std::swap(c1, c2);

        p.setColor(QPalette::Window, c2);
        setPalette(p);

        p.setColor(QPalette::WindowText, c1);
        m_pLabel->setPalette(p);
        m_pTopLine->setPalette(p);
        m_pEncoding->setPalette(p);
        m_pLineEndStyle->setPalette(p);
    }

    return false;
}

void DiffTextWindowFrame::slotReturnPressed()
{
    if(m_pDiffTextWindow->getFileName() != m_pFileSelection->text())
    {
        Q_EMIT fileNameChanged(m_pFileSelection->text(), m_pDiffTextWindow->getWindowIndex());
    }
}

void DiffTextWindowFrame::slotBrowseButtonClicked()
{
    QString current = m_pFileSelection->text();

    QUrl newURL = QFileDialog::getOpenFileUrl(this, i18n("Open File"), QUrl::fromUserInput(current, QString(), QUrl::AssumeLocalFile));
    if(!newURL.isEmpty())
    {
        Q_EMIT fileNameChanged(newURL.url(), m_pDiffTextWindow->getWindowIndex());
    }
}

void DiffTextWindowFrame::slotEncodingChanged(const QByteArray& name)
{
    Q_EMIT encodingChanged(name); //relay signal from encoding label
    mSourceData->setEncoding(name);
}

EncodingLabel::EncodingLabel(const QString& text, const QSharedPointer<SourceData>& pSD):
    QLabel(text)
{
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

        const QByteArray& currentTextCodec = m_pSourceData->getEncoding(); // the codec that will be checked in the context menu

        const QStringList names= Utils::availableCodecs();
        QList<QByteArray> codecNameList;
        // Adding "main" encodings
        insertCodec(i18n("Unicode, 8 bit"), "UTF-8", codecNameList, m_pContextEncodingMenu, currentTextCodec);
        insertCodec(i18n("Unicode, 8 bit (BOM)"), "UTF-8-BOM", codecNameList, m_pContextEncodingMenu, currentTextCodec);
        insertCodec(i18n("System"), QStringConverter::nameForEncoding(QStringConverter::System), codecNameList, m_pContextEncodingMenu, currentTextCodec);

        // Adding recent encodings
        if(gOptions != nullptr)
        {
            const RecentItems<maxNofRecentCodecs>& recentEncodings = gOptions->m_recentEncodings;
            for(const QString& s: recentEncodings)
            {
                insertCodec("", s.toLatin1(), codecNameList, m_pContextEncodingMenu, currentTextCodec);
            }
        }
        // Submenu to add the rest of available encodings
        pContextEncodingSubMenu->setTitle(i18n("Other"));
        for(const QString& name: names)
        {
            insertCodec("", name.toLatin1(), codecNameList, pContextEncodingSubMenu, currentTextCodec);
        }

        m_pContextEncodingMenu->addMenu(pContextEncodingSubMenu);

        m_pContextEncodingMenu->exec(QCursor::pos());
    }
}

void EncodingLabel::insertCodec(const QString& visibleCodecName, const QByteArray& nameArray, QList<QByteArray>& codecList, QMenu* pMenu, const QByteArray& currentTextCodecName) const
{
    if(!codecList.contains(nameArray))
    {
        QAction* pAction = new QAction(pMenu); // menu takes ownership, so deleting the menu deletes the action too.
        const QLatin1String codecName = QLatin1String(nameArray);

        pAction->setText(visibleCodecName.isEmpty() ? codecName : visibleCodecName + u8" (" + codecName + u8")");
        pAction->setData(nameArray);
        pAction->setCheckable(true);
        if(currentTextCodecName == nameArray)
            pAction->setChecked(true);
        pMenu->addAction(pAction);
        chk_connect_a(pAction, &QAction::triggered, this, &EncodingLabel::slotSelectEncoding);
        codecList.append(nameArray);
    }
}

void EncodingLabel::slotSelectEncoding()
{
    QAction* pAction = qobject_cast<QAction*>(sender());
    if(pAction)
    {
        QByteArray codecName = pAction->data().toByteArray();
        QString s{QLatin1String(codecName)};
        RecentItems<maxNofRecentCodecs>& recentEncodings = gOptions->m_recentEncodings;
        if(s != "UTF-8" && s != "System")
        {
            recentEncodings.push_front(s);
        }

        Q_EMIT encodingChanged(codecName);
    }
}
