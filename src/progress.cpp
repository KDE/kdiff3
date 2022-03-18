/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "progress.h"
#include "common.h"
#include "defmac.h"

#include <QApplication>
#include <QEventLoop>
#include <QLabel>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QTimerEvent>
#include <QVBoxLayout>

#include <KIO/Job>
#include <KLocalizedString>

namespace placeholders = boost::placeholders;

ProgressDialog* g_pProgressDialog = nullptr;

ProgressDialog::ProgressDialog(QWidget* pParent, QStatusBar* pStatusBar)
    : QDialog(pParent), m_pStatusBar(pStatusBar)
{
    setObjectName("ProgressDialog");
    setModal(true);
    QVBoxLayout* layout = new QVBoxLayout(this);

    m_pInformation = new QLabel(" ", this);
    layout->addWidget(m_pInformation);

    m_pProgressBar = new QProgressBar();
    m_pProgressBar->setRange(0, 1000);
    layout->addWidget(m_pProgressBar);

    m_pSubInformation = new QLabel(" ", this);
    layout->addWidget(m_pSubInformation);

    m_pSubProgressBar = new QProgressBar();
    m_pSubProgressBar->setRange(0, 1000);
    layout->addWidget(m_pSubProgressBar);

    m_pSlowJobInfo = new QLabel(" ", this);
    layout->addWidget(m_pSlowJobInfo);

    QHBoxLayout* hlayout = new QHBoxLayout();
    layout->addLayout(hlayout);
    hlayout->addStretch(1);
    m_pAbortButton = new QPushButton(i18n("&Cancel"), this);
    hlayout->addWidget(m_pAbortButton);
    chk_connect(m_pAbortButton, &QPushButton::clicked, this, &ProgressDialog::slotAbort);
    if(m_pStatusBar != nullptr)
    {
        m_pStatusBarWidget = new QWidget;
        QHBoxLayout* pStatusBarLayout = new QHBoxLayout(m_pStatusBarWidget);
        pStatusBarLayout->setContentsMargins(0, 0, 0, 0);
        pStatusBarLayout->setSpacing(3);
        m_pStatusProgressBar = new QProgressBar;
        m_pStatusProgressBar->setRange(0, 1000);
        m_pStatusProgressBar->setTextVisible(false);
        m_pStatusAbortButton = new QPushButton(i18n("&Cancel"));
        chk_connect(m_pStatusAbortButton, &QPushButton::clicked, this, &ProgressDialog::slotAbort);
        pStatusBarLayout->addWidget(m_pStatusProgressBar);
        pStatusBarLayout->addWidget(m_pStatusAbortButton);
        m_pStatusBar->addPermanentWidget(m_pStatusBarWidget, 0);
        m_pStatusBarWidget->setFixedHeight(m_pStatusBar->height());
        m_pStatusBarWidget->hide();
    }

    resize(400, 100);
#ifndef AUTOTEST
    m_t1.start();
    m_t2.start();
#endif

    initConnections();
}

void ProgressDialog::initConnections()
{
    connections.push_back(ProgressProxy::push.connect(boost::bind(&ProgressDialog::push, this)));
    connections.push_back(ProgressProxy::pop.connect(boost::bind(&ProgressDialog::pop, this, placeholders::_1)));
    connections.push_back(ProgressProxy::clearSig.connect(boost::bind(&ProgressDialog::clear, this)));

    connections.push_back(ProgressProxy::enterEventLoopSig.connect(boost::bind(&ProgressDialog::enterEventLoop, this, placeholders::_1, placeholders::_2)));
    connections.push_back(ProgressProxy::exitEventLoopSig.connect(boost::bind(&ProgressDialog::exitEventLoop, this)));

    connections.push_back(ProgressProxy::setCurrentSig.connect(boost::bind(&ProgressDialog::setCurrent, this, placeholders::_1, placeholders::_2)));
    connections.push_back(ProgressProxy::addNofStepsSig.connect(boost::bind(&ProgressDialog::addNofSteps, this, placeholders::_1)));
    connections.push_back(ProgressProxy::setMaxNofStepsSig.connect(boost::bind(&ProgressDialog::setMaxNofSteps, this, placeholders::_1)));
    connections.push_back(ProgressProxy::stepSig.connect(boost::bind(&ProgressDialog::step, this, placeholders::_1)));

    connections.push_back(ProgressProxy::setRangeTransformationSig.connect(boost::bind(&ProgressDialog::setRangeTransformation, this, placeholders::_1, placeholders::_2)));
    connections.push_back(ProgressProxy::setSubRangeTransformationSig.connect(boost::bind(&ProgressDialog::setSubRangeTransformation, this, placeholders::_1, placeholders::_2)));

    connections.push_back(ProgressProxy::wasCancelledSig.connect(boost::bind(&ProgressDialog::wasCancelled, this)));

    connections.push_back(ProgressProxy::setInformationSig.connect(boost::bind(
        static_cast<void (ProgressDialog::*)(const QString&, bool)>(&ProgressDialog::setInformation),
        this,
        placeholders::_1, placeholders::_2)));
    connections.push_back(ProgressProxy::setInfoAndStepSig.connect(boost::bind(
        static_cast<void (ProgressDialog::*)(const QString&, int, bool)>(&ProgressDialog::setInformation),
        this,
        placeholders::_1, placeholders::_2, placeholders::_3)));
}

