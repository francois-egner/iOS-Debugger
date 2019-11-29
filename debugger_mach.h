#include <mach/mach.h>  //Alles möglich rund um die MACH API		
#include <stdio.h>		//fprintf()
#include <stdlib.h>		//atoi(), exit(), malloc()
#include <stdbool.h>	//für boolsche Werte


//Funktionsköpfe, damit die Funktionen ins Programm eingebunden werden
kern_return_t mach_vm_write(vm_map_t target_task, mach_vm_address_t address, vm_offset_t data, mach_msg_type_number_t dataCnt);	//Daten in Speicher schreiben
kern_return_t mach_vm_protect(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection); //Protection-Level bestimmen
kern_return_t mach_vm_read_overwrite(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, mach_vm_address_t data, mach_vm_size_t* outsize);	//Daten von Speicher lesen
extern int exc_server(mach_msg_header_t* InHeadP, mach_msg_header_t* OutHeadP);

mach_msg_type_number_t stateCount = ARM_THREAD_STATE64_COUNT;	//Für sämtliche Thread-Aktionen erforderlich
task_t globalTask;		//Globaler Task für Testzwecke
const unsigned char breakpointInstruction[] = { 0x00, 0x00 ,0x20, 0xd4 };
const unsigned char NOP[] = { 0x1f, 0x20, 0x03, 0xd5 };

task_t getTaskFromPID(pid_t);	//Task-Port von Zielprozess ermitteln

//-------------------------User input------------------------------//
char** getInput();
char* getRAWInput(int* outputSize);
char* getSubstring(char* input, int start_index, int size);

//-------------------------Execution------------------------------//
void pauseChild(task_t task);	//Task pausieren
void resumeChild(task_t task);	//Task forsetzen

//-------------------------Threads------------------------------//
void getThreads(task_t task, thread_act_port_array_t* threadsList, mach_msg_type_number_t*);	//Threads eines Tasks ermitteln
void getThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int);		//State eines Threads ermitteln
void setThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int);	//State eines Threads setzen

//-------------------------Debug------------------------------//
void getDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index);
void setDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index);

//-------------------------Single Stepping------------------------------//
void setSSBit(task_t task);
//-------------------------Registers------------------------------//
void showRegistersFromTask(task_t);	//Register eines Threads anzeigen
void showRegistersFromState(arm_thread_state64_t threadState);	//State detailliert ausgeben
uint64_t getRegister(task_t task, int indexRegister);	//Registerinhalt eines Tasks ermitteln
void setRegister(task_t, int, long long unsigned);		//Registerinhalt eines Tasks setzen

//-------------------------Memory------------------------------//
unsigned char* readMemory(task_t task, vm_address_t addr, mach_vm_size_t size);	//Speicher eines Tasks auslesen
bool writeMemory(task_t task, mach_vm_address_t dest, void* data, unsigned int size);	//Speicher eines Tasks schreiben

//-------------------------Exceptions------------------------------//
mach_port_t* createExceptionPort(task_t task);	//Exception-Port erstellen und Task zuweisen
void createExceptionHandler(mach_port_t exceptionPort);	//Exception-Handler erstellen

//-------------------------Breakpoints------------------------------//
typedef struct Breakpoint {
	vm_address_t addr;
	unsigned char* instruction;
	bool permanent;
	struct Breakpoint* next;
	struct Breakpoint* previous;
}Breakpoint;
Breakpoint* breakpointList = NULL;
bool addBreakpoint(task_t task, vm_address_t addr, bool permanent);
bool deleteBreakpoint(task_t task, vm_address_t addr);
void printBreakpoints();
bool BreakpointExists(vm_address_t addr);