#include "wavlog.h"

WavHeader::WavHeader() {
	chunk.id = "RIFF";
	chunk.format = "WAVE";

	fmt.format = "fmt "; // tricky; don't forget the last whitespace
	fmt.size = 16;	// PCM
	fmt.audioFormat = 1; // PCM
	fmt.bitsPerSample = 16;

	dataHeader.dataTag = "data";
}

void WavHeader::setNumChannels(int n) {
	this->fmt.nChannels = n;
	updateFmt();
}

void WavHeader::setSampleRate(int sr) {
	this->fmt.sampleRate = sr;
	updateFmt();
}

void WavHeader::updateFmt() {
	if (fmt.nChannels != 0) {
		this->fmt.blockAlign = fmt.nChannels * fmt.bitsPerSample / 8;
	}
	if (fmt.nChannels != 0 && fmt.sampleRate != 0) {
		this->fmt.byteRate = fmt.sampleRate * fmt.nChannels * fmt.bitsPerSample / 8;

	}
}

void WavHeader::setDataSize(int size) {
	this->dataHeader.size = size;
	this->chunk.size = size + 36;
}

std::ostream& operator << (std::ostream& os, const WavHeader& header) {
	os << header.chunk.id.c_str();
	os.write((const char *) &(header.chunk.size), 4);
	os << header.chunk.format.c_str();

	os << header.fmt.format.c_str();
	os.write((const char *) &(header.fmt.size), 4);
	os.write((const char *) &(header.fmt.audioFormat), 2);
	os.write((const char *) &(header.fmt.nChannels), 2);
	os.write((const char *) &(header.fmt.sampleRate), 4);
	os.write((const char *) &(header.fmt.byteRate), 4);
	os.write((const char *) &(header.fmt.blockAlign), 2);
	os.write((const char *) &(header.fmt.bitsPerSample), 2);

	os << header.dataHeader.dataTag.c_str();
	os.write((const char *) &(header.dataHeader.size), 4);
	return os;
}

void appendWavHeader(string filename, int nChannels, int sampleRate){
	filename = "/var/tmp/" + filename;
	ifstream in;
	in.open(filename.c_str(), ios::in | ios::binary);
	string wavFilename = filename + ".wav";
	ofstream out;
	out.open(wavFilename.c_str(), ios::out | ios::binary);
	WavHeader h;
	h.setNumChannels(nChannels);
	h.setSampleRate(sampleRate);
	struct stat fileStats;
	int padding;
	stat (filename.c_str(), &fileStats);
	padding = fileStats.st_size % h.getBlockAlign();
	h.setDataSize(fileStats.st_size - padding);
	out << h;
	out << in.rdbuf();
	in.close();
	out.close();
}

void logWav(string filename, short *frame, int nsamp) {
	filename = "/var/tmp/" + filename;
	ofstream out;
	out.open(filename.c_str(), ios::out | ios::binary | ios::app);
	for (int i = 0; i < nsamp; i ++) {
		out.write((const char *) &(frame[i]), 2);
	}
	out.close();
}
