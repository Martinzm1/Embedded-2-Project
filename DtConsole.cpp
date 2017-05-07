/*-----------------------------------------------------------------------

PROGRAM: DtConsole.cpp

PURPOSE:
Open Layers data acquisition example showing how to implement a
continuous analog input operation using windows messaging in a
console environment.

Project Properties > Linker > Additional Dependencies > Ws2_32.lib
Project Properties > C/C++ > Preprocessor > Preprocessor Definitions > _CRT_SECURE_NO_WARNINGS

****************************************************************************/
#include <string.h>
#include <stdio.h>
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <WinSock.h>
#include <time.h>
#include "oldaapi.h"

#define CHECKERROR( ecode )                           \
	do {                                                  \
	ECODE olStatus;                                    \
	if( OLSUCCESS != ( olStatus = ( ecode ) ) ) {      \
	printf("OpenLayers Error %d\n", olStatus );     \
	exit(1);                                        \
	}                                                  \
	}                                                     \
	while(0)      

#define NUM_OL_BUFFERS	4
#define ERR_CODE_NONE	0	/* no error */
#define ERR_CODE_SWI	1	/* software error */
#define CMD_LENGTH	5
#define ARG_NONE	1
#define ARG_NUMBER	2
#define noop ((void)0)
#define STRLEN 80        /* string size for general text manipulation   */
#define BUF_LEN			1000
#define B_COEF			101

typedef struct sp_comm {
	WSADATA wsaData;
	SOCKET cmdrecvsock;
	SOCKET cmdstatusock;
	SOCKET datasock;
	struct sockaddr_in server;
} *sp_comm_t;

typedef struct sp_flags {
	unsigned int start_system : 1;
	unsigned int pause_system : 1;
	unsigned int shutdown_system : 1;
	unsigned int analysis_started : 1;
	unsigned int restart : 1;
	unsigned int transmit_data : 1;
} *sp_flags_t;

typedef struct sp_struct {
	struct sp_comm		comm;
	struct sp_flags		flags;
} *sp_struct_t;

typedef struct {
	char cmd[CMD_LENGTH];
	int arg;
} cmd_struct_t;

char str[STRLEN];        /* global string for general text manipulation */
int counter = 0;
WSADATA wsaData;
DBL *arrData;
DBL *filData;
int x = 1;
int socketState = 1;
int channelState = 0;
int endSwitch = 0;
float sampleRate = 0.0;
char filename[40];
char dataFile[40];
char filFile[40];
int transmit = 0;
int bootState=0;
char strSend[20];
double d_coef[B_COEF];
FILE *fpD, *fpFD;
HANDLE hClientThread; // Thread to interface with the ProfileClient
DWORD dwClientThreadID;

// Function declarations
VOID client_iface_thread(LPVOID parameters);
void filterSignal(double *d_samples, double *d_coef, double *d_fsignal);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM hAD, LPARAM lParam);
BOOL CALLBACK EnumBrdProc(LPSTR lpszBrdName, LPSTR lpszDriverName, LPARAM lParam);

