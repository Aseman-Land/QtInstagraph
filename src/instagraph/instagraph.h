#ifndef INSTAGRAPH_H
#define INSTAGRAPH_H

#include <QObject>
#include <QDir>
#include <QVariant>

#include "libqinstagraph_global.h"

class LIBQINSTAGRAPHSHARED_EXPORT Instagraph : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(const QString &error READ error NOTIFY errorChanged)
    Q_PROPERTY(const QString &photos_path READ photos_path NOTIFY photos_pathChanged)
public:
    explicit Instagraph(QObject *parent = 0);

    bool busy() const;
    QString error() const;
    QString photos_path() const;

public Q_SLOTS:
    void login(bool forse = false);
    void logout();

    void setUsername(const QString &username){this->m_username = username;}
    void setPassword(const QString &password){this->m_password = password;}

    QString getUsernameId(){return this->m_username_id;}

    void postImage(const QString &path, const QString &caption, const QVariantMap &location, QString upload_id = QStringLiteral(""));
    void postVideo(QFile *video);

    void infoMedia(const QString &mediaId);
    void editMedia(const QString &mediaId, const QString &captionText = QStringLiteral(""));
    void deleteMedia(const QString &mediaId);
    void removeSelftag(const QString &mediaId);

    void enableMediaComments(const QString &mediaId);
    void disableMediaComments(const QString &mediaId);

    void postComment(const QString &mediaId, const QString &commentText);
    void deleteComment(const QString &mediaId, const QString &commentId);
    void likeComment(const QString &commentId);
    void unLikeComment(const QString &commentId);

    void saveMedia(const QString &mediaId);
    void unsaveMedia(const QString &mediaId);
    void getSavedFeed(const QString &max_id = QStringLiteral(""));

    void setPrivateAccount();
    void setPublicAccount();
    void changeProfilePicture(const QString &path);
    void removeProfilePicture();
    void getProfileData();
    void editProfile(const QString &url, const QString &phone, const QString &first_name, const QString &biography, const QString &email, bool gender);
    void getUsernameInfo(const QString &usernameId);

    void getRecentActivity();
    void getFollowingRecentActivity(const QString &max_id = QStringLiteral(""));

    void getUserTags(const QString &usernameId);
    void getGeoMedia(const QString &usernameId);
    void tagFeed(const QString &tag, const QString &max_id = QStringLiteral(""));
    void getTimeLine(const QString &max_id = QStringLiteral(""));
    void getUsernameFeed(const QString &usernameID, const QString &maxid = QStringLiteral(""), const QString &minTimestamp = QStringLiteral(""));
    void getPopularFeed(const QString &max_id = QStringLiteral(""));

    void getMediaLikers(const QString &mediaId);
    void getMediaComments(const QString &mediaId);

    void like(const QString &mediaId);
    void unLike(const QString &mediaId);

    void follow(const QString &userId);
    void unFollow(const QString &userId);
    void block(const QString &userId);
    void unBlock(const QString &userId);
    void userFriendship(const QString &userId);
    void pendingFriendships();
    void approveFriendship(const QString &userId);
    void rejectFriendship(const QString &userId);

    void getLikedMedia(const QString &max_id = QStringLiteral(""));

    void checkUsername(const QString &username);
    void createAccount(const QString &username, const QString &password, const QString &email);

    void searchUsername(const QString &username);

    void searchUsers(const QString &query);
    void searchTags(const QString &query);
    void searchFBLocation(const QString &query);
    void getLocationFeed(const QString &locationId, const QString &max_id = QStringLiteral(""));
    void searchLocation(const QString &latitude, const QString &longitude, const QString &query = QStringLiteral(""));

    void getv2Inbox(const QString &cursor_id = QStringLiteral(""));
    void directThread(const QString &threadId, const QString &cursor_id = QStringLiteral(""));
    void markDirectThreadItemSeen(const QString &threadId, const QString &threadItemId);
    void directMessage(const QString &recipients, const QString &text, const QString &thread_id = QStringLiteral("0"));
    void directLike(const QString &recipients, const QString &thread_id = QStringLiteral("0"));
    void directShare(const QString &mediaId, const QString &recipients, const QString &text = QStringLiteral(""));

    void changePassword(const QString &oldPassword, const QString &newPassword);

    void explore(const QString &max_id = QStringLiteral(""));
    void suggestions();

    void getRankedRecipients(const QString &query = QStringLiteral(""));
    void getRecentRecipients();

    void getUserFollowings(const QString &usernameId, const QString &max_id = QStringLiteral(""));
    void getUserFollowers(const QString &usernameId, const QString &max_id = QStringLiteral(""));
    void getUserBlockedList();

    void getReelsTrayFeed();
    void getUserReelsMediaFeed(const QString &user_id = QStringLiteral(""));
    void markStoryMediaSeen(const QString &reels);

    void rotateImg(const QString &filename, qreal deg);
    void squareImg(const QString &filename);
    void cropImg(const QString &filename, qreal propos);
    void scaleImg(const QString &filename);

    static QString mediaShortcodeToMediaID(const QString &shortcode);

