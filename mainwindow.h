#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QHash>
#include <QTextEdit>
#include <QProcess>

#include "newconfwizard.h"
#include "sessionenum.h"

class QDialogButtonBox;
class QTreeWidget;
class QTreeWidgetItem;
class QPushButton;
class QUrlInfo;
class ConfWizard;

class MainWindow: public QDialog
{
	Q_OBJECT

public:
	MainWindow(char * argv[], QWidget *parent = 0);
	QSize sizeHint() const;

private slots:

	void processItem();
	void joinConference();
	void editConference();
	void dismissConference();
	void newConference();
	void exportCert();
	void changePref();
	void addConferenceToList(Announcement *announce);
	void removeConferenceFromList(QString, QString);
	void listPrivateConferences();
	void mumbleCleanup();

private:
	void readSettings();
	void writeSettings();


private:
	QTreeWidget *pubConfList;
	//QTreeWidget *myConfList;

	QHash<QTreeWidgetItem *, Announcement *> itemToAnnouncement;

	QLabel *pubConfLabel;
	//QLabel *myConfLabel;
	QLabel *confDescLabel;
	QLabel *currentPrefLabel;

	QPushButton *newButton;
	QPushButton *prefButton;
	QPushButton *exportCertButton;
	QPushButton *quitButton;


	QPushButton *joinButton;
	QPushButton *editButton;
	QPushButton *dismissButton;

	QCheckBox *listPrivate;
	
	QTextEdit *confDesc;

//	QDialogButtonBox *topButtonBox;
	QDialogButtonBox *footButtonBox;

	QString prefix;
	QString audioPath;
	QString mumblePath;
	QString kiwiPath;
	
	SessionEnum * sd;

	QProcess *audioProcess;
	QProcess *mumbleProcess;
	QProcess *kiwiProcess;

	QString binaryPath;

};

#endif
