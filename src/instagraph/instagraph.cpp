#include <instagraph.h>
#include <instagraphrequest.h>

#include <QCryptographicHash>

#include <QFileInfo>
#include <QStandardPaths>
#include <QDateTime>
#include <QUuid>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImage>
#include <QDataStream>
#include <QUrl>

#include <QDebug>

Instagraph::Instagraph(QObject *parent)
    : QObject(parent),
      m_busy(false)
{
    this->m_data_path =  QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    this->m_photos_path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));

    if(!m_data_path.exists())
    {
        m_data_path.mkpath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    }

    if(!m_photos_path.exists())
    {
        m_photos_path.mkpath(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    }

    QUuid uuid;
    this->m_uuid = uuid.createUuid().toString();

    this->m_device_id = this->generateDeviceId();

    this->setUser();
}

bool Instagraph::busy() const
{
    return m_busy;
}

QString Instagraph::error() const
{
    return m_error;
}

QString Instagraph::photos_path() const
{
    return m_photos_path.absolutePath();
}

QString Instagraph::generateDeviceId()
{
    QFileInfo fi(m_data_path.absolutePath());
    QByteArray volatile_seed = QString::number(fi.created().toMSecsSinceEpoch()).toUtf8();

    QByteArray data_1 = QCryptographicHash::hash(
                        QString(this->m_username+this->m_password).toUtf8(),
                        QCryptographicHash::Md5).toHex();

    QString data_2 = QString(QCryptographicHash::hash(
                QString(data_1+volatile_seed).toUtf8(),
                QCryptographicHash::Md5).toHex());

    QString data = QStringLiteral("android-")+data_2.left(16);

    return data;
}


void Instagraph::setUser()
{
    if(this->m_username.length() == 0 or this->m_password.length() == 0)
    {
        Q_EMIT error(QStringLiteral("Username anr/or password is clean"));
    }
    else
    {
        QFile f_cookie(m_data_path.absolutePath()+QStringLiteral("/cookies.dat"));
        QFile f_userId(m_data_path.absolutePath()+QStringLiteral("/userId.dat"));
        QFile f_token(m_data_path.absolutePath()+QStringLiteral("/token.dat"));

        if(f_cookie.exists() && f_userId.exists() && f_token.exists())
        {
            this->m_isLoggedIn = true;
            this->m_username_id = f_userId.readAll().trimmed();
            this->m_rank_token = this->m_username_id+QStringLiteral("_")+this->m_uuid;
            this->m_token = f_token.readAll().trimmed();

            this->doLogin();
        }
    }
}

void Instagraph::login(bool forse)
{
    if(!this->m_isLoggedIn or forse)
    {
        this->setUser();

        Instagraph::syncFeatures(true);

        InstagraphRequest *loginRequest = new InstagraphRequest();
        loginRequest->request(QStringLiteral("si/fetch_headers/?challenge_type=signup&guid=")+this->m_uuid,NULL);
        QObject::connect(loginRequest,SIGNAL(replySrtingReady(QVariant)),this,SLOT(doLogin()));
    }
}

void Instagraph::logout()
{
    QFile f_cookie(m_data_path.absolutePath()+QStringLiteral("/cookies.dat"));
    QFile f_userId(m_data_path.absolutePath()+QStringLiteral("/userId.dat"));
    QFile f_token(m_data_path.absolutePath()+QStringLiteral("/token.dat"));

    f_cookie.remove();
    f_userId.remove();
    f_token.remove();

    InstagraphRequest *looutRequest = new InstagraphRequest();
    looutRequest->request(QStringLiteral("accounts/logout/"),NULL);
    QObject::connect(looutRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(doLogout(QVariant)));
}

void Instagraph::doLogin()
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *request = new InstagraphRequest();
    QRegExp rx(QStringLiteral("token=(\\w+);"));
    QFile f(m_data_path.absolutePath()+QStringLiteral("/cookies.dat"));
    if (!f.open(QFile::ReadOnly))
    {
        //qDebug() << m_data_path.absolutePath()+QStringLiteral("/cookies.dat");
        qDebug() << f.errorString();
        Q_EMIT error(QStringLiteral("Can`t open token file"));
    }
    QTextStream in(&f);
    rx.indexIn(in.readAll());
    if(rx.cap(1).length() > 0)
    {
        this->m_token = rx.cap(1);
        //qDebug() << rx.cap(1);
    }
    else
    {
        Q_EMIT error(QStringLiteral("Can`t find token"));
    }
    QUuid uuid;

    QJsonObject data;
        data.insert(QStringLiteral("phone_id"),     uuid.createUuid().toString());
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("username"),     this->m_username);
        data.insert(QStringLiteral("guid"),         this->m_uuid);
        data.insert(QStringLiteral("device_id"),    this->m_device_id);
        data.insert(QStringLiteral("password"),     this->m_password);
        data.insert(QStringLiteral("login_attempt_count"), QString(QStringLiteral("0")));

    QString signature = request->generateSignature(data);
    request->request(QStringLiteral("accounts/login/"),signature.toUtf8());

    QObject::connect(request,SIGNAL(replySrtingReady(QVariant)),this,SLOT(profileConnect(QVariant)));
}

