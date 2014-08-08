//
//  Weather update client in C++
//  Connects SUB socket to tcp://localhost:5556
//  Collects weather updates and finds avg temp in zipcode
//
//  Olivier Chamoux <olivier.chamoux@fr.thalesgroup.com>
//
#include <iostream>
#include <sstream>
#include <sys/time.h>
#include <unistd.h>
#include <deque>
#include <vector>
#include <algorithm>
#include <thread> 
#include <portaudio.h>
#include <boost/asio.hpp>

#include "chunk.h"
#include "utils.h"
#include "stream.h"

using boost::asio::ip::tcp;
using namespace std;


int bufferMs;
int deviceIdx;  
Stream* stream;



void player() 
{
	try
	{
		boost::asio::io_service io_service;
		tcp::resolver resolver(io_service);
		tcp::resolver::query query(tcp::v4(), "192.168.0.2", "98765");
		tcp::resolver::iterator iterator = resolver.resolve(query);

		while (true)
		{
			try
			{
cout << "connect\n";
				tcp::socket s(io_service);
				s.connect(*iterator);
				void* wireChunk = (void*)malloc(sizeof(WireChunk));
				while (true)
				{
					size_t toRead = sizeof(WireChunk);
					size_t len = 0;
					do 
					{
						len += s.read_some(boost::asio::buffer((char*)wireChunk + len, toRead));
						toRead = sizeof(WireChunk) - len;
//						cout << "len: " << len << "\ttoRead: " << toRead << "\n";
					}
					while (toRead > 0);
					stream->addChunk(new Chunk((WireChunk*)wireChunk));
				}
			}
			catch (std::exception& e)
			{
				std::cerr << "Exception: " << e.what() << "\n";
				usleep(500*1000);
			}
		}
	}
	catch (std::exception& e)
	{
		std::cerr << "Exception: " << e.what() << "\n";
	}
}




/* This routine will be called by the PortAudio engine when audio is needed.
** It may called at interrupt level on some machines so don't do anything
** that could mess up the system like calling malloc() or free().
*/
static int paStreamCallback( const void *inputBuffer, void *outputBuffer,
                            unsigned long framesPerBuffer,
                            const PaStreamCallbackTimeInfo* timeInfo,
                            PaStreamCallbackFlags statusFlags,
                            void *userData )
{
    Stream* stream = (Stream*)userData;
    short* out = (short*)outputBuffer;

    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
	stream->getPlayerChunk(out, timeInfo->outputBufferDacTime, framesPerBuffer);
    return paContinue;
}



int initAudio()
{
    PaStreamParameters outputParameters;
    PaStream *paStream;
    PaError err;
    
    printf("PortAudio Test: output sine wave. SR = %d, BufSize = %d\n", SAMPLE_RATE, FRAMES_PER_BUFFER);
    
    err = Pa_Initialize();
    if( err != paNoError ) goto error;

	int numDevices;
	numDevices = Pa_GetDeviceCount();
	if( numDevices < 0 )
	{
		printf( "ERROR: Pa_CountDevices returned 0x%x\n", numDevices );
		err = numDevices;
		goto error;
	}
	const PaDeviceInfo *deviceInfo;
	for(int i=0; i<numDevices; i++)
	{
	    deviceInfo = Pa_GetDeviceInfo(i);
		std::cerr << "Device " << i << ": " << deviceInfo->name << "\n";
	}

    outputParameters.device = deviceIdx==-1?Pa_GetDefaultOutputDevice():deviceIdx; /* default output device */
    std::cerr << "Using Device: " << outputParameters.device << "\n";
    if (outputParameters.device == paNoDevice) 
	{
      fprintf(stderr,"Error: No default output device.\n");
      goto error;
    }
    outputParameters.channelCount = CHANNELS;       /* stereo output */
    outputParameters.sampleFormat = paInt16; /* 32 bit floating point output */
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
std::cerr << "HighLatency: " << outputParameters.suggestedLatency << "\t LowLatency: " << Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency << "\n";
    err = Pa_OpenStream(
              &paStream,
              NULL, /* no input */
              &outputParameters,
              SAMPLE_RATE,
              paFramesPerBufferUnspecified, //FRAMES_PER_BUFFER,
              paClipOff,      /* we won't output out of range samples so don't bother clipping them */
              paStreamCallback,
              stream );
    if( err != paNoError ) goto error;

    err = Pa_StartStream( paStream );
    if( err != paNoError ) goto error;

//    err = Pa_StopStream( paStream );
//    if( err != paNoError ) goto error;

//    err = Pa_CloseStream( paStream );
//    if( err != paNoError ) goto error;

//    Pa_Terminate();
//    printf("Test finished.\n");
    
    return err;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return err;
}




int main (int argc, char *argv[])
{
        deviceIdx = -1;
	bufferMs = 300;	
	if (argc > 1)
		bufferMs = atoi(argv[1]);
	if (argc > 2)
		deviceIdx = atoi(argv[2]);

	stream = new Stream();
	stream->setBufferLen(bufferMs);
	initAudio();
	std::thread playerThread(player);
	
	std::string cmd;
	while (true && (argc > 3))
	{
		std::cout << "> ";
		std::getline(std::cin, cmd);
//		line = fgets( str, 256, stdin );
//		if (line == NULL)
//			continue;
//		std::string cmd(line);
		std::cerr << "CMD: " << cmd << "\n";
		if (cmd == "quit")
			break;
		else
		{
			stream->setBufferLen(atoi(cmd.c_str()));
		}
	}

	playerThread.join();

    return 0;
}


