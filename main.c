/*
This one is to read RFID card by PN532 through SPI, respond to LED lights(P9813) and a beeper

Input:	PN532(SPI)
Output:	P9813, GPIO 1 port 

NFC module:	http://www.seeedstudio.com/wiki/NFC_Shield_V2.0
LED light:	http://www.seeedstudio.com/wiki/Grove_-_Chainable_RGB_LED
beeper:		http://item.taobao.com/item.htm?spm=0.0.0.0.A0Zkth&id=6859691900


Outline:
Repeatedly read from NFC, use cardID to identify,	// currently I don't know how to read the blocks.
if it matches some fixed number, it shows green light and beepGPIO;
otherwise, it shows red and beepGPIO another sound.

You need the wiringPi lib.

Compile: 
make

Run:
sudo ./NFClight


Thanks the following persons developed the libs which this project used.
wiringPi lib from:	Gordons Projects @ https://projects.drogon.net/raspberry-pi/wiringpi/
nfc lib from:		Katherine @ http://blog.iteadstudio.com/to-drive-itead-pn532-nfc-module-with-raspberry-pi/

This project is created by @DaochenShi (shidaochen@live.com)
*/


#include "PN532SPI.h"

#include <stdio.h>
#include <unistd.h>	// for usleep
#include <signal.h>	// for SIG_IGN etc.
#include <fcntl.h>	// for O_WRONLY
#include <errno.h>	// for errno
#include <time.h>	// for clock()
#include <pthread.h>	// for multithreading

//#define DEBUG

//#define BEEPER_GPIO_PIN	6
#define LED1	0
#define LED2	2
#define LED3	3

int initialWiringPi();

void break_program(int sig);
void* adjust_color();
//#define MAXWAITINGTIME 10
static int loopingStatus = 1;
static unsigned char colorBase[3];
static unsigned char colorTarget[3];

const uint32_t authCID = 0xFFFFFFFF;
const uint32_t exitCID = 0x77777777;

int main(int argc, const char* argv[])
{

	// NFC related, the read card ID. Currently I can only read this. I don't know how to get other infos.
	uint32_t cardID;

	// run this program in background when not in Debug mode
#ifndef DEBUG
	{
		pid_t pid, sid;
		pid = fork();
		if (pid < 0)
		{
			exit(EXIT_FAILURE);
		}
		if (pid > 0)
		{
			exit(EXIT_SUCCESS);
		}
		
		// change file mode mask
		umask(0);
		// open logs,,, but I did not record any logs
		
		// create SID for child process
		sid = setsid();
		if (sid < 0)
		{
			exit(EXIT_FAILURE);
		}
		
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
	}
#endif	
	
	initialWiringPi();
	pthread_t _colorThread;

	pthread_create(&_colorThread, NULL, adjust_color, NULL);

	printf("Initialized...\n");
	while(loopingStatus)
	{
#ifdef DEBUG
		printf("waiting for card read...\n");
#endif

		cardID = readPassiveTargetID(PN532_MIFARE_ISO14443A);

		if (cardID ==  authCID)
		{
			colorBase[0] = 0xFF; colorBase[1] = 0xFF; colorBase[2] = 0xFF;
			colorTarget[0] = 0xFF; colorTarget[1] = 0xFF; colorTarget[2] = 0xFF; 
			setColorRGBbuffered(colorBase[0], colorBase[1], colorBase[2]);
		}
		else
		{
			if (cardID == exitCID)
			{
				loopingStatus = 0;
				break;
			}
			colorTarget[0] = 0; colorTarget[1] = 0; colorTarget[2] = 0; 
#ifdef DEBUG
		printf("cardID = %X\n", cardID);
#endif
		}
		sleep(1);

	}
	colorTarget[0] = 0; colorTarget[1] = 0; colorTarget[2] = 0; 
	colorBase[0] = 0; colorBase[1] = 0; colorBase[2] = 0;
	setColorRGBbuffered(0, 0, 0);
	sleep(2);
	
	return 0;
}

// Set-up some necessary wiringPi and devices.
int initialWiringPi()
{
#ifdef DEBUG
	printf("Initializing.\n");
#endif

	uint32_t nfcVersion;
	
	// First to setup wiringPi
	if (wiringPiSetup() < 0)
	{
		fprintf(stderr, "Unable to setup wiringPi: %s \n", strerror(errno));
		exit(1);
	}
#ifdef DEBUG
	printf("Set wiringPi.\n");
#endif


	// Blank leds
	setColorRGBbuffered(0, 0, 0);
#ifdef DEBUG
	printf("LED blacked.\n");
#endif

	// Hook crtl+c event
	signal(SIGINT, break_program);
#ifdef DEBUG
	printf("Hooked Ctrl+C.\n");
#endif

	// Setting the pure white LED
	pinMode(LED1, OUTPUT);
	pinMode(LED2, OUTPUT);
	pinMode(LED3, OUTPUT);
#ifdef DEBUG
	printf("Pure white LED set.\n");
#endif

	// Use init function from nfc.c
	// why you use such a simple function name?!
	initialPN532SPI();
#ifdef DEBUG
	printf("NFC initialized begin().\n");
#endif
	nfcVersion = getFirmwareVersion();
	if (!nfcVersion)
	{
		fprintf(stderr, "Cannot find PN53x board after getFirmwareVersion.\n");
		exit(1);
	}

	SAMConfig();
	
	
#ifdef DEBUG
	printf("Initialized.\n");
#endif
	return 0;
}

// Accept Ctrl+C command, this seems not work when main process is forked.
void break_program(int sig)
{
	signal(sig, SIG_IGN);
	loopingStatus = 0;
	printf("Program end.\n");
	signal(sig, SIG_DFL);
}

// Adjust the P9813 color by another thread.
void* adjust_color(void)
{
	// LED related	
	int colorMaxDiff;
	while (loopingStatus){
		/// Parse current color, and gradually fade-in/fade-out
#ifdef DEBUG
		printf("Changing color...loop = %d\n", loopingStatus);
#endif
		colorMaxDiff = 0;
		colorMaxDiff = (colorMaxDiff > abs(colorBase[0] - colorTarget[0])) ? colorMaxDiff : abs(colorBase[0] - colorTarget[0]);
		colorMaxDiff = (colorMaxDiff > abs(colorBase[1] - colorTarget[1])) ? colorMaxDiff : abs(colorBase[1] - colorTarget[1]);
		colorMaxDiff = (colorMaxDiff > abs(colorBase[2] - colorTarget[2])) ? colorMaxDiff : abs(colorBase[2] - colorTarget[2]);
		

		//colorMaxDiff = (colorMaxDiff > 15) ? colorMaxDiff/16 : colorMaxDiff;
#ifdef DEBUG
		printf("colorMaxDiff = %d, %dR->%dR, %dG->%dG, %dB->%dB\n",
		colorMaxDiff,
		colorBase[0], colorTarget[0],
		colorBase[1], colorTarget[1],
		colorBase[2], colorTarget[2]);
#endif
		if (colorMaxDiff)
		{
			{
				colorBase[0] = colorBase[0] - (colorBase[0] - colorTarget[0]) / colorMaxDiff;
				colorBase[1] = colorBase[1] - (colorBase[1] - colorTarget[1]) / colorMaxDiff;
				colorBase[2] = colorBase[2] - (colorBase[2] - colorTarget[2]) / colorMaxDiff;
				setColorRGBbuffered(colorBase[0], colorBase[1], colorBase[2]);
			}
		}
		usleep(200 * 1000);
	}
}
