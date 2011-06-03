#include "debugbox.h"

void debug(QString msg) {
	qWarning() << "+++++Debug: "<<  msg << "\n";
}

void critical (QString msg) {
	debug(msg);
	exit(1);
}
