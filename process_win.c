// filepath: d:\4th Sam\OS\New folder\process_win.c
// This file contains the implementation of the advanced process viewer
// adapted from task.c for Windows systems
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <winbase.h>
#include <wtsapi32.h>
#include <conio.h>
#include <time.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "wtsapi32.lib")

#define MAX_PROCESSES 1024
#define MAX_LINE 4096
#define MAX_COUNTER_NAME 1024

// Color definitions for output
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_RESET   "\033[0m"

// Aliases for better readability
#define CYN COLOR_CYAN
#define YEL COLOR_YELLOW
#define GRN COLOR_GREEN
#define RED COLOR_RED
#define BLU COLOR_BLUE
#define MAG COLOR_MAGENTA
#define RESET COLOR_RESET

typedef struct {
    DWORD pid;
    char name[MAX_PATH];
    char state; // R=Running, S=Sleeping, Z=Zombie (approximated for Windows)
    DWORD ppid;
    SIZE_T memory;  // Working set size in KB
    float cpu_usage;
    char path[MAX_PATH];
    char user[256];
    FILETIME creation_time;
    int priority;
    ULARGE_INTEGER prev_kernel_time;
    ULARGE_INTEGER prev_user_time;
    ULARGE_INTEGER kernel_time;
    ULARGE_INTEGER user_time;
} Process;

Process processes[MAX_PROCESSES];
Process prev_processes[MAX_PROCESSES];
int process_count = 0;
int prev_process_count = 0;
ULARGE_INTEGER last_system_time = {0};
FILETIME last_fetch_time = {0};

// Random ASCII art for process manager
void print_random_ascii_art() {
    int choice = rand() % 3;
    
    switch (choice) {
        case 0:
            printf(CYN "\n               _____                                        \n");
            printf("              |  __ \\                                      \n");
            printf("              | |__) | __ ___   ___ ___  ___ ___           \n");
            printf("              |  ___/ \'__/ _ \\ / __/ _ \\/ __/ __|         \n");
            printf("              | |   | | | (_) | (_|  __/\\__ \\__ \\         \n");
            printf("              |_|   |_|  \\___/ \\___\\___||___/___/       \n" RESET);
            break;
            
        case 1:
            printf(CYN "\n           /$$      /$$                     /$$   /$$                      \n");
            printf("          | $$$    /$$$                    |__/  | $$                      \n");
            printf("          | $$$$  /$$$$  /$$$$$$  /$$$$$$$  /$$ /$$$$$$    /$$$$$$        \n");
            printf("          | $$ $$/$$ $$ /$$__  $$| $$__  $$| $$|_  $$_/   /$$__  $$       \n");
            printf("          | $$  $$$| $$| $$  \\ $$| $$  \\ $$| $$  | $$    | $$  \\ $$   \n");
            printf("          | $$\\  $ | $$| $$  | $$| $$  | $$| $$  | $$ /$$| $$  | $$      \n");
            printf("          | $$ \\/  | $$|  $$$$$$/| $$  | $$| $$  |  $$$$/|  $$$$$$/      \n");
            printf("          |__/     |__/ \\______/ |__/  |__/|__/   \\___/   \\______/    \n" RESET);
            break;
            
        case 2:
            printf(CYN "\n          .--.                  .-.                            \n");
            printf("         : .-\'                 .\' `.                           \n");
            printf("         : `;                  : `. :                            \n");
            printf("         : :     .--.  ,-.,-. .\' ,\' :     .--.  .--.  _______    \n");
            printf("         : :__  \' .; ; : ,. :.\' \' \' :    \'  .; : .--.\'..------.  \n");
            printf("         `.___.\'.__.\'':_;:_;:___.\' :__.__`.__.\'`.__.\';\'.------\'  \n");
            printf("                                                  ._.\'          \n" RESET);
            break;
    }
}

// ASCII art for the banner
void print_banner() {
    printf(CYN "\n+----------------------------------------------------------+\n");
    printf("|            Advanced Process Manager for Windows          |\n");
    printf("+----------------------------------------------------------+\n" RESET);
    
    print_random_ascii_art();
}

