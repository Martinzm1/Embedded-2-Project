/* This is a modified program provided in the lab
Martin Zamora Martinez 1000995810
Clint Wetzel
*/
/*-----------------------------------------------------------------------

PROGRAM: contadc.c

PURPOSE:
    Open Layers data acquisition example, shows:
        continuos ADC operation.  
        
FUNCTIONS:

    WinMain()        gets handle to ADC subsystem, configures ADC for
                     continuous operation, opens a dialog box to receive
                     window messages, and performs continuous operation
                     on the ADC.

    GetDriver()      callback to get board name and open board, opens
                     the first available Open Layers board.

    OutputBox()      dialog box to handle information and error window
                     messages from the ADC subsystem.

****************************************************************************/
#include <windows.h>     
#include <stdlib.h>     
#include <stdio.h>     
#include <olmem.h>         
#include <olerrors.h>         
#include <oldaapi.h>
#include "contadc.h"



#define NUM_BUFFERS 4
#define STRLEN 80        /* string size for general text manipulation   */
char str[STRLEN];        /* global string for general text manipulation */

/* Error handling macros */
#define SHOW_ERROR(ecode) MessageBox(HWND_DESKTOP,olDaGetErrorString(ecode,\
                  str,STRLEN),"Error", MB_ICONEXCLAMATION | MB_OK);

#define CHECKERROR(ecode) if ((board.status = (ecode)) != OLNOERROR)\
                  {\
                  SHOW_ERROR(board.status);\
                  olDaReleaseDASS(board.hdass);\
                  olDaTerminate(board.hdrvr);\
                  return ((UINT)NULL);}

#define CLOSEONERROR(ecode) if ((board.status = (ecode)) != OLNOERROR)\
                  {\
                  SHOW_ERROR(board.status);\
                  olDaReleaseDASS(board.hdass);\
                  olDaTerminate(board.hdrvr);\
                  EndDialog(hDlg, TRUE);\
                  return (TRUE);}

/* simple structure used with board */

typedef struct tag_board {
   HDEV  hdrvr;        /* device handle            */
   HDASS hdass;        /* sub system handle        */
   ECODE status;       /* board error status       */
   char name[MAX_BOARD_NAME_LENGTH];  /* string for board name    */
   char entry[MAX_BOARD_NAME_LENGTH]; /* string for board name    */
} BOARD;

typedef BOARD* LPBOARD;

static BOARD board;
static ULNG count = 0;

BOOL CALLBACK
GetDriver( LPSTR lpszName, LPSTR lpszEntry, LPARAM lParam )   
/*
this is a callback function of olDaEnumBoards, it gets the 
strings of the Open Layers board and attempts to initialize
the board.  If successful, enumeration is halted.
*/
{
   LPBOARD lpboard = (LPBOARD)(LPVOID)lParam;
   

   lstrcpyn(lpboard->name,lpszName,MAX_BOARD_NAME_LENGTH-1);
   lstrcpyn(lpboard->entry,lpszEntry,MAX_BOARD_NAME_LENGTH-1);


   lpboard->status = olDaInitialize(lpszName,&lpboard->hdrvr);
   if   (lpboard->hdrvr != NULL)
      return FALSE;          /* false to stop enumerating */
   else                      
      return TRUE;           /* true to continue          */
}


BOOL CALLBACK
InputBox( HWND hDlg, 
          UINT message, 
          WPARAM wParam, 
          LPARAM lParam )
