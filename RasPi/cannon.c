/*
 * cannon.cpp
 *
 *  Created on: Mar 17, 2017
 *      Author: max
 */

#include "config.h"
#include "cannon.h"
#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include "CanInterface.h"
#include "CanMessage.h"
#include <time.h>

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		fprintf(stderr, "got no arg\n");
		// print help
		fprintf(stderr, "Please use the help\n");
	}
	else if(argc == 2)
	{
		initCanInterface(0);

		if(__VERBOSE)
			printf("Got one args\n");
		if(strcmp("ping",argv[1]) == 0)
		{
			printf("Doing broadcast ping");
			fflush(stdout);
			int startTime = clock();

			if(__VERBOSE)
				printf("Start time: %i\n", startTime);

			doBroadcastPing();
			while(clock()-startTime < 50000)
			{
				BlGenericMessage msg;

				int nbytes = receiveGenericMessage(&msg);
				if(nbytes > 0)
				{
					if(msg.commandId == PING_RESPONSE_ID)
					{
						int pingTime = clock() - startTime;
						printf("Found device ID %i after %i ms\n", msg.targetDeviceId, pingTime);
						fflush(stdout);
					}
				}
			}
		}
		else if(strcmp("peek",argv[1]) == 0)
		{
			printf("Listening on CAN bus\n");
			while(1)
			{
				BlGenericMessage msg;

				int nbytes = receiveGenericMessage(&msg);
				if(nbytes > 0)
				{
					printf("%i \t-> %i : %i[", clock(), msg.targetDeviceId, msg.FOF, msg.length);
					int i;
					for(i = 0; i < msg.length; ++i)
					{
						printf("%i ", msg.data[i]);
					}
					printf("]\n");
				}
				usleep(500);
			}
		}
	}
	else if(argc == 3)
	{
		initCanInterface(0);
		if(strcmp("interrupt", argv[1]) == 0)
		{
			sendSignalMessage(strtol(argv[2], NULL, 0), INTERRUPT_ID);
			printf("Sent interrupt\n");
		}
	}
	else if(argc == 4)
	{
		if(__VERBOSE)
			printf("Got three args\n");
		if(strcmp("flash", argv[1]) ==0)
		{
			doFlash(argv[2], argv[3]);
		}
	}
}

void waitForSignal(int deviceId, int statusFlag, int triggerMessage, int triggerRate)
{
	bool nack = true;
	long counter = 0;

	if(__VERBOSE)
		printf("\nWait for signal\n");

	while(nack)
	{
		if(counter % 1000 == 0)
		{
			printf(".");
			fflush(stdout);
		}

		if(triggerMessage != 0)
		{
			if(counter % triggerRate == 0)
			{
				sendSignalMessage(deviceId, triggerMessage);
			}
		}

		BlGenericMessage msg;
		int nbytes = receiveGenericMessage(&msg);

		if(nbytes > 0)
		{
			//printf("Received msg: ID:%x TDI:%x\n", msg.commandId, msg.targetDeviceId);
			if(msg.targetDeviceId == deviceId && msg.commandId == STATUS_ID && msg.data[0] == statusFlag)
			{
				nack = false;
				printf(" ok\n");
				fflush(stdout);
				break;
			}
		}
		++counter;
		usleep(1);
	}
}

void doBroadcastPing()
{
	BlGenericMessage msg;

	msg.FOF = 0;
	msg.commandId = BROADCAST_PING_ID;
	msg.data[0] = CANNON_DEVICE_ID;
	msg.flashPackId = 0;
	msg.length = 1;
	msg.targetDeviceId = 0;

	sendGenericMessage(&msg);
}

