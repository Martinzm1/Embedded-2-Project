//Martin Zamora Martinez
//100995810
#include <string.h>
#include <stdio.h>
#include <Windows.h>
#include <WinSock.h>
#include <time.h>
#include <conio.h>
#include <omp.h>


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
#define B_COEF			101

typedef struct {
	char cmd[CMD_LENGTH];
	int arg;
} cmd_struct_t;
WSADATA wsaData;

int x = 1;
int simpleState = 1;
float rate = 0.0;
char filename[40];

/* Thread to interface with the ProfileClient */
HANDLE hClientThread;
DWORD dwClientThreadID;
VOID client_iface_thread(LPVOID parameters);
void filterSignal(double *d_samples, double *d_coef, double *d_fsignal);


int main(){
	struct sp_struct profiler;
	struct sockaddr_in saddr;
	struct hostent *hp;
	int res = 0, counter=0;
	int i;
	char ParamBuffer[110];
	char inputChar[100] = "";
	double fullData[20000];
	double dataBuffer[BUF_LEN];
	double d_coef[B_COEF];
	double d_fsignal[BUF_LEN + B_COEF - 1] = { 0 };
	FILE *fp = fopen("./Mysignal.txt", "r");
	for (i = 0; i < 20000; i++) {
		fscanf(fp, "%lf\n", &fullData[i]);
	}
	fclose(fp);
	fp = fopen("./b.txt", "r");
	for (i = 0; i < B_COEF - 1; i++) {
		fscanf(fp, "%lf\n", &d_coef[i]);
	}
	fclose(fp);
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
	while (simpleState != 3) {
		noop;
	}
	printf("Rate and filename recieved and stored\n");
	printf("%s %f", filename, rate);
	// turn on LEDS
	for (int count2 = 0; count2 < 10; count2++) {
		for (counter = 0; counter < BUF_LEN; counter++) {
			dataBuffer[counter] = fullData[(count2*BUF_LEN) + counter];
		}
		filterSignal(dataBuffer, d_coef, d_fsignal);
		sprintf(inputChar, "startBuf");
		send(comm->datasock, inputChar, sizeof(inputChar), 0);
		for (counter = 0; counter < BUF_LEN; counter++) {
			sprintf(inputChar, "%f", d_fsignal[counter]);
			send(comm->datasock, inputChar, sizeof(inputChar), 0);
		}
		sprintf(inputChar, "endBuf");
		send(comm->datasock, inputChar, sizeof(inputChar), 0);
		printf("startofsleep\n");
		Sleep(1000);
		Sleep(1000);
		printf("endofsleep\n");
	}
	noop;
	sprintf(inputChar, "FINAL");
	send(comm->datasock, inputChar, sizeof(inputChar), 0);
	while (x == 1)
	{
		/*if (_kbhit())
		{
			scanf("%s", inputChar);
			send(comm->datasock, inputChar, sizeof(inputChar), 0);
		}*/

	}
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


	printf("Executing Thread\n");
	printf("Checking for Data\n");
	while (ParamBuffer[0] != '!') {
		memset(ParamBuffer, 0, sizeof(ParamBuffer));
		saddr_len = sizeof(saddr);
		retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len);
		switch (simpleState) {
		case 1:
			strcpy(filename, ParamBuffer);
			simpleState = 2;
			break;
		case 2:
			rate = atof(ParamBuffer);
			simpleState = 3;
			break;
		case 3:
			break;
		}
	}
	x = 0;
}
void filterSignal(double *d_samples, double *d_coef, double *d_fsignal) {
	int i, j;

# pragma omp parallel for private(j)
	for (i = 0; i < B_COEF - 1; i++) {
		for (j = 0; j < BUF_LEN - 1; j++) {
			d_fsignal[i + j] = d_fsignal[i + j] + (d_samples[j] * d_coef[i]);
		}
	}
}
