#ifndef WAVLOG_H
#define WAVLOG_H
#include <iostream>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
using namespace std;

struct ChunkDesc {
	string id;
	int size;
	string format;
};

struct FmtDesc {
	string format;
	int size;
	short audioFormat;
	short nChannels;
	int sampleRate;
	int byteRate;
	short blockAlign;
	short bitsPerSample;
}; 

struct DataDesc {
	string dataTag;
	int size;
};

class WavHeader{
private:
	struct ChunkDesc chunk;
	struct FmtDesc fmt;
	struct DataDesc dataHeader; 

public:
	WavHeader();
	void setNumChannels(int n); 
	void setSampleRate(int sr); 
	void updateFmt();
	void setDataSize(int size); 
	int getBlockAlign() { return fmt.blockAlign; }
friend std::ostream& operator << (std::ostream& os, const WavHeader& header);
};
std::ostream& operator << (std::ostream& os, const WavHeader& header);
void appendWavHeader(string filename, int nChannels, int sampleRate);

#define BUFFER_SIZE 200000
class CircularBuffer{
private:
	int writePtr;
	int readPtr;
	long readBytes;
	long writeBytes;
	char buf[BUFFER_SIZE];
	FILE *fp;
	bool isFull() {return (writeBytes - readBytes >= BUFFER_SIZE); }
	bool isEmpty() { return (writeBytes == readBytes); }
	void update(char rw, int bytes); 

public:
	CircularBuffer(char *file);
	~CircularBuffer();
	void writeToBuffer(char *data, size_t len);
	int readToFile();
	void log(char *msg);
};



#endif
