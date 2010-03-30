/*
 *  gcc -Wall portaudio_devlist.c -o portaudio_devlist -lportaudio
 */  
#include <stdio.h>
#include <string.h>
#include <portaudio.h>

int main(int argc, char **argv)
{
	int i, c, numDevices;
	const PaDeviceInfo *deviceInfo;
	PaError err;
	char name[256];

	err = Pa_Initialize();
	if (err != paNoError)
		return err;


	numDevices = Pa_GetDeviceCount();
	if (numDevices < 0) {
		return 0;
	}
	if(argc==1){
		printf("usage: %s [input | output]\n", argv[0]);
		return 1;
	}
	for (i = 0; i < numDevices; i++) {
		deviceInfo = Pa_GetDeviceInfo(i);
		memset(name, '\0', sizeof(name));
		for(c=0; c<strlen( deviceInfo->name); c++){
			if( deviceInfo->name[c] == ' ')
				name[c]='_';
			else
				name[c]= deviceInfo->name[c];
		}
		if( !strcmp(argv[1], "input")&& deviceInfo->maxInputChannels)
		{
			printf("%d \"%s\" \n",
					i, 
					name);
		}
		else if( !strcmp(argv[1], "output")&& deviceInfo->maxOutputChannels)
		{
			printf("%d \"%s\" \n",
					i, 
					name);
		}
	}

	return numDevices;
}


