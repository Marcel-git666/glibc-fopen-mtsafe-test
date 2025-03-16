# glibc-fopen-mtsafe-test

## Overview
This repository contains a test designed to verify that the `fopen()` function in the GNU C Library (glibc) is MT-Safe (thread-safe). Thread-safety ensures that a function can be safely called concurrently from multiple threads without causing race conditions or other synchronization issues.

## Test Description
The test creates multiple threads that concurrently perform file operations using `fopen()`, including:
- Opening files for writing
- Writing thread-specific data
- Testing proper `errno` handling
- Reopening files for reading
- Verifying file contents

Each thread runs multiple iterations to maximize the chances of exposing potential thread-safety issues.

## Implementation Details
- Uses POSIX threads (`pthread`)
- Synchronizes thread start using barriers
- Performs careful error checking and reporting
- Validates thread isolation through file content verification

## Files
- `tst-fopen-mtsafe.c`: The main test implementation
- `PURPOSE`: Describes the test objectives and approach
- `PROGRESS`: Chronicles the development process and decisions

## Usage
This test can be built either as a standalone program or integrated into the glibc test suite.

## Troubleshooting with ThreadSanitizer
When running with ThreadSanitizer, use:
```bash
setarch $(uname -m) -R ./tst-fopen-mtsafe
```
This command helps address potential memory mapping issues with sanitizers.

### Standalone Build
```bash
make
./tst-fopen-mtsafe
```

## Within glibc
Place the test files in the appropriate directory (usually libio/) and run:
```bash
make check test=libio/tst-fopen-mtsafe
```

## Expected Results
If fopen() is truly MT-Safe, all threads should be able to perform their operations without interference, and the test will report success.

