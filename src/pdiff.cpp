/*
 *  This file is part of KDiff3.
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "difftextwindow.h"
#include "DirectoryInfo.h"
#include "directorymergewindow.h"
#include "fileaccess.h"
#include "Logging.h"
#include "kdiff3.h"
#include "optiondialog.h"
#include "progress.h"
#include "Utils.h"

#include "mergeresultwindow.h"
#include "smalldialogs.h"

#include <algorithm>
#include <cstdio>
#include <list>

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QEvent> // QKeyEvent, QDropEvent, QInputEvent
#include <QFile>
#include <QLayout>
#include <QLineEdit>
#include <QMimeData>
#include <QPointer>
#include <QProcess>
#include <QScrollBar>
#include <QSplitter>
#include <QStatusBar>
#include <QStringList>
#include <QTextCodec>
#include <QUrl>

#include <KLocalizedString>
#include <KMessageBox>
#include <KShortcutsDialog>

// Function uses setMinSize( sizeHint ) before adding the widget.
// void addWidget(QBoxLayout* layout, QWidget* widget);
template <class W, class L>
void addWidget(L* layout, W* widget)
{
    QSize s = widget->sizeHint();
    widget->setMinimumSize(QSize(std::max(s.width(), 0), std::max(s.height(), 0)));
    layout->addWidget(widget);
}

void KDiff3App::mainInit(TotalDiffStatus* pTotalDiffStatus, bool bLoadFiles, bool bUseCurrentEncoding)
{
    ProgressProxy pp;
    QStringList errors;
    // When doing a full analysis in the directory-comparison, then the statistics-results
    // will be stored in the given TotalDiffStatus. Otherwise it will be 0.
    bool bGUI = pTotalDiffStatus == nullptr;
    if(pTotalDiffStatus == nullptr)
        pTotalDiffStatus = &m_totalDiffStatus;

    //bool bPreserveCarriageReturn = m_pOptions->m_bPreserveCarriageReturn;

    bool bVisibleMergeResultWindow = !m_outputFilename.isEmpty();
    if(bGUI)
    {
        if(bVisibleMergeResultWindow && !m_pOptions->m_PreProcessorCmd.isEmpty())
        {
            QString msg = "- " + i18n("PreprocessorCmd: ") + m_pOptions->m_PreProcessorCmd + '\n';
            int result = KMessageBox::warningYesNo(this,
                i18n("The following option(s) you selected might change data:\n") + msg +
                    i18n("\nMost likely this is not wanted during a merge.\n"
                         "Do you want to disable these settings or continue with these settings active?"),
                i18n("Option Unsafe for Merging"),
                KGuiItem(i18n("Use These Options During Merge")),
                KGuiItem(i18n("Disable Unsafe Options")));

            if(result == KMessageBox::No)
            {
                m_pOptions->m_PreProcessorCmd = "";
            }
        }

        // Because of the progressdialog paintevents can occur, but data is invalid,
        // so painting must be suppressed
        setLockPainting(true);
    }

    //insure merge result window never has stale iterators.
    if(m_pMergeResultWindow) m_pMergeResultWindow->clearMergeList();
    m_diff3LineList.clear();
    m_diff3LineVector.clear();

    if(bLoadFiles)
    {
        m_manualDiffHelpList.clear();

        if(m_sd3->isEmpty())
            pp.setMaxNofSteps(4); // Read 2 files, 1 comparison, 1 finediff
        else
            pp.setMaxNofSteps(9); // Read 3 files, 3 comparisons, 3 finediffs

        // First get all input data.
        pp.setInformation(i18n("Loading A"));
        qCInfo(kdiffMain) << i18n("Loading A: %1", m_sd1->getFilename());

        if(bUseCurrentEncoding)
            errors = m_sd1->readAndPreprocess(m_sd1->getEncoding(), false);
        else
            errors = m_sd1->readAndPreprocess(m_pOptions->m_pEncodingA, m_pOptions->m_bAutoDetectUnicodeA);

        pp.step();

        pp.setInformation(i18n("Loading B"));
        qCInfo(kdiffMain) << i18n("Loading B: %1", m_sd2->getFilename());

        if(bUseCurrentEncoding)
            errors = m_sd2->readAndPreprocess(m_sd2->getEncoding(), false);
        else
            errors = m_sd2->readAndPreprocess(m_pOptions->m_pEncodingB, m_pOptions->m_bAutoDetectUnicodeB);

        pp.step();
    }
    else
    {
        if(m_sd3->isEmpty())
            pp.setMaxNofSteps(2); // 1 comparison, 1 finediff
        else
            pp.setMaxNofSteps(6); // 3 comparisons, 3 finediffs
    }

    if(pTotalDiffStatus)
        pTotalDiffStatus->reset();
    if(errors.isEmpty())
    {
        // Run the diff.
        if(m_sd3->isEmpty())
        {
            pTotalDiffStatus->setBinaryEqualAB(m_sd1->isBinaryEqualWith(m_sd2));

            if(m_sd1->isText() && m_sd2->isText())
            {
                pp.setInformation(i18n("Diff: A <-> B"));
                qCInfo(kdiffMain) << i18n("Diff: A <-> B");
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_diffList12, e_SrcSelector::A, e_SrcSelector::B,
                                             m_pOptionDialog->getOptions());

                pp.step();

                pp.setInformation(i18n("Linediff: A <-> B"));
                qCInfo(kdiffMain) << i18n("Linediff: A <-> B");
                m_diff3LineList.calcDiff3LineListUsingAB(&m_diffList12);
                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay()));
                if(m_sd1->getSizeBytes() == 0) pTotalDiffStatus->setTextEqualAB(false);

                pp.step();
            }
            else
            {
                pp.step();
                pp.step();
            }
        }
        else
        {
            if(bLoadFiles)
            {
                pp.setInformation(i18n("Loading C"));
                qCInfo(kdiffMain) << i18n("Loading C: %1", m_sd2->getFilename());

                if(bUseCurrentEncoding)
                    errors = m_sd3->readAndPreprocess(m_sd3->getEncoding(), false);
                else
                    errors = m_sd3->readAndPreprocess(m_pOptions->m_pEncodingC, m_pOptions->m_bAutoDetectUnicodeC);

                pp.step();
            }

            pTotalDiffStatus->setBinaryEqualAB(m_sd1->isBinaryEqualWith(m_sd2));
            pTotalDiffStatus->setBinaryEqualAC(m_sd1->isBinaryEqualWith(m_sd3));
            pTotalDiffStatus->setBinaryEqualBC(m_sd3->isBinaryEqualWith(m_sd2));

            pp.setInformation(i18n("Diff: A <-> B"));
            qCInfo(kdiffMain) << i18n("Diff: A <-> B");

            if(m_sd1->isText() && m_sd2->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_diffList12, e_SrcSelector::A, e_SrcSelector::B,
                                             m_pOptionDialog->getOptions());

                m_diff3LineList.calcDiff3LineListUsingAB(&m_diffList12);
            }
            pp.step();

            pp.setInformation(i18n("Diff: A <-> C"));
            qCInfo(kdiffMain) << i18n("Diff: A <-> C");

            if(m_sd1->isText() && m_sd3->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(), m_sd3->getLineDataForDiff(), m_sd3->getSizeLines(), m_diffList13, e_SrcSelector::A, e_SrcSelector::C,
                                             m_pOptionDialog->getOptions());

                m_diff3LineList.calcDiff3LineListUsingAC(&m_diffList13);
                m_diff3LineList.correctManualDiffAlignment(&m_manualDiffHelpList);
                m_diff3LineList.calcDiff3LineListTrim(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), &m_manualDiffHelpList);
            }
            pp.step();

            pp.setInformation(i18n("Diff: B <-> C"));
            qCInfo(kdiffMain) << i18n("Diff: B <-> C");

            if(m_sd2->isText() && m_sd3->isText())
            {
                m_manualDiffHelpList.runDiff(m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(), m_sd3->getLineDataForDiff(), m_sd3->getSizeLines(), m_diffList23, e_SrcSelector::B, e_SrcSelector::C,
                                             m_pOptionDialog->getOptions());
                if(m_pOptions->m_bDiff3AlignBC)
                {
                    m_diff3LineList.calcDiff3LineListUsingBC(&m_diffList23);
                    m_diff3LineList.correctManualDiffAlignment(&m_manualDiffHelpList);
                    m_diff3LineList.calcDiff3LineListTrim(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff(), &m_manualDiffHelpList);
                }
            }
            pp.step();

            m_diff3LineList.debugLineCheck(m_sd1->getSizeLines(), e_SrcSelector::A);
            m_diff3LineList.debugLineCheck(m_sd2->getSizeLines(), e_SrcSelector::B);
            m_diff3LineList.debugLineCheck(m_sd3->getSizeLines(), e_SrcSelector::C);

            pp.setInformation(i18n("Linediff: A <-> B"));
            qCInfo(kdiffMain) << i18n("Linediff: A <-> B");
            if(m_sd1->hasData() && m_sd2->hasData() && m_sd1->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay()));
            pp.step();

            pp.setInformation(i18n("Linediff: B <-> C"));
            qCInfo(kdiffMain) << i18n("Linediff: B <-> C");
            if(m_sd2->hasData() && m_sd3->hasData() && m_sd2->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualBC(m_diff3LineList.fineDiff(e_SrcSelector::B, m_sd2->getLineDataForDisplay(), m_sd3->getLineDataForDisplay()));
            pp.step();

            pp.setInformation(i18n("Linediff: A <-> C"));
            qCInfo(kdiffMain) << i18n("Linediff: A <-> C");
            if(m_sd1->hasData() && m_sd3->hasData() && m_sd1->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualAC(m_diff3LineList.fineDiff(e_SrcSelector::C, m_sd3->getLineDataForDisplay(), m_sd1->getLineDataForDisplay()));
            m_diff3LineList.debugLineCheck(m_sd2->getSizeLines(), e_SrcSelector::B);
            m_diff3LineList.debugLineCheck(m_sd3->getSizeLines(), e_SrcSelector::C);

            pp.setInformation(i18n("Linediff: A <-> B"));
            if(m_sd1->hasData() && m_sd2->hasData() && m_sd1->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualAB(m_diff3LineList.fineDiff(e_SrcSelector::A, m_sd1->getLineDataForDisplay(), m_sd2->getLineDataForDisplay()));
            pp.step();

            pp.setInformation(i18n("Linediff: B <-> C"));
            if(m_sd3->hasData() && m_sd2->hasData() && m_sd3->isText() && m_sd2->isText())
                pTotalDiffStatus->setTextEqualBC(m_diff3LineList.fineDiff(e_SrcSelector::B, m_sd2->getLineDataForDisplay(), m_sd3->getLineDataForDisplay()));
            pp.step();

            pp.setInformation(i18n("Linediff: A <-> C"));
            if(m_sd1->hasData() && m_sd3->hasData() && m_sd1->isText() && m_sd3->isText())
                pTotalDiffStatus->setTextEqualAC(m_diff3LineList.fineDiff(e_SrcSelector::C, m_sd3->getLineDataForDisplay(), m_sd1->getLineDataForDisplay()));
            pp.step();
            if(m_sd1->getSizeBytes() == 0)
            {
                pTotalDiffStatus->setTextEqualAB(false);
                pTotalDiffStatus->setTextEqualAC(false);
            }
            if(m_sd2->getSizeBytes() == 0)
            {
                pTotalDiffStatus->setTextEqualAB(false);
                pTotalDiffStatus->setTextEqualBC(false);
            }
        }
    }
    else
    {
        pp.clear();
    }

    if(errors.isEmpty() && m_sd1->isText() && m_sd2->isText())
    {
        m_diffBufferInfo->init(&m_diff3LineList, &m_diff3LineVector,
                               m_sd1->getLineDataForDiff(), m_sd1->getSizeLines(),
                               m_sd2->getLineDataForDiff(), m_sd2->getSizeLines(),
                               m_sd3->getLineDataForDiff(), m_sd3->getSizeLines());
        Diff3Line::m_pDiffBufferInfo = m_diffBufferInfo;

        m_diff3LineList.calcWhiteDiff3Lines(m_sd1->getLineDataForDiff(), m_sd2->getLineDataForDiff(), m_sd3->getLineDataForDiff());
        m_diff3LineList.calcDiff3LineVector(m_diff3LineVector);
    }

    // Calc needed lines for display
    m_neededLines = m_diff3LineList.size();

    QList<int> oldHeights;
    if(m_pDirectoryMergeSplitter->isVisible())
        oldHeights = m_pMainSplitter->sizes();

    initView();
    m_pMergeResultWindow->connectActions();

    if(m_pDirectoryMergeSplitter->isVisible())
    {
        if(oldHeights.count() < 2)
            oldHeights.append(0);
        if(oldHeights[1] == 0) // Distribute the available space evenly between the two widgets.
        {
            oldHeights[1] = oldHeights[0] / 2;
            oldHeights[0] -= oldHeights[1];
        }
        if(oldHeights[0] == 0 && oldHeights[1] == 0)
        {
            oldHeights[1] = 100;
            oldHeights[0] = 100;
        }
        m_pMainSplitter->setSizes(oldHeights);
    }

    m_pMainWidget->setVisible(bGUI);

    m_bTripleDiff = !m_sd3->isEmpty();

    m_pMergeResultWindowTitle->setEncodings(m_sd1->getEncoding(), m_sd2->getEncoding(), m_sd3->getEncoding());
    if(!m_pOptions->m_bAutoSelectOutEncoding)
        m_pMergeResultWindowTitle->setEncoding(m_pOptions->m_pEncodingOut);

    m_pMergeResultWindowTitle->setLineEndStyles(m_sd1->getLineEndStyle(), m_sd2->getLineEndStyle(), m_sd3->getLineEndStyle());

    if(bGUI)
    {
        const ManualDiffHelpList* pMDHL = &m_manualDiffHelpList;
        m_pDiffTextWindow1->init(m_sd1->getAliasName(), m_sd1->getEncoding(), m_sd1->getLineEndStyle(),
                                 m_sd1->getLineDataForDisplay(), m_sd1->getSizeLines(), &m_diff3LineVector, pMDHL);
        m_pDiffTextWindowFrame1->init();

        m_pDiffTextWindow2->init(m_sd2->getAliasName(), m_sd2->getEncoding(), m_sd2->getLineEndStyle(),
                                 m_sd2->getLineDataForDisplay(), m_sd2->getSizeLines(), &m_diff3LineVector, pMDHL);
        m_pDiffTextWindowFrame2->init();

        m_pDiffTextWindow3->init(m_sd3->getAliasName(), m_sd3->getEncoding(), m_sd3->getLineEndStyle(),
                                 m_sd3->getLineDataForDisplay(), m_sd3->getSizeLines(), &m_diff3LineVector, pMDHL);
        m_pDiffTextWindowFrame3->init();

        m_pDiffTextWindowFrame3->setVisible(m_bTripleDiff);
    }

    m_bOutputModified = bVisibleMergeResultWindow;

    m_pMergeResultWindow->init(
        m_sd1->getLineDataForDisplay(), m_sd1->getSizeLines(),
        m_sd2->getLineDataForDisplay(), m_sd2->getSizeLines(),
        m_bTripleDiff ? m_sd3->getLineDataForDisplay() : nullptr, m_sd3->getSizeLines(),
        &m_diff3LineList,
        pTotalDiffStatus);
    m_pMergeResultWindowTitle->setFileName(m_outputFilename.isEmpty() ? QString("unnamed.txt") : m_outputFilename);

    if(!bGUI)
    {
        // We now have all needed information. The rest below is only for GUI-activation.
        m_sd1->reset();
        m_sd2->reset();
        m_sd3->reset();
    }
    else
    {
        m_pOverview->init(&m_diff3LineList);
        DiffTextWindow::mVScrollBar->setValue(0);
        m_pHScrollBar->setValue(0);
        MergeResultWindow::mVScrollBar->setValue(0);
        setLockPainting(false);

        if(!bVisibleMergeResultWindow)
            m_pMergeWindowFrame->hide();
        else
            m_pMergeWindowFrame->show();

        // Try to create a meaningful but not too long caption
        if(!isPart() && errors.isEmpty())
        {
            createCaption();
        }
        m_bFinishMainInit = true; // call slotFinishMainInit after finishing the word wrap
        m_bLoadFiles = bLoadFiles;
        postRecalcWordWrap();
    }
}

void KDiff3App::setLockPainting(bool bLock)
{
    if(m_pDiffTextWindow1) m_pDiffTextWindow1->setPaintingAllowed(!bLock);
    if(m_pDiffTextWindow2) m_pDiffTextWindow2->setPaintingAllowed(!bLock);
    if(m_pDiffTextWindow3) m_pDiffTextWindow3->setPaintingAllowed(!bLock);
    if(m_pOverview) m_pOverview->setPaintingAllowed(!bLock);
    if(m_pMergeResultWindow) m_pMergeResultWindow->setPaintingAllowed(!bLock);
}

void KDiff3App::createCaption()
{
    // Try to create a meaningful but not too long caption
    // 1. If the filenames are equal then show only one filename
    QString caption;
    QString f1 = m_sd1->getAliasName();
    QString f2 = m_sd2->getAliasName();
    QString f3 = m_sd3->getAliasName();
    int p;

    if((p = f1.lastIndexOf('/')) >= 0 || (p = f1.lastIndexOf('\\')) >= 0)
        f1 = f1.mid(p + 1);
    if((p = f2.lastIndexOf('/')) >= 0 || (p = f2.lastIndexOf('\\')) >= 0)
        f2 = f2.mid(p + 1);
    if((p = f3.lastIndexOf('/')) >= 0 || (p = f3.lastIndexOf('\\')) >= 0)
        f3 = f3.mid(p + 1);

    if(!f1.isEmpty())
    {
        if((f2.isEmpty() && f3.isEmpty()) ||
           (f2.isEmpty() && f1 == f3) || (f3.isEmpty() && f1 == f2) || (f1 == f2 && f1 == f3))
            caption = f1;
    }
    else if(!f2.isEmpty())
    {
        if(f3.isEmpty() || f2 == f3)
            caption = f2;
    }
    else if(!f3.isEmpty())
        caption = f3;

    // 2. If the files don't have the same name then show all names
    if(caption.isEmpty() && (!f1.isEmpty() || !f2.isEmpty() || !f3.isEmpty()))
    {
        caption = (f1.isEmpty() ? QString("") : f1);
        caption += QLatin1String(caption.isEmpty() || f2.isEmpty() ? "" : " <-> ") + (f2.isEmpty() ? QString("") : f2);
        caption += QLatin1String(caption.isEmpty() || f3.isEmpty() ? "" : " <-> ") + (f3.isEmpty() ? QString("") : f3);
    }

    m_pKDiff3Shell->setWindowTitle(caption.isEmpty() ? QString("KDiff3") : caption + QString(" - KDiff3"));
}

void KDiff3App::setHScrollBarRange()
{
    int w1 = m_pDiffTextWindow1 != nullptr && m_pDiffTextWindow1->isVisible() ? m_pDiffTextWindow1->getMaxTextWidth() : 0;
    int w2 = m_pDiffTextWindow2 != nullptr && m_pDiffTextWindow2->isVisible() ? m_pDiffTextWindow2->getMaxTextWidth() : 0;
    int w3 = m_pDiffTextWindow3 != nullptr && m_pDiffTextWindow3->isVisible() ? m_pDiffTextWindow3->getMaxTextWidth() : 0;

    int wm = m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible() ? m_pMergeResultWindow->getMaxTextWidth() : 0;

    int v1 = m_pDiffTextWindow1 != nullptr && m_pDiffTextWindow1->isVisible() ? m_pDiffTextWindow1->getVisibleTextAreaWidth() : 0;
    int v2 = m_pDiffTextWindow2 != nullptr && m_pDiffTextWindow2->isVisible() ? m_pDiffTextWindow2->getVisibleTextAreaWidth() : 0;
    int v3 = m_pDiffTextWindow3 != nullptr && m_pDiffTextWindow3->isVisible() ? m_pDiffTextWindow3->getVisibleTextAreaWidth() : 0;
    int vm = m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible() ? m_pMergeResultWindow->getVisibleTextAreaWidth() : 0;

    // Find the minimum, but don't consider 0.
    int pageStep = 0;
    if((pageStep == 0 || pageStep > v1) && v1 > 0)
        pageStep = v1;
    if((pageStep == 0 || pageStep > v2) && v2 > 0)
        pageStep = v2;
    if((pageStep == 0 || pageStep > v3) && v3 > 0)
        pageStep = v3;
    if((pageStep == 0 || pageStep > vm) && vm > 0)
        pageStep = vm;

    int rangeMax = 0;
    if(w1 > v1 && w1 - v1 > rangeMax && v1 > 0)
        rangeMax = w1 - v1;
    if(w2 > v2 && w2 - v2 > rangeMax && v2 > 0)
        rangeMax = w2 - v2;
    if(w3 > v3 && w3 - v3 > rangeMax && v3 > 0)
        rangeMax = w3 - v3;
    if(wm > vm && wm - vm > rangeMax && vm > 0)
        rangeMax = wm - vm;

    m_pHScrollBar->setRange(0, rangeMax);
    m_pHScrollBar->setPageStep(pageStep);
}

void KDiff3App::resizeDiffTextWindowHeight(int newHeight)
{
    m_DTWHeight = newHeight;

    DiffTextWindow::mVScrollBar->setRange(0, std::max(0, m_neededLines + 1 - newHeight));
    DiffTextWindow::mVScrollBar->setPageStep(newHeight);
    m_pOverview->setRange(DiffTextWindow::mVScrollBar->value(), DiffTextWindow::mVScrollBar->pageStep());

    setHScrollBarRange();
}

void KDiff3App::scrollDiffTextWindow(int deltaX, int deltaY)
{
    if(deltaY != 0 && DiffTextWindow::mVScrollBar != nullptr)
    {
        DiffTextWindow::mVScrollBar->setValue(DiffTextWindow::mVScrollBar->value() + deltaY);
    }
    if(deltaX != 0 && m_pHScrollBar != nullptr)
        m_pHScrollBar->QScrollBar::setValue(m_pHScrollBar->value() + deltaX);
}

void KDiff3App::scrollMergeResultWindow(int deltaX, int deltaY)
{
    if(deltaY != 0)
        MergeResultWindow::mVScrollBar->setValue(MergeResultWindow::mVScrollBar->value() + deltaY);
    if(deltaX != 0)
        m_pHScrollBar->setValue(m_pHScrollBar->value() + deltaX);
}

void KDiff3App::sourceMask(int srcMask, int enabledMask)
{
    chooseA->blockSignals(true);
    chooseB->blockSignals(true);
    chooseC->blockSignals(true);
    chooseA->setChecked((srcMask & 1) != 0);
    chooseB->setChecked((srcMask & 2) != 0);
    chooseC->setChecked((srcMask & 4) != 0);
    chooseA->blockSignals(false);
    chooseB->blockSignals(false);
    chooseC->blockSignals(false);
    chooseA->setEnabled((enabledMask & 1) != 0);
    chooseB->setEnabled((enabledMask & 2) != 0);
    chooseC->setEnabled((enabledMask & 4) != 0);
}

void KDiff3App::initView()
{
    // set the main widget here
    if(m_pMainWidget != nullptr)
    {
        return;
        //delete m_pMainWidget;
    }
    m_pMainWidget = new QWidget(); // Contains vertical splitter and horiz scrollbar
    m_pMainSplitter->addWidget(m_pMainWidget);
    m_pMainWidget->setObjectName("MainWidget");
    QVBoxLayout* pVLayout = new QVBoxLayout(m_pMainWidget);
    pVLayout->setMargin(0);
    pVLayout->setSpacing(0);

    QSplitter* pVSplitter = new QSplitter();
    pVSplitter->setObjectName("VSplitter");
    pVSplitter->setOpaqueResize(false);
    pVSplitter->setOrientation(Qt::Vertical);
    pVLayout->addWidget(pVSplitter);

    QWidget* pDiffWindowFrame = new QWidget(); // Contains diff windows, overview and vert scrollbar
    pDiffWindowFrame->setObjectName("DiffWindowFrame");
    QHBoxLayout* pDiffHLayout = new QHBoxLayout(pDiffWindowFrame);
    pDiffHLayout->setMargin(0);
    pDiffHLayout->setSpacing(0);
    pVSplitter->addWidget(pDiffWindowFrame);

    m_pDiffWindowSplitter = new QSplitter();
    m_pDiffWindowSplitter->setObjectName("DiffWindowSplitter");
    m_pDiffWindowSplitter->setOpaqueResize(false);

    m_pDiffWindowSplitter->setOrientation(m_pOptions->m_bHorizDiffWindowSplitting ? Qt::Horizontal : Qt::Vertical);
    pDiffHLayout->addWidget(m_pDiffWindowSplitter);

    m_pOverview = new Overview(m_pOptionDialog->getOptions());
    m_pOverview->setObjectName("Overview");
    pDiffHLayout->addWidget(m_pOverview);

    DiffTextWindow::mVScrollBar = new QScrollBar(Qt::Vertical, pDiffWindowFrame);
    pDiffHLayout->addWidget(DiffTextWindow::mVScrollBar);

    chk_connect_a(m_pOverview, &Overview::setLine, DiffTextWindow::mVScrollBar, &QScrollBar::setValue);
    chk_connect_a(this, &KDiff3App::showWhiteSpaceToggled, m_pOverview, &Overview::slotRedraw);
    chk_connect_a(this, &KDiff3App::changeOverViewMode, m_pOverview, &Overview::setOverviewMode);

    m_pDiffTextWindowFrame1 = new DiffTextWindowFrame(m_pDiffWindowSplitter, m_pOptionDialog->getOptions(), e_SrcSelector::A, m_sd1);
    m_pDiffWindowSplitter->addWidget(m_pDiffTextWindowFrame1);
    m_pDiffTextWindowFrame2 = new DiffTextWindowFrame(m_pDiffWindowSplitter, m_pOptionDialog->getOptions(), e_SrcSelector::B, m_sd2);
    m_pDiffWindowSplitter->addWidget(m_pDiffTextWindowFrame2);
    m_pDiffTextWindowFrame3 = new DiffTextWindowFrame(m_pDiffWindowSplitter, m_pOptionDialog->getOptions(), e_SrcSelector::C, m_sd3);
    m_pDiffWindowSplitter->addWidget(m_pDiffTextWindowFrame3);
    m_pDiffTextWindow1 = m_pDiffTextWindowFrame1->getDiffTextWindow();
    m_pDiffTextWindow2 = m_pDiffTextWindowFrame2->getDiffTextWindow();
    m_pDiffTextWindow3 = m_pDiffTextWindowFrame3->getDiffTextWindow();

    m_pDiffTextWindowFrame1->setupConnections(this);
    m_pDiffTextWindowFrame2->setupConnections(this);
    m_pDiffTextWindowFrame3->setupConnections(this);

    // Merge window
    m_pMergeWindowFrame = new QWidget(pVSplitter);
    m_pMergeWindowFrame->setObjectName("MergeWindowFrame");
    pVSplitter->addWidget(m_pMergeWindowFrame);
    QHBoxLayout* pMergeHLayout = new QHBoxLayout(m_pMergeWindowFrame);
    pMergeHLayout->setMargin(0);
    pMergeHLayout->setSpacing(0);
    QVBoxLayout* pMergeVLayout = new QVBoxLayout();
    pMergeHLayout->addLayout(pMergeVLayout, 1);

    m_pMergeResultWindowTitle = new WindowTitleWidget(m_pOptionDialog->getOptions());
    pMergeVLayout->addWidget(m_pMergeResultWindowTitle);

    m_pMergeResultWindow = new MergeResultWindow(m_pMergeWindowFrame, m_pOptionDialog->getOptions(), statusBar());
    pMergeVLayout->addWidget(m_pMergeResultWindow, 1);

    MergeResultWindow::mVScrollBar = new QScrollBar(Qt::Vertical, m_pMergeWindowFrame);
    pMergeHLayout->addWidget(MergeResultWindow::mVScrollBar);

    m_pMainSplitter->addWidget(m_pMainWidget);

    autoAdvance->setEnabled(true);

    QList<int> sizes = pVSplitter->sizes();
    int total = sizes[0] + sizes[1];
    if(total < 10)
        total = 100;
    sizes[0] = total / 2;
    sizes[1] = total / 2;
    pVSplitter->setSizes(sizes);

    QList<int> hSizes;
    hSizes << 1 << 1 << 1;
    m_pDiffWindowSplitter->setSizes(hSizes);

    m_pMergeResultWindow->installEventFilter(m_pMergeResultWindowTitle); // for focus tracking

    QHBoxLayout* pHScrollBarLayout = new QHBoxLayout();
    pVLayout->addLayout(pHScrollBarLayout);
    m_pHScrollBar = new ReversibleScrollBar(Qt::Horizontal, &m_pOptions->m_bRightToLeftLanguage);
    pHScrollBarLayout->addWidget(m_pHScrollBar);
    m_pCornerWidget = new QWidget(m_pMainWidget);
    pHScrollBarLayout->addWidget(m_pCornerWidget);

    chk_connect_a(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pOverview, &Overview::setFirstLine);
    chk_connect_a(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pDiffTextWindow1, &DiffTextWindow::setFirstLine);
    chk_connect_a(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, m_pDiffTextWindow1, &DiffTextWindow::setHorizScrollOffset);
    m_pDiffTextWindow1->setupConnections(this);

    chk_connect_a(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pDiffTextWindow2, &DiffTextWindow::setFirstLine);
    chk_connect_a(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, m_pDiffTextWindow2, &DiffTextWindow::setHorizScrollOffset);
    m_pDiffTextWindow2->setupConnections(this);

    chk_connect_a(DiffTextWindow::mVScrollBar, &QScrollBar::valueChanged, m_pDiffTextWindow3, &DiffTextWindow::setFirstLine);
    chk_connect_a(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, m_pDiffTextWindow3, &DiffTextWindow::setHorizScrollOffset);
    m_pDiffTextWindow3->setupConnections(this);

    MergeResultWindow* p = m_pMergeResultWindow;
    chk_connect_a(MergeResultWindow::mVScrollBar, &QScrollBar::valueChanged, p, &MergeResultWindow::setFirstLine);

    chk_connect_a(m_pHScrollBar, &ReversibleScrollBar::valueChanged2, p, &MergeResultWindow::setHorizScrollOffset);
    chk_connect_a(p, &MergeResultWindow::modifiedChanged, m_pMergeResultWindowTitle, &WindowTitleWidget::slotSetModified);
    p->setupConnections(this);
    sourceMask(0, 0);

    chk_connect_a(p, &MergeResultWindow::setFastSelectorRange, m_pDiffTextWindow1, &DiffTextWindow::setFastSelectorRange);
    chk_connect_a(p, &MergeResultWindow::setFastSelectorRange, m_pDiffTextWindow2, &DiffTextWindow::setFastSelectorRange);
    chk_connect_a(p, &MergeResultWindow::setFastSelectorRange, m_pDiffTextWindow3, &DiffTextWindow::setFastSelectorRange);
    chk_connect_a(m_pDiffTextWindow1, &DiffTextWindow::setFastSelectorLine, p, &MergeResultWindow::slotSetFastSelectorLine);
    chk_connect_a(m_pDiffTextWindow2, &DiffTextWindow::setFastSelectorLine, p, &MergeResultWindow::slotSetFastSelectorLine);
    chk_connect_a(m_pDiffTextWindow3, &DiffTextWindow::setFastSelectorLine, p, &MergeResultWindow::slotSetFastSelectorLine);
    chk_connect_a(m_pDiffTextWindow1, &DiffTextWindow::gotFocus, p, &MergeResultWindow::updateSourceMask);
    chk_connect_a(m_pDiffTextWindow2, &DiffTextWindow::gotFocus, p, &MergeResultWindow::updateSourceMask);
    chk_connect_a(m_pDiffTextWindow3, &DiffTextWindow::gotFocus, p, &MergeResultWindow::updateSourceMask);
    chk_connect_a(m_pDirectoryMergeInfo, &DirectoryMergeInfo::gotFocus, p, &MergeResultWindow::updateSourceMask);

    chk_connect_a(m_pDiffTextWindow1, &DiffTextWindow::resizeHeightChangedSignal, this, &KDiff3App::resizeDiffTextWindowHeight);
    // The following two connects cause the wordwrap to be recalced thrice, just to make sure. Better than forgetting one.
    chk_connect_a(m_pDiffTextWindow1, &DiffTextWindow::resizeWidthChangedSignal, this, &KDiff3App::postRecalcWordWrap);
    chk_connect_a(m_pDiffTextWindow2, &DiffTextWindow::resizeWidthChangedSignal, this, &KDiff3App::postRecalcWordWrap);
    chk_connect_a(m_pDiffTextWindow3, &DiffTextWindow::resizeWidthChangedSignal, this, &KDiff3App::postRecalcWordWrap);

    m_pDiffTextWindow1->setFocus();
    m_pMainWidget->setMinimumSize(50, 50);
    m_pCornerWidget->setFixedSize(DiffTextWindow::mVScrollBar->width(), m_pHScrollBar->height());
    showWindowA->setChecked(true);
    showWindowB->setChecked(true);
    showWindowC->setChecked(true);
}

// called after word wrap is complete
void KDiff3App::slotFinishMainInit()
{
    Q_ASSERT(m_pDiffTextWindow1 != nullptr && DiffTextWindow::mVScrollBar != nullptr && m_pOverview != nullptr);

    setHScrollBarRange();

    int newHeight = m_pDiffTextWindow1->getNofVisibleLines();
    /*int newWidth  = m_pDiffTextWindow1->getNofVisibleColumns();*/
    m_DTWHeight = newHeight;

    DiffTextWindow::mVScrollBar->setRange(0, std::max(0, m_neededLines + 1 - newHeight));
    DiffTextWindow::mVScrollBar->setPageStep(newHeight);
    m_pOverview->setRange(DiffTextWindow::mVScrollBar->value(), DiffTextWindow::mVScrollBar->pageStep());

    int d3l = -1;
    if(!m_manualDiffHelpList.empty())
        d3l = m_manualDiffHelpList.front().calcManualDiffFirstDiff3LineIdx(m_diff3LineVector);
    if(d3l >= 0 && m_pDiffTextWindow1)
    {
        int line = m_pDiffTextWindow1->convertDiff3LineIdxToLine(d3l);
        DiffTextWindow::mVScrollBar->setValue(std::max(0, line - 1));
    }
    else
    {
        m_pMergeResultWindow->slotGoTop();
        if(!m_outputFilename.isEmpty() && !m_pMergeResultWindow->isUnsolvedConflictAtCurrent())
            m_pMergeResultWindow->slotGoNextUnsolvedConflict();
    }

    if(m_pCornerWidget)
        m_pCornerWidget->setFixedSize(DiffTextWindow::mVScrollBar->width(), m_pHScrollBar->height());

    slotUpdateAvailabilities();
    setUpdatesEnabled(true);

    bool bVisibleMergeResultWindow = !m_outputFilename.isEmpty();
    TotalDiffStatus* pTotalDiffStatus = &m_totalDiffStatus;

    if(m_bLoadFiles)
    {
        if(bVisibleMergeResultWindow)
            m_pMergeResultWindow->showNrOfConflicts();
        else if(
            // Avoid showing this message during startup without parameters.
            !(m_sd1->getAliasName().isEmpty() && m_sd2->getAliasName().isEmpty() && m_sd3->getAliasName().isEmpty()) &&
            (m_sd1->isValid() && m_sd2->isValid() && m_sd3->isValid()))
        {
            QString totalInfo;
            if(pTotalDiffStatus->isBinaryEqualAB() && pTotalDiffStatus->isBinaryEqualAC())
                totalInfo += i18n("All input files are binary equal.");
            else if(pTotalDiffStatus->isTextEqualAB() && pTotalDiffStatus->isTextEqualAC())
                totalInfo += i18n("All input files contain the same text, but are not binary equal.");
            else
            {
                if(pTotalDiffStatus->isBinaryEqualAB())
                    totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("A"), i18n("B"));
                else if(pTotalDiffStatus->isTextEqualAB())
                    totalInfo += i18n("Files %1 and %2 have equal text, but are not binary equal. \n", i18n("A"), i18n("B"));
                if(pTotalDiffStatus->isBinaryEqualAC())
                    totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("A"), i18n("C"));
                else if(pTotalDiffStatus->isTextEqualAC())
                    totalInfo += i18n("Files %1 and %2 have equal text, but are not binary equal. \n", i18n("A"), i18n("C"));
                if(pTotalDiffStatus->isBinaryEqualBC())
                    totalInfo += i18n("Files %1 and %2 are binary equal.\n", i18n("B"), i18n("C"));
                else if(pTotalDiffStatus->isTextEqualBC())
                    totalInfo += i18n("Files %1 and %2 have equal text, but are not binary equal. \n", i18n("B"), i18n("C"));
            }

            if(!totalInfo.isEmpty())
                KMessageBox::information(this, totalInfo);
        }

        if(bVisibleMergeResultWindow && (!m_sd1->isText() || !m_sd2->isText() || !m_sd3->isText()))
        {
            KMessageBox::information(this, i18n(
                                               "Some input files do not seem to be pure text files.\n"
                                               "Note that the KDiff3 merge was not meant for binary data.\n"
                                               "Continue at your own risk."));
        }
        if(m_sd1->isIncompleteConversion() || m_sd2->isIncompleteConversion() || m_sd3->isIncompleteConversion())
        {
            QString files;
            if(m_sd1->isIncompleteConversion())
                files += i18n("A");
            if(m_sd2->isIncompleteConversion())
                files += files.isEmpty() ? i18n("B") : i18n(", B");
            if(m_sd3->isIncompleteConversion())
                files += files.isEmpty() ? i18n("C") : i18n(", C");

            KMessageBox::information(this, i18n("Some input characters could not be converted to valid unicode.\n"
                                                "You might be using the wrong codec. (e.g. UTF-8 for non UTF-8 files).\n"
                                                "Do not save the result if unsure. Continue at your own risk.\n"
                                                "Affected input files are in %1.", files));
        }
    }

    if(bVisibleMergeResultWindow && m_pMergeResultWindow)
    {
        m_pMergeResultWindow->setFocus();
    }
    else if(m_pDiffTextWindow1)
    {
        m_pDiffTextWindow1->setFocus();
    }
}

