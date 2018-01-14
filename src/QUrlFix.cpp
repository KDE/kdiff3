#include <QUrl>
#include "QUrlFix.h"

 QUrlFix::QUrlFix(const QString &url):QUrl(url, TolerantMode)
 {
     if(this->scheme().isEmpty()) this->setScheme("file");//so QUrl:isLocalFile: works as expected on QT5.
 }

#ifndef QT_NO_URL_CAST_FROM_STRING
QUrlFix& QUrlFix::operator =(const QString &url){
    setUrl(url);
    return *this;
}
#endif
void QUrlFix::setUrl(const QString &url){
    QUrl::setUrl(url);
    if(this->scheme().isEmpty()) this->setScheme("file");//so QUrl:isLocalFile: works as expected on QT5.
}
    
