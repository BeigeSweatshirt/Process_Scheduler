/* file handling: https://www.programiz.com/c-programming/c-file-input-output
 * str to dec conversion: https://www.tutorialspoint.com/c_standard_library/c_function_strtol.htm
 * counting # of bytes in file: https://stackoverflow.com/questions/25613766/how-to-count-number-of-bytes-in-a-file-using-c
 * limiting to two trailing zeroes https://stackoverflow.com/questions/277772/avoid-trailing-zeroes-in-printf
 * quicksort: https://www.programiz.com/dsa/quick-sort
 * rounding: https://stackoverflow.com/questions/13483430/how-to-make-rounded-percentages-add-up-to-100
*/ 
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>

const int QUANTUM = 2;
const int EXEC_DELAY = 3;

typedef struct  __attribute__((packed)){
   char           priority;
   char           name[32];
   int            id;
   char           status;
   int            burst;
   int            breg;
   long           lreg;
   char           ptype;
   int            numfiles;
} PCB;

typedef struct {
   int            sched;
   int            remaining_procs;
   int            total_procs;
   PCB*           pcb;
} CPU;

int num_queues;
CPU* cpulist;
pthread_mutex_t lock;

void* cpu(void *p_cpu_data);
void* age_thread(void *arg);
void* load_balancing(void *arg);
int get_num_procs(FILE *fp, char* file);
PCB *get_procs_from_file(FILE *fp, char* file, int num_procs);
int* get_scheds(int num_scheds, char* input[]);
float* get_weights(int num_scheds, char* input[]);
int* get_round_procs(int num_procs, int num_scheds, float* weights);
CPU *init_CPUs(PCB *pcblist, int num_scheds, int* scheds, int* num_procs);
int compare_burst (const void * a, const void * b);
int compare_priority (const void * a, const void * b);
void print_proc_stats(PCB *pcblist, int num_procs);
int max (int a, int b);
int min (int a, int b);
bool all_CPUs_complete(void);
int get_busiest_CPU(int x);
void print_load_balance_stats(int s_cpu, int t_cpu);



int main(int argc, char * argv[]) {
   FILE *fp;

   // read file
   if ((fp = fopen(argv[1], "rb")) == NULL) {
      perror("[ERROR]:");
      exit(EXIT_FAILURE);
   }
   int num_procs = get_num_procs(fp, argv[1]);
   PCB *pcblist = get_procs_from_file(fp, argv[1], num_procs);
   fclose(fp);
   
   // parse command line
   num_queues = (argc-2) / 2;
   if ((argc % 2) != 0) {
      printf("[ERROR]: Invalid number of arguments. There should be an equal number of scheduling codes and weights.\n");
      exit(EXIT_FAILURE);
   }
   int* sched_algos = get_scheds(num_queues, argv);
   float* weights = get_weights(num_queues, argv);
   int* weighted_procs = get_round_procs(num_procs, num_queues, weights);
   
   // init CPUs 
   cpulist = init_CPUs(pcblist, num_queues, sched_algos, weighted_procs);

   free(weights);
   free(sched_algos);
   free(weighted_procs);

   for (int i = 0; i < num_queues; i++) {
      printf("====================");
      printf("[CPU %d]", i);
      printf("====================\n");
      print_proc_stats(cpulist[i].pcb, cpulist[i].total_procs);
      printf("\n\n");
   }
   
   // init CPU threads
   pthread_t t[num_queues];
   if (pthread_mutex_init(&lock, NULL) != 0) {
      printf("\n mutex init has failed\n");
      return 1;
   }
   for (int i = 0; i < num_queues; i++) {
      int *arg = malloc(sizeof(*arg));
      *arg = i;
      if (pthread_create(&t[i], NULL, &cpu, arg)) {
         perror("Failed to create thread");
         exit(EXIT_FAILURE);
      }
   }

   // Cleanup
   for (int i = 0; i < num_queues; i++) {
      pthread_join(t[i], NULL);
      // printf("Thread %d completed execution", i);
   }

   for (int i = 0; i < num_queues; i++) {
      printf("====================");
      printf("[CPU %d]", i);
      printf("====================\n");
      print_proc_stats(cpulist[i].pcb, cpulist[i].total_procs);
      printf("\n\n");
   }
   free(pcblist);
   for (int i = 0; i < num_queues; i++) {
      free(cpulist[i].pcb);
   }
   free(cpulist);
   return 0;
}