void ProgressDialog::setStayHidden(bool bStayHidden)
{
#ifndef AUTOTEST
    if(m_bStayHidden != bStayHidden)
    {
        m_bStayHidden = bStayHidden;
        if(m_pStatusBarWidget != nullptr)
        {
            if(m_bStayHidden)
            {
                if(m_delayedHideStatusBarWidgetTimer)
                {
                    killTimer(m_delayedHideStatusBarWidgetTimer);
                    m_delayedHideStatusBarWidgetTimer = 0;
                }
                m_pStatusBarWidget->show();
            }
            else
                hideStatusBarWidget(); // delayed
        }
        if(isVisible() && m_bStayHidden)
            hide(); // delayed hide
    }
#else
    Q_UNUSED(bStayHidden);
#endif
}

void ProgressDialog::push()
{
#ifndef AUTOTEST
    ProgressLevelData pld;
    if(!m_progressStack.empty())
    {
        pld.m_dRangeMax = m_progressStack.back().m_dSubRangeMax;
        pld.m_dRangeMin = m_progressStack.back().m_dSubRangeMin;
    }
    else
    {
        m_bWasCancelled = false;
        m_t1.restart();
        m_t2.restart();
        if(!m_bStayHidden)
            show();
    }

    m_progressStack.push_back(pld);
#endif
}

void ProgressDialog::pop(bool bRedrawUpdate)
{
#ifndef AUTOTEST
    if(!m_progressStack.empty())
    {
        m_progressStack.pop_back();
        if(m_progressStack.empty())
        {
            hide();
        }
        else
            recalc(bRedrawUpdate);
    }
#else
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressDialog::setInformation(const QString& info, int current, bool bRedrawUpdate)
{
    if(m_progressStack.empty())
        return;

    setCurrentImp(current);
    setInformation(info, bRedrawUpdate);
}

void ProgressDialog::setInformation(const QString& info, bool bRedrawUpdate)
{
    if(m_progressStack.empty())
        return;

    setInformationImp(info);
    recalc(bRedrawUpdate);
}

void ProgressDialog::setMaxNofSteps(const qint64 maxNofSteps)
{
#ifndef AUTOTEST
    if(m_progressStack.empty() || maxNofSteps == 0)
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_maxNofSteps = maxNofSteps;
    pld.m_current = 0;
#else
    Q_UNUSED(maxNofSteps);
#endif
}

void ProgressDialog::setInformationImp(const QString& info)
{
#ifndef AUTOTEST
    assert(!m_progressStack.empty());

    int level = m_progressStack.size();
    if(level == 1)
    {
        m_pInformation->setText(info);
        m_pSubInformation->setText("");
        if(m_pStatusBar && m_bStayHidden)
            m_pStatusBar->showMessage(info);
    }
    else if(level == 2)
    {
        m_pSubInformation->setText(info);
    }
#else
    Q_UNUSED(info);
#endif
}

void ProgressDialog::addNofSteps(const qint64 nofSteps)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_maxNofSteps.fetchAndAddRelaxed(nofSteps);
#else
    Q_UNUSED(nofSteps);
#endif
}

void ProgressDialog::step(bool bRedrawUpdate)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_current.fetchAndAddRelaxed(1);
    recalc(bRedrawUpdate);
