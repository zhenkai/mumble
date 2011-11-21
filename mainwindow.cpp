#include <QtGui>
#include <QInputDialog>
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QTimer>
#include "sessionenum.h"

#include "mainwindow.h"
#include "debugbox.h"


MainWindow::MainWindow(char *argv[], QWidget *parent) 
	:QDialog(parent)
{
	readSettings();

	binaryPath = argv[0];
	QString temp = binaryPath.left(binaryPath.lastIndexOf("/"));
	binaryPath = temp;
	debug("binary path is " + temp);
	pubConfList = new QTreeWidget;
	pubConfList->setEnabled(false);
	pubConfList->setRootIsDecorated(false);
	pubConfList->setHeaderLabels(QStringList() << tr("")<<tr("")<<tr("")<<tr("")<<tr("Conference Name")<<tr("Organizer")<<tr("Contact"));
	
	pubConfList->setColumnWidth(0, 25);
	pubConfList->setColumnWidth(1, 25);
	pubConfList->setColumnWidth(2, 25);
	pubConfList->setColumnWidth(3, 25);
	pubConfList->setColumnWidth(4, 200);
	pubConfList->setColumnWidth(5, 150);

	confDesc = new QTextEdit;
	confDesc->setReadOnly(true);
	confDesc->setEnabled(false);

	pubConfLabel = new QLabel(tr("Public Conferences"));
	confDescLabel = new QLabel(tr("Conference Descriptions"));
	QString warning = "Current prefix for voice data is: " + prefix ;
	currentPrefLabel = new QLabel(warning);

	newButton = new QPushButton(tr("New"));
	prefButton = new QPushButton(tr("Preferences"));
	exportCertButton = new QPushButton(tr("Export Cert"));
	quitButton = new QPushButton(tr("Quit"));

	joinButton = new QPushButton(tr("Join"));
	joinButton->setEnabled(false);
	dismissButton = new QPushButton(tr("Dismiss"));
	dismissButton->setEnabled(false);

	listPrivate = new QCheckBox(tr("Also List Private Conferences"));
	connect(listPrivate, SIGNAL(stateChanged(int)), this, SLOT(listPrivateConferences()));
	
	QHBoxLayout *topLayout = new QHBoxLayout;
	topLayout->addWidget(newButton);
	topLayout->addWidget(prefButton);
	topLayout->addWidget(exportCertButton);
	topLayout->addWidget(quitButton);
	

	footButtonBox = new QDialogButtonBox;
	footButtonBox->addButton(joinButton, QDialogButtonBox::ActionRole);
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
	mainLayout->addWidget(currentPrefLabel);
	mainLayout->addWidget(footButtonBox);
	setLayout(mainLayout);

	setWindowTitle(tr("Conference Management Tool"));

	connect(newButton, SIGNAL(clicked()), this, SLOT(newConference()));
	connect(prefButton, SIGNAL(clicked()), this, SLOT(changePref()));
	connect(exportCertButton, SIGNAL(clicked()), this, SLOT(exportCert()));
	connect(quitButton, SIGNAL(clicked()), this, SLOT(close()));
	connect(joinButton, SIGNAL(clicked()), this, SLOT(joinConference()));
	connect(dismissButton, SIGNAL(clicked()), this, SLOT(dismissConference()));
	connect(pubConfList, SIGNAL(currentItemChanged(QTreeWidgetItem *, QTreeWidgetItem *)), this, SLOT(processItem()));
	
	sd = new SessionEnum();
	connect(sd, SIGNAL(expired(QString, QString)), this, SLOT(removeConferenceFromList(QString, QString)));
	connect(sd, SIGNAL(add(Announcement *)), this, SLOT(addConferenceToList(Announcement *)));

	if (prefix == "") {
		QTimer::singleShot(500, this, SLOT(changePref()));
	}

	audioProcess = NULL;
	mumbleProcess = NULL;
	kiwiProcess = NULL;
	
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
		dismissButton->setEnabled(false);
		return;
	}
	
	Announcement *a = itemToAnnouncement[current];
	if (a->getOwner()) {
		joinButton->setEnabled(true);
		dismissButton->setEnabled(true);

	} else {
		joinButton->setEnabled(true);
		dismissButton->setEnabled(false);
	}
	QString desc = current->data(3, Qt::UserRole).toString();
	confDesc->setPlainText(desc);

}



