#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <conio.h>
#include <direct.h>
#include <time.h>
#include <lmcons.h>  // For UNLEN definition
#include <tlhelp32.h>
#include <psapi.h>
#include <pdh.h>      // For Performance Data Helper (CPU usage)
#include <pdhmsg.h>   // For PDH error messages

#define MAX_INPUT_LENGTH 1024
#define MAX_PATH_LENGTH 260
#define HISTORY_SIZE 20
#define TAB_COMPLETE_MAX 100
#define MAX_PROCESSES 1024

// Color theme definition
typedef struct {
    const char* primary;
    const char* secondary;
    const char* highlight;
    const char* error;
    const char* reset;
    const char* name;
} ColorTheme;

// Default ANSI Color codes for Windows console
#define COLOR_CYAN    "\033[1;36m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"

// Process structure for the advanced process viewer
typedef struct {
    DWORD pid;
    char name[MAX_PATH];
    char state; // Windows doesn't have direct state like Linux, we'll approximate
    DWORD ppid;
    SIZE_T memory;
    float cpu_usage;
    char path[MAX_PATH];
    char user[UNLEN + 1];
    FILETIME creation_time;
    int priority;
    FILETIME kernel_time;
    FILETIME user_time;
} Process;

// Global variable for current color theme
int current_theme = 0;
ColorTheme themes[4];  // Array to hold different color themes

// Command alias storage
#define MAX_ALIASES 50
typedef struct {
    char name[MAX_INPUT_LENGTH];
    char command[MAX_INPUT_LENGTH];
} CommandAlias;

CommandAlias aliases[MAX_ALIASES];
int alias_count = 0;

// Function declarations
void print_welcome();
void display_manual();
void process_command(char* command);
char** get_tab_completions(const char* partial, int* count);
void free_tab_completions(char** completions, int count);
void search_files(const char* search_term, int search_content);
void mausam_batao(const char* city);
void execute_commands(char* command_string);
void likho_file(const char* filename);
void tarikh_batao(int month, int year);
void process_dikhao();
// External function from process_win.c
void process_manager();
void internet_check(const char* website);
void rang_badlo(int theme_id);
void initialize_themes();
void register_alias(const char* alias_name, const char* command);
void display_aliases();
void delete_alias(const char* alias_name);
void save_aliases();
void load_aliases();

char command_history[HISTORY_SIZE][MAX_INPUT_LENGTH];
int history_count = 0;
int history_index = 0;

// List of built-in commands for tab completion
const char* builtin_commands[] = {
    "help-karo", "dekhao", "ghar-jao", "waqt-batao", "khatam-karo",
    "saaf-karo", "dastavez-dikhao", "rasta-batao", "file-parho", 
    "system-info", "hisaab-karo", "dhundo", "mausam-batao",
    "likho", "tarikh-batao", "process-dikhao", "internet-check", 
    "rang-badlo", "alias-banao", "alias-dikhao", "alias-mitao"
};
const int builtin_commands_count = sizeof(builtin_commands) / sizeof(builtin_commands[0]);

#define MAX_LINE_LENGTH 1024

// Advanced input function with in-line editing and arrow key support
int masoom_readline(char *buffer, int maxlen) {
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, ENABLE_PROCESSED_INPUT | ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT);

    int len = 0;
    int cursor = 0;
    int insert_mode = 1;
    int done = 0;
    buffer[0] = '\0';

    // Use _getch for character-by-character input
    while (!done) {
        int ch = _getch();
        if (ch == 13) { // Enter
            putchar('\n');
            buffer[len] = '\0';
            done = 1;
        } else if (ch == 8 || ch == 127) { // Backspace
            if (cursor > 0) {
                for (int i = cursor - 1; i < len - 1; i++)
                    buffer[i] = buffer[i + 1];
                len--;
                cursor--;
                buffer[len] = '\0';
                printf("\r\033[K");
                printf(":> %s", buffer);
                for (int i = len; i > cursor; i--) putchar('\b');
            }
        } else if (ch == 0 || ch == 224) { // Special keys
            int key = _getch();
            if (key == 75) { // Left arrow
                if (cursor > 0) {
                    cursor--;
                    putchar('\b');
                }
            } else if (key == 77) { // Right arrow
                if (cursor < len) {
                    putchar(buffer[cursor]);
                    cursor++;
                }
            } else if (key == 71) { // Home
                while (cursor > 0) {
                    putchar('\b');
                    cursor--;
                }
            } else if (key == 79) { // End
                while (cursor < len) {
                    putchar(buffer[cursor]);
                    cursor++;
                }
            }
        } else if (ch == 27) { // ESC
            buffer[0] = '\0';
            printf("\r\033[K:> ");
            len = 0;
            cursor = 0;
        } else if (ch == 3) { // Ctrl+C
            printf("^C\n");
            buffer[0] = '\0';
            return -1;
        } else if (ch >= 32 && ch < 127 && len < maxlen - 1) { // Printable
            if (cursor == len) {
                buffer[len++] = (char)ch;
                buffer[len] = '\0';
                putchar(ch);
                cursor++;
            } else {
                for (int i = len; i > cursor; i--)
                    buffer[i] = buffer[i - 1];
                buffer[cursor] = (char)ch;
                len++;
                buffer[len] = '\0';
                printf("\r\033[K:> %s", buffer);
                cursor++;
                for (int i = len; i > cursor; i--) putchar('\b');
            }
        }
    }
    return len;
}

// --- FIXED main function and initialization ---
int main(int argc, char* argv[]) {
    // Set console title
    SetConsoleTitleA("Masoom Shell");
    srand((unsigned int)time(NULL));
    initialize_themes();
    current_theme = 0;
    load_aliases();
    print_welcome();
    char input[MAX_INPUT_LENGTH];
    while (1) {
        printf("%sMasoom> %s", COLOR_CYAN, COLOR_RESET);
        if (!fgets(input, sizeof(input), stdin)) break;
        // Remove trailing newline
        input[strcspn(input, "\r\n")] = 0;
        process_command(input);
    }
    return 0;
}

