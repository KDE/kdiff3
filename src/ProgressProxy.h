#ifndef PROGRESSPROXY_H
#define PROGRESSPROXY_H

#include "combiners.h"

#include <boost/signals2.hpp>

#include <QObject>
#include <QString>

class ProgressDialog;
class KJob;

namespace signals2 = boost::signals2;
// When using the ProgressProxy you need not take care of the push and pop, except when explicit.
class ProgressProxy: public QObject
{
    Q_OBJECT
  public:
    ProgressProxy();
    ~ProgressProxy() override;

    void setInformation(const QString& info, bool bRedrawUpdate = true);
    void setInformation(const QString& info, int current, bool bRedrawUpdate = true);
    void setCurrent(qint64 current, bool bRedrawUpdate = true);
    void step(bool bRedrawUpdate = true);
    void clear();
    void setMaxNofSteps(const qint64 maxNofSteps);
    void addNofSteps(const qint64 nofSteps);
    bool wasCancelled();
    void setRangeTransformation(double dMin, double dMax);
    void setSubRangeTransformation(double dMin, double dMax);

    static void exitEventLoop();
    static void enterEventLoop(KJob* pJob, const QString& jobInfo);

    static signals2::signal<void()> push;
    static signals2::signal<void(bool)> pop;
    static signals2::signal<void()> clearSig;

    static signals2::signal<void(KJob*, const QString&)> enterEventLoopSig;
    static signals2::signal<void()> exitEventLoopSig;

    static signals2::signal<void(qint64, bool)> setCurrentSig;
    static signals2::signal<void(qint64)> setMaxNofStepsSig;
    static signals2::signal<void(qint64)> addNofStepsSig;
    static signals2::signal<void(bool)> stepSig;

    static signals2::signal<void(double, double)> setRangeTransformationSig;
    static signals2::signal<void(double, double)> setSubRangeTransformationSig;

    static signals2::signal<bool(), find> wasCancelledSig;

    static signals2::signal<void(const QString&, bool)> setInformationSig;
    static signals2::signal<void(const QString&, int, bool)> setInfoAndStepSig;
};

#endif /* PROGRESSPROXY_H */