#else
    Q_UNUSED(bRedrawUpdate);
#endif
}

void ProgressDialog::setCurrent(qint64 subCurrent, bool bRedrawUpdate)
{
    if(m_progressStack.empty())
        return;
    setCurrentImp(subCurrent);
    recalc(bRedrawUpdate);
}

void ProgressDialog::setCurrentImp(qint64 subCurrent)
{
    Q_ASSERT(!m_progressStack.empty());
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_current = subCurrent;
}

void ProgressDialog::clear()
{
#ifndef AUTOTEST
    if(m_progressStack.isEmpty())
        return;

    ProgressLevelData& pld = m_progressStack.back();
    setCurrent(pld.m_maxNofSteps);
#endif
}

// The progressbar goes from 0 to 1 usually.
// By supplying a subrange transformation the subCurrent-values
// 0 to 1 will be transformed to dMin to dMax instead.
// Requirement: 0 < dMin < dMax < 1
void ProgressDialog::setRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_dRangeMin = dMin;
    pld.m_dRangeMax = dMax;
    pld.m_current = 0;
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressDialog::setSubRangeTransformation(double dMin, double dMax)
{
#ifndef AUTOTEST
    if(m_progressStack.empty())
        return;
    ProgressLevelData& pld = m_progressStack.back();
    pld.m_dSubRangeMin = dMin;
    pld.m_dSubRangeMax = dMax;
#else
    Q_UNUSED(dMin);
    Q_UNUSED(dMax);
#endif
}

void ProgressDialog::enterEventLoop(KJob* pJob, const QString& jobInfo)
{
#ifndef AUTOTEST
    m_pJob = pJob;
    m_currentJobInfo = jobInfo;
    m_pSlowJobInfo->setText(m_currentJobInfo);
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = startTimer(3000); /* 3 s delay */

    // immediately show the progress dialog for KIO jobs, because some KIO jobs require password authentication,
    // but if the progress dialog pops up at a later moment, this might cover the login dialog and hide it from the user.
    if(m_pJob && !m_bStayHidden)
        show();

    // instead of using exec() the eventloop is entered and exited often without hiding/showing the window.
    if(m_eventLoop == nullptr)
    {
        m_eventLoop = QPointer<QEventLoop>(new QEventLoop(this));
        m_eventLoop->exec(); // this function only returns after ProgressDialog::exitEventLoop() is called.
        m_eventLoop.clear();
    }
    else
    {
        m_eventLoop->processEvents(QEventLoop::WaitForMoreEvents);
    }
#else
    Q_UNUSED(pJob);
    Q_UNUSED(jobInfo);
#endif
}

void ProgressDialog::exitEventLoop()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = 0;
    m_pJob = nullptr;
    if(m_eventLoop != nullptr)
        m_eventLoop->exit();
#endif
}

void ProgressDialog::recalc(bool bUpdate)
{
#ifndef AUTOTEST
    if(!m_bWasCancelled)
    {
        if(QThread::currentThread() == m_pGuiThread)
        {
            if(m_progressDelayTimer)
                killTimer(m_progressDelayTimer);
            m_progressDelayTimer = 0;
            if(!m_bStayHidden)
                m_progressDelayTimer = startTimer(3000); /* 3 s delay */

            int level = m_progressStack.size();
            if((bUpdate && level == 1) || m_t1.elapsed() > 200)
            {
                if(m_progressStack.empty())
                {
                    m_pProgressBar->setValue(0);
                    m_pSubProgressBar->setValue(0);
                }
                else
                {
                    QList<ProgressLevelData>::iterator i = m_progressStack.begin();
                    int value = int(1000.0 * (getAtomic(i->m_current) * (i->m_dRangeMax - i->m_dRangeMin) / getAtomic(i->m_maxNofSteps) + i->m_dRangeMin));
                    m_pProgressBar->setValue(value);
                    if(m_bStayHidden && m_pStatusProgressBar)
                        m_pStatusProgressBar->setValue(value);

                    ++i;
                    if(i != m_progressStack.end())
                        m_pSubProgressBar->setValue(int(1000.0 * (getAtomic(i->m_current) * (i->m_dRangeMax - i->m_dRangeMin) / getAtomic(i->m_maxNofSteps) + i->m_dRangeMin)));
                    else
                        m_pSubProgressBar->setValue(int(1000.0 * m_progressStack.front().m_dSubRangeMin));
                }

                if(!m_bStayHidden && !isVisible())
                    show();
                qApp->processEvents();
                m_t1.restart();
            }
        }
        else
        {
            QMetaObject::invokeMethod(this, "recalc", Qt::QueuedConnection, Q_ARG(bool, bUpdate));
        }
    }
#else
    Q_UNUSED(bUpdate);
#endif
}

