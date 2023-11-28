/* Written by: Nick Young, Chase Simao, starting code from Joe Matta
 * Date: October 2023 */

void printString(char*);
void printChar(char*);
char* readString(char*);
void readSector(char*, int);
void handleInterrupt21(int ax, char* bx, int cx, int dx);
void readFile(char* filename, char* output_buffer, int* sectorsRead);
int directoryLineCompare(char* directory_buffer, int* file_entry, char* string_to_beat);
void executeProgram(char* name);
void writeSector(char*, int);
void deleteFile(char* filename);
void writeFile(char*, char*, int);
void terminate();

#define SECTOR_SIZE 512
#define MAX_SECTORS 26

int main()
{
	makeInterrupt21();
	interrupt(0x21, 4, "shell", 0, 0);
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

void printChar(char* inputChar)
{
	char al = inputChar[0];
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
	// THIS DOES NOT WORK PROPERLY IF YOU DELETE A FILE
	// IF YOU CLEAR A SECTOR AND THEN THERE ARE UNFREE SECTORS AFTER THAT, IT ASSUMES ALL SECTORS AFTER THE FIRST FREE SECTOR IS FREE
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
	for (i = 0; i < sectorCounter && sectorCounter < MAX_SECTORS; i++) {
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

void executeProgram(char* program_name)
{
    	char buffer[SECTOR_SIZE * MAX_SECTORS];
    	int sectorsRead;
	int offset = 0;

	// Read program_name into buffer
    	readFile(program_name, buffer, &sectorsRead);

	// Put the buffer into memory
    	for (offset = 0; offset < sectorsRead * SECTOR_SIZE; offset++) { 
        	// putInMemory(int segment, int address, char character)
         	putInMemory(0x2000, offset, buffer[offset]); 
    	}

	launchProgram(0x2000); // will not return, sets of registers and jumps to the program located at 0x2000
}

void terminate()
{
	char shellname[6];
	shellname[0] = 's';
	shellname[1] = 'h';
	shellname[2] = 'e';
	shellname[3] = 'l';
	shellname[4] = 'l';
	shellname[5] = '\0';

	executeProgram(shellname);
}

