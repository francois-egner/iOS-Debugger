#include "debugger_mach.h"

int main(int ac, char **av){													
																					
	pid_t pid = atoi(av[1]); //Convert given PID-String into integer
	globalTask = getTaskFromPID(pid); //Determine task of given PID													

	mach_port_t* port = createExceptionPort(globalTask); //Create exception port for debugee
	createExceptionHandler(*port); //Register exception handler for exception port

    return 0;
}


//Exception handling (breakpoints only) aka. debug loop
extern kern_return_t catch_exception_raise(
	mach_port_t                          exception_port,
	mach_port_t                                  thread,
	mach_port_t                                    task,
	exception_type_t                          exception,
	exception_data_t                               code,
	mach_msg_type_number_t                   code_count
) {	
	
	if (exception == EXC_BREAKPOINT) { //Is exception caused by a breakpoint hit?
		uint64_t pc = getRegister(task, 32); //Read program counter
		unsigned char* instruction = readMemory(task, pc, 4); //Read instruction where PC is pointing to
		fprintf(stderr, "Address: %llx Instruction: 0x%.2hhx%.2hhx%.2hhx%.2hhx\n", pc, instruction[0], instruction[1], instruction[2], instruction[3]); //Print PC and associated instruction

		
		if (BreakpointExists(pc)) { //Was the breakpoint set by the debugger?
			deleteBreakpoint(task, pc); //Delete the breakpoint
		}
		
		//Some ugly input parsing...
		while (true) {
			printf(">");
			int groesse = 0;
			char** input = getInput(&groesse);
			
			//Breakpoint commands
			if (!strcmp(input[0], "breakpoint")) {
				if (groesse == 1) {
					printf("Missing argument!\n");
				}else if (!strcmp(input[1], "set")) {
					if (groesse == 3) {		
						vm_address_t addr = (vm_address_t)strtoull(input[2], NULL, 16);
						addBreakpoint(task, addr, false);
					}
				}else if (!strcmp(input[1], "showAll")) {
					printBreakpoints();
				}else if (!strcmp(input[1], "delete")) {
					if (groesse == 3) {
						vm_address_t addr = strtoull(input[2], NULL, 16);
						if (BreakpointExists(addr)) {
							deleteBreakpoint(task, addr);
						}else {
							printf("Unable to delete breakpoint because it doesnt exist in breakpoint list!\n");
						}
					}else { printf("Nicht genug Parameter für breakpoint delete-Befehl!\n"); }
				}else {
					printf("Invalid breakpoint command!\n");
				}
			//Register commands
			}else if (!strcmp(input[0], "register")) {
				if (groesse ==	1) {
					printf("Fehlende Parameter!");
				}else if (!strcmp(input[1],"showAll")) {
					showRegistersFromTask(task);	
				}else if (!strcmp(input[1], "set")) {
					if (groesse == 4) {
						int index = atoi(input[2]);
						unsigned long long int data = strtoull(input[3], NULL, 16);
						setRegister(task, index, data);
						printf("Register %d auf 0x%llx\n", index, data);
					}else { printf("Ungueltige Parameteranzahl für register set-Befehl!"); }
				}else if (!strcmp(input[1], "read")) {
					if (groesse == 3) {
						int index = atoi(input[2]);
						uint64_t data = getRegister(task, index);
						printf("Register %d: 0x%llx\n", index, data);
					}
					else { printf("Invalid count of register set command arguments"); }
				}else {
					printf("Invalid register command!\n");
				}
			//Memory commands
			}else if (!strcmp(input[0], "memory")) {
				if (groesse == 1) {
					printf("Fehlende Parameter!\n");
				}else if (!strcmp(input[1], "write")) {
					if (groesse == 4) {
						mach_vm_address_t addr = strtoull(input[2], NULL, 16);
						
						const char* pos = input[3];
						int length = strlen(input[3])/2;
						
						if (input[3][0] == '0' && input[3][1] == 'x'){
							length -= 1;
							pos = pos + 2;
						}
						
						unsigned char buffer[length];

						
						for (int count = 0; count < length; count++) {
							sscanf(pos, "%2hhx", &buffer[count]);
							pos += 2 * sizeof(char);
						}

						writeMemory(task, addr, (void*)&buffer, length);
					}else { printf("Invalid count of write command arguments!"); }
				}else if (!strcmp(input[1], "read")) {		//Lese Speicher
					if (groesse == 4) {
						mach_vm_address_t addr = strtoull(input[2], NULL, 16);
						unsigned long long int size = strtoull(input[3], NULL, 10);
						unsigned char* data = readMemory(task, addr, size);
						
						for (int row = 0; row <= size / 8; row++) {
							printf("0x%llx	", addr+row*8);
							for (int byte = 0; byte < 8; byte++) {
								printf("%.2hhx ", data[row*8 + byte]);
							}
							printf("\n");
						}
					}
					else { printf("Ungueltige Parameteranzahl für register set-Befehl!\n"); }
				}
				else {
					printf("Invalid memory command!\n");
				}
			}
			//NOP instruction at PC
			else if (!strcmp(input[0], "f") || !strcmp(input[0], "fix")) {	
				writeMemory(task, pc, (void*)&NOP, 4);	
			//Continue program execution
			}else if(!strcmp(input[0],"c") || !strcmp(input[0],"continue")){
				return KERN_SUCCESS;
			//Single Step
			}else if (!strcmp(input[0], "n") || !strcmp(input[0], "next")) {	//Single Step-Case
				setSSBit(task);
				return KERN_SUCCESS;
			}else {
				printf("Invalid command!\n");
			}
		}
		
	}else{
		fprintf(stderr, "Kein Breakpoint!");
	}
	return KERN_SUCCESS;
}


