#include "instagraphrequest.h"
#include "cripto/hmacsha.h"

#include <QDataStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

InstagraphRequest::InstagraphRequest(QObject *parent) : QObject(parent)
{
    this->m_data_path =  QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));

    this->m_manager = new QNetworkAccessManager();
    this->m_jar = new QNetworkCookieJar;

    if(!m_data_path.exists())
    {
        m_data_path.mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    }


}

void InstagraphRequest::fileRquest(QString endpoint, QString boundary, QByteArray data)
{
    QFile f(m_data_path.absolutePath()+QStringLiteral("/cookies.dat"));
    f.open(QIODevice::ReadOnly);
    QDataStream s(&f);

    QUrl url(API_URL+endpoint);
    QNetworkRequest request(url);

    while(!s.atEnd()){
        QByteArray c;
        s >> c;
        QList<QNetworkCookie> list = QNetworkCookie::parseCookies(c);
        if(list.count() > 0)
        {
            this->m_jar->insertCookie(list.at(0));
        }
    }

    request.setRawHeader("Connection","close");
    request.setRawHeader("Accept","*/*");
    request.setHeader(QNetworkRequest::ContentTypeHeader, QString(QStringLiteral("multipart/form-data; boundary=")+boundary).toUtf8());
    request.setHeader(QNetworkRequest::ContentLengthHeader,data.size());
    request.setHeader(QNetworkRequest::UserAgentHeader,USER_AGENT);

    request.setRawHeader("Cookie2","$Version=1");
    request.setRawHeader("Accept-Language","en-US");
    request.setRawHeader("Accept-Encoding","gzip");
    request.setRawHeader("X-IG-Capabilities",X_IG_CAPABILITIES.toUtf8());
    request.setRawHeader("X-IG-Connection-Type","WIFI");

    this->m_manager->setCookieJar(this->m_jar);
    this->m_reply = this->m_manager->post(request,data);

    QObject::connect(this->m_reply, SIGNAL(uploadProgress(qint64, qint64)), SLOT(progressChanged(qint64, qint64)));
    QObject::connect(this->m_reply, SIGNAL(finished()), this, SLOT(finishGetUrl()));
    QObject::connect(this->m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(saveCookie()));
}

void InstagraphRequest::request(QString endpoint, QByteArray post, bool apiV2)
{
    QFile f(m_data_path.absolutePath()+QStringLiteral("/cookies.dat"));
    f.open(QIODevice::ReadOnly);
    QDataStream s(&f);

    QString api_url = apiV2 ? API_URL2 : API_URL;

    QUrl url(api_url+endpoint);
    QNetworkRequest request(url);

    while(!s.atEnd()){
        QByteArray c;
        s >> c;
        QList<QNetworkCookie> list = QNetworkCookie::parseCookies(c);
        if(list.count() > 0)
        {
            this->m_jar->insertCookie(list.at(0));
        }
    }

    request.setRawHeader("Connection","close");
    request.setRawHeader("Accept","*/*");
    request.setRawHeader("Content-type","application/x-www-form-urlencoded; charset=UTF-8");
    request.setRawHeader("Cookie2","$Version=1");
    request.setRawHeader("Accept-Language","en-US");
    request.setRawHeader("User-Agent",USER_AGENT.toUtf8());
    request.setRawHeader("X-IG-Capabilities",X_IG_CAPABILITIES.toUtf8());
    request.setRawHeader("X-IG-Connection-Type","WIFI");

    this->m_manager->setCookieJar(this->m_jar);
    this->m_reply = this->m_manager->post(request,post);

    QObject::connect(this->m_reply, SIGNAL(finished()), this, SLOT(finishGetUrl()));
    QObject::connect(this->m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(saveCookie()));
}

void InstagraphRequest::directRquest(QString endpoint, QString boundary, QByteArray data)
{
    QFile f(m_data_path.absolutePath()+QStringLiteral("/cookies.dat"));
    f.open(QIODevice::ReadOnly);
    QDataStream s(&f);

    QUrl url(API_URL+endpoint);
    QNetworkRequest request(url);

    while(!s.atEnd()){
        QByteArray c;
        s >> c;
        QList<QNetworkCookie> list = QNetworkCookie::parseCookies(c);
        if(list.count() > 0)
        {
            this->m_jar->insertCookie(list.at(0));
        }
    }

    request.setRawHeader("Connection","keep-alive");
    request.setRawHeader("Accept","*/*");
    request.setHeader(QNetworkRequest::ContentTypeHeader,QString(QStringLiteral("multipart/form-data; boundary=")+boundary).toUtf8());
    request.setHeader(QNetworkRequest::ContentLengthHeader,data.size());
    request.setHeader(QNetworkRequest::UserAgentHeader,USER_AGENT);

    request.setRawHeader("Accept-Language","en-en");

    this->m_manager->setCookieJar(this->m_jar);
    this->m_reply = this->m_manager->post(request,data);

    QObject::connect(this->m_reply, SIGNAL(finished()), this, SLOT(finishGetUrl()));
    QObject::connect(this->m_manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(saveCookie()));
}

void InstagraphRequest::finishGetUrl()
{
    this->m_reply->deleteLater();
    QVariant answer = QString::fromUtf8(this->m_reply->readAll());
    if(answer.toString().length() > 1)
    {
        //qDebug() << answer;
        Q_EMIT replySrtingReady(answer);
    }
}

void InstagraphRequest::saveCookie()
{
    QList<QNetworkCookie> list =
        m_manager->cookieJar()->cookiesForUrl(QUrl(API_URL+QStringLiteral("/")));

    QFile f(m_data_path.absolutePath()+QStringLiteral("/cookies.dat"));
    f.open(QIODevice::ReadWrite);
    for(int i = 0; i < list.size(); ++i){
        QDataStream s(&f);
        s << list.at(i).toRawForm();
    }

    f.close();
}

void InstagraphRequest::progressChanged(qint64 a, qint64 b)
{
    if (b > 0) {
        Q_EMIT progressReady(100.0*(double)a/(double)b);
    }
}

QString InstagraphRequest::generateSignature(QJsonObject data)
{
    QJsonDocument data_doc(data);
    QString data_string( data_doc.toJson(QJsonDocument::Compact));

    //Fix to image config string
    data_string.replace(QStringLiteral("\"crop_center\":[0,0]"),QStringLiteral("\"crop_center\":[0.0,-0.0]"));

    HmacSHA *hmac = new HmacSHA();
    QByteArray hash = hmac->hash(data_string.toUtf8(), IG_SIG_KEY.toUtf8());

    return QString(QStringLiteral("ig_sig_key_version=")+SIG_KEY_VERSION+QStringLiteral("&signed_body=")+hash.toHex()+QStringLiteral(".")+data_string.toUtf8());
}

