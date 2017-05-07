/*Clint Wetzel
//Martin Zamora Martinez
/CSE 4320 - 001  Embedded Systems II 
//University of Texas at Arlington
Real-Time Data Acquasition  with Communication between Server and Client

*****Windows thread and socket implementation adapted from Tx.cpp and Rx.cpp provided by Dr.Walker.
*****DT9816 used for data acquasition. Some methods to interface with the ST9816 from Data Translations example files.
*/

/* Includes */
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

/*structure used for communication*/
typedef struct sp_comm {
	WSADATA wsaData;
	SOCKET cmdrecvsock;
	SOCKET cmdstatusock;
	SOCKET datasock;
	struct sockaddr_in server;
} *sp_comm_t;
/*structure used to hold flags*/
typedef struct sp_flags {
	unsigned int start_system : 1;
	unsigned int pause_system : 1;
	unsigned int shutdown_system : 1;
	unsigned int analysis_started : 1;
	unsigned int restart : 1;
	unsigned int transmit_data : 1;
} *sp_flags_t;
/*structure that holds a flag structure and communication structure*/
typedef struct sp_struct {
	struct sp_comm		comm;
	struct sp_flags		flags;
} *sp_struct_t;

/*Defines*/
#define ERR_CODE_NONE	0	/* no error */
#define ERR_CODE_SWI	1	/* software error */ 
#define CMD_LENGTH		5
#define ARG_NONE		1
#define ARG_NUMBER		2
#define noop ((void)0)
#define BUF_LEN			1000
#define B_COEF			101
#define PI 3.1415926535897
/*sturcture used for DT9816*/
typedef struct {
	char cmd[CMD_LENGTH];
	int arg;
} cmd_struct_t;
/*global variables*/
WSADATA wsaData;
int x = 1;
/* Thread to interface with the ProfileClient */
HANDLE hClientThread;
DWORD dwClientThreadID;
VOID client_iface_thread(LPVOID parameters); //function for thread to handle receiving messages
char snum[5];
char fName[20];//holds user input for name prefix
char filName[20];//holds user input for filter file
char sampRate[20];//hols user input for sampling rate
char maxFile[40];//appends name prefix with full file name
char areaFile[40];//appends name prefix with full file name
char minFile[40];//appends name prefix with full file name
double buffer[BUF_LEN]; //buffer to hold receiving data
int bufBusy = 2; //state for buffer filling
int lastSample = 0; //state to end program
int dCount;
double max, min, area; 
void getInputs();// function to accept user inputs
void processData();// function to calculate and process data buffer
double maxSignal(double *d_signal, int length); //function to return max value in bffer
double minSignal(double *d_signal, int length);//function to return min value in buffer
double integrate();//function to calculate area under curve

				   /*MAIN*/
int main(){
	struct sp_struct profiler;
	struct sockaddr_in saddr;
	struct hostent *hp;
	int res = 0;
	char ParamBuffer[110];
	char inputChar[110] = "";
	double filter[B_COEF];
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
	/*create thread for receiving messages*/
	hClientThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)client_iface_thread, (LPVOID)&profiler, 0, &dwClientThreadID);
	SetThreadPriority(hClientThread, THREAD_PRIORITY_LOWEST);
	getInputs();//grab inputs
	FILE *fp2;
	fp2 = fopen(filName, "r");// open filter file
	for (int i = 0; i < B_COEF - 1; i++) {// load filter into filter buffer
		fscanf(fp2, "%lf\n", &filter[i]);
	}
	fclose(fp2);//close filter file
	send(comm->datasock, fName, sizeof(fName), 0); //send file name prefix to server
	send(comm->datasock, sampRate, sizeof(sampRate), 0); //send sampling rate to server
	for (int i = 0; i < B_COEF - 1; i++) { //send the values in filter buffer to server
		sprintf(ParamBuffer, "%f", filter[i]);
		send(comm->datasock, ParamBuffer, sizeof(ParamBuffer), 0);
	}
	while (!lastSample) {
		if (bufBusy == 0) { //buffer complete
			processData();// process full buffer
			printf("data processed\n");
			bufBusy = 3;// dont keep processing same data
		}
		if (bufBusy == 1) {//buffer is being filled
						   //printf("waiting for buffer to fill\n");
			noop;// keep waiting for full buffer
		}
		if (bufBusy == 2) {// no buffer recieved yet
						   //printf("no buffer yet\n");
			noop;
		}
	}
	printf("Data saved in %s\n", fName);
	printf("Ending Client.\n");
	return 0;
}

