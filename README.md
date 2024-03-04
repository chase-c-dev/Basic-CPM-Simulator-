Project is a bootable shell program that reads files into memory and executes programs.
Worked on it in a group with https://github.com/AmbivalentLeather.
Runs in a terminal through a java simulator.

COMMANDS:

- exec: will continuously print out number in the shell

	Usage - "exec number"
- kill: terminates a process

	Usage - "kill 3" (terminates process 3)

MORE COMMANDS:
	type: Type out the contents of a file Usage - "type filename"
	copy: Copy a file into another file Usage - "copy filename1 filename2"
	create: Create a new file Usage - "create filename"
	dir: List the available files Usage - "dir"
	del: Delete a file Usage - "delete filename"


To verify the code
  
  ./compileOS
  
  java -jar simulator.jar
  
  Run/Test the commands above


And use the disk.img as disk c
In the booted terminal ("C>")

In this project we added the ability for multiple functions to be executing at the same time. 
We added arrays to hold the eight processes and stack pointers
We added the ability to kill/terminate processes
We added scheduling to manage running processes

https://github.com/chase-c-dev/COMP350-ProjectE


