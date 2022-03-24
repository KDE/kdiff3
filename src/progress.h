/*
 * KDiff3 - Text Diff And Merge Tool
 *
 * SPDX-FileCopyrightText: 2002-2011 Joachim Eibl, joachim.eibl at gmx.de
 * SPDX-FileCopyrightText: 2018-2020 Michael Reeves reeves.87@gmail.com
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PROGRESS_H
#define PROGRESS_H

#include "ProgressProxy.h"

#include <QDialog>
#include <QElapsedTimer>
#include <QList>
#include <QPointer>
#include <QThread>

class KJob;
class QEventLoop;
class QLabel;
class QProgressBar;
class QStatusBar;

/*
    Due to Qt/automoc wonkyness multiple inhertance can not be used with by this class.
*/
class ProgressDialog: public QDialog
{
    Q_OBJECT
  public:
    ProgressDialog(QWidget* pParent, QStatusBar*);

    void setStayHidden(bool bStayHidden);
    void setInformation(const QString& info, bool bRedrawUpdate = true);
    void setInformation(const QString& info, int current, bool bRedrawUpdate = true);
    void setCurrent(qint64 current, bool bRedrawUpdate = true);
    void step(bool bRedrawUpdate = true);
    void clear();
    void setMaxNofSteps(const qint64 dMaxNofSteps);
    void addNofSteps(const qint64 nofSteps);
    void push();
    void pop(bool bRedrawUpdate = true);

    // The progressbar goes from 0 to 1 usually.
    // By supplying a subrange transformation the subCurrent-values
    // 0 to 1 will be transformed to dMin to dMax instead.
    // Requirement: 0 < dMin < dMax < 1
    void setRangeTransformation(double dMin, double dMax);
    void setSubRangeTransformation(double dMin, double dMax);

    void exitEventLoop();
    void enterEventLoop(KJob* pJob, const QString& jobInfo);

    bool wasCancelled();
    enum e_CancelReason
    {
        eUserAbort,
        eResize
    };
    void cancel(e_CancelReason);
    e_CancelReason cancelReason();
    void clearCancelState();
    void show();
    void hide();
    void hideStatusBarWidget();
    void delayedHideStatusBarWidget();

    void timerEvent(QTimerEvent* event) override;

  protected:
    void reject() override;

  private:
    void setInformationImp(const QString& info);
    void setCurrentImp(qint64 subCurrent);
    void initConnections();

    //Treated as slot by direct call to QMetaObject::invokeMethod
  public Q_SLOTS:
    void recalc(bool bUpdate);

  private Q_SLOTS:
    void delayedHide();
    void slotAbort();

  private:
    struct ProgressLevelData {
        ProgressLevelData()
        {
            m_current = 0;
            m_maxNofSteps = 1;
            m_dRangeMin = 0;
            m_dRangeMax = 1;
            m_dSubRangeMin = 0;
            m_dSubRangeMax = 1;
        }
        QAtomicInteger<qint64> m_current;
        QAtomicInteger<qint64> m_maxNofSteps; // when step() is used.
        double m_dRangeMax;
        double m_dRangeMin;
        double m_dSubRangeMax;
        double m_dSubRangeMin;
    };
    QList<ProgressLevelData> m_progressStack;

    int m_progressDelayTimer = 0;
    int m_delayedHideTimer = 0;
    int m_delayedHideStatusBarWidgetTimer = 0;
    QPointer<QEventLoop> m_eventLoop;

    QProgressBar* m_pProgressBar;
    QProgressBar* m_pSubProgressBar;
    QLabel* m_pInformation;
    QLabel* m_pSubInformation;
    QLabel* m_pSlowJobInfo;
    QPushButton* m_pAbortButton;
    QElapsedTimer m_t1;
    QElapsedTimer m_t2;
    bool m_bWasCancelled = false;
    e_CancelReason m_eCancelReason = eUserAbort;
    KJob* m_pJob = nullptr;
    QString m_currentJobInfo; // Needed if the job doesn't stop after a reasonable time.
    bool m_bStayHidden = false;
    QThread* m_pGuiThread = QThread::currentThread();
    QStatusBar* m_pStatusBar = nullptr; // status bar of main window (if exists)
    QWidget* m_pStatusBarWidget = nullptr;
    QProgressBar* m_pStatusProgressBar = nullptr;
    QPushButton* m_pStatusAbortButton = nullptr;
    /*
        This list exists solely to auto disconnect boost signals.
    */
    std::list<boost::signals2::scoped_connection> connections;
};

extern ProgressDialog* g_pProgressDialog;

#endif
