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
#include <signal.h>
#include <time.h>
#include <errno.h>

// Structure to hold process information
typedef struct {
    int pid;
    char name[256];
    unsigned long long utime;
    unsigned long long stime;
    unsigned long long total_time;
} ProcessInfo;

// Global log file pointer
FILE *log_file = NULL;

// Function prototypes
void display_menu();
void cpu_usage();
void memory_usage();
void top_processes();
void continuous_monitoring();
void continuous_monitoring_with_interval(int interval);
void clear_screen();
int is_numeric(const char *str);
int read_process_info(int pid, ProcessInfo *proc);
int compare_processes(const void *a, const void *b);
void init_log();
void write_log(const char *mode, const char *details);
void close_log();
void signal_handler(int signum);
char* get_timestamp();
void display_help();
int parse_arguments(int argc, char *argv[]);

int main(int argc, char *argv[]) {
    int choice;
    int running = 1;

    // Initialize logging
    init_log();
    
    // Register signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, signal_handler);
    
    // Log program start
    write_log("SYSTEM", "System Monitor started");

    // Check for command-line arguments (non-interactive mode)
    if (argc > 1) {
        int result = parse_arguments(argc, argv);
        close_log();
        return result;
    }

    // Interactive mode - display menu
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
                write_log("SYSTEM", "User exited normally");
                running = 0;
                break;
            default:
                printf("\nInvalid choice. Please select 1-5.\n");
                sleep(2);
        }
    }

    close_log();
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
typedef struct {
    unsigned long long user, nice, system, idle, iowait, irq, softirq, steal;
    unsigned long long total, active; 
} CPUStats;

int get_cpu_stats(CPUStats *stats) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return -1;

    // Read all fields including steal
    int fields = fscanf(fp, "%*s %llu %llu %llu %llu %llu %llu %llu %llu",
                        &stats->user, &stats->nice, &stats->system, &stats->idle,
                        &stats->iowait, &stats->irq, &stats->softirq, &stats->steal);
    fclose(fp);

    if (fields < 8) return -1;


    stats->active = stats->user + stats->nice + stats->system + 
                    stats->irq + stats->softirq;
    
    // Total includes everything.
    stats->total = stats->active + stats->idle + stats->iowait + stats->steal;

    return 0;
}

void cpu_usage() {
    CPUStats prev, curr;
    
    clear_screen();
    printf("=== CPU Usage Monitor ===\n");
    printf("Sampling CPU... (1 second)\n");

    if (get_cpu_stats(&prev) != 0) return;
    sleep(1);
    if (get_cpu_stats(&curr) != 0) return;

    unsigned long long total_delta = curr.total - prev.total;
    unsigned long long active_delta = curr.active - prev.active;
    unsigned long long idle_delta = curr.idle - prev.idle;
    unsigned long long iowait_delta = curr.iowait - prev.iowait;
    unsigned long long steal_delta = curr.steal - prev.steal; // Calculate steal separately

    if (total_delta == 0) total_delta = 1;

    double usage_percent = (double)active_delta / total_delta * 100.0;
    double idle_percent = (double)idle_delta / total_delta * 100.0;
    double iowait_percent = (double)iowait_delta / total_delta * 100.0;
    double steal_percent = (double)steal_delta / total_delta * 100.0;

    printf("\n--------------------------------\n");
    printf("Real-time CPU Usage:\n");
    printf("%-20s: %.2f%%\n", "Active Usage", usage_percent);
    printf("%-20s: %.2f%%\n", "Idle", idle_percent);
    printf("%-20s: %.2f%%\n", "I/O Wait", iowait_percent);
    
    //Only show steal if it's significant
    if (steal_percent > 0.1) {
        printf("%-20s: %.2f%% (Waiting for Host)\n", "Steal Time", steal_percent);
    }
    printf("--------------------------------\n");

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
    write_log("MENU", "Memory Usage viewed");
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
        perror("Error: Memory allocation failed");
        write_log("ERROR", "Memory allocation failed for process array");
        printf("\nPress Enter to return to menu...");
        getchar();
        return;
    }

    // Open /proc directory
    proc_dir = opendir("/proc");
    if (!proc_dir) {
        perror("Error: Cannot open /proc directory");
        write_log("ERROR", "Failed to open /proc directory");
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
    
    // Log the activity
    char log_msg[256];
    snprintf(log_msg, sizeof(log_msg), "Top 5 Processes viewed (%d processes found)", proc_count);
    write_log("MENU", log_msg);
    
    free(processes);
    printf("\nPress Enter to return to menu...");
    getchar();
}

/*
 * Continuously monitor system statistics (interactive mode)
 */
