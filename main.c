#include <stdio.h>
#include <string.h>

#include "list.h"

#define COMMANDCHARSSIZE 13
#define MAX_SEMAPHORE_COUNT 5

static int globalPidHolder = 1;
typedef enum
{
    READY,
    RUNNING,
    BLOCKED,
    UNINITIALIZED
} processState;

typedef struct PCB_s PCB;
struct PCB_s
{
    int pid;
    int priority;
    processState state;
    char msg[40]; // for send and reply
};
typedef struct SenderReceiverInfo_s SRInfo;
struct SenderReceiverInfo_s
{
    int senderPid;
    int receiverPid;
    char msg[40];
};

typedef struct Semaphore_s SEM;
struct Semaphore_s
{
    int id;
    int value;
    List *waitingQueue;
};

static List *priority0ReadyQueue;
static List *priority1ReadyQueue;
static List *priority2ReadyQueue;
static List *blockedQueue;
static PCB *runningProcess;
static List *SRInfoList;
static PCB PCBPool[LIST_MAX_NUM_NODES];
static SRInfo SRInfoPool[LIST_MAX_NUM_NODES];
static SEM SEMPool[MAX_SEMAPHORE_COUNT];
static List *freePCBList;
static List *freeSRInfoList;
static PCB *initProcess;
static List *freeSemList;
static List *semList;

static int nodesUsed;
static int SRInfoUsed;
static int semUSed;

// helers
void printMainMenu()
{
    printf("Welcome to OS Simulation, Please choose the respective character to call the respective commands\n");
    printf("1. C - Create [create a process and put it on the appropriate ready Q.]\n");
    printf("2. F - Fork [Copy the currently running process and put it on the ready Q corresponding to the original process' priority. Attempting to Fork the \"init\" process (see below) should fail.]\n");
    printf("3. K - Kill [kill the named process and remove it from the system.]\n");
    printf("4. E - Exit [kill the currently running process.]\n");
    printf("5. Q - Quantum [time quantum of running process expires.]\n");
    printf("6. S - Send [send a message to another process - block until reply]\n");
    printf("7. R - Receive [receive a message - block until one arrives]\n");
    printf("8. Y - Reply [unblocks sender and delivers reply]\n");
    printf("9. N - New Semaphore [Initialize the named semaphore with the value given. ID's can take a value from 0 to 4. This can only be done once for a semaphore - subsequent attempts result in error.]\n");
    printf("10. P - Semaphore P [execute the semaphore P operation on behalf of the running process. You can assume sempahores IDs numbered 0 through 4.]\n");
    printf("11. V - Semaphore V [execute the semaphore V operation on behalf of the running process. You can assume sempahores IDs numbered 0 through 4.]\n");
    printf("12. I - Procinfo [dump complete state information of process to screen (this includes process status and anything else you can think of)]\n");
    printf("13. T - Totalinfo [display all process queues and their contents]\n");
    printf("14. X - Exit the program\n");
}
void initFn()
{
    int nodesUsed = 0;
    int SRInfoUsed = 0;
    priority0ReadyQueue = List_create();
    priority1ReadyQueue = List_create();
    priority2ReadyQueue = List_create();
    SRInfoList = List_create();
    blockedQueue = List_create();
    freePCBList = List_create();
    freeSRInfoList = List_create();

    for (int i = nodesUsed; i < LIST_MAX_NUM_NODES; i++)
    {
        PCB *newPCB = &PCBPool[i];
        newPCB->pid = -1;
        newPCB->priority = -1;
        newPCB->state = UNINITIALIZED;
        strcpy(newPCB->msg, "");
        List_append(freePCBList, &PCBPool[i]);
    }
    for (int i = SRInfoUsed; i < LIST_MAX_NUM_NODES; i++)
    {
        SRInfo *newSrinfo = &SRInfoPool[i];
        newSrinfo->senderPid = -1;
        newSrinfo->receiverPid = -1;
        strcpy(newSrinfo->msg, " ");
        List_append(freeSRInfoList, newSrinfo);
    }
    initProcess = (PCB *)List_remove(freePCBList);
    initProcess->pid = 0;
    initProcess->state = RUNNING;
    runningProcess = initProcess;
    nodesUsed++;

    // semaphore init
    int semUsed = 0;
    semList = List_create();
    freeSemList = List_create();
    for (int i = semUSed; i < MAX_SEMAPHORE_COUNT; i++)
    {
        SEM *sem = &SEMPool[i];
        sem->id = -1;
        sem->value = -1;
        sem->waitingQueue = List_create();
        List_append(freeSemList, sem);
    }
}

