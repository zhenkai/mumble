#include <QApplication>
#include "mainwindow.h"
#include <QIcon>

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(actd);
	QApplication app(argc, argv);
#ifdef __APPLE__
	app.setWindowIcon(QIcon(":/actd.icns"));
#else
	app.setWindowIcon(QIcon(":/actd.svg"));
#endif
	MainWindow actdWin(argv);
	actdWin.show();
	actdWin.activateWindow();
	//actdWin.raise();
	return actdWin.exec();
}