/*
This function processes messages for "Input" dialog box
*/
{
   DBL min=0,max=0;
   DBL voltsA;
   DBL voltsB;
   ULNG valueA;
   ULNG valueB;
   ULNG samples;
   UINT encoding=0,resolution=0;
   HBUF  hBuffer = NULL;
   PDWORD  pBuffer32 = NULL;
   PWORD  pBuffer = NULL;
   char WindowTitle[128];
   char temp[128];    
   DASSINFO ssinfo;
   int state=0;
   double tVal=2.51;


   switch (message) {
      case WM_INITDIALOG:   /* message: initialize dialog box  */
         
         /* set the title to the board name */ 
         
         olDaGetDASSInfo(board.hdass,&ssinfo);
         itoa(ssinfo.uiElement,temp,10);
         strncpy(WindowTitle,board.name,128);
         strncat(WindowTitle," Element # ",128);
         strncat(WindowTitle,temp,128);

         SetWindowText(hDlg,WindowTitle);
            

         /* set window handle and configure sub system */
         
         CLOSEONERROR (olDaSetWndHandle(board.hdass, hDlg,(UINT)NULL));
         CLOSEONERROR (olDaConfig(board.hdass));

         /* start sub system */

         CLOSEONERROR (olDaStart(board.hdass));
         return (TRUE);   /* Did process a message */

      case OLDA_WM_BUFFER_REUSED:   /* message: buffer reused  */
         break;

      case OLDA_WM_BUFFER_DONE:     /* message: buffer done  */

         /* get buffer off done list */

         CHECKERROR (olDaGetBuffer(board.hdass, &hBuffer));

         /* if there is a buffer */

         if( hBuffer )
         {

             /* get sub system information for code/volts conversion */

             CLOSEONERROR (olDaGetRange(board.hdass,&max,&min));
             CLOSEONERROR (olDaGetEncoding(board.hdass,&encoding));
             CLOSEONERROR (olDaGetResolution(board.hdass,&resolution));

            /* get max samples in input buffer */

            CLOSEONERROR (olDmGetValidSamples( hBuffer, &samples ) );

            /* get pointer to the buffer */
            if (resolution > 16)
            {
            CLOSEONERROR (olDmGetBufferPtr( hBuffer,(LPVOID*)&pBuffer32));
            /* get last sample in buffer */
            valueA = pBuffer32[samples-1];
            }
            else
            {
            CLOSEONERROR (olDmGetBufferPtr( hBuffer,(LPVOID*)&pBuffer));

            valueA = pBuffer[samples-1]; //value from 1 input
			      valueB = pBuffer[samples-2]; //value from second input
            }
            /* put buffer back to ready list */

            CHECKERROR (olDaPutBuffer(board.hdass, hBuffer));
         
            /*  convert value to volts */

            if (encoding != OL_ENC_BINARY) 
            {
               /* convert to offset binary by inverting the sign bit */
      
               valueA ^= 1L << (resolution-1);
               valueA &= (1L << resolution) - 1;     /* zero upper bits */
			         valueB ^= 1L << (resolution-1);
               valueB &= (1L << resolution) - 1;     /* zero upper bits */

            }
   
            voltsA = ((float)max-(float)min)/(1L<<resolution)*valueA + (float)min;
			      voltsB = ((float)max-(float)min)/(1L<<resolution)*valueB + (float)min;

            /* display value */
   
            //sprintf(str,"%.2f:%.2f",voltsB,voltsA);
			if (state==0){
				if(voltsB> tVal){
					sprintf(str, " ON");
					state=1;
				}
				else{
					sprintf(str, " OFF");
				}
				

			}
			else{
				//checkB
				sprintf(str,"State is now next state");
			}
			
            SetDlgItemText (hDlg, IDD_VALUE, str);
         }
         return (TRUE);   /* Did process a message */
         
      case OLDA_WM_QUEUE_DONE:
      case OLDA_WM_QUEUE_STOPPED:
         /* using wrap multiple - if these message are received */
         /* acquisition has stopped.                            */
   
         EndDialog(hDlg, TRUE);
         return (TRUE);   /* Did process a message */
    
      case OLDA_WM_TRIGGER_ERROR:
         /* Process trigger error message */

         MessageBox(hDlg,"Trigger error: acquisition stopped",
                  "Error", MB_ICONEXCLAMATION | MB_OK);
         EndDialog(hDlg, TRUE);
         return (TRUE);   /* Did process a message */
        
      case OLDA_WM_OVERRUN_ERROR:
         /* Process underrun error message */
        
         MessageBox(hDlg,"Input overrun error: acquisition stopped",
                  "Error", MB_ICONEXCLAMATION | MB_OK);
         EndDialog(hDlg, TRUE);   /* Did process a message */
         return (TRUE);   /* Did process a message */
 
      case WM_COMMAND:      /* message: received a command */

         switch ( LOWORD(wParam) )  {
            case IDOK:
            case IDCANCEL:
               CLOSEONERROR (olDaAbort(board.hdass));
               EndDialog(hDlg, TRUE);
               return (TRUE);   /* Did process a message */
         }
         break;   
    }                               
    return (FALSE);               /* Didn't process a message */
}
   
 
int WINAPI 
WinMain( HINSTANCE hInstance, 
         HINSTANCE hPrevInstance, 
         LPSTR lpCmdLine, 
         int nCmdShow )
