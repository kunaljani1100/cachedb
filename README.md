# cachedb

A small in-memory key-value store written in C. Values live in a hashmap and can be persisted to `hashmap.dat` on disk.

## Requirements

- A C compiler (for example `gcc` or `clang`)
- POSIX environment (`signal`, `unistd`)

## Build

```bash
gcc -o sample-database sample-database.c
```

## Run

```bash
./sample-database
```

The program prints a `$:` prompt and waits for commands on stdin.

## Commands

| Command | Description |
| --- | --- |
| `write <key> <value>` | Store a key-value pair in memory. |
| `read <key>` | Print the value for a key, or `NULL` if it is missing. |
| `quit db` | Flush the map to `hashmap.dat` and exit. |
| `quit` | Exit without writing to disk. |

Keys and values are single tokens (no spaces). Maximum command length is 256 characters.

Example session:

```text
$:write name cachedb
cachedb: Key successfully added.
$:read name
cachedb: Value retrieved: cachedb
$:quit db
```

## Persistence

On startup, cachedb loads existing entries from `hashmap.dat` if that file is present.

The map is written to disk when:

- You run `quit db`
- The process receives `SIGINT` or `SIGTERM` (for example Ctrl+C)

Each persisted line is `key hash value`.

## Implementation notes

- The hashmap has a fixed size of 256 buckets.
- Hashing uses a polynomial rolling hash (`hash * 31 + char`) modulo table size.
- Colliding keys overwrite the same bucket; there is no chaining or open addressing.
- Interrupt handlers write the current map to disk before exiting.
