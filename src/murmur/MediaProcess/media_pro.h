/* Copyright (C) 2010-present, Zhenkai Zhu <zhenkai@cs.ucla.edu>, Sen Wang <swangfly@qq.vip.com>

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
#ifndef MEDIA_PRO_H
#define MEDIA_PRO_H

#include "murmur_pch.h"
#include <QTimer>

#ifdef   __cplusplus 
extern "C" { 
#endif 
#include "pthread.h"
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
#ifdef   __cplusplus 
#include <time.h>
#include <stdlib.h>
} 
#endif 
#include "../GroupManager/aes_util.h"

#define INTEREST_LIFETIME 4
#define MAXNONCE 512 
#define PER_PACKET_LEN 20

struct buf_list {
    void *buf;
    size_t len;
    struct buf_list *link;
};

class NDNState;
struct data_buffer {
    struct ccn_closure *callback;
    NDNState *state;
    char direction[5];
    struct buf_list *buflist;
};

enum UserType{REMOTE_USER, LOCAL_USER};

class UserDataBuf:public QObject{
    private:
        Q_OBJECT;
        Q_DISABLE_COPY(UserDataBuf);

    private:   

    public:
    UserDataBuf();
    ~UserDataBuf();
		
    public:

    /*the owner name of this buffer*/
    QString user_name;
    /*data buffer for caching the ongoing/incoming packet*/
    struct data_buffer data_buf;
    /*user type: REMOTE_USER, LOCAL_USER*/
    int user_type;
    /*the flag for identifying whether this user is a newcomer*/
    int interested;
    /* flag to indicate this buf has detached with a user */
    int iNeedDestroy;
	long seq;
};

class NDNState:public QObject{
    private:
        Q_OBJECT;
        Q_DISABLE_COPY(NDNState);

    public:
    /*flag indicating whether the ccn connection is alive*/
    int active;
    /*signal for notifying other module changes of ccn connection*/
    pthread_cond_t changed;
    /*CCN(NDN) connection*/
    struct ccn *ccn;
	struct ccn_charbuf *signed_info;

    public:
    NDNState();


    ~NDNState() {};

    void emitSignal(QString strUserName) {
        emit remoteMediaArrivalSig(strUserName);
    };

    signals:
    void remoteMediaArrivalSig(QString); 
    /* after receiving a media packet of remote users, this signal will be emitted to notify
     * other module to get it.
     */

};

class NdnMediaProcess:public QThread {
    private:
        Q_OBJECT;
        Q_DISABLE_COPY(NdnMediaProcess);

    public: 
    /* the data structure for maintaining ccn API related stuff*/
    NDNState ndnState; 
    
    private:
    /* the remote user map */
    QHash<QString, UserDataBuf *> qhRemoteUser;
    QHash<QString, UserDataBuf *> qhLocalUser;
    
	bool isPrivate;
	unsigned char sessionKey[512/8];
	long localSeq;
	QTimer *clock;
    static int hint_ahead;                                                                  

    private:
    /* cache the packet in buffer waiting for working thread sending*/
    int ndnDataSend(QString strUserName, const void *buf, size_t len);

    /*get the data from the buffer, do not block if there are no data actually*/
    int ndn_wait_message(UserDataBuf *userBuf, char *msg, int msg_len);

    /* check each local user's buffer to find out if there are some packets 
     * to be sent.If have, send it.
     */
    int doPendingSend();

    /* find the new or resetted remote user in the remote user list,send the 
     * first interest for the user.
     */
    int checkInterest();


	private slots:
	void tick();

    public:
    NdnMediaProcess();
    ~NdnMediaProcess();
	static void initPipe(struct ccn_closure *selfp, struct ccn_upcall_info *info, UserDataBuf *userBuf);
	void setPrivate() { isPrivate = true; }
	void setSK(QByteArray sk);
    void run();
    int startThread();
    int stopThread();
    void addRemoteUser(QString strUserName);
    void deleteRemoteUser(QString strUserName);
    void addLocalUser(QString strUserName);
    void deleteLocalUser(QString strUserName);
    /* this function stores the media data of local users to the buffer,then 
     * the caller of this function can return. the work thread is in charge of sending 
     * the data actually.
     */
    int sendLocalMedia(QString &strUserName, char *msg, int msg_len);

    /* get a media packet of some remote user, this function will get a oldest data packet from 
     * the remote user's buffer. the caller will not be blocked, because before calling this 
     * funtion, it must have received the remodeMediaArrivalSig signal
     */  
    int getRemoteMedia(QString strUserName, char *msg, int msg_len);

};
#endif 
