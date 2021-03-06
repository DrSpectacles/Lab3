/* Functions to implement a link layer protocol:
   LL_connect() connects to another computer;
   LL_discon()  disconnects;
   LL_send()    sends a block of data;
   LL_receive() waits to receive a block of data;
   LL_getOptBlockSize()  returns the optimum size of data block
   All functions take a debug argument - if non-zero, they print
   messages explaining what is happening.  Regardless of debug,
   functions print messages when things go wrong.
   LL_send and LL_receive behave in a simpler way if debug is 1,
   to facilitate testing of some parts of the protocol.
   All functions return negative values on failure.
   Definitions of constants are in the header file.  */


#include <stdio.h>      // input-output library: print & file operations
#include <time.h>       // for timing functions
#include "physical.h"   // physical layer functions
#include "linklayer.h"  // these functions

/* These variables need to retain their values between function calls, so they
   are declared as static.  By declaring them outside any function, they are
   made available to all the functions in this file.  */
static int seqNumTx;        // sequence number of transmit data block
static int lastSeqRx;       // sequence number of last good block received
static int connected = FALSE;   // keep track of state of connection
static int framesSent = 0;  // count of frames sent
static int acksSent = 0;    // count of ACKs sent
static int naksSent = 0;    // count of NAKs sent
static int acksRx = 0;      // count of ACKs received
static int naksRx = 0;      // count of NAKs received
static int badFrames = 0;   // count of bad frames received
static int goodFrames = 0;  // count of good frames received
static int timeouts = 0;    // count of timeouts
static long timerRx;        // time value for timeouts at receiver
static long connectTime;    // time when connection was established

// ===========================================================================
/* Function to connect to another computer.
   It just calls PHY_open() and reports any problem.
   It also initialises counters for debug purposes.  */
int LL_connect(int portNum, int debug)
{
    // Try to connect using port number given, bit rate as in header file,
    // always uses 8 data bits, no parity, fixed time limits.
    int retCode = PHY_open(portNum,BIT_RATE,8,0,1000,50,PROB_ERR);
    if (retCode == SUCCESS)   // check if succeeded
    {
        connected = TRUE;   // record that we are connected
        seqNumTx = 0;       // set first sequence number for sender
        lastSeqRx = -1;     // set an impossible value for last seq. received
        framesSent = 0;     // initialise all counters for this new connection
        acksSent = 0;
        naksSent = 0;
        acksRx = 0;
        naksRx = 0;
        badFrames = 0;
        goodFrames = 0;
        timeouts = 0;
        connectTime = clock();  // capture time when connection was established
        if (debug) printf("LL: Connected\n");
        return SUCCESS;
    }
    else  // failed
    {
        connected = FALSE;  // record that we are not connected
        printf("LL: Failed to connect, PHY returned code %d\n",retCode);
        return -retCode;  // return a negative value to indicate failure
    }
}


// ===========================================================================
/* Function to disconnect from the other computer.
   It just calls PHY_close() and prints a report of what happened.  */
int LL_discon(int debug)
{
    long elapsedTime = clock() - connectTime;  // measure time connected
    float connTime = ((float) elapsedTime ) / CLOCKS_PER_SEC; // convert to seconds
    int retCode = PHY_close();  // try to disconnect
    connected = FALSE;  // assume we are no longer connected
    if (retCode == SUCCESS)   // check if succeeded
    {
        // Print the report - have to print all the counters,
        // as we don't know if we were sending or receiving
        printf("\nLL: Disconnected after %.2f s.  Sent %d data frames\n",
               connTime, framesSent);
        printf("LL: Received %d good and %d bad frames, had %d timeouts\n",
               goodFrames, badFrames, timeouts);
        printf("LL: Sent %d ACKs and %d NAKs\n", acksSent, naksSent);
        printf("LL: Received %d ACKs and %d NAKs\n", acksRx, naksRx);
        return SUCCESS;
    }
    else  // failed
    {
        printf("LL: Failed to disconnect, PHY returned code %d\n", retCode);
        return -retCode;  // return negative value to indicate failure
    }
}