void* cpu(void *arg) {
   int x = *((int*)arg);
   free(arg);

   int *agarg = malloc(sizeof(*agarg));
   int *lbarg = malloc(sizeof(*lbarg));
   lbarg = &x;
   agarg = &x;

   while(!all_CPUs_complete()) {
      if (cpulist[x].sched == 3) { //SJF only
         qsort(cpulist[x].pcb, cpulist[x].total_procs, sizeof(PCB), compare_burst);
      } else if (cpulist[x].sched == 4) { //Priority only
         qsort(cpulist[x].pcb, cpulist[x].total_procs, sizeof(PCB), compare_priority);

         pthread_t at;
         pthread_create(&at, NULL, &age_thread, agarg);
      }
      if (cpulist[x].sched != 2) { //FCFS, SJF and Priority
         for (int i = 0; i < cpulist[x].total_procs; i++) {
            while (cpulist[x].pcb[i].burst >= 0) {
               usleep(EXEC_DELAY*1000);
               cpulist[x].pcb[i].burst -= QUANTUM;
            }
            cpulist[x].pcb[i].burst = 0;
            cpulist[x].pcb[i].status = 0;
            cpulist[x].remaining_procs--;
         }
      } else { //RR Only
         int i = 0;
         while (cpulist[x].remaining_procs > 0) {
            if (cpulist[x].pcb[i].burst > 0) {
               usleep(EXEC_DELAY*1000);
               cpulist[x].pcb[i].burst = max(0, cpulist[x].pcb[i].burst-QUANTUM);
               if (cpulist[x].pcb[i].burst == 0) {
                  cpulist[x].pcb[i].status = 0;
                  cpulist[x].remaining_procs--;
               }
            }
            i = (i+1) % cpulist[x].total_procs;
         }
      }
      pthread_t lb;
      pthread_create(&lb, NULL, &load_balancing, lbarg);
      pthread_join(lb, NULL);
   }
   
   return NULL;
}

void* age_thread(void *arg) {
   int x = *((int*)arg);
   // free(arg);

   char min_val;
   int min_burst = 2147483647;
   int lowest_index;

   
   while(min_burst > 0) {
      print_proc_stats(cpulist[x].pcb, cpulist[x].total_procs);
      sleep(2);
      min_val = 127;
      for (int i = 0; i < cpulist[x].total_procs; i++) {
         min_burst = min(min_burst, cpulist[x].pcb[i].burst);
         if(min_val > cpulist[x].pcb[i].priority) {
            min_val = cpulist[x].pcb[i].priority;
            lowest_index = i;
         }
         print_proc_stats(cpulist[x].pcb, cpulist[x].total_procs);
      }
      printf("Incrementing highest priority process (%s) from %d to prevent starvation\n", cpulist[x].pcb[lowest_index].name, min_val);
      // pthread_mutex_lock(&lock);
      cpulist[x].pcb[lowest_index].priority++;
      // pthread_mutex_unlock(&lock);
   }
   return NULL;
}

void* load_balancing(void *arg) {
   int t_cpu = *((int*)arg); //target CPU
   int s_cpu = get_busiest_CPU(t_cpu); //Source CPU
   if (s_cpu == -1 || cpulist[s_cpu].remaining_procs == 1) return NULL; //All other CPUs finished

   int s_half = cpulist[s_cpu].total_procs - (cpulist[s_cpu].remaining_procs / 2);
   int s_to_copy = cpulist[s_cpu].total_procs - s_half;
   int amount = min(cpulist[t_cpu].total_procs, s_to_copy);
   printf("CPU %d is empty\n", t_cpu);
   printf("Moving %d tasks from the busiest CPU (CPU %d)\n", amount, s_cpu);

   printf("Before\n");
   print_load_balance_stats(s_cpu, t_cpu);
   
   

   int j = 0;
   for (int i = 0; i < amount; i++) {
      if (cpulist[t_cpu].pcb[i].burst == 0) {
         while (j < cpulist[s_cpu].total_procs && cpulist[s_cpu].pcb[j].burst == 0) j++;
         cpulist[t_cpu].pcb[i] = cpulist[s_cpu].pcb[j];
         cpulist[t_cpu].remaining_procs++;
         cpulist[s_cpu].pcb[j].burst = 0;
         cpulist[s_cpu].pcb[j].status = 0;
         cpulist[s_cpu].remaining_procs--;
      }
   }
   printf("After\n");
   print_load_balance_stats(s_cpu, t_cpu);
   return NULL;
}

