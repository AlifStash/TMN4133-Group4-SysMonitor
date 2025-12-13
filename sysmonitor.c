/*
 * System Monitor - Command-line system monitoring tool
 * Uses Linux system calls and /proc filesystem
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>

// Structure to hold process information
typedef struct {
    int pid;
    char name[256];
    unsigned long long utime;
    unsigned long long stime;
    unsigned long long total_time;
} ProcessInfo;

// Function prototypes
void display_menu();
void cpu_usage();
void memory_usage();
void top_processes();
void continuous_monitoring();
void clear_screen();
int is_numeric(const char *str);
int read_process_info(int pid, ProcessInfo *proc);
int compare_processes(const void *a, const void *b);

int main() {
    int choice;
    int running = 1;

    while (running) {
        display_menu();
        printf("Enter your choice: ");
        
        if (scanf("%d", &choice) != 1) {
            // Clear invalid input
            while (getchar() != '\n');
            printf("\nInvalid input. Please enter a number.\n");
            sleep(2);
            continue;
        }

        // Clear input buffer
        while (getchar() != '\n');

        switch (choice) {
            case 1:
                cpu_usage();
                break;
            case 2:
                memory_usage();
                break;
            case 3:
                top_processes();
                break;
            case 4:
                continuous_monitoring();
                break;
            case 5:
                printf("\nExiting System Monitor. Goodbye!\n");
                running = 0;
                break;
            default:
                printf("\nInvalid choice. Please select 1-5.\n");
                sleep(2);
        }
    }

    return 0;
}

/*
 * Display the main menu
 */
void display_menu() {
    clear_screen();
    printf("=====================================\n");
    printf("    SYSTEM MONITOR - MAIN MENU\n");
    printf("=====================================\n");
    printf("1. CPU Usage\n");
    printf("2. Memory Usage\n");
    printf("3. Top 5 Processes\n");
    printf("4. Continuous Monitoring\n");
    printf("5. Exit\n");
    printf("=====================================\n");
}

/*
 * Display CPU usage statistics
 */
void cpu_usage() {
    clear_screen();
    printf("=== CPU Usage ===\n");
    printf("\n[Function not yet implemented]\n");
    printf("\nPress Enter to return to menu...");
    getchar();
}

/*
 * Display memory usage statistics
 */
void memory_usage() {
    clear_screen();
    printf("=== Memory Usage ===\n");
    printf("\n[Function not yet implemented]\n");
    printf("\nPress Enter to return to menu...");
    getchar();
}

/*
 * Display top 5 processes by CPU/memory usage
 */
void top_processes() {
    clear_screen();
    printf("=== Top 5 Processes ===\n\n");

    DIR *proc_dir;
    struct dirent *entry;
    ProcessInfo *processes = NULL;
    int proc_count = 0;
    int capacity = 100;

    // Allocate initial array for processes
    processes = (ProcessInfo *)malloc(capacity * sizeof(ProcessInfo));
    if (!processes) {
        printf("Error: Memory allocation failed\n");
        printf("\nPress Enter to return to menu...");
        getchar();
        return;
    }

    // Open /proc directory
    proc_dir = opendir("/proc");
    if (!proc_dir) {
        printf("Error: Cannot open /proc directory\n");
        printf("\nPress Enter to return to menu...");
        free(processes);
        getchar();
        return;
    }

    // Read all process directories
    while ((entry = readdir(proc_dir)) != NULL) {
        // Check if directory name is numeric (PID)
        if (!is_numeric(entry->d_name)) {
            continue;
        }

        int pid = atoi(entry->d_name);
        
        // Expand array if needed
        if (proc_count >= capacity) {
            capacity *= 2;
            ProcessInfo *temp = (ProcessInfo *)realloc(processes, capacity * sizeof(ProcessInfo));
            if (!temp) {
                printf("Error: Memory reallocation failed\n");
                break;
            }
            processes = temp;
        }

        // Read process information
        if (read_process_info(pid, &processes[proc_count])) {
            proc_count++;
        }
    }

    closedir(proc_dir);

    if (proc_count == 0) {
        printf("No processes found\n");
        free(processes);
        printf("\nPress Enter to return to menu...");
        getchar();
        return;
    }

    // Sort processes by total CPU time (descending)
    qsort(processes, proc_count, sizeof(ProcessInfo), compare_processes);

    // Display top 5 processes
    printf("%-8s %-20s %-15s %-15s %-15s\n", 
           "PID", "Process Name", "User Time", "System Time", "Total Time");
    printf("--------------------------------------------------------------------------------\n");

    int display_count = (proc_count < 5) ? proc_count : 5;
    for (int i = 0; i < display_count; i++) {
        printf("%-8d %-20s %-15llu %-15llu %-15llu\n",
               processes[i].pid,
               processes[i].name,
               processes[i].utime,
               processes[i].stime,
               processes[i].total_time);
    }

    printf("\nNote: Times are in clock ticks (divide by sysconf(_SC_CLK_TCK) for seconds)\n");
    free(processes);
    printf("\nPress Enter to return to menu...");
    getchar();
}

/*
 * Continuously monitor system statistics
 */
void continuous_monitoring() {
    clear_screen();
    printf("=== Continuous Monitoring ===\n");
    printf("\n[Function not yet implemented]\n");
    printf("\nPress Enter to return to menu...");
    getchar();
}

/*
 * Clear the terminal screen
 */
void clear_screen() {
    #ifdef _WIN32
        system("cls");
    #else
        system("clear");
    #endif
}

/*
 * Check if a string is numeric
 */
int is_numeric(const char *str) {
    if (*str == '\0') {
        return 0;
    }
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

/*
 * Read process information from /proc/[PID]/stat and /proc/[PID]/comm
 */
int read_process_info(int pid, ProcessInfo *proc) {
    char stat_path[256];
    char comm_path[256];
    FILE *fp;

    // Initialize process structure
    proc->pid = pid;
    proc->utime = 0;
    proc->stime = 0;
    proc->total_time = 0;
    strcpy(proc->name, "unknown");

    // Read process name from /proc/[PID]/comm
    snprintf(comm_path, sizeof(comm_path), "/proc/%d/comm", pid);
    fp = fopen(comm_path, "r");
    if (fp) {
        if (fgets(proc->name, sizeof(proc->name), fp)) {
            // Remove trailing newline
            size_t len = strlen(proc->name);
            if (len > 0 && proc->name[len-1] == '\n') {
                proc->name[len-1] = '\0';
            }
        }
        fclose(fp);
    }

    // Read CPU time from /proc/[PID]/stat
    snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);
    fp = fopen(stat_path, "r");
    if (!fp) {
        return 0;
    }

    // Parse /proc/[PID]/stat
    // Fields: pid, comm, state, ppid, pgrp, session, tty_nr, tpgid, flags,
    //         minflt, cminflt, majflt, cmajflt, utime(14), stime(15)...
    int fields_read = fscanf(fp, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu",
                             &proc->utime, &proc->stime);
    
    fclose(fp);

    if (fields_read != 2) {
        return 0;
    }

    proc->total_time = proc->utime + proc->stime;
    return 1;
}

/*
 * Comparison function for sorting processes by total CPU time
 */
int compare_processes(const void *a, const void *b) {
    ProcessInfo *proc_a = (ProcessInfo *)a;
    ProcessInfo *proc_b = (ProcessInfo *)b;
    
    // Sort in descending order (highest CPU time first)
    if (proc_b->total_time > proc_a->total_time) return 1;
    if (proc_b->total_time < proc_a->total_time) return -1;
    return 0;
}
