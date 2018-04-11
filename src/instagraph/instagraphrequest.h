#ifndef INSTAGRAPHREQUEST_H
#define INSTAGRAPHREQUEST_H

#include <QDir>
#include <QNetworkAccessManager>
#include <QObject>

#include "libqinstagraph_global.h"

class LIBQINSTAGRAPHSHARED_EXPORT InstagraphRequest : public QObject
{
    Q_OBJECT
public:
    explicit InstagraphRequest(QObject *parent = 0);

    void request(QString endpoint, QByteArray post, bool apiV2 = false);
    void fileRquest(QString endpoint, QString boundary, QByteArray data);
    void directRquest(QString endpoint, QString boundary, QByteArray data);
    QString generateSignature(QJsonObject data);
    QString buildBody(QList<QList<QString> > bodies, QString boundary);

private:
    QString API_URL             = QStringLiteral("https://i.instagram.com/api/v1/");
    QString API_URL2            = QStringLiteral("https://i.instagram.com/api/v2/");

    QString C_USER_AGENT        = QStringLiteral("Mozilla/5.0 (iPhone; CPU iPhone OS 9_3_3 like Mac OS X) AppleWebKit/601.1.46 (KHTML, like Gecko) Mobile/13G34 Instagram 8.5.2 (iPhone5,2; iPhone OS 9_3_3; es_ES; es-ES; scale=2.00; 640x1136)");

    // Old
    //QString USER_AGENT          = QStringLiteral("Instagram 10.15.0 Android (18/4.3; 320dpi; 720x1280; Xiaomi; HM 1SW; armani; qcom; en_US)");
    //QString IG_SIG_KEY          = QStringLiteral("b03e0daaf2ab17cda2a569cace938d639d1288a1197f9ecf97efd0a4ec0874d7");

    // New
    QString USER_AGENT          = QStringLiteral("Instagram 22.0.0.15.68 Android (24/7.0; 640dpi; 1440x2560; HUAWEI; LON-L29; HWLON; hi3660)");
    QString IG_SIG_KEY          = QStringLiteral("f372b2a5f14d1bebedaaa4ac6f8d506db30ffdd6185b8e0cdfa7dab42f5a9cc6");

    QString SIG_KEY_VERSION     = QStringLiteral("4");
    QString X_IG_CAPABILITIES   = QStringLiteral("3brDAw==");

    QDir m_data_path;

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    QNetworkCookieJar *m_jar;

Q_SIGNALS:
    void replySrtingReady(QVariant ans);

    void progressReady(double ans);

public Q_SLOTS:

private Q_SLOTS:
    void finishGetUrl();
    void saveCookie();

    void progressChanged(qint64 a, qint64 b);
};

#endif // INSTAGRAPHREQUEST_H
