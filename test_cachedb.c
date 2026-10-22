/**
 * Unit tests for cachedb.
 *
 * The production program lives in a single translation unit with main(), so
 * this file includes it after renaming main and exercises the hashmap and
 * persistence functions directly.
 *
 * Build and run:
 *   make test
 *   gcc -o test_cachedb test_cachedb.c && ./test_cachedb
 */

#define main cachedb_main
#include "sample-database.c"
#undef main

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int tests_run;
static int tests_failed;

#define EXPECT(cond, fmt, ...)                                                 \
    do {                                                                       \
        tests_run++;                                                           \
        if (!(cond)) {                                                         \
            tests_failed++;                                                    \
            fprintf(stderr, "  FAIL %s:%d: " fmt "\n", __func__, __LINE__,     \
                    ##__VA_ARGS__);                                            \
        }                                                                      \
    } while (0)

#define EXPECT_STREQ(actual, expected)                                         \
    EXPECT((actual) != NULL && (expected) != NULL &&                           \
               strcmp((actual), (expected)) == 0,                              \
           "expected \"%s\", got \"%s\"", expected,                            \
           (actual) == NULL ? "(null)" : (actual))

static HashMap make_map(void) {
    HashMap test_map;
    test_map.entry = calloc(TABLE_SIZE, sizeof(MapEntry));
    EXPECT(test_map.entry != NULL, "calloc should succeed");
    return test_map;
}

static void free_map(HashMap test_map) {
    int i;
    if (test_map.entry == NULL) {
        return;
    }
    for (i = 0; i < TABLE_SIZE; i++) {
        free(test_map.entry[i].key);
        free(test_map.entry[i].value);
    }
    free(test_map.entry);
}

static void enter_temp_dir(char *original_cwd, size_t cwd_size, char *testdir,
                           size_t testdir_size) {
    if (getcwd(original_cwd, cwd_size) == NULL) {
        perror("getcwd");
        exit(2);
    }
    snprintf(testdir, testdir_size, "/tmp/cachedb-test-XXXXXX");
    if (mkdtemp(testdir) == NULL) {
        perror("mkdtemp");
        exit(2);
    }
    if (chdir(testdir) != 0) {
        perror("chdir");
        exit(2);
    }
}

static void leave_temp_dir(const char *original_cwd, const char *testdir) {
    unlink("hashmap.dat");
    if (chdir(original_cwd) != 0) {
        perror("chdir");
        exit(2);
    }
    rmdir(testdir);
}

static void test_hash_is_in_table_range(void) {
    const char *keys[] = {"", "a", "key1", "cachedb", "12345"};
    size_t i;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        int index = hash((char *)keys[i]);
        EXPECT(index >= 0 && index < TABLE_SIZE,
               "hash(\"%s\") = %d is outside [0, %d)", keys[i], index,
               TABLE_SIZE);
    }
}

static void test_hash_is_deterministic(void) {
    char *key = "repeat-me";
    EXPECT(hash(key) == hash(key), "hash should be stable for the same key");
}

static void test_hash_empty_string(void) {
    EXPECT(hash("") == 0, "empty string should hash to 0");
}

static void test_put_returns_hash_index(void) {
    HashMap test_map = make_map();
    char *key = "alpha";
    int index = put(test_map, key, "1");
    EXPECT(index == hash(key), "put should return hash(key)");
    free_map(test_map);
}

static void test_put_get_roundtrip(void) {
    HashMap test_map = make_map();
    put(test_map, "name", "cachedb");
    EXPECT_STREQ(get(test_map, "name"), "cachedb");
    free_map(test_map);
}

static void test_get_missing_key_returns_null_token(void) {
    HashMap test_map = make_map();
    EXPECT_STREQ(get(test_map, "missing"), "NULL");
    free_map(test_map);
}

static void test_put_overwrites_same_key(void) {
    HashMap test_map = make_map();
    put(test_map, "k", "first");
    put(test_map, "k", "second");
    EXPECT_STREQ(get(test_map, "k"), "second");
    free_map(test_map);
}

static void test_put_stores_independent_copies(void) {
    HashMap test_map = make_map();
    char key[] = "city";
    char value[] = "seattle";
    put(test_map, key, value);
    key[0] = 'x';
    value[0] = 'x';
    EXPECT_STREQ(get(test_map, "city"), "seattle");
    free_map(test_map);
}