// ===========================================================================
/* Function to send a block of data in a frame.
   Arguments:  dataTx is a pointer to an array of data bytes,
               nTXdata is the number of data bytes to send,
               debug sets the mode of operation and controls printing.
   The return value indicates success or failure.
   If connected, builds a frame, then sends the frame using PHY_send.
   If debug is 1 (simple mode), it regards this as success, and returns.
   Otherwise, it waits for a reply, up to a time limit.
   What happens after that is for you to decide...  */
int LL_send(byte_t *dataTx, int nTXdata, int debug)
{
    static byte_t frameTx[3*MAX_BLK];  // array large enough for frame
    static byte_t frameAck[2*ACK_SIZE]; // twice expected ack frame size
    int sizeTXframe = 0;    // size of frame being transmitted
    int sizeAck = 0;        // size of ACK frame received
    int seqAck;             // sequence number in response received
    int attempts = 0;       // number of attempts to send this data
    int success = FALSE;    // flag to indicate block sent and ACKed
    int retVal;             // return value from other functions

    // First check if connected
    if (connected == FALSE)
    {
        printf("LLS: Attempt to send while not connected\n");
        return BADUSE;  // problem code
    }

    // Then check if block size OK - adjust limit for your design
    if (nTXdata > MAX_BLK)
    {
        printf("LLS: Cannot send block of %d bytes, max block size %d\n",
               nTXdata, MAX_BLK);
        return BADUSE;  // problem code
    }

    // Build the frame - sizeTXframe is the number of bytes in the frame
    sizeTXframe = buildDataFrame(frameTx, dataTx, nTXdata, seqNumTx);

    // Then loop, sending the frame and maybe waiting for response
    do
    {
        // Send the frame, then check for problems
        retVal = PHY_send(frameTx, sizeTXframe);  // send frame bytes
        if (retVal != sizeTXframe)  // problem!
        {
            printf("LLS: Block %d, failed to send frame\n", seqNumTx);
            return FAILURE;  // problem code
        }

        framesSent++;  // increment frame counter (for report)
        attempts++;    // increment attempt counter, so we don't try forever
        if (debug) printf("LLS: Sent frame of %d bytes, block %d, attempt %d\n",
                          sizeTXframe, seqNumTx, attempts);

        // In simple mode, this is all we have to do (there are no responses)
        if (debug == SIMPLE)
        {
            success = TRUE;  // set success to true to end the loop
            continue;       // and go straight to the while statement
        }

        // Otherwise, we must wait to receive a response (ack or nak)
        sizeAck = getFrame(frameAck, 2*ACK_SIZE, TX_WAIT);
        if (sizeAck < 0)  // some problem receiving
        {
            return FAILURE;  // quit if failed
        }
        if (sizeAck == 0)  // this means timeout
        {
            if (debug) printf("LLS: Timeout waiting for response\n");
            timeouts++;  // increment counter for report
            // What else should be done about that (if anything)?
            // If success remains FALSE, this loop will continue, so
            // it will re-transmit the frame and wait for a response...
        }
        else  // we have received a frame - check it
        {
            if (checkFrame(frameAck, sizeAck) == FRAMEGOOD)  // good frame
            {
                goodFrames++;  // increment counter for report
                // Extract some information from the response
                seqAck = (int) frameAck[SEQNUMPOS]; // extract the sequence number
                // If there is more than one type of response, extract the type
                // Need to check if this is a positive ACK,
                // and if it relates to the data block just sent...
                if ( seqAck == seqNumTx )  // need a sensible test here!!
                {
                    if (debug) printf("LLS: ACK received, seq %d\n", seqAck);
                    acksRx++;           // increment counter for report
                    success = TRUE;     // job is done
                }
                else // could be NAK, or ACK for wrong block...
                {
                    if (debug) printf("LLS: Response received, type %d, seq %d\n",
                            99, 99 );  // need sensible values here!!
                    naksRx++;          // increment counter for report
                    // What else should be done about this (if anything)?
                    // If success remains FALSE, this loop will continue, so
                    // it will re-transmit the frame and wait for a response...
               }
            }
            else  // bad frame received - errors found
            {
                badFrames++;  // increment counter for report
                if (debug) printf("LLS: Bad frame received\n");
                // No point in trying to extract anything from a bad frame.
                // What else should be done about this (if anything)?
                // If success remains FALSE, this loop will continue, so
                // it will re-transmit the frame and wait for a response...
            }

        }  // end of received frame processing
    }   // repeat all this until succeed or reach the limit
    while ((success == FALSE) && (attempts < MAX_TRIES));

    if (success == TRUE)  // the data block has been sent and acknowledged
    {
        seqNumTx = next(seqNumTx);  // increment the sequence number
        return SUCCESS;
    }
    else    // maximum number of attempts has been reached, without success
    {
        if (debug) printf("LLS: Block %d, tried %d times, failed\n",
                          seqNumTx, attempts);
        return GIVEUP;  // tried enough times, giving up
    }

}  // end of LL_send