private:
    QString EXPERIMENTS     = QStringLiteral(INSTAGRAPH_EXPERIMENTAL_STR);
    QString LOGIN_EXPERIMENTS   = QStringLiteral(INSTAGRAPH_LOGIN_EXPERIMENTAL_STR);

    QString m_username;
    QString m_password;
    QString m_debug;
    QString m_username_id;
    QString m_uuid;
    QString m_device_id;
    QString m_token;
    QString m_rank_token;
    QString m_IGDataPath;

    QString m_caption;
    QString m_image_path;

    QDir m_data_path;

    QDir m_photos_path;

    bool m_isLoggedIn = false;

    QString generateDeviceId();

    bool m_busy;
    QString m_error;

    QVariantMap lastUploadLocation;

Q_SIGNALS:
    void profileConnected(const QVariant &answer);
    void profileConnectedFail();

    void autoCompleteUserListReady(const QVariant &answer);

    void mediaInfoReady(const QVariant &answer);
    void mediaEdited(const QVariant &answer);
    void mediaDeleted(const QVariant &answer);

    void enableMediaCommentsReady(const QVariant &answer);
    void disableMediaCommentsReady(const QVariant &answer);

    void imageConfigureDataReady(const QVariant &answer);

    void removeSelftagDone(const QVariant &answer);
    void commentPosted(const QVariant &answer);
    void commentDeleted(const QVariant &answer);
    void commentLiked(const QVariant &answer);
    void commentUnLiked(const QVariant &answer);

    void profilePictureChanged(const QVariant &answer);
    void profilePictureDeleted(const QVariant &answer);
    void setProfilePrivate(const QVariant &answer);
    void setProfilePublic(const QVariant &answer);
    void profileDataReady(const QVariant &answer);
    void editDataReady(const QVariant &answer);
    void usernameDataReady(const QVariant &answer);

    void recentActivityDataReady(const QVariant &answer);
    void followingRecentDataReady(const QVariant &answer);

    void userTagsDataReady(const QVariant &answer);
    void geoMediaDataReady(const QVariant &answer);
    void tagFeedDataReady(const QVariant &answer);
    void timeLineDataReady(const QVariant &answer);
    void userTimeLineDataReady(const QVariant &answer);
    void popularFeedDataReady(const QVariant &answer);

    void mediaLikersDataReady(const QVariant &answer);
    void mediaCommentsDataReady(const QVariant &answer);
    void likeDataReady(const QVariant &answer);
    void unLikeDataReady(const QVariant &answer);

    void followDataReady(const QVariant &answer);
    void unFollowDataReady(const QVariant &answer);
    void blockDataReady(const QVariant &answer);
    void unBlockDataReady(const QVariant &answer);
    void userFriendshipDataReady(const QVariant &answer);
    void pendingFriendshipsDataReady(const QVariant &answer);
    void approveFriendshipDataReady(const QVariant &answer);
    void rejectFriendshipDataReady(const QVariant &answer);

    void likedMediaDataReady(const QVariant &answer);

    void doLogout(const QVariant &answer);

    void usernameCheckDataReady(const QVariant &answer);
    void createAccountDataReady(const QVariant &answer);

    void error(const QString &message);

    void searchUsernameDataReady(const QVariant &answer);

    void searchUsersDataReady(const QVariant &answer);
    void searchTagsDataReady(const QVariant &answer);
    void searchFBLocationDataReady(const QVariant &answer);
    void getLocationFeedDataReady(const QVariant &answer);
    void searchLocationDataReady(const QVariant &answer);

    void v2InboxDataReady(const QVariant &answer);
    void directThreadReady(const QVariant &answer);
    void markDirectThreadItemSeenReady(const QVariant &answer);
    void directMessageReady(const QVariant &answer);
    void directLikeReady(const QVariant &answer);
    void directShareReady(const QVariant &answer);

    void changePasswordReady(const QVariant &answer);

    void exploreDataReady(const QVariant &answer);
    void suggestionsDataReady(const QVariant &answer);

    void rankedRecipientsDataReady(const QVariant &answer);
    void recentRecipientsDataReady(const QVariant &answer);

    void saveMediaDataReady(const QVariant &answer);
    void unsaveMediaDataReady(const QVariant &answer);
    void getSavedFeedDataReady(const QVariant &answer);

    void userFollowingsDataReady(const QVariant &answer);
    void userFollowersDataReady(const QVariant &answer);
    void userBlockedListDataReady(const QVariant &answer);

    void reelsTrayFeedDataReady(const QVariant &answer);
    void userReelsMediaFeedDataReady(const QVariant &answer);
    void markStoryMediaSeenDataReady(const QVariant &answer);

    void busyChanged();
    void errorChanged();
    void photos_pathChanged();

    void imgSquared();
    void imgRotated();
    void imgCropped();
    void imgScaled();

    void imageUploadProgressDataReady(double answer);

private Q_SLOTS:
    void setUser();
    void doLogin();
    void syncFeatures(bool prelogin = false);
    void autoCompleteUserList();
    void profileConnect(const QVariant &profile);
    void configurePhoto(const QVariant &answer);
};

#endif // INSTAGRAPH_H
 
