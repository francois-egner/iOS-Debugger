#include "debugger_mach.h"

int main(int ac, char **av){													
																					
	pid_t pid = atoi(av[1]);			//Übergebene PID als Integer entgegennehmen
	globalTask = getTaskFromPID(pid);		//Globale Task-Variable setzen													

	mach_port_t* port = createExceptionPort(globalTask);	//Exception-Port für Child-Task erstellen und setzen
	createExceptionHandler(*port);							//Exception registrieren

    return 0;
}


//Exception-Handling aka. Debug-Loop
extern kern_return_t catch_exception_raise(
	mach_port_t                          exception_port,
	mach_port_t                                  thread,
	mach_port_t                                    task,
	exception_type_t                          exception,
	exception_data_t                               code,
	mach_msg_type_number_t                   code_count
) {	
	
	if (exception == EXC_BREAKPOINT) {
		uint64_t pc = getRegister(task, 32);	//Program Counter auslesen
		unsigned char* instruction = readMemory(task, pc, 4);	//Aktuelle Instruktion auslesen
		fprintf(stderr, "Adresse: %llx Instruktion: 0x%.2hhx%.2hhx%.2hhx%.2hhx\n", pc, instruction[0], instruction[1], instruction[2], instruction[3]);

		//Originale Instruktion des Breakpoints wiederherstellen, wenn ein Breakpoint dort von diesem Debugger gesetzt wurde
		if (BreakpointExists(pc)) {		//Wenn es einen Breakpoint für die Adresse ist, die derzeit den Interrupt beinhaltet
			deleteBreakpoint(task, pc);
		}//writeMemory(task, pc, (void*)&NOP, 4);	//Schreibe eine NOP an die Stelle des Breakpoints, der nicht vom Debugger behandelt werden kann
		
		while (true) {
			printf(">");
			int groesse = 0;
			char** input = getInput(&groesse);

			if (!strcmp(input[0], "breakpoint")) {
				if (groesse == 1) {		//Wenn kein Parameter für den Breakpoint-Befehl angegeben wurde...
					printf("Fehlender Parameter!\n");
				}else if (!strcmp(input[1], "set")) {
					if (groesse == 3) {		//Prüfe ob es einen weiteren Parameter gibt, der die Adresse für den neuen Breakpoint ist
						vm_address_t addr = (vm_address_t)strtoull(input[2], NULL, 16);	//Wandle den eingegebenen Hex-String in Adresse um
						addBreakpoint(task, addr, false);	//Setze den Breakpoint
					}
				}else if (!strcmp(input[1], "showAll")) {
					printBreakpoints();
				}else if (!strcmp(input[1], "delete")) {
					if (groesse == 3) {
						vm_address_t addr = strtoull(input[2], NULL, 16);
						if (BreakpointExists(addr)) {
							deleteBreakpoint(task, addr);
						}else {
							printf("Breakpoint konnte nicht gelöscht werden, da dieser nicht existiert!\n");
						}
					}else { printf("Nicht genug Parameter für breakpoint delete-Befehl!\n"); }
				}else {
					printf("Ungueltiger breakpoint-Befehl!\n");
				}
			}else if (!strcmp(input[0], "register")) {	//Single Step-Case
				if (groesse ==	1) {
					printf("Fehlende Parameter!");
				}else if (!strcmp(input[1],"showAll")) {		//Zeige alle Register an
					showRegistersFromTask(task);	
				}else if (!strcmp(input[1], "set")) {		//Beschreibe Register
					if (groesse == 4) {
						int index = atoi(input[2]);
						unsigned long long int data = strtoull(input[3], NULL, 16);
						setRegister(task, index, data);
						printf("Register %d auf 0x%llx\n", index, data);
					}else { printf("Ungueltige Parameteranzahl für register set-Befehl!"); }
				}else if (!strcmp(input[1], "read")) {		//Beschreibe Register
					if (groesse == 3) {
						int index = atoi(input[2]);
						uint64_t data = getRegister(task, index);
						printf("Register %d: 0x%llx\n", index, data);
					}
					else { printf("Ungueltige Parameteranzahl für register set-Befehl!"); }
				}else {
					printf("Ungueltiger register-Befehl!\n");
				}
			}else if (!strcmp(input[0], "memory")) {	//Single Step-Case
				if (groesse == 1) {
					printf("Fehlende Parameter!\n");
				}else if (!strcmp(input[1], "write")) {		//Beschreibe Register
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
					}else { printf("Ungueltige Parameteranzahl für memory write-Befehl!"); }
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
					printf("Ungueltiger memory-Befehl!\n");
				}
			}
			else if (!strcmp(input[0], "f") || !strcmp(input[0], "fix")) {	//Fix für den hardcoded Breakpoint im Programmcode
				writeMemory(task, pc, (void*)&NOP, 4);	//Schreibe eine NOP an die Stelle des Breakpoints, der nicht vom Debugger behandelt werden kann
			}else if(!strcmp(input[0],"c") || !strcmp(input[0],"continue")){	//Continue-Case
				return KERN_SUCCESS;
			}else if (!strcmp(input[0], "n") || !strcmp(input[0], "next")) {	//Single Step-Case
				setSSBit(task);
				return KERN_SUCCESS;
			}else {
				printf("Ungueltiger Befehl!\n");
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
	thread_act_port_array_t threads;										//Liste zur Speicherung der Threads des Tasks
	mach_msg_type_number_t threadsCount;
	arm_thread_state64_t threadState;

																			//Pausiere den Task
	getThreads(task, &threads, &threadsCount);								//Ermittle alle Threads des Tasks		
	getThreadState(&threadState, ARM_THREAD_STATE64_COUNT, threads, 0);		//Ermittle den aktuellen Status des Threads, adressiert durch "index"

	fprintf(stderr, "Program counter: 0x%llx\n", threadState.__pc);
	fprintf(stderr, "Current program status register: 0x%x\n", threadState.__cpsr);
	fprintf(stderr, "Frame pointer: 0x%llX\n", threadState.__fp);
	fprintf(stderr, "Link register: 0x%llX\n", threadState.__lr);
	fprintf(stderr, "Stack pointer: 0x%llX\n", threadState.__sp);
	fprintf(stderr, "Unbekanntes Register: 0x%X\n", threadState.__pad);

	for (int gpregister = 0; gpregister < 29; gpregister += 4)
		fprintf(stderr, "X%02d:%016llx	X%02d:%016llx	X%02d:%016llx	X%02d:0x%016llx\n", gpregister, threadState.__x[gpregister],
			gpregister + 1, threadState.__x[gpregister + 1], gpregister + 2, threadState.__x[gpregister + 2], gpregister + 3, threadState.__x[gpregister + 3]);
	
}

//Ermittle Task anhand der Prozess-ID
task_t getTaskFromPID(pid_t pID) {

	if (pID != 0) {
		task_t task;

		kern_return_t kreturn = task_for_pid(mach_task_self(), pID, &task);			//Ermittle Task des Prozesses -> Parent ist dieser Prozess & Task wird in "task" gespeichert

		if (kreturn != KERN_SUCCESS) {										//Ermittlung fehlgeschlagen....
			fprintf(stderr, "TASK_FOR_PID: %s\n", mach_error_string(kreturn));
			exit(kreturn);
		}
		else { 
			fprintf(stderr, "Task erfolgreich ermittelt!\n");
			return task;
		}
	
	}
	else {
		exit(1);
	}
}

//Pausiere die Ausfuehrung des uebergebenen Tasks
void pauseChild(task_t task) {
	kern_return_t kreturn = task_suspend(task);									//Task "anhalten" -> Die Ausführung aller Threads wird gestoppt
	if (kreturn != KERN_SUCCESS) {									//Anhalten fehlgeschlagen...
		fprintf(stderr, "TASK_SUSPEND: %s\n", mach_error_string(kreturn));
		exit(kreturn);
	}
}

//Ermittle alle Threads des uebergebenen Tasks
void getThreads(task_t task, thread_act_port_array_t* threadsList, mach_msg_type_number_t* threadsCount) {
	kern_return_t kreturn = task_threads(task, threadsList, threadsCount);		//Ermittle alle Threads des Tasks -> Speichere alle in "thread_list" und Anzahl dieser in "thread_count"
	if (kreturn != KERN_SUCCESS) {									//Ermittlung fehlgeschlagen...
		printf("TASK_THREADS: %s\n", mach_error_string(kreturn));
	}
}

//Setze die Ausfuehrung eines pausierten Tasks fort
void resumeChild(task_t task) {
	//TODO: Pruefe ob Task überhaupt angehalten ist
	kern_return_t kreturn = task_resume(task);
	if (kreturn != KERN_SUCCESS) {
		printf("TASK_RESUME: %s\n", mach_error_string(kreturn));
	}
}

//Ermittle Debug-State
void getDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_get_state(threadsList[index], ARM_DEBUG_STATE64, (thread_state_t)state, &stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_GET_STATE: %s\n", mach_error_string(kreturn));
	}
}

//Setze Debug-State (für SS-Bit)
void setDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_set_state(threadsList[index], ARM_DEBUG_STATE64, (thread_state_t)state, stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_SET_STATE: %s\n", mach_error_string(kreturn));
	}
}

