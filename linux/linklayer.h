/* Define a type called byte_t, if not already defined.
   This is an 8-bit variable, able to hold integers from 0 to 255.
   It could be named "byte", but this conflicts with a definition in
   windows.h, which is needed for the real physical layer functions. */
#ifndef BYTE_T_DEFINED
#define BYTE_T_DEFINED
typedef unsigned char byte_t;  // define type "byte_t" for simplicity
#endif


#ifndef LINKLAYER_H_INCLUDED
#define LINKLAYER_H_INCLUDED

// Link Layer Protocol definitions - adjust all these to match your design
#define MAX_BLK 200   // largest number of data bytes allowed in one frame
#define OPT_BLK 70    // optimum number of data bytes in a frame
#define MOD_SEQNUM 16 // modulo for sequence numbers

// Frame marker byte values
#define STARTBYTE 212   // start of frame marker
#define ENDBYTE 204     // end of frame marker


// Frame header byte positions
#define FRSPOS 1        // position of frame sdize
#define SEQNUMPOS 2     // position of sequence number

// Header and trailer size
#define HEADERSIZE 3	// number of bytes in frame header
#define TRAILERSIZE 2	// number of bytes in frame trailer

// Frame error check results
#define FRAMEGOOD 1     // the frame has passed the tests
#define FRAMEBAD 0      // the frame is damaged

// Acknowledgement values
#define POSACK 1        // positive acknowledgement
#define NEGACK 26       // negative acknowledgement
#define ACK_SIZE 5      // number of bytes in ack frame

// Time limits
#define TX_WAIT 4.0   // sender waiting time in seconds
#define RX_WAIT 6.0   // receiver waiting time in seconds
#define MAX_TRIES 5   // number of times to re-try (either end)

// Physical Layer settings to be used
#define PORTNUM 1        // default port number: COM1
#define BIT_RATE 4800    // use a low speed for initial tests
#define PROB_ERR 3.0E-4  // probability of simulated error on receive

// Logical values
#define TRUE 1
#define FALSE 0

// Return codes - indicate success or what has gone wrong
#define SUCCESS 0       // good result, job done
#define BADUSE -9       // function cannot be used in this way
#define FAILURE -12     // function has failed for some reason
#define GIVEUP -15      // function has failed MAX_TRIES times

// Definition for debug mode - control link layer functions
#define SIMPLE 1        // simple mode for initial testing
#define FULL 2          // full mode for later testing

// Definition of modulo to be used in the checksum
#define MODULO 251      // will be used to compose checksum

/* Functions to implement link layer protocol.
   All functions take a debug argument - if non-zero, they print
   messages explaining what is happening.  Regardless of debug,
   functions print messages when things go wrong.
   LL_send and LL_receive behave in a simpler way if debug is 1,
   to facilitate testing of some parts of the protocol.
   Functions return negative values on failure.  */

// Function to connect to another computer.
int LL_connect(char *portName, int debug);

// Function to disconnect from other computer.
int LL_discon(int debug);

// Function to send a block of data in a frame.
int LL_send(byte_t *dataTx, int nTXdata, int debug);

// Function to receive a frame and return a block of data.
int LL_receive(byte_t *dataRx, int maxData, int debug);

// Function to return the optimum size of a data block.
int LL_getOptBlockSize(int debug);


// ==========================================================
// Functions called by the main link layer functions above

// Function to build a frame around a block of data.
int buildDataFrame(byte_t *frameTx, byte_t *dataTx, int nData, int seq);

// Function to get a frame from bytes received by the physical layer.
int getFrame(byte_t *frameRx, int maxSize, float timeLimit);

// Function to check a frame for errors.
int checkFrame(byte_t *frameRx, int sizeFrame);

// Function to process a received frame.
int processFrame(byte_t *frameRx, int sizeFrame,
                 byte_t *dataRx, int maxData, int *seqNum);

// Function to send an acknowledgement - positive or negative.
int sendAck(int type, int seq, int debug);

// ==========================================================
// Helper functions used by various other functions

// Function to advance the sequence number
int next(int seq);

// Function to set time limit at a point in the future.
long timeSet(float limit);

// Function to check if time limit has elapsed.
int timeUp(long timeLimit);

// Function to check if a byte is a protocol byte.
int special(byte_t b);

// Function to print bytes of a frame, in groups of 10.
void printFrame(byte_t *frame, int sizeFrame);

#endif // LINKLAYER_H_INCLUDED