// ===========================================================================
/* Function to receive a frame and extract a block of data.
   Arguments:  dataRx is a pointer to an array to hold the data block,
               maxData is the maximum size of the data block,
               debug sets the mode of operation and controls printing.
   The return value is the size of the data block, or negative on failure.
   If connected, try to get a frame from the received bytes.
   If a frame is found, check if it is a good frame, with no errors.
   In simple mode, for good frames, data is extracted and the function returns.
   For bad frames, a block of ten # characters is returned as the data.
   In normal mode, the sequence number is also checked, and the function
   loops until it gets a good frame with the expected sequence number,
   then returns with the data bytes from the frame.  */
int LL_receive(byte_t *dataRx, int maxData, int debug)
{
    static byte_t frameRx[3*MAX_BLK];  // create an array to hold the frame
    int nRXdata = 0;      // number of data bytes received
    int sizeRXframe = 0;  // number of bytes in the frame received
    int seqNumRx = 0;     // sequence number of the received frame
    int success = FALSE;  // flag to indicate success
    int attempts = 0;     // attempt counter
    int i = 0;            // used in for loop
    int expected = next(lastSeqRx);  // calculate expected sequence number

    // First check if connected
    if (connected == FALSE)
    {
        printf("LLR: Attempt to receive while not connected\n");
        return BADUSE;  // problem code
    }

    /* Loop to receive a frame, repeats until a frame is received.
       In normal mode, repeats until a good frame with the expected
       sequence number is received. */
    do
    {
        // First get a frame, up to size of frame array, with time limit.
        // getFrame unction returns the number of bytes in the frame,
        // or zero if it did not receive a frame within the time limit
        // or a negative value if there was some other problem.
        sizeRXframe = getFrame(frameRx, 3*MAX_BLK, RX_WAIT);
        if (sizeRXframe < 0)  // some problem receiving
        {
            return FAILURE;  // quit if there was a problem
        }

        attempts++;  // increment attempt counter
        if (sizeRXframe == 0)  // that means a timeout occurred
        {
            printf("LLR: Timeout trying to receive frame, attempt %d\n",
								attempts);
            timeouts++; // increment the counter for the report
            // No frame was received, so no response is needed
            // If success remains FALSE, loop will continue and try again...
        }
        else  // we have received a frame
        {
            if (debug) printf("LLR: Got frame, %d bytes, attempt %d\n",
								        sizeRXframe, attempts);

            // Next step is to check it for errors
            if (checkFrame(frameRx, sizeRXframe) == FRAMEBAD ) // frame is bad
            {
                badFrames++;  // increment bad frame counter
                if (debug) printf("LLR: Bad frame received\n");
                if (debug) printFrame(frameRx, sizeRXframe);
                // In simple mode, just return some dummy data
                if (debug == SIMPLE)  // simple mode
                {
                    success = TRUE;  // pretend this is a success
                    // Put some dummy bytes in the data array
                    for (i=0; i<10; i++) dataRx[i] = 35; // # symbol
                    nRXdata = 10;     // number of dummy bytes
                }
                else
                {
                // In normal mode, this is not a success!
                // Maybe send a response to the sender ?
                // If so, what sequence number ?
                // See the sendAck() function below.
                }

            }
            else  // we have a good frame - process it
            {
                goodFrames++;  // increment good frame counter
                // Extract the data bytes and the sequence number
                nRXdata = processFrame(frameRx, sizeRXframe, dataRx,
                                     maxData, &seqNumRx);
                if (debug) printf("LLR: Received block %d with %d data bytes\n",
                                  seqNumRx, nRXdata);

                // In simple mode, just accept the data - no further checking
                if (debug == SIMPLE) success = TRUE;
                // In normal mode, need to check the sequence number
                else if (seqNumRx == expected)  // got the expected data block
                {
                    success = TRUE;  // job is done
                    lastSeqRx = seqNumRx;  // update last sequence number
                    // Maybe send a response to the sender ?
                    // If so, what sequence number ?
                    // See the sendAck() function below.
		    sendAck(POSACK, seqNumRx, debug); //ADDED

                }
                else if (seqNumRx == lastSeqRx) // got a duplicate data block
                {
                    if (debug) printf("LLR: Duplicate rx seq. %d, expected %d\n",
                                  seqNumRx, expected);
                    // What should be done about this?

		    //overwrite prev sent
		    success = TRUE;  // job is done
                    lastSeqRx = seqNumRx;  // update last sequence number
		    sendAck(POSACK, seqNumRx, debug); //ADDED

                }
                else // some other data block??
                {
                    if (debug) printf("LLR: Unexpected block rx seq. %d, expected %d\n",
                                  seqNumRx, expected);
                    // What should be done about this?
		    sendAck(POSACK, lastSeqRx, debug);
		    success = FALSE;

                }  // end of sequence number checking

            }  // end of good frame processing

        } // end of received frame processing
    }   // repeat all this until succeed or reach the limit
    while ((success == FALSE) && (attempts < MAX_TRIES));

    if (success == TRUE)  // received good frame with expected sequence number
        return nRXdata;  // return number of data bytes extracted from frame
    else // failed to get a good frame within limit
    {
        if (debug) printf("LLR: Tried to receive a frame %d times, failed\n",
                          attempts);
        return GIVEUP;  // tried enough times, giving up
    }

}  // end of LL_receive