void KDiff3App::resizeEvent(QResizeEvent* e)
{
    QSplitter::resizeEvent(e);
    if(m_pCornerWidget)
        m_pCornerWidget->setFixedSize(DiffTextWindow::mVScrollBar->width(), m_pHScrollBar->height());
}

void KDiff3App::wheelEvent(QWheelEvent* pWheelEvent)
{
    pWheelEvent->accept();
    QPoint delta = pWheelEvent->angleDelta();

    //Block diagonal scrolling easily generated unintentionally with track pads.
    if(delta.x() != 0 && abs(delta.y()) < abs(delta.x()) && m_pHScrollBar != nullptr)
        QCoreApplication::postEvent(m_pHScrollBar, new QWheelEvent(*pWheelEvent));
}

void KDiff3App::keyPressEvent(QKeyEvent* keyEvent)
{
    if(keyEvent->key() == Qt::Key_Escape && m_pKDiff3Shell && m_pOptions->m_bEscapeKeyQuits)
    {
        m_pKDiff3Shell->close();
        return;
    }

    bool bCtrl = (keyEvent->QInputEvent::modifiers() & Qt::ControlModifier) != 0;

    switch(keyEvent->key())
    {
        case Qt::Key_Down:
        case Qt::Key_Up:
        case Qt::Key_PageDown:
        case Qt::Key_PageUp:
            if(DiffTextWindow::mVScrollBar != nullptr)
                QCoreApplication::postEvent(DiffTextWindow::mVScrollBar, new QKeyEvent(*keyEvent));
            return;
        case Qt::Key_Left:
        case Qt::Key_Right:
            if(m_pHScrollBar != nullptr)
                QCoreApplication::postEvent(m_pHScrollBar, new QKeyEvent(*keyEvent));

            break;
        case Qt::Key_End:
        case Qt::Key_Home:
            if(bCtrl)
            {
                if(DiffTextWindow::mVScrollBar != nullptr)
                    QCoreApplication::postEvent(DiffTextWindow::mVScrollBar, new QKeyEvent(*keyEvent));
            }
            else
            {
                if(m_pHScrollBar != nullptr)
                    QCoreApplication::postEvent(m_pHScrollBar, new QKeyEvent(*keyEvent));
            }
            break;
        default:
            break;
    }
}

