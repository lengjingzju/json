# LJSON: The Versatile, High-Performance, Truly Streaming C Language JSON and Numerical Engine

**Translated by Deepseek**

## 1. Project Positioning & Design Philosophy

LJSON was born in October 2019, one year earlier than yyjson. Its original intention was not merely to pursue extreme parsing speed in benchmarks, but to find the optimal balance between **performance, memory usage, editability, streaming capability, and maintainability**.

Its current state is:

-   **Able to match or even surpass yyjson's performance in most scenarios**
-   **Provides true streaming processing capabilities that the yyjson architecture cannot easily implement**
-   **Maintains code readability and extensibility, suitable for long-term evolution**

## 2. Core Capabilities

Dual Core

### Versatile JSON Processing

-   Full support for JSON5 specification (hexadecimal, comments, trailing commas, single quotes, etc.)
-   DOM / SAX dual parsing modes
-   True streaming file processing (parse while reading, print while writing)

### High-Performance Numerical Conversion Engine

-   Innovative **ldouble** algorithm
-   Floating-point ↔ string conversion performance far exceeds standard library and mainstream algorithms (sprintf, grisu2, dragonbox)
-   Precision is 16 digits, pursuing the shortest representation (rather than even) in boundary cases (the standard is 16 or 17 digits, depending on the multiplier's resolution when converting binary exponents to decimal exponents).

## 3. Architecture & Mode Design

LJSON provides **7 parsing modes** and **4 printing modes**, covering full-scenario needs from memory to file, and from DOM to SAX:

### Parsing Modes

1.  DOM Classic Mode (uses malloc/free)
2.  DOM Memory Pool Mode
3.  DOM Reuse Mode (Editable, in-place string reuse)
4.  DOM File Streaming Mode (True streaming)
5.  DOM File Streaming Memory Pool Mode (True streaming + memory pool)
6.  SAX Mode (Callback-based processing)
7.  SAX File Streaming Mode (True streaming + callbacks)

### Printing Modes

1.  DOM → String
2.  DOM → File (True streaming)
3.  SAX → String
4.  SAX → File (True streaming)

> **True Streaming**: Parse while reading the file, print while writing to the file. No need to read the entire file or generate large intermediate buffers. Memory usage can be reduced to a constant level; even processing a 1GB JSON file may require only KBs of memory.

**yyjson ≈ LJSON's DOM Reuse Mode + Aggressive Read-Only Optimization**

## 4. Design Differences Compared to yyjson

Core differences include:

| Dimension           | LJSON                                                                 | yyjson                                                               |
| ------------------- | --------------------------------------------------------------------- | -------------------------------------------------------------------- |
| **Mode Design**     | Multi-Mode (incl. True Streaming, Editable Reuse)                     | Single Reuse Mode (Read-only optimized)                              |
| **Editability**     | Reuse mode is editable                                                | Read-only and editable strictly separated (val / mut_val)            |
| **String Storage**  | Standard C strings (1 trailing `\0`)                                  | Non-standard (4 trailing `\0`s) (reduces boundary checks)            |
| **Object Storage**  | Keys and values stored together                                       | Keys and values stored separately                                    |
| **Memory Strategy** | On-demand allocation, minimizes waste                                 | Large block pre-allocation, high redundancy                          |
| **Access Speedup**  | `json_items_t` cache, Array O(1), Object O(logN) lookup               | Cache mechanism unknown, likely O(N)/O(2N)                           |
| **Optimization**    | Inlining, branch prediction, lookup tables, cached meta-info; Readable | Similar optimizations + heavy macro loop unrolling; Opaque code      |
| **Streaming**       | True streaming parse/print                                            | Not supported                                                        |

## 5. Performance Sources & Trade-offs

### Optimization Technique Comparison

Both use:

-   Algorithm optimization
-   Memory pool (block) optimization
-   Inlining optimization (inline)
-   Branch prediction optimization (likely/unlikely)
-   Cache hit optimization
-   Lookup table optimization
-   Copy optimization
-   Information recording optimization (caching string lengths, etc.)

Differences:

-   **yyjson**: Heavy use of macros for manual loop unrolling, code is compact but obscure, performance is more aggressive; uses non-standard features (4 trailing `\0`s, unaligned access, etc.).
-   **LJSON**: Pursues an engineering balance of maintainability, extensibility, low footprint, and high performance. Maintains readability; uses macros more selectively.

### Performance Comparison

Yyjson essentially only implements **LJSON's DOM Reuse Mode**, and when converting to editable mode, its objects and strings are stored in memory blocks, similar to LJSON's memory pool principle.

-   **yyjson read-only reuse mode** performance is slightly higher than LJSON's, due to:
    -   Single mode focus
    -   Aggressive resource usage (pre-allocating redundant memory)
    -   Non-standard features (4 trailing `\0`s, unaligned access)
    -   Macro loop unrolling
-   **yyjson read-only mode** performance is very close to LJSON's, with wins/losses depending on the case
-   **yyjson editable mode** performance can be significantly slower than LJSON's (**for large files, yyjson uses ~2x the memory and is ~2x slower than LJSON for parsing into editable structures**)

Note: If LJSON abandoned streaming, standard strings, and editability, and adopted more obscure code, it could achieve **yyjson read-only reuse mode** performance, but that would no longer align with LJSON's design philosophy.

## 6. Optimization Mechanism Highlights

-   **Zero Heap Allocation**: Initial allocation followed by cyclic reuse, avoiding frequent malloc/free.
-   **In-place String Reuse**: Directly references the input buffer, reducing copy operations and memory bandwidth consumption.
-   **json_items Accelerator**: Array O(1) access, Object O(logN) lookup.
-   **True Streaming Pipeline**: Parsing and I/O happen in parallel, eliminating peak memory usage and extra copies.
-   **ldouble Numerical Engine**: Floating-point to string conversion performance leads sprintf by 10~70x.

## 7. Multi-Dimensional Comparison (Editable Mode)

| Comparison Dimension | LJSON (High, 547ms, Editable Reuse) | yyjson (Medium-High, 1011ms, Editable) | RapidJSON (Relatively High) | cJSON (Medium) |
| -------------------- | ----------------------------------- | -------------------------------------- | --------------------------- | -------------- |
| **Parse Performance**  | High (547ms, Editable Reuse)        | Medium-High (1011ms, Editable)         | Relatively High             | Medium         |
| **Print Performance**  | High (True Streaming + ldouble)     | High (Full Buffering)                  | Medium                      | Low            |
| **Memory Usage**      | Can be constant (Streaming)         | Relatively High                        | Relatively High             | Relatively High |
| **CPU Feature Deps.** | Low                                 | Low                                    | Medium                      | Low            |
| **Extensibility**     | High                                | Low                                    | Medium                      | High           |
| **Code Readability**  | High                                | Very Low                               | Medium                      | High           |
| **Key Mechanisms**    | Zero heap alloc, mem pool reuse, in-place strings, json_items, ldouble | In-place reuse, memory blocks, non-standard string tail, loop unrolling | Templates+allocators        | malloc/free    |

*(Note: Parse/Print performance notes integrated into first row for clarity)*

## 8. Applicable Scenarios

-   **Large File Processing**: Formatting/compression of GB-level JSON files, memory usage can be constant.
-   **Embedded Systems**: Low memory, high performance requirements.
-   **High-Frequency Read/Write**: Real-time data stream parsing and generation.
-   **Cross-Platform Development**: Linux / Windows / Embedded RTOS.

## 9. Design Philosophy Summary

> **yyjson**: Takes extreme performance as the primary goal, achieved through aggressive optimization techniques and deep focus on a single mode. Suitable for extreme read-only parsing scenarios.
> **LJSON**: Pursues an engineering balance of maintainability, extensibility, low footprint, and high performance. Suitable for general-purpose, editable, streaming processing, and large file/low memory usage scenarios. Pure C implementation, zero third-party dependencies, cross-platform compilation, interface design references cJSON, clear logic, fast compilation speed, easy for secondary development.

LJSON's value lies not only in benchmark scores but also in its ability to stably support diverse JSON processing needs in real engineering environments with extremely low memory usage and high performance.