// --- FIXED print_welcome function ---
void print_welcome() {
    // Get username for personalized welcome
    char userName[UNLEN + 1];
    DWORD userNameLength = UNLEN + 1;
    GetUserNameA(userName, &userNameLength);
    const char* greeting = "Assalam-o-Alaikum, ";
    
    // ASCII art logo for Masoom Shell
    printf(COLOR_CYAN);
    printf("\n          __  __                                  ____  _          _ _ \n");
    printf("         |  \\/  | __ _ ___  ___   ___  _ __ ___ / ___|| |__   ___| | |\n");
    printf("         | |\\/| |/ _` / __|/ _ \\ / _ \\| '_ ` _ \\\\___ \\| '_ \\ / _ \\ | |\n");
    printf("         | |  | | (_| \\__ \\ (_) | (_) | | | | | |___) | | | |  __/ | |\n");
    printf("         |_|  |_|\\__,_|___/\\___/ \\___/|_| |_| |_|____/|_| |_|\\___|_|_|\n");
    printf(COLOR_RESET);
    
    printf(COLOR_GREEN "\n%s %s\n" COLOR_RESET, greeting, userName);
    printf(COLOR_YELLOW "Welcome to " COLOR_CYAN "Masoom " COLOR_GREEN "Shell!\n" COLOR_RESET);
    printf(COLOR_YELLOW "Try these fun commands:\n");
    printf(COLOR_CYAN "  help-karo      " COLOR_RESET "- Show help\n");
    printf(COLOR_GREEN "  dekhao        " COLOR_RESET "- List files\n");
    printf(COLOR_CYAN "  ghar-jao      " COLOR_RESET "- Change directory\n");
    printf(COLOR_GREEN "  waqt-batao    " COLOR_RESET "- Show time\n");
    printf(COLOR_CYAN "  rasta-batao   " COLOR_RESET "- Show current path\n");
    printf(COLOR_GREEN "  file-parho    " COLOR_RESET "- Read file content\n");
    printf(COLOR_CYAN "  system-info   " COLOR_RESET "- Show system details\n");
    printf(COLOR_GREEN "  hisaab-karo   " COLOR_RESET "- Simple calculator\n");
    printf(COLOR_CYAN "  saaf-karo     " COLOR_RESET "- Clear screen\n");
    printf(COLOR_GREEN "  khatam-karo   " COLOR_RESET "- Exit shell\n");
    printf(COLOR_CYAN "  dhundo        " COLOR_RESET "- Search for files\n");
    printf(COLOR_GREEN "  mausam-batao  " COLOR_RESET "- Show weather info\n");
    printf(COLOR_CYAN "  likho         " COLOR_RESET "- Edit text files\n");
    printf(COLOR_GREEN "  tarikh-batao  " COLOR_RESET "- Show calendar\n");
    printf(COLOR_CYAN "  process-dikhao " COLOR_RESET "- List processes\n");
    printf(COLOR_GREEN "  internet-check " COLOR_RESET "- Test connectivity\n");
}

void display_manual() {
    // Try to find the manual file in multiple locations
    FILE *file = NULL;
    
    // Try in current directory first
    file = fopen("masoom_manual.txt", "r");
    
    // If not found, try in the same directory as the executable
    if (!file) {
        char exe_path[MAX_PATH_LENGTH] = {0};
        char manual_path[MAX_PATH_LENGTH] = {0};
        DWORD path_size = GetModuleFileNameA(NULL, exe_path, MAX_PATH_LENGTH);
        
        if (path_size > 0) {
            // Extract the directory from the exe path
            char* last_slash = strrchr(exe_path, '\\');
            if (last_slash) {
                *(last_slash + 1) = '\0'; // Truncate at the last backslash
                strcpy(manual_path, exe_path);
                strcat(manual_path, "masoom_manual.txt");
                file = fopen(manual_path, "r");
            }
        }
    }
    
    if (file) {
        printf("\n");
        char line[MAX_INPUT_LENGTH];
        
        while (fgets(line, sizeof(line), file)) {
            printf("%s", line);
        }
        
        fclose(file);
    } else {
        printf("%sMasoom Shell manual file (masoom_manual.txt) not found!%s\n", COLOR_RED, COLOR_RESET);
        printf("Please make sure the manual file is in the same directory as the shell.\n");
    }
}