void checkerFn()
{
    if (runningProcess == initProcess)
    {
        PCB *currentProcess = NULL;
        if (List_count(priority0ReadyQueue) > 0)
        {
            initProcess->state = READY;
            currentProcess = List_trim(priority0ReadyQueue);
        }
        else if (List_count(priority1ReadyQueue) > 0)
        {
            initProcess->state = READY;
            currentProcess = List_trim(priority1ReadyQueue);
        }
        else if (List_count(priority2ReadyQueue) > 0)
        {
            initProcess->state = READY;
            currentProcess = List_trim(priority2ReadyQueue);
        }
        else
        {
            currentProcess = initProcess;
        }
        currentProcess->state = RUNNING;
        runningProcess = currentProcess;
    }
    //For blocked processes that received a message
    if (List_count(blockedQueue) > 0 && List_count(SRInfoList) > 0)
    {
        List_first(blockedQueue);
        while (blockedQueue->pCurrentNode != NULL)
        {
            List_first(SRInfoList);
            while (SRInfoList->pCurrentNode != NULL)
            {
                if (((PCB *)(blockedQueue->pCurrentNode->pItem))->pid == ((SRInfo *)(SRInfoList->pCurrentNode->pItem))->receiverPid) // Check if a blockedQueue's pid matches an SRInfo receiverpi
                {
                    PCB *blockedProcess = List_remove(blockedQueue);
                    SRInfo *srinfo = List_remove(SRInfoList);
                    strcpy(blockedProcess->msg, srinfo->msg);
                    srinfo->receiverPid = -1;
                    srinfo->senderPid = -1;
                    strcpy(srinfo->msg, "");
                    List_remove(SRInfoList);
                    List_prepend(freeSRInfoList, srinfo);
                    blockedProcess->state = READY;
                    if (blockedProcess->priority == 0)
                    {
                        List_prepend(priority0ReadyQueue, blockedProcess);
                    }
                    else if (blockedProcess->priority == 1)
                    {
                        List_prepend(priority1ReadyQueue, blockedProcess);
                    }
                    else if (blockedProcess->priority == 2)
                    {
                        List_prepend(priority2ReadyQueue, blockedProcess);
                    }
                    break;
                }
                SRInfoList->pCurrentNode = SRInfoList->pCurrentNode->pNext;
            }
            blockedQueue->pCurrentNode = blockedQueue->pCurrentNode->pNext;
        }
    }
}

PCB *createProcessHelperFn(int pid, int priority) // Sometimes you ask user for info, sometimes routines give you the info
{
    if (nodesUsed >= LIST_MAX_NUM_NODES)
    {
        printf("Error: All nodes are in use\n");
        return NULL;
    }
    List_first(freePCBList);
    PCB *newProcess = List_remove(freePCBList);

    newProcess->pid = pid;
    globalPidHolder++;
    newProcess->priority = priority;
    newProcess->state = READY;
    nodesUsed++;
    return newProcess;
}

