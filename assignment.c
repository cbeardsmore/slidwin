/**************************************************************************
*  FILE: assignment.c
*  AUTHOR: Connor Beardsmore - 15504319
*  UNIT: CC Assignment. S1 - 2016
*  PURPOSE: Sliding window protocol on a 7-node network via CNET simulator
*  LAST MOD: 16/05/16
*  REQUIRES: assignment.h
***************************************************************************/

#include "assignment.h"

//**************************************************************************
// TRANSPORT - DOWN
//**************************************************************************

// FUNCTION: transport_down
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Read message from the application layer and pass down to network

static void transport_down(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    int destination;

    // ALLOCATE MEMORY FOR MESSAGE SO WE CAN PASS ARRAY ON THE HEAP
    char* msg = (char*)malloc(MAX_MESSAGE_SIZE * sizeof(char));
    size_t msgLength = MAX_MESSAGE_SIZE * sizeof(char);
    // READ THE MESSAGE FROM THE APPLICATION LAYER AND THROW DOWN
    CHECK(CNET_read_application( &destination, msg, &msgLength ));
    network_down( msg, msgLength, destination );
}

//**************************************************************************
// TRANSPORT - UP
//**************************************************************************

// FUNCTION: transport_up
// IMPORT: data (char*), dataLength (size_t)
// PURPOSE: Write a message back up to the application layer

static void transport_up( char* data, size_t dataLength )
{
    // WRITE THE MESSAGE BACK TO THE APPLICATION LAYER
    CHECK(CNET_write_application( data, &dataLength ));
}

//**************************************************************************
// NETWORK LAYER - DOWN
//**************************************************************************

// FUNCTION: network_down
// IMPORT: data (char*), dataLength (size_t), destination (int)
// PURPOSE: Read application layer and send frame down to dll after routing

static void network_down( char* data, size_t dataLength, int destination )
{
    FRAME frame;

    // READ MESSAGE FROM APPLICATION LAYER AND ENCAPSULATE INTO FRAME
    frame.len = dataLength;
    memcpy( frame.data, data, frame.len );
    frame.srcNode = nodeinfo.nodenumber;
    frame.destNode = destination;
    frame.kind = DL_DATA;

    free(data); // DATA IS MEM COPIED SO CAN FREE ORIGINAL

    // DETERMINE THE ROUTE OF WHERE TO SEND TO VIA ROUTING TABLE
    frame.link = get_route(frame.destNode);

    // THROW FRAME DOWN TO DATA LINK LAYER
    datalink_down(frame, frame.link, 0);

    // PRINT CONTENT OF WINDOW AND BUFFER
    print_buffers(frame.link);
}

//**************************************************************************

// FUNCTION: forward_frame
// IMPORT: frame (FRAME), inLink (int)
// PURPOSE: Takes in a frame and forwards out to its actual destination

static void forward_frame(FRAME frame, int inLink)
{
    // DETERMINE NODE TO SEND OUT ON TO REACH ACTUAL DESTINATION
    frame.link = get_route(frame.destNode);

    printf("\t\t\t\t\tFORWARDING FRAME VIA LINK %d\n", frame.link);

    // THROW FRAME DOWN TO DATA LINK LAYER
    datalink_down(frame, inLink, frame.link);

    print_buffers(frame.link);
}

//**************************************************************************

// FUNCTION: get_route
// IMPORT: destination (int)
// EXPORT: link (int)
// PURPOSE: Calculate link required to get to a given destination

static int get_route(int destination)
{
    return routingTable[nodeinfo.nodenumber][destination];
}

//**************************************************************************
// NETWORK LAYER - UP
//**************************************************************************

// FUNCTION: network_up
// IMPORT: frame (FRAME), link (int)
// PURPOSE: Take in frame, unpack data and throw it upwards

static void network_up(FRAME frame, int link)
{
    // IF THE FRAME'S DESTINATION IS THIS NODE
    if ( frame.destNode == nodeinfo.nodenumber )
    {
        // UNPACK MESSAGE AND SEND TO TRANSPORT LAYER
        char* data = frame.data;
        size_t dataLength = frame.len;

        // ACKNOWLEDGE THE MESSAGE AND CHANGE FRAME EXPECTED
        transmit_ack(link, frame.seq);
        inc(&frameExpected[link - 1]);

        // THROW DATA UP TO TRANSPORT LAYER
        transport_up( data, dataLength );
    }
    // FRAMES DESTINATION IS NOT THIS NODE, MUST BE FORWARDED
    else
    {
        // LET FORWARD FRAME RE-ROUTE TO CORRECT PLACE
        forward_frame( frame, link );
    }
}

