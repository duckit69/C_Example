#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

// Define basic types
typedef unsigned char BYTE;
typedef unsigned int  DWORD;

// --- Card's Internal Database ---
#define MAX_RECORDS 10
#define MAX_RECORD_SIZE 100
char g_database_records[MAX_RECORDS][MAX_RECORD_SIZE];
int g_record_count = 0;

// --- APDU Structures ---
typedef struct {
    BYTE cla;
    BYTE ins;
    BYTE p1;
    BYTE p2;
    BYTE lc;
    BYTE data[255];
    BYTE le;
} APDU_COMMAND;

typedef struct {
    BYTE data[255];
    int length;
    BYTE sw1;
    BYTE sw2;
} APDU_RESPONSE;

// Command handlers (same as your original code)
void handle_create(APDU_COMMAND* cmd, APDU_RESPONSE* resp) {
    if (g_record_count >= MAX_RECORDS) {
        resp->sw1 = 0x6A; resp->sw2 = 0x84;
        resp->length = 0;
        return;
    }

    if (cmd->lc == 0 || cmd->lc > MAX_RECORD_SIZE - 1) {
        resp->sw1 = 0x67; resp->sw2 = 0x00;
        resp->length = 0;
        return;
    }
    
    int new_record_index = g_record_count;
    memcpy(g_database_records[new_record_index], cmd->data, cmd->lc);
    g_database_records[new_record_index][cmd->lc] = '\0';
    g_record_count++;

    resp->sw1 = 0x90; resp->sw2 = 0x00;
    resp->length = 0;
}

void handle_read(APDU_COMMAND* cmd, APDU_RESPONSE* resp) {
    int record_num = cmd->p1;

    if (record_num < 1 || record_num > g_record_count) {
        resp->sw1 = 0x6A; resp->sw2 = 0x82;
        resp->length = 0;
        return;
    }

    int record_index = record_num - 1;
    int data_len = strlen(g_database_records[record_index]);
    memcpy(resp->data, g_database_records[record_index], data_len);
    resp->length = data_len;
    resp->sw1 = 0x90; resp->sw2 = 0x00;
}

void handle_update(APDU_COMMAND* cmd, APDU_RESPONSE* resp) {
    int record_num = cmd->p1;

    if (record_num < 1 || record_num > g_record_count) {
        resp->sw1 = 0x6A; resp->sw2 = 0x82;
        resp->length = 0;
        return;
    }

    if (cmd->lc == 0 || cmd->lc > MAX_RECORD_SIZE - 1) {
        resp->sw1 = 0x67; resp->sw2 = 0x00;
        resp->length = 0;
        return;
    }

    int record_index = record_num - 1;
    memcpy(g_database_records[record_index], cmd->data, cmd->lc);
    g_database_records[record_index][cmd->lc] = '\0';
    resp->sw1 = 0x90; resp->sw2 = 0x00;
    resp->length = 0;
}

void handle_delete(APDU_COMMAND* cmd, APDU_RESPONSE* resp) {
    int record_num = cmd->p1;

    if (record_num < 1 || record_num > g_record_count) {
        resp->sw1 = 0x6A; resp->sw2 = 0x82;
        resp->length = 0;
        return;
    }

    int record_index = record_num - 1;
    for (int i = record_index; i < g_record_count - 1; i++) {
        memcpy(g_database_records[i], g_database_records[i+1], MAX_RECORD_SIZE);
    }
    
    g_record_count--;
    g_database_records[g_record_count][0] = '\0';
    resp->sw1 = 0x90; resp->sw2 = 0x00;
    resp->length = 0;
}

// --- Interactive Functions ---
void print_database() {
    printf("\n=== Current Database (%d/%d records) ===\n", g_record_count, MAX_RECORDS);
    if (g_record_count == 0) {
        printf("Database is empty\n");
    } else {
        for (int i = 0; i < g_record_count; i++) {
            printf("Record %d: '%s'\n", i+1, g_database_records[i]);
        }
    }
    printf("====================================\n");
}

void print_response(APDU_RESPONSE* resp) {
    printf("Card Response: ");
    if (resp->length > 0) {
        printf("Data='");
        for (int i = 0; i < resp->length; i++) {
            printf("%c", resp->data[i]);
        }
        printf("' ");
    }
    printf("Status=0x%02X%02X", resp->sw1, resp->sw2);
    
    // Show status meaning
    if (resp->sw1 == 0x90 && resp->sw2 == 0x00) {
        printf(" (SUCCESS)");
    } else if (resp->sw1 == 0x6A && resp->sw2 == 0x82) {
        printf(" (RECORD NOT FOUND)");
    } else if (resp->sw1 == 0x6A && resp->sw2 == 0x84) {
        printf(" (DATABASE FULL)");
    } else if (resp->sw1 == 0x67 && resp->sw2 == 0x00) {
        printf(" (WRONG LENGTH)");
    }
    printf("\n");
}

