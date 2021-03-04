/*  Physical Layer functions using serial port.
       PHY_open    opens and configures the port
       PHY_close   closes the port
       PHY_send    sends bytes
       PHY_get     gets received bytes
    All functions print explanatory messages if there is
    a problem, and return values to indicate failure.
    This version uses standard C functions and some functions specific
	to Microsoft Windows.  It will NOT work on other operating systems.  */

#include <stdio.h>   // needed for printf
//#include <windows.h>  // needed for port functions
#include <string.h>
#include <stdlib.h>  // for random number functions
#include <time.h>    // for time function, used to seed rand
#include "physical.h"  // header file for functions in this file

// Linux specific
#include <fcntl.h> // Contains file controls like O_RDWR
#include <errno.h> // Error integer and strerror() function
#include <termios.h> // Contains POSIX terminal control definitions
#include <unistd.h> // write(), read(), close()

/* Creating a variable this way allows it to be shared
   by the functions in this file only.  */
// static HANDLE serial = INVALID_HANDLE_VALUE;  // handle for serial port
static double rxProbErr = 0.0; // probability of error, used in PHY_get()

static int serial_port = -1;

/* PHY_open function - to open and configure the serial port.
   Arguments are port number, bit rate, number of data bits, parity,
   receive timeout constant, rx timeout interval, rx probability of error.
   See comments below for more detail on timeouts.
   Returns zero if it succeeds - anything non-zero is a problem.*/