void doFlash(char* file, char* device)
{
	int i, j;
	int packsPerSprint = 64;

	int deviceId = strtol(device, NULL, 0);
	printf("Flashing file %s on device %i\n", file, deviceId);

	initCanInterface(deviceId);

	int size = getFileSize(file);
	char* binArray = readFile(file);

	// Very verbose printing of complete bin file
	/*
	int counter = 0;
	for(i = 0; i < size/16; ++i)
	{
		printf("#%x\t", counter);
		for(j = 0; j < 4; ++j)
		{
			printf("[");
			int k;
			for(k = 0; k < 4; ++k)
			{
				printf("%x ", binArray[counter]);
				counter++;
			}
			printf("]\t");
		}
		printf("\n");
	}
	*/

	printf("Waiting for interrupt to be confirmed");
	fflush(stdout);
	// Wait for InterruptConfirm

	waitForSignal(deviceId, STATUS_IN_BOOT_MENU, INTERRUPT_ID, 20000);

	usleep(500000);
	printf("Waiting for flash to be erased");
	fflush(stdout);

	sendSignalMessage(deviceId, INIT_FLASH_ID);

	waitForSignal(deviceId, STATUS_ERASE_FINISHED, 0, 0);

	usleep(50000);

	startFlashing(deviceId, packsPerSprint, size);
	usleep(50000);
	printf("Flashing");
	fflush(stdout);

	int sprintBasePack = 0;
	bool nack = true;

	for(i = 0; i < size/8; ++i)
	{
		usleep(500);

		char pack[8];
		for(j = 0; j < 8; ++j)
		{
			pack[j] = binArray[i*8 + j];
		}
		sendFlashPack(deviceId, i, pack, 8);

		if(i != 0 && i%packsPerSprint == packsPerSprint-1)
		{
			if(__VERBOSE)
				printf("Sprint %i\n", i/packsPerSprint);
			else
				printf(".");
			fflush(stdout);
			usleep(500);
			sendSignalMessage(deviceId, ACK_REQUEST_ID);

			nack = true;
			while(nack)
			{
				BlGenericMessage msg;
				int nbytes = receiveGenericMessage(&msg);
				if(nbytes > 0)
				{
					if(msg.targetDeviceId == deviceId && msg.commandId == ACK_ID)
					{
						nack = false;
						break;
					}
					else if(msg.targetDeviceId == deviceId && msg.commandId == NACK_ID)
					{
						printf("\nReceived NACK!\n");
						int k;
						long sprintFlags = 0;
						for(k = 0; k < msg.length; ++k)
						{
							sprintFlags += msg.data[k] << (k*8);
						}

						for(k = 0; k < packsPerSprint; ++k)
						{
							if(!((sprintFlags>>k)&0x1))
							{
								printf("0");
								//printf("\nResending pack %i\n", sprintBasePack + k);
								fflush(stdout);
								for(j = 0; j < 8; ++j)
								{
									pack[j] = binArray[sprintBasePack + k + j];
								}
								sendFlashPack(deviceId, sprintBasePack + k, pack, 8);
								usleep(500);
							}
							else
							{
								printf("1");
							}
						}
						sendSignalMessage(deviceId, ACK_REQUEST_ID);
						usleep(500);
					}
				}
				usleep(100000);
			}
			sprintBasePack = i+1;
		}
	}

	usleep(500);
	int remainder = size%8;
	if(remainder != 0)
	{
		int base = size-((size/8)*8);
		int i;
		char pack[8];
		for(i = 0; i < remainder; ++i)
		{
			pack[i] = binArray[base + i];
		}
		sendFlashPack(deviceId, size/8, pack, remainder);
	}
	// TODO: add ACK check on end
	printf(" done\n");

	usleep(500000);
	sendSignalMessage(deviceId, END_FLASH_ID);
	usleep(500);
	sendSignalMessage(deviceId, EXIT_FLASH_ID);
	usleep(500);
	sendSignalMessage(deviceId, START_APP_ID);
	printf("starting user app");

	waitForSignal(deviceId, STATUS_STARTING_APP, 0, 0);

	printf("\nThanks for traveling with air penguin!\n");
}

void sendSignalMessage(int deviceId, char command)
{
	BlGenericMessage msg;

	msg.FOF = 0;
	msg.targetDeviceId = deviceId;
	msg.flashPackId = 0;
	msg.commandId = command;
	msg.length = 0;
	sendGenericMessage(&msg);
}

void startFlashing(int deviceId, int packsPerSprint, int size)
{
	BlGenericMessage msg;

	msg.FOF = 0;
	msg.targetDeviceId = deviceId;
	msg.flashPackId = 0;
	msg.commandId = START_FLASH_ID;
	msg.length = 4;
	msg.data[0] = size&0xFF;
	msg.data[1] = (size>>8)&0xFF;
	msg.data[2] = (size>>16)&0xFF;
	msg.data[3] = packsPerSprint&0xFF;
    sendGenericMessage(&msg);
}

void sendFlashPack(int deviceId, int packId, char* data, int len)
{
	/*
	if(packId == 642 || packId == 1548 || len != 8)
	{
		printf("\n\n%i\n%i\n", packId, len);
		int i = 0;
		for(;i < len; ++i)
		{
			printf(" [%x] ", data[i]);
		}
		printf("\n\n");
	}*/
	BlGenericMessage msg;

        msg.FOF = 1;
        msg.targetDeviceId = deviceId;
        msg.flashPackId = packId;
        msg.commandId = 0;
        msg.length = 8;
        int i;
	for(i = 0; i < 8; ++i)
	{
		if(i < len)
		{
			msg.data[i] = data[i];
		}
		else
		{
			msg.data[i] = 0xFF;
		}
	}
        sendGenericMessage(&msg);
}

char* readFile(char* file)
{
	int size;
	size = getFileSize(file);
	FILE* fp;
	fp = fopen(file, "rb");
	char* data = (char*) malloc(sizeof(char)*size);
	fread(data, 1, size, fp);
	int i;
	printf("Data read!\n\n");

	fclose(fp);
	return data;
}

int getFileSize(char* file)
{
	int size;
        FILE* fp;
        fp = fopen(file, "rb");
        fseek(fp, 0L, SEEK_END);
        size = ftell(fp);

        printf("File size: %i\n", size);

        fclose(fp);

	return size;
}

void waitFor (unsigned int secs) {
    unsigned int retTime = time(0) + secs;   // Get finishing time.
    while (time(0) < retTime);               // Loop until it arrives.
}
