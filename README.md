# SQL Engine (SillyQL)

This project is a **simplified SQL interpreter** written in C++.
It implements a subset of SQL functionality, allowing the creation of
tables, insertion of rows, filtering, joining, and indexed lookups.

------------------------------------------------------------------------

## Features

-   **Table Management**
    -   Create tables with named columns and specified data types
    -   Insert rows of values into tables
-   **Query Execution**
    -   `PRINT` -- display table contents
    -   `PRINT ... WHERE` -- filter rows based on conditions
    -   `DELETE` -- remove rows matching conditions
    -   `JOIN` -- join two tables on a shared column
    -   `GENERATE` -- create indices for fast lookup
    -   `REMOVE` -- drop indices
-   **Indexing**
    -   **Hash index** -- constant-time lookups for equality searches
    -   **BST index** -- ordered lookups for range queries
-   **Error Handling**
    -   Custom error messages defined in `Error_messages.txt`
    -   Detects invalid commands, duplicate tables, type mismatches,
        etc.

------------------------------------------------------------------------

## Repository Structure

    SQL-Engine-main/
    ├── Field.cpp / Field.h        # Field class representing typed values
    ├── silly.cpp                  # Main engine implementation
    ├── Makefile                   # Build instructions
    ├── Error_messages.txt         # Reference of engine error outputs
    ├── *.txt                      # Test inputs and expected outputs
    │   ├── cp1-in.txt / cp1-out.txt
    │   ├── cp2-in.txt / cp2-out.txt
    │   ├── join-input.txt / join-output.txt
    │   ├── spec-input.txt / spec-output.txt
    │   ├── test-*.txt (various unit tests)
    │   └── outputcorrect.txt
    └── Files-README.md            # Additional documentation

------------------------------------------------------------------------

## Building

Compile the engine with the provided `Makefile`:

``` bash
make
```

This produces an executable (commonly named `silly`).

------------------------------------------------------------------------

## Usage

Run the engine with an input file containing SQL-like commands:

``` bash
./silly < test-1.txt
```

To compare outputs against reference files:

``` bash
./silly < cp1-in.txt > myout.txt
diff myout.txt cp1-out.txt
```

------------------------------------------------------------------------

## Example

Input (`join-input.txt`):

    CREATE TABLE students (id int, name string);
    INSERT INTO students VALUES (1, "Alice");
    INSERT INTO students VALUES (2, "Bob");

    CREATE TABLE grades (id int, grade string);
    INSERT INTO grades VALUES (1, "A");
    INSERT INTO grades VALUES (2, "B");

    JOIN students AND grades ON id;

Output (`join-output.txt`):

    id name grade
    1 Alice A
    2 Bob   B

------------------------------------------------------------------------

## Testing

-   `spec-input.txt` / `spec-output.txt`: Specification-level tests
-   `cp*-in.txt` / `cp*-out.txt`: Checkpoint outputs for grading
-   `test-*.txt`: Additional scenarios covering edge cases

Run tests manually or redirect outputs into `.out.txt` files for diff
comparison.