//**************************************************************************
// DATA LINK LAYER - DOWN
//**************************************************************************

// FUNCTION: datalink_down
// IMPORT: frame (FRAME), inLink (int), outLink (int)
// PURPOSE: Takes a frame and writes down to physical layer. If frame is
//          being forwarded (outLink != 0), also transmit an ACK

static void datalink_down(FRAME frame, int inLink, int outLink)
{
    // FRAME IS BEING FORWARDED
    if ( outLink != 0 )
    {
        // MORE ROOM TO SEND SO TRANSMIT FRAME
        if (numInWindow[outLink - 1] < MAX_SEQ)
        {
            transmit_ack(inLink, frame.seq);
            inc(&frameExpected[inLink - 1]);

            frame.seq = nextFrameToSend[outLink - 1];
            window[outLink - 1][nextFrameToSend[outLink - 1]] = frame;
            numInWindow[outLink - 1] += 1;

            frame.link = outLink;
            transmit_frame(frame);
            inc(&nextFrameToSend[outLink - 1]); 
        }
        // NOT ENOUGH ROOM IN WINDOW TO SEND
        else
        {
            if (numInBuffer[outLink - 1] < MAX_SEQ + 1)
            {
                transmit_ack(inLink, frame.seq);
                inc(&frameExpected[inLink - 1]);

                buffer[outLink - 1][bufferBounds[outLink - 1][1]] = frame;
                numInBuffer[outLink - 1] += 1;
                inc(&bufferBounds[outLink - 1][1]);
                printf("\t\t\t\t\tADDED TO BUFFER\n");
            }
            else
            {
                // IF WINDOW AND BUFFER BOTH FULL, CAN'T DO ANYTHING. DROP
                printf("\t\t\t\t\tBUFFER FULL, FRAME DROPPED\n");
            }
        }

        // IF THE WINDOW IS CURRENTLY FULL
        if (numInWindow[outLink - 1] >= MAX_SEQ)
        {
            // DISABLE GENERATION OF NEW MESSAGES

            for ( int ii = 0; ii < NUM_NODES; ii++ )
            { 
                if ( routingTable[nodeinfo.nodenumber][ii] == outLink )
                {    
                    CNET_disable_application( ii );
                    printf("\t\t\t\t\tDISABLED APPLICATION\n");
                    CNET_set_LED( outLink-1, "red" );
                }
            }
        }
    }
    else
    {
        // MORE ROOM TO SEND SO TRANSMIT FRAME
        if (numInWindow[inLink - 1] < MAX_SEQ)
        {
            // GET SEQUENCE NUMBER, STORE FRAME IN WINDOW, INC WINDOW NUMBER
            frame.seq = nextFrameToSend[inLink - 1];
            window[inLink - 1][nextFrameToSend[inLink - 1]] = frame;
            numInWindow[inLink - 1] += 1;
            // CALL DLL TO TRANSMIT FRAME
            frame.link = inLink;
            transmit_frame(frame);
            inc(&nextFrameToSend[inLink - 1]);
        }
        // NOT ENOUGH ROOM IN WINDOW TO SEND
        else
        {
            // ADD FRAME TO THE BUFFER WAITING TO SEND OUT
            buffer[inLink - 1][bufferBounds[inLink - 1][1]] = frame;
            numInBuffer[inLink - 1] += 1;
            inc(&bufferBounds[inLink - 1][1]);

            // PRINT DESTINATION OF MESSAGE, LINK, INDEX IN BUFFER
            printf("\nADDED TO BUFFER\n");
            printf("DEST:\t%s\n", nodenames[frame.destNode]);
            printf("LINK:\t%d\n", inLink);
            printf("INDEX:\t%d\n", bufferBounds[inLink - 1][1] - 1);
        }

        // IF THE WINDOW IS CURRENTLY FULL
        if (numInWindow[inLink - 1] >= MAX_SEQ)
        {
            for ( int ii = 0; ii < NUM_NODES; ii++ )
            { 
                if ( routingTable[nodeinfo.nodenumber][ii] == inLink )
                {    
                    CNET_disable_application( ii );
                    printf("DISABLE APPLICATION FOR %s\n", nodenames[ii] );
                    CNET_set_LED( inLink-1 , "red" );
                }
            }        
        }

    }
}

