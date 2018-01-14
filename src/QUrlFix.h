#ifndef QURLFIX_H
#define QURLFIX_H

#include <QUrl>

class QUrlFix:public QUrl{
public:
    QUrlFix():QUrl(){};//make compiler shutup
    QUrlFix(const QUrl &copy):QUrl(copy){};//make compiler shutup
    QUrlFix(const QString &url);
    void setUrl(const QString &url);
    QUrlFix& operator =(const QString &url);
};

#endif