# yyjson ≠ The Universal Champion: It's Just a Specialized Instance of LJSON's DOM Reuse Mode

*— A deep dialogue on the architectural philosophy of JSON engines*

**Translated by Deepseek**

## Introduction: Beyond Performance Lies the Boundary of Architecture

In the world of C language JSON libraries, yyjson has become a frequent contender in benchmarks due to its astonishing parsing speed. But if you are a systems-level engineer seeking not just benchmark scores but architectural controllability, maintainability, and adaptability in real-world scenarios, you will discover:

> **yyjson is essentially just a specialized version of the DOM reuse mode provided by LJSON.** (First commit time: yyjson **2020.10**, LJSON **2019.10**)

This is not a criticism, but a clarification of architectural philosophy. This article will delve into the fundamental differences between yyjson and LJSON from multiple perspectives: pattern design, performance strategies, memory behavior, streaming capabilities, and more.

## 1. DOM Reuse Mode: The Starting Point for Performance, Not the Endpoint

The so-called DOM reuse mode refers to reusing the strings from the input buffer when parsing JSON, avoiding extra copying and heap allocations, thereby improving performance and reducing memory usage.

LJSON's DOM reuse mode features:

-   In-place string reuse (zero-copy)
-   Memory pool allocation for structures (avoids frequent malloc/free)
-   Editability (supports modifying keys/values)

yyjson's core mode highly overlaps with this, but it further implements aggressive read-only optimizations.

## 2. yyjson's Extreme Optimization: The Champion for Read-Only Reuse Scenarios

Building upon DOM reuse, yyjson employs the following strategies to pursue ultimate performance:

-   **Non-standard string termination**: Uses `\0\0\0\0` instead of standard C string termination (`\0`) to reduce boundary checks.
-   **Macro-unrolled loops**: Heavily uses macros to manually unroll parsing loops, squeezing CPU instruction pipelines.
-   **Pre-allocated redundant memory**: Allocates a large block of memory (with relatively high redundancy) at once to store object structures, avoiding frequent `realloc`.
-   **Read-only, non-editable**: The parsed structure cannot be modified (object structures can be smaller, e.g., 16-byte aligned to cache lines). Sacrifices flexibility for speed (requires conversion for editing).

These techniques indeed make yyjson impressive in read-only scenarios, but they also limit its applicable boundaries.

> Note: yyjson is only faster than LJSON in the *read-only reuse of original strings* mode; its read-only mode and LJSON's are sometimes faster than the other depending on the case; its editable mode can be significantly slower than LJSON's.

> Analysis 1: Why do yyjson's read-only mode and LJSON's mode sometimes outperform each other? Yyjson does not support standard C strings (null-terminated); for standard strings, it must copy the original data (appending 4 zeros) before parsing. LJSON supports standard strings natively. Yyjson lacks the aggressive optimizations and macro loop unrolling for this single mode in LJSON's broader approach.

> Analysis 2: Why is yyjson's editable mode slower than LJSON's? Yyjson needs to copy and convert data into an editable object, potentially doubling memory usage. LJSON's reuse mode is inherently editable from the start.

## 3. LJSON's Architectural Versatility: More Than Just Speed

LJSON provides up to **7 parsing modes** and **4 printing modes**, covering full-scenario needs from memory to file, and from DOM to SAX:

### Parsing modes include:

1.  DOM Classic Mode (uses malloc/free)
2.  DOM Memory Pool Mode
3.  DOM Reuse Mode (Editable, in-place string reuse)
4.  DOM File Streaming Mode (True streaming)
5.  DOM File Streaming Memory Pool Mode (True streaming + memory pool)
6.  SAX Mode (Callback-based processing)
7.  SAX File Streaming Mode (True streaming + callbacks)

> True streaming parsing (Chunked file parsing: parse while reading, no need to read the entire file into a buffer first).

### Printing modes include:

1.  DOM → String
2.  DOM → File (True streaming)
3.  SAX → String
4.  SAX → File (True streaming)

> True streaming printing (Print and write to file simultaneously, no need to print to a complete string first before writing to file).

This architectural design allows LJSON to handle the following scenarios with ease:

-   **Large file processing**: Supports constant-memory parsing of GB-level JSON files.
-   **Embedded systems**: Low memory footprint, no heap allocation (in certain modes).
-   **High-frequency read/write**: Supports editable DOM and high-performance printing.
-   **Cross-platform development**: Pure C implementation, no third-party dependencies.

## 4. The Divide in Design Philosophy: Depth vs. Single-Point Breakthrough

| Dimension             | yyjson                          | LJSON                               |
| --------------------- | ------------------------------- | ----------------------------------- |
| Core Mode             | DOM Reuse (Read-only)           | Multi-Mode (DOM/SAX/Streaming)      |
| Editability           | ❌ Not supported (Convertible) | ✅ Supported (No extra conversion)  |
| Streaming Processing  | ❌ Not supported (Requires full read) | ✅ True Streaming                    |
| Memory Strategy       | Memory Blocks + In-place Reuse  | Memory Pools + In-place Reuse       |
| Code Readability      | Very Low (Macro-heavy) (~20k LOC) | High (Clear structure) (~10k LOC)    |
| Applicable Scenarios  | Extreme Read-only Parsing       | General JSON Processing             |

Yyjson is a sharp scalpel, focused on extreme performance for read-only parsing. LJSON is a complete toolbox, balancing performance, flexibility, and maintainability.

## 5. Conclusion: The Essence of Selection is Understanding Architectural Boundaries

Yyjson's success lies in its deep refinement of a single mode, but its architecture is essentially just a specialized instance within LJSON's multi-mode system (**yyjson ≈ LJSON's DOM Reuse Mode + Aggressive Read-Only Optimization**). The true value of LJSON lies in its ability to stably support diverse JSON processing needs in real engineering environments with extremely low memory usage and high performance.

When selecting a library, developers should weigh the actual scenario: is it the extreme speed of read-only parsing, or the architectural elasticity for general processing? Understanding the essential difference between these two is key to making truly engineering-sound decisions.

