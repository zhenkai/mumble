#include "debugbox.h"
#include <QMessageBox>

#ifdef QT_NO_DEBUG
void debug(QString msg) {
}

void critical (QString msg) {
}

#else
void debug(QString msg) {
	qDebug() << "+++++Debug: "<<  msg << "\n";
}

void critical (QString msg) {
	debug(msg);
	exit(1);
}
#endif
