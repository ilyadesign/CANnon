
/*
 * target_device.h
 *
 *  Created on: 13.03.2017
 *      Author: MPARTENF
 */

#ifndef APPLICATION_INC_TARGET_DEVICE_H_
#define APPLICATION_INC_TARGET_DEVICE_H_

#include "config.h"
#include "stdbool.h"
#include "link_layer.h"

/**
 * Command ID defines start
 */
#define SINGLE_PING_ID		0x00 // INCOMING <--
#define BROADCAST_PING_ID	0x01 // INCOMING <--
#define PING_RESPONSE_ID	0x02 // OUTGOING   -->
#define INTERRUPT_ID		0x03 // INCOMING <--
#define GET_VERSION_ID		0x04 // INCOMING <--
#define SEND_VERSION_ID		0x05 // OUTGOING   -->
#define GET_NAME_ID			0x06 // INCOMING <--
#define SEND_NAME_ID		0x07 // OUTGOING   -->
#define INIT_FLASH_ID		0x08 // INCOMING <--
#define ACK_REQUEST_ID		0x09 // INCOMING <--
#define ACK_ID				0x0A // OUTGOING   -->
#define NACK_ID				0x0B // OUTGOING   -->
#define EXIT_FLASH_ID		0x0C // INCOMING <--
#define GET_CRC_ID			0x0D // INCOMING <--
#define SEND_CRC_ID			0x0E // OUTGOING   -->
#define START_FLASH_ID		0x0F // INCOMING <--
#define END_FLASH_ID		0x10 // INCOMING <--
#define STATUS_ID			0x11 // OUTGOING   -->
#define GET_CHIP_ID			0x12 // INCOMING <--
#define SEND_CHIP_ID		0x13 // Outgoing   -->
#define START_APP_ID		0xFF // INCOMING <--
/**
 * Command ID defines end
 */

/**
 * Error Codes
 */
#define ERRCODE_OK 						0U
#define ERRCODE_NO_FLASH_PROCESS 		1U
#define ERRCODE_NOT_IN_FLASH_MODE 		2U
#define ERRCODE_NOT_IN_BOOT_MENU		3U
#define ERRCODE_ALREADY_FLASHING		4U
#define STATUS_IN_BOOT_MENU				5U
#define STATUS_START_ERASE				6U
#define STATUS_ERASE_FINISHED			7U
#define STATUS_FLASH_START				8U
#define STATUS_FLASH_DONE				9U
#define STATUS_STARTING_APP				10U
/*
 * Error Codes end
 */

/**
 * Private Variables
 */
bool bootMenu;
bool flashMode;
bool inFlashProcess;

uint32_t packCount;			// Total amount of packs for flash
uint32_t packCounter;		// Current pack
uint8_t sprintPackCount;	// Number of packs per sprint
uint8_t sprintPackCounter;	// Current pack in sprint
uint64_t sprintFlags; 		// 64Bit number flagging arrived packs


/**
 * Function definitions
 */
void processBlMessage(BlGenericMessage* msg);
void sendPingResponse(BlGenericMessage* msg);
void enterBootMenu();
void sendVersion();
void sendName();
void initFlashMode();
void exitFlashMode();
void startFlashing(BlGenericMessage* msg);
void stopFlashing();
void sendStatus(uint8_t statusCode);
void checkSprint();
void sendCRC();
void sendChipId();
void startApplication();

void tryToWriteFlash(BlGenericMessage* msg);
void sendAck();
void sendNack(uint64_t sprintFlags);

#endif /* APPLICATION_INC_TARGET_DEVICE_H_ */
