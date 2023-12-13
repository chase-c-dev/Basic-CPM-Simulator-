/* Written by: Nick Young, Chase Simao, starting code from Joe Matta
 * Date: October 2023 */

void printString(char*);
void printChar(char inputChar);
char* readString(char*);
void readSector(char*, int);
void handleInterrupt21(int ax, char* bx, int cx, int dx);
void readFile(char* filename, char* output_buffer, int* sectorsRead);
int directoryLineCompare(char* directory_buffer, int* file_entry, char* string_to_beat);

int executeProgram(char* name);

void writeSector(char*, int);
void deleteFile(char* filename);
void writeFile(char*, char*, int);
void handleTimerInterrupt(int, int);
void killProcess(char*);
void wait(char *pid);
void terminate();
int intPID(char* pid);

#define SECTOR_SIZE 512
#define MAX_SECTORS 26

int processActive[8]; // at each index contains 1 for active or 0 for not active
int processStackPointer[8]; // stores processes stack pointer
int processWaitingOn[8]; // Stores waiting process ids
int currentProcess; // points to the process the table entry is currently executing

int main()
{
    int j = 0;


	makeInterrupt21();

	for (j = 0; j < 8; j++) { // sets all process actives to 0 on all entries and stack pointer values to 0xff00
		processActive[j] = 0;
		processStackPointer[j] = 0xff00;
	}
	currentProcess = -1; // sets current process to -1 because theres no user processes yet

	interrupt(0x21, 4, "shell", 0, 0);
	makeTimerInterrupt(); // call in main before launching the shell
	while(1);
}

void handleInterrupt21(int ax, char* bx, int cx, int dx)
{
	switch(ax)
	{
		case 0: printString(bx);
			break;
		case 1: readString(bx);
			break;
		case 2: readSector(bx, cx);
			break;
		case 3: readFile(bx, cx, dx);
			break;
        	case 4: executeProgram(bx);
            		break;
		case 5: terminate();
			break;
		case 6: writeSector(bx, cx);
			break;
		case 7: deleteFile(bx);
			break;
		case 8: writeFile(bx, cx, dx);
			break;
        	case 9: killProcess(bx);
            		break;
		case 10: wait(bx);
			 break;
		default: printString("Error AX is invalid");
			break;
	}
}

void printString(char* chars)
{	
	int i = 0;
	while (chars[i] != 0x0) {
		char al = chars[i];
		char ah = 0xe;
		int ax = ah * 256 + al;
		interrupt(0x10, ax, 0, 0, 0);
		++i;
	}
}

void printChar(char inputChar)
{
	char al = inputChar;
	char ah = 0xe;
	int ax = ah * 256 + al;
	interrupt(0x10, ax, 0, 0, 0);
}

char* readString(char* inputArray)
{
	char keyboardInput = 0x0;

	int i = 0;
	while (i != 79) { // While the index is below 80
		keyboardInput = (char) interrupt(0x16, 0, 0, 0, 0);

		if (keyboardInput == 0xd)
			// Break out of the loop if the user presses enter
			break;
		if (keyboardInput == 0x8 && i == 0)
			// If the index is 0, don't print backspace
			continue;
		if (keyboardInput == 0x8){
			// Print backspace character, decrement array index
			// We use backspace twice so the character is deleted visually
			char space = ' ';
			interrupt(0x10, 0xe * 256 + keyboardInput, 0, 0, 0);
			interrupt(0x10, 0xe * 256 + space, 0, 0, 0);
			interrupt(0x10, 0xe * 256 + keyboardInput, 0, 0, 0);
			--i;
			continue;
		}
		// If conditions above are met, character prints normally

		inputArray[i] = keyboardInput;
		interrupt(0x10, 0xe * 256 + keyboardInput, 0, 0, 0);
		++i;
	}
	inputArray[i] = 0xd;	// Add return character to character array
	inputArray[i + 1] = 0x0;	// Add null character to character array

	interrupt(0x10, 0xe * 256 + 0xd, 0, 0, 0); // Print return character
	interrupt(0x10, 0xe * 256 + 0xa, 0, 0, 0); // Print line feed character

	return inputArray;
}

void readSector(char* address, int sector)
{
	int AH = 2;	// this number tells BIOS to read a sector as opposed to write
	int AL = 1;	// numbers of sectors to read
	int AX = AH * 256 + AL;

	char* BX = address; // address where the data should be stored to

	int CH = 0;	// track number
	int CL = sector + 1; // relative sector number
	int CX = CH * 256 + CL;

	int DH = 0;	// head number
	int DX = DH * 256 + 0x80;

	interrupt(0x13, AX, BX, CX, DX);  
}

void writeSector(char* address, int sector)
{
	// this number tells BIOS to read a sector as opposed to write (changed to 3 for writeSector)
	int AH = 3;
	int AL = 1;	// numbers of sectors to read
	int AX = AH * 256 + AL;

	char* BX = address; // address where the data should be stored to

	int CH = 0;	// track number
	int CL = sector + 1; // relative sector number
	int CX = CH * 256 + CL;

	int DH = 0;	// head number
	int DX = DH * 256 + 0x80;

	interrupt(0x13, AX, BX, CX, DX);  
}

