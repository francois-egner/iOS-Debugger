Based on the elaboration for the project group "IT Security" at the Rheinische Friedrich-Wilhelms-University in Bonn.  
Elaboration to be found at: https://www.francois-egner.de/projekte ("Development of an iOS-Debugger")    
  
Final presentation of the project group: https://bit.ly/2L6bEaO  

Usage:  
----------------------------  
Write to memory: memory write [0xAddress] [0xData]    
Read memory: memory read [0x address]    
Read register: register read [index]  
Write to registers register set [index] [0xData]  
Show all registers: register showAll  
Set software breakpoint: breakpoint set [0x address].  
Delete software breakpoint: breakpoint delete [0xAddress].  
Show all breakpoints: breakpoint showAll  
Single Step: n or next  
Continue: c or continue  
  
Implemented:  
------------
Pause & resume task  
Memory read & write  
Register read & write  
Set & delete breakpoint  
single step  
  
Still to be implemented, if necessary:  
--------------------------------------  
Code segment -> Assembly