//Setze das Software Step Bit im mdscr_el1 Debug-Register (für Single Steping)
void setSSBit(task_t task) {
	thread_act_port_array_t threads;										
	mach_msg_type_number_t threadsCount;
	arm_debug_state64_t debugState;

	getThreads(task, &threads, &threadsCount);								//Ermittle alle Threads des Tasks		
	getDebugState(&debugState, ARM_DEBUG_STATE64_COUNT, threads, 0);		//Ermittle den aktuellen Status des Threads, adressiert durch "index"

	debugState.__mdscr_el1 |= 1;

	setDebugState(&debugState, ARM_DEBUG_STATE64_COUNT, threads, 0);
}

//Hole Registerinhalt des 1. Threads eines Tasks
uint64_t getRegister(task_t task, int indexRegister) {
	thread_act_port_array_t threads;										//Liste zur Speicherung der Threads des Tasks
	mach_msg_type_number_t threadsCount;
	arm_thread_state64_t threadState;

	//Pausiere den Task
	getThreads(task, &threads, &threadsCount);								//Ermittle alle Threads des Tasks		
	getThreadState(&threadState, ARM_THREAD_STATE64_COUNT, threads, 0);		//Ermittle den aktuellen Status des Threads, adressiert durch "index"
	
	if (indexRegister <= 28) {
		return threadState.__x[indexRegister];
	}
	else if (indexRegister == 29) {
		return threadState.__fp;
	}
	else if (indexRegister == 30) {
		return threadState.__lr;
	}
	else if (indexRegister == 31) {
		return threadState.__sp;
	}
	else if (indexRegister == 32) {
		return threadState.__pc;
	}
	else if (indexRegister == 33) {
		return threadState.__cpsr;
	}
	else if (indexRegister == 34) {
		return threadState.__pad;
	}
	else {
		//Was kann hier hin kommen, wenn etwas ungültig ist?!
		return 0;
	}
}

//Gebe alle Register des 1. Threads des uebergebenen Tasks aus
void showRegistersFromTask(task_t task) {
	thread_act_port_array_t threads;										
	mach_msg_type_number_t threadsCount;
	arm_thread_state64_t threadState;

																			
	getThreads(task, &threads, &threadsCount);								//Determine threads of task		
	getThreadState(&threadState, ARM_THREAD_STATE64_COUNT, threads, 0);		//Determine state of first thread
	
	//Print all GPOs
	fprintf(stderr, "Program counter: 0x%llx\n", threadState.__pc);
	fprintf(stderr, "Current program status register: 0x%x\n", threadState.__cpsr);
	fprintf(stderr, "Frame pointer: 0x%llX\n", threadState.__fp);
	fprintf(stderr, "Link register: 0x%llX\n", threadState.__lr);
	fprintf(stderr, "Stack pointer: 0x%llX\n", threadState.__sp);
	fprintf(stderr, "Unkown Register: 0x%X\n", threadState.__pad);			//Dunno registers name

	for (int gpregister = 0; gpregister < 29; gpregister += 4)
		fprintf(stderr, "X%02d:%016llx	X%02d:%016llx	X%02d:%016llx	X%02d:0x%016llx\n", gpregister, threadState.__x[gpregister],
			gpregister + 1, threadState.__x[gpregister + 1], gpregister + 2, threadState.__x[gpregister + 2], gpregister + 3, threadState.__x[gpregister + 3]);
	
}

