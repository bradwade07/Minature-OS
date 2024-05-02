# Minature-OS

## Done by:
- Tawheed Sarker Aakash
- Mohammad Ashraful Islam Bhuiyan


This assignment uses the provided list implementation and static memory allocation to make lists and uses pointer manipulation to read/write data. PCB (The process datatype), SRInfo (Sender Receiver Information) and SEM (Semaphore) datatypes are used to make the simulation. PCB is the main process datatype, initialized with pid: -1, priority: -1, state: -1 (later enummed) and msg: "". Pid 0 is held for the init process. SRInfo stores messages and the sender's and receivers pid. SEM is used for semaphore implentation. For testing/debugging purposes a testing function, printLists() has been left in the code which can be accessed by the option 1 (not listed in the main menu). TAs can call it to see where a certain PCB is currently (any of the ready queues, blocked queue or running process) as well as the master PCBPool, SRInfoPool and all the initialized semaphores with their waiting queues. If a certain function needs clarification please email tsa140@sfu.ca for explanation.