int PHY_open(const char *portName,       // port name: e.g. "ttyS10"
             int bitRate,       // bit rate: e.g. 1200, 4800, etc.
             int nDataBits,     // number of data bits: 7 or 8
             int parity,        // parity: 0 = none, 1 = odd, 2 = even
             int rxTimeConst,   // rx timeout constant in ms: 0 waits forever
             int rxTimeIntv,    // rx timeout interval in ms: 0 waits forever
             double probErr)    // rx probability of error: 0.0 for none
{
    // Define variables
    int bitRatio, bitRatioValid, i;  // for bit rate checking
    //DCB serialParams = {0};  // Device Control Block (DCB) for serial port

    struct termios tty;

    
    //COMMTIMEOUTS serialTimeLimits = {0};  // COMMTIMEOUTS structure for port
    int timeMult;   // for calculating time limits
    char Full_portName[50];  // string to hold port name


    speed_t baudRate;

    switch (bitRate) {
    case 1200:
      baudRate = B1200;
      break;
    case 2400:	
      baudRate = B1200;
      break;
    case 4800:
      baudRate = B4800;
      break;
    case 9600:
      baudRate = B9600;
      break;
    case 19200:
      baudRate = B19200;
      break;
    case 38400:
      baudRate = B38400;
      break;
    default:
      printf("PHY: Invalid bit rate requested: %d\n", bitRate);
      return 3;
    }
    // First check that parameters given are valid - first bit rate
    // This code only allows 1200, 2400, 4800, 9600, 19200, 38400 bit/s

    // Now check the number of data bits requested
    // Only 7 or 8 data bits allowed
    if ((nDataBits!=7) && (nDataBits!= 8))
    {
        printf("PHY: Invalid number of data bits: %d\n", nDataBits);
        return 3;
    }

    // Now check parity - only 0, 1, 2 allowed
    if ((parity<0) || (parity>2))
    {
        printf("PHY: Invalid parity requested: %d\n", parity);
        return 3;
    }

    // Make port name string, by adding port name to /dev/
    sprintf(Full_portName, "/dev/%s", portName);  // print to string
    printf("Full port name is %s\n", Full_portName);
    
    /* Windows Version    
    // Try to open the port
    serial = CreateFile(portName, GENERIC_READ | GENERIC_WRITE,
                                0, 0, OPEN_EXISTING, 0, 0);
    // Check for failure
    if (serial == INVALID_HANDLE_VALUE)
    {
        printf("PHY: Failed to open port |%s|\n",portName);
        printProblem();  // give details of the problem
        return 1;  // non-zero return value indicates failure
    }
    */

    serial_port = open(Full_portName, O_RDWR); 

    if (serial_port < 0) {
      printf("Error number %i from open(): %s\n", errno, strerror(errno));
    }
    
    if(tcgetattr(serial_port, &tty) != 0) {
      printf("Error %i from tcgetattr: %s\n", errno, strerror(errno));
    }

    cfmakeraw(&tty);   
    
    //Set up parameters using termios flags
    // tty.c_cflag |= PARENB; //enable parity bit

    tty.c_cflag &= ~CSTOPB; //use only 1 stop bit

    if (nDataBits == 7) //set number of data bits to be 7 or 8, 8 default (most common)
      tty.c_cflag |= CS7; 
    else
      tty.c_cflag |= CS8;

    tty.c_cflag &= ~CRTSCTS; //disable CTS signal

    tty.c_cflag |= CREAD | CLOCAL;// enable read,

    //these disable special responses for certain bits
    tty.c_lflag &= ~ECHOE;

    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // disables software flow control


    tty.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    
    cfsetispeed(&tty, baudRate); //input BaudRate
    cfsetospeed(&tty, baudRate); //output BaudRate

        

    /* 
    // Set length of device control block before use
    serialParams.DCBlength = sizeof(serialParams);

    // Fill the DCB with the parameters of the port, and check for failure
    if (!GetCommState(serial, &serialParams))
    {
        printf("PHY: Problem getting port parameters\n");
        printProblem();  // give details of the problem
        CloseHandle(serial);
        return 2;
    }
    */

    /* Change the parameters to configure the port as required,
       without interpreting or substituting any characters,
       and with no added flow control.  */
    //    serialParams.BaudRate = bitRate;    // bit rate
    //  serialParams.ByteSize = nDataBits;  // number of data bits in group
    //serialParams.Parity = parity;      // parity mode
    // serialParams.StopBits = ONESTOPBIT;  // just one stop bit
    // serialParams.fOutxCtsFlow = FALSE;   // ignore CTS signal
    // serialParams.fOutxDsrFlow = FALSE;   // ignore DSR signal on transmit
    // serialParams.fDsrSensitivity = FALSE; // ignore DSR signal on receive
    // serialParams.fOutX = FALSE;           // ignore XON/XOFF on transmit
    // serialParams.fInX = FALSE;           // do not send XON/XOFF on receive
    // serialParams.fParity = FALSE;        // ignore parity on receive
    // serialParams.fNull = FALSE;   // do not discard null bytes on receive

    // Apply the new parameters to the port
    

/*  Set timeout values for write and read functions.  If the requested
    number of bytes  have not been sent or receiveed in the time allowed,
    the function returns anyway.  The number of bytes actually written or
    read will be returned, so the problem can be detected.
    Time allowed for write operation, in ms, is given by
    WriteTotalTimeoutConstant + WriteTotalTimeoutMultipier * no. bytes requested.
    If both values are zero, timeout is not used - waits forever.
    Time allowed for read operation is similar.  The read function can also
    return if the time interval between bytes, in ms, exceeds ReadIntervalTimeout.
    This can be useful when waiting for a large block of data, and only a small
    block of data arrives - the read function can return promptly with the data.

    In this function, time multipliers are derived from the bit rate, allowing
    11 transmitted bits per data byte (the maximum possible value).
    Transmit constant is 100 ms. Receive constant is set by user - a value of 0
    forces the receive multiplier to 0 also, to disable receive time limits.*/

    // Calculate multiplier, based on bit rate, minimum 1 ms
    //timeMult = 1 + 11000/bitRate;  // 10 ms at 1200, 1 ms above 9600 bit/s

    // Set the transmit timeout values.
    //serialTimeLimits.WriteTotalTimeoutConstant = 100;
    // serialTimeLimits.WriteTotalTimeoutMultiplier = (DWORD)timeMult;

    // Modify multiplier for receive if necessary
    //if (rxTimeConst==0) timeMult = 0;

    // Set receive timeout values
    //serialTimeLimits.ReadTotalTimeoutConstant = (DWORD)rxTimeConst;
    //serialTimeLimits.ReadTotalTimeoutMultiplier = timeMult;
    //serialTimeLimits.ReadIntervalTimeout = (DWORD)rxTimeIntv;

    // Apply the time limits to the port
    //if (!SetCommTimeouts(serial, &serialTimeLimits))
    //{
    //    printf("PHY: Problem setting timeouts\n");
    //    printProblem();  // give details of the problem
    //   CloseHandle(serial);
    //     return 5;
	// }

    

    rxTimeConst /= 100; //convert to deciseconds for VMIN & VTIME
    printf("setting Vtime to %d \n", rxTimeConst);
    tty.c_cc[VTIME] = rxTimeConst;    // Wait for up to 1s (10 deciseconds), returning as soon as any data is received.
    tty.c_cc[VMIN] = 0;



    // Save tty settings, also checking for error
    if (tcsetattr(serial_port, TCSANOW, &tty) != 0) {   //this fuction sets and saves attributes described above
      printf("Error %i from tcsetattr: %s\n", errno, strerror(errno));
      return 1;
    }

    // Clear the receive buffer, in case there is rubbish waiting
    //if (!PurgeComm(serial, PURGE_RXCLEAR))
    //{
    //    printf("PHY: Problem purging receive buffer\n");
    //   printProblem();  // give details of the problem
    //    CloseHandle(serial);
    //    return 6;
    // }

    

    /* Set up simulated errors on the receive path:
       Set the seed for the random number generator,
       and check the probability of error value. */
    srand(time(NULL));  // get time and use as seed
    if ((probErr>=0.0) && (probErr<=1.0))  // check valid
        rxProbErr = probErr; // pass value to shared variable

    // If we get this far, the port is open and configured
    sleep(2); //required to make flush work, for some reason
    tcflush(serial_port, TCIOFLUSH);

    return 0;
}