// routines
int createFn()
{
    if (nodesUsed >= LIST_MAX_NUM_NODES)
    {
        printf("Error: All nodes are in use.\n");
        printf("Create Function Exitted\n");
        return -1;
    }
    int priority;
    printf("Enter prority (0 = high, 1 = norm, 2 = low): ");
    scanf("%d", &priority);
    PCB *newProcess = createProcessHelperFn(globalPidHolder, priority);
    if (newProcess->priority == 0)
    {
        List_prepend(priority0ReadyQueue, newProcess);
    }
    else if (newProcess->priority == 1)
    {
        List_prepend(priority1ReadyQueue, newProcess);
    }
    else if (newProcess->priority == 2)
    {
        List_prepend(priority2ReadyQueue, newProcess);
    }
    return newProcess->pid;
}

int forkFn()
{
    if (nodesUsed >= LIST_MAX_NUM_NODES)
    {
        printf("Error: All nodes are in use.\n");
        printf("Fork Function Exitted\n");
        return -1;
    }
    PCB *currentProcess = runningProcess;
    if (currentProcess == NULL)
    {
        printf("Error: No process is running!\n");
        printf("Fork Function Exitted\n");
        return -1;
    }
    if (currentProcess->pid > 0)
    {
        PCB *forkedProcess = createProcessHelperFn(globalPidHolder, currentProcess->priority);
        if (forkedProcess->priority == 0)
        {
            List_prepend(priority0ReadyQueue, forkedProcess);
        }
        else if (forkedProcess->priority == 1)
        {
            List_prepend(priority1ReadyQueue, forkedProcess);
        }
        else if (forkedProcess->priority == 2)
        {
            List_prepend(priority2ReadyQueue, forkedProcess);
        }
    }
    return 1;
}

int killFn()
{
    if (nodesUsed <= 1)
    {
        printf("Error: No nodes are in use.\n");
        printf("Kill Function Exitted\n");
        return -1;
    }
    int pid;
    printf("Enter pid (pid of process to be killed): ");
    scanf("%d", &pid);
    PCB *processToKill = NULL;
    if (pid > 0)
    {
        for (int i = 0; i < LIST_MAX_NUM_NODES; i++)
        {
            if (PCBPool[i].pid == pid)
            {
                processToKill = &PCBPool[i];
                break;
            }
        }
        if (processToKill == NULL)
        {
            printf("Process with pid: %d not found!", pid);
            return 1;
        }
        if (processToKill->state == BLOCKED)
        {
            printf("Process with pid: %d is in blocked state!", pid);
            printf("Kill Function Completed\n");
            return 1;
        }
        else if (processToKill->state == RUNNING)
        {
            runningProcess = initProcess;
        }
        else if (processToKill->state == READY)
        {
            List *priorityQueue = NULL;
            if (processToKill->priority == 0)
            {
                priorityQueue = priority0ReadyQueue;
            }
            else if (processToKill->priority == 1)
            {
                priorityQueue = priority1ReadyQueue;
            }
            else if (processToKill->priority == 2)
            {
                priorityQueue = priority2ReadyQueue;
            }
            List_first(priorityQueue);
            while (priorityQueue->pCurrentNode != NULL)
            {
                if (((PCB *)(priorityQueue->pCurrentNode->pItem))->pid == pid)
                {
                    List_remove(priorityQueue);
                    break;
                }
                priorityQueue->pCurrentNode = priorityQueue->pCurrentNode->pNext;
            }
        }
        processToKill->pid = -1;
        processToKill->priority = -1;
        processToKill->state = UNINITIALIZED;
        nodesUsed--;
        strcpy(processToKill->msg, "");
        List_prepend(freePCBList, processToKill);
    }
    else
    {
        printf("Error: Cannot kill init process\nKill Function Exitted\n");
    }
    return 1;
}

