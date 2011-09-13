/* Copyright (C) 2010-present, Zhenkai Zhu <zhenkai@cs.ucla.edu>, Yang Xu <yangxu.5661@yahoo.com.cn>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#ifndef _GROUPMANAGER_H
#define _GROUPMANAGER_H




#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/x509.h>
#include <openssl/aes.h>
#include <openssl/hmac.h>
#include <ccn/ccn.h>
#include <ccn/uri.h>
#include <ccn/bloom.h>
#include <ccn/charbuf.h>
#include <ccn/keystore.h>
#include <ccn/signing.h>
#ifdef __cplusplus
}
#endif

#include "murmur_pch.h"
#include "ServerUser.h"
#include "RemoteUser.h"

#include "../MediaProcess/media_pro.h"
#include "aes_util.h"

class GroupManager: public QThread {
    private:
        Q_OBJECT
        Q_DISABLE_COPY(GroupManager)

    public:
        void ndnContext();
        int get_public_key(struct ccn_pkey **pkeyp, const char *, const char *);
		void handleLeave(struct ccn_upcall_info *info);
		void incomingInterest(struct ccn_upcall_info *info);
		void incomingContent(struct ccn_upcall_info *info);

    private:
        int ccn_init();
        int ccn_open();
        int ccn_free();
        int ccn_publish_key(const char *, const char *);

    private:
        struct ccn *ccn;
        struct ccn_closure *join_closure;
        struct ccn_closure *req_closure;
		struct ccn_closure *leave_closure;

    protected:
        bool bRunning;

        void StartThread();
        void StopThread();
    public:
        void run();
        void log(const QString &msg);
		void testBug();
        GroupManager(NdnMediaProcess* pNdnMediaPro);
        ~GroupManager();
        QHash<QString, RemoteUser *> getRemoteUsers() { return qhRemoteUsers; };
        RemoteUser *getRemoteUser(QString user) { return qhRemoteUsers[user]; };
        void setLocalUser(ServerUser *u);
        int addRemoteUser(QString prefix, QString userName);
		void deleteRemoteUser(QString remoteUser);
		void userLeft(RemoteUser *ru);
        int userListtoXml(const char **);
        int parseXmlUserList(const unsigned char *incoming_data, size_t len);
		void sendLeaveInterest();
		QString getFullLocalName() { return (prefix + "/" + userName); }
		void expressEnumInterest(struct ccn_charbuf *interest, QList<QString> &toExclude);

    signals:
        void remoteUserJoin(RemoteUser *);
        void remoteUserLeave(int);

    public slots:
        void enumerate();
		void checkAlive();

    public:
        /* senwang*/
        NdnMediaProcess *pNdnMediaPro;  
		unsigned char conferenceKey[512/8];
        QHash<QString, RemoteUser *> qhRemoteUsers;

    private:
        /* the connection information about the local user
         * this is needed to notify the local user about the
         * status of remote users
         */
        ServerUser *localUser;
		QString prefix;
		QString confName;
		QString userName;
		bool isPrivate;
		QMutex *ruMutex;

        QTimer *enumTimer;
		QTimer *aliveTimer;
        QQueue<int> qqRSesIds;
};
        
#else
class GroupManager;
#endif
