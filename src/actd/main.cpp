#include <QApplication>
#include "mainwindow.h"
#include <QIcon>

int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(actd);
	QApplication app(argc, argv);
	app.setWindowIcon(QIcon(":/actd.icns"));
	MainWindow actdWin(argv);
	actdWin.show();
	actdWin.activateWindow();
	actdWin.raise();
	return actdWin.exec();
}