//**************************************************************************

// FUNCTION: transmit_ack
// IMPORT: link (int), seqNum (int)
// PURPOSE: Send an ACK frame containing the given seqnum and no data payload

static void transmit_ack(int link, int seqNum)
{
    // CREATE EMPTY ACK FRAME
    FRAME frame;
    frame.kind = DL_ACK;
    frame.len = 0;
    frame.seq = seqNum;
    frame.link = link;

    // CALL STANDARD TRANSMIT FRAME FUNCTION TO WRITE TO PHYSICAL
    transmit_frame(frame);
}

//**************************************************************************

// FUNCTION: transmit_frame
// IMPORT: frame (FRAME)
// PURPOSE: Takes a frame and sends on link given in frame.link

static void transmit_frame(FRAME frame)
{
    size_t frameLength;

    if (frame.kind == DL_DATA)          // DATA FRAME
    {
        // SET THE TIMER PRIOR TO SENDING
        printf("DATA TRANSMITTED\nTO:\t%s\n", nodenames[frame.destNode]);
        set_timer(frame);
    }
    else                                // ACK FRAME
    {
        // NO TIMER, SIMPLY SEND THE FRAME OUT
        printf("\nACK TRANSMITTED\n");
    }

    printf("VIA LINK:\t%d\nSEQ NO:\t%d\n\n", frame.link, frame.seq);

    // CALCULATE THE CHECKSUM AND ADD TO THE FRAME
    frame.checksum = 0;
    frameLength = FRAME_SIZE(frame);
    frame.checksum = CNET_ccitt((unsigned char *)&frame, frameLength);

    // SEND FRAME VIA PHYSICAL LAYER
    CHECK(CNET_write_physical(frame.link, (char *)&frame,  &frameLength));
}

//**************************************************************************
// DATA LINK LAYER - UP
//**************************************************************************

// FUNCTION: datalink_up
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Send frame up to network layer and acknowledge receival

static void datalink_up(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    FRAME frame;
    int link, checksum;

    // READ PHYSICAL LAYER FRAME
    size_t frameLength = sizeof(FRAME);
    CHECK(CNET_read_physical(&link, (char *)&frame, &frameLength));

    // ENSURE CHECKSUM WAS OKAY
    checksum = frame.checksum;
    frame.checksum = 0;

    if (CNET_ccitt((unsigned char *)&frame, (int)frameLength) != checksum)
    {
        // CHECKSUM IS BAD SO DROP THE PACKET. SENDER WILL TIMEOUT + RESEND
        printf("\t\t\t\t\tFRAME RECEIVED\n\t\t\t\t\tBAD CHECKSUM - IGNORE\n");
        return;
    }

    if (frame.kind == DL_DATA)
    {
        // PRINT FULL INFO OF ALL DATA WITHIN THE FRAME
        printf("\n\t\t\t\t\tDATA RECEIVED\n");
        printf("\t\t\t\t\tSOURCE:\t%s\n", nodenames[frame.srcNode]);
        printf("\t\t\t\t\tDEST:\t%s\n", nodenames[frame.destNode]);
        printf("\t\t\t\t\tIN LINK:\t%d\n", link);
        printf("\t\t\t\t\tSEQ NO:\t%d\n", frame.seq);
        printf("\t\t\t\t\tDATA:\t%s\n", frame.data);
        printf("\t\t\t\t\tLENGTH:\t%d\n", frame.len);

        // CHECK IF SEQUENCE NUMBER OF FRAME IS THE ONE WE EXPECT
        if (frame.seq == frameExpected[link - 1])
        {
            // ACCEPT IT, THROW FRAME UP TO THE NETWORK LAYER
            network_up(frame, link);
        }
        // CHECK IF FRAME IS A DUPLICATE
        // MATHS CHECKS IF SEQ == FRAMEEXPECTED - 1, ACCOUNTING FOR WRAPAROUND
        else if (frame.seq == (frameExpected[link - 1] + MAX_SEQ) % (MAX_SEQ + 1))
        {
            printf("\t\t\t\t\tDUPLICATE FRAME\n");
            printf("\t\t\t\t\tEXPECTED: %d\n", frameExpected[ link - 1]);

            // DUPLICATE MUST BE BECAUSE ACK LOST, RESEND THE ACK
            transmit_ack(link, frame.seq);
        }
    }
    // IF NOT DATA, MUST BE AN ACK FRAME
    else
    {
        // DEAL WITH ACK VIA GO-BACK-N
        ack_received(frame, link);
    }
}