/*++
This is our apps entry point.  It calls the open layers functions
to execute the required operation.
--*/
{
   DBL  freq;
   UINT size,dma,gainsup,samplesize;
   int i;

   UINT channel = 0;
   UINT channel1 = 1;
   UINT numberADs = 0;
   UINT currentAD = 0;
   DBL gain = 1.0;
   HBUF  hBuffer = NULL;
   ECODE ec=OLNOERROR;


   board.hdrvr = NULL;

    // Get cmdline board
    if (lpCmdLine[0]!=0)
    {
     CHECKERROR( olDaInitialize(lpCmdLine,&board.hdrvr) );
     strcpy(board.name,lpCmdLine);
    }
    else
    {
   /* Get first available Open Layers board */
   CHECKERROR (olDaEnumBoards(GetDriver,(LPARAM)(LPBOARD)&board));
    }
   /* check for error within callback function */

   CHECKERROR (board.status);

   /* check for NULL driver handle - means no boards */

   if (board.hdrvr == NULL){
      MessageBox(HWND_DESKTOP, " No Open Layer boards!!!", "Error",
            MB_ICONEXCLAMATION | MB_OK);
      return ((UINT)NULL);
   }

   /* get handle to first available ADC sub system */
   CHECKERROR(olDaGetDevCaps(board.hdrvr,OLDC_ADELEMENTS,&numberADs));

    while(1)    // Enumerate through all the A/D subsystems and try and select the first available one...
   {
        ec=olDaGetDASS(board.hdrvr,OLSS_AD,currentAD,&board.hdass);
        if (ec!=OLNOERROR)
        {
        // busy subsys etc...
        currentAD++;
        if (currentAD>numberADs)
        {
            MessageBox(HWND_DESKTOP,"No Available A/D Subsystems!","Error",MB_ICONEXCLAMATION|MB_OK);
              return ((UINT)NULL);
        }

        }
        else
        break;

   }

   /*
      Set up the ADC - multiple wrap so we can get buffer reused
      window messages.
   */

   CHECKERROR (olDaGetSSCapsEx(board.hdass,OLSSCE_MAXTHROUGHPUT,&freq));
   CHECKERROR (olDaGetSSCaps(board.hdass,OLSSC_NUMDMACHANS,&dma));
   CHECKERROR (olDaGetSSCaps(board.hdass,OLSSC_SUP_PROGRAMGAIN,&gainsup));
   
   dma  = min (1, dma);            /* try for one dma channel   */ 
   freq = min (1000.0, freq);      /* try for 1000hz throughput */

   CHECKERROR (olDaSetDataFlow(board.hdass,OL_DF_CONTINUOUS));
   CHECKERROR (olDaSetWrapMode(board.hdass,OL_WRP_MULTIPLE));
   CHECKERROR (olDaSetClockFrequency(board.hdass,freq));
   CHECKERROR (olDaSetDmaUsage(board.hdass,dma));

   CHECKERROR (olDaSetChannelListEntry(board.hdass,0,channel));
   CHECKERROR (olDaSetChannelListEntry(board.hdass,1,channel1));

   /* only set the gain if the board supports it!!! */

   if (gainsup) {
      CHECKERROR (olDaSetGainListEntry(board.hdass,0,gain));
	  CHECKERROR (olDaSetGainListEntry(board.hdass,1,gain));
   }

   CHECKERROR (olDaSetChannelListSize(board.hdass,2));
   CHECKERROR (olDaConfig(board.hdass));

   size = (UINT)freq/1;     /* 1 second buffer */
   
   /* allocate the input buffers */
   /* Put the buffer to the ADC  */
    
    CHECKERROR(olDaGetResolution(board.hdass,&samplesize));
    if (samplesize > 16)
        samplesize=4; //4 bytes...// e.g. 24 bits = 4 btyes
    else
        samplesize=2;             // e.g. 16 or 12 bits = 2 bytes

   for (i=0;i<NUM_BUFFERS;i++)
   {
      CHECKERROR (olDmCallocBuffer(0,0,(ULNG) size,samplesize,&hBuffer));
      CHECKERROR (olDaPutBuffer(board.hdass, hBuffer));
   }

   /*
      use a dialog box to collect information and error
      messages from the subsystem
   */

   DialogBox(hInstance, (LPCSTR)INPUTBOX, HWND_DESKTOP, InputBox);

   /*
      get the input buffers from the ADC subsystem and
      free the input buffers
   */

   CHECKERROR (olDaFlushBuffers(board.hdass));
   
   for (i=0;i<NUM_BUFFERS;i++)
   {
      CHECKERROR (olDaGetBuffer(board.hdass, &hBuffer));
      CHECKERROR (olDmFreeBuffer(hBuffer));
   }
   
   /* release the subsystem and the board */

   CHECKERROR (olDaReleaseDASS(board.hdass));
   CHECKERROR (olDaTerminate(board.hdrvr));

   /* all done - return */

   return ((UINT)NULL);
}