void Instagraph::profileConnect(const QVariant &profile)
{
    QJsonDocument profile_doc = QJsonDocument::fromJson(profile.toString().toUtf8());
    QJsonObject profile_obj = profile_doc.object();

    //qDebug() << QStringLiteral("Reply: ") << profile_obj;

    if(profile_obj[QStringLiteral("status")].toString().toUtf8() == QStringLiteral("fail"))
    {
        Q_EMIT error(profile_obj[QStringLiteral("message")].toString().toUtf8());
        Q_EMIT profileConnectedFail();
    }
    else
    {

        QJsonObject user = profile_obj[QStringLiteral("logged_in_user")].toObject();

        this->m_isLoggedIn = true;
        this->m_username_id = QString::number(user[QStringLiteral("pk")].toDouble(),'g', 10);
        this->m_rank_token = this->m_username_id+QStringLiteral("_")+this->m_uuid;

        this->syncFeatures();
        this->autoCompleteUserList();

        Q_EMIT profileConnected(profile);
    }

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::syncFeatures(bool prelogin)
{
    if (prelogin) {
        InstagraphRequest *syncRequest = new InstagraphRequest();
        QJsonObject data;;
            data.insert(QStringLiteral("id"),           this->m_uuid);
            data.insert(QStringLiteral("experiments"),  LOGIN_EXPERIMENTS);

        QString signature = syncRequest->generateSignature(data);
        syncRequest->request(QStringLiteral("qe/sync/"),signature.toUtf8());
    } else {
        InstagraphRequest *syncRequest = new InstagraphRequest();
        QJsonObject data;
            data.insert(QStringLiteral("_uuid"),        this->m_uuid);
            data.insert(QStringLiteral("_uid"),         this->m_username_id);
            data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
            data.insert(QStringLiteral("id"),           this->m_username_id);
            data.insert(QStringLiteral("experiments"),  EXPERIMENTS);

        QString signature = syncRequest->generateSignature(data);
        syncRequest->request(QStringLiteral("qe/sync/"),signature.toUtf8());
    }
}

void Instagraph::autoCompleteUserList()
{
    InstagraphRequest *autoCompleteUserListRequest = new InstagraphRequest();
    autoCompleteUserListRequest->request(QStringLiteral("friendships/autocomplete_user_list/?version=2"),NULL);
    QObject::connect(autoCompleteUserListRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(autoCompleteUserListReady(QVariant)));
}

void Instagraph::postImage(const QString &path, const QString &caption, const QVariantMap &location, QString upload_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    lastUploadLocation = location;

    this->m_caption = caption;
    this->m_image_path = path;

    QFile image(path);
    image.open(QIODevice::ReadOnly);
    QByteArray dataStream = image.readAll();

    QFileInfo info(image.fileName());
    QString ext = info.completeSuffix();

    QString boundary = this->m_uuid;

    if(upload_id.size() == 0)
    {
        upload_id = QString::number(QDateTime::currentMSecsSinceEpoch());
    }
    /*Body build*/
    QString body = QStringLiteral("");
    body += QStringLiteral("--") + boundary + QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"upload_id\"\r\n\r\n");
    body += upload_id+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"_uuid\"\r\n\r\n");
    body += this->m_uuid.replace(QStringLiteral("{"),QStringLiteral("")).replace(QStringLiteral("}"),QStringLiteral(""))+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"_csrftoken\"\r\n\r\n");
    body += this->m_token+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"image_compression\"\r\n\r\n");
    body += QStringLiteral("{\"lib_name\":\"jt\",\"lib_version\":\"1.3.0\",\"quality\":\"87\"}\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"photo\"; filename=\"pending_media_")+upload_id+QStringLiteral(".")+ext+QStringLiteral("\"\r\n");
    body += QStringLiteral("Content-Transfer-Encoding: binary\r\n");
    body += QStringLiteral("Content-Type: application/octet-stream\r\n\r\n");

    body += dataStream+QStringLiteral("\r\n");
    body += QStringLiteral("--")+boundary+QStringLiteral("--");

    InstagraphRequest *putPhotoReqest = new InstagraphRequest();
    putPhotoReqest->fileRquest(QStringLiteral("upload/photo/"),boundary, body.toUtf8());

    QObject::connect(putPhotoReqest,SIGNAL(progressReady(double)),this,SIGNAL(imageUploadProgressDataReady(double)));
    QObject::connect(putPhotoReqest,SIGNAL(replySrtingReady(QVariant)),this,SLOT(configurePhoto(QVariant)));
}

void Instagraph::configurePhoto(const QVariant &answer)
{
    QJsonDocument jsonResponse = QJsonDocument::fromJson(answer.toByteArray());
    QJsonObject jsonObject = jsonResponse.object();
    if(jsonObject[QStringLiteral("status")].toString() != QString(QStringLiteral("ok")))
    {
        Q_EMIT error(jsonObject[QStringLiteral("message")].toString());
    }
    else
    {
        QString upload_id = jsonObject[QStringLiteral("upload_id")].toString();
        if(upload_id.length() == 0)
        {
            Q_EMIT error(QStringLiteral("Wrong UPLOAD_ID:")+upload_id);
        }
        else
        {
            QImage image = QImage(this->m_image_path);
            InstagraphRequest *configureImageRequest = new InstagraphRequest();

            //qDebug() << QStringLiteral("width: ") << image.width();
            //qDebug() << QStringLiteral("height: ") << image.height();

            QJsonObject device;
                device.insert(QStringLiteral("manufacturer"),   QString(QStringLiteral("Xiaomi")));
                device.insert(QStringLiteral("model"),          QString(QStringLiteral("HM 1SW")));
                device.insert(QStringLiteral("android_version"),18);
                device.insert(QStringLiteral("android_release"),QString(QStringLiteral("4.3")));

            QJsonObject extra;
                extra.insert(QStringLiteral("source_width"),    image.width());
                extra.insert(QStringLiteral("source_height"),   image.height());

            QJsonArray crop_original_size;
                crop_original_size.append(image.width());
                crop_original_size.append(image.height());

            QJsonArray crop_center;
                crop_center.append(0.0);
                crop_center.append(-0.0);

            QJsonObject edits;
                edits.insert(QStringLiteral("crop_original_size"), crop_original_size);
                edits.insert(QStringLiteral("crop_zoom"),          1);
                edits.insert(QStringLiteral("crop_center"),        crop_center);

            QJsonObject data;
                data.insert(QStringLiteral("_csrftoken"),           QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
                data.insert(QStringLiteral("media_folder"),         QStringLiteral("Instagram"));
                data.insert(QStringLiteral("source_type"),          4);
                data.insert(QStringLiteral("_uid"),                 this->m_username_id);
                data.insert(QStringLiteral("_uuid"),                this->m_uuid);
                data.insert(QStringLiteral("caption"),              this->m_caption);
                data.insert(QStringLiteral("upload_id"),            upload_id);
                data.insert(QStringLiteral("device"),               device);
                data.insert(QStringLiteral("edits"),                edits);
                data.insert(QStringLiteral("extra"),                extra);

                if (lastUploadLocation.count() > 0 && lastUploadLocation[QStringLiteral("name")].toString().length() > 0) {
                    QJsonObject location;
                    QString eisk = lastUploadLocation[QStringLiteral("external_id_source")].toString() + QStringLiteral("_id");
                    location.insert(eisk, lastUploadLocation[QStringLiteral("external_id")].toString());
                    location.insert(QStringLiteral("name"),             lastUploadLocation[QStringLiteral("name")].toString());
                    location.insert(QStringLiteral("lat"),              lastUploadLocation[QStringLiteral("lat")].toString());
                    location.insert(QStringLiteral("lng"),              lastUploadLocation[QStringLiteral("lng")].toString());
                    location.insert(QStringLiteral("address"),          lastUploadLocation[QStringLiteral("address")].toString());
                    location.insert(QStringLiteral("external_source"),  lastUploadLocation[QStringLiteral("external_id_source")].toString());

                    QJsonDocument doc(location);
                    QString strJson(doc.toJson(QJsonDocument::Compact));

                    data.insert(QStringLiteral("location"),             strJson);
                    data.insert(QStringLiteral("geotag_enabled"),       true);
                    data.insert(QStringLiteral("media_latitude"),       lastUploadLocation[QStringLiteral("lat")].toString());
                    data.insert(QStringLiteral("posting_latitude"),     lastUploadLocation[QStringLiteral("lat")].toString());
                    data.insert(QStringLiteral("media_longitude"),      lastUploadLocation[QStringLiteral("lng")].toString());
                    data.insert(QStringLiteral("posting_longitude"),    lastUploadLocation[QStringLiteral("lng")].toString());
                    data.insert(QStringLiteral("altitude"),             rand() % 10 + 800);
                }


            QString signature = configureImageRequest->generateSignature(data);
            configureImageRequest->request(QStringLiteral("media/configure/"),signature.toUtf8());
            QObject::connect(configureImageRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(imageConfigureDataReady(QVariant)));

            lastUploadLocation.clear();

            m_busy = false;
            Q_EMIT busyChanged();
        }
    }
    this->m_caption = QStringLiteral("");
    this->m_image_path = QStringLiteral("");
}

//FIXME: uploadImage is not public yeat. Give me few weeks to optimize code
void Instagraph::postVideo(QFile *video)
{
    Q_UNUSED(video)
}

void Instagraph::editMedia(const QString &mediaId, const QString &captionText)
{
    InstagraphRequest *editMediaRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("caption_text"), captionText);

    QString signature = editMediaRequest->generateSignature(data);
    editMediaRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/edit_media/"),signature.toUtf8());
    QObject::connect(editMediaRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(mediaEdited(QVariant)));
}