// ===========================================================================
/* Function to return the optimum size of a data block for this protocol.
   The optimum size is defined in the header file by the protocol designer.
   Arguments: debug controls printing
   The return value is the optimum numer of data bytes in a frame.  */
int LL_getOptBlockSize(int debug)
{
    if (debug) printf("LLGOBS: Optimum size of data block is %d bytes\n",
                      OPT_BLK);
    return OPT_BLK;
}


// ===========================================================================
/* Function to build a frame around a block of data.
   This function puts the header bytes into the frame, then copies in the
   data bytes.  Then it adds the trailer bytes to the frame.
   It calculates the total number of bytes in the frame, and returns this
   value to the calling function.
   Arguments: frameTx is a pointer to an array to hold the frame,
              dataTx is the array of data bytes to be put in the frame,
              nData is the number of data bytes to be put in the frame,
              seq is the sequence number to include in the frame header.
   The return value is the total number of bytes in the frame.  */
int buildDataFrame(byte_t *frameTx, byte_t *dataTx, int nData, int seq)
{
    int i = 0;  // for use in loo
    int checkSum = 0; //for error detection
    int frameSize = HEADERSIZE+TRAILERSIZE+nData;
    
    // Build the frame header first
    frameTx[0] = STARTBYTE;         // start of frame marker byte
    frameTx[FRSPOS] = frameSize;
    frameTx[SEQNUMPOS] = (byte_t) seq;  // sequence number as given
    checkSum += seq; //sequence number added to checksum
    checkSum += frameSize; //framesize added to checksum

    printf("framesize was %d \n", frameTx[FRSPOS]);

    // Copy the data bytes into the frame, starting after the header
    for (i = 0; i < nData; i++)     // step through the data array
    {
        frameTx[HEADERSIZE+i] = dataTx[i];  // copy a data byte
	checkSum += dataTx[i]; //and add same byte to cS
	
    }
    checkSum = checkSum % MODULO; //checksum calculated using MODULO from linklayer.h
    printf("CHECKSUM is %d\n", checkSum);
    // Add the trailer to the frame
     
    // Need to include some error detection system - one or more check
    // bytes in the trailer, either before or after the end marker.

    // Add the end marker - for now, this goes after the last data byte
    frameTx[HEADERSIZE+nData] = checkSum; // end of frame marker byte

    frameTx[HEADERSIZE+nData+1] = ENDBYTE;
    
    // Return the size of the frame
    return HEADERSIZE+nData+TRAILERSIZE;
}