void MainWindow::readSettings() {
	QSettings settings("UCLA_IRL", "ACTD");
	prefix = settings.value("prefix", QString("")).toString();
}

void MainWindow::writeSettings() {
	QSettings settings("UCLA_IRL", "ACTD");
	settings.setValue("prefix", prefix);
}

void MainWindow::exportCert() {
	QString certFilename = QString("%1/.actd/actd_cert.pem").arg(getenv("HOME"));
	QFile certFile(certFilename);
	if (!certFile.exists()) {
		QString keystoreFilename = QString("%1/.actd/.actd_keystore").arg(getenv("HOME"));
		QFile keystoreFile(keystoreFilename);
		if (!keystoreFile.exists()) {
			critical("Keystore file has been deleted or renamed after actd launches. Please restart actd.");
		}
		FILE *fp;
		PKCS12 *keystore;
		OpenSSL_add_all_algorithms();
		fp = fopen(keystoreFilename.toStdString().c_str(), "rb");
		if (fp == NULL)
			abort();
		
		keystore = d2i_PKCS12_fp(fp, NULL);
		fclose(fp);
		if (keystore == NULL)
			abort();

		EVP_PKEY *private_key;
		X509 *certificate;
		int res = PKCS12_parse(keystore, (char *)"Th1s1s@p8ssw0rdf0r8ctd.", &private_key, &certificate, NULL);
		PKCS12_free(keystore);
		if (res == 0)
			return;

		fp = fopen(certFilename.toStdString().c_str(), "w");
		res = PEM_write_X509(fp, certificate);
		fclose(fp);
		if (res == 0)
			abort();
	}

	QString exportFilename = QFileDialog::getSaveFileName(this, "Export Actd Cert", QString("%1/actd_cert.pem").arg(getenv("HOME")));
	if (!certFile.copy(exportFilename)) {
		QFile::remove(exportFilename);
		if (!certFile.copy(exportFilename)) {
			QMessageBox::warning(this, "Export Cert Failed", "Can not export cert. Please check if you have write permission");
		}
	}
}


void MainWindow::changePref() {
	bool ok;

	QString text = QInputDialog::getText(this, tr("Get Name Prefix for Conference"), tr("Input the name prefix for the conference application:"), QLineEdit::Normal, prefix, &ok);
	if (ok && !text.isEmpty()) {
		prefix = text; 
		writeSettings();
		QString warning = "Current prefix for voice data is: " + prefix ;
		currentPrefLabel->setText(warning);
	}
}