//Determine task(port) of given PID
task_t getTaskFromPID(pid_t pID) {

	if (pID != 0) {
		task_t task;

		kern_return_t kreturn = task_for_pid(mach_task_self(), pID, &task);			

		if (kreturn != KERN_SUCCESS) {										
			fprintf(stderr, "TASK_FOR_PID: %s\n", mach_error_string(kreturn));
			exit(kreturn);
		}
		else { 
			fprintf(stderr, "Succesfully determined task/-port");
			return task;
		}
	
	}
	else {
		exit(1);
	}
}

//Pause task execution(its threads) (needs some renaming)
void pauseChild(task_t task) {
	kern_return_t kreturn = task_suspend(task);						//Stop execution of all threads
	if (kreturn != KERN_SUCCESS) {									
		fprintf(stderr, "TASK_SUSPEND: %s\n", mach_error_string(kreturn));
		exit(kreturn);
	}
}

//Determine all threads of task
void getThreads(task_t task, thread_act_port_array_t* threadsList, mach_msg_type_number_t* threadsCount) {
	kern_return_t kreturn = task_threads(task, threadsList, threadsCount);		
	if (kreturn != KERN_SUCCESS) {									
		printf("TASK_THREADS: %s\n", mach_error_string(kreturn));
	}
}

//Resume execution of task (its threads)
void resumeChild(task_t task) {
	//TODO: Check if task was paused before (Critical error attempting to resume a non paused task/threads)
	kern_return_t kreturn = task_resume(task);
	if (kreturn != KERN_SUCCESS) {
		printf("TASK_RESUME: %s\n", mach_error_string(kreturn));
	}
}

//Determine debug state of a thread (indexed by "index")
void getDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_get_state(threadsList[index], ARM_DEBUG_STATE64, (thread_state_t)state, &stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_GET_STATE: %s\n", mach_error_string(kreturn));
	}
}

//Set debug state of a thread (indexed by "index")
void setDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_set_state(threadsList[index], ARM_DEBUG_STATE64, (thread_state_t)state, stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_SET_STATE: %s\n", mach_error_string(kreturn));
	}
}

//Determine registers value of a thread (indexed by "index")
void getThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_get_state(threadsList[index], ARM_THREAD_STATE64, (thread_state_t)state, &stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_GET_STATE: %s\n", mach_error_string(kreturn));
	}
}

//Set registers value of a thread (indexed by "index")
void setThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_set_state(threadsList[index], ARM_THREAD_STATE64, (thread_state_t)state, stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_SET_STATE: %s\n", mach_error_string(kreturn));
	}
}


//Set individual register of first thread (this function needs to become generalized(set content of an individual register in a given thread))
void setRegister(task_t task, int indexRegister, long long unsigned value) {
		thread_act_port_array_t threads;										
		mach_msg_type_number_t threadsCount;
		arm_thread_state64_t threadState;

		getThreads(task, &threads, &threadsCount);								
		getThreadState(&threadState, ARM_THREAD_STATE64_COUNT, threads, 0);

		if (indexRegister <= 28) {
			threadState.__x[indexRegister] = value;
		}
		else if (indexRegister == 29) {
			threadState.__fp = value;
		}
		else if (indexRegister == 30) {
			threadState.__lr = value;
		}
		else if (indexRegister == 31) {
			threadState.__sp = value;
		}
		else if (indexRegister == 32) {
			threadState.__pc = value;
		}
		else if (indexRegister == 33) {
			threadState.__cpsr = value;
		}
		else if (indexRegister == 34) {
			threadState.__pad = value;
		}
		
		setThreadState(&threadState, ARM_THREAD_STATE64_COUNT, threads, 0);
}

