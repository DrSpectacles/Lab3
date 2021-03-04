/* EEEN20060 Communication Systems, file comparison program.
   This program opens two files and reads blocks of bytes from each.
   It compares the corresponding blocks, and prints
   details of any blocks that differ.  */

#include <stdio.h>  // standard input-output library
#include <string.h>     // needed for string manipulation

#define MAX_BLOCK 200    // maximum block size
#define MAX_IN 80  // maximum input string length

typedef unsigned char byte;

int main()
{
    char inString[MAX_IN];   // string to hold  filename
    FILE *fp1, *fp2;  // file handles
    byte data1[MAX_BLOCK+2];  // arrays of bytes from files
    byte data2[MAX_BLOCK+2];
    int nInput;         // length of input string
    int nByte1, nByte2;    // number of bytes read from files
    int nBlock=0, nBadBlock=0, nBadByte=0;  // counters
    int chk1, chk2; // check values
    int i, j;       // for use in loops
    int blockSize;  // size of block to read and compare
    int checkMod;   // checksum modulo value

    printf("File Comparison Program\n");  // welcome message

    // Ask user for name of first file
    printf("\nEnter name of good file (name.ext): ");
    fgets(inString, MAX_IN, stdin);  // get filename
    nInput = strlen(inString);
    inString[nInput-1] = '\0';   // remove the newline at the end

    // Open the first file and check for failure
    printf("\nOpening %s for input\n", inString);
    fp1 = fopen(inString, "rb");  // open for binary read
    if (fp1 == NULL)
    {
        perror("Failed to open input file");
        printf("Press return to exit\n");
        fgets(inString, MAX_IN, stdin);  // get some input
        return 1;
    }

    // Ask user for name of second file
    printf("\nEnter name of suspect file (name.ext): ");
    fgets(inString, MAX_IN, stdin);  // get filename
    nInput = strlen(inString);
    inString[nInput-1] = '\0';   // remove the newline at the end

    // Open the second file and check for failure
    printf("\nOpening %s for comparison\n", inString);
    fp2 = fopen(inString, "rb");  // open for binary write
    if (fp2 == NULL)
    {
        fclose(fp1);  // close the first file
        perror("Failed to open input file");
        printf("Press return to exit\n");
        fgets(inString, MAX_IN, stdin);  // get some input
        return 2;
    }

    // Ask user for block size to read
    printf("\nEnter size of block to compare (max %d): ", MAX_BLOCK);
    scanf("%3d", &blockSize);  // get block size
    fgets(inString, MAX_IN, stdin);  // remove newline from input stream

    if ((blockSize < 10) || (blockSize > MAX_BLOCK))
    {
        fclose(fp1);  // close the files
        fclose(fp2);
        printf("Invalid block size - press return to exit\n");
        fgets(inString, MAX_IN, stdin);  // get some input
        return 3;  // and quit
    }

    // Ask user for checksum modulo
    printf("\nEnter checksum modulo value, or 0 for parity check: ");
    scanf("%3d", &checkMod);  // get checksum modulo value
    fgets(inString, MAX_IN, stdin);  // remove newline from input stream

    printf("\n");  // blank line

    // Read the contents of the files, one block at a time
    do
    {
        // read bytes from files, store in arrays
        nByte1 = (int) fread(data1, 1, blockSize, fp1);
        if (ferror(fp1))  // check for error
        {
            perror("Main: Problem reading good file");
            break;
        }

        nByte2 = (int) fread(data2, 1, blockSize, fp2);
        if (ferror(fp2))  // check for error
        {
            perror("Main: Problem reading suspect file");
            break;
        }

        nBlock++;  // advance the block counter
        printf(".");  // progress marker
        if (nBlock % 50 == 0) printf("\n"); // new line

        // check block size
        if (nByte1 != nByte2)
        {
            printf("\nBlock %d, sizes differ: good %d, bad %d\n",
                   nBlock, nByte1, nByte2);
        }

        // compare the blocks
        j = 0;  // start each block with error count at 0
        chk1 = chk2 = 0;    // initialise check values
        for (i=0; (i<nByte1)&&(i<nByte2); i++)
        {
            if (data1[i] != data2[i])
            {
                printf("\nError in block %d, byte %3d: %2X -> %2X\n",
                       nBlock, i, (int) data1[i], (int) data2[i]);
                j++;
            }
            if (checkMod == 0)
            {
                chk1 ^= data1[i];   // update parity checks
                chk2 ^= data2[i];
            }
            else
            {
                chk1 += (int) data1[i];  // update checksums
                chk2 += (int) data2[i];
            }
        }

        // update main counters
        if (j > 0)
        {
            nBadBlock++;
            nBadByte += j;
            if (checkMod == 0)
            {
                printf("\nBlock %d checkbits: good 0x%2X, bad 0x%2X\n",
                       nBlock, chk1, chk2 );
            }
            else
            {
                printf("\nBlock %d checksums: good %d, bad %d\n",
                        nBlock, chk1 % checkMod, chk2 % checkMod);
            }
        }

    }
    while ((feof(fp1) == 0) && (feof(fp2)==0));  // until end of file

    // Check why the loop ended, and print message
    if (feof(fp1) != 0) printf("\nEnd of good file");
    if (feof(fp2) != 0) printf("\nEnd of suspect file");

    // Print statistics
    printf("\nRead %d blocks, %d bad bytes in %d bad blocks\n",
           nBlock, nBadByte, nBadBlock);

    fclose(fp1);    // close input file
    fclose(fp2);    // close output file
    printf("\nFiles closed - press return to exit\n");
    fgets(inString, MAX_IN, stdin);  // get filename
    return 0;

}