//Ermittle den Status/State (Registerinhalte) eines uebergebenen Threads
void getThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_get_state(threadsList[index], ARM_THREAD_STATE64, (thread_state_t)state, &stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_GET_STATE: %s\n", mach_error_string(kreturn));
	}
}

//Setze den Status/State des uebergebenen Threads
void setThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index) {
	kern_return_t kreturn = thread_set_state(threadsList[index], ARM_THREAD_STATE64, (thread_state_t)state, stateCount);
	if (kreturn != KERN_SUCCESS) {
		printf("THREAD_SET_STATE: %s\n", mach_error_string(kreturn));
	}
}

//Ein Register des 1. Threads des uebergebenen Tasks setzen
void setRegister(task_t task, int indexRegister, long long unsigned value) {
		thread_act_port_array_t threads;										//Liste zur Speicherung der Threads des Tasks
		mach_msg_type_number_t threadsCount;
		arm_thread_state64_t threadState;

		getThreads(task, &threads, &threadsCount);								//Ermittle alle Threads des Tasks		
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

//size Bytes ab Stelle addr aus Speicher des übergebenen Task lesen
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

//Speicher des übergebenen Task überschreiben
bool writeMemory(task_t task, mach_vm_address_t dest, void* data, unsigned int size) {
	unsigned char* dataToWrite = (unsigned char*)data;

	//fprintf(stderr, "Writing at 0x%llx: 0x%.2hhx%.2hhx%.2hhx%.2hhx\n", dest, dataToWrite[0], dataToWrite[1], dataToWrite[2], dataToWrite[3]);
	
	kern_return_t kern = mach_vm_protect(task, dest, 10, false, VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
	if (kern != KERN_SUCCESS) {
		printf("Protection1: %s\n", mach_error_string(kern));
		return false;
	}
	
	kern = mach_vm_write(task, dest, (vm_offset_t)data, (mach_msg_type_number_t)size);
	if (kern != KERN_SUCCESS) {
		printf("Write: %s\n", mach_error_string(kern));
		return false;
	}

	fprintf(stderr, "Erfolgreich!\n");


	/* Change memory protections back to r-x */
	kern = mach_vm_protect(task, dest, 10, false, VM_PROT_EXECUTE | VM_PROT_READ);
	if (kern != KERN_SUCCESS) {
		printf("Protection2: %s\n", mach_error_string(kern));
		return false;
	}
	return true;
}

//Erstelle einen ExceptionPort für child task
mach_port_t* createExceptionPort(task_t task) {
	kern_return_t kreturn;

	mach_port_t* newExceptionPort = (mach_port_t*)malloc(sizeof(mach_port_t));
	exception_mask_t exceptionMask = EXC_MASK_ALL;

	//Erstelle einen neuen Port
	kreturn = mach_port_allocate(mach_task_self(),MACH_PORT_RIGHT_RECEIVE, newExceptionPort);
	if (kreturn != KERN_SUCCESS) {
		fprintf(stderr, "MACH_PORT_ALLOCATE: %s\n", mach_error_string(kreturn)); 
		return NULL; 
	}

	//Vergebe Rechte, die ein Exception-Port benoetigt
	kreturn = mach_port_insert_right(mach_task_self(), *newExceptionPort, *newExceptionPort, MACH_MSG_TYPE_MAKE_SEND);
	if (kreturn != KERN_SUCCESS) {
		fprintf(stderr, "MACH_PORT_INSERT_RIGHT: %s\n", mach_error_string(kreturn));
		return NULL;
	}

	//Ordne den neuen Exception-Port dem Debugee zu
	kreturn = task_set_exception_ports(task, exceptionMask, *newExceptionPort, EXCEPTION_DEFAULT, ARM_THREAD_STATE64);
	if (kreturn != KERN_SUCCESS) {
		fprintf(stderr, "TASK_SET_EXCEPTION: %s\n", mach_error_string(kreturn));
		return NULL;
	}
	
	//Gebhe den neuen Exception-Port zurück
	return newExceptionPort;
}

//Exception-Handler erstellen/starten
void createExceptionHandler(mach_port_t exceptionPort) {
	mach_msg_server(exc_server, 1052, exceptionPort, MACH_MSG_TIMEOUT_NONE);
}

//Breakpoint hinzufügen
bool addBreakpoint(task_t task, vm_address_t addr, bool permanent) {
	//Neue Sicherung erstellen
	Breakpoint* newBP = (Breakpoint*)malloc(sizeof(Breakpoint));
	newBP->instruction = readMemory(task, addr, 4);
	newBP->addr = addr;
	newBP->permanent = permanent;
	newBP->next = NULL;
	newBP->previous = NULL;

	//Originale Instruktion mit Breakpoint-Instruktion überschreiben
	bool result = writeMemory(task, addr, (void*)&breakpointInstruction, 4);
	if (result) {
		if (breakpointList == NULL) { breakpointList = newBP; return true; }		//Wenn Liste leer ist, dann setze neuen BP als erstes Element mit keinem Vorgänger und Nachfolger

		Breakpoint* tmp = breakpointList;
		while (tmp->next != NULL) { tmp = tmp->next; }
		newBP->previous = tmp;
		tmp->next = newBP;

		return true;
	}else {
		free(newBP);
		fprintf(stderr, "Breakpoint-Instruktion konnte nicht geschrieben werden ");
		return false;
	}
	
	
	
}

//Breakpoints als Liste ausgeben
void printBreakpoints() {
	fprintf(stderr, "           Breakpoints:\n------------------------------------\n");
	Breakpoint* tmp = breakpointList;
	while (tmp != NULL) {
		fprintf(stderr, "Adresse: 0x%lx - Instruktion: 0x%.2hhx%.2hhx%.2hhx%.2hhx\n", tmp->addr, tmp->instruction[0], tmp->instruction[1], tmp->instruction[2], tmp->instruction[3]);
		tmp = tmp->next;
	}
	fprintf(stderr, "------------------------------------\n");
}

//Breakpoint aus Liste entfernen
bool deleteBreakpoint(task_t task, vm_address_t addr) {
	Breakpoint* tmp = breakpointList;

	while (tmp != NULL) {
		if (tmp->addr == addr) {
			bool result = writeMemory(task, addr, tmp->instruction, 4);	//Schreibe originale Instruktion in den Programmspeicher
			if (result) {
				if (tmp->previous != NULL) {
					tmp->previous->next = tmp->next;
					tmp->next->previous = tmp->previous;
					free(tmp);
					fprintf(stderr, "Wiederherstellung erfolgreich!\n");
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
				fprintf(stderr, "Fehler bei Wiederherstellung der Instruktion 0x%.2hhx%.2hhx%.2hhx%.2hhx an Stelle 0x%lx\n", tmp->instruction[0], tmp->instruction[1], tmp->instruction[2], tmp->instruction[3], tmp->addr);
				return false;
			}
		}
		tmp = tmp->next;
	}
	fprintf(stderr, "Kein Breakpoint für Speicheradresse 0x%lx nicht gefunden!\n", tmp->addr);
	return false;
}

//Prüfen ob für eine übergebene Adresse ein Breakpoint in der Liste existiert
bool BreakpointExists(vm_address_t addr) {
	Breakpoint* tmp = breakpointList;
	while (tmp != NULL) {
		if (tmp->addr == addr) { return true; }
		tmp = tmp->next;
	}
	return false;
}

//Die Eingabe des Nutzers entgegennehmen und als Wort-Array zurückgeben
char** getInput(char* words_count) {
	int word_start_index = 0;
	int size = 0;   //Größe der rohen Eingabe
	int word_size = 0;
	int words_counter = 0;  //Wortzähler
	char** words = NULL;   //Array der Wörter
	char* input = getRAWInput(&size);   //Rohe Eingabe des Nutzers

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

//Teilstring aus übergebenem String ab Start-Index extrahieren
char* getSubstring(char* input, int start_index, int size) {

	char* newString = (char*)malloc(sizeof(char) * size);	//Allokiere neuen Speicherbereich für den Substring
	for (int offset = 0; offset < size; offset++) {
		newString[offset] = input[start_index + offset];	
	}
	newString[size - 1] = '\0';

	return newString;

}

//Komplette "rohe" Eingabe des Nutzers entgegennehmen
char* getRAWInput(int* outputSize) {
	unsigned int i = 0;
	const unsigned int max_length = 128;      //Maximale Länge des Eingabestrings für den Anfang
	unsigned int size = 0;              //Zähler für die jeweils maximale Größe des Speichers

	char* finalString = malloc(max_length); //Allokiere Speicher für Eingabe mit voerst maximaler Länge
	size = max_length;


	if (finalString != NULL) {
		int c = EOF;

		//Solgange kein keine neue Zeile angefordert wird ("Enter")
		while ((c = getchar()) != '\n' && c != EOF) {
			finalString[i++] = (char)c;

			//Wenn die maximale Anzahl an verfügbaren Bytes erreicht ist
			if (i == size) {
				size = i + max_length;
				finalString = realloc(finalString, size);   //Allokiere weitere max_length Bytes für weitere Eingabe
			}
		}

		finalString[i] = '\0';      //Füge dem eingelesenen String noch einen NULL-Charakter hinzu

	}
	*outputSize = i;
	return finalString;
}