//**************************************************************************

// FUNCTION: ack_received
// IMPORT: frame (FRAME), link (int)
// PURPOSE: Deal with an ACK frame using the Go-Back-N approach

static void ack_received(FRAME frame, int link)
{
    FRAME tempFrame;
    int first, second, third, fourth;

    // PRINT ACKOWLEDGEMENT MESSAGE
    printf("\n\t\t\t\t\tACK RECEIVED\n");
    printf("\t\t\t\t\tIN LINK:%d\n", link);
    printf("\t\t\t\t\tSEQ NO:\t%d\n", frame.seq);

    // ENSURE ACK NUMBER IS BETWEEN ACK EXPECTED AND NEXT FRAME TO SEND
    if (between(ackExpected[link - 1], frame.seq, nextFrameToSend[link - 1]))
    {

        // LOOP UNTIL ACKEXPECTED IS ONE MORE THAN THE SEQNUM OF THE ACK
        while (between(ackExpected[link - 1], frame.seq, nextFrameToSend[link - 1]))
        {
            // STOP THE TIMER FOR THAT FRAME TO PREVENT A TIMEOUT
            CNET_stop_timer(timers[link - 1][ackExpected[link - 1]]);
            // INCREMENT ACKEXPECTED AND DECREASE NUMBER IN WINDOW
            inc(&ackExpected[link - 1]);
            numInWindow[link - 1] -= 1;
        }
    }
    else
    {
        // SHOULD NEVER OCCUR IF CODING IS CORRECT. STILL CHECK REGARDLESS
        printf("\n\t\t\t\t\tERROR: OUTSIDE WINDOW BOUNDS\n");
    }

    // ENSURE WINDOW SIZE IS VALID AND BUFFER IS NOT EMPTY
    while (numInWindow[link - 1] < MAX_SEQ && numInBuffer[link - 1] > 0)
    {
        // ADD FRAMES FROM THE BUFFER TO THE WINDOW
        printf("\t\t\t\t\tSENDING FRAME FROM BUFFER\n");
        //print_buffers(link);

        // REMOVE FRAME FROM THE FRONT OF THE BUFFER
        tempFrame = buffer[link - 1][bufferBounds[link - 1][0]];
        inc(&bufferBounds[link - 1][0]);
        numInBuffer[link - 1] -= 1;

        // STORE THE FRAME FROM THE BUFFER IN THE WINDOW
        tempFrame.seq = nextFrameToSend[link - 1];
        window[link - 1][nextFrameToSend[link - 1]] = tempFrame;
        numInWindow[link - 1] += 1;

        // TRANSMIT THE FRAME FROM THE BUFFER (NOW IN THE WINDOW)
        tempFrame.link = get_route(tempFrame.destNode);
        transmit_frame(tempFrame);
        inc(&nextFrameToSend[link - 1]);
    }

    // IF WINDOW NOT FULL AND BUFFER IS EMPTY
    first  = ( numInBuffer[0] == 0 ) && ( numInWindow[0] < MAX_SEQ );
    second = ( numInBuffer[1] == 0 ) && ( numInWindow[1] < MAX_SEQ );
    third  = ( numInBuffer[2] == 0 ) && ( numInWindow[2] < MAX_SEQ );
    fourth = ( numInBuffer[3] == 0 ) && ( numInWindow[3] < MAX_SEQ );

    // REENABLE APPLICATION LAYER TO GENERATE MESSAGES AGAIN
    if ( first && second && third && fourth )
    {
        CHECK(CNET_enable_application(ALLNODES));
        for ( int ii = 0; ii < nodeinfo.nlinks; ii++ )
            CNET_set_LED(ii, "green" );
    }

            
    print_buffers(link);
}

//**************************************************************************
// TIMEOUTS
//**************************************************************************

// FUNCTION: set_timer
// IMPORT: frame (FRAME)
// PURPOSE: Set timer for a given node and frame before sending

