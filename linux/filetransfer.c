/* EEEN20060 Communication Systems, Link Layer Protocol Design
   This is the application program for file transfer.
   The application layer protocol is very simple:
   the first byte of each block transferred is a header value,
   identifying the type of block. This requires that the link
   layer protocol preserve the block boundaries.
   There are 3 block types:  file name, file data, end of file marker. */


#include <stdio.h>      // standard input-output library
#include <string.h>     // needed for string manipulation
#include <stdlib.h>   // needed for atoi()
#include "linklayer.h"  // link layer functions

#define FILENAME 233  // header value for file name
#define FILEDATA 234  // header value for data
#define FILEEND 235   // header value to mark end of file
#define MAX_DATA 300  // maximum data block size to use

#define MAX_FNAME 80  // maximum file name length
#define MAX_MODE 10   // maximum length of mode input

// Function prototypes
int sendFile(char *fName, char *portName, int debug);
int receiveFile(char *portName, int debug);

int main()
{
    char fName[MAX_FNAME];   // string to hold  filename
    char inString[MAX_MODE]; // string to hold user command
    int nInput;         // length of input string
    int retVal;         // return value from functions
    char portName[MAX_MODE];        // serial port number
    int debug = FALSE;  // flag to select more printing

    printf("Link Layer Assignment - Application Program\n");  // welcome message

    // First ask if the user wants lots of debug information
    printf("\nSelect debug or quiet mode (d/q): ");
    fgets(inString, MAX_MODE, stdin);  // get user input
    if ((inString[0] == 'd')||(inString[0] == 'D')) debug = FULL;

    // Ask which port to use
    printf("\nName of port to use (eg: ttyS10): ");
    fgets(portName, MAX_MODE, stdin);  // get user input
    portName[ strlen(portName) - 1 ] = '\0';   // remove trailing newline
    
    printf("Program will use port /dev/%s\n", portName); // print the result

    // Then ask what the user wants to do
    printf("\nSelect send or receive (s/r): ");
    fgets(inString, MAX_MODE, stdin);  // get user input

    // Decide what to do, based on what the user entered
    switch (inString[0])
    {
        case 's':
        case 'S':
            printf("\nEnter name of file to send with extension (name.ext): ");
            fgets(fName, MAX_FNAME, stdin);  // get filename
            nInput = strlen(fName);
            fName[nInput-1] = '\0';   // remove the newline at the end
            printf("\n");  // blank line
            retVal = sendFile(fName, portName, debug);  // call function to send file
            if (retVal == 0) printf("\nFile sent!\n");
            else printf("\n*** Send failed, code %d\n", retVal);
            break;

        case 'r':
        case 'R':
	  retVal = receiveFile(portName, debug);  // call function to receive file
            if (retVal == 0) printf("\nFile received!\n");
            else printf("\n*** Receive failed, code %d\n", retVal);
            break;

        default:
            printf("\nCommand not recognised\n");
            break;

   } // end of switch

    // Prompt for user input, so window stays open when run outside CodeBlocks
    printf("\nPress enter key to end:");
    fgets(inString, MAX_MODE, stdin);  // get user response

    return 0;
}  // end of main


// ============================================================================
/* Function to send a file, using the link layer protocol.
   It opens the given input file, connects to another computer and sends the
   file name.  Then it reads blocks of data of fixed size from the input,
   and sends each block over the connection.  When end-of-file is reached,
   it sends an END block, then closes the connection.
   If debug is non-zero, it prints progress information,
   if debug is 0, it only prints if there is a problem.
   Returns 0 for success, or a non-zero failure code.  */

