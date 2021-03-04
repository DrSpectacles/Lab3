/* EEEN20060 Communication Systems, Link Layer test program
   This program opens a file and reads block of bytes from it.
   It uses the link layer functions to send each block through
   a simulated physical layer, and to receive the result.
   It writes the result to an output file for inspection.
   it can also work with the real physical layer, either
   sending or receiving as required.  */


#include <stdio.h>    // standard input-output library
#include <string.h>   // needed for string manipulation
#include <stdlib.h>   // needed for atoi()
#include <time.h>     // needed for delay function
#include "linklayer.h"  // link layer functions

#define MAX_DATA 200   // maximum data block size to use
// Keep block size reasonably small for initial tests
#define MAX_FNAME 80   // maximum file name length
#define MAX_MODE 10    // maximum length of mode input
#define END_FILE 1     // loop ends because file ended

/* Function to delay for a specified number of ms.
   This could be replaced by the Sleep() function in windows.h */
void delay(int delay_ms)
{
    clock_t endTick, delayTick;   // variables to hold clock values
    delayTick = ((clock_t) delay_ms * CLOCKS_PER_SEC) / 1000;  // delay in ticks
    endTick = clock() + delayTick; // end time in ticks
    while (clock() < endTick);  // wait until time is reached
    return; // then return
}


int main()
{
    char fName[MAX_FNAME];  // string to hold input filename
    char outName[MAX_FNAME+1] = "Z"; // string to hold output filename
    char inString[MAX_MODE];  // string to hold mode required
    char *portName;    // port number to use
    int nInput;   // length of input string
    int mode = 0;   // mode 0 = loopback, 1 = send, 2 = receive
    int sizeDataBlk;   // number of data bytes per block
    int retVal;     // return value from various functions
    FILE *fpi = NULL, *fpo = NULL;  // file handles
    byte_t dataSend[MAX_DATA+2];  // array for bytes to send
    byte_t dataReceive[MAX_DATA+2];  // array for bytes received
    int nRx = 0;    // number of bytes received
    int nRead;      // number of bytes read from the input file
    int nWrite;     // number of bytes written to the output file
    int endLoop = 0;    // why did main loop end? 1 = end file, 2 = problem
    long SendCount = 0, RxCount = 0; // more byte counters

    printf("Link Layer Test Program\n");  // welcome message

    // Ask user for file name
    printf("\nEnter name of file to use (name.ext): ");
    fgets(fName, MAX_FNAME, stdin);  // get filename
    nInput = strlen(fName);
    fName[nInput-1] = '\0';   // remove the newline at the end
    printf("\n");  // blank line

    // Set mode of operation: 0 = loopback, 1 = send, 2 = receive
    printf("\nChoose Loopback, Send or Receive (l/s/r): ");
    fgets(inString, MAX_MODE, stdin);  // get user input
    printf("\n");  // blank line
    if ((inString[0] == 's')||(inString[0] == 'S')) mode = 1;
    if ((inString[0] == 'r')||(inString[0] == 'R')) mode = 2;

    // If not loopback, ask for port number to use
    if (mode > 0)
    {
        printf("\nName of Port to use: ");
        fgets(inString, MAX_MODE, stdin);  // get user input
        portName = inString;   // set port name
	portName[strlen(portName) - 1] = '\0';
	
        printf("Program will use port %s\n", portName); // print the result
    }


    // If sending, open the input file and check for failure
    if (mode <= 1)
    {
        printf("\nMain: Opening %s for input\n", fName);
        fpi = fopen(fName, "rb");  // open for binary read
        if (fpi == NULL)
        {
            perror("Main: Failed to open input file");
            return 1;
        }
    }  // end if sending

    // If receiving, open the output file and check for failure
    if ((mode == 2) || (mode == 0))
    {
        // Output filename: add Z in front of filename given
        strcat(outName, fName);
        printf("\nMain: Opening %s for output\n", outName);
        fpo = fopen(outName, "wb");  // open for binary write
        if (fpo == NULL)
        {
            perror("Main: Failed to open output file");
            if (fpi != NULL) fclose(fpi);     // close input file
            return 1;
        }
    }

    // Ask link layer to make the connection
    printf("\nMain: Connecting...\n");
    retVal = LL_connect(portName, SIMPLE);
    if (retVal < 0)  // problem connecting
    {
        if (fpi != NULL) fclose(fpi);    // close input file
        if (fpo != NULL) fclose(fpo);    // close ouptut file
        return retVal;  // pass back the problem code
    }

    // Ask link layer for the optimum size of data block
    sizeDataBlk = LL_getOptBlockSize(SIMPLE);
    // Limit to the size of the arrays
    if (sizeDataBlk>MAX_DATA) sizeDataBlk = MAX_DATA;

    // Send the contents of the file, one block at a time
    do  // loop block by block
    {
        // This section is only needed if sending or loopback
        if (mode <= 1)
        {
            // Read bytes from input file, store in array
            nRead = (int) fread(dataSend, 1, sizeDataBlk, fpi);
            if (ferror(fpi))  // check for problem
            {
                perror("Main: Problem reading input file");
                break;    // exit the loop
            }
            printf("\nMain: Read %d bytes, sending...\n", nRead);
            SendCount += nRead;  // add to byte count

            // Send these bytes using the link layer send function
            retVal = LL_send(dataSend, nRead, SIMPLE);
            // retVal is 0 if succeeded
            if (retVal != 0) break; // if failed, need to get out of loop

            // Check for end of file, and set flag to end the loop
            if (feof(fpi) != 0) endLoop = END_FILE;

            delay(250);  // Delay 500 ms to allow progress to be seen
			// delay(250);  // Use this shorter delay for onlineGDB
			
        }  // end of sending section

        // This section is needed for receive or loopback
        if ((mode == 2) || (mode == 0))
        {

            printf("\nMain: Trying to receive bytes...\n");
            // Receive bytes from the link layer, up to the size of array
            nRx = LL_receive(dataReceive, MAX_DATA+2, SIMPLE);
            // nRx will be number of bytes received, or negative if failure
            if (nRx < 0 ) printf("Main: Problem receiving data, code %d\n", nRx);
            else if (nRx == 0) printf("Main: Zero bytes received\n");
            else // we got some data - write to output file
            {
                printf("Main: Received %d bytes, writing\n", nRx);
                nWrite = (int) fwrite(dataReceive, 1, nRx, fpo);
                if (ferror(fpo))  // check for problem
                {
                    perror("Main: Problem writing output file");
                    break;
                }
                RxCount += nWrite;
            }
        }  // end of receive section

        // This delay is only needed in loopback
        if (mode == 0) delay(2000);  // Delay 2 s between blocks 
		// if (mode == 0) delay(1000);  // Delay 1 s if using onlineGDB
    }
    while ((endLoop == 0) && (nRx >= 0)); // until input file ends or error

    // Check why the loop ended, and print message
    if (endLoop == END_FILE) printf("\nMain: End of input file\n");
    else printf("\nMain: Receive problem or error in loop\n");

    // Print statistics
    if (mode <= 1) printf("Read %ld bytes from input file\n", SendCount);
    if ((mode == 2) || (mode == 0))
        printf("Wrote %ld bytes to output file\n", RxCount);

    if (fpi != NULL) fclose(fpi);    // close input file if open
    if (fpo != NULL) fclose(fpo);    // close ouptut file if open
    LL_discon(SIMPLE);  // disconnect

    // Prompt for user input, so window stays open when run outside CodeBlocks
    printf("\nPress enter key to end:");
    fgets(inString, MAX_MODE, stdin);  // get user response

    return 0;

} // end of main