void ProgressDialog::show()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    if(m_delayedHideTimer)
        killTimer(m_delayedHideTimer);
    m_progressDelayTimer = 0;
    m_delayedHideTimer = 0;
    if(!isVisible() && (parentWidget() == nullptr || parentWidget()->isVisible()))
    {
        QDialog::show();
    }
#endif
}

void ProgressDialog::hide()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = 0;
    // Calling QDialog::hide() directly doesn't always work. (?)
    if(m_delayedHideTimer)
        killTimer(m_delayedHideTimer);
    m_delayedHideTimer = startTimer(100);
#endif
}

void ProgressDialog::delayedHide()
{
#ifndef AUTOTEST
    if(m_pJob != nullptr)
    {
        m_pJob->kill(KJob::Quietly);
        m_pJob = nullptr;
    }
    QDialog::hide();
    m_pInformation->setText("");

    //m_progressStack.clear();

    m_pProgressBar->setValue(0);
    m_pSubProgressBar->setValue(0);
    m_pSubInformation->setText("");
    m_pSlowJobInfo->setText("");
#endif
}

void ProgressDialog::hideStatusBarWidget()
{
#ifndef AUTOTEST
    if(m_delayedHideStatusBarWidgetTimer)
        killTimer(m_delayedHideStatusBarWidgetTimer);
    m_delayedHideStatusBarWidgetTimer = startTimer(100);
#endif
}

void ProgressDialog::delayedHideStatusBarWidget()
{
#ifndef AUTOTEST
    if(m_progressDelayTimer)
        killTimer(m_progressDelayTimer);
    m_progressDelayTimer = 0;
    if(m_pStatusBarWidget != nullptr)
    {
        m_pStatusBarWidget->hide();
        m_pStatusProgressBar->setValue(0);
        m_pStatusBar->clearMessage();
    }
#endif
}

void ProgressDialog::reject()
{
#ifndef AUTOTEST
    cancel(eUserAbort);
    QDialog::reject();
#endif
}

void ProgressDialog::slotAbort()
{
#ifndef AUTOTEST
    reject();
#endif
}

bool ProgressDialog::wasCancelled()
{
#ifndef AUTOTEST
    if(QThread::currentThread() == m_pGuiThread)
    {
        if(m_t2.elapsed() > 100)
        {
            qApp->processEvents();
            m_t2.restart();
        }
    }
    return m_bWasCancelled;
#else
    return false;
#endif
}

void ProgressDialog::clearCancelState()
{
#ifndef AUTOTEST
    m_bWasCancelled = false;
#endif
}

void ProgressDialog::cancel(e_CancelReason eCancelReason)
{
#ifndef AUTOTEST
    if(!m_bWasCancelled)
    {
        m_bWasCancelled = true;
        m_eCancelReason = eCancelReason;
        if(m_eventLoop != nullptr)
            m_eventLoop->exit(1);
    }
#else
    Q_UNUSED(eCancelReason);
#endif
}

ProgressDialog::e_CancelReason ProgressDialog::cancelReason()
{
    return m_eCancelReason;
}

void ProgressDialog::timerEvent(QTimerEvent* te)
{
#ifndef AUTOTEST
    if(te->timerId() == m_progressDelayTimer)
    {
        if(!isVisible() && !m_bStayHidden)
        {
            show();
        }
        m_pSlowJobInfo->setText(m_currentJobInfo);
    }
    else if(te->timerId() == m_delayedHideTimer)
    {
        killTimer(m_delayedHideTimer);
        m_delayedHideTimer = 0;
        delayedHide();
    }
    else if(te->timerId() == m_delayedHideStatusBarWidgetTimer)
    {
        killTimer(m_delayedHideStatusBarWidgetTimer);
        m_delayedHideStatusBarWidgetTimer = 0;
        delayedHideStatusBarWidget();
    }
#else
    Q_UNUSED(te);
#endif
}
