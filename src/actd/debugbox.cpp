#include "debugbox.h"
#include <QMessageBox>

void debug(QString msg) {
	qDebug() << "+++++Debug: "<<  msg << "\n";
}

void critical (QString msg) {
	debug(msg);
	exit(1);
}