/************************************************************************************
* VOID client_iface_thread(LPVOID)
*
* Description: Thread communicating commands from client and the status of their
*				completion back to client. It handles special messages from the Server
*				to control the states of this program or fill the data buffer.
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
		retval = recvfrom(comm->cmdrecvsock, ParamBuffer, sizeof(ParamBuffer), 0, (struct sockaddr *)&saddr, &saddr_len); //grab input from socket
		if (bufBusy == 1) { //if filling buffer is activated
			buffer[dCount] = atof(ParamBuffer);	//copy recieved data to buffer
			dCount++;
		}
		if (strcmp(ParamBuffer, "startBuf") == 0) { //when starting buffer message recevied 
			bufBusy = 1;							//change buffer state to busy
			dCount = 0;
		}
		if (strcmp(ParamBuffer, "endBuf") == 0) { //when ending buffer message received
			bufBusy = 0;						  //change buffer state to not busy
		}
		if (strcmp(ParamBuffer, "FINAL") == 0) {// when ending program message recieved
			lastSample = 1;						//set state to last sample
			printf("final message recieved\n");
		}
	}
}
/*getInput:
	Description: Void function that prompts the user for a filter name, file name prefix
				and sampling rate. The filter file must be in Client.cpp directory.
				This data will then be passed to the Server by socket communication.

*/
void getInputs() {
	printf("Input filter filename:\n");
	scanf("%s", filName);
	printf("Input desired data filename prefix:\n");
	scanf("%s", fName);
	sprintf(maxFile, "%s_max.txt", fName);
	sprintf(areaFile, "%s_area.txt", fName);
	sprintf(minFile, "%s_min.txt", fName);
	printf("Input desired Sampling Rate:\n");
	scanf("%s", sampRate);
	printf("Press ENTER to begin Data Collection\n");
	getchar();//grab newline key that was leftover from scanf.
	getchar();//wait for enter.
	return;
}
/*processData:
		Description: processData is a void function that uses a globally stored value buffer
					and other functions to find the max, min and area under the curve of the
					signal stored in the value buffer. The data is then also stored in 3 files.

		NOTE:		processData appends files it writes to. If there is an existing file with the 
					same name, it will only be appeneded. IF there is no such file, the file will 
					be created.
*/
void processData() {
	FILE *fpMax, *fpArea, *fpMin; //3 file pointers
	//each file opened withe the name stored earlier
	fpMax = fopen(maxFile, "a"); 
	fpArea = fopen(areaFile, "a");
	fpMin = fopen(minFile, "a");
	
	max = maxSignal(buffer, BUF_LEN);	//max function called
	min = minSignal(buffer, BUF_LEN);	//min function called
	area = integrate();					//area function called
	printf("area: %f min: %f max: %f\n", area, min, max);	//print data to console
	//each data stored in its corresponding file
	fprintf(fpMax, "%f\n", max);		
	fprintf(fpArea, "%f\n", area);
	fprintf(fpMin, "%f\n", min);
	//each file closed
	fclose(fpMin);
	fclose(fpArea);
	fclose(fpMax);
	return;
}\
/*maxSignal function
	Description: accepts a pointer to a double array and an int of the length of that array to 
				find the max value in the array.
*/
double maxSignal(double *d_signal, int length) {
	int i;
	double max = d_signal[0];

	for (i = 100; i < length - 100; i++) {// start at 100 to ignore the transient region
		if (d_signal[i] > max) {
			max = d_signal[i];
		}
	}

	return max;
}
/*minSignal function
Description: accepts a pointer to a double array and an int of the length of that array to
find the min value in the array.
*/
double minSignal(double *d_signal, int length) {
	int i;
	double min = d_signal[100];

	for (i = 100; i < length - 100; i++) {// start at 100 to ignore the transient region
		if (d_signal[i] < min) {
			min = d_signal[i];
		}
	}
	return min;
}

/* Intergrate void function
	Description: Integrate is a void function that uses a global value
				buffer that contains a signal. It uses the trapezoidal function
				to calculate the area under the curve.
*/
double integrate() {
	int count, max;
	double sum, lowLim, highLim;
	lowLim = 0;
	highLim = PI / 2;
	max = BUF_LEN / 4;
	sum = buffer[249];
	for (int count = 250; count < max - 1; count++) {
		sum = sum + (2 * buffer[count]);
	}
	sum = sum + buffer[max + 249];
	sum = sum*(highLim - lowLim) / (2 * max);
	return sum;
}