QString Instagraph::mediaShortcodeToMediaID(const QString &shortcode)
{
    QChar chr;
    qint64 id = 0;
    QString alphabet = QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_");
    for (qint32 i = 0; i < shortcode.length(); i++) {
        chr = shortcode[i];
        id = (id * 64) + alphabet.indexOf(chr);
    }
    return QString::number(id);
}

void Instagraph::infoMedia(const QString &mediaId)
{
    InstagraphRequest *infoMediaRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("media_id"), mediaId);

    QString signature = infoMediaRequest->generateSignature(data);
    infoMediaRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/info/"),signature.toUtf8());
    QObject::connect(infoMediaRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(mediaInfoReady(QVariant)));
}

void Instagraph::deleteMedia(const QString &mediaId)
{
    InstagraphRequest *deleteMediaRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("media_id"),    mediaId);

    QString signature = deleteMediaRequest->generateSignature(data);
    deleteMediaRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/delete/"),signature.toUtf8());
    QObject::connect(deleteMediaRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(mediaDeleted(QVariant)));
}

void Instagraph::removeSelftag(const QString &mediaId)
{
    InstagraphRequest *removeSelftagRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = removeSelftagRequest->generateSignature(data);
    removeSelftagRequest->request(QStringLiteral("usertags/")+mediaId+QStringLiteral("/remove/"),signature.toUtf8());
    QObject::connect(removeSelftagRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(removeSelftagDone(QVariant)));
}

void Instagraph::enableMediaComments(const QString &mediaId)
{
    InstagraphRequest *enableMediaCommentsRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = enableMediaCommentsRequest->generateSignature(data);
    enableMediaCommentsRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/enable_comments/"),signature.toUtf8());
    QObject::connect(enableMediaCommentsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(enableMediaCommentsReady(QVariant)));
}

void Instagraph::disableMediaComments(const QString &mediaId)
{
    InstagraphRequest *enableMediaCommentsRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = enableMediaCommentsRequest->generateSignature(data);
    enableMediaCommentsRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/disable_comments/"),signature.toUtf8());
    QObject::connect(enableMediaCommentsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(disableMediaCommentsReady(QVariant)));
}

