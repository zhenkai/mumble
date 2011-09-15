/* Copyright (C) 2010-present, Zhenkai Zhu <zhenkai@cs.ucla.edu>

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
#include "murmur_pch.h"
#include "RemoteUser.h"
#define REFRESH_INTERVAL 10
#define REMOVE_INTERVAL 21

RemoteUser::RemoteUser(QString prefix, QString name) : remoteUserPrefix(prefix) {
    qsName = name;
    timestamp = QDateTime::currentDateTime();
	left = false;
}


void RemoteUser::refreshReceived() {
    timestamp = QDateTime::currentDateTime();
}

bool RemoteUser::needRefresh() {
	QDateTime now = QDateTime::currentDateTime();
	if (timestamp.secsTo(now) > REFRESH_INTERVAL) {
		return true;
	}
	return false;
}

bool RemoteUser::isStaled() {
	/*
	if (left)
		return true;
		*/

	QDateTime now = QDateTime::currentDateTime();
	if (timestamp.isNull()) {
		/*
		fprintf(stderr, "timestamp is null, will initialize one");
		timestamp = QDateTime::currentDateTime();
		*/
		// Null time: this RU is weird, remote it;
		return true;
	}
	if (timestamp.secsTo(now) > REMOVE_INTERVAL) {
		return true;
	}
	return false;
}

void RemoteUser::setPrefix(QString prefix) {
	this->remoteUserPrefix = prefix;
}