int exitFn()
{
    PCB *processToExit = runningProcess;
    if (processToExit->pid > 0)
    {
        processToExit->pid = -1;
        processToExit->priority = -1;
        processToExit->state = UNINITIALIZED;
        nodesUsed--;
        strcpy(processToExit->msg, "");
        List_prepend(freePCBList, processToExit);
        runningProcess = initProcess;
    }
    else if ((List_count(priority0ReadyQueue) + List_count(priority1ReadyQueue) + List_count(priority2ReadyQueue)) <= 0)
    {
        processToExit->pid = -1;
        processToExit->priority = -1;
        processToExit->state = UNINITIALIZED;
        strcpy(processToExit->msg, "");
        List_prepend(freePCBList, processToExit);
        nodesUsed--;
        return 2;
    }
    else
    {
        printf("Error: Cannot exit init process\n Exit Function Exitted\n");
    }
    return 1;
}

int quantumFn()
{
    if (runningProcess->pid == 0)
    {
        printf("Cannot call Quantum call on init process\nQuantum Function Exitted\n");
        return 1;
    }
    runningProcess->state = READY;
    List *queue = NULL;
    if (runningProcess->priority == 0)
    {
        queue = priority0ReadyQueue;
    }
    else if (runningProcess->priority == 1)
    {
        queue = priority1ReadyQueue;
    }
    else if (runningProcess->priority == 2)
    {
        queue = priority2ReadyQueue;
    }
    List_prepend(queue, runningProcess);
    runningProcess = initProcess;
    return 1;
}

int sendFn()
{
    if (nodesUsed >= LIST_MAX_NUM_NODES)
    {
        printf("Error: all nodes in use\nSend Function Exitted\n");
        return 1;
    }
    else if (SRInfoUsed >= LIST_MAX_NUM_NODES)
    {
        printf("Error: all SRI nodes in use\nSend Function Exitted\n");
        return 1;
    }
    int pid;
    char msg[40];
    int priority;
    printf("Enter pid (pid of process to send message to): ");
    scanf("%d", &pid);
    printf("Enter prority (0 = high, 1 = norm, 2 = low): ");
    scanf("%d", &priority);
    if (pid == globalPidHolder)
    {
        printf("Error: a process cannot send a message to itself\nSend Function Exitted\n");
    }
    printf("Enter your message (max 40 characters and null terminated): ");
    scanf(" %[^\n]s", msg);
    PCB *senderProcess = createProcessHelperFn(globalPidHolder, priority);
    senderProcess->state = BLOCKED;
    List_append(blockedQueue, senderProcess);
    List_first(freeSRInfoList);
    SRInfo *newSrinfo = List_remove(freeSRInfoList);
    newSrinfo->senderPid = senderProcess->pid;
    newSrinfo->receiverPid = pid;
    strcpy(newSrinfo->msg, msg);
    SRInfoUsed++;
    List_prepend(SRInfoList, newSrinfo);
    return 1;
}

int receiveFn()
{
    if (runningProcess->pid == 0)
    {
        printf("Error: init process cannot receive message\n Receive Function Exitted\n");
        return 1;
    }
    SRInfo *receiverSRInfo = NULL;
    List_first(SRInfoList);
    while (SRInfoList->pCurrentNode != NULL)
    {
        if (((SRInfo *)(SRInfoList->pCurrentNode->pItem))->receiverPid == runningProcess->pid)
        {
            receiverSRInfo = (SRInfo *)(SRInfoList->pCurrentNode->pItem);
            break;
        }
        SRInfoList->pCurrentNode = SRInfoList->pCurrentNode->pNext;
    }
    if (receiverSRInfo == NULL)
    {
        runningProcess->state = BLOCKED;
        List_prepend(blockedQueue, runningProcess);
        runningProcess = initProcess;
        printf("Error: SRInfo not found\n Receive Function Exitted\n");
        return 1;
    }
    printf("Message received from PID %d: %s\n", receiverSRInfo->senderPid, receiverSRInfo->msg);
    return 1;
}

