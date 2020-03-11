#include <mach/mach.h>  //Mach-API		
#include <stdio.h>	//fprintf()
#include <stdlib.h>	//atoi(), exit(), malloc()
#include <stdbool.h>	//boolean values


//Mach API provided function signatures
kern_return_t mach_vm_write(vm_map_t target_task, mach_vm_address_t address, vm_offset_t data, mach_msg_type_number_t dataCnt); //Write data into target_tasks memory
kern_return_t mach_vm_protect(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, boolean_t set_maximum, vm_prot_t new_protection); //Change protection level of memory region
kern_return_t mach_vm_read_overwrite(vm_map_t target_task, mach_vm_address_t address, mach_vm_size_t size, mach_vm_address_t data, mach_vm_size_t* outsize); //Read data from target_tasks memory
extern int exc_server(mach_msg_header_t* InHeadP, mach_msg_header_t* OutHeadP);	//Exception server

mach_msg_type_number_t stateCount = ARM_THREAD_STATE64_COUNT;
task_t globalTask;
const unsigned char breakpointInstruction[] = { 0x00, 0x00 ,0x20, 0xd4 }; //ARMv8 BRK instruction
const unsigned char NOP[] = { 0x1f, 0x20, 0x03, 0xd5 }; //ARMv8 NOP instruction

task_t getTaskFromPID(pid_t); //Determine port from debuggee

//-------------------------User input------------------------------//
char** getInput();
char* getRAWInput(int* outputSize);
char* getSubstring(char* input, int start_index, int size);

//-------------------------Execution------------------------------//
void pauseChild(task_t task); //Pause task
void resumeChild(task_t task); //Resume task

//-------------------------Threads------------------------------//
void getThreads(task_t task, thread_act_port_array_t* threadsList, mach_msg_type_number_t*); //Determine every existing thread of given task
void getThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index); //Determine state of a given thread (list ü index)
void setThreadState(arm_thread_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index); //Set state of a given thread (list +index)

//-------------------------Debug------------------------------//
void getDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index);
void setDebugState(arm_debug_state64_t* state, mach_msg_type_number_t stateCount, thread_act_port_array_t threadsList, int index);

//-------------------------Single Stepping------------------------------//
void setSSBit(task_t task); //Set SingleStep/SoftwareStep bit
//-------------------------Registers------------------------------//
void showRegistersFromTask(task_t); //Show the contents of the first thread of the task
void showRegistersFromState(arm_thread_state64_t threadState); //Output all informtion of given a thread state
uint64_t getRegister(task_t task, int indexRegister); //Show content of a specific register (1st thread)
void setRegister(task_t, int indexRegister, long long unsigned value); //Set content of a specific register (1st thread)

//-------------------------Memory------------------------------//
unsigned char* readMemory(task_t task, vm_address_t addr, mach_vm_size_t size);	//Read memory chunk of given tasks memory
bool writeMemory(task_t task, mach_vm_address_t dest, void* data, unsigned int size);	//Write chunk of data into given tasks memory

//-------------------------Exceptions------------------------------//
mach_port_t* createExceptionPort(task_t task); //Create exception port and assign it to given task
void createExceptionHandler(mach_port_t exceptionPort);	//Create exception handler

//-------------------------Breakpoints------------------------------//
typedef struct Breakpoint {
	vm_address_t addr;
	unsigned char* instruction;
	bool permanent;
	struct Breakpoint* next;
	struct Breakpoint* previous;
}Breakpoint; //Breakpoint list element
Breakpoint* breakpointList = NULL; //Double linked breakpoint list header
bool addBreakpoint(task_t task, vm_address_t addr, bool permanent);
bool deleteBreakpoint(task_t task, vm_address_t addr);
void printBreakpoints();
bool BreakpointExists(vm_address_t addr);