void readFile(char* filename, char* output_buffer, int* sectorsRead)
{
	char directory_buffer[SECTOR_SIZE];
	int i = 0;

	int file_entry = 0;
	int* pfile_entry;
	pfile_entry = &file_entry;
	*sectorsRead = 0;

	// Reads directory (sector 2) into directory buffer
	readSector(directory_buffer, 2);

	// Checks if filename exists in directory
	if (directoryLineCompare(directory_buffer, pfile_entry, filename)) {
		// Reads the sectors with filename file into given output_buffer
		while(directory_buffer[*pfile_entry + i] != 0) {
			readSector(output_buffer, directory_buffer[*pfile_entry + 6 + i]);
			output_buffer += SECTOR_SIZE;
			++*sectorsRead;
			++i;
		}
       	} else
		*sectorsRead = 0;
}

void writeFile(char* buffer, char* filename, int numberOfSectors)
{
	char dir[SECTOR_SIZE], map[SECTOR_SIZE], genericSectorBuffer[SECTOR_SIZE];
	int i, j;
	int file_entry, directoryColumn, sectorCounter, currentSector, totalSectors;

	// Integer array to store the available sectors
	int freeSectors[MAX_SECTORS + 5]; // + 5 for good measure

	readSector(map, 1); // reads map sector 1 into map buffer
	readSector(dir, 2); //reads directory sector 2 into directory buffer
    
	i = 0;
	// Find a free sector in map
	for (totalSectors = 3; totalSectors < SECTOR_SIZE && sectorCounter < numberOfSectors; totalSectors++) {
		// Set empty sector(s) to 0xFF in map
		if (map[totalSectors] == '\0') {
			map[totalSectors] = 0xFF;
			sectorCounter++;
			freeSectors[i] = totalSectors;
			i++;
		}
	}

	// Find free directory entry, append filename and sector numbers
	for (file_entry = 0; file_entry < SECTOR_SIZE; file_entry += 32) { 
		if (dir[file_entry] == '\0') {
			// Copy filename into free directory entry
			for (i = 0; i < 6; i++) {
				if (filename[i] == '\0')
					dir[file_entry + i] = '\0';
				dir[file_entry + i] = filename[i];
			}

			// Store the location in dir for the sector numbers
			directoryColumn = file_entry + 6;

			// Write sector numbers
			for (i = 0; i < sectorCounter && i < MAX_SECTORS; i++)
				dir[directoryColumn + i] = freeSectors[i];

			break;
		}
	}
	
	// Write the given buffer into necessary sectors
	// If there are more than 26 sectors assigned this whole thing will break
	// so we have added MAX_SECTORS as an upper limit to break the file instead
	for (i = 0; i < sectorCounter && i < MAX_SECTORS; i++) {
		currentSector = freeSectors[i];
		readSector(genericSectorBuffer, currentSector);

		// This loop writes 512 bytes from the buffer into the sector i
		for (j = 0; j < SECTOR_SIZE; j++)
			genericSectorBuffer[j] = buffer[j + (i * SECTOR_SIZE)];

		writeSector(genericSectorBuffer, currentSector);
	}

	writeSector(map, 1); // writes map sector back to disk
	writeSector(dir, 2); // writes directory sector back to disk
}

void deleteFile(char* filename)
{
	char dir[SECTOR_SIZE], map[SECTOR_SIZE];
	int i = 0;
	int mapIndex = 0;

	int file_entry = 0;
	int* pfile_entry = &file_entry;

	// Reads map (sector 1) into map buffer
	readSector(map, 1);
	// Reads directory (sector 2) into directory buffer
	readSector(dir, 2);

	// Checks if filename exists in directory
	if (directoryLineCompare(dir, pfile_entry, filename)) {
		dir[*pfile_entry] = '\0';
		while(dir[*pfile_entry + 6 + i] != '\0') {
			mapIndex = dir[*pfile_entry + 6 + i];
			map[mapIndex] = 0;
			i++;
		}
       	}

	writeSector(map, 1);
	writeSector(dir, 2);

}

int directoryLineCompare(char* directory_buffer, int* file_entry, char* filename_to_beat)
{
	// N: I still don't like the way this function is written because of the dependance
	// on six characters. Yes I wrote it. Yes it works. No I don't like it.
	int correctIndex = 0;
	int i = 0;
	int DIR_LINE_LENGTH = 32;

	// Check every line in directory_buffer, incrementing 32 to move to the next line
	for (*file_entry = 0; *file_entry < SECTOR_SIZE; *file_entry += DIR_LINE_LENGTH){
		// Compare the first 6 characters to the given filename_to_beat
		while (i < 6) {
			if(directory_buffer[*file_entry + i] != filename_to_beat[i])
				break;
			else 
				correctIndex++;
			++i;
		}
		// Return true if all characters match, this works regardless of string length
		if(correctIndex == 6)
			return 1;
	}
	// Base case, if the loop above finds nothing, return false
	return 0;
}