int replyFn()
{
    int pid;
    char msg[40];
    printf("Enter pid (pid of process to reply message to): ");
    scanf("%d", &pid);
    SRInfo *srinfo = NULL;
    Node *current = SRInfoList->pFirstNode;
    while (current != NULL)
    {
        if (((SRInfo *)(current->pItem))->senderPid == pid && ((SRInfo *)(current->pItem))->receiverPid == runningProcess->pid)
        {
            srinfo = ((SRInfo *)(current->pItem));
            break;
        }
       current = current->pNext;
    }
    if (srinfo == NULL)
    {
        printf("Error: SRInfo not found\nReply Function Exitted\n");
        return 1;
    }
    srinfo->senderPid = -1;
    srinfo->receiverPid = -1;
    strcpy(srinfo->msg, "");
    List_prepend(freeSRInfoList, srinfo);
    SRInfoUsed--;
    printf("Enter your message (max 40 characters and null terminated): ");
    scanf(" %[^\n]s", msg);
    List_first(blockedQueue);
    PCB *blockedProcess = NULL;
    while (blockedQueue->pCurrentNode != NULL)
    {
        if (((PCB *)(blockedQueue->pCurrentNode->pItem))->pid == pid)
        {
            blockedProcess = List_remove(blockedQueue);
            break;
        }
        blockedQueue->pCurrentNode = blockedQueue->pCurrentNode->pNext;
    }
    strcpy(blockedProcess->msg, msg);
    blockedProcess->state = READY;
    List *queue = NULL;
    if (blockedProcess->priority == 0)
    {
        queue = priority0ReadyQueue;
    }
    else if (blockedProcess->priority == 1)
    {
        queue = priority1ReadyQueue;
    }
    else if (blockedProcess->priority == 2)
    {
        queue = priority2ReadyQueue;
    }
    List_prepend(queue, blockedProcess);
    return 1;
}

int newSemaphoreFn()
{
    if (semUSed >= MAX_SEMAPHORE_COUNT)
    {
        printf("Error: All semaphores in use\n New Semaphore Function Exitted\n");
        return 1;
    }
    int id;
    int value;
    printf("Enter semaphore ID (0-4): ");
    scanf("%d", &id);
    List_first(semList);
    while (semList->pCurrentNode != NULL)
    {
        if (((SEM *)(semList->pCurrentNode->pItem))->id == id)
        {
            printf("Error: semaphore with id:%d already exists\n New Semaphore Function Exitted\n", id);
            return 1;
        }
        semList->pCurrentNode = semList->pCurrentNode->pNext;
    }
    printf("Enter initial value (0 or higher): ");
    scanf("%d", &value);
    if (semUSed >= MAX_SEMAPHORE_COUNT)
    {
        printf("Error: No free semaphore left\nNew Semaphore Function Exitted\n");
        return 1;
    }
    List_first(freeSemList);
    SEM *sem = List_remove(freeSemList);
    sem->id = id;
    sem->value = value;
    List_append(semList, sem);
    return 1;
}

int SemaphorePFn()
{
    int id;
    printf("Enter semaphore ID (0-4): ");
    scanf("%d", &id);
    Node *current = semList->pFirstNode;
    SEM *sem = NULL;
    while (current != NULL)
    {
        if (((SEM *)(current->pItem))->id == id)
        {
            sem = (SEM *)(current->pItem);
            break;
        }
    }
    if (sem == NULL)
    {
        printf("Error: semaphore with id:%d not found\nSemaphore P Function Exitted\n", id);
        return 1;
    }
    if (sem->value <= 0)
    {
        runningProcess->state = BLOCKED;
        List_prepend(sem->waitingQueue, runningProcess);
        runningProcess = initProcess;
    }
    else
    {
        sem->value--;
    }
    return 1;
}