void KDiff3App::slotFinishDrop()
{
    raise();
    mainInit();
}

void KDiff3App::slotFileOpen()
{
    if(!shouldContinue()) return;
    //create dummy DirectoryInfo record for first run so we don't crash.
    if(m_dirinfo == nullptr)
        m_dirinfo = QSharedPointer<DirectoryInfo>::create();

    if(m_pDirectoryMergeWindow->isDirectoryMergeInProgress())
    {
        int result = KMessageBox::warningYesNo(this,
                                               i18n("You are currently doing a folder merge. Are you sure, you want to abort?"),
                                               i18n("Warning"),
                                               KGuiItem(i18n("Abort")),
                                               KGuiItem(i18n("Continue Merging")));
        if(result != KMessageBox::Yes)
            return;
    }

    slotStatusMsg(i18n("Opening files..."));

    for(;;)
    {
         QPointer<OpenDialog> d = QPointer<OpenDialog>(new OpenDialog(this,
                     QDir::toNativeSeparators(m_bDirCompare ? m_dirinfo->dirA().prettyAbsPath() : m_sd1->isFromBuffer() ? QString("") : m_sd1->getAliasName()),
                     QDir::toNativeSeparators(m_bDirCompare ? m_dirinfo->dirB().prettyAbsPath() : m_sd2->isFromBuffer() ? QString("") : m_sd2->getAliasName()),
                     QDir::toNativeSeparators(m_bDirCompare ? m_dirinfo->dirC().prettyAbsPath() : m_sd3->isFromBuffer() ? QString("") : m_sd3->getAliasName()),
                     m_bDirCompare ? !m_dirinfo->destDir().prettyAbsPath().isEmpty() : !m_outputFilename.isEmpty(),
                     QDir::toNativeSeparators(m_bDefaultFilename ? QString("") : m_outputFilename), m_pOptionDialog->getOptions()));

        int status = d->exec();
        if(status == QDialog::Accepted)
        {
            m_sd1->setFilename(d->m_pLineA->currentText());
            m_sd2->setFilename(d->m_pLineB->currentText());
            m_sd3->setFilename(d->m_pLineC->currentText());

            if(d->m_pMerge->isChecked())
            {
                if(d->m_pLineOut->currentText().isEmpty())
                {
                    m_outputFilename = "unnamed.txt";
                    m_bDefaultFilename = true;
                }
                else
                {
                    m_outputFilename = d->m_pLineOut->currentText();
                    m_bDefaultFilename = false;
                }
            }
            else
                m_outputFilename = "";

            m_bDirCompare = m_sd1->isDir();
            bool bSuccess = improveFilenames(false);

            if(!bSuccess)
                continue;

            if(m_bDirCompare)
            {
                m_pDirectoryMergeSplitter->show();
                if(m_pMainWidget != nullptr)
                {
                    m_pMainWidget->hide();
                }
                break;
            }
            else
            {
                m_pDirectoryMergeSplitter->hide();
                mainInit();

                if((!m_sd1->getErrors().isEmpty()) ||
                   (!m_sd2->getErrors().isEmpty()) ||
                   (!m_sd3->getErrors().isEmpty()))
                {
                    QString text(i18n("Opening of these files failed:"));
                    text += "\n\n";
                    if(!m_sd1->getErrors().isEmpty())
                        text += " - " + m_sd1->getAliasName() + '\n' + m_sd1->getErrors().join('\n') + '\n';
                    if(!m_sd2->getErrors().isEmpty())
                        text += " - " + m_sd2->getAliasName() + '\n' + m_sd2->getErrors().join('\n') + '\n';
                    if(!m_sd3->getErrors().isEmpty())
                        text += " - " + m_sd3->getAliasName() + '\n' + m_sd3->getErrors().join('\n') + '\n';

                    KMessageBox::sorry(this, text, i18n("File open error"));

                    continue;
                }
            }
        }
        break;
    }

    slotUpdateAvailabilities();
    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFileOpen2(const QString& fn1, const QString& fn2, const QString& fn3, const QString& ofn,
                              const QString& an1, const QString& an2, const QString& an3, TotalDiffStatus* pTotalDiffStatus)
{
    if(!shouldContinue()) return;

    if(fn1.isEmpty() && fn2.isEmpty() && fn3.isEmpty() && ofn.isEmpty() && m_pMainWidget != nullptr)
    {
        m_pMainWidget->hide();
        return;
    }

    slotStatusMsg(i18n("Opening files..."));

    m_sd1->setFilename(fn1);
    m_sd2->setFilename(fn2);
    m_sd3->setFilename(fn3);

    m_sd1->setAliasName(an1);
    m_sd2->setAliasName(an2);
    m_sd3->setAliasName(an3);

    if(!ofn.isEmpty())
    {
        m_outputFilename = ofn;
        m_bDefaultFilename = false;
    }
    else
    {
        m_outputFilename = "";
        m_bDefaultFilename = true;
    }

    improveFilenames(true); // Create new window for KDiff3 for directory comparison.

    if(!m_sd1->isDir())
    {
        mainInit(pTotalDiffStatus);

        if(pTotalDiffStatus != nullptr)
            return;

        if(!((!m_sd1->isEmpty() && !m_sd1->hasData()) ||
           (!m_sd2->isEmpty() && !m_sd2->hasData()) ||
           (!m_sd3->isEmpty() && !m_sd3->hasData())))
        {
            if(m_pDirectoryMergeWindow != nullptr && m_pDirectoryMergeWindow->isVisible() && !dirShowBoth->isChecked())
            {
                slotDirViewToggle();
            }
        }
    }
    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotFileNameChanged(const QString& fileName, e_SrcSelector winIdx)
{
    QString fn1 = m_sd1->getFilename();
    QString an1 = m_sd1->getAliasName();
    QString fn2 = m_sd2->getFilename();
    QString an2 = m_sd2->getAliasName();
    QString fn3 = m_sd3->getFilename();
    QString an3 = m_sd3->getAliasName();
    if(winIdx == e_SrcSelector::A)
    {
        fn1 = fileName;
        an1 = "";
    }
    if(winIdx == e_SrcSelector::B)
    {
        fn2 = fileName;
        an2 = "";
    }
    if(winIdx == e_SrcSelector::C)
    {
        fn3 = fileName;
        an3 = "";
    }

    slotFileOpen2(fn1, fn2, fn3, m_outputFilename, an1, an2, an3, nullptr);
}

void KDiff3App::slotEditCut()
{
    slotStatusMsg(i18n("Cutting selection..."));
    Q_EMIT cut();
    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditCopy()
{
    slotStatusMsg(i18n("Copying selection to clipboard..."));
    QString s;
    if(m_pDiffTextWindow1 != nullptr) s = m_pDiffTextWindow1->getSelection();
    if(s.isEmpty() && m_pDiffTextWindow2 != nullptr) s = m_pDiffTextWindow2->getSelection();
    if(s.isEmpty() && m_pDiffTextWindow3 != nullptr) s = m_pDiffTextWindow3->getSelection();
    if(s.isEmpty() && m_pMergeResultWindow != nullptr) s = m_pMergeResultWindow->getSelection();
    if(!s.isEmpty())
    {
        QApplication::clipboard()->setText(s, QClipboard::Clipboard);
    }

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditPaste()
{
    slotStatusMsg(i18n("Inserting clipboard contents..."));

    if(m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible())
    {
        m_pMergeResultWindow->pasteClipboard(false);
    }
    else
    {
        if(shouldContinue())
        {
            QString error;
            bool do_init = false;

            if(m_pDiffTextWindow1->hasFocus())
            {
                error = m_sd1->setData(QApplication::clipboard()->text(QClipboard::Clipboard));
                do_init = true;
            }
            else if(m_pDiffTextWindow2->hasFocus())
            {
                error = m_sd2->setData(QApplication::clipboard()->text(QClipboard::Clipboard));
                do_init = true;
            }
            else if(m_pDiffTextWindow3->hasFocus())
            {
                error = m_sd3->setData(QApplication::clipboard()->text(QClipboard::Clipboard));
                do_init = true;
            }

            if(!error.isEmpty())
            {
                KMessageBox::error(m_pOptionDialog, error);
            }

            if(do_init)
            {
                mainInit();
            }
        }
    }

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotEditSelectAll()
{

    Q_EMIT selectAll();

    slotStatusMsg(i18n("Ready."));
}

void KDiff3App::slotGoCurrent()
{
    Q_EMIT goCurrent();
}
void KDiff3App::slotGoTop()
{
    Q_EMIT goTop();
}
void KDiff3App::slotGoBottom()
{
    Q_EMIT goBottom();
}
void KDiff3App::slotGoPrevUnsolvedConflict()
{
    Q_EMIT goPrevUnsolvedConflict();
}
void KDiff3App::slotGoNextUnsolvedConflict()
{
    m_bTimerBlock = false;
    Q_EMIT goNextUnsolvedConflict();
}
void KDiff3App::slotGoPrevConflict()
{
    Q_EMIT goPrevConflict();
}
void KDiff3App::slotGoNextConflict()
{
    m_bTimerBlock = false;
    Q_EMIT goNextConflict();
}
void KDiff3App::slotGoPrevDelta()
{
    Q_EMIT goPrevDelta();
}
void KDiff3App::slotGoNextDelta()
{
    Q_EMIT goNextDelta();
}

void KDiff3App::choose(e_SrcSelector choice)
{
    if(!m_bTimerBlock)
    {
        if(m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->hasFocus())
        {
            if(choice == e_SrcSelector::A) m_pDirectoryMergeWindow->slotCurrentChooseA();
            if(choice == e_SrcSelector::B) m_pDirectoryMergeWindow->slotCurrentChooseB();
            if(choice == e_SrcSelector::C) m_pDirectoryMergeWindow->slotCurrentChooseC();

            chooseA->setChecked(false);
            chooseB->setChecked(false);
            chooseC->setChecked(false);
        }
        else if(m_pMergeResultWindow)
        {
            m_pMergeResultWindow->choose(choice);
            if(autoAdvance->isChecked())
            {
                m_bTimerBlock = true;
                QTimer::singleShot(m_pOptions->m_autoAdvanceDelay, this, &KDiff3App::slotGoNextUnsolvedConflict);
            }
        }
    }
}

void KDiff3App::slotChooseA()
{
    choose(e_SrcSelector::A);
}
void KDiff3App::slotChooseB()
{
    choose(e_SrcSelector::B);
}
void KDiff3App::slotChooseC()
{
    choose(e_SrcSelector::C);
}

void KDiff3App::slotAutoSolve()
{
    Q_EMIT autoSolve();

    slotUpdateAvailabilities();
}

void KDiff3App::slotUnsolve()
{
    Q_EMIT unsolve();
}

void KDiff3App::slotMergeHistory()
{
    Q_EMIT mergeHistory();
}

void KDiff3App::slotRegExpAutoMerge()
{
    Q_EMIT regExpAutoMerge();
}

void KDiff3App::slotSplitDiff()
{
    LineRef firstLine;
    LineRef lastLine;
    DiffTextWindow* pDTW = nullptr;
    if(m_pDiffTextWindow1)
    {
        pDTW = m_pDiffTextWindow1;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow2)
    {
        pDTW = m_pDiffTextWindow2;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow3)
    {
        pDTW = m_pDiffTextWindow3;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(pDTW && firstLine.isValid() && m_pMergeResultWindow)
    {
        pDTW->resetSelection();

        m_pMergeResultWindow->slotSplitDiff(firstLine, lastLine);
    }
}

void KDiff3App::slotJoinDiffs()
{
    LineRef firstLine;
    LineRef lastLine;
    DiffTextWindow* pDTW = nullptr;
    if(m_pDiffTextWindow1)
    {
        pDTW = m_pDiffTextWindow1;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow2)
    {
        pDTW = m_pDiffTextWindow2;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(!firstLine.isValid() && m_pDiffTextWindow3)
    {
        pDTW = m_pDiffTextWindow3;
        pDTW->getSelectionRange(&firstLine, &lastLine, eD3LLineCoords);
    }
    if(pDTW && firstLine.isValid() && m_pMergeResultWindow)
    {
        pDTW->resetSelection();

        m_pMergeResultWindow->slotJoinDiffs(firstLine, lastLine);
    }
}

void KDiff3App::slotConfigure()
{
    m_pOptionDialog->setState();
    m_pOptionDialog->setMinimumHeight(m_pOptionDialog->minimumHeight() + 40);
    m_pOptionDialog->exec();
    slotRefresh();
}

void KDiff3App::slotConfigureKeys()
{
    KShortcutsDialog::configure(actionCollection(), KShortcutsEditor::LetterShortcutsAllowed, this);
}

void KDiff3App::slotRefresh()
{
    QApplication::setFont(m_pOptions->m_appFont);

    Q_EMIT doRefresh();

    if(m_pHScrollBar != nullptr)
    {
        m_pHScrollBar->setAgain();
    }
    if(m_pDiffWindowSplitter != nullptr)
    {
        m_pDiffWindowSplitter->setOrientation(m_pOptions->m_bHorizDiffWindowSplitting ? Qt::Horizontal : Qt::Vertical);
    }
}

void KDiff3App::slotSelectionStart()
{
    //editCopy->setEnabled( false );
    //editCut->setEnabled( false );

    const QObject* s = sender();
    if(m_pDiffTextWindow1 && s != m_pDiffTextWindow1) m_pDiffTextWindow1->resetSelection();
    if(m_pDiffTextWindow2 && s != m_pDiffTextWindow2) m_pDiffTextWindow2->resetSelection();
    if(m_pDiffTextWindow3 && s != m_pDiffTextWindow3) m_pDiffTextWindow3->resetSelection();
    if(m_pMergeResultWindow && s != m_pMergeResultWindow) m_pMergeResultWindow->resetSelection();
}

void KDiff3App::slotSelectionEnd()
{
    //const QObject* s = sender();
    //editCopy->setEnabled(true);
    //editCut->setEnabled( s==m_pMergeResultWindow );
    if(m_pOptions->m_bAutoCopySelection)
    {
        slotEditCopy();
    }
    else
    {
        QClipboard* clipBoard = QApplication::clipboard();

        if(clipBoard->supportsSelection())
        {
            QString s;
            if(m_pDiffTextWindow1 != nullptr) s = m_pDiffTextWindow1->getSelection();
            if(s.isEmpty() && m_pDiffTextWindow2 != nullptr) s = m_pDiffTextWindow2->getSelection();
            if(s.isEmpty() && m_pDiffTextWindow3 != nullptr) s = m_pDiffTextWindow3->getSelection();
            if(s.isEmpty() && m_pMergeResultWindow != nullptr) s = m_pMergeResultWindow->getSelection();
            if(!s.isEmpty())
            {
                clipBoard->setText(s, QClipboard::Selection);
            }
        }
    }
}

void KDiff3App::slotClipboardChanged()
{
    const QClipboard* clipboard = QApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if(mimeData && mimeData->hasText())
    {
        QString s = clipboard->text();
        editPaste->setEnabled(!s.isEmpty());
    }
    else
    {
        editPaste->setEnabled(false);
    }
}

void KDiff3App::slotOutputModified(bool bModified)
{
    if(bModified && !m_bOutputModified)
    {
        m_bOutputModified = true;
        slotUpdateAvailabilities();
    }
}

void KDiff3App::slotAutoAdvanceToggled()
{
    m_pOptions->m_bAutoAdvance = autoAdvance->isChecked();
}

void KDiff3App::slotWordWrapToggled()
{
    m_pOptions->setWordWrap(wordWrap->isChecked());
    postRecalcWordWrap();
}

// Enable or disable all widgets except the status bar widget.
void KDiff3App::mainWindowEnable(bool bEnable)
{
    if(QMainWindow* pWindow = dynamic_cast<QMainWindow*>(window()))
    {
        QWidget* pStatusBarWidget = pWindow->statusBar();
        pWindow->setEnabled(bEnable);
        pStatusBarWidget->setEnabled(true);
    }
}

void KDiff3App::postRecalcWordWrap()
{
    if(!m_bRecalcWordWrapPosted)
    {
        m_bRecalcWordWrapPosted = true;
        m_firstD3LIdx = -1;
        Q_EMIT sigRecalcWordWrap();
    }
    else
    {
        g_pProgressDialog->cancel(ProgressDialog::eResize);
    }
}

void KDiff3App::slotRecalcWordWrap()
{
    recalcWordWrap();
}

// visibleTextWidthForPrinting is >=0 only for printing, otherwise the really visible width is used
void KDiff3App::recalcWordWrap(int visibleTextWidthForPrinting)
{
    m_bRecalcWordWrapPosted = true;
    mainWindowEnable(false);

    if(m_firstD3LIdx < 0)
    {
        m_firstD3LIdx = 0;
        if(m_pDiffTextWindow1)
            m_firstD3LIdx = m_pDiffTextWindow1->convertLineToDiff3LineIdx(m_pDiffTextWindow1->getFirstLine());
    }

    // Convert selection to D3L-coords (converting back happens in DiffTextWindow::recalcWordWrap()
    if(m_pDiffTextWindow1)
        m_pDiffTextWindow1->convertSelectionToD3LCoords();
    if(m_pDiffTextWindow2)
        m_pDiffTextWindow2->convertSelectionToD3LCoords();
    if(m_pDiffTextWindow3)
        m_pDiffTextWindow3->convertSelectionToD3LCoords();

    g_pProgressDialog->clearCancelState(); // clear cancelled state if previously set

    if(!m_diff3LineList.empty())
    {
        if(m_pOptions->wordWrapOn())
        {
            m_diff3LineList.recalcWordWrap(true);

            // Let every window calc how many lines will be needed.
            if(m_pDiffTextWindow1)
            {
                m_pDiffTextWindow1->recalcWordWrap(true, 0, visibleTextWidthForPrinting);
            }
            if(m_pDiffTextWindow2)
            {
                m_pDiffTextWindow2->recalcWordWrap(true, 0, visibleTextWidthForPrinting);
            }
            if(m_pDiffTextWindow3)
            {
                m_pDiffTextWindow3->recalcWordWrap(true, 0, visibleTextWidthForPrinting);
            }
        }
        else
        {
            m_neededLines = m_diff3LineVector.size();
            if(m_pDiffTextWindow1)
                m_pDiffTextWindow1->recalcWordWrap(false, 0, 0);
            if(m_pDiffTextWindow2)
                m_pDiffTextWindow2->recalcWordWrap(false, 0, 0);
            if(m_pDiffTextWindow3)
                m_pDiffTextWindow3->recalcWordWrap(false, 0, 0);
        }
        bool bRunnablesStarted = DiffTextWindow::startRunnables();
        if(!bRunnablesStarted)
            slotFinishRecalcWordWrap(visibleTextWidthForPrinting);
        else
        {
            g_pProgressDialog->setInformation(m_pOptions->wordWrapOn()
                                                  ? i18n("Word wrap (Cancel disables word wrap)")
                                                  : i18n("Calculating max width for horizontal scrollbar"),
                                              false);
        }
    }
    else
    {
        //don't leave proccessing incomplete if m_diff3LineList isEmpty as when an error occures during reading.
        slotFinishRecalcWordWrap(visibleTextWidthForPrinting);
    }
}

void KDiff3App::slotFinishRecalcWordWrap(int visibleTextWidthForPrinting)
{
    g_pProgressDialog->pop();

    if(m_pOptions->wordWrapOn() && g_pProgressDialog->wasCancelled())
    {
        if(g_pProgressDialog->cancelReason() == ProgressDialog::eUserAbort)
        {
            wordWrap->setChecked(false);
            m_pOptions->setWordWrap(wordWrap->isChecked());
        }

        Q_EMIT sigRecalcWordWrap();
        return;
    }
    else
    {
        m_bRecalcWordWrapPosted = false;
    }

    g_pProgressDialog->setStayHidden(false);

    bool bPrinting = visibleTextWidthForPrinting >= 0;

    if(!m_diff3LineList.empty())
    {
        if(m_pOptions->wordWrapOn())
        {
            LineCount sumOfLines = m_diff3LineList.recalcWordWrap(false);

            // Finish the word wrap
            if(m_pDiffTextWindow1)
                m_pDiffTextWindow1->recalcWordWrap(true, sumOfLines, visibleTextWidthForPrinting);
            if(m_pDiffTextWindow2)
                m_pDiffTextWindow2->recalcWordWrap(true, sumOfLines, visibleTextWidthForPrinting);
            if(m_pDiffTextWindow3)
                m_pDiffTextWindow3->recalcWordWrap(true, sumOfLines, visibleTextWidthForPrinting);

            m_neededLines = sumOfLines;
        }
        else
        {
            if(m_pDiffTextWindow1)
                m_pDiffTextWindow1->recalcWordWrap(false, 1, 0);
            if(m_pDiffTextWindow2)
                m_pDiffTextWindow2->recalcWordWrap(false, 1, 0);
            if(m_pDiffTextWindow3)
                m_pDiffTextWindow3->recalcWordWrap(false, 1, 0);
        }
        slotStatusMsg(QString());
    }

    if(!bPrinting)
    {
        if(m_pOverview)
            m_pOverview->slotRedraw();
        if(DiffTextWindow::mVScrollBar)
            DiffTextWindow::mVScrollBar->setRange(0, std::max(0, m_neededLines + 1 - m_DTWHeight));
        if(m_pDiffTextWindow1)
        {
            if(DiffTextWindow::mVScrollBar)
                DiffTextWindow::mVScrollBar->setValue(m_pDiffTextWindow1->convertDiff3LineIdxToLine(m_firstD3LIdx));

            setHScrollBarRange();
            m_pHScrollBar->setValue(0);
        }
    }
    mainWindowEnable(true);

    if(m_bFinishMainInit)
    {
        m_bFinishMainInit = false;
        slotFinishMainInit();
    }
    if(m_pEventLoopForPrinting)
        m_pEventLoopForPrinting->quit();
}

void KDiff3App::slotShowWhiteSpaceToggled()
{
    m_pOptions->m_bShowWhiteSpaceCharacters = showWhiteSpaceCharacters->isChecked();
    m_pOptions->m_bShowWhiteSpace = showWhiteSpace->isChecked();

    Q_EMIT showWhiteSpaceToggled();
}

void KDiff3App::slotShowLineNumbersToggled()
{
    m_pOptions->m_bShowLineNumbers = showLineNumbers->isChecked();

    if(wordWrap->isChecked())
        recalcWordWrap();

    Q_EMIT showLineNumbersToggled();
}

/// Return true for success, else false
bool KDiff3App::improveFilenames(bool bCreateNewInstance)
{
    FileAccess f1(m_sd1->getFilename());
    FileAccess f2(m_sd2->getFilename());
    FileAccess f3(m_sd3->getFilename());
    FileAccess f4(m_outputFilename);

    if(f1.isFile() && f1.exists())
    {
        if(f2.isDir())
        {
            f2.addPath(f1.fileName());
            if(f2.isFile() && f2.exists())
                m_sd2->setFileAccess(f2);
        }
        if(f3.isDir())
        {
            f3.addPath(f1.fileName());
            if(f3.isFile() && f3.exists())
                m_sd3->setFileAccess(f3);
        }
        if(f4.isDir())
        {
            f4.addPath(f1.fileName());
            if(f4.isFile() && f4.exists())
                m_outputFilename = f4.absoluteFilePath();
        }
    }
    else if(f1.isDir())
    {
        if(bCreateNewInstance)
        {
            Q_EMIT createNewInstance(f1.absoluteFilePath(), f2.absoluteFilePath(), f3.absoluteFilePath());
        }
        else
        {
            bool bDirCompare = m_bDirCompare;
            FileAccess destDir;

            if(!m_bDefaultFilename) destDir = f4;
            m_pDirectoryMergeSplitter->show();
            if(m_pMainWidget != nullptr) m_pMainWidget->hide();
            setUpdatesEnabled(true);

            m_dirinfo = QSharedPointer<DirectoryInfo>::create(f1, f2, f3, destDir);
            bool bSuccess = m_pDirectoryMergeWindow->init(
                m_dirinfo,
                !m_outputFilename.isEmpty());
            //This is a bug if it still happens.
            Q_ASSERT(m_bDirCompare == bDirCompare);

            if(bSuccess)
            {
                m_sd1->reset();
                if(m_pDiffTextWindow1 != nullptr)
                {
                    m_pDiffTextWindow1->init(QString(""), nullptr, eLineEndStyleDos, nullptr, 0, nullptr, nullptr);
                    m_pDiffTextWindowFrame1->init();
                }
                m_sd2->reset();
                if(m_pDiffTextWindow2 != nullptr)
                {
                    m_pDiffTextWindow2->init(QString(""), nullptr, eLineEndStyleDos, nullptr, 0, nullptr, nullptr);
                    m_pDiffTextWindowFrame2->init();
                }
                m_sd3->reset();
                if(m_pDiffTextWindow3 != nullptr)
                {
                    m_pDiffTextWindow3->init(QString(""), nullptr, eLineEndStyleDos, nullptr, 0, nullptr, nullptr);
                    m_pDiffTextWindowFrame3->init();
                }
            }
            slotUpdateAvailabilities();
            return bSuccess;
        }
    }
    return true;
}

void KDiff3App::slotReload()
{
    if(!shouldContinue()) return;

    mainInit();
}

bool KDiff3App::canContinue()
{
    // First test if anything must be saved.
    if(m_bOutputModified)
    {
        int result = KMessageBox::warningYesNoCancel(this,
                                                     i18n("The merge result has not been saved."),
                                                     i18n("Warning"),
                                                     KGuiItem(i18n("Save && Continue")),
                                                     KGuiItem(i18n("Continue Without Saving")));
        if(result == KMessageBox::Cancel)
            return false;
        else if(result == KMessageBox::Yes)
        {
            slotFileSave();
            if(m_bOutputModified)
            {
                KMessageBox::sorry(this, i18n("Saving the merge result failed."), i18n("Warning"));
                return false;
            }
        }
    }

    m_bOutputModified = false;
    return true;
}

void KDiff3App::slotDirShowBoth()
{
    if(dirShowBoth->isChecked())
    {
        if(m_pDirectoryMergeSplitter)
            m_pDirectoryMergeSplitter->setVisible(m_bDirCompare);

        if(m_pMainWidget != nullptr)
            m_pMainWidget->show();
    }
    else
    {
        bool bTextDataAvailable = (m_sd1->hasData() || m_sd2->hasData() || m_sd3->hasData());
        if(m_pMainWidget != nullptr && bTextDataAvailable)
        {
            m_pMainWidget->show();
            m_pDirectoryMergeSplitter->hide();
        }
        else if(m_bDirCompare)
        {
            m_pDirectoryMergeSplitter->show();
        }
    }

    slotUpdateAvailabilities();
}

void KDiff3App::slotDirViewToggle()
{
    if(m_bDirCompare)
    {
        if(!m_pDirectoryMergeSplitter->isVisible())
        {
            m_pDirectoryMergeSplitter->show();
            if(m_pMainWidget != nullptr)
                m_pMainWidget->hide();
        }
        else
        {
            if(m_pMainWidget != nullptr)
            {
                m_pDirectoryMergeSplitter->hide();
                m_pMainWidget->show();
            }
        }
    }
    slotUpdateAvailabilities();
}

void KDiff3App::slotShowWindowAToggled()
{
    if(m_pDiffTextWindow1 != nullptr)
    {
        m_pDiffTextWindowFrame1->setVisible(showWindowA->isChecked());
        slotUpdateAvailabilities();
    }
}

void KDiff3App::slotShowWindowBToggled()
{
    if(m_pDiffTextWindow2 != nullptr)
    {
        m_pDiffTextWindowFrame2->setVisible(showWindowB->isChecked());
        slotUpdateAvailabilities();
    }
}

void KDiff3App::slotShowWindowCToggled()
{
    if(m_pDiffTextWindow3 != nullptr)
    {
        m_pDiffTextWindowFrame3->setVisible(showWindowC->isChecked());
        slotUpdateAvailabilities();
    }
}

void KDiff3App::slotEditFind()
{
    m_pFindDialog->currentLine = 0;
    m_pFindDialog->currentPos = 0;
    m_pFindDialog->currentWindow = 1;

    // Use currently selected text:
    QString s;
    if(m_pDiffTextWindow1 != nullptr) s = m_pDiffTextWindow1->getSelection();
    if(s.isEmpty() && m_pDiffTextWindow2 != nullptr) s = m_pDiffTextWindow2->getSelection();
    if(s.isEmpty() && m_pDiffTextWindow3 != nullptr) s = m_pDiffTextWindow3->getSelection();
    if(s.isEmpty() && m_pMergeResultWindow != nullptr) s = m_pMergeResultWindow->getSelection();
    if(!s.isEmpty() && !s.contains('\n'))
    {
        m_pFindDialog->m_pSearchString->setText(s);
    }

    if(QDialog::Accepted == m_pFindDialog->exec())
    {
        slotEditFindNext();
    }
}

void KDiff3App::slotEditFindNext()
{
    QString s = m_pFindDialog->m_pSearchString->text();
    if(s.isEmpty())
    {
        slotEditFind();
        return;
    }

    bool bDirDown = true;
    bool bCaseSensitive = m_pFindDialog->m_pCaseSensitive->isChecked();

    LineRef d3vLine = m_pFindDialog->currentLine;
    int posInLine = m_pFindDialog->currentPos;
    LineRef l = 0;
    int p = 0;
    if(m_pFindDialog->currentWindow == 1)
    {
        if(m_pFindDialog->m_pSearchInA->isChecked() && m_pDiffTextWindow1 != nullptr &&
           m_pDiffTextWindow1->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pDiffTextWindow1->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length(), l, p);
            DiffTextWindow::mVScrollBar->setValue(l - DiffTextWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, p + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }
        m_pFindDialog->currentWindow = 2;
        m_pFindDialog->currentLine = 0;
        m_pFindDialog->currentPos = 0;
    }

    d3vLine = m_pFindDialog->currentLine;
    posInLine = m_pFindDialog->currentPos;
    if(m_pFindDialog->currentWindow == 2)
    {
        if(m_pFindDialog->m_pSearchInB->isChecked() && m_pDiffTextWindow2 != nullptr &&
           m_pDiffTextWindow2->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pDiffTextWindow2->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length(), l, p);
            DiffTextWindow::mVScrollBar->setValue(l - DiffTextWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, p + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }
        m_pFindDialog->currentWindow = 3;
        m_pFindDialog->currentLine = 0;
        m_pFindDialog->currentPos = 0;
    }

    d3vLine = m_pFindDialog->currentLine;
    posInLine = m_pFindDialog->currentPos;
    if(m_pFindDialog->currentWindow == 3)
    {
        if(m_pFindDialog->m_pSearchInC->isChecked() && m_pDiffTextWindow3 != nullptr &&
           m_pDiffTextWindow3->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pDiffTextWindow3->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length(), l, p);
            DiffTextWindow::mVScrollBar->setValue(l - DiffTextWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, p + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }
        m_pFindDialog->currentWindow = 4;
        m_pFindDialog->currentLine = 0;
        m_pFindDialog->currentPos = 0;
    }

    d3vLine = m_pFindDialog->currentLine;
    posInLine = m_pFindDialog->currentPos;
    if(m_pFindDialog->currentWindow == 4)
    {
        if(m_pFindDialog->m_pSearchInOutput->isChecked() && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isVisible() &&
           m_pMergeResultWindow->findString(s, d3vLine, posInLine, bDirDown, bCaseSensitive))
        {
            m_pMergeResultWindow->setSelection(d3vLine, posInLine, d3vLine, posInLine + s.length());
            MergeResultWindow::mVScrollBar->setValue(d3vLine - MergeResultWindow::mVScrollBar->pageStep() / 2);
            m_pHScrollBar->setValue(std::max(0, posInLine + s.length() - m_pHScrollBar->pageStep()));
            m_pFindDialog->currentLine = d3vLine;
            m_pFindDialog->currentPos = posInLine + 1;
            return;
        }
        m_pFindDialog->currentWindow = 5;
        m_pFindDialog->currentLine = 0;
        m_pFindDialog->currentPos = 0;
    }

    KMessageBox::information(this, i18n("Search complete."), i18n("Search Complete"));
    m_pFindDialog->currentWindow = 1;
    m_pFindDialog->currentLine = 0;
    m_pFindDialog->currentPos = 0;
}

void KDiff3App::slotMergeCurrentFile()
{
    if(m_bDirCompare && m_pDirectoryMergeWindow->isVisible() && m_pDirectoryMergeWindow->isFileSelected())
    {
        m_pDirectoryMergeWindow->mergeCurrentFile();
    }
    else if(m_pMainWidget != nullptr && m_pMainWidget->isVisible())
    {
        if(!shouldContinue()) return;

        if(m_outputFilename.isEmpty())
        {
            if(!m_sd3->isEmpty() && !m_sd3->isFromBuffer())
            {
                m_outputFilename = m_sd3->getFilename();
            }
            else if(!m_sd2->isEmpty() && !m_sd2->isFromBuffer())
            {
                m_outputFilename = m_sd2->getFilename();
            }
            else if(!m_sd1->isEmpty() && !m_sd1->isFromBuffer())
            {
                m_outputFilename = m_sd1->getFilename();
            }
            else
            {
                m_outputFilename = "unnamed.txt";
                m_bDefaultFilename = true;
            }
        }
        mainInit();
    }
}

void KDiff3App::slotWinFocusNext()
{
    QWidget* focus = qApp->focusWidget();
    if(focus == m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->isVisible() && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }

    std::list<QWidget*> visibleWidgetList;
    if(m_pDiffTextWindow1 && m_pDiffTextWindow1->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow1);
    if(m_pDiffTextWindow2 && m_pDiffTextWindow2->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow2);
    if(m_pDiffTextWindow3 && m_pDiffTextWindow3->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow3);
    if(m_pMergeResultWindow && m_pMergeResultWindow->isVisible()) visibleWidgetList.push_back(m_pMergeResultWindow);
    if(m_bDirCompare /*m_pDirectoryMergeWindow->isVisible()*/) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
    //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());
    if(visibleWidgetList.empty())
        return;

    std::list<QWidget*>::iterator i = std::find(visibleWidgetList.begin(), visibleWidgetList.end(), focus);
    ++i;
    if(i == visibleWidgetList.end())
        i = visibleWidgetList.begin();

    if(*i == m_pDirectoryMergeWindow && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }
    (*i)->setFocus();
}

void KDiff3App::slotWinFocusPrev()
{
    QWidget* focus = qApp->focusWidget();
    if(focus == m_pDirectoryMergeWindow && m_pDirectoryMergeWindow->isVisible() && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }

    std::list<QWidget*> visibleWidgetList;
    if(m_pDiffTextWindow1 && m_pDiffTextWindow1->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow1);
    if(m_pDiffTextWindow2 && m_pDiffTextWindow2->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow2);
    if(m_pDiffTextWindow3 && m_pDiffTextWindow3->isVisible()) visibleWidgetList.push_back(m_pDiffTextWindow3);
    if(m_pMergeResultWindow && m_pMergeResultWindow->isVisible()) visibleWidgetList.push_back(m_pMergeResultWindow);
    if(m_bDirCompare /* m_pDirectoryMergeWindow->isVisible() */) visibleWidgetList.push_back(m_pDirectoryMergeWindow);
    //if ( m_pDirectoryMergeInfo->isVisible() ) visibleWidgetList.push_back(m_pDirectoryMergeInfo->getInfoList());
    if(visibleWidgetList.empty())
        return;

    std::list<QWidget*>::iterator i = std::find(visibleWidgetList.begin(), visibleWidgetList.end(), focus);
    if(i == visibleWidgetList.begin())
        i = visibleWidgetList.end();
    --i;
    //i will never be
    if(*i == m_pDirectoryMergeWindow && !dirShowBoth->isChecked())
    {
        slotDirViewToggle();
    }
    (*i)->setFocus();
}

void KDiff3App::slotWinToggleSplitterOrientation()
{
    if(m_pDiffWindowSplitter != nullptr)
    {
        m_pDiffWindowSplitter->setOrientation(
            m_pDiffWindowSplitter->orientation() == Qt::Vertical ? Qt::Horizontal : Qt::Vertical);

        m_pOptions->m_bHorizDiffWindowSplitting = m_pDiffWindowSplitter->orientation() == Qt::Horizontal;
    }
}

void KDiff3App::slotOverviewNormal()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMNormal);

    slotUpdateAvailabilities();
}

void KDiff3App::slotOverviewAB()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMAvsB);

    slotUpdateAvailabilities();
}

void KDiff3App::slotOverviewAC()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMAvsC);

    slotUpdateAvailabilities();
}

void KDiff3App::slotOverviewBC()
{
    Q_EMIT changeOverViewMode(e_OverviewMode::eOMBvsC);

    slotUpdateAvailabilities();
}

void KDiff3App::slotNoRelevantChangesDetected()
{
    if(m_bTripleDiff && !m_outputFilename.isEmpty())
    {
        //KMessageBox::information( this, "No relevant changes detected", "KDiff3" );
        if(!m_pOptions->m_IrrelevantMergeCmd.isEmpty())
        {
            /*
                QProcess doesn't check for single quotes and uses non-standard escaping syntax for double quotes.
                    The distinction between single and double quotes is purely a command shell issue. So
                    we split the command string ourselves.
            */
            QStringList args;
            QString program;
            Utils::getArguments(m_pOptions->m_IrrelevantMergeCmd, program, args);
            QProcess process;
            process.start(program, args);
            process.waitForFinished(-1);
        }
    }
}

void KDiff3App::slotAddManualDiffHelp()
{
    LineRef firstLine;
    LineRef lastLine;
    e_SrcSelector winIdx = e_SrcSelector::Invalid;
    if(m_pDiffTextWindow1)
    {
        m_pDiffTextWindow1->getSelectionRange(&firstLine, &lastLine, eFileCoords);
        winIdx = e_SrcSelector::A;
    }
    if(!firstLine.isValid() && m_pDiffTextWindow2)
    {
        m_pDiffTextWindow2->getSelectionRange(&firstLine, &lastLine, eFileCoords);
        winIdx = e_SrcSelector::B;
    }
    if(!firstLine.isValid() && m_pDiffTextWindow3)
    {
        m_pDiffTextWindow3->getSelectionRange(&firstLine, &lastLine, eFileCoords);
        winIdx = e_SrcSelector::C;
    }

    if(!firstLine.isValid() || !lastLine.isValid() || lastLine < firstLine)
        KMessageBox::information(this, i18n("Nothing is selected in either diff input window."), i18n("Error while adding manual diff range"));
    else
    {
        m_manualDiffHelpList.insertEntry(winIdx, firstLine, lastLine);

        mainInit(nullptr, false); // Init without reload
        slotRefresh();
    }
}

void KDiff3App::slotClearManualDiffHelpList()
{
    m_manualDiffHelpList.clear();
    mainInit(nullptr, false); // Init without reload
    slotRefresh();
}

void KDiff3App::slotEncodingChanged(QTextCodec* c)
{
    Q_UNUSED(c);
    mainInit(nullptr, true, true); // Init with reload
    slotRefresh();
}

void KDiff3App::slotUpdateAvailabilities()
{
    if(m_pMainSplitter == nullptr || m_pDiffTextWindow2 == nullptr || m_pDiffTextWindow1 == nullptr || m_pDiffTextWindow3 == nullptr)
        return;

    bool bTextDataAvailable = (m_sd1->hasData() || m_sd2->hasData() || m_sd3->hasData());

    if(dirShowBoth->isChecked())
    {
        if(m_pDirectoryMergeSplitter != nullptr)
            m_pDirectoryMergeSplitter->setVisible(m_bDirCompare);

        if(m_pMainWidget != nullptr && !m_pMainWidget->isVisible() &&
           bTextDataAvailable && !m_pDirectoryMergeWindow->isScanning())
            m_pMainWidget->show();
    }

    bool bDiffWindowVisible = m_pMainWidget != nullptr && m_pMainWidget->isVisible();
    bool bMergeEditorVisible = m_pMergeWindowFrame != nullptr && m_pMergeWindowFrame->isVisible() && m_pMergeResultWindow != nullptr;

    m_pDirectoryMergeWindow->updateAvailabilities(m_bDirCompare, bDiffWindowVisible, chooseA, chooseB, chooseC);

    dirShowBoth->setEnabled(m_bDirCompare);
    dirViewToggle->setEnabled(
        m_bDirCompare &&
        ((m_pDirectoryMergeSplitter != nullptr && m_pMainWidget != nullptr) &&
         ((!m_pDirectoryMergeSplitter->isVisible() && m_pMainWidget->isVisible()) ||
          (m_pDirectoryMergeSplitter->isVisible() && !m_pMainWidget->isVisible() && bTextDataAvailable))));

    bool bDirWindowHasFocus = m_pDirectoryMergeSplitter != nullptr && m_pDirectoryMergeSplitter->isVisible() && m_pDirectoryMergeWindow->hasFocus();

    showWhiteSpaceCharacters->setEnabled(bDiffWindowVisible);
    autoAdvance->setEnabled(bMergeEditorVisible);
    mAutoSolve->setEnabled(bMergeEditorVisible && m_bTripleDiff);
    mUnsolve->setEnabled(bMergeEditorVisible);
    if(!bDirWindowHasFocus)
    {
        chooseA->setEnabled(bMergeEditorVisible);
        chooseB->setEnabled(bMergeEditorVisible);
        chooseC->setEnabled(bMergeEditorVisible && m_bTripleDiff);
    }

    editCut->setEnabled(allowCut());
    if(m_pMergeResultWindow != nullptr)
    {
        m_pMergeResultWindow->slotUpdateAvailabilities();
    }

    mMergeHistory->setEnabled(bMergeEditorVisible);
    mergeRegExp->setEnabled(bMergeEditorVisible);
    showWindowA->setEnabled(bDiffWindowVisible && (m_pDiffTextWindow2->isVisible() || m_pDiffTextWindow3->isVisible()));
    showWindowB->setEnabled(bDiffWindowVisible && (m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow3->isVisible()));
    showWindowC->setEnabled(bDiffWindowVisible && m_bTripleDiff && (m_pDiffTextWindow1->isVisible() || m_pDiffTextWindow2->isVisible()));
    editFind->setEnabled(bDiffWindowVisible);
    editFindNext->setEnabled(bDiffWindowVisible);
    m_pFindDialog->m_pSearchInC->setEnabled(m_bTripleDiff);
    m_pFindDialog->m_pSearchInOutput->setEnabled(bMergeEditorVisible);

    bool bSavable = bMergeEditorVisible && m_pMergeResultWindow->getNrOfUnsolvedConflicts() == 0;
    fileSave->setEnabled(m_bOutputModified && bSavable);
    fileSaveAs->setEnabled(bSavable);

    mGoTop->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaAboveCurrent());
    mGoBottom->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaBelowCurrent());
    mGoCurrent->setEnabled(bDiffWindowVisible);
    mGoPrevUnsolvedConflict->setEnabled(bMergeEditorVisible && m_pMergeResultWindow->isUnsolvedConflictAboveCurrent());
    mGoNextUnsolvedConflict->setEnabled(bMergeEditorVisible && m_pMergeResultWindow->isUnsolvedConflictBelowCurrent());
    mGoPrevConflict->setEnabled(bDiffWindowVisible && bMergeEditorVisible && m_pMergeResultWindow->isConflictAboveCurrent());
    mGoNextConflict->setEnabled(bDiffWindowVisible && bMergeEditorVisible && m_pMergeResultWindow->isConflictBelowCurrent());
    mGoPrevDelta->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaAboveCurrent());
    mGoNextDelta->setEnabled(bDiffWindowVisible && m_pMergeResultWindow != nullptr && m_pMergeResultWindow->isDeltaBelowCurrent());

    overviewModeNormal->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    overviewModeAB->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    overviewModeAC->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    overviewModeBC->setEnabled(m_bTripleDiff && bDiffWindowVisible);
    e_OverviewMode overviewMode = m_pOverview == nullptr ? e_OverviewMode::eOMNormal : m_pOverview->getOverviewMode();
    overviewModeNormal->setChecked(overviewMode == e_OverviewMode::eOMNormal);
    overviewModeAB->setChecked(overviewMode == e_OverviewMode::eOMAvsB);
    overviewModeAC->setChecked(overviewMode == e_OverviewMode::eOMAvsC);
    overviewModeBC->setChecked(overviewMode == e_OverviewMode::eOMBvsC);

    winToggleSplitOrientation->setEnabled(bDiffWindowVisible && m_pDiffWindowSplitter != nullptr);
}