int get_num_procs(FILE *fp, char* file) {
   int file_size;
   printf("\n");
   for(file_size = 0; getc(fp) != EOF; ++file_size);
   printf("%s size:\t\t\t\t%d bytes\n", file, file_size);
   int num_procs = (int) file_size / sizeof(PCB);
   printf("Total Processes:\t\t\t%i\n\n", num_procs);
   rewind(fp);
   return num_procs;
}

PCB *get_procs_from_file(FILE *fp, char* file, int num_procs) {
   printf("Moving data from %s into memory...", file);
   PCB *pcblist = malloc(sizeof(PCB) * num_procs);

   for (int i = 0; i < num_procs; i++) {
      fread(&pcblist[i].priority, sizeof(pcblist->priority), 1, fp);
      fread(&pcblist[i].name, sizeof(pcblist->name), 1, fp);
      fread(&pcblist[i].id, sizeof(pcblist->id), 1, fp);
      fread(&pcblist[i].status, sizeof(pcblist->status), 1, fp);
      fread(&pcblist[i].burst, sizeof(pcblist->burst), 1, fp);
      fread(&pcblist[i].breg, sizeof(pcblist->breg), 1, fp);
      fread(&pcblist[i].lreg, sizeof(pcblist->lreg), 1, fp);
      fread(&pcblist[i].ptype, sizeof(pcblist->ptype), 1, fp);
      fread(&pcblist[i].numfiles, sizeof(pcblist->numfiles), 1, fp);
   }
   printf("DONE\n\n");
   return pcblist;
}

int* get_scheds(int num_scheds, char* input[]) {
   int* scheds = malloc(num_scheds * sizeof(*scheds));
      for (int i = 2; i <= num_scheds*2; i=i+2) {
      int sched_algo = (int) strtod(input[i], NULL);
      if ((sched_algo < 1) || (sched_algo > 4)) {
         printf("[ERROR]: Argument %d is invalid. Must be an integer between 1-4 where:"
         "\n1 = First Come First Serve"
         "\n2 = Round Robin"
         "\n3 = Shortest Job First"
         "\n4 = Priority Scheduling\n", i);
         exit(EXIT_FAILURE);
      }
      scheds[(i/2)-1] = sched_algo;
   }
   return scheds;
}

float* get_weights (int num_scheds, char* input[]) {
   float* weights = malloc(num_scheds * sizeof(*weights));
   float weight_sum = 0.0;
   for (int i = 3; i <= (num_scheds*2)+1; i=i+2) {
      float weight = strtof(input[i], NULL);
      if ((weight <= 0) || (weight > 1)) {
         printf("[ERROR]: Argument %d is invalid. Must be rational number greater than 0 and less than or equal to 1\n", i);
         exit(EXIT_FAILURE);
      }
      weight_sum += weight;
      if (weight_sum > 1) {
         printf("[ERROR]: Sum of scheduling algorithm weights exceeds 1.00\n");
         exit(EXIT_FAILURE);
      }
      weights[((i-1)/2)-1] = weight;
   }
   return weights;
}

int* get_round_procs(int total_procs, int num_cpus, float* weights) {
   float* procs_unrounded = malloc(num_cpus * sizeof(float));
   for (int i = 0; i < num_cpus; i++) {
      procs_unrounded[i] = total_procs * weights[i];
   }

   float curr_value = 0;
   float cumul_value = 0;
   int cumul_rounded = 0;
   int last_cumul_rounded = 0;
   int rounded_value = 0;
   int* procs_rounded = malloc(num_cpus * sizeof(int));
   for (int i = 0; i < num_cpus; i++) {
      curr_value = procs_unrounded[i];
      cumul_value += curr_value;
      cumul_rounded = roundf(cumul_value);
      rounded_value = cumul_rounded - last_cumul_rounded;
      procs_rounded[i] = rounded_value;
      last_cumul_rounded = cumul_rounded;
   }
   for (int i = 0; i < num_cpus; i++) {
      printf("%d\n", procs_rounded[i]);
   }
   free(procs_unrounded);
   return procs_rounded;
}