int SemaphoreVFn()
{
    int id;
    printf("Enter semaphore ID (0-4): ");
    scanf("%d", &id);
    Node *current = semList->pFirstNode;
    SEM *sem = NULL;
    while (current != NULL)
    {
        if (((SEM *)(current->pItem))->id == id)
        {
            sem = (SEM *)(current->pItem);
            break;
        }
    }
    if (sem == NULL)
    {
        printf("Error: semaphore with id:%d not found\nSemaphore P Function Exitted\n", id);
        return 1;
    }
    List_first(sem->waitingQueue);
    PCB *unblockedProcess = List_trim(sem->waitingQueue);
    unblockedProcess->state = READY;
    if (unblockedProcess->priority == 0)
    {
        List_prepend(priority0ReadyQueue, unblockedProcess);
    }
    else if (unblockedProcess->priority == 1)
    {
        List_prepend(priority1ReadyQueue, unblockedProcess);
    }
    else if (unblockedProcess->priority == 2)
    {
        List_prepend(priority2ReadyQueue, unblockedProcess);
    }
    sem->value++;
    return 1;
}

int procinfoFn()
{
    int pid;
    printf("Enter PID of process for info: ");
    scanf("%d", &pid);
    PCB *requestedProcess = NULL;
    for (int i = 0; i < LIST_MAX_NUM_NODES; i++)
    {
        if (PCBPool[i].pid == pid)
        {
            requestedProcess = &PCBPool[i];
            break;
        }
    }
    if (requestedProcess == NULL)
    {
        printf("Error: process with id:%d not found\nProcinfo Function Exitted\n", pid);
    }
    char *stateText = "";
    switch (requestedProcess->state)
    {
    case READY:
        stateText = "READY";
        break;
    case RUNNING:
        stateText = "RUNNING";
        break;
    case BLOCKED:
        stateText = "BLOCKED";
        break;
    case UNINITIALIZED:
        stateText = "UNINITIALIZED";
        break;
    }
    printf("Process ID: %d\n Priority: %d, \nState: %s\n", requestedProcess->pid, requestedProcess->priority, stateText);
    return 1;
}

int totalinfoFn()
{
    int i;
    Node *current;
    current = blockedQueue->pFirstNode;
    i = 0;
    printf("blockedQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\nblockedQueue Count: %d \nEnd of blockedQueue\n\n", List_count(blockedQueue));

    current = priority0ReadyQueue->pFirstNode;
    i = 0;
    printf("priority0ReadyQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\npriority0ReadyQueue Count: %d \nEnd of priority0ReadyQueue\n\n", List_count(priority0ReadyQueue));

    current = priority1ReadyQueue->pFirstNode;
    i = 0;
    printf("priority1ReadyQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\npriority1ReadyQueue Count: %d \nEnd of priority1ReadyQueue\n\n", List_count(priority1ReadyQueue));

    current = priority2ReadyQueue->pFirstNode;
    i = 0;
    printf("priority2ReadyQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\npriority2ReadyQueue Count: %d \nEnd of priority2ReadyQueue\n\n", List_count(priority2ReadyQueue));

    int j = 0;
    for (; i < MAX_SEMAPHORE_COUNT; i++)
    {
        SEM *sem = &SEMPool[i];
        if (List_count(SEMPool->waitingQueue) > 0)
        {
            printf("waiting queue of Semaphore: %d\n, ", i);
            current = sem->waitingQueue->pFirstNode;
            while (current != NULL)
            {
                PCB *process = (PCB *)(current->pItem);
                if (process != NULL)
                {
                    printf("list index: %d, pid: %d\n\n", j, process->pid);
                }
                current = current->pNext;
            }
            printf("End of SEM: %d\n\n\n", i);
        }
    }
    return 1;
}