int sendFile(char *fName, char *portName, int debug)
{
    FILE *fpi;  // file handle for input file
    byte_t data[MAX_DATA+2];  // array of bytes
    int sizeDataBlk;    // number of data bytes per block
    int nByte;   // number of bytes read or found in filename
    int retVal;  // return code from functions
    long byteCount = 0; // total number of bytes read

    // Open the input file and check for failure
    if (debug) printf("\nSend: Opening %s for input\n", fName);
    fpi = fopen(fName, "rb");  // open for binary read
    if (fpi == NULL)
    {
        perror("Send: Failed to open input file");
        return 1;
    }

    // Ask link layer to connect to other computer
    if (debug) printf("Send: Connecting using port %s...\n", portName);
    retVal = LL_connect(portName, debug);  // try to connect
    if (retVal < 0)  // problem connecting
    {
        fclose(fpi);     // close input file
        return retVal;  // pass back the problem code
    }

    // Ask link layer for the optimum size of data block
    // Subtract 1 to allow for application layer header byte
    sizeDataBlk = LL_getOptBlockSize(FULL) - 1;
    // Limit to the size of the arrays
    if (sizeDataBlk > MAX_DATA) sizeDataBlk = MAX_DATA;

    // Send a block of data containing the name of the file
    data[0] = (byte_t) FILENAME;  // header byte
    nByte = 0;  // initialise counter
    do  // loop to copy file name into data array
    {
        data[nByte+1] = fName[nByte]; // copy byte from file name
    }
    while (fName[nByte++] != 0);  // including end of string

    // print message about this
    if (debug) printf("\nSend: Sending file name block, %d bytes...\n", nByte+1);
    retVal = LL_send(data, nByte+1, debug);  // send bytes to link layer
    if (retVal < 0)
    {
        printf("Send: Problem sending file name block\n");
        fclose(fpi);  // close the file
        LL_discon(debug);  // disconnect
        return retVal;  // and quit
    }

    // Start sending the contents of the file, one block at a time
    do  // loop block by block
    {
        data[0] = (byte_t) FILEDATA;  // set the header byte
        // read bytes from file, store in array starting after header
        nByte = (int) fread(data+1, 1, sizeDataBlk, fpi);
        if (ferror(fpi))  // check for problem
        {
            perror("Send: Problem reading input file");
            fclose(fpi);   // close input file
            LL_discon(debug);  // disconnect link
            return 3;  // we are giving up on this
        }
        if (debug)
            printf("\nSend: Read %d bytes from file, sending %d bytes...\n",
                   nByte, nByte+1);
        byteCount += nByte;  // add to byte count

        retVal = LL_send(data, nByte+1, debug);  // send bytes to link layer
        // retVal is 0 if succeeded, non-zero if failed
    }
    while ((retVal == 0) && (feof(fpi) == 0));  // until input file ends or error

    if (retVal < 0)   // deal with error
    {
        printf("Send: Problem sending data\n");
        fclose(fpi);  // close the file
        LL_discon(debug);  // disconnect
        return retVal;  // and quit
    }

    // if here, the entire file has been sent
    if (debug) printf("\nSend: End of input file after %ld bytes\n", byteCount);
    fclose(fpi);    // close input file

    // Now send an ending mark
    data[0] = (byte_t) FILEEND;  // header byte (and only byte)
    retVal = LL_send(data, 1, debug);  // send block of one byte
    if (retVal < 0) printf("Send: Problem sending end block\n");
    else if (debug) printf("Send: Sent end block, %d byte\n", retVal);

    // Ask link layer to disconnect
    if (debug) printf("Send: Disconnecting...\n");
    LL_discon(debug);  // ignore return value here...

    return retVal;  // indicate success or failure
}  // end of sendFile


// ============================================================================
/* Function to receive a file, using the link layer protocol.
   It connects to another computer, and waits to receive a block of data.
   The first block should contain the file name, and it opens the output file,
   with a modified file name (to avoid over-writing anything important).
   The following blocks of data received should be data blocks, and are written
   to the file. The final block should be an end marker, then the file is closed
   and the link disconnected.
   If debug is non-zero, it prints progress information,
   if debug is 0, it only prints if there is a problem.
   It returns 0 for success, or a non-zero failure code.  */
