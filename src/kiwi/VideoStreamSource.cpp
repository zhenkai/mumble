#include "VideoStreamSource.h"
#include <QtXml>
#include <QMessageBox>
#define BUF_SIZE 10000000
#define FRAME_PER_SECOND 25 
#define MAX_EDGE_LENGTH 640 
#define FRESHNESS 2
#define ANNOUNCE_INTERVAL 598 
#define BROADCAST_PREFIX ("/ndn/broadcast/conference")
static enum ccn_upcall_res publishInfoCallback(struct ccn_closure *selfp, enum ccn_upcall_kind kind, struct ccn_upcall_info *info);
static SourceAnnouncer *gSourceAnnouncer;

CameraVideoInput::CameraVideoInput() {
	initialized = false;
	initCamera();
}

CameraVideoInput::~CameraVideoInput() {
	if (cap != NULL) {
		cvReleaseCapture(&cap);
		cap = NULL;
		initialized = false;
	}
}

bool CameraVideoInput::initCamera() {
	if (initialized == true)
		return true;

	cap = cvCaptureFromCAM(0);
	if (cap == NULL)
		return false;
	initialized = true;
}

IplImage *CameraVideoInput::getNextFrame() {
	if (!initialized)
		initCamera();
	
	if (!cvGrabFrame(cap))
		return NULL;

	IplImage *capImage = cvRetrieveFrame(cap);
	int x = capImage->width;
	int y = capImage->height;
	int max = x > y ? x : y;
	float scale = (float)((float) max / MAX_EDGE_LENGTH);
	int newX = int (x/scale);
	int newY = int (y/scale);
	if (newX % 2 != 0)
		newX++;
	if (newY % 2 != 0)
		newY++;
	CvSize size = cvSize(newX, newY);
	IplImage *retImage = cvCreateImage(size, 8, 3);
	cvResize(capImage, retImage);
	return retImage;
}

VideoStreamSource::VideoStreamSource() {
	seq = 0;
	initialized = false;
	cam = NULL;
	encoder = NULL;
	cam = new CameraVideoInput();
	if (cam == NULL)
		return;
	
	IplImage *image = cam->getNextFrame();
	if (image == NULL)
		return;
	
	encoder = new VideoStreamEncoder(BUF_SIZE, image->width, image->height, FRAME_PER_SECOND);

	if (encoder == NULL)
		return;
	
	nh = new NdnHandler();

	readNdnParams();

	sa = new SourceAnnouncer(confName, namePrefix);

	initialized = true;
	cvReleaseImage(&image);
	captureTimer = new QTimer(this);
	connect(captureTimer, SIGNAL(timeout()), this, SLOT(processFrame()));
	captureTimer->start(1000 / FRAME_PER_SECOND);

	bRunning = true;
	start();

}

VideoStreamSource::~VideoStreamSource() {
	bRunning = false;
	if (isRunning())
		wait();
	if (cam != NULL)
		delete cam;
	if (encoder != NULL)
		delete encoder;
	if (nh != NULL) {
		delete nh;
		nh = NULL;
	}
}

void VideoStreamSource::processFrame() {
	IplImage *currentFrame = cam->getNextFrame();
	int frameSize = 0;
	// do not free encodedFrame, as it is a pointer to the internal buffer of encoder
	const unsigned char *encodedFrame = encoder->encodeVideoFrame(currentFrame, &frameSize);
	IplImage *localDisplayFrame = cvCloneImage(currentFrame);
	emit imageCaptured("Me", localDisplayFrame);
	cvReleaseImage(&currentFrame);
	generateNdnContent(encodedFrame, frameSize);

	seq++;
}

void VideoStreamSource::readNdnParams() {
	QDomDocument settings;
	QString configFileName = QDir::homePath() + "/.actd/.config";
	QFile config(configFileName);
	if (!config.exists()) {
		fprintf(stderr, "Config file does not exist\n");
		abort();
	}
	if (!config.open(QIODevice::ReadOnly)) {
		fprintf(stderr, "Can not open config file\n");
		abort();
	}
	if (!settings.setContent(&config)) {
		config.close();
		fprintf(stderr, "Can not parse config file\n");
		abort();
	}

	QDomElement docElem = settings.documentElement();
	QDomNode n = docElem.firstChild();
	while (!n.isNull()) {
		if (n.nodeName() == "prefix") {
			namePrefix = n.toElement().text();
		}
		else if (n.nodeName() == "confName") {
			confName = n.toElement().text();
		}
		n = n.nextSibling();
	}

	if (namePrefix == "" || confName == "") {
		fprintf(stderr, "Null name prefix or conference name\n");
		abort();
	}

	username = getenv("KIWI_USERNAME");
	if (username.isEmpty()) {
		QMessageBox::warning(0, "Kiwi", "Environment variable \"KIWI_USERNAME\" not set. Program terminating.");
		std::exit(0);
	}

}

void VideoStreamSource::generateNdnContent(const unsigned char *buffer, int len) {
	struct ccn_charbuf *signed_info = ccn_charbuf_create();
	int res = ccn_signed_info_create(signed_info, nh->getPublicKeyDigest(), nh->getPublicKeyDigestLength(), NULL, CCN_CONTENT_DATA, FRESHNESS, NULL, nh->keylocator);
	if (res < 0) {
		fprintf(stderr, "Failed to create signed_info\n");
		abort();
	}
	
	//   e.g. /ndn/ucla.edu/kiwi/video/2
	struct ccn_charbuf *path = ccn_charbuf_create();
	ccn_name_from_uri(path, namePrefix.toStdString().c_str());
	ccn_name_append_str(path, username.toStdString().c_str());
	ccn_name_append_str(path, "video");
	struct ccn_charbuf *seqBuf = ccn_charbuf_create();
	ccn_charbuf_putf(seqBuf, "%ld", seq);
	ccn_name_append(path, seqBuf->buf, seqBuf->length);
	struct ccn_charbuf *content = ccn_charbuf_create();
	res = ccn_encode_ContentObject(content, path, signed_info, buffer, len, NULL, nh->getPrivateKey());
	if (res) {
		fprintf(stderr, "Failed to create video content\n");
		abort();
	}
	ccn_put(nh->h, content->buf, content->length);

	ccn_charbuf_destroy(&path);
	ccn_charbuf_destroy(&seqBuf);
	ccn_charbuf_destroy(&signed_info);
	ccn_charbuf_destroy(&content);
}