void get_memory_usage(ULONGLONG *total, ULONGLONG *available) {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    *total = memInfo.ullTotalPhys;
    *available = memInfo.ullAvailPhys;
}

void print_system_health() {
    printf(CYN "\nSystem Health Overview:\n" RESET);
    
    // Uptime
    DWORD tickCount = GetTickCount();
    int uptime_days = tickCount / (1000 * 60 * 60 * 24);
    int uptime_hours = (tickCount % (1000 * 60 * 60 * 24)) / (1000 * 60 * 60);
    int uptime_minutes = (tickCount % (1000 * 60 * 60)) / (1000 * 60);
    printf("Uptime: %d days, %d hours, %d minutes\n",
           uptime_days, uptime_hours, uptime_minutes);
    
    // CPU Usage (using PDH)
    PDH_HQUERY cpuQuery;
    PDH_HCOUNTER cpuTotal;
    DWORD counterType = 0;
    PDH_FMT_COUNTERVALUE counterVal;
    
    PdhOpenQuery(NULL, 0, &cpuQuery);
    PdhAddCounter(cpuQuery, "\\Processor(_Total)\\% Processor Time", 0, &cpuTotal);
    PdhCollectQueryData(cpuQuery);
    Sleep(1000); // Need to wait to get an accurate reading
    PdhCollectQueryData(cpuQuery);
    PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, &counterType, &counterVal);
    printf("CPU Usage: %.2f%%\n", counterVal.doubleValue);
    PdhCloseQuery(cpuQuery);
    
    // Memory usage
    ULONGLONG total_mem = 0, available_mem = 0;
    get_memory_usage(&total_mem, &available_mem);
    printf("Memory Usage:\n");
    printf("  Total: %llu MB\n  Available: %llu MB\n  Used: %llu MB\n",
           total_mem / (1024 * 1024), 
           available_mem / (1024 * 1024),
           (total_mem - available_mem) / (1024 * 1024));
    
    // Disk space (C: drive)
    ULARGE_INTEGER freeBytesAvailable, totalBytes, totalFreeBytes;
    if (GetDiskFreeSpaceEx("C:\\", &freeBytesAvailable, &totalBytes, &totalFreeBytes)) {
        printf("Disk Space (C:):\n");
        printf("  Total: %llu GB\n  Free: %llu GB\n",
               totalBytes.QuadPart / (1024 * 1024 * 1024),
               totalFreeBytes.QuadPart / (1024 * 1024 * 1024));
    }
}

// Get the parent process ID (ppid) for a given pid
DWORD get_parent_pid(DWORD pid) {
    DWORD ppid = 0;
    HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32 pe = { 0 };
    pe.dwSize = sizeof(PROCESSENTRY32);
    
    if (Process32First(h, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                ppid = pe.th32ParentProcessID;
                break;
            }
        } while (Process32Next(h, &pe));
    }
    
    CloseHandle(h);
    return ppid;
}

// Get username of process owner
void get_process_username(DWORD pid, char *username, int size) {
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    HANDLE hToken = NULL;
    
    if (hProcess) {
        if (OpenProcessToken(hProcess, TOKEN_QUERY, &hToken)) {
            DWORD dwSize = 0;
            GetTokenInformation(hToken, TokenUser, NULL, 0, &dwSize);
            
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                TOKEN_USER *pUserInfo = (TOKEN_USER*)malloc(dwSize);
                if (pUserInfo) {
                    if (GetTokenInformation(hToken, TokenUser, pUserInfo, dwSize, &dwSize)) {
                        SID_NAME_USE SidType;
                        char domainName[256] = {0};
                        DWORD domainSize = sizeof(domainName);
                        
                        if (LookupAccountSid(NULL, pUserInfo->User.Sid, username, (LPDWORD)&size,
                                            domainName, &domainSize, &SidType)) {
                            // Successfully got username
                        } else {
                            strncpy(username, "unknown", size);
                        }
                    }
                    free(pUserInfo);
                }
            }
            CloseHandle(hToken);
        } else {
            strncpy(username, "unknown", size);
        }
        CloseHandle(hProcess);
    } else {
        strncpy(username, "unknown", size);
    }
}