void continuous_monitoring() {
    clear_screen();
    printf("=== Continuous Monitoring ===\n");
    printf("\n[Function not yet implemented]\n");
    printf("\nFor command-line mode, use: ./sysmonitor -c <interval>\n");
    write_log("MENU", "Continuous Monitoring viewed");
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

/*
 * Get current timestamp as a formatted string
 */
char* get_timestamp() {
    static char timestamp[64];
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", t);
    return timestamp;
}

/*
 * Initialize logging system - create or open syslog.txt
 */
void init_log() {
    log_file = fopen("syslog.txt", "a");
    if (!log_file) {
        perror("Warning: Could not open log file");
        fprintf(stderr, "Logging will be disabled.\n");
    }
}

/*
 * Write an entry to the log file with timestamp
 */
void write_log(const char *mode, const char *details) {
    if (!log_file) {
        return;
    }
    
    fprintf(log_file, "[%s] Mode: %-10s | %s\n", get_timestamp(), mode, details);
    fflush(log_file); // Ensure immediate write to disk
}

/*
 * Close the log file properly
 */
void close_log() {
    if (log_file) {
        write_log("SYSTEM", "Session ended - Program terminated normally");
        fclose(log_file);
        log_file = NULL;
    }
}

/*
 * Signal handler for SIGINT (Ctrl+C)
 */
void signal_handler(int signum) {
    if (signum == SIGINT) {
        printf("\n\nExiting... Saving log.\n");
        write_log("SIGNAL", "SIGINT received (Ctrl+C) - Saving log and terminating");
        close_log();
        exit(0);
    }
}

/*
 * Display help information for command-line usage
 */
void display_help() {
    printf("Usage: sysmonitor [OPTIONS]\n\n");
    printf("Options:\n");
    printf("  -m cpu          Display CPU usage only\n");
    printf("  -m mem          Display memory usage only\n");
    printf("  -m proc         List top 5 active processes\n");
    printf("  -c <interval>   Continuous monitoring every <interval> seconds\n");
    printf("  -h              Display this help message\n\n");
    printf("Examples:\n");
    printf("  ./sysmonitor -m cpu     # Display CPU usage and save to log\n");
    printf("  ./sysmonitor -m mem     # Display memory info and save to log\n");
    printf("  ./sysmonitor -m proc    # List top 5 processes\n");
    printf("  ./sysmonitor -c 2       # Monitor continuously every 2 seconds\n\n");
    printf("If no options are provided, the program runs in interactive menu mode.\n");
}

/*
 * Parse command-line arguments and execute appropriate function
 */
int parse_arguments(int argc, char *argv[]) {
    if (argc < 2) {
        return 1; // Will fall through to interactive mode
    }

    // Check for help flag
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
        display_help();
        write_log("CLI", "Help displayed");
        return 0;
    }

    // Check for -m flag (mode)
    if (strcmp(argv[1], "-m") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: missing parameter. Use -m [cpu/mem/proc].\n");
            write_log("ERROR", "Missing parameter for -m flag");
            return 1;
        }

        if (strcmp(argv[2], "cpu") == 0) {
            write_log("CLI", "CPU usage displayed via command-line");
            cpu_usage();
            return 0;
        } else if (strcmp(argv[2], "mem") == 0) {
            write_log("CLI", "Memory usage displayed via command-line");
            memory_usage();
            return 0;
        } else if (strcmp(argv[2], "proc") == 0) {
            write_log("CLI", "Top processes displayed via command-line");
            top_processes();
            return 0;
        } else {
            fprintf(stderr, "Invalid option. Use -h for help.\n");
            write_log("ERROR", "Invalid mode parameter");
            return 1;
        }
    }

    // Check for -c flag (continuous monitoring)
    if (strcmp(argv[1], "-c") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: missing parameter. Use -c <interval>.\n");
            write_log("ERROR", "Missing interval for -c flag");
            return 1;
        }

        int interval = atoi(argv[2]);
        if (interval <= 0) {
            fprintf(stderr, "Error: interval must be a positive number.\n");
            write_log("ERROR", "Invalid interval value for continuous monitoring");
            return 1;
        }

        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Continuous monitoring started with %d second interval", interval);
        write_log("CLI", log_msg);
        continuous_monitoring_with_interval(interval);
        return 0;
    }

    // Check for -x flag (invalid option for testing)
    if (strcmp(argv[1], "-x") == 0) {
        fprintf(stderr, "Invalid option. Use -h for help.\n");
        write_log("ERROR", "Invalid option -x provided");
        return 1;
    }

    // Unknown option
    fprintf(stderr, "Invalid option. Use -h for help.\n");
    write_log("ERROR", "Unknown command-line option");
    return 1;
}

/*
 * Continuous monitoring with specified interval
 */
void continuous_monitoring_with_interval(int interval) {
    printf("=== Continuous Monitoring (Every %d seconds) ===\n", interval);
    printf("Press Ctrl+C to stop...\n\n");
    
    int iteration = 0;
    while (1) {
        iteration++;
        
        // Clear screen and display header
        clear_screen();
        printf("=== Continuous Monitoring (Iteration %d) ===\n", iteration);
        printf("Refresh Interval: %d seconds | Press Ctrl+C to stop\n\n", interval);
        
        // Display timestamp
        printf("[%s]\n\n", get_timestamp());
        
        // Display system information (placeholder for now)
        printf("CPU Usage: [Not implemented]\n");
        printf("Memory Usage: [Not implemented]\n");
        printf("\nMonitoring... (waiting %d seconds)\n", interval);
        
        // Log periodic entry
        char log_msg[256];
        snprintf(log_msg, sizeof(log_msg), "Continuous monitoring - iteration %d", iteration);
        write_log("MONITOR", log_msg);
        
        // Wait for specified interval
        sleep(interval);
    }
}