int receiveFile(char *portName, int debug)
{
    FILE *fpo;  // file handle for output file
    byte_t data[MAX_DATA+2];  // array of bytes
    int nByte, nWrite;  // number of bytes received or written
    int header = 0;  // header value from received block
    int retVal;  // return code from other functions
    long byteCount = 0; // total number of bytes received

    // Connect to other computer
    if (debug) printf("RX: Connecting using port %s...\n", portName);
    retVal = LL_connect(portName, debug);  // try to connect
    if (retVal < 0)  // problem connecting
    {
        return retVal;  // pass back the problem code
    }
    printf("RX: Connected, waiting to receive...\n");

    // Try to receive one block of data
    nByte = LL_receive(data, MAX_DATA+1, debug);
    // nByte will be number of bytes received, or negative if problem
    if (nByte < 0)  // check for problem
    {
        printf("RX: Problem receiving first data block, code %d\n", nByte);
        if (debug) printf("RX: Disconnecting...\n");
        LL_discon(debug);  // disconnect
        return nByte;   // return problem code
    }
    if (nByte == 0)  // empty data block
    {
        printf("RX: Received empty data block at start\n");
        if (debug) printf("RX: Disconnecting...\n");
        LL_discon(debug);  // disconnect
        return 5;   // return problem code
    }

    // If we get here, we have received a data block
    if (debug) printf("RX: Received first block of %d bytes\n", nByte);

    header = (int) data[0];  // extract the header byte
    if (header != FILENAME)  // wrong type of block
    {
        printf("RX: Unexpected block type: %d\n", header);
        if (debug) printf("RX: Disconnecting...\n");
        LL_discon(debug);  // disconnect
        return 6;   // return problem code
    }

    // If we get here, we have a filename!
    data[0] = 'Z';  // put Z as the first character

    // Open the output file and check for failure
    if (debug) printf("RX: Opening %s for output\n\n", (char*)data);
    fpo = fopen((char*)data, "wb");  // open for binary write
    if (fpo == NULL)
    {
        perror("RX: Problem opening output file");
        LL_discon(debug);  // disconnect
        return 2;
    }

    // Finally, we can start to receive the data
    // Get each block of data and write to file
    do  // loop block by block
    {
        nByte = LL_receive(data, MAX_DATA+1, debug);  // try to receive data block
        // nByte will be number of bytes received, or negative if problem

        // First check nByte, to see what to do...
        if (nByte < 0 ) printf("RX: Problem receiving data, code %d\n",nByte);
        else if (nByte == 0)
        {
            if (debug) printf("RX: Zero bytes received\n");
        }
        else // we got some data!
        {
            // Now check the header byte to see what to do...
            header = (int) data[0];  // extract the header
            if (header == FILEDATA)  // got data block - write data to file
            {
                byteCount += nByte-1;  // add to byte count
                // write bytes to file, starting after header
                nWrite = (int) fwrite(data+1, 1, nByte-1, fpo);
                if (ferror(fpo))  // check for problem
                {
                    perror("RX: Problem writing output file");
                    nByte = -9;  // fake value to end loop
                }
                else if (debug)
                    printf("RX: Wrote %d bytes to file\n\n", nWrite);
            }
            else if (header == FILEEND)  // got end marker
            {
                if (debug)
                    printf("RX: End marker after %ld bytes\n\n", byteCount);
                nByte = -1;  // fake value to end loop
            }
            else
            {
                if (debug)
                printf("RX: Unexpected block type: %d\n\n", header);
            } // end of inner if - checking header

        } // end of outer if - checking nByte
    }
    while (nByte >= 0);  // repeat until problem or end marker

    fclose(fpo);  // close output file
    // Ask link layer to disconnect
    if (debug) printf("RX: Disconnecting...\n");
    LL_discon(debug);  // ignore return value here...

    return (nByte < -1) ? -nByte : 0;  // indicate success or failure
}  // end of receiveFile