// ===========================================================================
/* Function to find and extract a frame from the received bytes.
   Arguments: frameRx is a pointer to an array of bytes to hold the frame,
              maxSize is the maximum number of bytes to receive,
              timeLimit is the time limit for receiving a frame.
   The return value is the number of bytes in the frame,
   zero if time limit or size limit was reached before frame received,
   or a negative value if there was some other problem. */
int getFrame(byte_t *frameRx, int maxSize, float timeLimit)
{
    int nRx = 0;  // number of bytes received so far
    int retVal = 0;  // return value from other functions
    int frameSize = 0;

    timerRx = timeSet(timeLimit);  // set time limit to wait for frame

    // First search for the start of frame marker
    do
    {
        retVal = PHY_get(frameRx, 1); // get one byte at a time
        // Return value is number of bytes received, or negative for problem
        if (retVal < 0) return retVal;  // check for problem and give up
        else nRx += retVal;  // otherwise update the bytes received count
     }
    while (((retVal < 1) || (frameRx[0] != STARTBYTE)) && !timeUp(timerRx));
    // until we get a byte which is a start of frame marker, or timeout

    // If we are out of time, without finding the start marker,
    // report the facts, but return 0 - no useful bytes received
    if (timeUp(timerRx))
    {
        printf("LLGF: Timeout seeking START, %d bytes received\n", nRx);
        return 0;  // no frame received, but not a failure situation
    }
    
    retVal = PHY_get((frameRx + 1), 1);  // get one byte at a time
    if (retVal < 0) return retVal;  // check for problem and give up
    else nRx += retVal;  // otherwise update the bytes received count
    frameSize = frameRx[FRSPOS];

    printf("RxFramesize was %d \n", frameSize);

    nRx = 2;  // if we found the start marker, we got 1 byte

    for (int i = 0; i < frameSize-2; i++) {

      retVal = PHY_get((frameRx + nRx), 1);  // get one byte at a time
      if (retVal < 0) return retVal;  // check for problem and give up
      else nRx += retVal;  // otherwise update the bytes received count
      
    }

    printf("nRx was %d \n", nRx);
	
    // until we get the end of frame marker or reach time limit or size limit

    // If we reached the time limit, without finding the end marker,
    // this will be a bad frame, so report the facts but return 0
    if (timeUp(timerRx))
    {
        printf("LLGF: Timeout seeking END, %d bytes received\n", nRx);
        return 0;  // no frame received, but not a failure situation
    }

    // If we filled the frame array, without finding the end marker,
    // this will be a bad frame, so so report the facts but return 0
    if (nRx >= maxSize)
    {
        printf("LLGF: Size limit seeking END, %d bytes received\n", nRx);
        return 0;  // no frame received, but not a failure situation
    }

    // Otherwise, we found the end marker
    return nRx;  // return the number of bytes in the frame
}  // end of getFrame