CPU *init_CPUs(PCB *pcblist, int num_scheds, int* scheds, int* num_procs) {
   printf("Delegating processes to processors:\n");
   printf("Processor\tScheduling Algorithm\t\tProcesses\n");
   
   CPU* cpulist = malloc(num_scheds * sizeof(*cpulist));
   int currpcb = 0;
   for (int i = 0; i < num_scheds; i++) {
      printf("%d\t\t", i);

      cpulist[i].sched = scheds[i];
      char * sched_name;
      if (scheds[i] == 1) sched_name = "FCFS";
      else if (scheds[i] == 2) sched_name = "RR";
      else if (scheds[i] == 3) sched_name = "SJF";
      else sched_name = "PRI";
      printf("%s\t\t\t\t", sched_name);

      cpulist[i].remaining_procs = num_procs[i];
      cpulist[i].total_procs = num_procs[i];
      printf("%d\n", cpulist[i].total_procs);
      cpulist[i].pcb = malloc(num_procs[i] * sizeof(PCB));
      for (int j = 0; j < num_procs[i]; j++) {
         cpulist[i].pcb[j] = pcblist[currpcb];
         currpcb++;
      }
   }
   printf("\n\n");
   return cpulist;
}

int compare_burst (const void * a, const void * b) {
   PCB pcb1 = *(PCB *)a, pcb2 = *(PCB *)b;
   if (pcb1.burst < pcb2.burst) return -1;
   if (pcb1.burst == pcb2.burst) return 0;
   else return 1;
}

int compare_priority(const void * a, const void * b) {
   PCB pcb1 = *(PCB *)a, pcb2 = *(PCB *)b;
   if (pcb1.priority < pcb2.priority) return -1;
   if (pcb1.priority == pcb2.priority) return 0;
   else return 1;
}

void print_proc_stats(PCB *pcblist, int num_procs) {
   printf("Priority | Name | ID | Status | CPU Burst Time | Base Register | Limit Register | Process Type | Files\n");

   for (int i = 0; i < num_procs; i++) {
      char priority = pcblist[i].priority;
      char * name = pcblist[i].name;
      int id = pcblist[i].id;
      char status = pcblist[i].status;
      int burst = pcblist[i].burst;
      int breg = pcblist[i].breg;
      long lreg = pcblist[i].lreg;
      char ptype = pcblist[i].ptype;
      int numfiles = pcblist[i].numfiles;
      
      
      printf("%-3d %-11s %-3d %-1d %-3d %-4d %-5lu %-1d %-3d\n",
      priority, name, id, status, burst, breg, lreg, ptype, numfiles);
      
   }
   printf("\n");
}

int max (int a, int b) {
   return (a < b) ? b : a;
}

int min (int a, int b) {
   return (a > b) ? b : a;
}

bool all_CPUs_complete(void) {
   for (int i = 0; i < num_queues; i++) {
      if (cpulist[i].remaining_procs > 0) return false;
   }
   return true;
}

int get_busiest_CPU(int x) {
   
   int max_value = 0;
   int busiest_cpu = -1;

   for (int i = 0; i < num_queues; i++) {
      if (cpulist[i].remaining_procs > max_value && i != x) {
         max_value = cpulist[i].remaining_procs;
         busiest_cpu = i;
      }
   }
   return busiest_cpu;
}

void print_load_balance_stats(int s_CPU, int t_cpu) {
   printf("\tFrom\n");
   printf("====================");
   printf("[CPU %d]", s_CPU);
   printf("====================\n");
   print_proc_stats(cpulist[s_CPU].pcb, cpulist[s_CPU].total_procs);
   printf("\n\n");

   printf("\n\tTo\n");
   printf("====================");
   printf("[CPU %d]", t_cpu);
   printf("====================\n");
   print_proc_stats(cpulist[t_cpu].pcb, cpulist[t_cpu].total_procs);
   printf("\n\n");
}

int* rr_offset(int amount, int cpu) {
   int *offset = malloc(sizeof(*offset) * amount);
   int total_found = 0;
   for (int i = 0; i < cpulist[cpu].total_procs && total_found < amount; i = (i+1) % cpulist[cpu].total_procs) {
      if (cpulist[cpu].pcb[i].burst > 0) {
         offset[total_found] = i;
         total_found++;
      }
   }
   if (total_found == 0) {
      for (int i = 0; i < amount; i++) offset[i] = 0;
   }
   return offset;
}