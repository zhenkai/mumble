#!/usr/bin/python
import shutil
import re
import sys

# patch ACL.cpp
subs = "/* zhenkai */\nPermissions def = Enter | Speak;\n"

try:
	f = open('ACL.cpp', 'r')
	o = open('ACL.temp', 'w')
	comment = False
	for line in f:

		if re.search(r"Default permission", line):
			comment = True
			o.write(line)
			continue

		if comment:
			o.write("//" + line)
			o.write(subs)
			comment = False
			print "Patch 1: ACL.cpp patched"
		else: 
			o.write(line)


	f.close()
	o.close()
	shutil.move('ACL.temp', 'ACL.cpp')
except IOError:
	print "IOError"
	sys.exit()

# patch murmur/Message.cpp
subs = "/* yangxu */\npGroupManager->setLocalUser(uSource);\n"
try:
	f = open('murmur/Messages.cpp', 'r')
	o = open('murmur/Messages.temp', 'w')
	for line in f:
		o.write(line)
		if re.search(r"emit userConnected", line):
			o.write(subs)
			print "Patch 2: murmur/Messages.cpp patched"
	f.close()
	o.close()
	shutil.move('murmur/Messages.temp', 'murmur/Messages.cpp')
except IOError:
	print "IOError"
	sys.exit()


# patch murmur/Server.h
headers = '/* yangxu */\n#include "GroupManager/GroupManager.h"\n#include "MediaProcess/media_pro.h"\n'

subs = '''
        /* yangxu */
    private:
        GroupManager *pGroupManager;
        NdnMediaProcess ndnMediaPro;

    public slots:
        // cheat localUsers to treat remotuser
        void newRemoteClient(RemoteUser *);
        void delRemoteClient(int);

        /* senwang*/
    public slots:
        void receiveRemoteData(QString strUserName);
		'''
try:
	f = open('murmur/Server.h', 'r')
	o = open('murmur/Server.h.temp', 'w')
	for line in f:
		o.write(line)
		if re.search(r"DBus.h", line):
			o.write(headers)
			print "Patch 4: headers murmur/Server.h patched"
		if re.search(r"void dblog", line):
			o.write(subs)
			print "Patch 3: subs in murmur/Server.h patched"


	f.close()
	o.close()
	shutil.move('murmur/Server.h.temp', 'murmur/Server.h')
except IOError:
	print "IOError"
	sys.exit()
			

# patch murmur/Server.cpp
add_del_remoteusers = '''
/* yangxu */
void Server::newRemoteClient(RemoteUser *u) {
    MumbleProto::UserState mpus;
    mpus.set_session(u->uiSession);
    mpus.set_name(u->qsName.toLocal8Bit().constData());

    sendAll(mpus);
}

void Server::delRemoteClient(int uid) {
    MumbleProto::UserRemove mpur;
    mpur.set_session(uid);

    sendAll(mpur);
}
'''

p_group_manager = '''
    /* yangxu */
    pGroupManager = new GroupManager(&ndnMediaPro);
    connect(pGroupManager, SIGNAL(remoteUserJoin(RemoteUser *)), this, SLOT(newRemoteClient(RemoteUser *)));
    connect(pGroupManager, SIGNAL(remoteUserLeave(int)), this, SLOT(delRemoteClient(int)));

   connect(&ndnMediaPro.ndnState, SIGNAL(remoteMediaArrivalSig(QString)),
        this, SLOT(receiveRemoteData(QString)));
'''

remote_data = '''
/* senwang*/
void Server::receiveRemoteData(QString strUserName) {

    int res = 0;
    QByteArray qba;
    char buf[5000];
    res = ndnMediaPro.getRemoteMedia(strUserName, buf, sizeof(buf));
    if (res <= 0) return;
    QHash<unsigned int, ServerUser *>::const_iterator i;
    for (i = qhUsers.constBegin(); i != qhUsers.constEnd(); i++ ) {
        sendMessage(i.value(), buf, res, qba, true);
	}

}
'''

distribute_data = '''
            /* senwang*/
            QString strFullName = pGroupManager->getFullLocalName();
            ndnMediaPro.sendLocalMedia(strFullName,buffer, len);
'''

leave_note = '''
	/* zhenkai */
	// send out leave notification
	pGroupManager->sendLeaveInterest();

	/* zhenkai */
	// close murmurd after the call is finished
	std::exit(0);
'''

try:
	f = open('murmur/Server.cpp', 'r')
	o = open('murmur/Server.cpp.temp', 'w')
	speech = False
	for line in f:
		o.write(line)
		if re.search(r"define UDP_PACKET_SIZE", line):
			o.write(add_del_remoteusers)
			print "Patch 5: add_del_remoteusers in murmur/Server.cpp patched"
		if re.search(r"initializeCert", line):
			o.write(p_group_manager)
			print "Patch 6: p_group_manager in murmur/Server.cpp patched"
		if re.search(r"Normal speech", line):
			speech = True
		if  speech and re.search(r"SENDTO", line):
			o.write(distribute_data)
			print "Patch 7: distribute_data in murmur/Server.cpp patched"
			speech = False
		if re.search(r"u->deleteLater", line):
			o.write(leave_note)
			print "Patch 8: leave_note in murmur/Server.cpp patched"
	o.write(remote_data)	
	print "Patch 9: remote_data in murmur/Server.cpp patched"

	f.close()
	o.close()
	shutil.move('murmur/Server.cpp.temp', 'murmur/Server.cpp')
except IOError:
	print "IOError"
	sys.exit()