/************************************************************************************
* VOID client_iface_thread(LPVOID)
* Description: Thread communicating commands from client and the status of their
*				completion back to client.
*
* Receives filename from socket, stores it, then receives sample rate and stores it.
*
************************************************************************************/
VOID client_iface_thread(LPVOID parameters) {
	sp_struct_t profiler = (sp_struct_t)parameters;
	sp_comm_t comm = &profiler->comm;
	INT retval;
	struct sockaddr_in saddr;
	int saddr_len;
	char ParamBuffer[110] = { 0 };
	int filterCount = 0;

	while (ParamBuffer[0] != '!') {
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len = sizeof(saddr);
		retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
		switch (socketState) {
		case 1:
			strcpy(filename, ParamBuffer);
			socketState = 2;
			break;
		case 2:
			sampleRate = atof(ParamBuffer);
			socketState = 3;
			break;
		case 3:
			if (filterCount < (B_COEF - 2)) {
				d_coef[filterCount] = atof(ParamBuffer);
				filterCount++;
			}
			else {
				socketState = 4;
			}
			break;
		case 4:
			break;
		}
	}
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM hAD, LPARAM lParam) {
	DBL min = 0, max = 0;
	DBL voltsA;
	ULNG valueA;
	ULNG *rawData;
	ULNG samples;
	UINT encoding = 0, resolution = 0;
	double tVal = 2.51;
	PWORD  pBuffer = NULL;
	switch (msg) {
	case OLDA_WM_BUFFER_DONE:
		HBUF hBuf;
		//	Channel 0 pBuffer[samples - 2]
		//	Channel 1 pBuffer[samples - 1]

		olDaGetBuffer((HDASS)hAD, &hBuf);
		// Check CONTADC.C > Input Box > if( hBuffer )
		/* get sub system information for code/volts conversion */
		olDaGetRange((HDASS)hAD, &max, &min);
		olDaGetResolution((HDASS)hAD, &resolution);

		/* get max samples in input buffer */
		olDmGetValidSamples(hBuf, &samples);

		/* get pointer to the buffer */
		olDmGetBufferPtr(hBuf, (LPVOID*)&pBuffer);
		if(pBuffer== NULL){
			break;
		}
		printf("Samples :%lu\n", samples);
		rawData=(ULNG *) malloc(sizeof(ULNG)*sampleRate);
		if (channelState == 0) { // if waiting for channel 0 
			valueA = pBuffer[samples - 2];
			voltsA = ((float)max - (float)min) / (1L << resolution)*valueA + (float)min;

		}
		else if (channelState == 1) { // if switch has been activated
			for(int i=1; i<(long)samples; i=i+2){
				rawData[(i-1)/2]=pBuffer[i];
			}
		}

		/* display value */
		if (channelState == 0) { // if waiting for switch
			if (voltsA > tVal) {
				sprintf(str, "Channel 0: %.2f Status: ON ", voltsA);
				channelState = 1;
				bootState=1;
			}
			else {
				if(bootState==1){
					endSwitch = 1;
				}
				sprintf(str, "Channel 0: %.2f Status: OFF ", voltsA);
			}
		}
		else { // if switch activated
			   // Store to array from buffer
			while (counter < (sampleRate)) {
				arrData[counter] = ((float)max - (float)min) / (1L << resolution)*rawData[counter] + (float)min;
				fprintf(fpD, "%f\n", arrData[counter]);
				printf("%d: %lf\n", counter, arrData[counter]);
				counter++;
			}
		}
		if(counter >= (sampleRate)) {
			channelState = 0; // now look back at the switch
			counter = 0;
			transmit = 1;
			filterSignal(arrData, d_coef, filData); //data filtered
		}
		puts(str);
		free(rawData);
		olDaPutBuffer((HDASS)hAD, hBuf);
		break;

	case OLDA_WM_QUEUE_DONE:
		printf("\nAcquisition stopped, rate too fast for current options.");
		PostQuitMessage(0);
		break;

	case OLDA_WM_TRIGGER_ERROR:
		printf("\nTrigger error: acquisition stopped.");
		PostQuitMessage(0);
		break;

	case OLDA_WM_OVERRUN_ERROR:
		printf("\nInput overrun error: acquisition stopped.");
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, msg, hAD, lParam);
	}
	return 0;
}


BOOL CALLBACK EnumBrdProc(LPSTR lpszBrdName, LPSTR lpszDriverName, LPARAM lParam) {
	// Make sure we can Init Board
	if (OLSUCCESS != (olDaInitialize(lpszBrdName, (LPHDEV)lParam))) {
		return TRUE;  // try again
	}

	// Make sure Board has an A/D Subsystem 
	UINT uiCap = 0;
	olDaGetDevCaps(*((LPHDEV)lParam), OLDC_ADELEMENTS, &uiCap);
	if (uiCap < 1) {
		return TRUE;  // try again
	}

	printf("\n%s succesfully initialized.\n", lpszBrdName);
	return FALSE;    // all set , board handle in lParam
}