//Read memory chunk of tasks memory space 
unsigned char* readMemory(task_t task, vm_address_t addr, mach_vm_size_t size) {
	
	unsigned char* buf = (unsigned char*)malloc(sizeof(unsigned char) * size);
	kern_return_t kr = mach_vm_read_overwrite(task, (vm_address_t)addr, size, (vm_address_t)buf, &size);

	if (kr != KERN_SUCCESS) {
		printf("[!] Read failed. %s\n", mach_error_string(kr));
		return NULL;
	}
	else {
		return buf;
		
	}

	return NULL;

}

//Write chunk of data into tasks memory space
bool writeMemory(task_t task, mach_vm_address_t dest, void* data, unsigned int size) {
	unsigned char* dataToWrite = (unsigned char*)data;
	
	kern_return_t kern = mach_vm_protect(task, dest, 10, false, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY); //Add write privilege to/for memory chunk the data will be written to
	if (kern != KERN_SUCCESS) {
		printf("Protection1: %s\n", mach_error_string(kern));
		return false;
	}
	
	kern = mach_vm_write(task, dest, (vm_offset_t)data, (mach_msg_type_number_t)size); //Write new data into memory
	if (kern != KERN_SUCCESS) {
		printf("Write: %s\n", mach_error_string(kern));
		return false;
	}

	fprintf(stderr, "Erfolgreich!\n");


	/* Change memory protections back to r-x */
	kern = mach_vm_protect(task, dest, 10, false, VM_PROT_EXECUTE | VM_PROT_READ); //Undo write privilege
	if (kern != KERN_SUCCESS) {
		printf("Protection2: %s\n", mach_error_string(kern));
		return false;
	}
	return true;
}

//Create new exception port for given task
mach_port_t* createExceptionPort(task_t task) {
	kern_return_t kreturn;

	mach_port_t* newExceptionPort = (mach_port_t*)malloc(sizeof(mach_port_t));
	exception_mask_t exceptionMask = EXC_MASK_ALL; //Define exception mask (could be breakpont only)

	
	kreturn = mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE, newExceptionPort); //Create new port with Receive right for debugger/caller
	if (kreturn != KERN_SUCCESS) {
		fprintf(stderr, "MACH_PORT_ALLOCATE: %s\n", mach_error_string(kreturn)); 
		return NULL; 
	}

	
	kreturn = mach_port_insert_right(mach_task_self(), *newExceptionPort, *newExceptionPort, MACH_MSG_TYPE_MAKE_SEND); //Add Send right for debugger/caller to port
	if (kreturn != KERN_SUCCESS) {
		fprintf(stderr, "MACH_PORT_INSERT_RIGHT: %s\n", mach_error_string(kreturn));
		return NULL;
	}

	
	kreturn = task_set_exception_ports(task, exceptionMask, *newExceptionPort, EXCEPTION_DEFAULT, ARM_THREAD_STATE64); //Assign the new exception port to the task/debugee
	if (kreturn != KERN_SUCCESS) {
		fprintf(stderr, "TASK_SET_EXCEPTION_PORT: %s\n", mach_error_string(kreturn));
		return NULL;
	}
	
	return newExceptionPort;
}

//Create new exception handler
void createExceptionHandler(mach_port_t exceptionPort) {
	mach_msg_server(exc_server, 1052, exceptionPort, MACH_MSG_TIMEOUT_NONE); //Call message server provided by MACH-API
}

//Create new breakpoint (typical double linked list node stuff...)
bool addBreakpoint(task_t task, vm_address_t addr, bool permanent) {
	
	Breakpoint* newBP = (Breakpoint*)malloc(sizeof(Breakpoint));
	newBP->instruction = readMemory(task, addr, 4); //Save original instruction at given address
	newBP->addr = addr;
	newBP->permanent = permanent;
	newBP->next = NULL;
	newBP->previous = NULL;

	
	bool result = writeMemory(task, addr, (void*)&breakpointInstruction, 4); //Overwrite original instruction with breakpoint instruction
	if (result) {
		if (breakpointList == NULL) { breakpointList = newBP; return true; }

		Breakpoint* tmp = breakpointList;
		while (tmp->next != NULL) { tmp = tmp->next; }
		newBP->previous = tmp;
		tmp->next = newBP;

		return true;
	}else {
		free(newBP);
		fprintf(stderr, "Unable to create new breakpoint!\n");
		return false;
	}
	
	
	
}