// ===========================================================================
/* Function to check a received frame for errors.
   Arguments: frameRx is a pointer to an array of bytes holding a frame,
              sizeFrame is the number of bytes in the frame.
   As a minimum, this function should check the error detecting code.
   This example just checks the start and end markers.
   The return value indicates if the frame is good or bad.  */
int checkFrame(byte_t *frameRx, int sizeFrame)
{
  //Declare local and Rx Checksums, read transmitted Checksum
  int checkSum_Rx = frameRx[sizeFrame-2];
  int checkSum_Lcl = 0;
  
  //Calculate local checksum via with MODULO from linklayer.h
    for (int i = 0; i < sizeFrame-3 ; i++) {
	
        checkSum_Lcl += frameRx[(HEADERSIZE-2)+i]; 
      
    }
    checkSum_Lcl = checkSum_Lcl % MODULO;

    //if checkSums do not match, return error message && "FRAMEBAD"
    if (checkSum_Lcl != checkSum_Rx) {

        printf("LLCF:  Frame bad - Checksum Mismatch\n");
	return FRAMEBAD;
    }

    //If start marker not found on data, return message && "FRAMEBAD" 
    if (frameRx[0] != STARTBYTE)
    {
        printf("LLCF: Frame bad - no start marker\n");
        return FRAMEBAD;
    }

    // Check the end-of-frame marker in the last byte
    if (frameRx[sizeFrame-1] != ENDBYTE)
    {
        printf("LLCF: Frame bad - no end marker\n");
        return FRAMEBAD;
    }

    // Need to check the error detecting code here.
    // If this check fails, print a message and return FRAMEBAD

    // If all tests are passed, indicate a good frame
    return FRAMEGOOD;
}  // end of checkFrame


// ===========================================================================
/* Function to process a received frame, to extract the data & sequence number.
   The frame has already been checked for errors, so this simple
   implementation assumes everything is where is should be.
   Arguments: frameRx is a pointer to the array holding the frame,
              sizeFrame is the number of bytes in the frame,
              dataRx is a pointer to an array to hold the data bytes,
              maxData is the max number of data bytes to extract,
              seqNum is a pointer to the sequence number.
   The return value is the number of data bytes extracted. */
int processFrame(byte_t *frameRx, int sizeFrame,
                 byte_t *dataRx, int maxData, int *seqNum)
{
    int i = 0;  // for use in loop
    int nRXdata;  // number of data bytes in the frame

    // First get the sequence number from its place in the header
    *seqNum = (int) frameRx[SEQNUMPOS];

    // Calculate the number of data bytes, based on the frame size
    nRXdata = sizeFrame - HEADERSIZE - TRAILERSIZE;

    // Check if this is within the limit given
    if (nRXdata > maxData) nRXdata = maxData;  // limit to the max allowed

    // Now copy the data bytes from the middle of the frame
    for (i = 0; i < nRXdata; i++)
    {
        dataRx[i] = frameRx[HEADERSIZE + i];  // copy one byte
    }

    return nRXdata;  // return the size of the data block extracted
}  // end of processFrame


// ===========================================================================
/* Function to send an acknowledgement - positive or negative.
   Arguments: type is the type of acknowledgement to send,
              seq is the sequence number that the ack should carry,
              debug controls printing of messages.
   Return value indicates success or failure.
   Note type is used to update statistics for report, so the argument
   is needed even if its value is not included in the ack frame. */