void Instagraph::postComment(const QString &mediaId, const QString &commentText)
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *postCommentRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("comment_text"), commentText);

    QString signature = postCommentRequest->generateSignature(data);
    postCommentRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/comment/"),signature.toUtf8());
    QObject::connect(postCommentRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(commentPosted(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::deleteComment(const QString &mediaId, const QString &commentId)
{
    InstagraphRequest *deleteCommentRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = deleteCommentRequest->generateSignature(data);
    deleteCommentRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/comment/")+commentId+QStringLiteral("/delete/"),signature.toUtf8());
    QObject::connect(deleteCommentRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(commentDeleted(QVariant)));
}

void Instagraph::likeComment(const QString &commentId)
{
    InstagraphRequest *likeCommentRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = likeCommentRequest->generateSignature(data);
    likeCommentRequest->request(QStringLiteral("media/")+commentId+QStringLiteral("/comment_like/"),signature.toUtf8());
    QObject::connect(likeCommentRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(commentLiked(QVariant)));
}

void Instagraph::unLikeComment(const QString &commentId)
{
    InstagraphRequest *unLikeCommentRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = unLikeCommentRequest->generateSignature(data);
    unLikeCommentRequest->request(QStringLiteral("media/")+commentId+QStringLiteral("/comment_unlike/"),signature.toUtf8());
    QObject::connect(unLikeCommentRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(commentUnLiked(QVariant)));
}

void Instagraph::saveMedia(const QString &mediaId)
{
    InstagraphRequest *saveMediaRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = saveMediaRequest->generateSignature(data);
    saveMediaRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/save/"),signature.toUtf8());
    QObject::connect(saveMediaRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(saveMediaDataReady(QVariant)));
}

void Instagraph::unsaveMedia(const QString &mediaId)
{
    InstagraphRequest *unsaveMediaRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = unsaveMediaRequest->generateSignature(data);
    unsaveMediaRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/unsave/"),signature.toUtf8());
    QObject::connect(unsaveMediaRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(unsaveMediaDataReady(QVariant)));
}

void Instagraph::getSavedFeed(const QString &max_id)
{
    QString target =QStringLiteral("feed/saved/");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("?max_id=")+max_id;
    }

    InstagraphRequest *getSavedFeedRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = getSavedFeedRequest->generateSignature(data);
    getSavedFeedRequest->request(target,signature.toUtf8());
    QObject::connect(getSavedFeedRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(getSavedFeedDataReady(QVariant)));
}

void Instagraph::changeProfilePicture(const QString &path)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QFile image(path);
    image.open(QIODevice::ReadOnly);
    QByteArray dataStream = image.readAll();

    QString boundary = this->m_uuid;

    /*Body build*/
    QString body = QStringLiteral("");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"_uuid\"\r\n\r\n");
    body += this->m_uuid.replace(QStringLiteral("{"),QStringLiteral("")).replace(QStringLiteral("}"),QStringLiteral(""))+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"_csrftoken\"\r\n\r\n");
    body += this->m_token+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"_uid\"\r\n\r\n");
    body += this->m_username_id+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"profile_pic\"; filename=\"profile_pic\"\r\n");
    body += QStringLiteral("Content-Transfer-Encoding: binary\r\n");
    body += QStringLiteral("Content-Type: application/octet-stream\r\n\r\n");

    body += dataStream+QStringLiteral("\r\n");
    body += QStringLiteral("--")+boundary+QStringLiteral("--");

    InstagraphRequest *putPhotoReqest = new InstagraphRequest();
    putPhotoReqest->fileRquest(QStringLiteral("accounts/change_profile_picture/"),boundary, body.toUtf8());
    QObject::connect(putPhotoReqest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(profilePictureChanged(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::removeProfilePicture()
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *removeProfilePictureRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = removeProfilePictureRequest->generateSignature(data);
    removeProfilePictureRequest->request(QStringLiteral("accounts/remove_profile_picture/"),signature.toUtf8());
    QObject::connect(removeProfilePictureRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(profilePictureDeleted(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::setPrivateAccount()
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *setPrivateRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = setPrivateRequest->generateSignature(data);
    setPrivateRequest->request(QStringLiteral("accounts/set_private/"),signature.toUtf8());
    QObject::connect(setPrivateRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(setProfilePrivate(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::setPublicAccount()
{
    InstagraphRequest *setPublicRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = setPublicRequest->generateSignature(data);
    setPublicRequest->request(QStringLiteral("accounts/set_public/"),signature.toUtf8());
    QObject::connect(setPublicRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(setProfilePublic(QVariant)));
}

void Instagraph::getProfileData()
{
    InstagraphRequest *getProfileRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

    QString signature = getProfileRequest->generateSignature(data);
    getProfileRequest->request(QStringLiteral("accounts/current_user/?edit=true"),signature.toUtf8());
    QObject::connect(getProfileRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(profileDataReady(QVariant)));
}
/**
 * Edit profile.
 *
 * @param QString url
 *   Url - website. QStringLiteral("") for nothing
 * @param QString phone
 *   Phone number. QStringLiteral("") for nothing
 * @param QString first_name
 *   Name. QStringLiteral("") for nothing
 * @param QString email
 *   Email. Required.
 * @param bool gender
 *   Gender. male = true , female = false
 */
void Instagraph::editProfile(const QString &url, const QString &phone, const QString &first_name, const QString &biography, const QString &email, bool gender)
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *editProfileRequest = new InstagraphRequest();
    QString gen_string;
    if(gender)
    {
        gen_string = QStringLiteral("1");
    }
    else
    {
        gen_string = QStringLiteral("0");
    }

    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("external_url"), url);
        data.insert(QStringLiteral("phone_number"), phone);
        data.insert(QStringLiteral("username"),     this->m_username);
        data.insert(QStringLiteral("first_name"),    first_name);
        data.insert(QStringLiteral("biography"),    biography);
        data.insert(QStringLiteral("email"),        email);
        data.insert(QStringLiteral("gender"),       gen_string);

    QString signature = editProfileRequest->generateSignature(data);
    editProfileRequest->request(QStringLiteral("accounts/edit_profile/"),signature.toUtf8());
    QObject::connect(editProfileRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(editDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getUsernameInfo(const QString &usernameId)
{
    InstagraphRequest *getUsernameRequest = new InstagraphRequest();
    getUsernameRequest->request(QStringLiteral("users/")+usernameId+QStringLiteral("/info/"),NULL);
    QObject::connect(getUsernameRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(usernameDataReady(QVariant)));
}

void Instagraph::getRecentActivity()
{
    InstagraphRequest *getRecentActivityRequest = new InstagraphRequest();
    getRecentActivityRequest->request(QStringLiteral("news/inbox/?"),NULL);
    QObject::connect(getRecentActivityRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(recentActivityDataReady(QVariant)));
}

void Instagraph::getFollowingRecentActivity(const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("news/");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("?max_id=")+max_id+QStringLiteral("&");
    }

    InstagraphRequest *getFollowingRecentRequest = new InstagraphRequest();
    getFollowingRecentRequest->request(target,NULL);
    QObject::connect(getFollowingRecentRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(followingRecentDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getUserTags(const QString &usernameId)
{
    InstagraphRequest *getUserTagsRequest = new InstagraphRequest();
    getUserTagsRequest->request(QStringLiteral("usertags/")+usernameId+QStringLiteral("/feed/?rank_token=")+this->m_rank_token+QStringLiteral("&ranked_content=true&"),NULL);
    QObject::connect(getUserTagsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(userTagsDataReady(QVariant)));
}

void Instagraph::getGeoMedia(const QString &usernameId)
{
    InstagraphRequest *getGeoMediaRequest = new InstagraphRequest();
    getGeoMediaRequest->request(QStringLiteral("maps/user/")+usernameId+QStringLiteral("/"),NULL);
    QObject::connect(getGeoMediaRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(geoMediaDataReady(QVariant)));
}

void Instagraph::tagFeed(const QString &tag, const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("feed/tag/")+tag+QStringLiteral("/?rank_token=")+this->m_rank_token+QStringLiteral("&ranked_content=true&");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("&max_id=")+max_id;
    }

    InstagraphRequest *getTagFeedRequest = new InstagraphRequest();
    getTagFeedRequest->request(target,NULL);
    QObject::connect(getTagFeedRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(tagFeedDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getTimeLine(const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("feed/timeline/?rank_token=")+this->m_rank_token+QStringLiteral("&ranked_content=true&");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("&max_id=")+max_id;
    }

    InstagraphRequest *getTimeLineRequest = new InstagraphRequest();
    getTimeLineRequest->request(target,NULL);
    QObject::connect(getTimeLineRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(timeLineDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getUsernameFeed(const QString &usernameID, const QString &maxid, const QString &minTimestamp)
{
    QString endpoint;
    endpoint = QStringLiteral("feed/user/")+usernameID+QStringLiteral("/?rank_token=")+this->m_rank_token;
    if(maxid.length() > 0)
    {
        endpoint += QStringLiteral("&max_id=")+maxid;
    }
    if(minTimestamp.length() > 0)
    {
        endpoint += QStringLiteral("&min_timestamp=")+minTimestamp;
    }
    endpoint += QStringLiteral("&ranked_content=true");

    InstagraphRequest *getUserTimeLineRequest = new InstagraphRequest();
    getUserTimeLineRequest->request(endpoint,NULL);
    QObject::connect(getUserTimeLineRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(userTimeLineDataReady(QVariant)));
}

void Instagraph::getPopularFeed(const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("feed/popular/?people_teaser_supported=1&rank_token=")+this->m_rank_token+QStringLiteral("&ranked_content=true&");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("&max_id=")+max_id;
    }

    InstagraphRequest *getPopularFeedRequest = new InstagraphRequest();
    getPopularFeedRequest->request(target,NULL);
    QObject::connect(getPopularFeedRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(popularFeedDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getMediaLikers(const QString &mediaId)
{
    InstagraphRequest *getMediaLikersRequest = new InstagraphRequest();
    getMediaLikersRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/likers/?"),NULL);
    QObject::connect(getMediaLikersRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(mediaLikersDataReady(QVariant)));
}

void Instagraph::like(const QString &mediaId)
{
    InstagraphRequest *likeRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("media_id"),     mediaId);

    QString signature = likeRequest->generateSignature(data);
    likeRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/like/"),signature.toUtf8());
    QObject::connect(likeRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(likeDataReady(QVariant)));
}

void Instagraph::unLike(const QString &mediaId)
{
    InstagraphRequest *unLikeRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("media_id"),     mediaId);

    QString signature = unLikeRequest->generateSignature(data);
    unLikeRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/unlike/"),signature.toUtf8());
    QObject::connect(unLikeRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(unLikeDataReady(QVariant)));
}

void Instagraph::getMediaComments(const QString &mediaId)
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *getMediaCommentsRequest = new InstagraphRequest();
    getMediaCommentsRequest->request(QStringLiteral("media/")+mediaId+QStringLiteral("/comments/?"),NULL);
    QObject::connect(getMediaCommentsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(mediaCommentsDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::follow(const QString &userId)
{
    InstagraphRequest *followRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("user_id"),     userId);

    QString signature = followRequest->generateSignature(data);
    followRequest->request(QStringLiteral("friendships/create/")+userId+QStringLiteral("/"),signature.toUtf8());
    QObject::connect(followRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(followDataReady(QVariant)));
}

void Instagraph::unFollow(const QString &userId)
{
    InstagraphRequest *unFollowRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("user_id"),     userId);

    QString signature = unFollowRequest->generateSignature(data);
    unFollowRequest->request(QStringLiteral("friendships/destroy/")+userId+QStringLiteral("/"),signature.toUtf8());
    QObject::connect(unFollowRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(unFollowDataReady(QVariant)));
}

void Instagraph::block(const QString &userId)
{
    InstagraphRequest *blockRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("user_id"),     userId);

    QString signature = blockRequest->generateSignature(data);
    blockRequest->request(QStringLiteral("friendships/block/")+userId+QStringLiteral("/"),signature.toUtf8());
    QObject::connect(blockRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(blockDataReady(QVariant)));
}

void Instagraph::unBlock(const QString &userId)
{
    InstagraphRequest *unBlockRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("user_id"),     userId);

    QString signature = unBlockRequest->generateSignature(data);
    unBlockRequest->request(QStringLiteral("friendships/unblock/")+userId+QStringLiteral("/"),signature.toUtf8());
    QObject::connect(unBlockRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(unBlockDataReady(QVariant)));
}

void Instagraph::userFriendship(const QString &userId)
{
    InstagraphRequest *userFriendshipRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("user_id"),     userId);

    QString signature = userFriendshipRequest->generateSignature(data);
    userFriendshipRequest->request(QStringLiteral("friendships/show/")+userId+QStringLiteral("/"),signature.toUtf8());
    QObject::connect(userFriendshipRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(userFriendshipDataReady(QVariant)));
}

void Instagraph::pendingFriendships()
{
    InstagraphRequest *pendingFriendshipsRequest = new InstagraphRequest();
    pendingFriendshipsRequest->request(QStringLiteral("friendships/pending/"),NULL);
    QObject::connect(pendingFriendshipsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(pendingFriendshipsDataReady(QVariant)));
}

void Instagraph::approveFriendship(const QString &userId)
{
    InstagraphRequest *approveFriendshipRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("user_id"),      userId);
        data.insert(QStringLiteral("radio_type"),   QStringLiteral("wifi-none"));

    QString signature = approveFriendshipRequest->generateSignature(data);
    approveFriendshipRequest->request(QStringLiteral("friendships/approve/")+userId+QStringLiteral("/"),signature.toUtf8());
    QObject::connect(approveFriendshipRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(approveFriendshipDataReady(QVariant)));
}

void Instagraph::rejectFriendship(const QString &userId)
{
    InstagraphRequest *rejectFriendshipRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("user_id"),      userId);
        data.insert(QStringLiteral("radio_type"),   QStringLiteral("wifi-none"));

    QString signature = rejectFriendshipRequest->generateSignature(data);
    rejectFriendshipRequest->request(QStringLiteral("friendships/ignore/")+userId+QStringLiteral("/"),signature.toUtf8());
    QObject::connect(rejectFriendshipRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(rejectFriendshipDataReady(QVariant)));
}

void Instagraph::getLikedMedia(const QString &max_id)
{
    QString target =QStringLiteral("feed/liked/");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("?max_id=")+max_id;
    }

    InstagraphRequest *getLikedMediaRequest = new InstagraphRequest();
    getLikedMediaRequest->request(target,NULL);
    QObject::connect(getLikedMediaRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(likedMediaDataReady(QVariant)));
}

/*
 * Return json string
 * {
 *   QStringLiteral("username"):    STRING  Checking username,
 *   QStringLiteral("available"):   BOOL    Aviable to registration,
 *   QStringLiteral("status"):      STRING  Status of request,
 *   QStringLiteral("error"):       STRING  Error string if aviable
 *   }
 */
void Instagraph::checkUsername(const QString &username)
{
    InstagraphRequest *checkUsernameRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_csrftoken"),   QString(QStringLiteral("missing")));
        data.insert(QStringLiteral("username"),     username);

    QString signature = checkUsernameRequest->generateSignature(data);
    checkUsernameRequest->request(QStringLiteral("users/check_username/"),signature.toUtf8());
    QObject::connect(checkUsernameRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(usernameCheckDataReady(QVariant)));
}
/*
 * Return JSON string
 * {
 *  QStringLiteral("status"): STRING    Status of request,
 *  QStringLiteral("errors"):{
 *            ARRAY     Array of errors if aviable
 *      QStringLiteral("password"):[],  STRING  Error message if password wrong if aviable
 *      QStringLiteral("email"):[],     STRING  Error message if email wrong if aviable
 *      QStringLiteral("FIELD_ID"):[]   STRING  Error message if FIELD_ID wrong if aviable
 *  },
 *  QStringLiteral("account_created"),  BOOL Status of creation account
 *  QStringLiteral("created_user")      ARRAY Array of new user params
 *  }
 *
 */
void Instagraph::createAccount(const QString &username, const QString &password, const QString &email)
{
    InstagraphRequest *createAccountRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),               this->m_uuid);
        data.insert(QStringLiteral("_csrftoken"),          QString(QStringLiteral("missing")));
        data.insert(QStringLiteral("username"),            username);
        data.insert(QStringLiteral("first_name"),          QString(QStringLiteral("")));
        data.insert(QStringLiteral("guid"),                this->m_uuid);
        data.insert(QStringLiteral("device_id"),           this->m_device_id);
        data.insert(QStringLiteral("email"),               email);
        data.insert(QStringLiteral("force_sign_up_code"),  QString(QStringLiteral("")));
        data.insert(QStringLiteral("qs_stamp"),            QString(QStringLiteral("")));
        data.insert(QStringLiteral("password"),            password);

    QString signature = createAccountRequest->generateSignature(data);
    createAccountRequest->request(QStringLiteral("accounts/create/"),signature.toUtf8());
    QObject::connect(createAccountRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(createAccountDataReady(QVariant)));
}

void Instagraph::searchUsername(const QString &username)
{
    InstagraphRequest *searchUsernameRequest = new InstagraphRequest();
    searchUsernameRequest->request(QStringLiteral("users/")+username+QStringLiteral("/usernameinfo/"), NULL);
    QObject::connect(searchUsernameRequest,SIGNAL(replySrtingReady(QVariant)), this, SIGNAL(searchUsernameDataReady(QVariant)));
}

void Instagraph::searchUsers(const QString &query)
{
    InstagraphRequest *searchUsersRequest = new InstagraphRequest();
    searchUsersRequest->request(QStringLiteral("users/search/?ig_sig_key_version=4&is_typeahead=true&query=")+query+QStringLiteral("&rank_token=")+this->m_rank_token, NULL);
    QObject::connect(searchUsersRequest,SIGNAL(replySrtingReady(QVariant)), this, SIGNAL(searchUsersDataReady(QVariant)));
}

void Instagraph::searchTags(const QString &query)
{
    InstagraphRequest *searchTagsRequest = new InstagraphRequest();
    searchTagsRequest->request(QStringLiteral("tags/search/?is_typeahead=true&q=")+query+QStringLiteral("&rank_token=")+this->m_rank_token, NULL);
    QObject::connect(searchTagsRequest,SIGNAL(replySrtingReady(QVariant)), this, SIGNAL(searchTagsDataReady(QVariant)));
}

void Instagraph::searchFBLocation(const QString &query)
{
    InstagraphRequest *searchFBLocationRequest = new InstagraphRequest();
    searchFBLocationRequest->request(QStringLiteral("fbsearch/places/?query=")+query+QStringLiteral("&rank_token=")+this->m_rank_token, NULL);
    QObject::connect(searchFBLocationRequest,SIGNAL(replySrtingReady(QVariant)), this, SIGNAL(searchFBLocationDataReady(QVariant)));
}

void Instagraph::getLocationFeed(const QString &locationId, const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("feed/location/")+locationId+QStringLiteral("/");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("?max_id=")+max_id;
    }

    InstagraphRequest *getLocationFeedRequest = new InstagraphRequest();
    getLocationFeedRequest->request(target,NULL);
    QObject::connect(getLocationFeedRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(getLocationFeedDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::searchLocation(const QString &latitude, const QString &longitude, const QString &query)
{
    QString target = QStringLiteral("location_search/?rank_token=")+this->m_rank_token+QStringLiteral("&latitude=")+latitude+QStringLiteral("&longitude=")+longitude;

    if(query.length() > 0)
    {
        target += QStringLiteral("&search_query=")+query;
    }
    else
    {
        target += QStringLiteral("&timestamp=")+QString::number(QDateTime::currentMSecsSinceEpoch());
    }

    InstagraphRequest *searchLocationRequest = new InstagraphRequest();
    searchLocationRequest->request(target, NULL);
    QObject::connect(searchLocationRequest,SIGNAL(replySrtingReady(QVariant)), this, SIGNAL(searchLocationDataReady(QVariant)));
}

void Instagraph::getv2Inbox(const QString &cursor_id)
{
    QString target =QStringLiteral("direct_v2/inbox/?use_unified_inbox=true");

    if(cursor_id.length() > 0)
    {
        target += QStringLiteral("&cursor=")+cursor_id;
    }

    InstagraphRequest *getv2InboxRequest = new InstagraphRequest();
    getv2InboxRequest->request(target,NULL);
    QObject::connect(getv2InboxRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(v2InboxDataReady(QVariant)));
}

void Instagraph::directThread(const QString &threadId, const QString &cursor_id)
{
    QString target =QStringLiteral("direct_v2/threads/")+threadId+QStringLiteral("/?use_unified_inbox=true");

    if(cursor_id.length() > 0)
    {
        target += QStringLiteral("&cursor=")+cursor_id;
    }

    InstagraphRequest *directThreadRequest = new InstagraphRequest();
    directThreadRequest->request(target,NULL);

    QObject::connect(directThreadRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(directThreadReady(QVariant)));
}

void Instagraph::markDirectThreadItemSeen(const QString &threadId, const QString &threadItemId)
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *markDirectThreadItemSeenRequest = new InstagraphRequest();

    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("use_unified_inbox"), QStringLiteral("true"));
        data.insert(QStringLiteral("action"),       QStringLiteral("mark_seen"));
        data.insert(QStringLiteral("thread_id"),    threadId);
        data.insert(QStringLiteral("item_id"),      threadItemId);

    QString signature = markDirectThreadItemSeenRequest->generateSignature(data);
    markDirectThreadItemSeenRequest->request(QStringLiteral("direct_v2/threads/")+threadId+QStringLiteral("/items/")+threadItemId+QStringLiteral("/seen/"),signature.toUtf8());
    QObject::connect(markDirectThreadItemSeenRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(markDirectThreadItemSeenReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::directShare(const QString &mediaId, const QString &recipients, const QString &text)
{
    m_busy = true;
    Q_EMIT busyChanged();

    //QString recipient_users = QStringLiteral("\"")+recipients+QStringLiteral("\"");

    QString boundary = this->m_uuid;

    /*Body build*/
    QString body = QStringLiteral("");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"media_id\"\r\n\r\n");
    body += mediaId+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"recipient_users\"\r\n\r\n");
    body += QStringLiteral("[[")+recipients+QStringLiteral("]]\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"client_context\"\r\n\r\n");
    body += this->m_uuid.replace(QStringLiteral("{"),QStringLiteral("")).replace(QStringLiteral("}"),QStringLiteral(""))+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"text\"\r\n\r\n");
    body += text+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("--");

    InstagraphRequest *directMessageShare = new InstagraphRequest();
    directMessageShare->directRquest(QStringLiteral("direct_v2/threads/broadcast/media_share/?media_type=photo"),boundary, body.toUtf8());
    QObject::connect(directMessageShare,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(directShareReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::directMessage(const QString &recipients, const QString &text, const QString &thread_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString boundary = this->m_uuid;

    QUuid uuid;

    /*Body build*/
    QString body = QStringLiteral("");
    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"recipient_users\"\r\n\r\n");
    body += QStringLiteral("[[")+recipients+QStringLiteral("]]\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"client_context\"\r\n\r\n");
    body += uuid.createUuid().toString().replace(QStringLiteral("{"),QStringLiteral("")).replace(QStringLiteral("}"),QStringLiteral(""))+QStringLiteral("\r\n");

    if (thread_id != QStringLiteral("")) {
        body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
        body += QStringLiteral("Content-Disposition: form-data; name=\"thread_ids\"\r\n\r\n");
        body += QStringLiteral("[\"")+thread_id+QStringLiteral("\"]\r\n");
    }

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"text\"\r\n\r\n");
    body += text+QStringLiteral("\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("--");

    InstagraphRequest *directMessageRequest = new InstagraphRequest();
    directMessageRequest->directRquest(QStringLiteral("direct_v2/threads/broadcast/text/"),boundary, body.toUtf8());
    QObject::connect(directMessageRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(directMessageReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::directLike(const QString &recipients, const QString &thread_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString boundary = this->m_uuid;

    QUuid uuid;

    /*Body build*/
    QString body = QStringLiteral("");
    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"recipient_users\"\r\n\r\n");
    body += QStringLiteral("[[")+recipients+QStringLiteral("]]\r\n");

    body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
    body += QStringLiteral("Content-Disposition: form-data; name=\"client_context\"\r\n\r\n");
    body += uuid.createUuid().toString().replace(QStringLiteral("{"),QStringLiteral("")).replace(QStringLiteral("}"),QStringLiteral(""))+QStringLiteral("\r\n");

    if (thread_id != QStringLiteral("")) {
        body += QStringLiteral("--")+boundary+QStringLiteral("\r\n");
        body += QStringLiteral("Content-Disposition: form-data; name=\"thread_ids\"\r\n\r\n");
        body += QStringLiteral("[\"")+thread_id+QStringLiteral("\"]\r\n");
    }

    body += QStringLiteral("--")+boundary+QStringLiteral("--");

    InstagraphRequest *directLikeRequest = new InstagraphRequest();
    directLikeRequest->directRquest(QStringLiteral("direct_v2/threads/broadcast/like/"),boundary, body.toUtf8());
    QObject::connect(directLikeRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(directLikeReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::changePassword(const QString &oldPassword, const QString &newPassword)
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *changePasswordRequest = new InstagraphRequest();

    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("old_password"), oldPassword);
        data.insert(QStringLiteral("new_password1"), newPassword);
        data.insert(QStringLiteral("new_password2"), newPassword);

    QString signature = changePasswordRequest->generateSignature(data);
    changePasswordRequest->request(QStringLiteral("accounts/change_password/"),signature.toUtf8());
    QObject::connect(changePasswordRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(changePasswordReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::explore(const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("discover/explore/?");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("&max_id=")+max_id;
    }

    InstagraphRequest *exploreRequest = new InstagraphRequest();
    exploreRequest->request(target,NULL);
    QObject::connect(exploreRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(exploreDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::suggestions()
{
    m_busy = true;
    Q_EMIT busyChanged();

    InstagraphRequest *suggestionsRequest = new InstagraphRequest();

    QUuid uuid;
    QJsonObject data;
        data.insert(QStringLiteral("phone_id"), uuid.createUuid().toString());
        data.insert(QStringLiteral("_csrftoken"), QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));
        data.insert(QStringLiteral("module"), QStringLiteral("explore_people"));
        data.insert(QStringLiteral("_uuid"), this->m_uuid);
        data.insert(QStringLiteral("paginate"), QStringLiteral("true"));
        data.insert(QStringLiteral("num_media"), QStringLiteral("3"));

    QString signature = suggestionsRequest->generateSignature(data);
    suggestionsRequest->request(QStringLiteral("discover/ayml/"),signature.toUtf8());
    QObject::connect(suggestionsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(suggestionsDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getRankedRecipients(const QString &query)
{
    QString target = QStringLiteral("direct_v2/ranked_recipients/?mode=raven&show_threads=true&use_unified_inbox=false&");

    if(query.length() > 0)
    {
        target += QStringLiteral("&query=")+query;
    }
    else
    {
        target += QStringLiteral("&");
    }

    InstagraphRequest *getRankedRecipientsRequest = new InstagraphRequest();
    getRankedRecipientsRequest->request(target, NULL);
    QObject::connect(getRankedRecipientsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(rankedRecipientsDataReady(QVariant)));
}

void Instagraph::getRecentRecipients()
{
    InstagraphRequest *getRecentRecipientsRequest = new InstagraphRequest();
    getRecentRecipientsRequest->request(QStringLiteral("direct_share/recent_recipients/"),NULL);
    QObject::connect(getRecentRecipientsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(recentRecipientsDataReady(QVariant)));
}

void Instagraph::getUserFollowings(const QString &usernameId, const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("friendships/")+usernameId+QStringLiteral("/following/?rank_token=")+this->m_rank_token+QStringLiteral("&ig_sig_key_version=4&");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("&max_id=")+max_id;
    }

    InstagraphRequest *getUserFollowingsRequest = new InstagraphRequest();
    getUserFollowingsRequest->request(target,NULL);
    QObject::connect(getUserFollowingsRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(userFollowingsDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getUserFollowers(const QString &usernameId, const QString &max_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("friendships/")+usernameId+QStringLiteral("/followers/?rank_token=")+this->m_rank_token+QStringLiteral("&ig_sig_key_version=4&");

    if(max_id.length() > 0)
    {
        target += QStringLiteral("&max_id=")+max_id;
    }

    InstagraphRequest *getUserFollowersRequest = new InstagraphRequest();
    getUserFollowersRequest->request(target,NULL);
    QObject::connect(getUserFollowersRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(userFollowersDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getUserBlockedList()
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("users/blocked_list/");

    InstagraphRequest *getUserBlockedListRequest = new InstagraphRequest();
    getUserBlockedListRequest->request(target,NULL);
    QObject::connect(getUserBlockedListRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(userBlockedListDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getReelsTrayFeed()
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target =QStringLiteral("feed/reels_tray/");

    InstagraphRequest *getReelsTrayFeedRequest = new InstagraphRequest();
    getReelsTrayFeedRequest->request(target,NULL);
    QObject::connect(getReelsTrayFeedRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(reelsTrayFeedDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::getUserReelsMediaFeed(const QString &user_id)
{
    m_busy = true;
    Q_EMIT busyChanged();

    QString target = QStringLiteral("feed/user/")+user_id+QStringLiteral("/reel_media/");

    InstagraphRequest *getUserReelsMediaFeedRequest = new InstagraphRequest();
    getUserReelsMediaFeedRequest->request(target,NULL);
    QObject::connect(getUserReelsMediaFeedRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(userReelsMediaFeedDataReady(QVariant)));

    m_busy = false;
    Q_EMIT busyChanged();
}

void Instagraph::markStoryMediaSeen(const QString &reels)
{
    InstagraphRequest *markStoryMediaSeenRequest = new InstagraphRequest();
    QJsonObject data;
        data.insert(QStringLiteral("_uuid"),        this->m_uuid);
        data.insert(QStringLiteral("_uid"),         this->m_username_id);
        data.insert(QStringLiteral("_csrftoken"),   QJsonValue(QStringLiteral("Set-Cookie: csrftoken=")+this->m_token));

        QJsonDocument jDoc = QJsonDocument::fromJson(reels.toLatin1());
        data.insert(QStringLiteral("reels"),        jDoc.object());

        QJsonObject live_vods;
        data.insert(QStringLiteral("live_vods"),    live_vods);

    QString signature = markStoryMediaSeenRequest->generateSignature(data);
    markStoryMediaSeenRequest->request(QStringLiteral("media/seen/?reel=1&live_vod=0"),signature.toUtf8(),true);
    QObject::connect(markStoryMediaSeenRequest,SIGNAL(replySrtingReady(QVariant)),this,SIGNAL(markStoryMediaSeenDataReady(QVariant)));
}

// Camera
void Instagraph::rotateImg(const QString &filename, qreal deg)
{
    QImage image(filename);
    QTransform rot;
    rot.rotate(deg);
    image = image.transformed(rot);

    QFile imgFile(filename);
    imgFile.open(QIODevice::ReadWrite);

    if(!image.save(&imgFile,"JPG",100))
    {
        qDebug() << QStringLiteral("NOT SAVE");
    } else {
        Q_EMIT imgRotated();
    }

    imgFile.close();
}

void Instagraph::squareImg(const QString &filename)
{
    QImage image(filename);

    QFile imgFile(filename);
    imgFile.open(QIODevice::ReadWrite);

    if (image.height() > image.width()) {
        image = image.copy(0, (image.height()-image.width())/2, image.width(), image.width());

        if(!image.save(&imgFile,"JPG",100))
        {
            qDebug() << QStringLiteral("NOT SAVE ON CROP");
        } else {
            Q_EMIT imgSquared();
        }
    } else if (image.height() < image.width()) {
        image = image.copy((image.width()-image.height())/2, 0, image.height(), image.height());

        if(!image.save(&imgFile,"JPG",100))
        {
            qDebug() << QStringLiteral("NOT SAVE ON CROP");
        } else {
            Q_EMIT imgSquared();
        }
    }

    imgFile.close();
}

void Instagraph::cropImg(const QString &filename, qreal propos)
{
    QImage image(filename);
    image = image.copy(0, image.height()*propos, image.width(), image.width());

    QFile imgFile(filename);
    imgFile.open(QIODevice::ReadWrite);

    if(!image.save(&imgFile,"JPG",100))
    {
        qDebug() << QStringLiteral("NOT SAVE ON CROP");
    } else {
        Q_EMIT imgCropped();
    }

    imgFile.close();
}

void Instagraph::scaleImg(const QString &filename)
{
    QImage image(filename);

    QFile imgFile(filename);
    imgFile.open(QIODevice::ReadWrite);

    if (image.width() > 800) {
        int w_s = image.width()/800;

        int s_w = image.width()/w_s;
        int s_h = image.height()/w_s;

        image = image.scaled(s_w, s_h, Qt::KeepAspectRatio);

        if(!image.save(&imgFile,"JPG",100))
        {
            qDebug() << QStringLiteral("NOT SAVE ON CROP");
        } else {
            Q_EMIT imgScaled();
        }
    } else {
        Q_EMIT imgScaled();
    }

    imgFile.close();
}