int main() {
	// start of TX
	struct sp_struct profiler;
	struct sockaddr_in saddr;
	struct hostent *hp;
	int res = 0;
	char ParamBuffer[110] = { 0 };
	char inputChar[100] = "";
	double d_fsignal[BUF_LEN + B_COEF - 1] = { 0 };
	memset(&profiler, 0, sizeof(profiler));
	sp_comm_t comm = &profiler.comm;

	if ((res = WSAStartup(0x202, &wsaData)) != 0) {
		fprintf(stderr, "WSAStartup failed with error %d\n", res);
		WSACleanup();
		return(ERR_CODE_SWI);
	}

	/**********************************************************************************
	* Setup data transmition socket to broadcast data
	**********************************************************************************/

	hp = (struct hostent*)malloc(sizeof(struct hostent));
	hp->h_name = (char*)malloc(sizeof(char) * 17);
	hp->h_addr_list = (char**)malloc(sizeof(char*) * 2);
	hp->h_addr_list[0] = (char*)malloc(sizeof(char) * 5);
	strcpy(hp->h_name, "lab_example\0");
	hp->h_addrtype = 2;
	hp->h_length = 4;

	//broadcast in 255.255.255.255 network 	
	hp->h_addr_list[0][0] = (signed char)255;//192;129
	hp->h_addr_list[0][1] = (signed char)255; //168;107
	hp->h_addr_list[0][2] = (signed char)255; //0; 255
	hp->h_addr_list[0][3] = (signed char)255; //140;255
	hp->h_addr_list[0][4] = 0;

	/**********************************************************************************
	* Setup a socket for broadcasting data
	**********************************************************************************/
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = hp->h_addrtype;
	memcpy(&(saddr.sin_addr), hp->h_addr, hp->h_length);
	saddr.sin_port = htons(1024);

	if ((comm->datasock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		fprintf(stderr, "socket(datasock) failed: %d\n", WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_NONE);
	}

	if (connect(comm->datasock, (struct sockaddr*)&saddr, sizeof(saddr)) == SOCKET_ERROR) {
		fprintf(stderr, "connect(datasock) failed: %d\n", WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_SWI);
	}

	/**********************************************************************************
	* Setup and bind a socket to listen for commands from client
	**********************************************************************************/
	memset(&saddr, 0, sizeof(struct sockaddr_in));
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(1500);
	if ((comm->cmdrecvsock = socket(AF_INET, SOCK_DGRAM, 0)) == INVALID_SOCKET) {
		fprintf(stderr, "socket(cmdrecvsock) failed: %d\n", WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_NONE);
	}

	if (bind(comm->cmdrecvsock, (struct sockaddr*)&saddr, sizeof(saddr)) == SOCKET_ERROR) {
		fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
		WSACleanup();
		return(ERR_CODE_NONE);
	}

	hClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_iface_thread, (LPVOID)&profiler, 0, &dwClientThreadID);
	SetThreadPriority(hClientThread, THREAD_PRIORITY_LOWEST);
	while (socketState != 4) {
		noop;
	}
	printf("Rate, filter coeffecients and filename recieved and stored\n");
	printf("%s %f", filename, sampleRate);
	sprintf(dataFile, "%s_data.txt", filename);
	sprintf(filFile, "%s_filtered.txt", filename);
	fpD = fopen(dataFile, "a");
	fpFD = fopen(filFile, "a");

	arrData = (DBL *)malloc(sizeof(DBL) * sampleRate);
	filData = (DBL*)malloc(sizeof(DBL)*((sampleRate) + B_COEF-1));
	// turn on LEDS

	// end of TX

	// create a window for messages
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));

	// Set Window wc, message from WndProc
	wc.lpfnWndProc = WndProc;

	// Set wc window title
	wc.lpszClassName = "DtConsoleClass";
	RegisterClass(&wc);

	HWND hWnd = CreateWindow(wc.lpszClassName,
		NULL,
		NULL,
		0, 0, 0, 0,
		NULL,
		NULL,
		NULL,
		NULL);

	if (!hWnd)
		exit(1);

	HDEV hDev = NULL;
	CHECKERROR(olDaEnumBoards(EnumBrdProc, (LPARAM)&hDev));

	HDASS hAD = NULL;
	HDASS hAD2 = NULL;
	CHECKERROR(olDaGetDASS(hDev, OLSS_AD, 0, &hAD));

	CHECKERROR(olDaSetWndHandle(hAD, hWnd, 0));

	CHECKERROR(olDaSetDataFlow(hAD, OL_DF_CONTINUOUS));
	CHECKERROR(olDaSetChannelListSize(hAD, 2)); // 2 Channels
												// Channel setup
	CHECKERROR(olDaSetChannelListEntry(hAD, 0, 0)); // Channel 0
	CHECKERROR(olDaSetChannelListEntry(hAD, 1, 1)); // Channel 1
	CHECKERROR(olDaSetGainListEntry(hAD, 0, 1));
	CHECKERROR(olDaSetGainListEntry(hAD, 1, 1));
	CHECKERROR(olDaSetTrigger(hAD, OL_TRG_SOFT));
	CHECKERROR(olDaSetClockSource(hAD, OL_CLK_INTERNAL));
	CHECKERROR(olDaSetClockFrequency(hAD, sampleRate));
	CHECKERROR(olDaSetWrapMode(hAD, OL_WRP_NONE));
	CHECKERROR(olDaConfig(hAD));

	///* get handle to DOUT sub system  for LED turn on*/
	CHECKERROR(olDaGetDASS(hDev, OLSS_DOUT, 0, &hAD2));
	CHECKERROR(olDaSetDataFlow(hAD2, OL_DF_SINGLEVALUE));
	CHECKERROR(olDaConfig(hAD2));
	CHECKERROR(olDaPutSingleValue(hAD2, 1, 0, 1.0));

	HBUF hBufs[NUM_OL_BUFFERS];
	for (int i = 0; i < NUM_OL_BUFFERS; i++) {
		if (OLSUCCESS != olDmAllocBuffer(GHND, 2000, &hBufs[i])) {
			for (i--; i >= 0; i--) {
				olDmFreeBuffer(hBufs[i]);
			}
			exit(1);
		}
		olDaPutBuffer(hAD, hBufs[i]);
	}

	if (OLSUCCESS != (olDaStart(hAD))) {
		printf("A/D Operation Start Failed...hit any key to terminate.\n");
	}
	else {
		printf("A/D Operation Started...hit any key to terminate.\n\n");
	}

	MSG msg;
	SetMessageQueue(50);

	// Increase the our message queue size so
	// we don't lose any data acq messages
	// Acquire and dispatch messages until a key is hit...since we are a console app 
	// we are using a mix of Windows messages for data acquistion and console approaches
	// for keyboard input.
	// GetMessage( message structure, handle of window receiving the message, lowest message to examine, highest message to examine)
	while (GetMessage(&msg, hWnd, 0, 0)) {
		TranslateMessage(&msg);    // Translates virtual key codes       
		DispatchMessage(&msg);     // Dispatches message to window 
		if (transmit == 1) {
			sprintf(strSend, "startBuf");
			send(comm->datasock, strSend, sizeof(strSend), 0);
			while (counter < BUF_LEN) {
				printf("Transmitting back to client %lf\n", filData[counter]);
				sprintf(strSend, "%lf", filData[counter]);
				fprintf(fpFD, "%s\n", strSend);
				send(comm->datasock, strSend, sizeof(strSend), 0);
				counter++;
			}
			sprintf(strSend, "endBuf");
			send(comm->datasock, strSend, sizeof(strSend), 0);
			printf("%d\n", counter);
			transmit = 0;
			counter = 0;
		}
		if ( endSwitch) { //if switch is turned off.
			PostQuitMessage(0);
		}
	}
	CHECKERROR(olDaPutSingleValue(hAD2, 0, 0, 1.0));
	// abort A/D operation
	olDaAbort(hAD);
	olDaAbort(hAD2);
	printf("\nA/D Operation Terminated \n");
	sprintf(strSend, "FINAL");
	send(comm->datasock, strSend, sizeof(strSend), 0);
	free(arrData);
	free(filData);
	fclose(fpD);
	fclose(fpFD);
	for (int i = 0; i < NUM_OL_BUFFERS; i++) {
		olDmFreeBuffer(hBufs[i]);
	}
	CHECKERROR(olDaPutSingleValue(hAD2, 0, 0, 1.0));
	CHECKERROR(olDaReleaseDASS(hAD2));
	olDaTerminate(hDev);
	exit(0);
}

void filterSignal(double *d_samples, double *d_coef, double *d_fsignal) {
	int i, j;
#pragma omp parallel
	for (j = 0; j < BUF_LEN; j++) {
		d_fsignal[j] = 0;
	}
# pragma omp parallel for private(j)
	for (i = 0; i < B_COEF - 1; i++) {
		for (j = 0; j < BUF_LEN - 1; j++) {
			d_fsignal[i + j] = d_fsignal[i + j] + (d_samples[j] * d_coef[i]);
		}
	}
}