static void set_timer(FRAME frame)
{
    CnetTimerID timerID;
    CnetTime timeout;
    int link = frame.link;
    int seqNum = frame.seq;

    // CALCULATE TIMEOUT VALUE BASED ON BANDWIDTH + PROP DELAY
    timeout = FRAME_SIZE(frame)*((CnetTime)MAGIC_NUM / linkinfo[link].bandwidth);
    timeout += linkinfo[link].propagationdelay;

    // START SPECIFIC TIMER FOR CORRECT LINK. EVERY LINK USES A SEPERATE
    // TIMER QUEUE
    switch (link)
    {
        case 1:
            timerID = CNET_start_timer(EV_TIMER1, T_MOD * timeout, (CnetData)seqNum);
            break;
        case 2:
            timerID = CNET_start_timer(EV_TIMER2, T_MOD * timeout, (CnetData)seqNum);
            break;
        case 3:
            timerID = CNET_start_timer(EV_TIMER3, T_MOD * timeout, (CnetData)seqNum);
            break;
        case 4:
            timerID = CNET_start_timer(EV_TIMER4, T_MOD * timeout, (CnetData)seqNum);
            break;
    }

    // STORE TIMER ID IN THE TIMER ARRAY FOR LATER REFERENCE
    timers[link - 1][seqNum] = timerID;
}

//**************************************************************************

// FUNCTION: timeout_link_1
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Re-transmit any frame that timeouts and forces timeout event

static void timeout_link_1(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    FRAME frame;
    frame.link = 1;

    // GET FRAME THAT TIMED OUT FROM WINDOW + RESEND FRAME ON LINK 1
    int seqNum = (int)data;
    printf("TIMEOUT:\nOUT LINK: %d\nSEQ NO: %d\n\n", frame.link, seqNum);
    frame = window[frame.link - 1][seqNum];
    transmit_frame(frame);

    print_buffers(frame.link);
}

//**************************************************************************

// FUNCTION: timeout_link_2
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Re-transmit any frame that timeouts and forces timeout event

static void timeout_link_2(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    FRAME frame;
    frame.link = 2;

    // GET FRAME THAT TIMED OUT FROM WINDOW + RESEND FRAME ON LINK 2
    int seqNum = (int)data;
    printf("TIMEOUT:\nOUT LINK: %d\nSEQ NO: %d\n\n", frame.link, seqNum);
    frame = window[frame.link - 1][seqNum];
    transmit_frame(frame);

    print_buffers(frame.link);
}

//**************************************************************************

// FUNCTION: timeout_link_3
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Re-transmit any frame that timeouts and forces timeout event

static void timeout_link_3(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    FRAME frame;
    frame.link = 3;

    // GET FRAME THAT TIMED OUT FROM WINDOW + RESEND FRAME ON LINK 3
    int seqNum = (int)data;
    printf("TIMEOUT:\nOUT LINK: %d\nSEQ NO: %d\n\n", frame.link, seqNum);
    frame = window[frame.link - 1][seqNum];
    transmit_frame(frame);

    print_buffers(frame.link);
}

//**************************************************************************

// FUNCTION: timeout_link_4
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Re-transmit any frame that timeouts and forces timeout event

static void timeout_link_4(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    FRAME frame;
    frame.link = 4;

    // GET FRAME THAT TIMED OUT FROM WINDOW + RESEND FRAME ON LINK 4
    int seqNum = (int)data;
    printf("TIMEOUT:\nOUT LINK: %d\nSEQ NO: %d\n\n", frame.link, seqNum);
    frame = window[frame.link - 1][seqNum];
    transmit_frame(frame);

    print_buffers(frame.link);
}


//**************************************************************************
// NODE INITIALIZATION
//**************************************************************************

// FUNCTION: reboot_node
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Initalization for every node, set event handlers

