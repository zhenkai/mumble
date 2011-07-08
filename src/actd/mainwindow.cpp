#include <QtGui>
#include <QInputDialog>
#include <QFile>
#include <QDir>
#include <QTextStream>

#include "mainwindow.h"
#include "debugbox.h"


MainWindow::MainWindow(QWidget *parent) 
	:QDialog(parent)
{


	pubConfList = new QTreeWidget;
	pubConfList->setEnabled(false);
	pubConfList->setRootIsDecorated(false);
	pubConfList->setHeaderLabels(QStringList() << tr("")<<tr("")<<tr("")<<tr("Conference Name")<<tr("Organizer")<<tr("Contact"));
	
	pubConfList->setColumnWidth(0, 25);
	pubConfList->setColumnWidth(1, 25);
	pubConfList->setColumnWidth(2, 25);
	pubConfList->setColumnWidth(3, 200);
	pubConfList->setColumnWidth(4, 150);


	//myConfList = new QTreeWidget;
	//myConfList->setEnabled(false);
	//myConfList->setRootIsDecorated(false);
	//myConfList->setHeaderLabels(QStringList() << tr("Audio")<<tr("Video")<<tr("Conference Name")<<tr("Organizer"));

	confDesc = new QTextEdit;
	confDesc->setReadOnly(true);
	confDesc->setEnabled(false);

	pubConfLabel = new QLabel(tr("Public Conferences"));
	//myConfLabel = new QLabel(tr("My Conferences"));
	confDescLabel = new QLabel(tr("Conference Descriptions"));

	newButton = new QPushButton(tr("New"));
	prefButton = new QPushButton(tr("Preferences"));
	aboutButton = new QPushButton(tr("About"));
	quitButton = new QPushButton(tr("Quit"));

	joinButton = new QPushButton(tr("Join"));
	joinButton->setEnabled(false);
	//editButton = new QPushButton(tr("Edit"));
	//editButton->setEnabled(false);
	dismissButton = new QPushButton(tr("Dismiss"));
	dismissButton->setEnabled(false);

	listPrivate = new QCheckBox(tr("Also List Private Conferences"));
	connect(listPrivate, SIGNAL(stateChanged(int)), this, SLOT(listPrivateConferences()));
	
	QHBoxLayout *topLayout = new QHBoxLayout;
	topLayout->addWidget(newButton);
	topLayout->addWidget(prefButton);
	topLayout->addWidget(aboutButton);
	topLayout->addWidget(quitButton);
	

	footButtonBox = new QDialogButtonBox;
	footButtonBox->addButton(joinButton, QDialogButtonBox::ActionRole);
	//footButtonBox->addButton(editButton, QDialogButtonBox::ActionRole);
	footButtonBox->addButton(dismissButton, QDialogButtonBox::ActionRole);

	QHBoxLayout *midLayout = new QHBoxLayout;
	midLayout->addWidget(pubConfLabel);
	midLayout->addWidget(listPrivate);

	QVBoxLayout *mainLayout = new QVBoxLayout;
	mainLayout->addLayout(topLayout);
	mainLayout->addLayout(midLayout);
	mainLayout->addWidget(pubConfList);
	mainLayout->addWidget(confDescLabel);
	mainLayout->addWidget(confDesc);
	mainLayout->addWidget(footButtonBox);
	setLayout(mainLayout);

	setWindowTitle(tr("Conference Management Tool"));

	connect(newButton, SIGNAL(clicked()), this, SLOT(newConference()));
	connect(prefButton, SIGNAL(clicked()), this, SLOT(changePref()));
	connect(aboutButton, SIGNAL(clicked()), this, SLOT(about()));
	connect(quitButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(joinButton, SIGNAL(clicked()), this, SLOT(joinConference()));
	//connect(editButton, SIGNAL(clicked()), this, SLOT(editConference()));
	connect(dismissButton, SIGNAL(clicked()), this, SLOT(dismissConference()));
	connect(pubConfList, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(processItem()));
	
	readSettings();
	
	sd = new SessionEnum(prefix);
	connect(sd, SIGNAL(expired(QString, QString)), this, SLOT(removeConferenceFromList(QString, QString)));
	connect(sd, SIGNAL(add(Announcement *)), this, SLOT(addConferenceToList(Announcement *)));
	
	
	audioProcess = new QProcess(this);


}

QSize MainWindow::sizeHint() const
{
	return QSize(600, 600);
}


void MainWindow::processItem(){
	QTreeWidgetItem *current = pubConfList->currentItem();
	if (!current) {
		pubConfList->setEnabled(false);
		confDesc->clear();
		confDesc->setEnabled(false);
		joinButton->setEnabled(false);
		//editButton->setEnabled(false);
		dismissButton->setEnabled(false);
		return;
	}
	
	Announcement *a = itemToAnnouncement[current];
	if (a->getOwner()) {
		joinButton->setEnabled(true);
		//editButton->setEnabled(true);
		dismissButton->setEnabled(true);

	} else {
		joinButton->setEnabled(true);
		//editButton->setEnabled(false);
		dismissButton->setEnabled(false);
	}
//	confDesc->setPlainText(a->getDesc());
	QString desc = current->data(3, Qt::UserRole).toString();
	confDesc->setPlainText(desc);

}



void MainWindow::readSettings() {
	QSettings settings("UCLA_IRL", "ACTD");
	prefix = settings.value("prefix", QString("")).toString();

	if (prefix == "") {
		changePref();
	}

	audioPath = settings.value("audioPath", QString("")).toString();

}

void MainWindow::writeSettings() {
	QSettings settings("UCLA_IRL", "ACTD");
	settings.setValue("prefix", prefix);
	settings.setValue("audioPath", audioPath);
}

void MainWindow::about() {
	QMessageBox::about(this, tr("About actd"), QString("%1\n%2").arg(tr("This is an application similar to sd of the MBone.")).arg(tr("Conference management is handled by this application.")));

}


void MainWindow::changePref() {
	bool ok;

	QString text = QInputDialog::getText(this, tr("Get Name Prefix for Conference"), tr("Input the name prefix for the conference application:"), QLineEdit::Normal, prefix, &ok);
	if (ok && !text.isEmpty()) {
		prefix = text; 
		writeSettings();

	}
}

void MainWindow::joinConference() {

	QTreeWidgetItem *current = pubConfList->currentItem();	
	if (!current) {
		critical("current is null");
	}

	QString confName = current->text(3);
	// should be indicated from current item in the future
	bool audio = true;
	bool video = false;
	if (video) {
		// no video yet; do nothing
	}
	if (audio) {

		if (audioProcess->state() != QProcess::NotRunning) {
			int ret = QMessageBox::warning(this, tr("Conference Tool Set"),
											tr("The audio daemon is already running.\n" "Do you want to continue?"), QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Cancel);
			if (ret == QMessageBox::Cancel)
				return;
			else {
				audioProcess->kill();
				//if (audioProcess->state() != QProcess::NotRunning)
				//	debug("panic! can not kill audio tool process!");
				delete audioProcess;
				audioProcess = new QProcess(this);
			}
		}

		QString qsConfig = "<config><prefix>" + prefix + "</prefix><confName>"\
							+ confName + "</confName>";
		
		Announcement *a = itemToAnnouncement[current];
		if (a->getIsPrivate()) {
			qsConfig += "<private>true</private>";
			QByteArray confKey((const char *)a->conferenceKey, (int)sizeof(a->conferenceKey));	
			QString qsConfKey = QString(confKey.toBase64());
			qsConfig += "<confKey>" + qsConfKey + "</confKey>";
			QByteArray sessionKey((const char *)a->audioSessionKey, (int)sizeof(a->audioSessionKey));
			QString qsSessionKey = QString(sessionKey.toBase64());
			qsConfig += "<sessionKey>" + qsSessionKey + "</sessionKey>";
		}
		else {
			qsConfig += "<private>false</private>";
		}
		qsConfig += "</config>";

		QDomDocument doc;
		QDir actDir(QDir::homePath() + "/" + ".actd");
		if (!actDir.exists()) {
			QDir homedir = QDir::home();
			homedir.mkdir(".actd");
		}
		QString fileName = actDir.absolutePath() + "/" + ".config";
		QFile config(fileName);


		config.open(QIODevice::WriteOnly | QIODevice::Truncate);
		QTextStream out (&config);

		out << qsConfig;
		config.close();

		if (audioPath == "") {
			audioPath = QFileDialog::getOpenFileName(this, tr("Choose the audio tool"), ".", tr(""));
			writeSettings();
		}
		QFile audioFile(audioPath, this);
		if (!audioFile.exists()) {
			audioPath = QFileDialog::getOpenFileName(this, tr("Choose the audio tool"), ".", tr(""));
			writeSettings();
		}
		
		audioProcess->start(audioPath);
		while (audioProcess->error() == QProcess::FailedToStart) {
			int ret = QMessageBox::warning(this, tr("Failed to start audio tool."), tr("Failed to start audio tool\n Do you want to try again?"), QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Yes);
			if (ret == QMessageBox::Cancel)
				break;
			audioPath = QFileDialog::getOpenFileName(this, tr("Please choose the audio tool"), ".", tr(""));
			writeSettings();
			audioProcess->start(audioPath);
		}
		QProcess *mumbleProcess = new QProcess(this);
		
#ifdef __APPLE__
		QString mumblePath = "/Applications/Mumble.app/Contents/MacOS/mumble";
#else
		QString mumblePath ="mumble";
#endif

#ifdef __APPLE__
		QFile mumbleFile(mumblePath, this);
		if (!mumbleFile.exists()) {
			mumblePath = QFileDialog::getOpenFileName(this, tr("Choose path for Mumble"), ".", tr(""));
			mumbleProcess->start(mumblePath + "/Contents/MacOs/mumble");
		}
		else
			mumbleProcess->start(mumblePath);

#else
		mumbleProcess->start(mumblePath);
#endif

		/*
		//while (mumbleProcess->error() == QProcess::FailedToStart || mumbleProcess->UnknownError) {
		if(mumbleProcess->error() == QProcess::FailedToStart || mumbleProcess->UnknownError) {
			int ret = QMessageBox::warning(this, tr("Failed to start Mumble"),\
			tr("Failed to find Mumble application.\n Please launch mumble manually."), QMessageBox::Cancel | QMessageBox::Yes, QMessageBox::Yes);
			if (ret == QMessageBox::Cancel) {
				audioProcess->kill();
			}
			//mumblePath = QFileDialog::getOpenFileName(this, tr("Please choose Mumble"), ".", tr(""));
			//mumbleProcess->start(mumblePath);
		}
		*/
		
	}
}

void MainWindow::editConference() {
}


void MainWindow::dismissConference() {
	QTreeWidgetItem *current = pubConfList->currentItem();
	if (!current) {
		QString qs = "Dismiss: current is null!";
		critical(qs);
	}

	// remove from QList
	//QString confName = current->text(3);
	//QString organizer = current->text(4);
	Announcement *a = itemToAnnouncement[current];
	
	//sd->removeFromMyConferences(confName, organizer);
	sd->removeFromMyConferences(a);
	//delete a;
	itemToAnnouncement.remove(current);

	// remove from GUI
	delete current;
	
}

void MainWindow::newConference() {

	Announcement *a = new Announcement();
	ConfWizard wizard;
	wizard.exec();
	Announcement *announce = wizard.getAnnouncement();
	if (announce == NULL)
		return;
	a->copy(announce);
	addConferenceToList(a);
	sd->addToMyConferences(a);
}

void MainWindow::addConferenceToList(Announcement *announce) {

	if (announce == NULL) {
		QString qs ="add: conference announcement is null!";
		critical(qs);
	}

	QTreeWidgetItem *item = new QTreeWidgetItem;

	itemToAnnouncement.insert(item, announce);
	
	item->setText(3, announce->getConfName());
	QString oDesc = announce->getDesc();
	QString date = announce->getDate().toString("ddd MMM d"); 
	QString start = announce->getTime().toString("h:mm AP"); 
	int hours = announce->getHours();
	int minutes = announce->getMinutes();
	
	QString desc = QString("Date: %1\nTime: %2\nDuration: %3 Hours %4 Minutes\n-------------------------------\n%5").arg(date).arg(start).arg(hours).arg(minutes).arg(oDesc);

	item->setData(3, Qt::UserRole, desc);
	
	item->setText(4, announce->getOrganizer());
	item->setText(5, announce->getEmail());


	if (announce->getOwner() ) {
		QPixmap setting(":/images/setting.png");
		item->setIcon(0, setting);
	} else {
		if (announce->getIsPrivate()) {
			QPixmap priv(":/images/priv.png");
			item->setIcon(0, priv);
		}
	}

	if (announce->getAudio()) {
		QPixmap audio(":/images/audio.png");
		item->setIcon(1, audio);

	}

	if (announce->getVideo()) {
		QPixmap video(":/images/video.png");
		item->setIcon(2, video);
	}


	pubConfList->addTopLevelItem(item);
	if (!pubConfList->currentItem()) {
		pubConfList->setCurrentItem(pubConfList->topLevelItem(0));
		pubConfList->setEnabled(true);
		confDesc->setEnabled(true);
	}
}

void MainWindow::removeConferenceFromList(QString confName, QString organizer) {

	// remove from GUI
	QList<QTreeWidgetItem *> conferences = pubConfList->findItems(confName, Qt::MatchFixedString, 3);
	if (conferences.isEmpty()) {
		QString qs = QString("no conference named %1 found!!").arg(confName);
		critical(qs);
	}

	while (!conferences.isEmpty()) {
		QTreeWidgetItem *item = NULL;
		item = conferences.takeFirst();
		if (item != NULL && item->text(4) == organizer) {
			itemToAnnouncement.remove(item);
			delete item;
		}
	}

}

void MainWindow::listPrivateConferences() {
	if (listPrivate->isChecked()) {
		pubConfLabel->setText("Public And Private Conferences");
		sd->setListPrivate(true);
	}
	else {
		sd->setListPrivate(false);
		pubConfLabel->setText("Public Conferences");
	}
}
