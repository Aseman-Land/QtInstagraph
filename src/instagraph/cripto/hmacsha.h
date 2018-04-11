#ifndef HMACSHA_H
#define HMACSHA_H

#include <QObject>
#include <QCryptographicHash>

#include "libqinstagraph_global.h"

class LIBQINSTAGRAPHSHARED_EXPORT HmacSHA : public QObject
{
    Q_OBJECT
public:
    explicit HmacSHA(QObject *parent = 0);
    static QByteArray hash(QByteArray stringToSign, QByteArray secretKey);
};

#endif // HMACSHA_H