void MainWindow::joinConference() {

	mumbleCleanup();

	QTreeWidgetItem *current = pubConfList->currentItem();	
	if (!current) {
		critical("current is null");
	}

	QString confName = current->text(3);
	if (true) {


		QString qsConfig = "<config><prefix>" + prefix + "</prefix><confName>";


		
		Announcement *a = itemToAnnouncement[current];
		if (a->getIsPrivate()) {
			qsConfig += a->getOpaqueName() + "</confName>";
			qsConfig += "<private>true</private>";
			QByteArray confKey((const char *)a->conferenceKey, (int)sizeof(a->conferenceKey));	
			QString qsConfKey = QString(confKey.toBase64());
			qsConfig += "<confKey>" + qsConfKey + "</confKey>";
			QByteArray sessionKey((const char *)a->audioSessionKey, (int)sizeof(a->audioSessionKey));
			QString qsSessionKey = QString(sessionKey.toBase64());
			qsConfig += "<sessionKey>" + qsSessionKey + "</sessionKey>";
		}
		else {
			qsConfig += confName + "</confName>";
			qsConfig += "<private>false</private>";
		}
		qsConfig += "<channelName>" + confName + "</channelName>";
		qsConfig += "</config>";

		QDomDocument doc;
		QDir actDir(QDir::homePath() + "/" + ".actd");
		if (!actDir.exists()) {
			QDir homedir = QDir::home();
			homedir.mkdir(".actd");
		}
		QString fileName = actDir.absolutePath() + "/" + ".config";
		QFile config(fileName);
		if (config.exists()) {
			config.remove();
		}

		config.open(QIODevice::WriteOnly | QIODevice::Truncate);
		QTextStream out (&config);

		out << qsConfig;
		// flush, maybe not needed, but sometimes release version of murmurd on iMac
		// uses old conference name
		config.flush();
		config.close();

		if (a->getAudio()) {
			audioProcess = new QProcess(this);
			mumbleProcess = new QProcess(this);

#ifdef QT_NO_DEBUG
#ifdef __APPLE__
			audioPath = binaryPath + "/" + "ndn-murmurd";
			mumblePath = binaryPath + "/" + "Mumble";
			kiwiPath = binaryPath + "/" + "kiwi";
#else
			audioPath = "ndn-murmurd";
			mumblePath = "ndn-mumble";
			kiwiPath = "kiwi";
#endif
#else // QT_NO_DEBUG
#ifdef __APPLE__
			audioPath = binaryPath + "/../../../" + "ndn-murmurd";
			mumblePath = binaryPath + "/../../../Mumble.app/Contents/MacOS/" + "Mumble";
			kiwiPath = binaryPath + "/../../../kiwi.app/Contents/MacOS/kiwi";
#else
			audioPath = binaryPath + "/ndn-murmurd";
			mumblePath = binaryPath + "/ndn-mumble";
			kiwiPath = binaryPath + "kiwi";
#endif
#endif // QT_NO_DEBUG

		
			audioProcess->start(audioPath);
			mumbleProcess->start(mumblePath);

		}
		if (a->getVideo()) {
			kiwiProcess = new QProcess(this);
#ifdef QT_NO_DEBUG
#ifdef __APPLE__
			kiwiPath = binaryPath + "/" + "kiwi";
#else
			kiwiPath = "kiwi";
#endif
#else // QT_NO_DEBUG
#ifdef __APPLE__
			kiwiPath = binaryPath + "/../../../kiwi.app/Contents/MacOS/kiwi";
#else
			kiwiPath = binaryPath + "kiwi";
#endif
#endif // QT_NO_DEBUG
			kiwiProcess->start(kiwiPath);
		}
	}
}


void MainWindow::mumbleCleanup() {
	if (mumbleProcess != NULL) {
		mumbleProcess->kill();
		mumbleProcess->deleteLater();
		mumbleProcess = NULL;
	}
	if (audioProcess != NULL) {
		audioProcess->kill();
		audioProcess->deleteLater();
		audioProcess = NULL;
	}
	if (kiwiProcess != NULL) {
		kiwiProcess->kill();
		kiwiProcess->deleteLater();
		kiwiProcess = NULL;
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

	Announcement *a = itemToAnnouncement[current];
	
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
	
	item->setText(4, announce->getConfName());
	QString oDesc = announce->getDesc();
	QString date = announce->getDate().toString("ddd MMM d"); 
	QString start = announce->getTime().toString("h:mm AP"); 
	int hours = announce->getHours();
	int minutes = announce->getMinutes();
	
	QString desc = QString("Date: %1\nTime: %2\nDuration: %3 Hours %4 Minutes\n-------------------------------\n%5").arg(date).arg(start).arg(hours).arg(minutes).arg(oDesc);

	item->setData(4, Qt::UserRole, desc);
	
	item->setText(5, announce->getOrganizer());
	item->setText(6, announce->getEmail());


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
	if (announce->getText()) {
		QPixmap text(":/images/text.png");
		item->setIcon(3, text);
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