// Uses WTS API to get process info
void fetch_processes() {
    FILETIME system_time, ignore_time;
    GetSystemTimeAsFileTime(&system_time);
    
    // Copy current processes to previous processes for CPU usage calculation
    memcpy(prev_processes, processes, sizeof(processes));
    prev_process_count = process_count;
    process_count = 0;
    
    // Save system time for CPU calculation
    ULARGE_INTEGER curr_system_time;
    curr_system_time.LowPart = system_time.dwLowDateTime;
    curr_system_time.HighPart = system_time.dwHighDateTime;
    
    // Calculate system time diff for CPU usage calculation
    ULONGLONG system_time_diff = curr_system_time.QuadPart - last_system_time.QuadPart;
    last_system_time.QuadPart = curr_system_time.QuadPart;
    
    // Take snapshot of all processes
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) return;
    
    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);
    
    if (!Process32First(hProcessSnap, &pe32)) {
        CloseHandle(hProcessSnap);
        return;
    }
    
    do {
        if (process_count < MAX_PROCESSES) {
            Process p = {0};
            p.pid = pe32.th32ProcessID;
            p.ppid = pe32.th32ParentProcessID;
            strncpy(p.name, pe32.szExeFile, sizeof(p.name) - 1);
            
            // Set default state to running (we will update it later if possible)
            p.state = 'R';
            
            // Get process memory info
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, p.pid);
            if (hProcess) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    p.memory = pmc.WorkingSetSize / 1024; // Convert to KB
                }
                
                // Get process path
                if (GetModuleFileNameEx(hProcess, NULL, p.path, sizeof(p.path) - 1) == 0) {
                    // If we cannot get the file name, use the process name
                    strncpy(p.path, p.name, sizeof(p.path) - 1);
                }
                
                // Get process times for CPU usage calculation
                GetProcessTimes(hProcess, &p.creation_time, &ignore_time, 
                               (LPFILETIME)&p.kernel_time, (LPFILETIME)&p.user_time);
                
                // Get process priority
                p.priority = GetPriorityClass(hProcess);
                
                // Try to determine if process is suspended (not perfect)
                DWORD exitCode;
                if (GetExitCodeProcess(hProcess, &exitCode)) {
                    if (exitCode != STILL_ACTIVE) {
                        p.state = 'Z'; // Mark as zombie if process has exited
                    }
                }
                
                CloseHandle(hProcess);
            } else {
                p.state = 'Z'; // Marking as zombie (terminated/inaccessible) if we cannot open it
            }
            
            // Get username of process owner
            get_process_username(p.pid, p.user, sizeof(p.user) - 1);
            
            // Store the process in our array
            processes[process_count++] = p;
        }
    } while (Process32Next(hProcessSnap, &pe32));
    
    CloseHandle(hProcessSnap);
    
    // Calculate CPU usage for each process
    if (prev_process_count > 0 && system_time_diff > 0) {
        for (int i = 0; i < process_count; i++) {
            int found_prev = 0;
            
            for (int j = 0; j < prev_process_count; j++) {
                if (processes[i].pid == prev_processes[j].pid) {
                    found_prev = 1;
                    ULONGLONG process_time_diff =
                        (processes[i].kernel_time.QuadPart + processes[i].user_time.QuadPart) -
                        (prev_processes[j].prev_kernel_time.QuadPart + prev_processes[j].prev_user_time.QuadPart);
                    
                    if (process_time_diff > 0) {
                        processes[i].cpu_usage = (float)((100.0 * process_time_diff) / system_time_diff);
                        if (processes[i].cpu_usage > 100.0f) processes[i].cpu_usage = 100.0f;
                    }
                    
                    processes[i].prev_kernel_time.QuadPart = processes[i].kernel_time.QuadPart;
                    processes[i].prev_user_time.QuadPart = processes[i].user_time.QuadPart;
                    
                    break;
                }
            }
            
            if (!found_prev) {
                processes[i].prev_kernel_time.QuadPart = processes[i].kernel_time.QuadPart;
                processes[i].prev_user_time.QuadPart = processes[i].user_time.QuadPart;
                processes[i].cpu_usage = 0.0f;
            }
        }
    } else {
        for (int i = 0; i < process_count; i++) {
            processes[i].prev_kernel_time.QuadPart = processes[i].kernel_time.QuadPart;
            processes[i].prev_user_time.QuadPart = processes[i].user_time.QuadPart;
            processes[i].cpu_usage = 0.0f;
        }
    }
}

