#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"
#include "proc.h"
#include "spinlock.h"
#include "traps.h"
#include "x86.h"

struct node* getnode() 
{
    int i=0;
    while(i<NPROC)
    {
        if(avail_alloc_nodes[i].available != 0) 
        {
            i++;
            continue;
        }
        else
        {
            avail_alloc_nodes[i].available=1;
            return &avail_alloc_nodes[i];
        }
    }
    return 0;
}

int countnodes(struct node* head) 
{
    int i=0;
    while(head!=0)
    {
        head = head->next;
        i++;
    }
    return i;
}
struct node* push(struct node* head, struct proc* pc) 
{
    struct node* new = getnode();
    new->prc = pc, new->next = 0;
    if(head!=0)
    {
        struct node* cur = head;
        while(cur->next != 0) { cur=cur->next; }
        cur->next = new;
        return head;
    }
        return new;
}

struct node* pop(struct node* head) 
{
    if(head!=0)
    {
        struct node* temp = head->next;
        head->available=0;
        return temp;
    }
    else
    {
        return 0;
    }
    
}

// shifts all nodes with node->p->age > threshold from FROM list to TO list
// returns the number of nodes shifted and updates the head pointers
void proc_queue_change(int limit, struct node** from, struct node** target) 
{   
    if (*from == 0) { return ; }
    if (from==0 || target==0) { return ; }
    struct node* cur = *from;
    struct node* prev = 0;

    int count = 0;

    while(cur!=0) 
    {
        if(cur->prc->wtime <= limit)
        {
            break;
        }
        else //CHANGE QUEUE PROMOTE
        {
            cur->prc->cur_q--;
            // cprintf("PLOT %d %d %d\n",cur->prc->pid,cur->prc->cur_q,ticks);  
            // cprintf("process pid %d moved from q%d to q%d with age %d wtime %d timeslices %d\n",cur->prc->pid,cur->prc->cur_q+1,cur->prc->cur_q,ticks-cur->prc->age,cur->prc->wtime,cur->prc->cur_timeslices);
            cur->prc->age = ticks;
            count++;
            cur->prc->wtime=0;  
            prev = cur;
            cur = cur->next;
        } 
    }
    if (prev == 0) {
        return ;
    }

    if(*target == 0) 
    {
        prev->next = 0;
        *target = *from;
        *from = cur;
    } 
    else 
    {
        struct node* nextone = *target;
        while (nextone->next != 0) 
        {
            nextone = nextone->next;
        }
        prev->next = 0;
        nextone->next = *from;
        *from = cur;
    }
    return;
}