static int find_collision(char *first, char *second, size_t bufsize) {
    int seen[TABLE_SIZE];
    char keys[TABLE_SIZE][16];
    int i;
    memset(seen, -1, sizeof(seen));
    for (i = 0; i < 4096; i++) {
        char candidate[16];
        int index;
        snprintf(candidate, sizeof(candidate), "k%d", i);
        index = hash(candidate);
        if (index < 0) {
            continue;
        }
        if (seen[index] >= 0) {
            snprintf(first, bufsize, "%s", keys[index]);
            snprintf(second, bufsize, "%s", candidate);
            return 1;
        }
        seen[index] = i;
        snprintf(keys[index], sizeof(keys[index]), "%s", candidate);
    }
    return 0;
}

static void test_collision_overwrites_bucket(void) {
    HashMap test_map = make_map();
    char first[16];
    char second[16];
    EXPECT(find_collision(first, second, sizeof(first)),
           "should find two keys that hash to the same bucket");
    put(test_map, first, "one");
    put(test_map, second, "two");
    EXPECT_STREQ(get(test_map, first), "two");
    EXPECT_STREQ(get(test_map, second), "two");
    free_map(test_map);
}

static void test_write_creates_hashmap_dat(void) {
    char original_cwd[PATH_MAX];
    char testdir[64];
    HashMap test_map = make_map();

    enter_temp_dir(original_cwd, sizeof(original_cwd), testdir, sizeof(testdir));
    put(test_map, "name", "cachedb");
    write_values_to_disk(test_map);
    EXPECT(access("hashmap.dat", F_OK) == 0, "hashmap.dat should exist: %s",
           strerror(errno));
    leave_temp_dir(original_cwd, testdir);
    free_map(test_map);
}

static void test_write_uses_key_hash_value_format(void) {
    char original_cwd[PATH_MAX];
    char testdir[64];
    char line[256];
    char key[256];
    int stored_hash;
    char value[256];
    FILE *file;
    HashMap test_map = make_map();

    enter_temp_dir(original_cwd, sizeof(original_cwd), testdir, sizeof(testdir));
    put(test_map, "name", "cachedb");
    write_values_to_disk(test_map);

    file = fopen("hashmap.dat", "r");
    EXPECT(file != NULL, "should open hashmap.dat");
    if (file != NULL) {
        EXPECT(fgets(line, sizeof(line), file) != NULL, "file should have a line");
        EXPECT(sscanf(line, "%255s %d %255s", key, &stored_hash, value) == 3,
               "line should be 'key hash value'");
        EXPECT_STREQ(key, "name");
        EXPECT_STREQ(value, "cachedb");
        EXPECT(stored_hash == hash("name"), "stored hash should match hash(key)");
        fclose(file);
    }

    leave_temp_dir(original_cwd, testdir);
    free_map(test_map);
}

static void test_persist_roundtrip_multiple_keys(void) {
    char original_cwd[PATH_MAX];
    char testdir[64];
    HashMap writer = make_map();
    HashMap reader = make_map();

    enter_temp_dir(original_cwd, sizeof(original_cwd), testdir, sizeof(testdir));
    put(writer, "key1", "value1");
    put(writer, "key2", "value2");
    put(writer, "key3", "value3");
    write_values_to_disk(writer);

    read_values_from_disk(reader);
    EXPECT_STREQ(get(reader, "key1"), "value1");
    EXPECT_STREQ(get(reader, "key2"), "value2");
    EXPECT_STREQ(get(reader, "key3"), "value3");

    leave_temp_dir(original_cwd, testdir);
    free_map(writer);
    free_map(reader);
}

static void test_read_missing_file_leaves_map_empty(void) {
    char original_cwd[PATH_MAX];
    char testdir[64];
    HashMap test_map = make_map();

    enter_temp_dir(original_cwd, sizeof(original_cwd), testdir, sizeof(testdir));
    read_values_from_disk(test_map);
    EXPECT_STREQ(get(test_map, "anything"), "NULL");
    leave_temp_dir(original_cwd, testdir);
    free_map(test_map);
}

int main(void) {
    printf("cachedb unit tests\n");

    test_hash_is_in_table_range();
    test_hash_is_deterministic();
    test_hash_empty_string();
    test_put_returns_hash_index();
    test_put_get_roundtrip();
    test_get_missing_key_returns_null_token();
    test_put_overwrites_same_key();
    test_put_stores_independent_copies();
    test_collision_overwrites_bucket();
    test_write_creates_hashmap_dat();
    test_write_uses_key_hash_value_format();
    test_persist_roundtrip_multiple_keys();
    test_read_missing_file_leaves_map_empty();

    if (tests_failed == 0) {
        printf("%d checks passed\n", tests_run);
        return 0;
    }
    printf("%d/%d checks failed\n", tests_failed, tests_run);
    return 1;
}