// Comparison function for sorting processes by CPU usage
int compare_cpu(const void *a, const void *b) {
    float diff = ((Process*)b)->cpu_usage - ((Process*)a)->cpu_usage;
    return (diff > 0) ? 1 : ((diff < 0) ? -1 : 0);
}

// Comparison function for sorting processes by memory usage
int compare_mem(const void *a, const void *b) {
    LONGLONG diff = ((Process*)b)->memory - ((Process*)a)->memory;
    return (diff > 0) ? 1 : ((diff < 0) ? -1 : 0);
}

// Print details of a single process
void print_process(Process *p) {
    printf("%-6lu %-20s %-8s %-6c %-7d %.2f%%  \n", 
           p->pid, p->name, p->user, p->state, p->priority, p->cpu_usage);
}

// Recursive function to display process tree
void process_tree(DWORD ppid, int depth) {
    for (int i = 0; i < process_count; i++) {
        if (processes[i].ppid == ppid) {
            // Add indentation based on depth
            for (int j = 0; j < depth; j++) {
                if (j == depth - 1)
                    printf("|--- ");
                else
                    printf("|    ");
            }
            
            printf(GRN "%lu %s" RESET "\n", processes[i].pid, processes[i].name);
            process_tree(processes[i].pid, depth + 1);
        }
    }
}

// Function to terminate a process
int terminate_process(DWORD pid) {
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    int success = 0;
    
    if (hProcess) {
        if (TerminateProcess(hProcess, 0)) {
            success = 1;
        }
        CloseHandle(hProcess);
    }
    
    return success;
}

// Function to set process priority
int set_process_priority(DWORD pid, DWORD priority_class) {
    HANDLE hProcess = OpenProcess(PROCESS_SET_INFORMATION, FALSE, pid);
    int success = 0;
    
    if (hProcess) {
        if (SetPriorityClass(hProcess, priority_class)) {
            success = 1;
        }
        CloseHandle(hProcess);
    }
    
    return success;
}

// Function to filter processes by username
void filter_by_user(const char* username) {
    printf(GRN "\nProcesses for user %s:\n" RESET, username);
    printf("PID    NAME                 USER     STATE\n");
    printf("--------------------------------------------------\n");
    
    int count = 0;
    for (int i = 0; i < process_count; i++) {
        if (strcmp(processes[i].user, username) == 0) {
            printf("%-6lu %-20s %-8s %-6c\n", 
                   processes[i].pid, processes[i].name,
                   processes[i].user, processes[i].state);
            count++;
        }
    }
    
    if (count == 0) {
        printf(RED "No processes found for user %s\n" RESET, username);
    }
}