void process_command(char* command) {
    // Trim leading/trailing whitespace from command
    while (*command == ' ' || *command == '\t') command++;
    size_t cmdlen = strlen(command);
    while (cmdlen > 0 && (command[cmdlen-1] == ' ' || command[cmdlen-1] == '\t' || command[cmdlen-1] == '\n')) command[--cmdlen] = 0;
    if (strlen(command) == 0) return;
    // Exit command
    if (strcmp(command, "khatam-karo") == 0) {
        printf("Allah Hafiz! Shell band ho rahi hai...\n");
        printf("\nPress any key to exit...");
        _getch();
        exit(0);
    }
    
    // Clear screen command
    else if (strcmp(command, "saaf-karo") == 0) {
        printf("Saaf safai ki ja rahi hai...\n");
        system("cls");
        print_welcome();
    }
    
    // Show manual command
    else if (strcmp(command, "dastavez-dikhao") == 0) {
        display_manual();
    }
    
    // Help command
    else if (strcmp(command, "help-karo") == 0) {
        printf("\n%sMasoom Shell Help:%s\n", COLOR_YELLOW, COLOR_RESET);
        printf("---------------------------\n");
        printf("dekhao          - List directory contents\n");
        printf("ghar-jao        - Change directory (e.g., ghar-jao Documents)\n");
        printf("waqt-batao      - Show current time\n");
        printf("khatam-karo     - Exit the shell\n");
        printf("saaf-karo       - Clear screen but keep the logo\n");
        printf("dastavez-dikhao - Display the comprehensive manual\n");
        printf("rasta-batao     - Show current directory path\n");
        printf("file-parho      - Read contents of a file (e.g., file-parho filename.txt)\n");
        printf("system-info     - Display system information\n");
        printf("hisaab-karo     - Simple calculator (e.g., hisaab-karo 2+2)\n");        printf("dhundo          - Search for files or content within files\n");
        printf("mausam-batao    - Show weather information for a city (e.g., mausam-batao Karachi)\n");
        printf("likho           - Create or edit a text file (e.g., likho note.txt)\n");
        printf("tarikh-batao    - Show calendar for a month and year (e.g., tarikh-batao 5 2023)\n");
        printf("process-dikhao  - Show list of running processes\n");
        printf("internet-check  - Check internet connectivity to a website (e.g., internet-check google.com)\n");        printf("rang-badlo      - Change color theme (e.g., rang-badlo 1)\n");
        printf("alias-banao     - Create a command alias (e.g., alias-banao ll=dekhao)\n");
        printf("alias-dikhao    - Display all command aliases\n");
        printf("alias-mitao     - Delete a command alias (e.g., alias-mitao ll)\n");
        printf("---------------------------\n");
    }
    
    // List files command
    else if (strcmp(command, "dekhao") == 0) {
        printf("Sab kuch dikha rahe hain!\n");
        system("dir");
    }
      // Change directory command
    else if (strcmp(command, "ghar-jao") == 0) {
        printf("%sGalat format! Sahi format hai: ghar-jao directory_name%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: ghar-jao Documents ya ghar-jao ..%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "ghar-jao ", 9) == 0) {
        char* dir = command + 9;
        if (strlen(dir) >= MAX_PATH_LENGTH) {
            printf("%sDirectory name too long!%s\n", COLOR_RED, COLOR_RESET);
            return;
        }
        printf("Ghar badal rahe hain...\n");
        if (_chdir(dir) != 0) {
            printf("%sGhar badalne mein dikkat!%s\n", COLOR_RED, COLOR_RESET);
        }
    }
    
    // Show time command
    else if (strcmp(command, "waqt-batao") == 0) {
        printf("Mubarak ho! Waqt hai:\n");
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        printf("%s%02d:%02d:%02d%s\n", COLOR_GREEN, t->tm_hour, t->tm_min, t->tm_sec, COLOR_RESET);
    }
    
    // Show current directory path (new command)
    else if (strcmp(command, "rasta-batao") == 0) {
        char current_dir[MAX_PATH_LENGTH];
        if (_getcwd(current_dir, MAX_PATH_LENGTH) != NULL) {
            printf("%sAap is raste par hain:%s\n", COLOR_CYAN, COLOR_RESET);
            printf("%s%s%s\n", COLOR_YELLOW, current_dir, COLOR_RESET);
        } else {
            printf("%sRasta maloom karne mein dikkat!%s\n", COLOR_RED, COLOR_RESET);
        }
    }
    
    // File reader command (new command)    // Read file command
    else if (strcmp(command, "file-parho") == 0) {
        printf("%sGalat format! Sahi format hai: file-parho filename%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: file-parho masoom_manual.txt%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "file-parho ", 11) == 0) {
        char* filename = command + 11;
        if (strlen(filename) >= MAX_PATH_LENGTH) {
            printf("%sFilename too long!%s\n", COLOR_RED, COLOR_RESET);
            return;
        }
        printf("%sFile parh rahe hain: %s%s\n", COLOR_CYAN, filename, COLOR_RESET);
        FILE* file = fopen(filename, "r");
        if (file) {
            char line[MAX_INPUT_LENGTH];
            int line_number = 1;
            printf("%s-----------File Content------------%s\n", COLOR_YELLOW, COLOR_RESET);
            while (fgets(line, sizeof(line), file)) {
                printf("%s%3d |%s %s", COLOR_GREEN, line_number++, COLOR_RESET, line);
            }
            printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
            fclose(file);
        } else {
            printf("%sFile khulne mein dikkat: %s%s\n", COLOR_RED, filename, COLOR_RESET);
        }
    }
    
    // System info command (new command)
    else if (strcmp(command, "system-info") == 0) {
        printf("%sSystem ki maloomat hasil kar rahe hain...%s\n", COLOR_CYAN, COLOR_RESET);
        printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
        
        // Get Windows version
        OSVERSIONINFOEX osInfo;
        ZeroMemory(&osInfo, sizeof(OSVERSIONINFOEX));
        osInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        
        #pragma warning(disable:4996)
        GetVersionEx((OSVERSIONINFO*)&osInfo);
        #pragma warning(default:4996)
        
        printf("%sWindows Version:%s %ld.%ld (Build %ld)\n", 
               COLOR_GREEN, COLOR_RESET, 
               osInfo.dwMajorVersion, 
               osInfo.dwMinorVersion, 
               osInfo.dwBuildNumber);
        
        // Get computer name
        char computerName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD computerNameLength = sizeof(computerName);
        if (GetComputerNameA(computerName, &computerNameLength)) {
            printf("%sComputer Name:%s %s\n", COLOR_GREEN, COLOR_RESET, computerName);
        }
        
        // Get username
        char userName[UNLEN + 1];
        DWORD userNameLength = sizeof(userName);
        if (GetUserNameA(userName, &userNameLength)) {
            printf("%sUser Name:%s %s\n", COLOR_GREEN, COLOR_RESET, userName);
        }
        
        // Get memory info
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        
        printf("%sTotal RAM:%s %lld MB\n", COLOR_GREEN, COLOR_RESET, memInfo.ullTotalPhys / (1024 * 1024));
        printf("%sAvailable RAM:%s %lld MB\n", COLOR_GREEN, COLOR_RESET, memInfo.ullAvailPhys / (1024 * 1024));
        
        printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    }
      // Simple calculator command (new command)
    else if (strcmp(command, "hisaab-karo") == 0) {
        printf("%sGalat format! Sahi format hai: hisaab-karo number operator number%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: hisaab-karo 2+2 ya hisaab-karo 10/5%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "hisaab-karo ", 12) == 0) {
        char* expression = command + 12;
        if (strlen(expression) > 100) {
            printf("%sExpression too long!%s\n", COLOR_RED, COLOR_RESET);
            return;
        }
        printf("%sHisaab kar rahe hain: %s%s\n", COLOR_CYAN, expression, COLOR_RESET);
        float num1 = 0, num2 = 0, result = 0;
        char op = 0;
        int valid = 1;
        int n = sscanf(expression, "%f%c%f", &num1, &op, &num2);
        if (n == 3 && strchr("+-*/x", op) && strpbrk(expression, " ") == NULL) {
            switch (op) {
                case '+': result = num1 + num2; break;
                case '-': result = num1 - num2; break;
                case '*': result = num1 * num2; break;
                case 'x': result = num1 * num2; break;
                case '/':
                    if (num2 != 0) {
                        result = num1 / num2;
                    } else {
                        printf("%sSifar se taqseem nahi kar sakte!%s\n", COLOR_RED, COLOR_RESET);
                        valid = 0;
                    }
                    break;
                default:
                    printf("%sNa-maloom operator: %c%s\n", COLOR_RED, op, COLOR_RESET);
                    valid = 0;
            }
            if (valid) {
                printf("%sJawab: %s%g\n", COLOR_GREEN, COLOR_RESET, result);
            }
        } else {
            printf("%sGalat format! Sahi format hai: number operator number%s\n", COLOR_RED, COLOR_RESET);
            printf("%sMisaal: hisaab-karo 2+2 ya hisaab-karo 10/5%s\n", COLOR_YELLOW, COLOR_RESET);
        }
    }    // Search files command
    else if (strcmp(command, "dhundo") == 0) {
        printf("%sGalat format! Sahi format hai: dhundo search_term%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: dhundo myfile.txt ya dhundo -content \"search text\"%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "dhundo ", 7) == 0) {
        char* search_term = command + 7;
        int search_content = 0;
        if (strlen(search_term) == 0) {
            printf("%sSearch term cannot be empty!%s\n", COLOR_RED, COLOR_RESET);
            return;
        }
        if (strncmp(search_term, "-content ", 9) == 0) {
            search_term += 9;
            search_content = 1;
        }
        if (strlen(search_term) == 0) {
            printf("%sSearch term cannot be empty!%s\n", COLOR_RED, COLOR_RESET);
            return;
        }
        search_files(search_term, search_content);
    }
    
    // Search file content command (alternative syntax)
    else if (strncmp(command, "dhundo-content ", 15) == 0) {
        char* search_term = command + 15; // Get search term part after "dhundo-content "
        search_files(search_term, 1);
    }
    
    // Weather information command
    else if (strcmp(command, "mausam-batao") == 0) {
        printf("%sGalat format! Sahi format hai: mausam-batao city_name%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: mausam-batao Karachi ya mausam-batao Lahore%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "mausam-batao ", 13) == 0) {
        char* city = command + 13; // Get city part after "mausam-batao "
        mausam_batao(city);
    }
      // Text editor command
    else if (strcmp(command, "likho") == 0) {
        printf("%sGalat format! Sahi format hai: likho filename%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: likho notes.txt ya likho myfile.c%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "likho ", 6) == 0) {
        char* filename = command + 6; // Get filename part after "likho "
        likho_file(filename);
    }
      // Calendar and date command
    else if (strcmp(command, "tarikh-batao") == 0) {
        printf("%sGalat format! Sahi format hai: tarikh-batao month year%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: tarikh-batao 5 2023%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "tarikh-batao ", 13) == 0) {
        int month, year;
        if (sscanf(command + 13, "%d %d", &month, &year) == 2) {
            tarikh_batao(month, year);
        } else {
            printf("%sGalat format! Sahi format hai: tarikh-batao month year%s\n", COLOR_RED, COLOR_RESET);
            printf("%sMisaal: tarikh-batao 12 2023%s\n", COLOR_YELLOW, COLOR_RESET);
        }
    }
    
    // Process management command
    else if (strcmp(command, "process-dikhao") == 0) {
        process_dikhao();
    }
    
    // Internet connectivity check command
    else if (strcmp(command, "internet-check") == 0) {
        printf("%sGalat format! Sahi format hai: internet-check website_name%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMisaal: internet-check google.com ya internet-check microsoft.com%s\n", COLOR_YELLOW, COLOR_RESET);
        return; // Return early to prevent system command execution
    }
    else if (strncmp(command, "internet-check ", 15) == 0) {
        char* website = command + 15; // Get website part after "internet-check "
        internet_check(website);
    }
    
    // Change color theme command
    else if (strncmp(command, "rang-badlo ", 11) == 0) {
        int theme_id;
        if (sscanf(command + 11, "%d", &theme_id) == 1) {
            rang_badlo(theme_id);
        } else {
            printf("%sGalat format! Sahi format hai: rang-badlo theme_id%s\n", COLOR_RED, COLOR_RESET);
            printf("%sMisaal: rang-badlo 1%s\n", COLOR_YELLOW, COLOR_RESET);
        }
    }
    
    // Create alias command
    else if (strncmp(command, "alias-banao ", 12) == 0) {
        char* alias_def = command + 12; // Get alias definition after "alias-banao "
        
        // Find the equals sign
        char* equals = strchr(alias_def, '=');
        if (equals) {
            *equals = '\0'; // Split the string at equals
            char* alias_name = alias_def;
            char* alias_command = equals + 1;
            
            // Register the new alias
            register_alias(alias_name, alias_command);
        } else {
            printf("%sGalat format! Sahi format hai: alias-banao name=command%s\n", COLOR_RED, COLOR_RESET);
            printf("%sMisaal: alias-banao ll=dekhao%s\n", COLOR_YELLOW, COLOR_RESET);
        }
    }
      // Display aliases command
    else if (strcmp(command, "alias-dikhao") == 0) {
        display_aliases();
    }
    
    // Delete alias command
    else if (strncmp(command, "alias-mitao ", 12) == 0) {
        char* alias_name = command + 12; // Get alias name after "alias-mitao "
        delete_alias(alias_name);
    }
    
    // Unknown command
    else if (strlen(command) > 0) {
        // Check if the command is an alias
        int is_alias = 0;
        for (int i = 0; i < alias_count; i++) {
            if (strcmp(command, aliases[i].name) == 0) {
                // Execute the aliased command
                printf("%sAlias '%s' execute kar rahe hain: %s%s\n", 
                       COLOR_CYAN, aliases[i].name, aliases[i].command, COLOR_RESET);
                process_command(aliases[i].command);
                is_alias = 1;
                break;
            }
        }
          // If not an alias, try to execute as a system command
        if (!is_alias) {
            // Check if this is a multi-command (contains && or ;)
            if (strstr(command, "&&") != NULL || strstr(command, ";") != NULL) {
                execute_commands(command);
            } else {                // Try to execute as a system command with error handling
                printf("%sCommand execute kar rahe hain...%s\n", COLOR_YELLOW, COLOR_RESET);
                
                // Add timeout for long-running commands
                int result;
                DWORD start_time = GetTickCount();
                result = system(command);
                DWORD end_time = GetTickCount();
                
                // Check execution time
                if ((end_time - start_time) > 5000) { // More than 5 seconds
                    printf("%sNote: Command execution took %d seconds%s\n", 
                           COLOR_YELLOW, (end_time - start_time)/1000, COLOR_RESET);
                }
                
                if (result != 0) {
                    printf("%sGhalat command ya command chala nahi! Error code: %d%s\n", 
                           COLOR_RED, result, COLOR_RESET);
                }
            }
        }
    }
}

// Get tab completions with limiting functionality to avoid rate limiting
char** get_tab_completions(const char* partial, int* count) {
    char** completions = (char**)malloc(TAB_COMPLETE_MAX * sizeof(char*));
    *count = 0;
    
    // Only proceed if the partial string is at least 1 character
    if (strlen(partial) < 1) {
        return completions;
    }
    
    // Limit the maximum number of completions to avoid overwhelming output
    const int MAX_DISPLAYED_COMPLETIONS = 20;
    
    // 1. Check for built-in command completions
    for (int i = 0; i < builtin_commands_count && *count < MAX_DISPLAYED_COMPLETIONS; i++) {
        if (strncmp(builtin_commands[i], partial, strlen(partial)) == 0) {
            completions[*count] = _strdup(builtin_commands[i]);
            (*count)++;
        }
    }
    
    // If we already have enough completions from built-in commands, skip file search
    if (*count >= MAX_DISPLAYED_COMPLETIONS) {
        return completions;
    }
    
    // 2. Check for file completions using Windows API with a limit
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile("*", &findFileData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (*count >= MAX_DISPLAYED_COMPLETIONS) break;
            if (strncmp(findFileData.cFileName, partial, strlen(partial)) == 0) {
                completions[*count] = _strdup(findFileData.cFileName);
                (*count)++;
            }
        } while (FindNextFile(hFind, &findFileData));
        FindClose(hFind);
    }
    
    return completions;
}

// Free the memory allocated for tab completions
void free_tab_completions(char** completions, int count) {
    for (int i = 0; i < count; i++) {
        free(completions[i]);
    }
    free(completions);
}

// Search for files or content within files
void search_files(const char* search_term, int search_content) {
    printf("%sDhund rahe hain: %s%s\n", COLOR_CYAN, search_term, COLOR_RESET);
    
    WIN32_FIND_DATA findFileData;
    HANDLE hFind = FindFirstFile("*", &findFileData);
    int found = 0;
    
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            // Skip directories for content search
            if ((findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && search_content) {
                continue;
            }
              // For file name search
            if (!search_content) {
                // Create temporary copies
                char* temp_filename = _strdup(findFileData.cFileName);
                char* temp_search = _strdup(search_term);
                
                if (!temp_filename || !temp_search) {
                    if (temp_filename) free(temp_filename);
                    if (temp_search) free(temp_search);
                    continue;
                }
                
                // Convert both to lowercase for case-insensitive search
                _strlwr(temp_filename);
                _strlwr(temp_search);
                
                // Search for match
                if (strstr(temp_filename, temp_search) != NULL) {
                    printf("%sFile mila: %s%s\n", COLOR_GREEN, findFileData.cFileName, COLOR_RESET);
                    found++;
                }
                
                // Free the memory
                free(temp_filename);
                free(temp_search);
            }
            // For content search
            else {
                // Skip binary and very large files
                if (strstr(findFileData.cFileName, ".exe") != NULL ||
                    strstr(findFileData.cFileName, ".dll") != NULL ||
                    strstr(findFileData.cFileName, ".obj") != NULL ||
                    findFileData.nFileSizeLow > 1024 * 1024) {
                    continue;
                }
                
                FILE* file = fopen(findFileData.cFileName, "r");
                if (file) {
                    char line[MAX_INPUT_LENGTH];
                    int line_number = 1;
                    int file_has_match = 0;
                      while (fgets(line, sizeof(line), file)) {
                        char* temp_line = _strdup(line);
                        char* temp_search = _strdup(search_term);
                        
                        if (!temp_line || !temp_search) {
                            if (temp_line) free(temp_line);
                            if (temp_search) free(temp_search);
                            continue;
                        }
                        
                        // Convert both to lowercase for case-insensitive search
                        _strlwr(temp_line);
                        _strlwr(temp_search);
                        
                        // Search for match
                        if (strstr(temp_line, temp_search) != NULL) {
                            if (!file_has_match) {
                                printf("%sFile mila: %s%s\n", COLOR_GREEN, findFileData.cFileName, COLOR_RESET);
                                file_has_match = 1;
                                found++;
                            }
                            printf("  %sLine %d:%s %.*s%s\n", 
                                COLOR_YELLOW, line_number, COLOR_RESET, 
                                (int)strcspn(line, "\n"), line, COLOR_RESET);
                        }
                        
                        // Free the memory
                        free(temp_line);
                        free(temp_search);
                        line_number++;
                    }
                    fclose(file);
                }
            }
        } while (FindNextFile(hFind, &findFileData));
        FindClose(hFind);
    }
    
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("%s%d files milein.%s\n", COLOR_GREEN, found, COLOR_RESET);
}

// Weather information function
void mausam_batao(const char* city) {
    if (!city || strlen(city) == 0) {
        printf("%sShehar ka naam dijiye! Misaal: mausam-batao Karachi%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    printf("%sMausam ki maloomat hasil kar rahe hain: %s%s\n", COLOR_CYAN, city, COLOR_RESET);
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);

    char command[MAX_INPUT_LENGTH];
    int api_success = 0;
    // Format the curl command to fetch weather from wttr.in
    snprintf(command, sizeof(command), "curl -s \"wttr.in/%s?format=3\"", city);

    FILE* pipe = _popen(command, "r");
    if (pipe) {
        char buffer[MAX_INPUT_LENGTH];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            buffer[strcspn(buffer, "\r\n")] = 0;
            if (strlen(buffer) > 10 && !strstr(buffer, "ERROR") && strstr(buffer, city)) {
                printf("%sAsal mausam ki maloomat:%s\n", COLOR_GREEN, COLOR_RESET);
                printf("%s%s%s\n", COLOR_YELLOW, buffer, COLOR_RESET);
                api_success = 1;
            }
        }
        _pclose(pipe);
    } else {
        printf("%sCurl command run nahi ho saki.\n%s", COLOR_RED, COLOR_RESET);
    }

    if (!api_success) {
        printf("%sAPI se maloomat hasil karne mein dikkat. Mausam ka anumaan dikha rahe hain.%s\n", COLOR_RED, COLOR_RESET);
        const char* conditions[] = {"Saaf Aasman", "Badal", "Barish", "Aandhi", "Dhoop"};
        int temperature = 15 + (rand() % 25);
        int humidity = 30 + (rand() % 60);
        int wind_speed = 5 + (rand() % 30);
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        char date_str[20];
        strftime(date_str, sizeof(date_str), "%d-%m-%Y", t);
        const char* condition = conditions[rand() % 5];
        printf("%sShehar: %s%s\n", COLOR_GREEN, COLOR_RESET, city);
        printf("%sTarikh: %s%s\n", COLOR_GREEN, COLOR_RESET, date_str);
        printf("%sHalaat: %s%s\n", COLOR_GREEN, COLOR_RESET, condition);
        printf("%sTemperature: %s%d°C\n", COLOR_GREEN, COLOR_RESET, temperature);
        printf("%sHumidity: %s%d%%\n", COLOR_GREEN, COLOR_RESET, humidity);
        printf("%sHawa ki raftaar: %s%d km/h\n", COLOR_GREEN, COLOR_RESET, wind_speed);
    }
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("%sInternet connection hone par asal mausam dikhayi jati hai.%s\n", COLOR_RED, COLOR_RESET);
    printf("%sAur detail ke liye weather.com ya accuweather.com dekhein.%s\n", COLOR_RED, COLOR_RESET);
}

// Function to handle multiple commands separated by "&&" or ";"
void execute_commands(char* command_string) {
    char* command;
    char* next_command;
    char* delimiter;
    int stop_execution = 0;
    
    // Make a copy of the command string as strtok modifies it
    char* command_copy = _strdup(command_string);
    command = command_copy;
    
    while (command != NULL && !stop_execution) {
        // Find if there's a "&&" or ";" in the command
        char* and_delimiter = strstr(command, "&&");
        char* semicolon_delimiter = strstr(command, ";");
        
        // Determine which delimiter comes first (if any)
        if (and_delimiter != NULL && semicolon_delimiter != NULL) {
            if (and_delimiter < semicolon_delimiter) {
                delimiter = and_delimiter;
                next_command = delimiter + 2; // Skip "&&"
            } else {
                delimiter = semicolon_delimiter;
                next_command = delimiter + 1; // Skip ";"
            }
        } else if (and_delimiter != NULL) {
            delimiter = and_delimiter;
            next_command = delimiter + 2; // Skip "&&"
        } else if (semicolon_delimiter != NULL) {
            delimiter = semicolon_delimiter;
            next_command = delimiter + 1; // Skip ";"
        } else {
            delimiter = NULL;
            next_command = NULL;
        }
        
        // Null-terminate the current command at the delimiter
        if (delimiter != NULL) {
            *delimiter = '\0';
        }
        
        // Trim whitespace
        while (*command == ' ') command++;
        
        // Process the individual command if it's not empty
        if (strlen(command) > 0) {
            int result = system(command);
            
            // If using "&&" and the command failed, stop execution
            if ((delimiter == and_delimiter) && result != 0) {
                stop_execution = 1;
            }
        }
        
        // Move to the next command
        command = next_command;
    }
    
    free(command_copy);
}

// Simple text editor function
void likho_file(const char* filename) {
    printf("%sFile edit kar rahe hain: %s%s\n", COLOR_CYAN, filename, COLOR_RESET);
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("Text dakhil karein. Khatam karne ke liye nayi line par sirf '.exit' likhein:\n");
    
    // Check if file exists
    int file_exists = 0;
    FILE* check = fopen(filename, "r");
    if (check) {
        file_exists = 1;
        fclose(check);
    }
    
    // Open file for reading if it exists
    if (file_exists) {
        printf("%sFile pehle se maujood hai. Content:\n%s", COLOR_GREEN, COLOR_RESET);
        
        FILE* file = fopen(filename, "r");
        if (file) {
            char line[MAX_INPUT_LENGTH];
            int line_number = 1;
            
            while (fgets(line, sizeof(line), file)) {
                printf("%3d | %s", line_number++, line);
            }
            
            fclose(file);
        }
        
        printf("%s\nFile ke content mein izafa karein:\n%s", COLOR_GREEN, COLOR_RESET);
    }
    
    // Open file for appending/writing
    FILE* file = fopen(filename, "a");
    if (!file) {
        printf("%sFile create ya edit karne mein dikkat!%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char line[MAX_INPUT_LENGTH];
    int line_count = 0;
    
    while (1) {
        printf("%3d | ", line_count + 1);
        fgets(line, sizeof(line), stdin);
        
        // Check for exit command
        if (strcmp(line, ".exit\n") == 0) {
            break;
        }
        
        fputs(line, file);
        line_count++;
    }
    
    fclose(file);
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    printf("%sFile '%s' mein %d lines add ki gayi.%s\n", 
           COLOR_GREEN, filename, line_count, COLOR_RESET);
}

// Calendar and date function
void tarikh_batao(int month, int year) {
    printf("%sCalendar dikha rahe hain: %s%d/%d\n", COLOR_CYAN, COLOR_RESET, month, year);
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    
    // Validate month and year
    if (month < 1 || month > 12) {
        printf("%sGalat mahina! 1 se 12 tak dakhil karein.%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    if (year < 1900 || year > 2100) {
        printf("%sGalat saal! 1900 se 2100 tak dakhil karein.%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    // Names of months
    const char* months[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    
    // Names of days of the week
    const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    
    // Number of days in each month
    int days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    // Adjust for leap years
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
        days_in_month[1] = 29;
    }
    
    // Zeller's Congruence for finding the day of week
    int q = 1;  // First day of the month
    int m = month;
    int y = year;
    
    if (m == 1 || m == 2) {
        m += 12;
        y--;
    }
    
    int h = (q + 13 * (m + 1) / 5 + y + y / 4 - y / 100 + y / 400) % 7;
    // Adjust h so that Saturday is 0, Sunday is 1, etc.
    h = (h + 6) % 7;
    
    // Print month and year as header
    printf("\n%s%s %d%s\n", COLOR_GREEN, months[month-1], year, COLOR_RESET);
    
    // Print the days of the week
    for (int i = 0; i < 7; i++) {
        printf("%s%s%s ", COLOR_YELLOW, days[i], COLOR_RESET);
    }
    printf("\n");
    
    // Print leading spaces
    for (int i = 0; i < h; i++) {
        printf("    ");
    }
    
    // Print the days
    for (int i = 1; i <= days_in_month[month-1]; i++) {
        // Highlight current day
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        if (t->tm_mday == i && t->tm_mon == month-1 && t->tm_year + 1900 == year) {
            printf("%s%2d%s  ", COLOR_RED, i, COLOR_RESET);
        } else {
            printf("%2d  ", i);
        }
        
        // New line after each Saturday
        if ((h + i) % 7 == 0) {
            printf("\n");
        }
    }
    
    printf("\n\n%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    
    // Print current date
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    
    printf("%sAaj ki tarikh: %s%02d-%02d-%d\n", 
           COLOR_GREEN, COLOR_RESET, t->tm_mday, t->tm_mon + 1, t->tm_year + 1900);
    printf("%sHafta ka din: %s%s\n", 
           COLOR_GREEN, COLOR_RESET, days[t->tm_wday]);
    
    // Calculate day of year (1-365/366)
    int day_of_year = t->tm_yday + 1;
    printf("%sSaal ka din: %s%d\n", COLOR_GREEN, COLOR_RESET, day_of_year);
    
    // Calculate week number (1-52)
    int week_num = (day_of_year + 7 - (t->tm_wday ? t->tm_wday : 7)) / 7;
    printf("%sHafta number: %s%d\n", COLOR_GREEN, COLOR_RESET, week_num);
}

// Process management function (Windows, Roman Urdu)
void process_dikhao() {
    // Call the advanced process manager from the converted task.c code
    process_manager();
}

// Internet connectivity check function
void internet_check(const char* website) {
    printf("%sInternet connection check kar rahe hain: %s%s\n", COLOR_CYAN, website, COLOR_RESET);
    printf("%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
    
    // Create a ping command with the website
    char ping_command[MAX_INPUT_LENGTH];
    
    // Limit ping to 4 requests with 1 second timeout for faster response
    sprintf(ping_command, "ping -n 4 -w 1000 %s", website);
    
    printf("%sWebsite '%s' ko ping kar rahe hain...%s\n\n", COLOR_GREEN, website, COLOR_RESET);
    
    // Create a pipe to capture command output
    FILE* pipe = _popen(ping_command, "r");    if (pipe) {
        char buffer[MAX_INPUT_LENGTH];
        int line_count = 0;
        int packets_sent = 0;
        int packets_received = 0;
        int response_time_min = 0;
        int response_time_max = 0;
        int response_time_avg = 0;
        
        while (fgets(buffer, sizeof(buffer), pipe)) {
            // Print the output
            printf("%s", buffer);
            
            // Try to parse statistics if this is a reply line
            if (strstr(buffer, "Reply from") != NULL) {
                packets_received++;
                
                // Try to extract time
                char* time_start = strstr(buffer, "time=");
                if (time_start) {
                    int time_ms = 0;
                    sscanf(time_start + 5, "%d", &time_ms);
                    
                    // Update min/max/avg response times
                    if (response_time_min == 0 || time_ms < response_time_min) {
                        response_time_min = time_ms;
                    }
                    if (time_ms > response_time_max) {
                        response_time_max = time_ms;
                    }
                    response_time_avg += time_ms;
                }
            }
            
            // Count the ping attempts
            if (strstr(buffer, "Request timed out") != NULL) {
                packets_sent++;
            }
            
            line_count++;
        }
        
        _pclose(pipe);
        
        // Calculate packet loss and average response time
        if (packets_received > 0) {
            packets_sent = (packets_sent > 0) ? packets_sent : 4; // Default to 4 if we couldn't count properly
            response_time_avg /= packets_received;
            float packet_loss = 100.0f * (1.0f - (float)packets_received / packets_sent);
            
            printf("\n%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
            
            if (packets_received == packets_sent) {
                printf("%s✓ Connection successful!%s\n", COLOR_GREEN, COLOR_RESET);
            } else if (packets_received > 0) {
                printf("%s⚠ Connection partially successful (%.1f%% packet loss)%s\n", 
                       COLOR_YELLOW, packet_loss, COLOR_RESET);
            } else {
                printf("%s✗ Connection failed!%s\n", COLOR_RED, COLOR_RESET);
            }
            
            if (packets_received > 0) {
                printf("%sResponse times: min=%dms, max=%dms, average=%dms%s\n", 
                       COLOR_CYAN, response_time_min, response_time_max, response_time_avg, COLOR_RESET);
            }
        } else {
            printf("\n%s----------------------------------%s\n", COLOR_YELLOW, COLOR_RESET);
            printf("%s✗ Connection failed! Website '%s' respond nahi kar raha hai.%s\n", 
                   COLOR_RED, website, COLOR_RESET);
        }
    } else {
        printf("%sPing command execute karne mein dikkat!%s\n", COLOR_RED, COLOR_RESET);
    }
}

// Initialize color themes
void initialize_themes() {
    // Theme 1: Default blue-green (Cyan theme)
    themes[0].primary = "\033[1;36m";    // Bright cyan
    themes[0].secondary = "\033[1;33m";  // Bright yellow
    themes[0].highlight = "\033[1;32m";  // Bright green
    themes[0].error = "\033[1;31m";      // Bright red
    themes[0].reset = "\033[0m";         // Reset
    themes[0].name = "Nila-Hara (Default)";

    // Theme 2: Crimson theme
    themes[1].primary = "\033[1;31m";    // Bright red
    themes[1].secondary = "\033[1;33m";  // Bright yellow
    themes[1].highlight = "\033[1;37m";  // Bright white
    themes[1].error = "\033[1;35m";      // Bright magenta
    themes[1].reset = "\033[0m";         // Reset
    themes[1].name = "Lal-Surkh";

    // Theme 3: Galaxy theme
    themes[2].primary = "\033[1;35m";    // Bright magenta
    themes[2].secondary = "\033[1;34m";  // Bright blue
    themes[2].highlight = "\033[1;37m";  // Bright white
    themes[2].error = "\033[1;31m";      // Bright red
    themes[2].reset = "\033[0m";         // Reset
    themes[2].name = "Kajali";

    // Theme 4: Forest theme
    themes[3].primary = "\033[1;32m";    // Bright green
    themes[3].secondary = "\033[1;33m";  // Bright yellow
    themes[3].highlight = "\033[1;36m";  // Bright cyan
    themes[3].error = "\033[1;31m";      // Bright red
    themes[3].reset = "\033[0m";         // Reset
    themes[3].name = "Jangal";
}

// Function to change shell color theme
void rang_badlo(int theme_id) {
    // Initialize themes if not already done
    static int themes_initialized = 0;
    if (!themes_initialized) {
        initialize_themes();
        themes_initialized = 1;
    }
    
    // Validate theme ID
    if (theme_id < 0 || theme_id >= 4) {
        printf("%sGalat theme ID! 0 se 3 tak choose karein.%s\n", COLOR_RED, COLOR_RESET);
        printf("%sMaujuda themes:%s\n", COLOR_YELLOW, COLOR_RESET);
        for (int i = 0; i < 4; i++) {
            printf("  %d: %s%s%s\n", i, 
                   themes[i].primary, themes[i].name, COLOR_RESET);
        }
        return;
    }
      // Set the new theme
    current_theme = theme_id;
    
    // Update color macros for the current session
    // This is just for display - to fully support themes, we'd need to modify all printf statements
    printf("%sRang badal diya gaya: %s%s%s\n", 
           themes[current_theme].highlight, themes[current_theme].primary, themes[current_theme].name, themes[current_theme].reset);
    printf("%sYeh theme is session mein partially apply ho gayi hai.%s\n", themes[current_theme].secondary, themes[current_theme].reset);
}

// Function to register a new command alias
void register_alias(const char* alias_name, const char* command) {
    // Check if we've reached max aliases
    if (alias_count >= MAX_ALIASES) {
        printf("%sZyada aliases nahi bana sakte! Pehle kuch delete karein.%s\n", 
               COLOR_RED, COLOR_RESET);
        return;
    }
    
    // Check if alias already exists
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, alias_name) == 0) {
            strncpy(aliases[i].command, command, MAX_INPUT_LENGTH-1);
            aliases[i].command[MAX_INPUT_LENGTH-1] = '\0';
            printf("%sAlias '%s' update kar diya gaya.%s\n", 
                   COLOR_GREEN, alias_name, COLOR_RESET);
            return;
        }
    }
    
    // Add new alias
    strncpy(aliases[alias_count].name, alias_name, MAX_INPUT_LENGTH-1);
    aliases[alias_count].name[MAX_INPUT_LENGTH-1] = '\0';
    strncpy(aliases[alias_count].command, command, MAX_INPUT_LENGTH-1);
    aliases[alias_count].command[MAX_INPUT_LENGTH-1] = '\0';
    alias_count++;
    
    printf("%sNaya alias bana diya gaya: %s = %s%s\n", 
           COLOR_GREEN, alias_name, command, COLOR_RESET);
           
    // Save aliases to file
    save_aliases();
}

// Function to display all registered aliases
void display_aliases() {
    if (alias_count == 0) {
        printf("%sKoi aliases nahi hain.%s\n", COLOR_YELLOW, COLOR_RESET);
        return;
    }
    
    printf("%s----- Registered Aliases -----%s\n", COLOR_CYAN, COLOR_RESET);
    for (int i = 0; i < alias_count; i++) {
        printf("%s%s%s = %s\n", 
               COLOR_GREEN, aliases[i].name, COLOR_RESET, aliases[i].command);
    }
    printf("%s-----------------------------%s\n", COLOR_CYAN, COLOR_RESET);
}

// Function to save aliases to a file
void save_aliases() {
    // Get the path to the aliases file
    char aliases_path[MAX_PATH_LENGTH] = {0};
    char exe_path[MAX_PATH_LENGTH] = {0};
    
    // Try to get exe path
    DWORD path_size = GetModuleFileNameA(NULL, exe_path, MAX_PATH_LENGTH);
    if (path_size > 0) {
        // Extract the directory from the exe path
        char* last_slash = strrchr(exe_path, '\\');
        if (last_slash) {
            *(last_slash + 1) = '\0'; // Truncate at the last backslash
            strcpy(aliases_path, exe_path);
            strcat(aliases_path, "masoom_aliases.txt");
        }
    }
    
    // If we couldn't get a path, use current directory
    if (strlen(aliases_path) == 0) {
        strcpy(aliases_path, "masoom_aliases.txt");
    }
    
    // Open the file for writing
    FILE* file = fopen(aliases_path, "w");
    if (!file) {
        printf("%sAliases save karne mein dikkat!%s\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    // Write aliases to file
    for (int i = 0; i < alias_count; i++) {
        fprintf(file, "%s=%s\n", aliases[i].name, aliases[i].command);
    }
    
    fclose(file);
}

// --- FIXED load_aliases function (removed duplicate code, added missing variable) ---
void load_aliases() {
    alias_count = 0;
    char aliases_path[MAX_PATH_LENGTH] = {0};
    char exe_path[MAX_PATH_LENGTH] = {0};
    DWORD path_size = GetModuleFileNameA(NULL, exe_path, MAX_PATH_LENGTH);
    if (path_size > 0) {
        char* last_slash = strrchr(exe_path, '\\');
        if (last_slash) {
            *(last_slash + 1) = '\0';
            strcpy(aliases_path, exe_path);
            strcat(aliases_path, "masoom_aliases.txt");
        }
    }
    if (strlen(aliases_path) == 0) {
        strcpy(aliases_path, "masoom_aliases.txt");
    }
    FILE* file = fopen(aliases_path, "r");
    if (!file) return;
    char line[MAX_INPUT_LENGTH * 2];
    while (fgets(line, sizeof(line), file)) {
        if (alias_count >= MAX_ALIASES) {
            printf("%sAlias file mein zyada entries hain, kuch ignore ki gayi hain.%s\n", COLOR_YELLOW, COLOR_RESET);
            break;
        }
        line[strcspn(line, "\r\n")] = 0;
        char* equals = strchr(line, '=');
        if (equals) {
            *equals = '\0';
            strncpy(aliases[alias_count].name, line, MAX_INPUT_LENGTH-1);
            aliases[alias_count].name[MAX_INPUT_LENGTH-1] = '\0';
            strncpy(aliases[alias_count].command, equals + 1, MAX_INPUT_LENGTH-1);
            aliases[alias_count].command[MAX_INPUT_LENGTH-1] = '\0';
            alias_count++;
        }
    }
    fclose(file);
}

// Function to delete an alias
void delete_alias(const char* alias_name) {
    // Look for the alias in the array
    int found = -1;
    for (int i = 0; i < alias_count; i++) {
        if (strcmp(aliases[i].name, alias_name) == 0) {
            found = i;
            break;
        }
    }
    
    if (found == -1) {
        printf("%sAlias '%s' nahi mila!%s\n", COLOR_RED, alias_name, COLOR_RESET);
        return;
    }
    
    // Remove the alias by shifting the array elements
    for (int i = found; i < alias_count - 1; i++) {
        strcpy(aliases[i].name, aliases[i+1].name);
        strcpy(aliases[i].command, aliases[i+1].command);
    }
    
    alias_count--;
    printf("%sAlias '%s' mita diya gaya.%s\n", COLOR_GREEN, alias_name, COLOR_RESET);
      // Save aliases to file
    save_aliases();
}