void reboot_node(CnetEvent ev, CnetTimerID timer, CnetData data)
{
    // SET EVENT HANDLERS
    CHECK(CNET_set_handler( EV_APPLICATIONREADY,  transport_down,   0));
    CHECK(CNET_set_handler( EV_PHYSICALREADY,     datalink_up,      0));
    CHECK(CNET_set_handler( EV_TIMER1,		      timeout_link_1,   0));
    CHECK(CNET_set_handler( EV_TIMER2,		      timeout_link_2,   0));
    CHECK(CNET_set_handler( EV_TIMER3,		      timeout_link_3,   0));
    CHECK(CNET_set_handler( EV_TIMER4,            timeout_link_4,   0));
    CHECK(CNET_set_handler( EV_DRAWFRAME,         draw_frame,       0));    

    // INITIALISE TIMER ARRAY TO NULL TIMERS
    for ( int ii = 0; ii < MAX_LINKS; ii++ )
        for ( int jj = 0; jj < MAX_SEQ + 1; jj++ )
            timers[ii][jj] = NULLTIMER;

    // ENABLE ALL NODES TO SEND TO ALL OTHER NODES
    CHECK(CNET_enable_application(ALLNODES));

    // SET LEDS FOR LINKS TO ALL GREEN INITIALLY
    for ( int jj = 0; jj < nodeinfo.nlinks; jj++ )
        CNET_set_LED( jj, "green" );

    // SET LEDS FOR NON-USED LIGHTS TO ALL BLACK INITIALLY
    for ( int kk = nodeinfo.nlinks; kk < CNET_NLEDS; kk++ )
        CNET_set_LED( kk, "black" );
}

//**************************************************************************
// MISC FUNCTIONS
//**************************************************************************

// FUNCTION: draw_frame
// IMPORT: ev (CnetEvent), timer (CnetTimerID), data (CnetData)
// PURPOSE: Pre-built CNET functionality to draw frame travelling across
//          links

static void draw_frame(CnetEvent ev, CnetTimerID timer, CnetData data)
{

    // NOT MY CODE, THIS IS CHRIS MACDONALDS FUNCTION, REFERENCED IN REPORT
    // DID NOT MODIFY TERNARY'S FOR THIS REASON

    CnetDrawFrame *df  = (CnetDrawFrame *)data;
    FRAME         *f   = (FRAME *)df->frame;

    switch (f->kind) 
    {
        case DL_ACK:
            df->nfields    = 1;
            df->colours[0] = (f->seq == 0) ? "red" : "purple";
            df->pixels[0]  = 10;
            sprintf(df->text, "ack=%d", f->seq);
            break;

        case DL_DATA:
            df->nfields    = 2;
            df->colours[0] = (f->seq == 0) ? "red" : "purple";
            df->pixels[0]  = 10;
            df->colours[1] = "green";
            df->pixels[1]  = 30;
            sprintf(df->text, "data=%d", f->seq);
            break;
    }
}

//**************************************************************************

// FUNCTION: inc
// IMPORT: num (int*)
// PURPOSE: Increment a sequence number, resetting to 0 if above max seqnum

static void inc(int* seqNum)
{
    // INCREMENT IF POSSIBLE
    if (*seqNum < MAX_SEQ)
        *seqNum += 1;
    // RESET SEQUENCE NUMBER IF OVER MAXIMUM
    else
        *seqNum = 0;
}

//**************************************************************************

// FUNCTION: between
// IMPORT: a,b,c (ints)
// PURPOSE: Ensures that a frame is between the lower and upper bounds

static int between(int lower, int num, int upper)
{
    // THREE DIFFERENT SCENARIOS TO ACCOUNT FOR
    int first = ( lower <= num ) && ( num < upper );
    int second =  ( upper < lower ) && ( lower <= num );
    int third = ( num < upper ) && ( upper < lower );

    // EXPORT AS SIMPLE BOOLEAN VALUE
    return ( first || second || third );
}

//**************************************************************************

// FUNCTION: print_buffers
// IMPORT: link (int)
// PURPOSE: Print values of the windows and the buffer of a given link

static void print_buffers(int link)
{
    // PRINT SEQUENCE NUMBER OF ALL FRAMES IN THE WINDOW
    printf("WINDOW: [%d", window[link - 1][0].seq );
    for (int ii = 1; ii < MAX_SEQ + 1; ii++)
    {
        printf("|");        
        printf("%d", window[link - 1][ii].seq);      
    }
    printf("]\n");


    // PRINT WINDOW START AND END BOUNDS, LENGTH OF WINDOW
    printf("WINDOW LENGTH:\t[%d|%d] = %d\n", ackExpected[link - 1],
                                    nextFrameToSend[link - 1],
                                    numInWindow[link - 1]);
    // PRINT BUFFER START AND END BOUNDS, LENGTH OF BUFFER
    printf("BUFFER LENGTH:\t[%d|%d] = %d\n\n", bufferBounds[link - 1][0],
                                    bufferBounds[link - 1][1],
                                    numInBuffer[link - 1]);
}

//**************************************************************************