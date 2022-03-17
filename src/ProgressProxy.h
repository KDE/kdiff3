#ifndef PROGRESSPROXY_H
#define PROGRESSPROXY_H

#include <QDialog>
#include <QObject>
#include <QString>

class ProgressDialog;
class KJob;

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
    static QDialog* getDialog();

  private:
};

extern ProgressDialog* g_pProgressDialog;

#endif /* PROGRESSPROXY_H */
