#include "types.h"
#include "user.h"

int number_of_processes = 10;

int main(int argc, char *argv[])
{
  int j;
  for (j = 0; j < number_of_processes; j++)
  {
    int pid = fork();
    if (pid < 0)
    {
      printf(1, "Fork failed\n");
      continue;
    }
    if (pid == 0)
    {
      volatile int i;
      for (volatile int k = 0; k < number_of_processes; k++)
      {
        if (k <= j)
        {
          sleep(200); //io time
        }
        else
        {
          for (i = 0; i < 100000000; i++)
          {
            ; //cpu time
          }
        }
      }
      printf(1, "Process: %d Finished\n", j);
      ps();
      exit();
    }
    else if(pid>0)
    {
        // ;
      set_priority(100-(20+j),pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
    }
  }
    for (j = 0; j < number_of_processes+5; j++)
    {
        wait();
    }
  exit();
}
// #include "types.h"
// #include "user.h"

// int number_of_processes = 10;

// int main(int argc, char *argv[]) {
//     //int parent_pid = getpid();
//     for (int pNo = 0; pNo < number_of_processes; pNo++) {
//         int pid = fork();
//         if (pid < 0) {
//             printf(1, "Fork failed\n");
//             continue;
//         }
//         if (pid == 0) {
//             int total = 0;
//             if (pNo % 4 == 0) {
//                 // CPU
//                 for (int i = 0; i < (int)1e9; i++) {
//                     total += i;
//                 }
//             }
//             if (pNo % 4 == 1) {
//                 // IO
//                 for(int i = 0; i < 10; i++){
//                     sleep(70);
//                     total += i;
//                 }
//             }
//             if (pNo % 4 == 2) {
//                 // IO then CPU
//                 sleep(500);
//                 for(int i = 0; i < 5; i++){
//                     total += i;
//                     for(int j = 0; j < (int)1e8; j++){
//                         total += j;
//                     }
//                 }
//             }
//             if (pNo % 4 == 3) {
//                 // CPU then IO
//                 for(int i = 0; i < 5; i++){
//                     total += i;
//                     for(int j = 0; j < (int)1e8; j++){
//                         total += j;
//                     }
//                 }
//                 sleep(500);
//             }
//             printf(1, "Benchmark: %d Exited, Category : %d, Total : %d\n", pNo, pNo % 4, total);
//             // ps();
//             exit();
//         } else {
//             set_priority(100 - (20 + pNo) % 2,
//                          pid); // will only matter for PBS, comment it out if not implemented yet (better priorty for more IO intensive jobs)
//         }
//     }

//     for (int j = 0; j < number_of_processes + 5; j++) {
//         wait();
//     }
//     exit();
// }