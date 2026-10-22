#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

#define TABLE_SIZE 256

/**
 * This structure defines a key-value map entry for a hashmap.
 */
typedef struct MapEntry {
    char *key;
    char *value;
} MapEntry;

/**
 * Definition of a hashmap with multiple map entries.
 */
typedef struct HashMap {
    MapEntry *entry;
} HashMap;

/**
 * Hash function for a corresponding key value.
 */
int hash(char *key) {
    int hash = 0;
    while (*key) {
        hash = hash * 31 + *key;
        key++;
    }
    return hash % TABLE_SIZE;
}

/**
 * Inserts an entry into a hashmap with a specific key and a value.
 */
int put(HashMap map, char *key, char *value) {
    int index = hash(key);
    map.entry[index].key = strdup(key);
    map.entry[index].value = strdup(value);;
    return index;
}

/**
 * Gets the value of a key for a hashmap.
 */
char *get(HashMap map, char *key) {
    int index = hash(key);
    if (map.entry[index].value == NULL) {
        return "NULL";
    }
    return map.entry[index].value;
}

/**
 * Writes all the values to a disk.
 */
void write_values_to_disk(HashMap map) {
    int i = 0;
    FILE *file = fopen("hashmap.dat", "wb");
    for (i = 0; i < TABLE_SIZE; i++) {
        if (map.entry[i].key != NULL) {
            fprintf(file, "%s %d %s\n", map.entry[i].key, hash(map.entry[i].key), map.entry[i].value);
        }
    }
    fclose(file);
}

/**
 * Read all the values form disk and load them into memory.
 */
void read_values_from_disk(HashMap map) {
    int i = 0;
    FILE *file = fopen("hashmap.dat", "rb");
    if (file != NULL) { 
        char key[256];
        int index;
        char value[256];
        while (fscanf(file, "%255s %d %255s", key, &index, value) == 3) {
            put(map, key, value);
        }
    }
}

HashMap map;

/**
 * Write the values to the disk in case of an application crash. Exit the program
 */
void handle_siginterrupt(int signal) {
    write_values_to_disk(map);
    exit(1);
}

int main() {

    // Commands to handle unexpected failures.s
    signal(SIGINT, handle_siginterrupt);
    signal(SIGTERM, handle_siginterrupt);
    signal(EOF, handle_siginterrupt);

    map.entry = calloc(TABLE_SIZE, sizeof(MapEntry));
    read_values_from_disk(map);
    while (1)
    {
        char command[256];
        printf("$:");
        fgets(command, sizeof(command), stdin);
        if (strlen(command) > 255) {
            printf("cachedb: Command too long. Maximum length is 256 characters.\n");
            continue;;
        }
        char *operation = strtok(command, " \t");

        if (strcmp(operation, "write") == 0) {

            char *key = strtok(NULL, " \t\n");
            char *value = strtok(NULL, " \t\n");
            put(map, key, value);
            printf("cachedb: Key successfully added.\n");

        } else if (strcmp(operation, "read") == 0) {

            char *key = strtok(NULL, " \t\n");
            printf("cachedb: Value retrieved: %s\n", get(map, key));

        } else if (strcmp(operation, "quit") == 0) {
            char *key = strtok(NULL, " \t\n");
            if (strcmp(key, "db") == 0) {
                write_values_to_disk(map);
                break;
            }
            break;
        } else {
            printf("cachedb: Invalid command.\n");
        }
    }
    return 0;
}