// Main function for the process viewer
void process_manager() {
    int choice;
    // Initialize random seed for ASCII art selection
    srand((unsigned int)time(NULL));
    
    while (1) {
        system("cls");
        print_banner();
        print_system_health();
        printf(YEL "\n1.  List All Processes\n");
        printf("2.  Process Details\n");
        printf("3.  Terminate Process\n");
        printf("4.  Process Tree\n");
        printf("5.  Top CPU Processes\n");
        printf("6.  Top Memory Processes\n");
        printf("7.  Filter by User\n");
        printf("8.  Set Process Priority\n");
        printf("9.  Exit\n");
        printf("10. Search Process\n");
        printf("11. Real-time Process List\n");
        printf("12. Suspended Processes\n");
        printf("13. Advanced Termination\n" RESET);
        printf(MAG "Enter choice: " RESET);
        scanf("%d", &choice);
        
        // Clear input buffer
        while (getchar() != '\n');
        
        fetch_processes();

        switch (choice) {
            case 1:
                printf(GRN "\nPID    Name                 User     State  Priority CPU%%   \n" RESET);
                for (int i = 0; i < process_count; i++) {
                    const char *color = processes[i].state == 'Z' ? RED : "";
                    
                    // Print process details with conditional color
                    printf("%s%-6lu %-20s %-8s %-6c %-7d %.2f%%%s  \n", 
                           color, processes[i].pid, processes[i].name,
                           processes[i].user, processes[i].state,
                           processes[i].priority, processes[i].cpu_usage,
                           color[0] ? RESET : "");
                }
                break;
                
            case 2: {
                DWORD pid;
                printf("Enter PID to view details: ");
                scanf("%lu", &pid);
                int found = 0;
                
                for (int i = 0; i < process_count; i++) {
                    if (processes[i].pid == pid) {
                        printf(GRN "\n   ___                            ___      _        _ _     \n");
                        printf("  / _ \\ _ __ ___   ___ ___  ___ ___|   \\ ___| |_ __ _(_) |___ \n");
                        printf(" / /_)/ \'__/ _ \\ / __/ _ \\/ __/ __| |) / _ \\ __/ _` | | / __|\n");
                        printf("/ ___/| | | (_) | (_|  __/\\__ \\__ \\  __/  __/ || (_| | | \\__ \\\n");
                        printf("\\/    |_|  \\___/ \\___\\___||___/___/_|   \\___|\\__\\__,_|_|_|___/" RESET "\n\n");
                        print_process(&processes[i]);
                        found = 1;
                        break;
                    }
                }
                if (!found) printf(RED "Process with PID %d not found.\n" RESET, pid);
                break;
            }
                
            case 3: {
                DWORD pid;
                printf("Enter PID to terminate: ");
                scanf("%lu", &pid);
                if (terminate_process(pid)) {
                    printf(GRN "Process %lu terminated successfully.\n" RESET, pid);
                } else {
                    printf(RED "Failed to terminate process %lu. Access denied or process not found.\n" RESET, pid);
                }
                break;
            }
                
            case 4: {
                DWORD pid;
                printf(YEL "\n   _____                     _____              \n");
                printf("  |  __ \\                   |_   _| __ ___  ___ \n");
                printf("  | |__) | __ ___   ___ ___   | || \'__/ _ \\/ _ \\\n");
                printf("  |  ___/ \'__/ _ \\ / __/ _ \\  | || | |  __/  __/\n");
                printf("  | |   | | | (_) | (_|  __/  | || |  \\___|\\___|\n");
                printf("  |_|   |_|  \\___/ \\___\\___|  |_||_|            \n" RESET);
                
                printf("Enter PID to view process tree (0 for all): ");
                scanf("%lu", &pid);
                
                if (pid == 0) {
                    // Find root processes (those with no parent or parent not found)
                    printf(GRN "\nProcess Tree:\n" RESET);
                    for (int i = 0; i < process_count; i++) {
                        int has_parent = 0;
                        
                        for (int j = 0; j < process_count; j++) {
                            if (processes[i].ppid == processes[j].pid) {
                                has_parent = 1;
                                break;
                            }
                        }
                        
                        if (!has_parent || processes[i].ppid == 0) {
                            printf(GRN "%lu %s" RESET "\n", processes[i].pid, processes[i].name);
                            process_tree(processes[i].pid, 1);
                        }
                    }
                } else {
                    // Display tree for a specific process
                    int found = 0;
                    for (int i = 0; i < process_count; i++) {
                        if (processes[i].pid == pid) {
                            found = 1;
                            printf(GRN "%lu %s" RESET "\n", processes[i].pid, processes[i].name);
                            process_tree(processes[i].pid, 1);
                            break;
                        }
                    }
                    
                    if (!found) {
                        printf(RED "Process with PID %lu not found.\n" RESET, pid);
                    }
                }
                break;
            }
                
            case 5:
                qsort(processes, process_count, sizeof(Process), compare_cpu);
                printf(GRN "\n   _____           ___ ___ _   _   _____                              \n");
                printf("  |_   _|__  _ __ / __/ _ \\ | | | |  ___| __ ___   ___ ___  ___ ___ ___ ___ \n");
                printf("    | |/ _ \\| \'_ \\ (_| (_) | |_| | |_ | \'__/ _ \\ / __/ _ \\/ __/ __/ __/ __|\n");
                printf("    | | (_) | |_) \\__ \\___/|  _  |  _|| | | (_) | (_|  __/\\__ \\__ \\__ \\__ \\\n");
                printf("    |_|\\___/| .__/|___/    |_| |_|_|  |_|  \\___/ \\___\\___||___/___/___/___/\n");
                printf("            |_|                                                           " RESET "\n");
                for (int i = 0; i < 10 && i < process_count; i++) {
                    print_process(&processes[i]);
                }
                break;
                
            case 6:
                qsort(processes, process_count, sizeof(Process), compare_mem);
                printf(GRN "\n  __  __                                   _   _                    \n");
                printf(" |  \\/  | ___ _ __ ___   ___  _ __ _   _| | | |___  __ _  __ _  ___ \n");
                printf(" | |\\/| |/ _ \\ \'_ ` _ \\ / _ \\| \'__| | | | | | / __|/ _` |/ _` |/ _ \\\n");
                printf(" | |  | |  __/ | | | | | (_) | |  | |_| | |_| \\__ \\ (_| | (_| |  __/\n");
                printf(" |_|  |_|\\___|_| |_| |_|\\___/|_|   \\__, |\\___/|___/\\__,_|\\__, |\\___|\n");
                printf("                                   |___/                 |___/      " RESET "\n");
                for (int i = 0; i < 10 && i < process_count; i++) {
                    print_process(&processes[i]);
                }
                break;
                
            case 7: {
                char username[256];
                printf("Enter username to filter by (or 'current' for current user): ");
                fgets(username, sizeof(username), stdin);
                username[strcspn(username, "\n")] = 0; // Remove newline
                
                if (strcmp(username, "current") == 0) {
                    // Get current username
                    DWORD size = sizeof(username);
                    GetUserName(username, &size);
                }
                
                filter_by_user(username);
                break;
            }
                
            case 8: {
                DWORD pid, priority;
                printf("Enter PID to set priority: ");
                scanf("%lu", &pid);
                printf("Enter priority (1=Low, 2=Below Normal, 3=Normal, 4=Above Normal, 5=High, 6=Realtime): ");
                scanf("%lu", &priority);
                
                DWORD priority_class;
                switch (priority) {
                    case 1: priority_class = IDLE_PRIORITY_CLASS; break;
                    case 2: priority_class = BELOW_NORMAL_PRIORITY_CLASS; break;
                    case 3: priority_class = NORMAL_PRIORITY_CLASS; break;
                    case 4: priority_class = ABOVE_NORMAL_PRIORITY_CLASS; break;
                    case 5: priority_class = HIGH_PRIORITY_CLASS; break;
                    case 6: priority_class = REALTIME_PRIORITY_CLASS; break;
                    default: priority_class = NORMAL_PRIORITY_CLASS;
                }
                
                if (set_process_priority(pid, priority_class)) {
                    printf(GRN "Priority set successfully for PID %lu.\n" RESET, pid);
                } else {
                    printf(RED "Failed to set priority for PID %lu. Access denied or process not found.\n" RESET, pid);
                }
                break;
            }
                
            case 9:
                printf(GRN "Returning to Masoom Shell...\n" RESET);
                return;
                
            case 10: {
                int search_choice;
                printf(CYN "\n   _____                     _       _____                             \n");
                printf("  / ____|                   | |     |  __ \\                            \n");
                printf(" | (___   ___  __ _ _ __ ___| |__   | |__) | __ ___   ___ ___  ___ ___ \n");
                printf("  \\___ \\ / _ \\/ _` | \'__/ __| \'_ \\  |  ___/ \'__/ _ \\ / __/ _ \\/ __/ __|\n");
                printf("  ____) |  __/ (_| | | | (__| | | | | |   | | | (_) | (_|  __/\\__ \\__ \\\n");
                printf(" |_____/ \\___|\\__,_|_|  \\___|_| |_| |_|   |_|  \\___/ \\___\\___||___/___/\n" RESET);
                                                                 
                printf("Search by: 1. PID 2. Name\n");
                scanf("%d", &search_choice);
                
                if (search_choice == 1) {
                    DWORD search_pid;
                    printf("Enter PID to search for: ");
                    scanf("%lu", &search_pid);
                    
                    int found = 0;
                    for (int i = 0; i < process_count; i++) {
                        if (processes[i].pid == search_pid) {
                            printf(GRN "\nProcess found:\n" RESET);
                            print_process(&processes[i]);
                            found = 1;
                            break;
                        }
                    }
                    
                    if (!found) {
                        printf(RED "Process with PID %lu not found.\n" RESET, search_pid);
                    }
                } else if (search_choice == 2) {
                    char search_name[MAX_PATH];
                    printf("Enter name to search for (case-sensitive): ");
                    scanf("%s", search_name);
                    
                    printf(GRN "\nMatching processes:\n" RESET);
                    int found = 0;
                    for (int i = 0; i < process_count; i++) {
                        if (strstr(processes[i].name, search_name)) {
                            print_process(&processes[i]);
                            found = 1;
                        }
                    }
                    
                    if (!found) {
                        printf(RED "No processes matching '%s' found.\n" RESET, search_name);
                    }
                }
                break;
            }
                
            case 11: { // Real-time process list
                int refreshes = 5;
                printf(GRN "\nShowing real-time process list (top 10 by CPU, refreshing %d times)\n" RESET, refreshes);
                
                for (int i = 0; i < refreshes; i++) {
                    system("cls");
                    print_banner();
                    fetch_processes();
                    qsort(processes, process_count, sizeof(Process), compare_cpu);
                    
                    printf(CYN "\nRefreshing in 2 seconds... (%d/%d)\n" RESET, i+1, refreshes);
                    printf("PID    Name                 User     State  CPU%%\n");
                    printf("--------------------------------------------------\n");
                    
                    for (int j = 0; j < 10 && j < process_count; j++) {
                        const char *color = processes[j].state == 'Z' ? RED : "";
                        
                        printf("%s%-6lu %-20s %-8s %-6c %.2f%%%s\n", 
                               color,
                               processes[j].pid, processes[j].name,
                               processes[j].user, processes[j].state,
                               processes[j].cpu_usage,
                               color[0] ? RESET : "");
                    }
                    
                    Sleep(2000); // 2 second refresh
                }
                break;
            }
                
            case 12: {
                int suspended_count = 0;
                printf(YEL "\n   _____                               _          _ \n");
                printf("  / ____|                             | |        | |\n");
                printf(" | (___  _   _ ___ _ __   ___ _ __  __| | ___  __| |\n");
                printf("  \\___ \\| | | / __| \'_ \\ / _ \\ \'_ \\/ _` |/ _ \\/ _` |\n");
                printf("  ____) | |_| \\__ \\ |_) |  __/ | | | (_| |  __/ (_| |\n");
                printf(" |_____/ \\__,_|___/ .__/ \\___|_| |_|\\__,_|\\___|\\__,_|\n");
                printf("                  | |                                \n");
                printf("                  |_|                                \n" RESET);
                
                for (int i = 0; i < process_count; i++) {
                    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processes[i].pid);
                    if (hProcess) {
                        BOOL is_suspended = FALSE;
                        
                        // Check if process has suspended threads
                        HANDLE hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
                        if (hThreadSnap != INVALID_HANDLE_VALUE) {
                            THREADENTRY32 te32;
                            te32.dwSize = sizeof(THREADENTRY32);
                            
                            if (Thread32First(hThreadSnap, &te32)) {
                                do {
                                    if (te32.th32OwnerProcessID == processes[i].pid) {
                                        HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID);
                                        if (hThread) {
                                            DWORD suspendCount = SuspendThread(hThread);
                                            if (suspendCount > 0) {
                                                is_suspended = TRUE;
                                            }
                                            
                                            // Resume the thread we just suspended for checking
                                            ResumeThread(hThread);
                                            CloseHandle(hThread);
                                        }
                                    }
                                } while (Thread32Next(hThreadSnap, &te32));
                            }
                            CloseHandle(hThreadSnap);
                        }
                        
                        if (is_suspended) {
                            printf("PID: %lu, Name: %s\n", processes[i].pid, processes[i].name);
                            suspended_count++;
                        }
                        
                        CloseHandle(hProcess);
                    }
                }
                
                if (suspended_count == 0) {
                    printf(GRN "No suspended processes found.\n" RESET);
                }
                break;
            }
                
            case 13: {
                printf(GRN "\nAdvanced Termination Options:\n" RESET);
                printf("1. Terminate process and all child processes\n");
                printf("2. Force kill process (may cause data loss)\n");
                printf("3. Terminate process by name\n");
                printf("Enter option: ");
                
                int sub_choice;
                scanf("%d", &sub_choice);
                
                switch (sub_choice) {
                    case 1: {
                        DWORD pid;
                        printf("Enter parent PID to terminate with all children: ");
                        scanf("%lu", &pid);
                        
                        // First find and terminate all child processes
                        int children_terminated = 0;
                        for (int i = 0; i < process_count; i++) {
                            if (processes[i].ppid == pid) {
                                if (terminate_process(processes[i].pid)) {
                                    printf(GRN "Child process %lu terminated.\n" RESET, processes[i].pid);
                                    children_terminated++;
                                }
                            }
                        }
                        
                        // Then terminate the parent
                        if (terminate_process(pid)) {
                            printf(GRN "Parent process %lu terminated along with %d children.\n" RESET, 
                                   pid, children_terminated);
                        } else {
                            printf(RED "Failed to terminate parent process %lu. %d children were terminated.\n" RESET, 
                                   pid, children_terminated);
                        }
                        break;
                    }
                        
                    case 2: {
                        DWORD pid;
                        printf("Enter PID to force kill: ");
                        scanf("%lu", &pid);
                        
                        HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                        if (hProcess) {
                            if (TerminateProcess(hProcess, 1)) {
                                printf(GRN "Process %lu force killed.\n" RESET, pid);
                            } else {
                                printf(RED "Failed to force kill process %lu. Error code: %lu\n" RESET, 
                                       pid, GetLastError());
                            }
                            CloseHandle(hProcess);
                        } else {
                            printf(RED "Could not open process %lu. Error code: %lu\n" RESET, 
                                   pid, GetLastError());
                        }
                        break;
                    }
                        
                    case 3: {
                        char name[MAX_PATH];
                        printf("Enter process name to terminate (e.g., notepad.exe): ");
                        scanf("%s", name);
                        
                        int count = 0;
                        for (int i = 0; i < process_count; i++) {
                            if (stricmp(processes[i].name, name) == 0) {
                                if (terminate_process(processes[i].pid)) {
                                    printf(GRN "Process %s (PID %lu) terminated.\n" RESET, 
                                           name, processes[i].pid);
                                    count++;
                                }
                            }
                        }
                        
                        if (count > 0) {
                            printf(GRN "Terminated %d instances of %s.\n" RESET, count, name);
                        } else {
                            printf(RED "No instances of %s found or could not be terminated.\n" RESET, name);
                        }
                        break;
                    }
                }
                break;
            }
                
            default:
                printf(RED "Invalid choice! Please select a valid option.\n" RESET);
        }
        
        printf("\nPress Enter to continue...");
        getchar();
    }
}