int executeProgram(char* program_name)
{
    	char buffer[SECTOR_SIZE * MAX_SECTORS];
    	int sectorsRead;
	int offset = 0;
	int dataseg;
	int bufferSegment;

	int processIterator = 0;

	// Read program_name into buffer
    	readFile(program_name, buffer, &sectorsRead);

	// Step through the process active array looking for a free entry
	dataseg = setKernelDataSegment();
	while (processIterator < 8) {
		if (processActive[processIterator] == 0) {
			break;
		}
		processIterator += 1;
	}
	restoreDataSegment(dataseg);
	
	// Determine the segment (entry num + 2 * 0x1000)
	bufferSegment = (processIterator + 2) * 0x1000;

	// Copy the buffer into the segment with putInMemory
    	for (offset = 0; offset < sectorsRead * SECTOR_SIZE; offset++) { 
        	// putInMemory(int segment, int address, char character)
         	putInMemory(bufferSegment, offset, buffer[offset]); 
    	}

	// Call initilize program
	initializeProgram(bufferSegment);

	// Set the processActive for that entry to 1, set the entry's processStack pointer to 0xff00
	dataseg = setKernelDataSegment();
	processActive[processIterator] = 1;
	processStackPointer[processIterator] = 0xff00;
	restoreDataSegment(dataseg);
	
	return processIterator;
}

void handleTimerInterrupt(int segment, int sp)
{
	int dataseg, processIterator, i;

	dataseg = setKernelDataSegment();

	for (i = 0; i < 8; i++) {
		putInMemory(0xb800, 60 * 2 + i * 4 + 1, 0x30);
		if (processActive[i] == 1)
			putInMemory(0xb800, 60 * 2 + i * 4 + 1, 0x20);
		else
			putInMemory(0xb800, 60 * 2 + i * 4 + 1, 0);
	}

	if (currentProcess != -1) {
		processStackPointer[currentProcess] = sp;
	}

	processIterator = currentProcess + 1;

	while (processIterator < 8) {
		if (processActive[processIterator] == 1) {
			break;
		}

		if (processIterator == 7) {
			processIterator = -1;
		}

		processIterator++;
	}


	currentProcess = processIterator;

	segment = (currentProcess + 2) * 0x1000;
	sp = processStackPointer[currentProcess];

	restoreDataSegment(dataseg);

    	returnFromTimer(segment, sp);
}

void killProcess(char* BX){ //BX is the process number
    	int dataseg, j = 0, tempBX;
    	char holder[8];

    	holder[0] = '0';
    	holder[1] = '1';
    	holder[2] = '2';
    	holder[3] = '3';
    	holder[4] = '4';
    	holder[5] = '5';
    	holder[6] = '6';
    	holder[7] = '7';
  
    	for (j = 0; j < 8; j++){
       		if (holder[j] == BX[0]) {
			tempBX = j;
			break;
      		}
    	}   
        dataseg = setKernelDataSegment();
        processActive[tempBX] = 0;
        restoreDataSegment(dataseg);
 
}

/*
void nothing():
	// Convert char* to int, only works from 0-7 with this
	intBX = intPID(BX);

        dataseg = setKernelDataSegment();
	processActive[intBX] = 0;
	for (i = 0; i < 8; i++) {
		if (processWaitingOn[i] == currentProcess){
			processWaitingOn[i] = '\0';
			processActive[i] = 1;
		}
	}
        restoreDataSegment(dataseg);
 
}
*/

void wait(char* pid)
{
	int dataseg;

        dataseg = setKernelDataSegment();
	processActive[currentProcess] = 2;
	processWaitingOn[currentProcess] = intPID(pid);
        restoreDataSegment(dataseg);
}

void terminate()
{
	int dataseg, i;

	dataseg = setKernelDataSegment();
	processActive[currentProcess] = 0;
	
	// Every time terminate is called, set the process waiting on currentProcess to no longer wait
	for (i = 0; i < 8; i++) {
		if (processWaitingOn[i] == currentProcess){
			processWaitingOn[i] = '\0';
			processActive[i] = 1;
		}
	}
	restoreDataSegment(dataseg);
	while(1);
}

int intPID(char* pid)
{
	int intPID;
	intPID = pid[3] - '0';
	return intPID;
	/*
	 * Yes, I know I can do 
	 * pid[0] - '0'
	 * This is the first way we did it and I want that represented in the code.
	 * Unless of course this breaks it...
	switch(pid[3])
	{
		case '0': intPID = 0;
			  break;
		case '1': intPID = 1;
			  break;
		case '2': intPID = 2;
			  break;
		case '3': intPID = 3;
			  break;
		case '4': intPID = 4;
			  break;
		case '5': intPID = 5;
			  break;
		case '6': intPID = 6;
			  break;
		case '7': intPID = 7;
			  break;
	}
	*/
}