//===================================================================
/* PHY_close function, to close the serial port.
   Takes no arguments, returns 0 always.  */
int PHY_close()
{
    close(serial_port);
    return 0;
}

//===================================================================
/* PHY_send function, to send bytes.
   Arguments: pointer to array holding bytes to be sent;
              number of bytes to send.
   Returns number of bytesize sent, or negative value on failure.  */
int PHY_send(byte_t *dataTx, int nBytesToSend)
{
  //DWORD nBytesTx;  // double-word - number of bytes actually sent
     int nBytesSent;    // integer version of the same

     //NB: Port check
     
    // First check if the port is open
     // if (serial == INVALID_HANDLE_VALUE)
     // {
     //   printf("PHY: Port not valid\n");
     //   return -9;  // negative return value indicates failure
     //}

    // Try to send the bytes as requested

     printf("sending size=%d\n", nBytesToSend);
     nBytesSent = write(serial_port, dataTx, nBytesToSend);
     printf("bytes sent=%d\n", nBytesSent);
     
     if(( nBytesSent ) == -1) {
       printf("PHY: Problem sending data\n");
       printf("Error %i from function: %s\n", errno, strerror(errno));
       close(serial_port);
       return -5;
    }
       

     /*     if (!WriteFile(serial, dataTx, nBytesToSend, &nBytesTx, NULL ))
    {
        printf("PHY: Problem sending data\n");
        printProblem();  // give details of the problem
        CloseHandle(serial_port);
        return -5;
    }
    else if ((nBytesSent = (int)nBytesTx) != nBytesToSend)  // check for timeout
    {
        printf("PHY: Timeout in transmission, sent %d of %d bytes\n",
               nBytesSent, nBytesToSend);
    }
     */
    return nBytesSent; // if succeeded, return the number of bytes sent
    // note that timeout is not regarded as a failure here
}

//===================================================================
/* PHY_get function, to get received bytes.
   Arguments: pointer to array to hold received bytes;
              maximum number of bytes to receive.
   Returns number of bytes actually received, or negative value on failure.  */
int PHY_get(byte_t *dataRx, int nBytesToGet)
{
     int nBytesGot;      // integer version of above
     int threshold = 0;  // threshold for error simulation
     int i;             // for use in loop
     int flip;          // bit to change in simulating error
     byte_t pattern;    // bit pattern to cause error

     //NB: FIX CHECK

     // First check if the port is open
     /*    if (serial == INVALID_HANDLE_VALUE)
    {
        printf("PHY: Port not valid\n");
        return -9;  // negative return value indicates failure
    }

     */

     nBytesGot = read(serial_port, dataRx, nBytesToGet);
     //LEGACY: !ReadFile(serial, dataRx, nBytesToGet, &nBytesRx, NULL )
     
    // Try to get bytes as requested
    if (nBytesGot == -1)
    {
        printf("PHY: Problem receiving data\n");
        printf("Error %i from function: %s\n", errno, strerror(errno));
        close(serial_port);
        return -4;
    }

    // Add a bit error, with the probability specified
    if (rxProbErr != 0.0)
    {
        // set threshold as fraction of max, scaling for 8 bit bytes
        threshold = 1 + (int)(8.0 * (double)RAND_MAX * rxProbErr);
        for (i = 0; i < nBytesGot; i++)
        {
            if (rand() < threshold)  // we want to cause an error
            {
                flip = rand() % 8;  // random integer 0 to 7
                pattern = (byte_t) (1 << flip); // bit pattern: single 1 in random place
                dataRx[i] ^= pattern;  // invert one bit
                printf("PHY_get:  ####  Simulated bit error...  ####\n");
            }
        }
    }

    return nBytesGot; // if no problem, return the number of bytes received
}

// Function to print informative messages when something goes wrong...
void printProblem(void)
{
  //char lastProblem[1024];
  //	int problemCode;
  //	problemCode = GetLastError();  // get the code for the last problem
  //	FormatMessage(
		      //		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		//		NULL, problemCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		//	lastProblem, 1024, NULL);  // convert code to message
	//	printf("PHY: Code %d = %s\n", problemCode, lastProblem);

}

/* Function to delay for a specified number of ms. */
void waitms(int delay_ms)
{
  //    clock_t endTick, delayTick;   // variables to hold clock values
  // delayTick = ((clock_t) delay_ms * CLOCKS_PER_SEC) / 1000;  // delay in ticks
  // endTick = clock() + delayTick; // end time in ticks

  //while (clock() < endTick);  // wait until time is reached

  usleep(delay_ms*1000);

  printf("waitms slept for %d ms \n", delay_ms);
  
  return; // then return
}

