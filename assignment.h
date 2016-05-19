 /**************************************************************************
 *  FILE: assignment.h
 *  AUTHOR: Connor Beardsmore - 15504319
 *  UNIT: CC Assignment. S1 - 2016
 *  PURPOSE: Header File for assignment.c
 *  LAST MOD: 16/05/16
 *  REQUIRES: cnet.h, stdlib.h, stdio.h, string.h
 ***************************************************************************/

 #include <cnet.h>
 #include <stdlib.h>
 #include <stdio.h>
 #include <string.h>

 //**************************************************************************
 // CONSTANT DECLARATIONS
 //**************************************************************************

#define FRAME_SIZE(f)    ((sizeof(FRAME) - sizeof(f.data)) + f.len)
#define MAX_SEQ          4         // SIZE OF THE WINDOW
#define MAX_LINKS        4         // MAX LINKS ANY ONE NODE HAS
#define MAGIC_NUM        8000000   // MAGIC CNET TIMEOUT VALUE
#define T_MOD            4         // MODIFIER APPLIED TO TIMEOUT
#define LAST_LED         5         // ID OF THE LAST CNET LED
#define NUM_NODES        7

//**************************************************************************
// GLOBAL VARIABLES - FOR SIMULATION ONLY
//**************************************************************************


typedef enum{ DL_DATA, DL_ACK }   FRAMEKIND;

typedef struct
{
    int          srcNode;                  // NODE NUMBER OF SENDER
    int          destNode;                 // NODE NUMBER OF FINAL RECEIVER
    FRAMEKIND    kind;                     // EITHER DATA OR ACK
    size_t       len;                      // LENGTH OF DATA ONLY
    int          checksum;                 // ERROR CORRECTION FOR ENTIRE FRAME
    int          seq;                      // SEQUENCE NUMBER
    int          link;                     // LAST LINK THE FRAME WAS SENT ON
    char         data[MAX_MESSAGE_SIZE];   // ACTUAL PAYLOAD
} FRAME;

//**************************************************************************
// ROUTING TABLE
//**************************************************************************

// LINK TO GET TO EVERY DESTINATION NODE FROM EVERY SOURCE NODE
static int routingTable[7][7] =
    {{0,1,2,3,3,3,3},   //AUSTRALIA
     {1,0,1,1,1,1,1},   //FIJI
     {1,1,0,1,1,1,1},   //NEW ZEALAND
     {1,1,1,0,2,3,4},   //INDONESIA
     {1,1,1,1,0,2,1},   //SINGAPORE
     {1,1,1,1,2,0,1},   //MALAYSIA
     {1,1,1,1,1,1,0}};  //BRUNEI

//**************************************************************************
// NODE NAME ARRAY
//**************************************************************************

// MAP NODE NUMBERS INTO STRINGS FOR PRINTING TO USER
static char* nodenames[7] = {"Australia", "Fiji", "New Zealand", "Indonesia",
                              "Singapore", "Malaysia", "Brunei"};

//**************************************************************************
// TIMERS
//**************************************************************************

// EVERY LINK WINDOW HAS ARRAY OF TIMERS FOR EVERY FRAME SENT
static CnetTimerID timers[MAX_LINKS+1][MAX_SEQ + 1];

//**************************************************************************
// BUFFERS, WINDOWS + STORAGE
//*************************************************************************

// NUM ITEMS IN WINDOW
static int numInWindow[MAX_LINKS] = {0, 0, 0, 0};

// WINDOW STORING UNACKNOWLEDGED FRAMES
// EVERY NODE HAS WINDOW OF SIZE MAX_SEQ + 1 FOR EVERY LINK
static FRAME window[MAX_LINKS][MAX_SEQ + 1];

// NUM ITEMS IN BUFFER
static int numInBuffer[MAX_LINKS] = {0, 0, 0, 0};

// START AND END INDEX FOR EACH BUFFER, USING A CIRCULAR BUFFER
// START INDEX IS FIRST FULL INDEX
// LAST INDEX IS LAST EMPTY INDEX
static int bufferBounds[MAX_LINKS][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}};

// WINDOW STORING UNACKNOWLEDGED FRAMES...AGAIN
static FRAME buffer[MAX_LINKS][MAX_SEQ + 1];

// EXPECTED ACK SEQ NUM FOR NEXT INCOMING ACK FOR EVERY LINK
static  int ackExpected[MAX_LINKS]	    = {0, 0, 0, 0};
// SEQ NUM OF NEXT FRAME TO SEND FOR EVERY LINK
static	int	nextFrameToSend[MAX_LINKS]	= {0, 0, 0, 0};
// SEQ NUM OF NEXT EXPECTED FRAME FOR EVERY LINK
static	int	frameExpected[MAX_LINKS]    = {0, 0, 0, 0};


//**************************************************************************
// FUNCTION PROTOTYPES
//**************************************************************************


// TRANSPORT LAYER
static void transportDown(CnetEvent, CnetTimerID, CnetData);
static void transportUp(char* data, size_t dataLength);

// NETWORK LAYER
static void networkDown(char*, size_t, int);
static void forwardFrame(FRAME, int);
static int getRoute(int);
static void networkUp(FRAME, int);

// DATA LINK LAYER
static void datalinkDown(FRAME, int, int);
static void transmitAck(int, int);
static void transmitFrame(FRAME);
static void datalinkUp(CnetEvent, CnetTimerID, CnetData);
static void ackReceived(FRAME, int);

// TIMEOUTS
static void setTimer(FRAME);
static void timeoutLink1(CnetEvent, CnetTimerID, CnetData);
static void timeoutLink2(CnetEvent, CnetTimerID, CnetData);
static void timeoutLink3(CnetEvent, CnetTimerID, CnetData);
static void timeoutLink4(CnetEvent, CnetTimerID, CnetData);

// NODE INITIALIZATION
void reboot_node(CnetEvent, CnetTimerID, CnetData);

// MISCELLANEOUS
static int between(int, int, int);
static void printBuffers(int);
static void inc(int*);
//---------------------------------------------------------------------------