// Testing
void printLists()
{
    Node *current = NULL;
    int i = 0;

    printf("This is SRInfoPool:\n\n");
    for (int i = 0; i < LIST_MAX_NUM_NODES; i++)
    {
        SRInfo *srinfo = &SRInfoPool[i];
        printf("senderPid: %d, receiverPid: %d, msg: %s\n", srinfo->senderPid, srinfo->receiverPid, srinfo->msg);
    }
    printf("\nSRInfoPool Count: %d \nEnd of SRInfoPool\n\n", LIST_MAX_NUM_NODES);

    printf("This is freeSRInfoList:\n\n");
    current = freeSemList->pFirstNode;
    while (current != NULL)
    {
        SRInfo *srinfo = (SRInfo *)(current->pItem);
        printf("senderPid: %d, receiverPid: %d, msg: %s\n", srinfo->senderPid, srinfo->receiverPid, srinfo->msg);
        current = current->pNext;
    }
    printf("\nfreeSRInfoList Count: %d \nEnd of freeSRInfoList\n\n", List_count(freeSRInfoList));

    printf("This is SRInfoList:\n\n");
    current = SRInfoList->pFirstNode;
    while (current != NULL)
    {
        SRInfo *srinfo = (SRInfo *)(current->pItem);
        printf("senderPid: %d, receiverPid: %d, msg: %s\n", srinfo->senderPid, srinfo->receiverPid, srinfo->msg);
        current = current->pNext;
    }
    printf("\nSRInfoList Count: %d \nEnd of freeSRInfoList\n\n", List_count(SRInfoList));

    printf("This is PCBPool:\n\n");
    for (int i = 0; i < LIST_MAX_NUM_NODES; i++)
    {
        PCB *pcb = &PCBPool[i];
        char *stateText = "";
        switch (pcb->state)
        {
        case READY:
            stateText = "READY";
            break;
        case RUNNING:
            stateText = "RUNNING";
            break;
        case BLOCKED:
            stateText = "BLOCKED";
            break;
        case UNINITIALIZED:
            stateText = "UNINITIALIZED";
            break;
        }
        printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
    }
    printf("\nPCBPool Count: %d \nEnd of PCBPool\n\n", LIST_MAX_NUM_NODES);

    printf("This is freePCBList:\n\n");
    current = freePCBList->pFirstNode;
    while (current != NULL)
    {
        PCB *pcb = (PCB *)(current->pItem);
        char *stateText = "";
        switch (pcb->state)
        {
        case READY:
            stateText = "READY";
            break;
        case RUNNING:
            stateText = "RUNNING";
            break;
        case BLOCKED:
            stateText = "BLOCKED";
            break;
        case UNINITIALIZED:
            stateText = "UNINITIALIZED";
            break;
        }
        printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        current = current->pNext;
    }
    printf("\nfreePCBList Count: %d \nEnd of freePCBList\n\n", List_count(freePCBList));

    current = blockedQueue->pFirstNode;
    i = 0;
    printf("This is blockedQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\nblockedQueue Count: %d \nEnd of blockedQueue\n\n", List_count(blockedQueue));

    current = priority0ReadyQueue->pFirstNode;
    i = 0;
    printf("This is priority0ReadyQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\npriority0ReadyQueue Count: %d \nEnd of priority0ReadyQueue\n\n", List_count(priority0ReadyQueue));

    current = priority1ReadyQueue->pFirstNode;
    i = 0;
    printf("This is priority1ReadyQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\npriority1ReadyQueue Count: %d \nEnd of priority1ReadyQueue\n\n", List_count(priority1ReadyQueue));

    current = priority2ReadyQueue->pFirstNode;
    i = 0;
    printf("This is priority2ReadyQueue:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            PCB *pcb = (PCB *)current->pItem;
            char *stateText = "";
            switch (pcb->state)
            {
            case READY:
                stateText = "READY";
                break;
            case RUNNING:
                stateText = "RUNNING";
                break;
            case BLOCKED:
                stateText = "BLOCKED";
                break;
            case UNINITIALIZED:
                stateText = "UNINITIALIZED";
                break;
            }
            printf("PID: %d, priority: %d, state: %s, msg: %s\n", pcb->pid, pcb->priority, stateText, pcb->msg);
        }
        current = current->pNext;
        i++;
    }
    printf("\npriority2ReadyQueue Count: %d \nEnd of priority2ReadyQueue\n\n", List_count(priority2ReadyQueue));

    printf("Running Process:\n");
    if (runningProcess != NULL)
    {
        PCB *pcb = runningProcess;
        char *stateText = "";
        switch (pcb->state)
        {
        case READY:
            stateText = "READY";
            break;
        case RUNNING:
            stateText = "RUNNING";
            break;
        case BLOCKED:
            stateText = "BLOCKED";
            break;
        case UNINITIALIZED:
            stateText = "UNINITIALIZED";
            break;
        }
        printf("PID: %d, priority: %d, state: %s, msg: %s\n\n", pcb->pid, pcb->priority, stateText, pcb->msg);
    }
    else
    {
        printf("No process is running!\n\n");
    }
    // print semaphores
    i = 0;
    int j = 0;
    printf("This is SEMPool:\n\n");
    for (; i < MAX_SEMAPHORE_COUNT; i++)
    {
        printf("list index: %d, ", i);
        SEM *sem = &SEMPool[i];
        printf("ID: %d,value: %d\n", sem->id, sem->value);
        printf("Semaphore ID:%d waiting Queue\n", sem->id);
        current = sem->waitingQueue->pFirstNode;
        while (current != NULL)
        {
            PCB *process = (PCB *)(current->pItem);
            if (process != NULL)
            {
                printf("list index: %d, pid: %d\n\n", j, process->pid);
            }
            current = current->pNext;
        }
        printf("\n");
    }
    printf("\nfreeSemList Count: %d \nEnd of SEMPool\n\n", MAX_SEMAPHORE_COUNT);

    current = freeSemList->pFirstNode;
    i = 0;
    printf("This is freeSemList:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            SEM *sem = (SEM *)current->pItem;
            printf("ID: %d,value: %d\n", sem->id, sem->value);
        }
        current = current->pNext;
        i++;
    }
    printf("\nfreeSemList Count: %d \nEnd of freeSemList\n\n", List_count(freeSemList));

    current = semList->pFirstNode;
    i = 0;
    printf("This is semList:\n\n");
    while (current != NULL)
    {
        printf("list index: %d, ", i);
        if (current->pItem != NULL)
        {
            SEM *sem = (SEM *)current->pItem;
            printf("ID: %d,value: %d\n", sem->id, sem->value);
        }
        current = current->pNext;
    }
    printf("\nsemList Count: %d \nEnd of semList\n\n", List_count(semList));
}

