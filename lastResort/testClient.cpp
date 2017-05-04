//Martin Zamora Martinez
//1000995810
#include <string.h>
#include <stdio.h>
#include <Windows.h>
#include <WinSock.h>
#include <time.h>
#include <omp.h>
#include <conio.h>
#include <iostream>
#include <fstream>
#include <string>
#include <stdlib.h>

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




#define ERR_CODE_NONE	0	/* no error */
#define ERR_CODE_SWI	1	/* software error */

/////// 
#define CMD_LENGTH		5

#define ARG_NONE		1
#define ARG_NUMBER		2
#define noop ((void)0)
#define BUF_LEN			2000


typedef struct {
	char cmd[CMD_LENGTH];
	int arg;
} cmd_struct_t;
WSADATA wsaData;
int x = 1;

/* Thread to interface with the ProfileClient */
HANDLE hClientThread;
DWORD dwClientThreadID;
VOID client_iface_thread(LPVOID parameters);
char snum[5];
char fName[20];
char sampRate[20];
double buffer[BUF_LEN];
int bufBusy = 2;
int lastSample = 0;
int dCount;
double avg, max, min;
void getInputs();
void processData();
double maxSignal(double *d_signal, int length);
double avgSignal(double *d_signal, double average_offset, int length);

int main()
{
	struct sp_struct profiler;
	struct sockaddr_in saddr;
	struct hostent *hp;
	int res = 0;
	char ParamBuffer[110];
	char inputChar[110] = "";
	for (int count = 0; count < BUF_LEN; count++) {
		buffer[count] = count;
	}
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



	hp->h_addr_list[0][2] = (signed char)255; //0;255  
	hp->h_addr_list[0][3] = (signed char)255; //140;255
	hp->h_addr_list[0][4] = 0;

	/**********************************************************************************
	* Setup a socket for broadcasting data
	**********************************************************************************/
	memset(&saddr, 0, sizeof(saddr));
	saddr.sin_family = hp->h_addrtype;
	memcpy(&(saddr.sin_addr), hp->h_addr, hp->h_length);
	saddr.sin_port = htons(1500);

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
	saddr.sin_port = htons(1024);
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


	getInputs();//grab inputs
				//send data to server
				//receive data 
	printf("%s    %s \n", sampRate, fName);
	send(comm->datasock, fName, sizeof(fName), 0);
	send(comm->datasock, sampRate, sizeof(sampRate), 0);
	while (!lastSample) {
		if (bufBusy==0) { //buffer complete
			processData();// process full buffer
			printf("data processed\n");
			bufBusy = 3;// dont keep processing same data
		}
		if (bufBusy == 1) {//buffer is being filled
			//printf("waiting for buffer to fill\n");
			noop;// keep waiting for full buffer
		}
		if (bufBusy == 2) {// no buffer recieved yet
			printf("no buffer yet\n");
			noop;
		}
	}
	//save file
	//end

	//processData();
	printf("END");

	return 0;
}

/************************************************************************************
* VOID client_iface_thread(LPVOID)
*
* Description: Thread communicating commands from client and the status of their
*				completion back to client.
*
*
************************************************************************************/
VOID client_iface_thread(LPVOID parameters) //LPVOID parameters)
{
	sp_struct_t profiler = (sp_struct_t)parameters;
	sp_comm_t comm = &profiler->comm;
	INT retval;
	struct sockaddr_in saddr;
	int saddr_len;
	char ParamBuffer[110];

	while (ParamBuffer[0] != '!')
	{
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len = sizeof(saddr);
		retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
		if (bufBusy == 1) {
			buffer[dCount] = atof(ParamBuffer);
			dCount++;
			//printf("The value is : %s \n", ParamBuffer);
		}
		if (strcmp(ParamBuffer, "startBuf") == 0) {
			bufBusy = 1;
			dCount = 0;
		}
		if (strcmp(ParamBuffer, "endBuf") == 0) {
			bufBusy = 0;
		}
		if (strcmp(ParamBuffer, "FINAL") == 0) {
			lastSample = 1;
			printf("final message recieved\n");
		}
	}
}

void getInputs() {
	char temp[20], ch[5];
	printf("Input desired filename:\n");
	scanf("%s", fName);
	printf("Input desired Sampling Rate:\n");
	scanf("%s", sampRate);
	printf("Press ENTER to begin Data Collection\n");
	getchar();//grab newline key that was leftover from scanf.
	getchar();//wait for enter.
	return;
}
void processData() {
	max=maxSignal(buffer, BUF_LEN);
	avg=avgSignal(buffer, 100.0,BUF_LEN);
	printf("avg: %f max: %f\n", avg, max);
	return;
}
double maxSignal(double *d_signal, int length) {
	int i;
	double max = d_signal[0];

	for (i = 0; i < length; i++) {
		if (d_signal[i] > max) {
			max = d_signal[i];
		}
	}

	return max;
}

double avgSignal(double *d_signal, double average_offset, int length) {
	int i, avg;
	double sum = 0.0;

# pragma omp parallel for reduction(+:sum)
	for (i = average_offset; i < length; i++) {
		sum = sum + d_signal[i];
	}
	avg = sum / ((double)length - average_offset);

	return avg;
}