//Print all breakpoints in breakpoint list
void printBreakpoints() {
	fprintf(stderr, "           Breakpoints:\n------------------------------------\n");
	Breakpoint* tmp = breakpointList;
	while (tmp != NULL) {
		fprintf(stderr, "Address: 0x%lx - Instruction: 0x%.2hhx%.2hhx%.2hhx%.2hhx\n", tmp->addr, tmp->instruction[0], tmp->instruction[1], tmp->instruction[2], tmp->instruction[3]);
		tmp = tmp->next;
	}
	fprintf(stderr, "------------------------------------\n");
}

//Delete breakpoint from breakpontlist/Restore original instruction (again basic double linked list sutff)
bool deleteBreakpoint(task_t task, vm_address_t addr) {
	Breakpoint* tmp = breakpointList;

	while (tmp != NULL) {
		if (tmp->addr == addr) {
			bool result = writeMemory(task, addr, tmp->instruction, 4);	//Overwrite breakpoint instruction with original instruction
			if (result) {
				if (tmp->previous != NULL) {
					tmp->previous->next = tmp->next;
					tmp->next->previous = tmp->previous;
					free(tmp);
				}
				else if (tmp->previous == NULL && tmp->next != NULL) {
					tmp->next->previous = NULL;
					breakpointList = tmp->next;
				}
				else if (tmp->next == NULL && tmp->previous != NULL) {
					tmp->previous->next = NULL;
				}
				else { breakpointList = NULL; }
				free(tmp);
			}else {
				fprintf(stderr, "Failed to restore original instruction 0x%.2hhx%.2hhx%.2hhx%.2hhx at address 0x%lx\n", tmp->instruction[0], tmp->instruction[1], tmp->instruction[2], tmp->instruction[3], tmp->addr);
				return false;
			}
		}
		tmp = tmp->next;
	}
	fprintf(stderr, "No breakpoint found for address: 0x%lx\n", tmp->addr);
	return false;
}

//Check if a breakpoint at given address exists in list (breakpoint at address was set by the debugger)
bool BreakpointExists(vm_address_t addr) {
	Breakpoint* tmp = breakpointList;
	while (tmp != NULL) {
		if (tmp->addr == addr) { return true; }
		tmp = tmp->next;
	}
	return false;
}

//Some user input stuff...
char** getInput(char* words_count) {
	int word_start_index = 0;
	int size = 0;   
	int word_size = 0;
	int words_counter = 0;  
	char** words = NULL;   
	char* input = getRAWInput(&size);

	for (int i = 0; i < size + 1; i++) {
		word_size++;
		if (input[i] == 32 || input[i] == '\0') {

			words_counter++;
			words = realloc(words, (words_counter * sizeof(char**)));
			words[words_counter - 1] = malloc(sizeof(char) * word_size);

			words[words_counter - 1] = getSubstring(input, word_start_index, word_size);

			word_start_index = i + 1;
			word_size = 0;
		}
	}

	*words_count = words_counter;
	return words;
}

//Get substring of string (for user input stuff/parsing)
char* getSubstring(char* input, int start_index, int size) {

	char* newString = (char*)malloc(sizeof(char) * size);	//Allokiere neuen Speicherbereich für den Substring
	for (int offset = 0; offset < size; offset++) {
		newString[offset] = input[start_index + offset];	
	}
	newString[size - 1] = '\0';

	return newString;

}

//User input stuff again.......
char* getRAWInput(int* outputSize) {
	unsigned int i = 0;
	const unsigned int max_length = 128;      
	unsigned int size = 0;              

	char* finalString = malloc(max_length); 
	size = max_length;


	if (finalString != NULL) {
		int c = EOF;

		
		while ((c = getchar()) != '\n' && c != EOF) {
			finalString[i++] = (char)c;

			
			if (i == size) {
				size = i + max_length;
				finalString = realloc(finalString, size);  
			}
		}

		finalString[i] = '\0';      

	}
	*outputSize = i;
	return finalString;
}