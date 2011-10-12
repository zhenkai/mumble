#include <QApplication>
#include <QIcon>
#include "videowindow.h"

int main(int argc, char *argv[])
{
//	Q_INIT_RESOURCE(kiwi);
	QApplication app(argc, argv);
	
#ifdef __APPLE__
	app.setWindowIcon(QIcon(":/kiwi.icns"));
#else
	app.setWindowIcon(QIcon(":/kiwi.svg"));
#endif
	VideoWindow kiwi;
	kiwi.show();
	kiwi.activateWindow();
	return kiwi.exec();
}