int sendAck(int type, int seq, int debug)
{
    byte_t ackFrame[2*ACK_SIZE];  // twice expected frame size, for byte stuff
    int sizeAck = 5; // number of bytes in the ack frame so far
    int retVal; // return value from functions
    int checkSum = 0;

    // First build the frame
    ackFrame[0] = STARTBYTE;
    ackFrame[FRSPOS] =sizeAck;
    ackFrame[SEQNUMPOS] = (byte_t) seq;  // sequence number as given
    checkSum += seq; //sequence number added to checksum
    checkSum += sizeAck; //framesize added to checksum

    checkSum = checkSum % MODULO; //checksum calculated using MO
    
    ackFrame[HEADERSIZE] = checkSum; // end of frame marker byte

    ackFrame[HEADERSIZE+1] = ENDBYTE;


	// Add more bytes to the frame, and update sizeAck

    // Then send the frame and check for problems
    retVal = PHY_send(ackFrame, sizeAck);  // send the frame
    if (retVal != sizeAck)  // problem!
    {
        printf("LLSA: Failed to send response, seq. %d\n", seq);
        return FAILURE;  // problem code
    }
    else  // success - update the counters for the report
    {
        if (type == POSACK)  acksSent++;
        else if (type == NEGACK) naksSent++;
        if (debug)
            printf("LLSA: Sent response of %d bytes, type %d, seq %d\n",
                        sizeAck, type, seq);
        return SUCCESS;
    }
}


// ===========================================================================
// Function to advance the sequence number, wrapping around at maximum value.
int next(int seq)
{
    return ((seq + 1) % MOD_SEQNUM);
}


// ===========================================================================
/* Function to set a time limit at a point in the future.
   limit   is the time limit in seconds (from now)  */
long timeSet(float limit)
{
    long timeLimit = clock() + (long)(limit * CLOCKS_PER_SEC);
    return timeLimit;
}  // end of timeSet


// ===========================================================================
/* Function to check if a time limit has elapsed.
   timer  is the timer variable to check
   returns TRUE if time has reached or exceeded the limit,
           FALSE if time has not yet reached the limit.   */
int timeUp(long timeLimit)
{
    if (clock() < timeLimit) return FALSE;  // still within limit
    else return TRUE;  // time limit has been reached or exceeded
}  // end of timeUP

// ===========================================================================
/* Function to check if a byte is one of the protocol bytes.
   argument b is a byte value to check
   returns TRUE if b is a protocol byte,
           FALSE if b is not a protocol byte.*/
int special(byte_t b)
{
    return FALSE;   // no checking in this version
}

// ===========================================================================
/* Function to print the bytes of a frame, in groups of 10.
   For small frames, print all the bytes,
   for larger frames, just the start and end. */
void printFrame(byte_t *frame, int sizeFrame)
{
    int i, j;

    if (sizeFrame <= 50)  // small frame - print all the bytes
    {
        for (i=0; i<sizeFrame; i+=10)  // step in groups of 10 bytes
        {
            for (j=0; (j<10)&&(i+j<sizeFrame); j++)
            {
                printf("%3d ", frame[i+j]);  // print as number
            }
            printf(":  ");  // separator
            for (j=0; (j<10)&&(i+j<sizeFrame); j++)
            {
                printf("%c", frame[i+j]);   // print as character
            }
            printf("\n");   // new line
        }  // end for
    }
    else  // large frame - print start and end
    {
        for (j=0; (j<10); j++)  // first 10 bytes
            printf("%3d ", frame[j]);  // print as number
        printf(":  ");  // separator
        for (j=0; (j<10); j++)
            printf("%c", frame[j]); // print as character
        printf("\n - - -\n");   // new line, separator
        for (j=sizeFrame-10; (j<sizeFrame); j++)  // last 10 bytes
            printf("%3d ", frame[j]);  // print as number
        printf(":  ");  // separator
        for (j=sizeFrame-10; (j<sizeFrame); j++)
            printf("%c", frame[j]); // print as character
        printf("\n");   // new line
    }

}  // end of printFrame