void VideoStreamSource::run() {
	int res = 0;
	while(res >= 0 && bRunning) {
		res = ccn_run(nh->h, 0);
		usleep(100);
	}
}

SourceAnnouncer::SourceAnnouncer(QString confName, QString prefix) {
	gSourceAnnouncer = this;
	leaving = false;

	this->confName = confName;
	this->prefix = prefix;
	username = getenv("KIWI_USERNAME");
	if (username.isEmpty()) {
		QMessageBox::warning(0, "Kiwi", "Environment variable \"KIWI_USERNAME\" not set. Program terminating.");
		std::exit(0);
	}
	nh = new NdnHandler();
	registerInterest();
	announceTimer = new QTimer(this);
	announceTimer->setInterval(ANNOUNCE_INTERVAL * 1000);
	announceTimer->setSingleShot(true);
	connect(announceTimer, SIGNAL(timeout()), this, SLOT(generateSourceInfo()));
	generateSourceInfo();

	bRunning = true;
	start();
}

SourceAnnouncer::~SourceAnnouncer() {
	bRunning = false;
	if (isRunning())
		wait();

	if (nh != NULL) {
		delete nh;
		nh = NULL;
	}
	if (publishInfo != NULL)
		free(publishInfo);
}

void SourceAnnouncer::registerInterest() {
	publishInfo = (struct ccn_closure *)calloc(1, sizeof(struct ccn_closure));
	publishInfo->p = &publishInfoCallback;
	struct ccn_charbuf *path = ccn_charbuf_create();
	ccn_name_from_uri(path, (const char *)BROADCAST_PREFIX);
	ccn_name_append_str(path, confName.toLocal8Bit().constData());
	ccn_name_append_str(path, "video-list");
	ccn_set_interest_filter(nh->h, path, publishInfo);
}

void SourceAnnouncer::generateSourceInfo() {
	// e.g. /ndn/broadcast/conference/asfd/video-list/kiwi
	struct ccn_charbuf *path = ccn_charbuf_create();
	ccn_name_from_uri(path, (const char *)BROADCAST_PREFIX);
	ccn_name_append_str(path, confName.toLocal8Bit().constData());
	ccn_name_append_str(path, "video-list");
	ccn_name_append_str(path, username.toLocal8Bit().constData());
	
	QString qsInfo; 
	qsInfo.append("<user><username>");
	qsInfo.append(username);
	qsInfo.append("</username>");
	if (!leaving) {
		qsInfo.append("<prefix>");
		qsInfo.append(prefix);
		qsInfo.append("</prefix>");
	} else {
		qsInfo.append("<leave>true</leave>");
	}
	qsInfo.append("</user>");

	QByteArray qba = qsInfo.toLocal8Bit();
	char *buffer = new char[qba.size() + 1];
	memcpy(buffer, qba.constData(), qba.size());
	buffer[qba.size()] = '\0';

	struct ccn_charbuf *content = ccn_charbuf_create();
	
	struct ccn_charbuf *signed_info = ccn_charbuf_create();
	int res = ccn_signed_info_create(signed_info, nh->getPublicKeyDigest(), nh->getPublicKeyDigestLength(), NULL, CCN_CONTENT_DATA, FRESHNESS, NULL, nh->keylocator);
	if (res < 0) {
		fprintf(stderr, "Failed to create signed_info\n");
		abort();
	}
	
	res = ccn_encode_ContentObject(content, path, signed_info, buffer, strlen(buffer), NULL, nh->getPrivateKey());
	ccn_put(nh->h, content->buf, content->length);
	
	ccn_charbuf_destroy(&signed_info);
	ccn_charbuf_destroy(&path);
	ccn_charbuf_destroy(&content);

	delete buffer;

	// reset announce timer
	announceTimer->stop();
	announceTimer->start();
}

void SourceAnnouncer::run() {
	int res = 0;
	while(res >= 0 && bRunning) {
		res = ccn_run(nh->h, 0);
		usleep(100);
	}
}

enum ccn_upcall_res publishInfoCallback(struct ccn_closure *selfp, enum ccn_upcall_kind kind, struct ccn_upcall_info *info) {
	switch(kind) {
	case CCN_UPCALL_INTEREST:
	{
		struct ccn_parsed_interest *pi = info->pi;
		struct ccn_indexbuf *comps = info->interest_comps;
		const unsigned char *buf = info->interest_ccnb;
		int AnswerOriginKind = ccn_fetch_tagged_nonNegativeInteger(CCN_DTAG_AnswerOriginKind, buf, 
									pi->offset[CCN_PI_B_AnswerOriginKind],
									pi->offset[CCN_PI_E_AnswerOriginKind]);
		// accept stale data
		if (AnswerOriginKind == 4) {
			// only response to interests that accept stale data (from the newcomers)
			// if we receive such interests, it means that our (stale) sourceinfo is not cached anymore
			gSourceAnnouncer->generateSourceInfo();
		}
		break;
	}
	default:
		break;
	}
	return (CCN_UPCALL_RESULT_OK);
}