void create_apdu_command(APDU_COMMAND* cmd, BYTE ins, BYTE p1, const char* data) {
    cmd->cla = 0x80;
    cmd->ins = ins;
    cmd->p1 = p1;
    cmd->p2 = 0x00;
    
    if (data != NULL) {
        cmd->lc = strlen(data);
        memcpy(cmd->data, data, cmd->lc);
    } else {
        cmd->lc = 0;
    }
    cmd->le = 0;
}

void print_help() {
    printf("\nAvailable Commands:\n");
    printf("  CREATE <data>           - Create new record with data\n");
    printf("  READ <record_num>       - Read data from record number\n");
    printf("  UPDATE <record_num> <data> - Update record with new data\n");
    printf("  DELETE <record_num>     - Delete record\n");
    printf("  LIST                    - Show all records\n");
    printf("  HELP                    - Show this help\n");
    printf("  EXIT                    - Exit simulator\n");
    printf("\nExamples:\n");
    printf("  CREATE Hello World\n");
    printf("  READ 1\n");
    printf("  UPDATE 1 New Data\n");
    printf("  DELETE 1\n");
}

int parse_command(char* input, APDU_COMMAND* cmd) {
    char command[20];
    char data[256] = {0};
    int record_num = 0;
    
    // Parse the command
    if (sscanf(input, "%19s", command) != 1) {
        return 0;
    }
    
    // Convert to uppercase for case-insensitive comparison
    for (int i = 0; command[i]; i++) {
        command[i] = toupper(command[i]);
    }
    
    if (strcmp(command, "CREATE") == 0) {
        // Get the data (everything after "CREATE ")
        char* data_start = input + 7; // Skip "CREATE "
        while (*data_start == ' ') data_start++; // Skip extra spaces
        
        if (strlen(data_start) == 0) {
            printf("Error: CREATE requires data\n");
            return 0;
        }
        
        create_apdu_command(cmd, 0x01, 0x00, data_start);
        return 1;
    }
    else if (strcmp(command, "READ") == 0) {
        if (sscanf(input, "%*s %d", &record_num) != 1) {
            printf("Error: READ requires record number\n");
            return 0;
        }
        create_apdu_command(cmd, 0x02, record_num, NULL);
        return 1;
    }
    else if (strcmp(command, "UPDATE") == 0) {
        // Parse record number and data
        char* rest = input + 6; // Skip "UPDATE"
        while (*rest == ' ') rest++; // Skip spaces
        
        // Find the record number
        if (sscanf(rest, "%d", &record_num) != 1) {
            printf("Error: UPDATE requires record number and data\n");
            return 0;
        }
        
        // Find the data (skip past record number)
        while (*rest && *rest != ' ') rest++; // Skip record number
        while (*rest == ' ') rest++; // Skip spaces
        
        if (strlen(rest) == 0) {
            printf("Error: UPDATE requires data\n");
            return 0;
        }
        
        create_apdu_command(cmd, 0x03, record_num, rest);
        return 1;
    }
    else if (strcmp(command, "DELETE") == 0) {
        if (sscanf(input, "%*s %d", &record_num) != 1) {
            printf("Error: DELETE requires record number\n");
            return 0;
        }
        create_apdu_command(cmd, 0x04, record_num, NULL);
        return 1;
    }
    else if (strcmp(command, "LIST") == 0) {
        print_database();
        return 2; // Special return code for LIST command
    }
    else if (strcmp(command, "HELP") == 0) {
        print_help();
        return 2; // Special return code for HELP command
    }
    else if (strcmp(command, "EXIT") == 0) {
        return -1;
    }
    else {
        printf("Error: Unknown command '%s'. Type HELP for available commands.\n", command);
        return 0;
    }
}

int main() {
    printf("=== Interactive Smart Card Simulator ===\n");
    
    APDU_COMMAND cmd;
    APDU_RESPONSE resp;
    char input[256];
    
    // Initialize database
    for (int i = 0; i < MAX_RECORDS; i++) {
        g_database_records[i][0] = '\0';
    }
    g_record_count = 0;
    
    while (1) {
        printf("\nCARD> ");
        // Clean buffer for next usage
        fflush(stdout);
        
        // Get user input
        // NOTE: fgets takes input size and throw the rest
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Remove newline character
        input[strcspn(input, "\n")] = 0;
        
        // Skip empty lines
        if (strlen(input) == 0) {
            // handle empty input
            continue;
        }
        
        // Parse and execute command
        int result = parse_command(input, &cmd);
        
        if (result == -1) {
            printf("Goodbye!\n");
            break;
        } else if (result == 1) {
            // Valid APDU command - process it
            resp.length = 0;
            resp.sw1 = 0x00;
            resp.sw2 = 0x00;
            
            switch (cmd.ins) {
                case 0x01: handle_create(&cmd, &resp); break;
                case 0x02: handle_read(&cmd, &resp); break;
                case 0x03: handle_update(&cmd, &resp); break;
                case 0x04: handle_delete(&cmd, &resp); break;
                // default to be added 
            }
            
            print_response(&resp);
        }
    }
    
    return 0;
}