int main(int argc, char *argv[])
{
    initFn();
    char choice;
    printMainMenu();
    while (1)
    {
        printf("Please enter the desired command's character: ");
        scanf(" %c", &choice);
        checkerFn();
        switch (choice)
        {
        case 'C':
            if (createFn() >= -1)
            {
                break;
            }
        case 'F':
            if (forkFn() >= -1)
            {
                break;
            }
        case 'K':
            if (killFn() == 1)
            {
                break;
            }
        case 'E':
        {
            int returnStatus = exitFn();
            if (returnStatus == 1)
            {
                break;
            }
            else if (returnStatus == 2)
            {
                printf("Program Exitted, thank you\n");
                return 0;
            }
            break;
        }
        case 'Q':
            if (quantumFn() == 1)
            {
                break;
            }
        case 'S':
            if (sendFn() == 1)
            {
                break;
            }
        case 'R':
            if (receiveFn() == 1)
            {
                break;
            }
        case 'Y':
            if (replyFn() == 1)
            {
                break;
            }
        case 'N':
            if (newSemaphoreFn() == 1)
            {
                break;
            }
        case 'P':
            if (SemaphorePFn() == 1)
            {
                break;
            }
        case 'V':
            if (SemaphoreVFn() == 1)
            {
                break;
            }
        case 'I':
            if (procinfoFn() == 1)
            {
                break;
            }
        case 'T':
            if (totalinfoFn() == 1)
            {
                break;
            }
        case 'X':
            printf("Program Exitted, thank you\n");
            return 0;
        case '1': // Testing
            printLists();
            break;
        default:
            printf("Error: Invalid character chosen, please try again\n");
        }
    }

    return 0;
}
