#ifndef NEWCONFWIZARD_H
#define NEWCONFWIZARD_H

#include <QWizard>
#include <QDateTime>
#include "announcement.h"

class QLabel;
class QLineEdit;
class QCheckBox;
class QGroupBox;
class QRadioButton;
class QTextEdit;
class QDateTimeEdit;
class QSpinBox;
class QVBoxLayout;
class QListWidget;
class QPushButton;

class ConfWizard : public QWizard
{
	Q_OBJECT

public:
	ConfWizard(QWidget *parent = 0);

	void accept();
	Announcement *getAnnouncement();

private:
	Announcement *a;

};

class IntroPage : public QWizardPage
{
	Q_OBJECT

public:
	IntroPage(QWidget *parent = 0);

private:
	QLabel *label;
};

class ConfigPage: public QWizardPage
{
	Q_OBJECT
public:
	ConfigPage(QWidget *parent = 0);
private:
	QLabel *confNameLabel;
	QLabel *organizerLabel;
	QLabel *emailLabel;
	QLabel *descLabel;
	QLabel *dateLabel;
	QLabel *timeLabel;
	QLabel *durationLabel;
	QLabel *hourLabel;
	QLabel *minuteLabel;

	QLineEdit *confNameLineEdit;
	QLineEdit *organizerLineEdit;
	QLineEdit *emailLineEdit;
	QCheckBox *videoCheckBox;
	QCheckBox *audioCheckBox;
	QCheckBox *textCheckBox;
	QCheckBox *privateConfBox;
	QTextEdit *descTextEdit;
	QDateTimeEdit *date;
	QDateTimeEdit *time;
	QSpinBox *hourEdit;
	QSpinBox *minuteEdit;
};

class ConclusionPage : public QWizardPage
{
	Q_OBJECT
public:
	ConclusionPage(QWidget *parent = 0);
protected:
	void initializePage();

private slots:
	void browseCerts();

private:
	QLabel *label;
	QVBoxLayout *layout;
	QLabel *displayCerts;
	QListWidget *certList;
	QPushButton *browse;
};



#endif
