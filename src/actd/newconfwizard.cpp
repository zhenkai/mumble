#include "newconfwizard.h"
#include <QtGui>
#include <QSpinBox>
#include <QUuid>
#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/rand.h>
#ifdef __cplusplus
}
#endif


Announcement *ConfWizard::getAnnouncement() {
	return a;
}

ConfWizard::ConfWizard(QWidget *parent)
	: QWizard(parent)
{
	// no intro
	// addPage(new IntroPage);
	addPage(new ConfigPage);
	addPage(new ConclusionPage);
	a = NULL;
	setWindowTitle(tr("Conference Announcement Wizard"));
}

void ConfWizard::accept() {

	a = new Announcement();
	a->setOwner(true);
	QString confName = field("confName").toString();
	a->setConfName(confName);
	QString organizer = field("organizer").toString();
	a->setOrganizer(organizer);
	QString email = field("email").toString();
	a->setEmail(email);
	a->setAudio(true);
	bool video = field("video").toBool();
	a->setVideo(video);
	QString desc = field("confDesc").toString();
	a->setDesc(desc);
	QDate date = field("date").toDateTime().date();
	a->setDate(date);
	QTime time = field("time").toDateTime().time();
	a->setTime(time);
	int hours = field("hours").toInt();
	a->setHours(hours);
	int minutes = field("minutes").toInt();
	a->setMinutes(minutes);
	bool isPrivate = field("private").toBool();
	a->setIsPrivate(isPrivate);

	if (isPrivate) {
		QSettings settings("UCLA-IRL", "ACTD");
		QString qsCerts = settings.value("qsCerts").toString();
		settings.setValue("qsCerts", "");
		QStringList certs = qsCerts.split(":");
		a->setCerts(certs);
		
		/*
		unsigned char bytes[32];
		RAND_bytes(bytes, 32);
		QByteArray qba((char *)bytes, int (32));
		QString opaqueName(qba.toBase64()) ;
		*/
		QUuid opn = QUuid::createUuid();
		QString opaqueName = opn.toString();
		a->setOpaqueName(opaqueName);

		a->initConferenceKey();
		a->initAudioSessionKey();
	}


	QDialog::accept();

}

IntroPage::IntroPage(QWidget *parent)
	: QWizardPage(parent)
{
	setTitle(tr("Introduction"));
	setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/watermark1.jpg"));
	label = new QLabel(tr("This wizard will generate the conference announcement"
						 "for you. You simply need to fill in the information"
						"required according to the hints on the screen."));
	label->setWordWrap(true);
	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(label);
	setLayout(layout);
}

ConclusionPage::ConclusionPage(QWidget *parent)
	:QWizardPage(parent)
{
	setTitle(tr("Conclusion"));
	setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/watermark2.jpg"));

	label = new QLabel;
	label->setWordWrap(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(label);
	setLayout(layout);
	
}

void ConclusionPage::initializePage()
{
	bool isPrivate = field("private").toBool();
	if (isPrivate) {
		QStringList certs = QFileDialog::getOpenFileNames(this, "Import Certs of Participants",
														getenv("HOME"), "Certs (*.pem)");
		QString qsCerts = certs.join(":");
		QSettings settings("UCLA-IRL", "ACTD");
		settings.setValue("qsCerts", qsCerts);
	}
	QString finishText = wizard()->buttonText(QWizard::FinishButton);
	finishText.remove('&');
	label->setText(tr("Click %1 to generate the conference announement.").arg(finishText));
}

ConfigPage::ConfigPage(QWidget *parent)
	:QWizardPage(parent)
{

	setTitle(tr("Conference Information"));
	setPixmap(QWizard::LogoPixmap, QPixmap(":/images/logo1.jpg"));

	confNameLabel = new QLabel(tr("&Conference Name:"));
	confNameLineEdit = new QLineEdit;
	confNameLabel->setBuddy(confNameLineEdit);

	organizerLabel = new QLabel(tr("&Organizer Name:"));
	organizerLineEdit = new QLineEdit;
	organizerLabel->setBuddy(organizerLineEdit);

	emailLabel = new QLabel(tr("&Email:"));
	emailLineEdit = new QLineEdit;
	emailLabel->setBuddy(emailLineEdit);

	// disable video for now
	videoCheckBox = new QCheckBox(tr("Enable Video"));
	privateConfBox = new QCheckBox(tr("Private Conference"));
	videoCheckBox->setEnabled(false);

	descLabel = new QLabel(tr("Conference &Description"));
	descTextEdit = new QTextEdit;
	descLabel->setBuddy(descTextEdit);

	dateLabel = new QLabel(tr("Date & Time:"));
	date = new QDateTimeEdit(QDate::currentDate());
	date->setMinimumDate(QDate::currentDate());
	date->setCalendarPopup(true);

	time = new QDateTimeEdit(QTime::currentTime());
	time->setCurrentSection(QDateTimeEdit::HourSection);

	QHBoxLayout *dtLayout = new QHBoxLayout;
	dtLayout->addWidget(date);
	dtLayout->addWidget(time);


	durationLabel = new QLabel(tr("Duration:"));
	hourLabel = new QLabel(tr("Hours"));
	minuteLabel = new QLabel(tr("Minutes"));

	hourEdit = new QSpinBox;
	hourEdit->setRange(0, 23);
	minuteEdit = new QSpinBox;
	minuteEdit->setRange(0, 59);
	minuteEdit->setSingleStep(5);

	QHBoxLayout *durationLayout = new QHBoxLayout;
	durationLayout->addWidget(hourEdit);
	durationLayout->addWidget(hourLabel);
	durationLayout->addWidget(minuteEdit);
	durationLayout->addWidget(minuteLabel);
	
	registerField("confName*", confNameLineEdit); // mandatory
	registerField("organizer*", organizerLineEdit); // mandatory
	registerField("email", emailLineEdit);
	registerField("video", videoCheckBox);
	registerField("confDesc", descTextEdit, "plainText");
	registerField("date", date);
	registerField("time", time);
	registerField("hours", hourEdit, "value");
	registerField("minutes", minuteEdit, "value");
	registerField("private", privateConfBox);
	
	QGridLayout *layout = new QGridLayout;
	layout->addWidget(confNameLabel, 0, 0);
	layout->addWidget(confNameLineEdit, 0, 1);
	layout->addWidget(organizerLabel, 1, 0);
	layout->addWidget(organizerLineEdit, 1, 1);
	layout->addWidget(emailLabel, 2, 0);
	layout->addWidget(emailLineEdit, 2, 1);
	layout->addWidget(videoCheckBox, 3, 0);
	layout->addWidget(privateConfBox, 3, 1);
	layout->addWidget(dateLabel, 4, 0);
	layout->addLayout(dtLayout, 4, 1);
	layout->addWidget(durationLabel, 5, 0);
	layout->addLayout(durationLayout, 5, 1);
	layout->addWidget(descLabel, 6, 0, 1, 2);
	layout->addWidget(descTextEdit, 7, 0, 3, 2);

	setLayout(layout);

}
