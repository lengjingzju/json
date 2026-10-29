# LJSON

![License](https://img.shields.io/badge/license-MIT-brightgreen.svg) ![Language](https://img.shields.io/badge/language-C99%20%2F%20C11-blue.svg) ![Version](https://img.shields.io/badge/version-v5.0.5-orange.svg) ![Dependencies](https://img.shields.io/badge/dependencies-0-success.svg) ![SIMD](https://img.shields.io/badge/SIMD-not%20required-brightgreen.svg) ![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20Windows%20%7C%20RTOS-lightgrey.svg) ![Modes](https://img.shields.io/badge/parse%20modes-7-blueviolet.svg) ![Print](https://img.shields.io/badge/print%20modes-4-blueviolet.svg)

[English Edition](./README.md)

> **一句话认识 LJSON**
>
> **LJSON 是一个库，两个引擎：既是符合 JSON5 规范的全能 JSON 引擎，也是业界最快的数值字符串双向转换引擎。**
>
> 它用 **纯 C99** 写成、**零第三方依赖**、**不依赖任何 CPU 特性（没有 SIMD、没有内联汇编）**，却能在通用可编辑 JSON 库里跑出最快的一档成绩 —— 因为它的性能全部来自**算法、数据结构与工程权衡**，而不是靠压榨某一条指令集。

- [为什么是 LJSON：开局先看这十件事](#为什么是-ljson开局先看这十件事)
- [1. 项目定位](#1-项目定位)
  - [1.1 双模引擎：一个库，两个引擎](#11-双模引擎一个库两个引擎)
  - [1.2 "通用最强"的含义](#12-通用最强的含义)
  - [1.3 版本与兼容性](#13-版本与兼容性)
- [2. 主要特性](#2-主要特性)
  - [2.1 JSON 引擎特性](#21-json-引擎特性)
  - [2.2 数值引擎（LDOUBLE）特性](#22-数值引擎ldouble特性)
  - [2.3 支持的数据类型](#23-支持的数据类型)
- [3. 架构：7 种解析模式 × 4 种打印模式](#3-架构7-种解析模式--4-种打印模式)
  - [3.1 七种解析模式](#31-七种解析模式)
  - [3.2 四种打印模式](#32-四种打印模式)
  - [3.3 选型建议](#33-选型建议)
  - [3.4 模式组合矩阵](#34-模式组合矩阵)
- [4. 快速上手](#4-快速上手)
  - [4.1 先说接口文档在哪](#41-先说接口文档在哪)
  - [4.2 首选方案：零堆内存分配模式](#42-首选方案零堆内存分配模式)
  - [4.3 最小示例：解析 → 取值 → 打印](#43-最小示例解析--取值--打印)
  - [4.4 内存池模式与复用模式](#44-内存池模式与复用模式)
  - [4.5 消息解析的两种加速模式](#45-消息解析的两种加速模式)
    - [4.5.1 基础：把键字符串常量化](#451-基础把键字符串常量化)
    - [4.5.2 加速模式一：先获取 items，再批量访问](#452-加速模式一先获取-items再批量访问)
    - [4.5.3 加速模式二：预填 prev，接续查找](#453-加速模式二预填-prev接续查找)
    - [4.5.4 两种模式怎么选](#454-两种模式怎么选)
  - [4.6 增删改：编辑接口](#46-增删改编辑接口)
  - [4.7 SAX 解析：回调驱动](#47-sax-解析回调驱动)
  - [4.8 SAX 打印：不建 DOM 直接输出](#48-sax-打印不建-dom-直接输出)
  - [4.9 文件流式解析与打印](#49-文件流式解析与打印)
  - [4.10 数值引擎单独使用](#410-数值引擎单独使用)
  - [4.11 二进制扩展类型](#411-二进制扩展类型)
- [5. 编译与运行](#5-编译与运行)
  - [5.1 编译与安装方法](#51-编译与安装方法)
  - [5.2 编译开关](#52-编译开关)
  - [5.3 运行方法](#53-运行方法)
  - [5.4 调试方法](#54-调试方法)
  - [5.5 解析配置](#55-解析配置)
- [6. 性能测试](#6-性能测试)
  - [6.1 测试说明与环境](#61-测试说明与环境)
  - [6.2 Intel i7-1260P（v5.0.5，最新）★](#62-intel-i7-1260pv505最新)
  - [6.3 最新终端实测：7 种模式全对比](#63-最新终端实测7-种模式全对比)
  - [6.4 复用模式：重复解析 / 重复打印吞吐](#64-复用模式重复解析--重复打印吞吐)
  - [6.5 ARM Cortex-A53（历史数据，v3 时期）](#65-arm-cortex-a53历史数据v3-时期)
  - [6.6 Intel i7-10700（历史数据，v3 时期）](#66-intel-i7-10700历史数据v3-时期)
  - [6.7 数值转换性能](#67-数值转换性能)
- [7. LJSON 为什么这么快：代码级深度解析](#7-ljson-为什么这么快代码级深度解析)
  - [7.1 方法论：测量 → 分析 → 优化 → 验证](#71-方法论测量--分析--优化--验证)
  - [7.2 优化层次模型：收益从哪里来](#72-优化层次模型收益从哪里来)
  - [7.3 阿姆达尔定律视角：为什么先"选路"而不是"抠指令"](#73-阿姆达尔定律视角为什么先选路而不是抠指令)
  - [7.4 算法与框架：选对路是最省力的优化](#74-算法与框架选对路是最省力的优化)
  - [7.5 冷热分离：把不常跑的代码挪出热路径](#75-冷热分离把不常跑的代码挪出热路径)
    - [7.5.1 两种写法](#751-两种写法)
    - [7.5.2 LJSON 里的三处实例](#752-ljson-里的三处实例)
    - [7.5.3 两种方式怎么选](#753-两种方式怎么选)
    - [7.5.4 延伸：另一种"分离"——不同实现的一次性分派](#754-延伸另一种分离不同实现的一次性分派)
  - [7.6 两阶段字符串解析：快速路径前置 + 完整路径兜底](#76-两阶段字符串解析快速路径前置--完整路径兜底)
  - [7.7 打印热路径：局部变量 `cur` + `_XXX_SYNC` 宏 + 多分配 64 字节](#77-打印热路径局部变量-cur--_xxx_sync-宏--多分配-64-字节)
  - [7.8 数据结构：32 字节的 json_object](#78-数据结构32-字节的-json_object)
  - [7.9 链表设计：O(1) 追加的单向循环链表](#79-链表设计o1-追加的单向循环链表)
  - [7.10 内存池：bump 分配器，两次比较搞定一次分配](#710-内存池bump-分配器两次比较搞定一次分配)
  - [7.11 零拷贝：原地复用原始字符串](#711-零拷贝原地复用原始字符串)
  - [7.12 信息记录：把 strlen / 转义扫描的结果缓存下来](#712-信息记录把-strlen--转义扫描的结果缓存下来)
  - [7.13 访问加速：json_items 与哈希二分查找](#713-访问加速json_items-与哈希二分查找)
  - [7.14 空间换时间：查表法](#714-空间换时间查表法)
  - [7.15 快速整除：把除法换成乘法](#715-快速整除把除法换成乘法)
    - [7.15.1 实现原理](#7151-实现原理)
    - [7.15.2 python 实现](#7152-python-实现)
    - [7.15.3 常用快速除法表](#7153-常用快速除法表)
  - [7.16 批量操作：一次搬运 4/8/16 字节](#716-批量操作一次搬运-4816-字节)
  - [7.17 分支预测与手动循环展开](#717-分支预测与手动循环展开)
  - [7.18 真流式：把 I/O 压进解析/打印流水线](#718-真流式把-io-压进解析打印流水线)
  - [7.19 打印缓冲：几何增长 + 外部复用](#719-打印缓冲几何增长--外部复用)
  - [7.20 编译期配置：把运行期开关变成不存在的代码](#720-编译期配置把运行期开关变成不存在的代码)
  - [7.21 精度换效率：ldouble 的取舍](#721-精度换效率ldouble-的取舍)
  - [7.22 优化清单总表](#722-优化清单总表)
- [8. 横向对比：LJSON vs yyjson / RapidJSON / cJSON](#8-横向对比ljson-vs-yyjson--rapidjson--cjson)
  - [8.1 一图看懂](#81-一图看懂)
  - [8.2 vs cJSON](#82-vs-cjson)
  - [8.3 vs RapidJSON](#83-vs-rapidjson)
  - [8.4 vs yyjson](#84-vs-yyjson)
  - [8.5 决策树：我该选哪个](#85-决策树我该选哪个)
- [9. 常见问题（FAQ）](#9-常见问题faq)
- [10. 参考与许可](#10-参考与许可)

## 为什么是 LJSON：开局先看这十件事

如果你只想花 30 秒判断要不要往下读，看这张表就够了。这里的「通用库」指的是：**既能解析也能打印、既可编辑也能复用、既能处理小消息也能处理 GB 级大文件**，而不是只把某一项做到极致的专用库。

> 这一章是**速览**：每条只用一句话讲清"它是什么、对你有什么用"。
> 每一项的技术细节与实测数据在[2. 主要特性](#2-主要特性)、[6. 性能测试](#6-性能测试)、[7. 性能优化解析](#7-ljson-为什么这么快代码级深度解析)中展开。

| # | 卖点 | LJSON 的做法 | 对使用者的意义 |
|:--:|------|------|------|
| 1 | **纯 C99，零依赖** | 只用到 `<string.h>` / `<stdlib.h>` / `<stdio.h>` 等标准头；`jnum.c` 与 `json.c` 两个文件即可编译 | 嵌入式、RTOS、裸机、交叉编译全都能过；编译速度极快 |
| 2 | **不依赖 CPU 特性** | 没有 SIMD、没有内联汇编；用到的编译器内建（`__builtin_clzll`、`_umul128`、`__int128`）全部有可移植的 C 回退实现 | 换架构不改代码，ARM/x86/RISC-V/MIPS 表现一致 |
| 3 | **C11 统一泛型接口** | `json_create_unit` / `json_create_units` / `json_set_number` / `json_sax_print_array_item` 等用 `_Generic` 按类型自动分发；C++ 下自动切换为重载 | 一套 API 通吃 `bool/int32/uint32/int64/uint64/double/字符串/二进制`，写起来像动态语言 |
| 4 | **7 种解析模式** | 经典 DOM / 内存池 DOM / 复用 DOM / 文件流 DOM / 文件流+内存池 DOM / SAX / SAX 文件流 | 小消息、大文件、只读扫描、可编辑树，各有最优解，不用为所有场景妥协 |
| 5 | **4 种打印模式** | DOM→字符串、DOM→文件（真流式）、SAX→字符串、SAX→文件（真流式） | 打印超大 JSON 不必先攒出完整字符串，内存可常数级 |
| 6 | **真流式** | 边读边解析、边打印边写；不是"读完再解析 / 打印完再写"的伪流式 | 1 GB JSON 也能用 KB 级内存完成格式化转换 |
| 7 | **可编辑 + 零拷贝兼得** | 复用模式（模式3）原地复用原始字符串，**并且天然可编辑**（yyjson 需要额外转换一份，内存翻倍） | 既要快又要改数据的场景，LJSON 是少数可行解 |
| 8 | **零堆内存分配** | 首次分配后，循环解析/打印期间库内部**不再进行任何堆分配** | 高频消息收发无内存碎片、无分配抖动、延迟可预测 |
| 9 | **自研 ldouble 数值引擎** | 浮点→字符串耗时仅为 `sprintf` 的 **1.5%~10%**（快 10~70 倍）；字符串→数值比 `strtod` 快 1~10 倍 | 浮点密集 JSON（如地理/时序数据）的打印性能直接拉开一个数量级 |
| 10 | **工程可维护性** | 约 1 万行、结构清晰、每个接口都有**中英文双语注释**；接口风格参考 cJSON | 能读、能改、能二次开发，不是只能膜拜的黑盒 |

一句话总结：**LJSON 不是"某一种模式跑得最快的库"，而是"在任何场景下都能选到接近最优路径、同时还跑得比大多数库快"的库。**

---

## 1. 项目定位

### 1.1 双模引擎：一个库，两个引擎

LJSON 在一个仓库里提供了两套可以独立使用、也可以深度协同的引擎：

| 引擎 | 源码 | 头文件 | 职责 | 能不能单独用 |
|------|------|--------|------|:------------:|
| **JSON 引擎** | `json.c` | `json.h` | JSON5 文本的解析与生成：DOM / SAX 双范式、真流式文件 I/O、内存池、原地复用、零堆分配、`json_items` 访问加速、`prev` 访问加速 | ✅ |
| **数值引擎（jnum）** | `jnum.c` | `jnum.h` | 数值 ↔ 字符串双向高速转换：整数 / 长整数 / 十六进制 / 双精度浮点，自研 **ldouble** 算法 | ✅ 只编 `jnum.c` 即可 |

**两者的协同点**：JSON 打印时每一个数字都要经过"数值 → 字符串"转换。LJSON 没有用 `sprintf`，而是让打印路径直接调用自研的 `jnum_dtoa`。这一个决定，就是浮点密集数据（如 `canada.json`）上打印性能比 cJSON 快一个数量级的直接原因。

### 1.2 "通用最强"的含义

LJSON 的定位是**通用 JSON 库**，不是跑分专用库。它的目标函数里同时有五个变量：

```
性能  +  内存占用  +  可编辑性  +  流式能力  +  可维护性
```

这解释了它和"极限派"库的分野：

* **极限派**（如 yyjson 的只读模式）：把某一个点做到极致，代价是**放弃流式、放弃原地可编辑、放弃标准 C 字符串、放弃代码可读性**。
* **LJSON**：提供完整的模式矩阵，让每种场景都能匹配最接近最优的路径，同时**在通用可编辑路径上依然跑赢同类**。

用一句话概括它的架构哲学：

> **没有绝对最优的模式，只有最合适的模式。**
> 与其把所有场景都塞进一种实现里妥协，不如把选择权交给使用者 —— 但要保证每一种选择都不慢。

### 1.3 版本与兼容性

* 当前版本：**v5.x**
* 诞生时间：**2019 年 10 月**
* **重要兼容性说明**：64 位操作系统上，从 **v5.0.0** 开始，JSON 对象由 **40 字节**压缩到 **32 字节**，解析速度提升 **5%~10%**，**与 v3.x.x 版本不兼容**。从 v3 升级到 v5 时请一并更新调用代码。

---

## 2. 主要特性

### 2.1 JSON 引擎特性

**① 极速解析与打印**

在 `1000×twitter.json`（603 MB）、`1000×citm_catalog.json`（1.7 GB）、`100×canada.json`（215 MB）三个样本上，LJSON 复用模式（模式 3，可编辑）相对 cJSON / RapidJSON / yyjson 全面领先，解析快 1.2~6.6 倍、打印快 1.1~23 倍。完整数据与分析见 [6.3 最新终端实测：7 种模式全对比](#63-最新终端实测7-种模式全对比)与[第八章 横向对比](#八-横向对比ljson-vs-yyjson--rapidjson--cjson)。

**② 完整 JSON5 支持（默认关闭以保证速度，按需开启）**

支持十六进制、注释、尾逗号、单引号/无引号键、字符串内特殊字符、`NaN`/`Infinity` 等现代 JSON 特性。

> ⚠️ **重要**：部分 JSON5 特性会显著影响解析热路径，**默认配置是关闭的**（以速度优先）。需要时在 `json.c` 中把对应宏置 1 重新编译即可，详见 [5.5 解析配置](#55-解析配置)。
> 其中 `JSON_PARSE_SKIP_COMMENT`（注释）与 `JSON_PARSE_SPECIAL_QUOTES`（单引号/无引号键）**默认关闭且对速度影响最大**。

**③ 真流式文件处理**

* 解析：**边读文件边解析**，不是读完再解析
* 打印：**边打印边写文件**，不是打印完再写
* SAX + 文件流配合，内存占用降为**常数级**：即使 1 GB 的 JSON 文件，也能用 KB 级内存完成格式化转换

**④ 多层次内存管理**

| 层次 | 做法 | 收益 |
|------|------|------|
| 经典分配 | `malloc` / `free` 逐节点 | 通用、可单独释放 |
| 内存池 | 大块内存 bump 分配 | 无碎片、分配近乎零成本 |
| 字符串原地复用 | 直接把原始缓冲区当作键与字符串的存储点 | 零拷贝，省一次 memcpy 与一次 malloc |
| 零堆分配循环 | 首次分配后持续复用 | 循环期间库内零 `malloc`，延迟可预测 |

**⑤ 内存复用（两重含义）**

1. **原始数据直接作为键和字符串的存储点**（原地复用）
2. **多次解析和打印时一直复用同一块内存**，库内部都无需进行堆内存分配

**⑥ 扩展数据类型**

除标准 JSON 类型外，还支持：长整型（64 位）、十六进制数、**二进制数据扩展（格式 `<len:data>`）**。

**⑦ 紧凑对象**

64 位系统上 `json_object` 从 40 字节压缩至 **32 字节**（v5.0.0 起），解析速度提升 5%~10%。详见 [7.8 数据结构：32 字节的 json_object](#78-数据结构32-字节的-json_object)。

**⑧ 开发者友好**

* 接口参考 [cJSON](https://github.com/DaveGamble/cJSON) 设计，迁移成本极低
* 纯 C 实现，无第三方库依赖
* 支持 gcc / clang / MSVC 等多种编译器，x86 / ARM / RISC-V 等多种架构
* 每个接口都有**中英文双语注释**
* C11 `_Generic` 泛型接口 + C++ 重载接口，一套 API 通吃所有类型

### 2.2 数值引擎（LDOUBLE）特性

**字符串 → 数值（atod）**

* 比标准库 `strtod` 快 **1~10 倍**（多数场景 2~3 倍）

**数值 → 字符串（dtoa）**

* 耗时仅为 `sprintf` 的 **1.5%~10%**，即快 **10~70 倍**
* 拥有 **17 位精度**，且**始终输出最短精确表示**
* 除一位小数的情况外，一般显著快于 **grisu2**，科学计数法表示时可快 **100%** 以上
* 绝大多数情况下快于 **dragonbox**，科学计数法表示时可快 **30%** 以上

> 注：LDOUBLE 为追求性能，边界处理追求**最短表示**而非机械的偶数舍入，不完全符合 IEEE-754 的严格往返约定。若你的场景需要标准库行为，编译时指定 `-DJSON_DTOA_ALGORITHM=1` 即可切回 `sprintf`。

### 2.3 支持的数据类型

`json.h` 中定义的 `json_type_t`：

| 类型 | 含义 | 对应的值成员 | 备注 |
|------|------|--------------|------|
| `JSON_NULL` | null | — | 无值成员 |
| `JSON_BOOL` | true / false | `vnum.vbool` | |
| `JSON_INT` | 32 位有符号整数 | `vnum.vint` | |
| `JSON_HEX` | 32 位无符号十六进制 | `vnum.vhex` | 扩展类型，打印带 `0x` 前缀 |
| `JSON_LINT` | 64 位有符号长整数 | `vnum.vlint` | 扩展类型 |
| `JSON_LHEX` | 64 位无符号长十六进制 | `vnum.vlhex` | 扩展类型，打印带 `0x` 前缀 |
| `JSON_DOUBLE` | 双精度浮点 | `vnum.vdbl` | ldouble 算法转换 |
| `JSON_STRING` | 字符串 | `vstr` + `istr` | 带长度与转义标记 |
| `JSON_BINARY` | 二进制数据 | `vbin` + `istr` | 扩展类型，格式 `<len:data>` |
| `JSON_ARRAY` | 数组 | `value.head` | 链表头 |
| `JSON_OBJECT` | 对象 | `value.head` | 链表头 |

数值引擎侧 `jnum.h` 中的 `jnum_type_t` 与之一一对应（`JNUM_NULL` / `JNUM_BOOL` / `JNUM_INT` / `JNUM_HEX` / `JNUM_LINT` / `JNUM_LHEX` / `JNUM_DOUBLE`）。

---

## 3. 架构：7 种解析模式 × 4 种打印模式

LJSON 的核心设计是**模式矩阵**：解析有 7 种模式，打印有 4 种模式，它们可以自由组合。理解这张矩阵，就理解了 LJSON 的全部。

### 3.1 七种解析模式

| 模式 | 名称 | 核心 API | 内存行为 | 可编辑 | 典型场景 |
|:----:|------|----------|----------|:------:|----------|
| **1** | DOM 经典模式 | `json_parse_str` | `malloc`/`free`，逐节点分配 | ✅ | 通用、小 JSON、需要随意增删节点 |
| **2** | DOM 内存池模式 | `json_fast_parse_str` | 大块内存池 bump 分配，无碎片 | ✅ | 中大型 JSON，追求低分配开销 |
| **3** | **DOM 复用模式** ⭐ | `json_reuse_parse_str` | 内存池 + 字符串**原地复用**（零拷贝） | ✅ | **高频解析、消息总线、既快又要改** |
| **4** | DOM 文件流模式 | `json_parse_file` | 边读边解析，`malloc` 分配 | ✅ | 大文件，避免一次性读入 |
| **5** | DOM 文件流 + 内存池 | `json_fast_parse_file` | 边读边解析 + 内存池 | ✅ | 大文件 + 低分配开销 |
| **6** | SAX 模式 | `json_sax_parse_str` | 回调驱动，不构建完整树 | — | 只需抽取/转换，内存敏感 |
| **7** | **SAX 文件流模式** ⭐ | `json_sax_parse_file` | 边读边解析 + 回调 | — | **GB 级文件的常数级内存处理** |

**七个模式的本质差别，只有三个维度**

不要被七个模式吓到，它们的差别只来自三个正交的开关：

```
┌─ 是否构建 DOM 树？ ─ DOM（1/2/3/4/5）     vs SAX（6/7）
├─ 内存从哪来？ ────── malloc（1/4）        vs 内存池（2/3/5）
└─ 数据从哪读？ ────── 内存字符串（1/2/3/6） vs  文件流（4/5/7）
```

* **DOM vs SAX**：DOM 把整棵 JSON 树建出来，可以随意增删改查；SAX 只在回调里把事件交给你，不建树、内存常数级，但拿不到"整棵树"。
* **malloc vs 内存池**：内存池一次分配大块内存，之后内部 bump 分配（指针加一下就完事），**无法单独释放某个节点**，但可以一次性全部释放。
* **字符串 vs 文件流**：文件流是**真流式**，边读边解析，不需要先把整个文件读进内存。

**模式 3（复用模式）为什么特殊？**

它是唯一同时做到这三件事的模式：

1. 使用内存池（分配近乎零成本）
2. 字符串**原地复用**原始缓冲区（零拷贝，省一次 memcpy + 一次 malloc）
3. **保持可编辑**（yyjson 的复用模式是只读的，要编辑必须再转换一份，内存翻倍）

代价是：**原始 buffer 会被就地修改**，且在 json 使用期间不能释放。这也是它成为高频消息处理首选的原因。

### 3.2 四种打印模式

**四种打印模式**

- DOM从json结构树打印到字符串
- DOM从json结构树流式打印到文件（边打印边写文件）
- SAX直接从数据打印到字符串
- SAX直接从数据流式打印到文件（边打印边写文件）

**DOM 打印通用接口是 `json_print_common(json, json_print_choice_t *choice);` ，4个特化接口如下**

| 模式 | DOM 打印 API | 说明 |
|:----:|--------------|------|
| DOM → 字符串（格式化） | `json_print_format(json, item_total, &len, ptr)` | 标准序列化 |
| DOM → 字符串（压缩） | `json_print_unformat(json, item_total, &len, ptr)` | 压缩输出 |
| DOM → 文件（格式化，**真流式**） | `json_fprint_format(json, item_total, path, ptr)` | 边打印边写 |
| DOM → 文件（压缩，**真流式**） | `json_fprint_unformat(json, item_total, path, ptr)` | 边打印边写 |

**SAX 打印公共三件套（四种模式共用）**

| 阶段 | API | 作用 |
|------|-----|------|
| 开始 | `json_sax_print_start(json_print_choice_t *choice)` | 通用启动接口，通过 `choice` 决定"打印到字符串还是文件、格式化还是压缩" |
| 输出 | `json_sax_print_value(hd, type, jkey, value)` | 通用输出接口，所有类型的值都走它 |
| 结束 | `json_sax_print_finish(hd, length, ptr)` | 收尾，返回字符串（打印到字符串时）或刷新缓冲（打印到文件时） |

**SAX 打印四个特化的 start**

下面 4 个 start 都是 `json_sax_print_start` 的语法糖，只是把 `choice` 里的 `format_flag` 和 `path` 预先填好：

| 模式 | start API | 等价于 |
|:----:|-----------|--------|
| 1 | `json_sax_print_format_start(item_total, ptr)` | `choice.format_flag = true;  choice.path = NULL;` |
| 2 | `json_sax_print_unformat_start(item_total, ptr)` | `choice.format_flag = false; choice.path = NULL;` |
| 3 | `json_sax_fprint_format_start(item_total, path, ptr)` | `choice.format_flag = true;  choice.path = path;` |
| 4 | `json_sax_fprint_unformat_start(item_total, path, ptr)` | `choice.format_flag = false; choice.path = path;` |

> `ptr`（`json_print_ptr_t`）用于**复用打印缓冲**：传入后打印结果会一直复用 `ptr->p`，最后只需 `json_memory_free(ptr->p)` 释放一次。

### 3.3 选型建议

**先说结论，只有两条：**

> **解析：默认选模式 3（复用模式）。**
> **打印：默认选 SAX 直接打印，不要先生成 DOM。**

**解析为什么默认模式 3？**

模式 3 在三个数据集上都是 LJSON 自家最快的解析路径（`twitter` 320 ms vs 模式1 的 952 ms），而且它是**可编辑**的。它唯一的额外约束是"原始 buffer 会被修改、使用期间不能释放"，对于"收到一段消息 → 解析 → 处理"的常规流程，这个约束几乎总是可以接受。

只有在以下情况才退回到别的模式：

| 情况 | 改用 |
|------|------|
| 必须保留原始 buffer 不被修改 | 模式 2（内存池，字符串会复制一份） |
| 需要单独释放某个节点 / 长期持有部分子树 | 模式 1（经典 malloc） |
| 数据来自超大文件，不想一次性读入 | 模式 5（要 DOM）或模式 7（不要 DOM） |
| 只需从超大文件里抽取少量字段 | 模式 7（SAX 回调） |

**打印为什么默认 SAX 直打？**

因为 **DOM 是中间产物**。如果你的数据本来就在你自己的结构里（业务对象、数组、查询结果），那么"先建 DOM 再从 DOM 打印"完全是多余的两次转换：

```
❌ 你的数据 → 建 DOM（N 次 malloc + 挂链表）→ 遍历 DOM 打印 → 删 DOM
✅ 你的数据 → SAX 打印（一路写进缓冲区）
```

SAX 打印省掉了整棵 DOM 树的构建与销毁，打印吞吐实测可达 **2.6~5.2 GB/s**（见 [6.4 复用模式：重复解析 / 重复打印吞吐](#64-复用模式重复解析--重复打印吞吐)）。

只有在以下情况才先建 DOM 再打印：

| 情况 | 改用 |
|------|------|
| 数据本来就是一份 JSON，你要"读进来改一改再输出" | DOM 解析 + DOM 打印 |
| 需要按 key 随机访问、多次修改、反复打印 | DOM 解析 + DOM 打印 |
| 需要格式化一份已有的 JSON 文件 | 模式 5/7 流式，或 `json_fprint_format` |

**强烈推荐的组合：模式 3 解析 + SAX 打印 + 零堆分配**

这是 LJSON 为"高频消息收发"场景准备的标准答案，完整代码见[4.2 首选方案：零堆内存分配模式](#42-首选方案零堆内存分配模式)。它的特点是：

* 解析复用内存池 `mem`，打印复用 `json_print_ptr_t` 的 `p`
* 循环期间**库内部零堆分配**
* 访问用 `json_get_items` 加速，且 `json_items_t` 也一直复用

### 3.4 模式组合矩阵

解析模式 × 打印模式的常见组合：

| 场景 | 解析 | 打印 | 内存特征 |
|------|:----:|:----:|----------|
| 小消息高频收发（推荐） | 3 | SAX→字符串 | 零堆分配，常数内存 |
| 小消息高频收发（写文件） | 3 | SAX→文件 | 零堆分配 + 真流式写 |
| 常规业务 JSON | 1 或 2 | DOM→字符串 | 一次性，简单 |
| 大文件格式化（要 DOM） | 5 | DOM→文件 | 流式读 + 流式写 |
| 大文件格式化（不要 DOM） | 7 | SAX→文件 | **常数级内存** |
| 大文件抽取字段 | 7 | 自定义（回调里处理） | **常数级内存** |
| 生成超大 JSON | — | SAX→文件 | 边生成边写 |
| JSON 转 JSON（过滤/改写） | 7 | SAX→文件 | 解析与打印在一条流水线上 |

> 最后一行的组合（SAX 解析回调里直接 SAX 打印）就是 `json_test.c` 中模式 7 的实现方式 —— 它能在 1.7 GB 的 `citm_catalog.json` 上，用**常数级内存**完成"读 → 解析 → 格式化 → 写"的全过程，耗时 2946 ms。

---

## 4. 快速上手

### 4.1 先说接口文档在哪

**完整、权威的接口文档就在两个头文件里 —— [json.h](./json.h) 与 [jnum.h](./jnum.h) ，每一个接口都带有中英文双语注释**（参数含义、返回值、注意事项、使用建议）。可直接阅读，不必另找文档。

再记住这几条命名规律，基本就不用查文档了：

| 前缀 / 后缀 | 含义 | 例子 |
|-------------|------|------|
| `json_` | 经典接口（malloc 分配） | `json_create_int` |
| `pjson_` | 内存池接口（多一个 `json_mem_t *mem` 参数） | `pjson_create_int` |
| `json_create_xxx` | 创建指定类型的对象 | `json_create_double` |
| `json_create_unit(s)` | C11 `_Generic` 泛型版本，不用记类型后缀 | `json_create_unit(3.14)` |
| `json_add_xxx_to_array/object` | 添加指定类型的值 | `json_add_string_to_object` |
| `json_get_xxx_value` | 取指定类型的值 | `json_get_lint_value` |
| `json_sax_print_xxx` | SAX 打印一个值 | `json_sax_print_double` |
| `json_fprint_xxx` | 打印到文件（真流式） | `json_fprint_format` |
| `json_sax_fprint_xxx_start` | SAX 打印到文件（真流式） | `json_sax_fprint_format_start` |

可运行的示例集在 [json_test.c](./json_test.c) ：7 种解析模式的调用方式、SAX 打印的完整流程、复用模式重复解析/打印的写法都在里面。

**几个最容易踩的坑**

| 注意 | 说明 |
|------|------|
| 解析接口的参数是 `char *` 不是 `const char *` | 复用模式会就地修改 buffer （其他模式不会），不能传字符串常量 |
| 内存池模式不能逐个 `json_del_object` | 只能 `pjson_memory_free(&mem)` 整体释放 |
| 复用模式下原始 buffer 使用期间不能释放 | 字符串直接指向原始 buffer |
| `json_string_t` 的 `info` 要填 | 用 `json_string_info_update()` 自动填 `len` / `escaped` |
| 打印缓冲可以复用 | 传入 `json_print_ptr_t *ptr`，最后只释放一次 `ptr->p` |
| `jnum_xxx` 的 buffer 至少 27 字节 | 头文件注释中有明确说明 |
| 每一层嵌套都要独立的 `json_items_t` | 不能共用一个 |

---

### 4.2 首选方案：零堆内存分配模式

**如果你的场景是"小 JSON 循环解析打印"（高频消息收发、RPC、网关转发），请直接用这一节的写法，不要先看别的。**

它的特点是：LJSON 只在第一次处理时分配内存，**后续处理中库内部不再进行任何堆内存分配** —— 循环期间没有内存碎片、没有分配抖动、延迟可预测。

**四条规则**

* 解析存储 json 对象一直复用内存池 `mem`
* 解析存储字符串值复用原始字符串（即原始字符串会被修改，且使用 json 过程中不能释放）
* 获取 `JSON_ARRAY` / `JSON_OBJECT` 下的对象可使用 `json_get_items` 加速，可一直复用此对象内存（每个级别都要有单独的 `json_items_t`）
* 打印一直复用 `json_print_ptr_t` 的成员 `p`

```c
size_t len = 0;

// 初始化只做一次(解析)
json_mem_t mem;
pjson_memory_init(&mem);
/* obj_mgr需要设置合适的值以便一个内存块可以存下所有的object, 如果是不复用原始字符串，其它2个mgr也需要设置 */
mem.valid = true;
mem.obj_mgr.mem_size = max_data_size;

// 解析加速复用items
json_items_t items;
memset(&items, 0, sizeof(items));

// 初始化只做一次(打印)
json_print_ptr_t ptr = {PRINT_BUF_LEN, 0};

// 业务循环
while (1) {
    /* ---- 接收消息JSON解析 ---- */
    json_object *json = json_reuse_parse_str(buf, &len, &mem);
        // 获取某JSON_ARRAY下的所有数据
        if (json_get_items(array, &items) < 0)
            return -1;
        for (size_t i = 0; i < items.count; ++i) {
            ids.push_back(json_get_lint_value(items.items[i].json));
        }

    /* ----- 业务处理逻辑 ---- */

    /* ---- 发送消息JSON打印 ----*/
    json_sax_print_hd hd = json_sax_print_unformat_start(100, &ptr); // 开始打印(复用ptr)
    json_sax_print_object(hd, NULL, JSON_SAX_START); // 顶层左花括号
        // 打印某JSON_OBJECT下的数据
        json_sax_print_string(hd, &key, sval);
        // 打印某JSON_ARRAY下的所有数据
        json_sax_print_array(hd, &key, JSON_SAX_START);
        for (const auto &v : ids) {
            json_sax_print_lint(hd, NULL, v);
        }
        json_sax_print_array(hd, NULL, JSON_SAX_FINISH);
    ret |= json_sax_print_object(hd, NULL, JSON_SAX_FINISH); // 顶层右花括号
    json_sax_print_finish(hd, len, &ptr) // 结束打印，生成的字符串位于ptr.p或返回值

    /* ----- JSON内存池处理 ---- */
    // 不再需要json结构，只保留第一个链表节点
    pjson_memory_refresh(&mem);
}

// 销毁资源只做一次(解析)
json_free_items(&items);
pjson_memory_free(&mem);

// 销毁资源只做一次(打印)
json_memory_free(ptr.p);
```

**这段代码为什么能做到零堆分配？三个关键点**

1. **`mem.valid = true` + `obj_mgr.mem_size`**：告诉 LJSON"内存池已经有可用内存，且一个块就能装下所有 object"。这样解析时走 bump 分配（指针加法），永远不会触发新的 `malloc`。
2. **`pjson_memory_refresh(&mem)`**：每轮循环结束时调用，它**只保留第一个内存块节点**、释放其余块，并把写指针重置回块首 —— 于是下一轮又从同一块内存的开头开始分配。
3. **`json_print_ptr_t ptr`**：打印缓冲整轮复用，`json_sax_print_finish(hd, len, &ptr)` 会把结果字符串写回 `ptr.p`，下次循环继续使用。

实测在这套模式下，twitter 样本的吞吐是 **2.71 GB/s 解析、5.18 GB/s 压缩打印**（见 [6.4 复用模式：重复解析 / 重复打印吞吐](#64-复用模式重复解析--重复打印吞吐)），且循环期间库内零堆分配。

> 注：`PRINT_BUF_LEN` 是你自己定义的初始打印缓冲大小（例如 `#define PRINT_BUF_LEN 4096`）。缓冲不够时 LJSON 会自动扩容并更新 `ptr.p`，不必担心溢出。
>
> 这套写法背后的三个机制分别在 [7.10 内存池：bump 分配器，两次比较搞定一次分配](#710-内存池bump-分配器两次比较搞定一次分配)、[7.11 零拷贝：原地复用原始字符串](#711-零拷贝原地复用原始字符串)、[7.19 打印缓冲：几何增长 + 外部复用](#719-打印缓冲几何增长--外部复用) 中有源码级解析。

---

### 4.3 最小示例：解析 → 取值 → 打印

```c
#include <stdio.h>
#include "json.h"

int main(void)
{
    /* 注意：解析接口会就地使用这段 buffer，所以必须是可写的 char[]，不能是字符串常量 */
    char text[] = "{\"Name\":\"LJSON\",\"Year\":2019,\"Score\":9.9,"
                  "\"Tags\":[\"fast\",\"stream\",\"json5\"]}";
    size_t len = 0;
    char *out = NULL;

    /* 1. 解析：模式1 经典 DOM */
    json_object *root = json_parse_str(text, sizeof(text) - 1);
    if (!root) {
        printf("parse failed!\n");
        return -1;
    }

    /* 2. 取值：按 key 取对象成员 */
    json_object *name = json_get_object_item(root, "Name", NULL);
    json_object *year = json_get_object_item(root, "Year", NULL);
    json_object *score = json_get_object_item(root, "Score", NULL);

    printf("Name  = %s\n", name->value.vstr);
    printf("Year  = %d\n", json_get_int_value(year));
    printf("Score = %g\n", json_get_double_value(score));

    /* 3. 取数组：按下标取元素 */
    json_object *tags = json_get_object_item(root, "Tags", NULL);
    int i, n = json_get_array_size(tags);
    printf("Tags  = [");
    for (i = 0; i < n; ++i) {
        json_object *item = json_get_array_item(tags, i, NULL);
        printf("%s\"%s\"", i ? ", " : "", item->value.vstr);
    }
    printf("]\n");

    /* 4. 打印：DOM -> 格式化字符串 */
    out = json_print_format(root, json_item_total_get(root), &len, NULL);
    printf("format   (%zu bytes):\n%s\n", len, out);
    json_memory_free(out);

    /* 5. 打印：DOM -> 压缩字符串 */
    out = json_print_unformat(root, json_item_total_get(root), &len, NULL);
    printf("unformat (%zu bytes):\n%s\n", len, out);
    json_memory_free(out);

    /* 6. 释放整棵树 */
    json_del_object(root);
    return 0;
}
```

### 4.4 内存池模式与复用模式

把 `json_parse_str` 换成 `json_fast_parse_str`（模式2）或 `json_reuse_parse_str`（模式3）即可，差别只在多传一个 `json_mem_t`：

```c
#include <stdio.h>
#include "json.h"

int main(void)
{
    char data[] = "{\"Name\":\"LJSON\",\"Year\":2019,\"Score\":9.9,"
                  "\"Tags\":[\"fast\",\"stream\",\"json5\"]}";
    size_t size = strlen(data);
    json_object *json = NULL;
    json_mem_t mem;

    /* 假设 data/size 已经装好了一份 JSON 文本（需可写） */

    pjson_memory_init(&mem);          /* 初始化只做一次 */

    /* 模式2：内存池模式 —— 字符串会被复制一份，原始 buffer 不被修改 */
    json = json_fast_parse_str(data, size, &mem);

    /* 模式3：复用模式 —— 字符串原地复用原始 buffer（会被修改！），且可编辑 */
    /* mem.valid = true;                 标记已有可用内存 */
    /* mem.obj_mgr.mem_size = size;      让一个内存块能装下所有 object */
    /* json = json_reuse_parse_str(data, size, &mem); */

    if (!json) {
        printf("parse failed!\n");
        pjson_memory_free(&mem);
        return -1;
    }

    /* 用内存池统计已分配的对象数，可以作为打印时的 item_total，省一次递归统计 */
    int item_total = pjson_memory_statistics(&mem.obj_mgr) / (int)sizeof(json_object);
    size_t len = 0;
    char *out = json_print_format(json, item_total, &len, NULL);
    printf("%s\n", out);
    json_memory_free(out);

    /* 内存池模式不能逐个 json_del_object，只能整体释放 */
    pjson_memory_free(&mem);
    return 0;
}
```

> ⚠️ 内存池 / 复用模式下**不要**对单个节点调用 `json_del_object`，只能 `pjson_memory_free(&mem)` 整体释放。
> 复用模式下原始 buffer 会被就地修改，且在 json 使用期间**不能释放**。

### 4.5 消息解析的两种加速模式

处理"请求/响应消息"这类**结构固定、字段固定顺序**的 JSON 时，LJSON 提供两种加速手段。它们来自实际生产项目，可以直接照搬。

#### 4.5.1 基础：把键字符串常量化

先做一个小优化：**把所有的键定义成静态 `json_string_t`，长度在编译期写死**，避免每次查找都算一次 `strlen`。

```cpp
/* 用宏把 key 的名字和长度一次性写死 */
#define DEF_LJSON_STR(name, size)  json_string_t JmixerMsgStr::name = \
        {.str = const_cast<char *>(#name), .info = { .escaped = 0, .len = size }}

DEF_LJSON_STR(id, 2);
DEF_LJSON_STR(cid, 3);
DEF_LJSON_STR(pack_ms, 7);
DEF_LJSON_STR(samp_rate, 9);
/* 值与名字不一致时用带值的版本 */
#define DEF_LJSON_STR_VAL(name, val, size)  json_string_t JmixerMsgStr::name = \
        {.str = const_cast<char *>(val), .info = { .escaped = 0, .len = size }}
DEF_LJSON_STR_VAL(g711a, "G.711A", 6);
```

之后所有查找都用 `JmixerMsgStr::pack_ms.str`，既不用字符串字面量、也不用 `strlen`。

#### 4.5.2 加速模式一：先获取 items，再批量访问

**适用**：数据体里含 `JSON_ARRAY`（数组）或成员较多的 `JSON_OBJECT`，且要**遍历**或**按 key 反复查找**。

`json_get_items` 把某个 `JSON_ARRAY` / `JSON_OBJECT` 下的所有子对象一次性抽成**连续数组** `items`：

* 数组（`JSON_ARRAY`）：按下标 **O(1)** 访问，比 `json_get_array_item` 的 O(N) 快
* 对象（`JSON_OBJECT`）：按 key 哈希**二分查找 O(logN)**（内部会 `qsort` 并标记 hash 冲突）

真实案例 —— 解析"混音列表"（含数组 + 数组里套对象）：

```cpp
int JmixerMsgMixer::parse(json_object *json, json_items_t *items)
{
    json_object *item = NULL, *sub = NULL;

    /* ① 普通字段照常用 json_get_object_item */
    item = json_get_object_item(json, JmixerMsgStr::cid.str, NULL);
    if (!item)
        return -1;
    cid = static_cast<uint32_t>(json_get_lint_value(item));
    if (cid == 0)
        return -1;

    /* ② 遇到数组：先 get_items，再按下标 O(1) 遍历 */
    item = json_get_object_item(json, JmixerMsgStr::ids.str, NULL);
    if (!item)
        return -1;
    if (json_get_items(item, items) < 0)
        return -1;
    for (size_t i = 0; i < items->count; ++i) {
        ids.push_back(static_cast<uint32_t>(json_get_lint_value(items->items[i].json)));
    }
    if (ids.empty())
        return -1;

    /* ③ 数组里套对象：对 items->items[i].json 再按 key 取 */
    item = json_get_object_item(json, JmixerMsgStr::set_ids.str, NULL);
    if (item) {
        set_flag = true;
    } else {
        item = json_get_object_item(json, JmixerMsgStr::add_ids.str, NULL);
        set_flag = false;
    }
    if (item) {
        if (json_get_items(item, items) == 0) {
            for (size_t i = 0; i < items->count; ++i) {
                AudioMixerSource v;
                sub = json_get_object_item(items->items[i].json, JmixerMsgStr::id.str, NULL);
                if (!sub)
                    continue;
                v.id = static_cast<uint32_t>(json_get_lint_value(sub));

                sub = json_get_object_item(items->items[i].json, JmixerMsgStr::decay.str, NULL);
                if (!sub)
                    continue;
                v.decay = static_cast<uint8_t>(json_get_int_value(sub));
                if (v.decay > 9)
                    v.decay = 9;
                new_ids.push_back(v);
            }
        }
    }
    return 0;
}
```

**三个要点**

1. `items` 由**调用方提供并复用**（通常定义在循环外，或作为 `json_items_t items[2]` 数组按层传递），容量够就不再分配 —— 这是它能在零堆分配模式里使用的原因。
2. **每一层嵌套需要独立的 `json_items_t`**。上面的例子因为同一时刻只用一层，所以复用同一个 `items`；若两层要同时持有（如 `JVmixerMsgLinkParam` 里 `idxs` 和 `crops` 两个数组），就要传 `items[2]`。
3. 用之前先 `memset(&items, 0, sizeof(items))`，用完全部之后 `json_free_items(&items)` （如果items是json_items_t数组，每个数组元素都要单独调用）。

**对象场景按 key 二分查找**：

```c
json_string_t jkey = {0};
jkey.str = (char *)"Name";
json_string_info_update(&jkey);                     /* 自动填 info.len / info.escaped */
uint32_t hash = json_string_hash_code(&jkey);
json_object *found = json_search_object_item(&items, &jkey, hash);
```

#### 4.5.3 加速模式二：预填 prev，接续查找

**适用**：字段**按固定顺序出现**、只需**顺序取一遍**的对象。`json_get_object_item` 的第三个参数 `prev` 是"上一次找到的节点"，传入后查找**从 prev 之后开始**，而不是每次从头扫。

真实案例 —— 解析"区域参数"（`x` → `y` → `w` → `h` 顺序固定）：

```cpp
int JVmixerMsgArea::parse(json_object *json)
{
    json_object *item = NULL, *prev = NULL;

    item = json_get_object_item(json, JVmixerMsgStr::x.str, &prev);
    if (!item)
        return -1;
    x = static_cast<uint16_t>(json_get_int_value(item));

    prev = item; /* ★ 把这次的结果作为下次的起点 */
    item = json_get_object_item(json, JVmixerMsgStr::y.str, &prev);
    if (!item)
        return -1;
    y = static_cast<uint16_t>(json_get_int_value(item));

    prev = item;
    item = json_get_object_item(json, JVmixerMsgStr::w.str, &prev);
    if (!item)
        return -1;
    w = static_cast<uint16_t>(json_get_int_value(item));

    prev = item;
    item = json_get_object_item(json, JVmixerMsgStr::h.str, &prev);
    if (!item)
        return -1;
    h = static_cast<uint16_t>(json_get_int_value(item));

    return 0;
}
```

**为什么这样更快？**

```
不传 prev：每次都从链表头开始  →  O(N) + O(N) + O(N) + O(N) = O(4N)
传   prev：从上次位置接着找    →  平均只需走 1~2 步       ≈ O(N)
```

对有几十个字段的对象，这是**数倍**的差距，而且字段越多收益越大。

> ⚠️ **前提**：字段必须按代码里的顺序出现在 JSON 中。
> 如果某个 key 出现在 `prev` **之前**，`json_get_object_item` 会绕回链表头继续找一圈（不会漏，但也不会快）。
> 所以**字段顺序不确定时，改用 4.4.1 的 `json_get_items` + 二分查找**。

#### 4.5.4 两种模式怎么选

| 场景 | 推荐 | 理由 |
|------|------|------|
| 要遍历数组全部元素 | **模式一（items）** | 下标 O(1)，且连续数组 cache 友好 |
| 字段多、顺序固定、只取一遍 | **模式二（prev）** | 接续查找，几乎不走链表 |
| 字段多、顺序不固定、按 key 反复查 | **模式一（items + 二分）** | O(logN)，与顺序无关 |
| 数组里套对象，逐个按 key 取 | **两者结合** | items 拿到元素，再按 key 取 |
| 只在循环外解析一次、字段很少 | 直接 `json_get_object_item(json, key, NULL)` | 不必优化 |

**两者可以叠加**：先看数组用 items，再在元素内按固定顺序用 prev。

### 4.6 增删改：编辑接口

```c
#include <stdio.h>
#include <string.h>
#include "json.h"

/* 建议参考 `4.5.1 基础：把键字符串常量化` 将 key 常量化而不是每次 `set_str` 计算 */
static void set_str(json_string_t *js, char *s)
{
    js->info.len = 0; /* 必须先清零len，否则update不会计算 */
    js->str = s;
    json_string_info_update(js); /* 填 len 和 escaped 标记 */
}

int main(void)
{
    json_object *root = json_create_object();

    /* 添加各种类型的值 */
    json_string_t jkey;

    set_str(&jkey, (char *)"Name");
    json_string_t jval;
    set_str(&jval, (char *)"LJSON");
    json_add_string_to_object(root, &jkey, &jval);

    set_str(&jkey, (char *)"Age");
    json_add_int_to_object(root, &jkey, 30);

    set_str(&jkey, (char *)"Score");
    json_add_double_to_object(root, &jkey, 9.9);

    set_str(&jkey, (char *)"Big");
    json_add_lint_to_object(root, &jkey, 1234567890123LL);

    set_str(&jkey, (char *)"Mask");
    json_add_hex_to_object(root, &jkey, 0xDEADBEEF);

    /* 添加一个数组 */
    set_str(&jkey, (char *)"Hobby");
    json_object *hobby = json_add_array_to_object(root, &jkey);
    json_string_t h1, h2;
    set_str(&h1, (char *)"Reading");
    set_str(&h2, (char *)"Walking");
    json_add_string_to_array(hobby, &h1);
    json_add_string_to_array(hobby, &h2);

    /* 修改 */
    json_object *age = json_get_object_item(root, "Age", NULL);
    json_set_int_value(age, 31);

    /* 删除（按 key / 按序号） */
    json_del_item_from_object(root, "Score");
    json_del_item_from_array(hobby, 0);

    /* 替换 */
    json_object *newage = json_create_int(32);
    set_str(&jkey, (char *)"NewAge");  /* 默认没有开启空键支持，所以替换前要先设置 */
    json_set_key(newage, &jkey); 
    json_replace_item_in_object(root, newage);

    /* 深拷贝 */
    json_object *copy = json_deepcopy(root);

    /* C11 泛型写法（或 C++ 重载写法），一行搞定，不用记类型后缀 */
    set_str(&jkey, (char *)"Generic");
    json_object_append(root, &jkey, 3.14159); /* 自动识别为 double */

    size_t len = 0;
    char *out = json_print_format(root, json_item_total_get(root), &len, NULL);
    printf("%s\n", out);

    json_memory_free(out);
    json_del_object(copy);
    json_del_object(root);
    return 0;
}
```

### 4.7 SAX 解析：回调驱动

SAX 解析不构建 DOM 树，每解析出一个值就回调你一次。适合"从超大文件里抽取字段"或"JSON 转 JSON"。

回调函数参考 [json_test.c](./json_test.c) 的 `_sax_parser_cb` 函数

### 4.8 SAX 打印：不建 DOM 直接输出

这是**推荐的默认打印方式**。数据本来就在你的结构里时，不要先建 DOM。

也可参考 [json_test.c](./json_test.c) 的 `test_json_sax_print` 函数

```c
#include <stdio.h>
#include "json.h"

/* 建议参考 `4.5.1 基础：把键字符串常量化` 将 key 常量化而不是每次 `set_str` 计算 */
static void set_str(json_string_t *js, char *s)
{
    js->info.len = 0; /* 必须先清零len，否则update不会计算 */
    js->str = s;
    json_string_info_update(js); /* 填 len 和 escaped 标记 */
}

int main(void)
{
    json_sax_print_hd hd = NULL;
    json_string_t jkey, jstr;
    size_t len = 0;
    char *out = NULL;

    /* 开始：格式化输出到字符串（第二个参数是预估的对象总数，用于一次性分配够缓冲） */
    hd = json_sax_print_format_start(100, NULL);

    /* 顶层左花括号 */
    json_sax_print_object_start(hd, NULL);

    set_str(&jkey, (char *)"Name");
    set_str(&jstr, (char *)"LengJing");
    json_sax_print_object_item(hd, &jkey, &jstr);

    set_str(&jkey, (char *)"Age");
    json_sax_print_object_item(hd, &jkey, 30);              /* C11 泛型：自动识别 int32 */

    set_str(&jkey, (char *)"Phone");
    set_str(&jstr, (char *)"18368887550");
    json_sax_print_object_item(hd, &jkey, &jstr);

    /* 嵌套数组 */
    set_str(&jkey, (char *)"Hobby");
    json_sax_print_array_start(hd, &jkey);
    set_str(&jstr, (char *)"Reading");
    json_sax_print_array_item(hd, &jstr);
    set_str(&jstr, (char *)"Walking");
    json_sax_print_array_item(hd, &jstr);
    set_str(&jstr, (char *)"Thinking");
    json_sax_print_array_item(hd, &jstr);
    json_sax_print_array_finish(hd);

    /* 顶层右花括号 */
    json_sax_print_object_finish(hd);

    /* 结束：拿到结果字符串 */
    out = json_sax_print_finish(hd, &len, NULL);
    printf("%s\n", out);
    json_memory_free(out);

    return 0;
}
```

**打印到文件（真流式）**只需把 start 换成 `json_sax_fprint_format_start`：

```c
/* 边生成边写文件，不需要先把整个字符串攒在内存里 */
hd = json_sax_fprint_format_start(100, "out.json", NULL);
/* ... 同样的一系列 json_sax_print_xxx ... */
json_sax_print_finish(hd, NULL, NULL);
```

### 4.9 文件流式解析与打印

```c
#include "json.h"

int main(void)
{
    json_object *json = NULL;
    json_mem_t mem;

    /* 模式5：DOM 文件流 + 内存池，边读边解析 + 池分配 */
    pjson_memory_init(&mem);
    json = json_fast_parse_file("big.json", &mem);

    /* 模式2 之外的文件流打印：边打印边写文件 */
    int item_total = pjson_memory_statistics(&mem.obj_mgr) / (int)sizeof(json_object);
    json_fprint_format(json, item_total, "out.format.json", NULL);

    pjson_memory_free(&mem);
    return 0;
}
```

> 真流式的价值：1.7 GB 的 `citm_catalog.json` 用模式 7 完成"读+解析+格式化+写"只需 **2946 ms**，且**峰值内存是常数级**。

### 4.10 数值引擎单独使用

`jnum.h` 可以不依赖 `json.h` 单独使用，是一个独立的数值转换库：

```c
#include <stdio.h>
#include "jnum.h"

int main(void)
{
    char buf[32];        /* 文档建议至少 27 字节 */
    int len;

    /* 数值 -> 字符串 */
    len = jnum_dtoa(3.14159265358979, buf);   printf("dtoa : %s (%d)\n", buf, len);
    len = jnum_itoa(-123456, buf);            printf("itoa : %s (%d)\n", buf, len);
    len = jnum_ltoa(1234567890123LL, buf);    printf("ltoa : %s (%d)\n", buf, len);
    len = jnum_htoa(0xDEADBEEF, buf);         printf("htoa : %s (%d)\n", buf, len);
    len = jnum_lhtoa(0x123456789ABCDEFULL, buf); printf("lhtoa: %s (%d)\n", buf, len);

    /* 字符串 -> 数值（内联版本，比 strtod/atoi 快 1~10 倍） */
    printf("atod : %g\n", jnum_atod("3.14159265358979"));
    printf("atoi : %d\n", jnum_atoi("-123456"));
    printf("atol : %lld\n", (long long)jnum_atol("1234567890123"));
    printf("atoh : %u\n", jnum_atoh("0xDEADBEEF"));

    /* 需要知道解析出的类型时用 jnum_parse */
    jnum_type_t type;
    jnum_value_t value;
    int used = jnum_parse("  0x1F", &type, &value);   /* 自动跳过前导空白 */
    printf("type=%d, hex=0x%X, used=%d\n", type, value.vhex, used);

    return 0;
}
```

### 4.11 二进制扩展类型

LJSON 支持二进制数据扩展，文本格式为 `<len:data>`（例如 `<8:\x01\x02...>`）：

```c
#include <string.h>
#include "json.h"
#include <stdio.h>
#include "json.h"

int main(void)
{
    unsigned int raw[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    /* 构造 */
    json_binary_t jbin;
    jbin.bin = raw;
    jbin.info.len = sizeof(raw);
    json_object *bin = json_create_binary(&jbin);

    /* 读取 */
    json_binary_t out;
    json_get_binary_value(bin, &out);
    printf("len = %u\n", out.info.len);

    /* 作为对象成员打印出来会是 <8:...> 的形式 */
    json_object *root = json_create_object();
    json_string_t jkey;
    jkey.str = (char *)"Binary";
    json_string_info_update(&jkey);
    json_add_binary_to_object(root, &jkey, &jbin);

    size_t len = 0;
    char *s = json_print_unformat(root, json_item_total_get(root), &len, NULL);
    printf("%s len=%d\n", s, (int)len);

    json_memory_free(s);
    json_del_object(root);
    return 0;
}
```

---

## 5. 编译与运行

### 5.1 编译与安装方法

**① 直接编译（最省事，两个 .c 文件即可）**

```sh
gcc -o ljson json.c jnum.c json_test.c -O2 -ffunction-sections -fdata-sections -W -Wall
```

如果只要数值引擎：

```sh
gcc -o jnum_test jnum.c jnum_test.c -O2 -W -Wall
```

**② [IMAKE](https://github.com/lengjingzju/cbuild-ng) 编译 + 安装**

```sh
make O=<编译输出目录> && make O=<编译输出目录> DESTDIR=<安装目录> install
```

> `install` 目标会把编译产物（库文件与 `json.h` / `jnum.h` 头文件）安装到 `DESTDIR` 指定的目录，
> 方便其它工程直接 `#include <json.h>` 并链接。

**③ 交叉编译 + 安装**

```sh
make O=<编译输出目录> CROSS_COMPILE=<交叉编译器前缀> && make O=<编译输出目录> DESTDIR=<安装目录> install
```

例如 ARM 交叉编译：

```sh
make O=obj-arm CROSS_COMPILE=arm-linux-gnueabihf- && make O=obj-arm DESTDIR=./install-arm install
```

**④ Windows（MSVC）**

使用 `Visual Studio 2022` 打开 `MSC\ljson\ljson.sln` 直接构建。

> 注：MSVC 下 `JSON_MANUAL_LOOP_UNFOLD` 与 `JNUM_MANUAL_LOOP_UNFOLD` 默认置 **0**（MSVC 上手工展开反而变慢），
> `jnum.h` 中的 `jnum_atoi` 等函数在 MSVC 下也不使用 `inline`（MSVC 使用 inline 后性能下降）。这些都是库内部自动处理的。

### 5.2 编译开关

**① 选择浮点数转字符串算法**

```sh
gcc -DJSON_DTOA_ALGORITHM=n ...      # 或 make DTOA=n
```

| n | 算法 | 说明 |
|:-:|------|------|
| **0** | **ldouble（自研，默认）** | 最快，17 位精度，始终最短精确表示 |
| 1 | C 标准库 `sprintf`（`%1.16g`） | 最慢，但行为完全标准 |
| 2 | 个人优化的 grisu2 | Google 的经典算法 |
| 3 | 个人优化的 dragonbox | 比 grisu2 更快的新算法 |

**② 选择大查找表或小查找表**

```sh
gcc -DUSING_SMALL_LUT=n ...          # 或 make SMALL=n
```

| n | 说明 |
|:-:|------|
| **0** | 大查找表（默认），速度可能更快一些 |
| 1 | 小查找表，代码段小约 **5 KB** |

> 小查找表适合指令缓存紧张或固件体积敏感的嵌入式场景。

### 5.3 运行方法

```sh
./ljson <json文件名> <测试序号1-7>          # 测试各种解析模式和打印模式
./ljson <json文件名> <测试序号1-3> diswr    # 测试各种解析模式和打印模式，不生成文件
./ljson <json文件名> <测试次数大于7>        # 测试模式3（复用模式）重复解析指定次
./ljson <json文件名> <测试次数大于7> print  # 测试复用打印模式，重复从DOM打印成字符串指定次
```

对应关系：

| 参数 | 调用的 API | 模式 |
|------|-----------|:----:|
| `1` | `json_parse_str()` | DOM 经典模式 |
| `2` | `json_fast_parse_str()` | DOM 内存池模式 |
| `3` | `json_reuse_parse_str()` | DOM 复用模式 |
| `4` | `json_parse_file()` | DOM 文件流模式 |
| `5` | `json_fast_parse_file()` | DOM 文件流 + 内存池 |
| `6` | `json_sax_parse_str()` | SAX 模式 |
| `7` | `json_sax_parse_file()` | SAX 文件流模式 |
| `>7` | 模式3 + `pjson_memory_refresh()` 循环 | 复用模式重复解析 |
| `>7 print` | `json_print_format/unformat()` 循环 | 复用模式重复打印 |

### 5.4 调试方法

设置 `json.c` 中的 `JSON_ERROR_PRINT_ENABLE` 为 `1` 后重新编译，解析/打印出错时会打印出错的函数名、行号以及出错位置附近的文本，便于定位非法 JSON。

也可在编译时直接指定：

```sh
gcc -DJSON_ERROR_PRINT_ENABLE=1 -o ljson json.c jnum.c json_test.c -O2 -W -Wall
```

### 5.5 解析配置

以下宏位于 **`json.c`** 中，下表列出的是**源码中的默认值**：

| 宏 | 默认值 | 说明 | JSON5 |
|----|:------:|------|:------:|
| `JSON_PARSE_SKIP_COMMENT` | **0** | 是否允许 C 风格单行/多行注释 | ✅ |
| `JSON_PARSE_LAST_COMMA` | 1 | 是否允许数组/对象最后一个元素带尾逗号 | ✅ |
| `JSON_PARSE_EMPTY_KEY` | 0 | 是否允许键为空字符串 | ❌ |
| `JSON_PARSE_SPECIAL_CHAR` | 1 | 是否允许字符串中有特殊字符（如换行符） | ✅ |
| `JSON_PARSE_SPECIAL_QUOTES` | **0** | 是否允许单引号字符串/键、无引号键 | ✅ |
| `JSON_PARSE_HEX_NUM` | 1 | 是否允许十六进制数（如 `0xFF`） | ✅ |
| `JSON_PARSE_SPECIAL_NUM` | 1 | 是否允许特殊数字：`+99` `.1234` `10.` `001` 等 | ✅ |
| `JSON_PARSE_SPECIAL_DOUBLE` | 1 | 是否允许 `NaN` `Infinity` `-Infinity` | ✅ |
| `JSON_PARSE_SINGLE_VALUE` | 1 | 是否允许不以数组/对象开头的 JSON 值 | ❌ |
| `JSON_PARSE_FINISHED_CHAR` | 0 | 解析完成后是否忽略尾部字符检查 | ❌ |
| `JSON_ERROR_PRINT_ENABLE` | 0 | 是否打印错误信息 | — |

配套的行为开关（同样在 `json.c`）：

| 宏 | 默认值 | 说明 |
|----|:------:|------|
| `JSON_DTOA_ALGORITHM` | 0 | 浮点转字符串算法（见 5.2） |
| `JSON_MANUAL_LOOP_UNFOLD` | 1（MSVC 为 0） | 是否手工展开解析/打印循环 |
| `JSON_PRINT_UTF16_SUPPORT` | 0 | 打印时是否把控制字符转成 `\u00XX` |
| `USING_SMALL_LUT` | 0 | 数值引擎是否用小查找表 |

> **关于 JSON5 的默认策略（重要）**
>
> LJSON **完整支持 JSON5**，但默认配置是**速度优先**：
>
> * 位于解析热路径、代价较高的两条 JSON5 特性 —— **注释**（`JSON_PARSE_SKIP_COMMENT`）与**单引号/无引号键**（`JSON_PARSE_SPECIAL_QUOTES`）—— **默认为 0（关闭）**，因为它们会让每次取字符都多一次判断。
> * 其余对热路径影响很小的 JSON5 特性（尾逗号、特殊字符、十六进制、特殊数字、`NaN`/`Infinity`）**默认为 1（开启）**。
>
> 需要完整的 JSON5 / 宽松解析时，把对应宏置 `1` 重新编译即可。**开启注释与特殊引号会显著影响解析速度**，这是有意的设计取舍。

> **关于合规性**
>
> 如果需要 100% 符合 [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) 的严格校验，请把 `JSON_PARSE_EMPTY_KEY` 置 **1**，其它值全部置 **0**。

---

## 6. 性能测试

### 6.1 测试说明与环境

**测试样本**

样本来自 [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) 项目。为了压测大文件场景，做了重复拼接放大：

| 样本 | 原始大小 | 放大后 | 数据特征 |
|------|---------:|-------:|----------|
| `twitter.json` | 631,514 B | ×1000 = **603 MB** | 字符串 + 整数密集，中等嵌套 |
| `citm_catalog.json` | 1,727,204 B | ×1000 = **1.7 GB** | 多层嵌套对象 |
| `canada.json` | 2,251,051 B | ×100 = **215 MB** | **浮点密集**（地理坐标） |

**测试脚本**

```sh
#!/bin/bash

src=$1

if [ -z $src ] || [ ! -e $src ]; then
	echo "Usage: $0 <json file>"
	exit 1
fi

run_cmd() {
	printf "%-15s " $1
	eval $@
	sync
	sleep 0.1
}

for i in `seq 1 7`; do
	run_cmd ./ljson $src $i diswr
done

run_cmd ./cjson $src
run_cmd ./rapidjson $src
run_cmd ./yyjson $src
run_cmd ./yyjson $src 1
run_cmd ./strdup $src
```

对比库的编译（`-O2`，默认选项）：

```sh
gcc -o cjson cjson_test.c cJSON.c -O2               # cJSON
g++ -o rapidjson rapidjson_test.c -Iinclude -O2     # RapidJSON
gcc -o yyjson yyjson_test.c src/yyjson.c -Isrc -O2  # yyjson
gcc -o strdup strdup_test.c -O2                     # strdup和strlen
```

**计时口径（读数据前必须看清楚）**

| 模式 | 输出项 | 含义 |
|:----:|--------|------|
| DOM 1 / 2 / 3 | `read` 与 `parse` 分开 | `parse` **不含**文件读取时间 |
| DOM 1 / 2 / 3 | `format` / `unformat` | 打印到字符串的时间，**不含**写文件 |
| DOM 4 / 5 | `read+parse` | 边读边解析，无法拆分，**含**读取 |
| DOM 4 / 5 | `format+write` / `unformat+write` | 边打印边写，**含**写文件 |
| SAX 6 | `parse+format` | 解析与格式化在同一条流水线上 |
| SAX 7 | `read+parse+format+write` | 全流程一条流水线，**含读与写** |
| `strdup` | `strdup` / `strlen` | **内存带宽基线**：把整个文件复制一遍 / 扫一遍要多久 |

**对照组的两种 yyjson 模式**

* `unm`（unmutable）：只读模式，解析出的结构不可修改
* `mut`（mutable）：可编辑模式，解析出的结构可以修改


### 6.2 Intel i7-1260P（v5.0.5，最新）★

> 测试平台: PC | CPU: Intel i7-1260P | OS: Debian 13 (VMWare) | LJSON v5.0.5<br>
> 测试结果: LJSON 比 cJSON 解析最快可达 **562%**，打印最快可达 **2283%**；LJSON 比 RapidJSON 解析最快可达 **172%**，打印最快可达 **285%**；LJSON 比 yyjson（可编辑模式）解析最快可达 **145%**，打印最快可达 **168%**（耗时**不含**文件读写时间）

![x86_64-Linux测试结果2](image/test_for_x86_64-2.png)

**这是本 README 的基准数据，后面 6.3 / 6.4 两节的终端实测输出就来自同一批测试。**

**图片结论 × 实测数据：逐条交叉验证**

图上那六个"最快可达"的百分比，都能在 6.3 节的终端实测输出里**精确复现**（取整后完全一致）。这也证明了**图片数据与终端输出同源**，可以放心引用：

| 图片给出的结论 | 复现它的样本与指标 | LJSON 最优值 | 对比库耗时 | 计算 | 结果 |
|----------------|-------------------|------------:|-----------:|------|-----:|
| vs cJSON 解析 **562%** | canada · parse | **281**（模式3） | 1862 | (1862−281)/281 | **562.6%** ✓ |
| vs cJSON 打印 **2283%** | canada · unformat | **252**（模式1） | 6007 | (6007−252)/252 | **2283.7%** ✓ |
| vs RapidJSON 解析 **172%** | twitter · parse | **320**（模式3） | 871 | (871−320)/320 | **172.2%** ✓ |
| vs RapidJSON 打印 **285%** | twitter · unformat | **175**（模式2） | 675 | (675−175)/175 | **285.7%** ✓ |
| vs yyjson(mut) 解析 **145%** | twitter · parse | **320**（模式3） | 784 | (784−320)/320 | **145.0%** ✓ |
| vs yyjson(mut) 打印 **168%** | twitter · format | **222**（模式2） | 596 | (596−222)/222 | **168.5%** ✓ |

> 读这张表要注意一点：百分比用的是**每个指标下 LJSON 的最快模式**（所以叫"最快可达"），
> 因此解析统一取模式 3，而打印有时取模式 1、有时取模式 2 —— 它们本来就是不同指标下的最优值。
> 若想看"同一个模式下"的完整对比，直接看 [6.3 最新终端实测：7 种模式全对比](#63-最新终端实测7-种模式全对比) 的三张表。

**简要总结**

* 这是**最新版本**的图，且**不含文件读写**，更能反映库本身的实力。
* 相对 **cJSON**：解析最快 **5.6 倍**，打印最快 **22.8 倍**（浮点密集的 canada 场景）。
* 相对 **RapidJSON**：解析最快 **1.7 倍**、打印最快 **2.9 倍**。
* 相对 **yyjson 可编辑模式**：解析最快 **1.45 倍**、打印最快 **1.68 倍** —— 这是 LJSON 最关键的差异化优势：**同为可编辑模式，LJSON 更快**。

> 关于历史数据的说明：6.5 / 6.6 两节是 **v3 时期**某一版本的测试结果（ARM Cortex-A53 与 i7-10700），
> 用于展示 LJSON 在弱 CPU 与旧平台上的长期表现，**不代表当前版本**。
> 由于功能增加、逻辑优化等修改，最新版本不一定完全符合历史结果（例如采用更精确的数字处理后 `canada.json` 的解析性能会下降；优化复用模式的分支预测后解析性能会上升）。


### 6.3 最新终端实测：7 种模式全对比

以下是 `json_test.sh` 与 `./ljson` 的原始终端输出，**未做任何加工**。

<details>
<summary><b>展开：`1000×twitter.json`（603 MB）</b></summary>

```sh
lengjing@lengjing:~/data/json/json-test$ ./json_test.sh 1000xtwitter.json
-rw-rw-r-- 1 lengjing lengjing 603M Aug 19  2025 1000xtwitter.json
./ljson         [dom] 1. read=156   parse=952   format=272   unformat=222
./ljson         [dom] 2. read=157   parse=414   format=222   unformat=175
./ljson         [dom] 3. read=152   parse=320   format=234   unformat=182
./ljson         [dom] 4. read+parse=1546        format+write=350        unformat+write=303
./ljson         [dom] 5. read+parse=987         format+write=318        unformat+write=240
./ljson         [sax] 6. read=179   parse+format=785        write=177
./ljson         [sax] 7. read+parse+format+write=1374
./cjson         [dom] read=171  parse=1512  format=1037 unformat=948
./rapidjson     [dom] read=152  parse=871   format=752  unformat=675
./yyjson        [unm] read=151  parse=435   format=353  unformat=261
./yyjson        [mut] read=153  parse=784   format=596  unformat=295
./strdup        [str] read=155  strdup=151  strlen=48   length=631516002
```

</details>

<details>
<summary><b>展开：`1000×citm_catalog.json`（1.7 GB）</b></summary>

```sh
lengjing@lengjing:~/data/json/json-test$ ./json_test.sh 1000xcitm_catalog.json
-rw-rw-r-- 1 lengjing lengjing 1.7G Sep 15  2025 1000xcitm_catalog.json
./ljson         [dom] 1. read=440   parse=1786  format=558  unformat=414
./ljson         [dom] 2. read=425   parse=857   format=447  unformat=334
./ljson         [dom] 3. read=437   parse=825   format=472  unformat=364
./ljson         [dom] 4. read+parse=3165        format+write=682        unformat+write=529
./ljson         [dom] 5. read+parse=2068        format+write=547        unformat+write=400
./ljson         [sax] 6. read=435   parse+format=1552       write=259
./ljson         [sax] 7. read+parse+format+write=2946
./cjson         [dom] read=433  parse=3427  format=1677 unformat=1503
./rapidjson     [dom] read=434  parse=1411  format=1089 unformat=708
./yyjson        [unm] read=431  parse=1206  format=635  unformat=330
./yyjson        [mut] read=430  parse=1890  format=719  unformat=410
./strdup        [str] read=438  strdup=433  strlen=150  length=1727206002
```

</details>

<details>
<summary><b>展开：`100×canada.json`（215 MB，浮点密集）</b></summary>

```sh
lengjing@lengjing:~/data/json/json-test$ ./json_test.sh 100xcanada.json
-rw-rw-r-- 1 lengjing lengjing 215M Aug 23  2025 100xcanada.json
./ljson         [dom] 1. read=64    parse=522   format=283  unformat=252
./ljson         [dom] 2. read=59    parse=286   format=267  unformat=263
./ljson         [dom] 3. read=62    parse=281   format=273  unformat=260
./ljson         [dom] 4. read+parse=794         format+write=310        unformat+write=294
./ljson         [dom] 5. read+parse=503         format+write=301        unformat+write=282
./ljson         [sax] 6. read=60    parse+format=682        write=78
./ljson         [sax] 7. read+parse+format+write=897
./cjson         [dom] read=61   parse=1862  format=5956 unformat=6007
./rapidjson     [dom] read=62   parse=342   format=769  unformat=624
./yyjson        [unm] read=62   parse=336   format=435  unformat=273
./yyjson        [mut] read=60   parse=468   format=438  unformat=306
./strdup        [str] read=65   strdup=59   strlen=19   length=225105302
```

</details>

**怎么读这三组数据（单位 ms，越小越好）**

**① LJSON 的七种模式，谁最快？**

* **解析最快：模式 3（复用模式）**，三个数据集上都是最优 —— twitter **320**、citm **825**、canada **281**。
* **打印最快：模式 2（内存池）** —— twitter 格式化 **222**、压缩 **175**；citm 格式化 **447**、压缩 **334**；canada 格式化 **267**（注意 canada 的压缩打印是模式 1 的 **252** 最快，模式 2 为 263、模式 3 为 260，三者差距很小）。
* **模式 2 相对模式 1 的解析提升非常可观**：twitter 414 vs **952**（快 2.3 倍）、citm 857 vs **1786**（快 2.1 倍）、canada 286 vs **522**（快 1.8 倍）—— 内存池本身就值一倍多。
* **模式 3 再叠加原地复用**：twitter 320 vs 414（又快 29%）、citm 825 vs 857、canada 281 vs 286。
* 结论：**内存池 + 字符串原地复用，各自都在实打实地省时间，且它们是可叠加的。**

**② 对比其它库（以 LJSON 模式 3 · 可编辑复用 为基准）**

* **vs cJSON**：解析快 **4.2~6.6 倍**（twitter 1512→320 为 4.7 倍、citm 3427→825 为 4.2 倍、canada 1862→281 为 6.6 倍）；**格式化打印快 3.4~23 倍**（canada 5956→273 为 **21.8 倍**，浮点密集场景差距最大）。
* **vs RapidJSON**：解析快 **1.2~2.7 倍**（twitter 871→320 为 2.7 倍、citm 1411→825 为 1.7 倍、canada 342→281 为 1.2 倍）；格式化打印快 **1.9~3.7 倍**（canada 769→273 为 2.8 倍、twitter 752→234 为 3.2 倍、citm 1089→472 为 2.3 倍）。
* **vs yyjson 可编辑模式（mut）**：解析快 **1.7~2.5 倍**（twitter 784→320 为 2.45 倍、citm 1890→825 为 2.3 倍、canada 468→281 为 1.7 倍）；格式化打印快 **1.5~2.6 倍**（twitter 596→234 为 2.55 倍、canada 438→273 为 1.6 倍、citm 719→472 为 1.5 倍）。
* **vs yyjson 只读模式（unm）**：解析仍然快 **20%~46%**（twitter 435→320 快 36%、citm 1206→825 快 46%、canada 336→281 快 20%）。

**③ 诚实地说出落后的地方**

* `citm_catalog` 的**压缩打印**：yyjson 只读模式 330 ms，LJSON 模式 3 为 364 ms（**慢约 10%**），模式 2 为 334 ms（基本持平）。这是 LJSON 唯一明确落后的一项，原因是 citm 的数据以长字符串与整数为主，yyjson 的只读 + 宏展开在这种纯拷贝路径上略占优。
* `canada` 的**解析**领先幅度收敛到 20%（336→281）：LJSON 采用了更精确的数字处理，浮点解析要付出额外代价。**这是有意换取的正确性。**

**④ 和内存带宽比一比（一个很有说服力的视角）**

`strdup` 一行给出了纯内存拷贝的基线：twitter 把 603 MB 复制一遍需要 **151 ms**，而 LJSON 模式 3 **完整解析（含全部 JSON 语义）只需 320 ms**。

> 也就是说：**解析全部 JSON 语义的代价，仅约为"把数据原样复制一遍"的 2.1 倍**（citm 为 637/433 ≈ 1.9 倍）。
> 在此之前，多数 JSON 库的解析耗时是内存拷贝的 5~10 倍。

**⑤ 流式模式：时间几乎不增加，内存从 GB 级降到 KB 级**

流式常被误解为"用时间换内存"。实测给出了更有意思的答案（模式 7 是"读+解析+格式化+写"一条流水线，模式 1 只算"读+解析+格式化"、不含写）：

* twitter：模式 7 = **1374 ms**，模式 1 的 read+parse+format = 156+952+272 = **1380 ms** → **持平**
* citm：模式 7 = **2946 ms**，模式 1 = 440+1786+558 = **2784 ms** → **仅多 5.8%**
* canada：模式 7 = **897 ms**，模式 1 = 64+522+283 = **869 ms** → **仅多 3.2%**

> 模式 7 **多做了一步写文件**，总耗时却和"只读+只解析+只格式化（不写）"的经典 DOM 方案基本相当。
> 而代价方面：经典 DOM 处理 1.7 GB 的 citm 需要**数 GB 内存**建树，模式 7 只需要**常数级缓冲（KB 级）**。
> 换句话说：**流式不是用时间换内存，而是把原本串行的四步压成了一条流水线。**

如果确实需要 DOM 树、又不想一次性读入文件，**模式 5** 是折中选择：citm 的 `read+parse` 仅 2068 ms，相当于 **835 MB/s 的边读边解析吞吐**；canada 为 503 ms（约 448 MB/s）；twitter 为 987 ms（约 640 MB/s）。

### 6.4 复用模式：重复解析 / 重复打印吞吐

这组测试模拟"高频消息收发"：把同一份 JSON 反复解析 / 反复打印，统计总耗时。

<details>
<summary><b>展开：模式3 重复解析的原始输出</b></summary>

```sh
lengjing@lengjing:~/data/json/json-test$ ./ljson twitter.json 1000
Repeat 1000 times, LJSON reuse parse costs 232876723 ns
lengjing@lengjing:~/data/json/json-test$ ./ljson citm_catalog.json 1000
Repeat 1000 times, LJSON reuse parse costs 637765784 ns
lengjing@lengjing:~/data/json/json-test$ ./ljson canada.json 100
Repeat 100 times, LJSON reuse parse costs 263921066 ns
```

</details>

<details>
<summary><b>展开：重复打印的原始输出</b></summary>

```sh
lengjing@lengjing:~/data/json/json-test$ ./ljson twitter.json 1000 print
Repeat 1000 times, original size=631514, LJSON reuse print costs: format ns=120646144 len=559706; unformat ns=90114731, len=466906
lengjing@lengjing:~/data/json/json-test$ ./ljson citm_catalog.json 1000 print
Repeat 1000 times, original size=1727204, LJSON reuse print costs: format ns=275329477 len=805363; unformat ns=193317888, len=500299
lengjing@lengjing:~/data/json/json-test$ ./ljson canada.json 100 print
Repeat 100 times, original size=2251051, LJSON reuse print costs: format ns=232885613 len=2512356; unformat ns=218621631, len=2062031
```

</details>

**换算成单次耗时与吞吐**

| 数据集 | 单份大小 | 单次解析 | 解析吞吐 | 单次格式化打印 | 格式化吞吐 | 单次压缩打印 | 压缩吞吐 |
|--------|--------:|--------:|---------:|--------------:|-----------:|------------:|---------:|
| twitter | 631,514 B | **233 µs** | **2.71 GB/s** | 121 µs | **4.64 GB/s** | 90 µs | **5.18 GB/s** |
| citm_catalog | 1,727,204 B | **638 µs** | **2.71 GB/s** | 275 µs | 2.93 GB/s | 193 µs | 2.59 GB/s |
| canada | 2,251,051 B | 2,639 µs | 0.85 GB/s | 2,329 µs | 1.08 GB/s | 2,186 µs | 0.94 GB/s |

> 吞吐 = 输入（或输出）字节数 ÷ 单次耗时。例如 twitter：631514 B ÷ 232.877 ms/1000 = 2.71 GB/s。

**简要总结**

* 非浮点密集数据（twitter / citm）稳定跑在 **2.7 GB/s 解析**、**2.6~5.2 GB/s 打印**。
* canada 是浮点密集数据，吞吐降到 **0.85~1.08 GB/s** —— 浮点转换本身成为瓶颈；但对照 6.5 节，cJSON 在同样数据上要 1862 ms 解析、5956 ms 格式化打印，差距依然是**数量级**的。
* 这组测试的关键不在于"快"，而在于**复用模式下循环期间库内部零堆分配**（详见 [4.2 首选方案：零堆内存分配模式](#42-首选方案零堆内存分配模式)），所以延迟是稳定的、无抖动的 —— 这对服务端消息处理比峰值吞吐更重要。

### 6.5 ARM Cortex-A53（历史数据，v3 时期）

> 测试平台: ARM64 开发板 | CPU: ARM Cortex-A53 | OS: Linux-5.15<br>
> 测试结果: LJSON 比 cJSON 解析最快可达 **475%**，打印最快可达 **2836%**；LJSON 比 RapidJSON 解析最快可达 **131%**，打印最快可达 **147%**（耗时含文件读写时间）

![AARCH64-Linux测试结果](image/test_for_aarch64.png)

**简要总结**

* 即使在 Cortex-A53 这种低端内核上，LJSON 相对 cJSON 的优势反而**更大**（打印最多快 28 倍），说明它的优化对弱 CPU 同样有效 —— 省下的都是实打实的指令与内存访问。
* 相对 RapidJSON 也有稳定优势（解析最多快 1.3 倍、打印最多快 1.5 倍）。
* 此组数据**含文件读写时间**，因此百分比被 I/O 摊薄了。

### 6.6 Intel i7-10700（历史数据，v3 时期）

> 测试平台: PC | CPU: Intel i7-10700 | OS: Ubuntu 18.04 (VirtualBox)<br>
> 测试结果: LJSON 比 cJSON 解析最快可达 **560%**，打印最快可达 **3184%**；LJSON 比 RapidJSON 解析最快可达 **75%**，打印最快可达 **133%**（耗时含文件读写时间）

![x86_64-Linux测试结果](image/test_for_x86_64.png)

**简要总结**

* 桌面级 x86 上，相对 cJSON 的打印优势达到 **31 倍**，解析优势 **5.6 倍**。
* 相对 RapidJSON 解析快 75%、打印快 33%（注意此组**含文件读写**，I/O 会摊薄差距）。
* 这组测试还包含了 JsonCpp、nlohmann 等 C++ 库作为对照。

### 6.7 数值转换性能

**测试方法**

```sh
make O=obj
./benchmark/benchmark.sh obj
./benchmark/benchmark.sh obj atod
```

**测试环境**

> 测试平台: PC | CPU: Intel i7-1260P | OS: Ubuntu 20.04 (VMWare)<br>
> 测试平台: PC | CPU: Intel i7-1260P | OS: Windows 11 (MSVC 2023)<br>
> 测试命令(dtoa): `./jnum_test <num> 10000000` # 测试千万次<br>
> 测试命令(atod): `./jnum_test a <num> 10000000` # 测试千万次

![数值字符串转换测试结果](image/test_for_ldouble_zh.png)

**测试结论**

* **dtoa（数值 → 字符串）**：ldouble 比 `sprintf` 快一个数量级（某些用例可快 **40 倍**），也比 grisu2 和 dragonbox 快一些；在精度范围内可以保证最短，与 `printf` 输出一致。
* **atod（字符串 → 数值）**：ldouble 比 `strtod` 大多数情况下快 **2~3 倍**（某些用例可快 **10 倍**）。

**图中可以看到的细节**（四张分组柱状图，分别为 Linux / Windows 下的 dtoa 与 atod 加速倍数）：

* **dtoa 图**：`ldouble`（蓝）、`grisu2`（橙）、`dragonbox`（绿）三组柱子，横轴类别为短小数 / 长小数 / 整数 / 科学计数法 / 极大值。
  * Linux 下三类算法相对 `sprintf` 的加速大致在 **10~45 倍**区间，`ldouble` 在每个类别上都是最高的那根柱子。
  * Windows（MSVC）下的加速倍数**明显更高**，在"极大值"类别上 `ldouble` 达到约 **200 倍**量级，`dragonbox` 约 175 倍，`grisu2` 约 130 倍 —— 说明 MSVC 的 `sprintf` 实现相对更慢，而 ldouble 不受标准库实现质量影响。
* **atod 图**：只有 `ldouble` 一组蓝色柱子（相对 `strtod`）。Linux 下最高约 **10 倍**，Windows 下最高约 **16~17 倍**。

---

## 7. LJSON 为什么这么快：代码级深度解析

这一章回答一个问题：**在没有 SIMD、没有内联汇编、纯 C99 的前提下，LJSON 的性能是从哪里来的？**

答案是：**它不是靠某一招"银弹"，而是把二十几种各自只贡献几个百分点的技术，按正确的层次叠加起来。** 下面先讲方法论，再逐条结合源码说明。

### 7.1 方法论：测量 → 分析 → 优化 → 验证

LJSON 的每一项优化都遵循一个闭环，而不是"听说这样快就加上"：

```
   ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐
   │  测量    │───►│  分析    │───►│  优化    │───►│  验证    │
   │ Profile  │    │ Analyze  │    │ Optimize │    │  Test    │
   └──────────┘    └──────────┘    └──────────┘    └────┬─────┘
        ▲                                               │
        └─────────────── 未达标则回退 ───────────────────┘
```

**"验证"这一步在 LJSON 里留下了几个真实的痕迹** —— 它们都是"实测后推翻了想当然"的证据：

| 想当然的做法 | 实测结论 | 源码里的处理 |
|--------------|----------|--------------|
| 手工循环展开一定更快 | **MSVC 下反而变慢** | `#if defined(_MSC_VER)` → `JSON_MANUAL_LOOP_UNFOLD 0` |
| JSON5 全开更"完整" | 热路径上**每次取字符多一次判断** | 默认关闭，需要的人自己开 |
| 使用局部变量预先分配更快 | json_sax_print_value 按 _json_print_value_format 同样处理变慢了一点 | 保留2者各自不同的实现 |

### 7.2 优化层次模型：收益从哪里来

性能优化的收益是**分层的**，越往上收益越大、越应该先做：

```
收益大 ─────────────────────────────────────────────────► 收益小

  算法/数据结构  ►  空间换时间  ►  精度换效率  ►  内存层次  ►  减少分配  ►  CPU微架构  ►  编译器  ►  微优化
   10x~1000x        2x~100x       2x~50x        2x~10x       1.5x~5x      1.5x~4x     1.1x~2x    <1.1x
```

**LJSON 的每一项技术，都落在明确的层次上：**

| 层次 | 典型收益 | LJSON 的对应技术 | 章节 |
|------|---------:|------------------|:----:|
| **算法 / 框架选择** | 10×~1000× | **7 种解析模式 × 4 种打印模式**（选对路径） | [7.4 算法与框架：选对路是最省力的优化](#74-算法与框架选对路是最省力的优化) |
| **算法 / 数据结构** | 10×~1000× | 自研 **ldouble** 数值转换算法 | [7.21 精度换效率：ldouble 的取舍](#721-精度换效率ldouble-的取舍) |
| **空间换时间** | 2×~100× | 查找表、内存池**预分配冗余**、打印缓冲**多分配 64 字节** | [7.14 空间换时间：查表法](#714-空间换时间查表法) [7.10 内存池：bump 分配器，两次比较搞定一次分配](#710-内存池bump-分配器两次比较搞定一次分配) [7.7 打印热路径：局部变量 `cur` + `_XXX_SYNC` 宏 + 多分配 64 字节](#77-打印热路径局部变量-cur--_xxx_sync-宏--多分配-64-字节) |
| **精度换效率** | 2×~50× | ldouble 的 17 位精度与最短表示取舍 | [7.21 精度换效率：ldouble 的取舍](#721-精度换效率ldouble-的取舍) |
| **内存层次** | 2×~10× | 32 字节 `json_object`、连续 `json_items` 数组、小查找表 | [7.8 数据结构：32 字节的 json_object](#78-数据结构32-字节的-json_object) [7.13 访问加速：json_items 与哈希二分查找](#713-访问加速json_items-与哈希二分查找) |
| **减少动态分配** | 1.5×~5× | bump 内存池、原地字符串复用、零堆分配循环 | [7.10 内存池：bump 分配器，两次比较搞定一次分配](#710-内存池bump-分配器两次比较搞定一次分配) [7.11 零拷贝：原地复用原始字符串](#711-零拷贝原地复用原始字符串) |
| **CPU 微架构** | 1.5×~4× | `likely/unlikely` 分支预测、手动循环展开、**局部 `cur` 变量**、冷热分离 | [7.17 分支预测与手动循环展开](#717-分支预测与手动循环展开) [7.7 打印热路径：局部变量 `cur` + `_XXX_SYNC` 宏 + 多分配 64 字节](#77-打印热路径局部变量-cur--_xxx_sync-宏--多分配-64-字节) [7.5 冷热分离：把不常跑的代码挪出热路径](#75-冷热分离把不常跑的代码挪出热路径) |
| **编译器** | 1.1×~2× | 编译期配置开关、`static inline`、`__attribute__((always_inline))` | [7.20 编译期配置：把运行期开关变成不存在的代码](#720-编译期配置把运行期开关变成不存在的代码) |
| **微优化** | <1.1× | 位域复用、单次 `memcpy` 定长拷贝、快速整除 | [7.8 数据结构：32 字节的 json_object](#78-数据结构32-字节的-json_object) [7.16 批量操作：一次搬运 4/8/16 字节](#716-批量操作一次搬运-4816-字节) [7.15 快速整除：把除法换成乘法](#715-快速整除把除法换成乘法) |

**这张表最重要的信息在最上面两行**：LJSON 最大的性能收益来自**框架层（选对模式）**和**算法层（ldouble）**，而不是底层的微优化。

### 7.3 阿姆达尔定律视角：为什么先"选路"而不是"抠指令"

阿姆达尔定律：若某部分占总时间比例 $p$，将其加速 $S$ 倍，整体加速比为

$$Speedup = \frac{1}{(1-p) + p/S}$$

| 热点占比 $p$ | 加速 $S=2$ | 加速 $S=10$ | 加速 $S=\infty$ |
|:---:|:---:|:---:|:---:|
| 10% | 1.05× | 1.09× | 1.11× |
| 50% | 1.33× | 1.82× | 2.0× |
| 90% | 1.82× | 5.26× | **10.0×** |

**结论：优化占比 10% 的代码，即使把它加速到无穷快，整体也只有 11% 的提升。**

这正是 LJSON 优先做"框架与算法"的原因 —— 内存分配与字符串拷贝在 JSON 解析中占比极大（接近 90%），把它整个换掉（内存池 + 原地复用），收益远大于在字符判断上省几个周期。实测佐证（twitter 样本）：

```
模式1 → 模式2：换内存策略（malloc → 内存池）        952 ms → 414 ms   2.3×
模式2 → 模式3：换拷贝策略（复制 → 原地复用）        414 ms → 320 ms   1.29×
```

**两处改动没有触碰任何一行字符解析逻辑**，却拿到了 3 倍的整体提升 —— 这就是"先优化占比最大的部分"。

### 7.4 算法与框架：选对路是最省力的优化

LJSON 提供的 7+4 模式矩阵（见[第三章 架构：7 种解析模式 × 4 种打印模式](#三-架构7-种解析模式--4-种打印模式)）本身就是最大的一笔性能收益。

**三个正交开关，决定了七个模式**

```
┌─ 是否构建 DOM 树？ ───── DOM（1/2/3/4/5）     vs SAX（6/7）
├─ 内存从哪来？ ────────── malloc（1/4）        vs 内存池（2/3/5）
└─ 数据从哪读？ ────────── 内存字符串（1/2/3/6） vs 文件流（4/5/7）
```

**在哪一层选，就决定了上限在哪**：

| 决策 | 影响的对象 | 量级 |
|------|-----------|------|
| DOM vs SAX | 是否构建整棵树（N 次分配 + N 次挂链） | 数倍 |
| malloc vs 内存池 | 每次分配的成本（百纳秒 vs 一条指令） | 2 倍 |
| 复制 vs 原地复用 | 每个字符串的一次 malloc + memcpy | 30% |
| 文件流 vs 全量读入 | 峰值内存（GB 级 vs KB 级） | 内存降几个数量级 |

**打印侧同理**：数据是"先建 DOM 再打印"还是"SAX 直打"，差的是整棵树的构建与销毁。所以[3.3 选型建议](#33-选型建议)给出的默认建议是 —— **解析用模式 3，打印用 SAX 直打**。

### 7.5 冷热分离：把不常跑的代码挪出热路径

这是 LJSON 里**最典型、也最容易写错**的一类优化。原则只有一句话：

> **不经常运行的部分单独分出（做成非 `inline` 的函数），经常运行的部分做得更小并 `inline`。**

**为什么要这样做？** 一个功能往往有"快分支"和"慢分支"两条路（例如字符串"有转义 / 无转义"、内存"当前块够 / 要新开一块"）。如果把它们写成一个大函数：

* 函数体积变大，**很可能无法被内联**，每次调用都要付出函数调用开销；
* 冷分支的代码**占据热路径的指令缓存（i-cache）**，把真正高频执行的代码挤出去；
* 冷分支的额外分支会**干扰分支预测器**，也拖累寄存器的分配。

把它们拆开之后：热分支小到可以 `inline`，冷分支被"挪出"热路径，只在真正需要时才付出一次函数调用的代价。

#### 7.5.1 两种写法

同一个功能有热/冷两个分支时，LJSON 用下面两种方式之一来拆：

**① 完整函数 `inline` 且直接包含热分支，冷分支单独实现成一个非 `inline` 的函数，由完整函数调用**

```
调用点 ──► [inline] 完整函数（含热分支）
                │
                └─(罕见)─► [非 inline] 冷分支函数
```

**② 热分支单独做成一个 `inline` 函数，失败（或"没做完"）时，再调用完整函数或冷分支函数**

```
调用点 ──► [inline] 热分支函数 ──(失败)──► [非 inline] 完整函数 / 冷分支函数
```

方式 ② 就是下一节 [7.6 两阶段字符串解析：快速路径前置 + 完整路径兜底](#76-两阶段字符串解析快速路径前置--完整路径兜底) 的结构 —— 那里有完整的源码分析。

#### 7.5.2 LJSON 里的三处实例

| 功能 | 热版本（inline，直接含热分支） | 冷版本（非 inline，单独函数） | 热版本的特点 |
|------|------------------------------|------------------------------|--------------|
| 打印字符串 | `_json_print_string` | **`_json_print_escape_string`** | 打印字符串非转义的部分 `inline`，转义的情况单独取出作为函数 |
| 解析字符串 | `_json_parse_string_quick_stage` | **`_json_parse_string_reuse`** / **`_json_parse_string_rapid`**（或 `_json_parse_string`） | 解析字符串非转义的部分 `inline`，转义的情况单独取出作为函数 |
| 解析值（分配对象内存） | `pjson_memory_alloc` | **`pjson_memory_alloc_block`** | 直接从当前内存块分配是大多数情况，再分配一个内存块是少数情况 |

**① 打印字符串：非转义 `inline`，转义单独成函数**

```c
/* 热版本：static inline，直接把"不含转义字符"这条快路径写在里面 */
static inline int _json_print_string(json_print_t *print_ptr, const char *val,
                                     json_strinfo_t *info, size_t addi_size)
{
    if (likely(!info->escaped)) {          /* ★ 热分支：无转义，一次 memcpy 搞定 */
        char *cur = NULL;
        size_t len = info->len;
        _PRINT_PTR_REALLOC(len + 3 + addi_size);
        cur = print_ptr->cur;
        *cur++ = '\"';
        memcpy(cur, val, len);
        cur += len;
        *cur++ = '\"';
        print_ptr->cur = cur;
        return 0;
    }

    /* 冷分支：转义处理（逐字符扫描、\uXXXX 转换），单独一个非 inline 函数 */
    return _json_print_escape_string(print_ptr, val, info, addi_size);
}

/* 冷版本：static（非 inline），不挤占热路径的指令缓存 */
static int _json_print_escape_string(json_print_t *print_ptr, const char *val,
                                     json_strinfo_t *info, size_t addi_size)
{
    ...                                    /* 8 路展开扫描 + 逐字符转义 */
}
```

绝大多数 JSON 字符串不含转义字符，所以 `likely(!info->escaped)` 几乎总是命中：整个打印退化成"加两个引号 + 一次 `memcpy`"，而转义处理那一大段代码**完全不进入热路径**。

**② 解析字符串：非转义 `inline`，转义交给完整函数**

```c
/* 热版本：INLINE_ATTR = __attribute__((always_inline))，只处理"扫到第一个特殊字符就正常闭合"的情况 */
INLINE_ATTR
static inline int _json_parse_string_quick_stage(json_parse_t *parse_ptr,
    char end_ch, char **ppstr, json_strinfo_t *pinfo, json_mem_mgr_t *mgr)
{
    ...
    switch (c) {
    case '\"':
        ...
        {
            *ppstr = ptr;                  /* ★ 设置结果指针 = 成功 */
            pinfo->escaped = 0;
            pinfo->len = len;
            _UPDATE_PARSE_OFFSET(len);
        }
        break;
    default:
        break;                             /* 遇到转义/其它：什么都不做 = 失败 */
    }
    return len;                            /* 返回已扫描长度，供冷版本续用 */
}

/* 冷版本：static（非 inline），处理转义、\uXXXX、文件分块读等完整情况 */
static int _json_parse_string_reuse(json_parse_t *parse_ptr, int poffset, ...)
```

调用点：

```c
    poffset = _json_parse_string_quick_stage(parse_ptr, end_ch, &jkey.str, &jkey.info, &mgr);
    if (unlikely(!jkey.str && parse_ptr->parse_string(parse_ptr, poffset, end_ch,
                 &jkey.str, &jkey.info, &mgr) < 0)) {
        goto err;
    }
```

> 关键点：`parse_ptr->parse_string` 单独存在时**功能完全正常** —— 把 `quick_stage` 整个去掉也照样能解析，只是慢。
> 也就是说 **quick_stage 是纯粹的性能优化层，正确性由那个功能完整的版本兜底**。

**③ 解析值时分配对象内存：从当前块分配 `inline`，新开一块单独成函数**

```c
/* 热版本：static inline，绝大多数分配都在当前块内完成 */
static inline void *pjson_memory_alloc(size_t size, json_mem_mgr_t *mgr)
{
    if (likely((mgr->cur_node->cur - mgr->cur_node->ptr) + size <= mgr->cur_node->size)) {
        void *p = mgr->cur_node->cur;      /* ★ 热分支：指针加一下就完事 */
        mgr->cur_node->cur += size;
        return p;
    }
    return pjson_memory_alloc_block(size, mgr);   /* 冷分支：当前块不够了 */
}

/* 冷版本：static（非 inline），malloc 一个新块并挂到链表上 */
static void *pjson_memory_alloc_block(size_t size, json_mem_mgr_t *mgr) { ... }
```

解析一个 JSON 要分配成千上万个对象，其中"当前块还有空间"是绝大多数情况；只有块用尽时才需要真正去 `malloc`。把后者拆成独立函数后，热路径就是一个 `inline` 的指针加法。

#### 7.5.3 两种方式怎么选

| 判断依据 | 选 ①（完整函数 inline，冷分支外挂） | 选 ②（热分支 inline，失败后回落） |
|----------|:---:|:---:|
| 热分支能否独立完成整个功能 | 能（只是结果不同） | 不能（可能"扫一半发现不对"） |
| 冷分支是否需要热分支的中间结果 | 不需要 | 需要（如 `poffset` 断点续扫） |
| 典型例子 | 打印字符串、内存块分配 | 解析字符串（quick_stage） |

一句话：**能在分支里一次做完的用 ①，可能需要"做不完再补"的用 ②。**

#### 7.5.4 延伸：另一种"分离"——不同实现的一次性分派

上面讲的是**同一实现内部的冷热分支分离**。LJSON 里还有另一类分离：**同一功能的两套实现**（一套为内存字符串优化、一套支持文件流分块读），防止"为了兼容慢场景而让通用实现变复杂，进而拖快慢场景"。

`json_parse_t` 里放着三个函数指针，在 `json_parse_common` 里**一次性选好**，之后每次调用都是直接跳转、零判断：

```c
typedef struct _json_parse_t {
    void (*skip_blank)(struct _json_parse_t *parse_ptr);
    int (*parse_string)(struct _json_parse_t *parse_ptr, int poffset,
        char end_ch, char **ppstr, json_strinfo_t *pinfo, json_mem_mgr_t *mgr);
    int (*parse_value)(struct _json_parse_t *parse_ptr, json_object **root);
} json_parse_t;
```

| 功能 | 通用版（支持文件流分块读） | 内存版（数据全在内存） |
|------|--------------------------|----------------------|
| 跳过空白 | `_skip_blank` | `_skip_blank_rapid`（`static inline`） |
| 解析字符串 | `_json_parse_string` | `_json_parse_string_rapid` / `_json_parse_string_reuse` |
| 解析值 | `_json_parse_value` | `_json_parse_value_rapid` |

内存版省掉了"数据够不够、要不要再读一块"的 `_get_parse_ptr` 判断，可以直接 `parse_ptr->str + offset` 取指针，还能安全 `inline`。

> 两类分离的区别，一句话说清：
> **7.5.1~7.5.3 是"同一实现里的冷热分支"分离 —— 让热分支小到能 inline；**
> **7.5.4 是"不同实现"分离 —— 避免慢场景（文件流）的兼容逻辑把快场景（全内存）拖慢。**

### 7.6 两阶段字符串解析：快速路径前置 + 完整路径兜底

这是"冷热分离"思想在**同一个函数内部**的延伸，也是 LJSON 字符串解析最关键的一招。

**思路：绝大多数 JSON 字符串不含转义字符。** 所以把"不含转义"的这种情况单独抽成一个 **`always_inline`** 的小函数，先跑它；只有当它没能完成（说明遇到了转义、特殊字符、文件边界等）时，才回落到功能完整的通用版本。

```c
INLINE_ATTR                                  /* __attribute__((always_inline)) */
static inline int _json_parse_string_quick_stage(json_parse_t *parse_ptr,
    char end_ch UNUSED_END_CH, char **ppstr, json_strinfo_t *pinfo, json_mem_mgr_t *mgr)
{
    char *str = NULL, *bak = NULL, *ptr= NULL;
    char c = '\0';

    *ppstr = NULL;
    memset(pinfo, 0, sizeof(*pinfo));

    str = parse_ptr->str + parse_ptr->offset;
    bak = str;

    /* ① 一路扫到第一个"特殊字符"为止（8 路展开） */
    while (1) {
        c = *str++; if (unlikely(_is_special_char((uint8_t)c))) break;
#if JSON_MANUAL_LOOP_UNFOLD
        c = *str++; if (unlikely(_is_special_char((uint8_t)c))) break;
        ... /* 共 8 次 */
#endif
    }

    int len = (int)(str - bak - 1);
    switch (c) {
    case '\"':
#if JSON_PARSE_SPECIAL_QUOTES
    case '\'':
    case ':':
        if (c == end_ch)
#endif
        {
            /* ② 只有确认是正常闭合，才真正产出结果 */
            if (parse_ptr->reuse_flag) {
                ptr = bak;                                   /* 复用模式：原地指向原始 buffer */
            } else {
                if (unlikely((ptr = (char *)_parse_alloc(parse_ptr, len + 1, mgr)) == NULL)) {
                    JsonErr("malloc failed!\n");
                    return 0;                                /* 失败：返回 0，交给慢路径 */
                }
                memcpy(ptr, bak, len);
            }
            ptr[len] = '\0';
            *ppstr = ptr;                                    /* ★ 设置结果指针 */
            pinfo->escaped = 0;
            pinfo->len = len;
            _UPDATE_PARSE_OFFSET(len);
        }
        break;
    default:
        break;                                               /* 遇到转义/其它：什么都不做 */
    }

    return len;                                              /* 返回已扫描的长度（供慢路径续用） */
}
```

**调用点长这样**（解析 key、解析字符串值，共三处）：

```c
    end_ch = *str;
    _UPDATE_PARSE_OFFSET(1);
    poffset = _json_parse_string_quick_stage(parse_ptr, end_ch,
                  &jkey.str, &jkey.info, &parse_ptr->mem->key_mgr);
    if (unlikely(!jkey.str && parse_ptr->parse_string(parse_ptr, poffset,
                  end_ch, &jkey.str, &jkey.info, &parse_ptr->mem->key_mgr) < 0)) {
        goto err;
    }
    _UPDATE_PARSE_OFFSET(1);
```

**读懂这五行，就读懂了这个设计**

1. `quick_stage` **先跑**，返回扫描过的长度 `poffset`。
2. 判断 `!jkey.str` —— 如果 `quick_stage` 没能设置结果指针（说明它没走到正常闭合分支，即遇到了转义、特殊字符、或其它复杂情况），才调用 `parse_ptr->parse_string`。
3. `unlikely(...)` —— 明确告诉编译器"回落是罕见的"，把慢路径的代码挪到 fall-through 之外。
4. `poffset` 传给慢路径 —— **快速路径已经扫过的部分不白扫**，慢路径从断点继续。

**为什么这样更快？**

| 收益 | 说明 |
|------|------|
| **快速路径极致小** | `quick_stage` 只有"扫描 + 一次分配 + 一次 memcpy"，被 `always_inline` 内联后几乎没有函数边界 |
| **慢路径不污染 i-cache** | 转义处理、`\uXXXX` 转 UTF-8（含 `utf16_literal_to_utf8`）、文件分块读等复杂代码都在**独立的函数**里，不占热路径的指令缓存 |
| **分支预测友好** | `unlikely` 让 CPU 几乎总是正确预测走快速路径 |
| **零冗余判断** | 快速路径里根本没有"处理转义"的分支，连判断都不做 |

**`parse_ptr->parse_string` 单独存在，功能本身完全正常** —— 这一点很重要：

* 它是一个**函数指针**，在 `json_parse_common` 里按场景绑定到 `_json_parse_string`（文件流）/`_json_parse_string_rapid`（内存池）/`_json_parse_string_reuse`（原地复用）三种实现之一。
* 就算把 `quick_stage` 整个去掉，只调用 `parse_ptr->parse_string`，解析功能也**完全正确**，只是慢一些。
* SAX 解析路径上，它被绑定为 `_json_sax_parse_string`。

也就是说：**`quick_stage` 是纯粹的性能优化层，不是功能层。** 这种"快速路径前置 + 完整路径兜底"的结构，保证了性能优化永远不会牺牲正确性 —— 因为正确性由那个功能完整的版本兜底。

### 7.7 打印热路径：局部变量 `cur` + `_XXX_SYNC` 宏 + 多分配 64 字节

这是打印侧最关键的三招，它们配合得非常紧密。先看 `_json_print_value_format` 的骨架：

```c
static int _json_print_value_format(json_print_t *print_ptr, json_object *json)
{
    json_object *stack_array[JSON_ITEM_NUM_PLUS_DEF];
    json_object **item_array = stack_array;
    json_object *parent = NULL;
    struct json_list *first = NULL;
    const char *brackets[2] = {"[]", "{}"};
    char rbracket = '\0';
    char *cur = print_ptr->cur;                    /* ★① 局部副本 */
    int item_count = print_ptr->item_count;        /* ★① 局部副本 */
    int item_depth = -1, item_total = JSON_ITEM_NUM_PLUS_DEF;

    _PRINT_PTR_REALLOC_SYNC(0);
    goto next2;

next1:
    if (parent->ikey.type == JSON_OBJECT) {
        _PRINT_ADDI_FORMAT_SYNC(print_ptr, item_depth + 1);   /* ★② 需要同步的调用 */
        if (json->ikey.len) {
            _JSON_PRINT_STRING_SYNC(print_ptr, json->key, &json->ikey);
            memcpy(cur, ":\t", 2); cur += 2;                  /* ★③ 无需同步，直接写 cur */
        } else {
            ...
        }
    }
    ...

next2:
    switch (json->ikey.type) {
    case JSON_NULL:
        memcpy(cur, "null", 4); cur += 4;                     /* ★③ */
        break;
    case JSON_BOOL:
        if (json->value.vnum.vbool) {
            memcpy(cur, "true", 4); cur += 4;
        } else {
            memcpy(cur, "false", 5); cur += 5;
        }
        ...
    }
    ...
    print_ptr->cur = cur;   // store the pointer              /* ★④ 收尾写回 */
```

**① 为什么要把 `print_ptr->cur` 拷贝成局部变量 `cur`？**

`print_ptr->cur` 是**结构体成员，位于内存**；而 `cur` 是**局部变量，编译器几乎总是把它放在寄存器里**。

打印过程中的典型操作是 `memcpy(cur, "null", 4); cur += 4;`：

* 用 `print_ptr->cur`：每次写都是"读内存 → 写内存 → 写回成员"，而且编译器无法确信 `print_ptr->cur` 不会被别的指针改写（别名分析），可能反复重新加载。
* 用 `cur`：全程在寄存器里推进，只在必要时刻同步一次。

同理 `item_count` 也被拷贝成局部变量 —— 它每打印一个对象就 `++`，是第二热的数据。

**② `_XXX_SYNC` 宏：只在"可能 realloc"的调用前后同步**

`cur` 是副本，那么调用那些内部会改 `print_ptr->cur` 的函数时，就必须先写回、再取回。这就是四个 `SYNC` 宏的作用：

```c
#define _PRINT_ALLOC_ADDI_SIZE      64

#define _PRINT_PTR_REALLOC_SYNC(nz)             do {                              \
    print_ptr->cur = cur;                                                         \
    print_ptr->item_count = item_count;                                           \
    if (unlikely(GET_BUF_FREE_SIZE(print_ptr) < (nz + _PRINT_ALLOC_ADDI_SIZE)     \
        && print_ptr->realloc(print_ptr, nz + _PRINT_ALLOC_ADDI_SIZE) < 0))       \
        goto err;                                                                 \
    cur = print_ptr->cur;                                                         \
    item_count = print_ptr->item_count;                                           \
} while(0)
```

四个 `SYNC` 宏（`_PRINT_PTR_REALLOC_SYNC` / `_PRINT_ADDI_FORMAT_SYNC` / `_JSON_PRINT_STRING_SYNC` / `_JSON_PRINT_BINARY_SYNC`）结构完全一致：

```
写回 print_ptr->cur（和 item_count）
  → 调用可能 realloc 的函数
  → 取回 print_ptr->cur（和 item_count）
```

`realloc` 之后缓冲区可能整个搬了家，所以**必须**重新取回指针 —— 这正是同步的意义。

**③ 不需要同步的写入：直接操作 `cur`**

像 `memcpy(cur, "null", 4); cur += 4;`、`*cur++ = ' '`、`memcpy(cur, ":\t", 2);` 这类**定长小写入**，因为它们的前提是"缓冲区还有空间"（由前面的 `SYNC` 保证），所以可以完全绕开 `print_ptr`，只在寄存器里推进。

**④ `_PRINT_ALLOC_ADDI_SIZE` = 64：多分配 64 字节，换多次免检查**

这是本节最精华的一处。注意 `SYNC` 宏的判断条件不是"剩余空间 < nz"，而是：

```c
GET_BUF_FREE_SIZE(print_ptr) < (nz + _PRINT_ALLOC_ADDI_SIZE)     /* nz + 64 */
```

**含义：只要剩余空间不足 64 字节，就触发一次扩容；而扩容时会多要 64 字节。**

于是：

* **一次扩容之后，缓冲区至少还有 64 字节的余量**；
* 接下来的若干次小写入（`"null"` 4 字节、`"true"` 4 字节、`":\t"` 2 字节、缩进、逗号……）都**命中"剩余 ≥ 64"这个条件，直接跳过检查与 realloc**；
* 也就是说：**把"每写一次检查一次"变成了"每 64 字节左右才检查一次"。**

这是一次典型的**空间换时间**：

| 代价 | 收益 |
|------|------|
| 每次扩容多要 **64 字节**（可能末尾有些用不上） | 后续多次写入**免检查、免 realloc 调用** |
| 内存浪费 ≤ 64 字节 | 省掉数十次 `GET_BUF_FREE_SIZE` 比较与分支 |

64 这个数字也选得很讲究：**足够大**（能覆盖绝大多数连续小写入），又**足够小**（远小于 L1 cache line 的浪费容忍度，且不会让 buffer 尾部浪费明显）。

**三招合起来的效果**

```
局部 cur        → 写入全程在寄存器，省掉 N 次内存读改写
SYNC 宏         → 只在真正可能 realloc 的 4 个调用点同步，其余零开销
多分配 64 字节  → 把"每次检查"降为"每 ~64 字节检查一次"
```

这就是 LJSON 打印能跑出 **2.6~5.2 GB/s**（见 [6.4 复用模式：重复解析 / 重复打印吞吐](#64-复用模式重复解析--重复打印吞吐)）的直接原因之一。

### 7.8 数据结构：32 字节的 json_object

```c
typedef struct {
    struct json_list list;   /*  8 字节：链表节点 */
    char *key;               /*  8 字节：键（只有 JSON_OBJECT 的子对象才有） */
    json_strinfo_t ikey;     /*  4 字节：键的信息（含类型！） */
    json_strinfo_t istr;     /*  4 字节：字符串/二进制值的信息 */
    json_value_t value;      /*  8 字节：值（union） */
} json_object;               /* 合计 32 字节（64 位系统） */
```

**为什么是 32 字节？**（v5.0.0 从 40 字节压缩而来，解析速度因此提升 5%~10%）

* **32 是 2 的幂，且刚好是半个 cache line**：两个对象 64 字节（1 条 cache line），四个对象 128 字节，与 cache line 和常见预取粒度对齐。
* **对象体积直接决定 cache 能装下多少节点**：32 字节意味着同样的 cache 容量能多放 25% 的节点，遍历链表时的 cache miss 显著减少。

**位域的巧思：类型藏在哪？**

```c
typedef struct {
    uint32_t type:4;      /* json 对象类型 —— 类型不占独立字段！ */
    uint32_t escaped:1;   /* 是否含需要转义的字符 */
    uint32_t alloced:1;   /* 是否在堆中分配（SAX 接口用） */
    uint32_t len:26;      /* 字符串长度，最大 67108863 */
} json_strinfo_t;         /* 只占 4 字节 */
```

最精妙的一点：**对象的类型没有单独占一个字段，而是塞进了 `ikey` 的 4 位位域**。`ikey` 本来就要存键的信息，把 `type` 借位进去，就省掉了整个枚举字段的空间。这是从 40 字节压到 32 字节的关键之一。

相应的取值方式（宏封装，读起来仍然自然）：

```c
#define json_get_object_type(json)   ((json)->ikey.type)
```

**union 省值空间**

```c
typedef union {
    json_number_t vnum;    /* bool / int32 / uint32 / int64 / uint64 / double */
    char *vstr;            /* 字符串 */
    void *vbin;            /* 二进制 */
    struct json_list head; /* 数组/对象的子链表头 */
} json_value_t;            /* 都是 8 字节 */
```

所有值类型共用一个 8 字节 union —— 一个数组节点和一个 double 节点**一样大**。

### 7.9 链表设计：O(1) 追加的单向循环链表

```c
struct json_list {
    struct json_list *next;      /* 只有一个指针！ */
};
```

LJSON 用**单向**链表管理子对象（而不是 cJSON 那种双向链表），每个节点只占 8 字节。那怎么做到 O(1) 追加？

```c
static inline void json_list_add_tail(struct json_list *list, struct json_list *head)
{
    if (unlikely(json_list_empty(head))) {
        list->next = list;              /* 空表：自己指向自己 */
        head->next = list;
    } else {
        list->next = head->next->next;  /* 新节点指向当前的 first */
        head->next->next = list;        /* 原 first 的前驱（即 tail）改指新节点 */
        head->next = list;              /* head 指向新的 tail */
    }
}
```

**约定：`head->next` 指向尾节点，`tail->next` 指向头节点。**

| 操作 | 复杂度 | 说明 |
|------|:------:|------|
| 追加到尾部 | **O(1)** | 解析时 99% 的操作，这是热路径 |
| 取第一个元素 | **O(1)** | `head->next->next` |
| 取最后一个元素 | **O(1)** | `head->next` |
| 删除 | O(N)（需找前驱） | 冷路径，可接受 |

**取舍很清晰**：解析是"不断往后追加"的过程，所以把**追加**做成 O(1)，代价是删除需要遍历找前驱 —— 而删除在 JSON 处理中是低频操作。这也是"冷热分离"思想在数据结构上的体现：**让高频操作最快，把成本转嫁给低频操作。**

### 7.10 内存池：bump 分配器，两次比较搞定一次分配

```c
static inline void *pjson_memory_alloc(size_t size, json_mem_mgr_t *mgr)
{
    if (likely((mgr->cur_node->cur - mgr->cur_node->ptr) + size <= mgr->cur_node->size)) {
        void *p = mgr->cur_node->cur;
        mgr->cur_node->cur += size;
        return p;
    }
    return pjson_memory_alloc_block(size, mgr);   /* 慢路径：再申请一个大块 */
}
```

这就是经典的 **bump allocator（指针碰撞分配器）**：

* **快路径**：一次减法、一次比较、一次加法、一次赋值 —— **没有函数调用、没有空闲链表、没有边界标记**。
* **慢路径**只在当前块用尽时触发，一次申请 `mem_size`（默认 `JSON_POOL_MEM_SIZE_DEF` = **8192**，可由 `obj_mgr.mem_size` 指定）的大块。
* `likely()` 告诉编译器"绝大多数情况下走快路径"，让快路径排在 fall-through 上。

对比 `malloc`：后者要维护空闲链表 / bin、处理合并分割、可能触发系统调用 —— **每次调用几十到几百纳秒**，而 bump 分配是**一两条指令**。

**这也是一处"空间换时间"**：一次性多要 8192 字节（其中一部分暂时用不上），换来成百上千次分配免于进入 `malloc`。

**代价（必须知道）**：内存块只能**统一申请、统一释放**，无法单独释放某个对象。所以它按用途分成三个独立的管理器：

```c
typedef struct {
    bool valid;
    json_mem_mgr_t obj_mgr;    /* 管理 json_object */
    json_mem_mgr_t key_mgr;    /* 管理键值 */
    json_mem_mgr_t str_mgr;    /* 管理 JSON_STRING 的值 */
} json_mem_t;
```

> 分成三个而非一个，是因为 `json_object` 可能有**内存地址对齐要求**，把它和变长的字符串混在一个块里会造成对齐浪费。

内存池的大小在解析开始时按文件大小估算（又一次"预分配"）：

```c
    if (choice->mem && !choice->mem->valid) {
        pjson_memory_init(choice->mem);
        mem_size = total_size / JSON_PARSE_NUM_DIV_DEF;      /* 默认按 1/8 估算 */
        if (mem_size < choice->mem_size)
            mem_size = choice->mem_size;
        choice->mem->obj_mgr.mem_size = mem_size;
        choice->mem->key_mgr.mem_size = mem_size;
        choice->mem->str_mgr.mem_size = mem_size;
    }
```

而 `pjson_memory_refresh` 让内存池可以循环使用：

```c
static void _json_mem_refresh(json_mem_mgr_t *mgr)
{
    struct json_list *head = &mgr->head;
    if (!json_list_empty(head)) {
        /* 释放除第一个块之外的所有块 */
        ...
        first->cur = first->ptr;     /* 写指针回到块首 —— 下一轮从零开始复用 */
        mgr->cur_node = first;
    }
}
```

**只保留第一个块、写指针归零** —— 这就是零堆分配模式循环复用的全部秘密。

### 7.11 零拷贝：原地复用原始字符串

复用模式（模式3）的字符串解析 `_json_parse_string_reuse` 的核心：

```c
    end = parse_ptr->str + parse_ptr->size;
    str = parse_ptr->str + parse_ptr->offset;
    bak = str;
    ptr = str;
    *ppstr = str;              /* ★ 直接指向原始 buffer，不分配、不拷贝 */
    str += poffset;

loop1:
    while (1) {
        c = *str++; if (unlikely(_is_special_char((uint8_t)c))) goto next1;
        /* 手工展开 8 次 */
        ...
    }

next1:
    switch (c) {
    case '\"':
        {
            len = (int)(str - bak - 1);
            _UPDATE_PARSE_OFFSET(len);
            ptr[len] = '\0';                  /* ★ 就地截断，原地写入结束符 */
            pinfo->escaped = escaped != 0;
            pinfo->len = len;
            return len;
        }
```

而 `quick_stage` 里对应的分支更简洁（`parse_ptr->reuse_flag` 为真时）：

```c
            if (parse_ptr->reuse_flag) {
                ptr = bak;                    /* 零拷贝：直接指向原始位置 */
            } else {
                ptr = _parse_alloc(parse_ptr, len + 1, mgr);   /* 分配 + 拷贝 */
                memcpy(ptr, bak, len);
            }
            ptr[len] = '\0';
```

两个 ★ 是关键：

1. `*ppstr = str` / `ptr = bak` —— 解析出的字符串**直接指向输入 buffer**，没有 `malloc`、没有 `memcpy`。
2. `ptr[len] = '\0'` —— 就地写入结束符，把原始 JSON 文本"改造"成可用的 C 字符串。

**省掉了什么**：对每个字符串值，省掉一次 `malloc` + 一次 `memcpy`。twitter.json 里字符串值占比很高，这就是模式 2 → 模式 3 还能再快 29% 的原因。

**代价（必须知道）**：原始 buffer 会被就地修改，且在 json 使用期间**不能释放**。这也是为什么它叫"复用"而不是"拷贝"。

### 7.12 信息记录：把 strlen / 转义扫描的结果缓存下来

LJSON 在解析时就**顺手**把这些元信息记录到对象里：

| 记录项 | 存在哪 | 省掉了什么 |
|--------|--------|-----------|
| 字符串长度 | `info.len` | 打印时不必再调 `strlen` |
| 是否含转义字符 | `info.escaped` | 打印时不必再扫描一遍判断要不要转义 |
| 键的哈希 | `json_item_t.hash` | 查找时不必每次重算哈希 |
| 对象类型 | `ikey.type` | 打印时不必反复判断分支 |

**效果最直观的是打印路径**：

```c
static inline int _json_print_string(json_print_t *print_ptr, const char *val,
                                     json_strinfo_t *info, size_t addi_size)
{
    if (likely(!info->escaped)) {              /* ★ 解析时就知道：这个串没有转义字符 */
        char *cur = NULL;
        size_t len = info->len;                /* ★ 解析时就知道：不用 strlen */
        _PRINT_PTR_REALLOC(len + 3 + addi_size);
        cur = print_ptr->cur;
        *cur++ = '\"';
        memcpy(cur, val, len);                 /* 一次 memcpy 搞定 */
        cur += len;
        *cur++ = '\"';
        print_ptr->cur = cur;
        return 0;
    }

    return _json_print_escape_string(print_ptr, val, info, addi_size);  /* 慢路径 */
}
```

**绝大多数 JSON 字符串不含转义字符**，所以这个 `likely` 分支几乎总是命中：整个打印退化成"加两个引号 + 一次 memcpy"。而如果一个库不记录 `escaped`，它就必须逐字符扫描一遍 —— 打印一个 100 字节的字符串就要多跑 100 次判断。

**哈希也只在建 items 时算一次**：

```c
static inline uint32_t _string_hash_code(const char *str, int len)
{
    int i = 0;
    uint32_t hash = 0;
    if (!len)
        return 0;
    for (i = 0; i < len; ++i)
        hash = (hash << 5) - hash + (uint8_t)str[i];   /* hash * 31 + c */
    return hash;
}
```

> 这一节的本质也是"空间换时间"：用 `json_strinfo_t` 的 4 个字节，换掉运行时的重复扫描。

### 7.13 访问加速：json_items 与哈希二分查找

`json_get_items` 把 `JSON_ARRAY` / `JSON_OBJECT` 的所有子对象抽成一个**连续数组** `json_item_t[]`：

```c
typedef struct {
    uint32_t hash;
    json_object *json;
} json_item_t;

typedef struct {
    uint32_t conflicted:1;    /* 是否有哈希冲突 */
    uint32_t reserved:31;
    uint32_t total;           /* 数组容量 */
    uint32_t count;           /* 当前数量 */
    json_item_t *items;
} json_items_t;
```

对 `JSON_OBJECT`，会按哈希排序一次，之后查找就是**二分**：

```c
static inline void json_object_items_sort(json_items_t *items)
{
    items->conflicted = 0;
    if (items->count > 1) {
        uint32_t i = 0;
        qsort(items->items, items->count, sizeof(json_item_t), _json_hash_cmp);
        for (i = 1; i < items->count; ++i) {
            if (items->items[i-1].hash == items->items[i].hash) {
                items->conflicted = 1;      /* 有冲突：查找时退化成线性比对 key */
                break;
            }
        }
    }
}

json_object *json_search_object_item(json_items_t *items, json_string_t *jkey, uint32_t hash)
{
    int left = 0, right = 0, middle = 0, i = 0, count = 0;
    ...
    right = count - 1;
    while (left <= right) {
        middle = (left + right) >> 1;
        if (hash == items->items[middle].hash) {
            if (!items->conflicted)
                return items->items[middle].json;      /* ★ 无冲突：直接命中 */
            /* 有冲突：在相邻同 hash 区间内线性比对 key 内容 */
            ...
        }
        if (hash > items->items[middle].hash)
            left = middle + 1;
        else
            right = middle - 1;
    }
    return NULL;
}
```

**复杂度对比**

| 访问方式 | 数组 | 对象 |
|----------|------|------|
| 链表逐个遍历（默认 `json_get_object_item`） | O(N) | O(N) |
| `json_get_items` + `items[i]` | **O(1)** | — |
| `json_get_items` + `json_search_object_item` | — | **O(logN)**（无哈希冲突时） |

`conflicted` 标记的巧思：**排序时顺手检测是否存在哈希冲突**。无冲突时查找直接返回 middle 的元素（O(logN)）；有冲突时才退化到线性比对 key 内容。绝大多数真实 JSON 的 key 哈希不冲突，所以走的是快路径。

同时这也是一次**索引（Indexing）**式优化：连续数组比链表遍历的 cache 友好度高得多。

### 7.14 空间换时间：查表法

查表法的本质是：**输入域有限、计算代价高的操作，把结果预先算好，运行时用一次内存访问替代一串计算。**

LJSON 在数值引擎与字符串处理里大量使用：

**① 两位数转字符串表（一次查表出 2 个字符）**

```c
static const char ch_100_lut[200] = {
    '0','0','0','1','0','2', ... '9','8','9','9',
};
```

`ch_100_lut[n<<1]` 直接给出 `00`~`99` 的两个字符。写数字时**一次查表出两位**，比"除 10 取余 + 加 '0'"快得多。

**② 尾零计数表（避免循环统计末尾有多少个 0）**

```c
static const uint8_t tz_100_lut[100] = {
    2, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 00 -> 2 个尾零 */
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0,     /* 10 -> 1 个尾零 */
    ...
};
```

生成最短表示时需要知道尾部的零有多少个 —— 查表一次得到，不用循环。

**③ 十六进制字符表**

```c
static const char hex_char_lut[] = {
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'
};
```

**④ 转义字符表（256 项，一次查表完成"是否转义 + 转义成什么"）**

```c
static inline char _is_escape_char(uint8_t val)
{
    static const char char_escape_lut[256] = {
        /* '"' -> '"', '\\' -> '\\', '\b' -> 'b', '\t' -> 't', '\n' -> 'n' ... */
        1  , 1  , 1  , 1  , 98 , 116, 110, 118, 102, 114, 1  , 1  , ...
    };
    return char_escape_lut[val];
}
```

这张表的返回值**一箭双雕**：

* 返回 `0` → 不需要转义
* 返回非 0 → 需要转义，且**返回值就是转义后的那个字符**（`98`='b'、`116`='t'、`110`='n'、`118`='v'、`102`='f'、`114`='r'）

于是一次查表同时完成了"判断"和"查映射"两件事，且**完全没有分支**。

> 这张表甚至把生成它的代码写在源码注释里，方便 `JSON_PRINT_UTF16_SUPPORT` 开关切换时重新生成。

**⑤ 为什么"小而精"很重要**

查找表的收益取决于它能不能待在 cache 里：

| 表大小 | 效果 |
|--------|------|
| ≤ L1（32~64 KB） | 极快，一次访问 1~4 周期 |
| > L3（数十 MB） | **可能比直接计算更慢** |

LJSON 的这些表加起来只有几百字节到几 KB，全部能装进 L1。这也是 `USING_SMALL_LUT` 开关存在的意义：**指令/数据 cache 紧张的场景，可以主动换用小表**（代码段小约 5 KB）。

### 7.15 快速整除：把除法换成乘法

一般 CPU 的整数除法速度远远慢于整数乘法（x86 上 `div` 指令可能要 20~40 个周期，而乘法只要 3~5 个）。那么能否把 `N/D` 换成乘法形式 `((type)N * M) >> K` 呢？

**① 源码里的两个宏**

```c
#define FAST_DIV100(n)      (((n) * 5243) >> 19)                            /* 0 <= n < 10000 */
#define FAST_DIV10000(n)    ((uint32_t)(((uint64_t)(n) * 109951163) >> 40)) /* 0 <= n < 100000000 */
```

`n / 100` 变成 `n * 5243 >> 19`，`n / 10000` 变成 `n * 109951163 >> 40`。**一次乘法 + 一次移位**替代一次除法。

**② 实际应用：整数转字符串**

```c
int jnum_itoa(int32_t num, char *buffer)
{
    ...
    if (n < 100000000) {
        s += fill_1_8_digits(s, n, &tz);
    } else {
        uint32_t q = (uint32_t)(((uint64_t)n * 1441151881) >> 57);  /* ★ n / 100000000，无除法 */
        uint32_t r = n - q * 100000000;
        ...
    }
}
```

**一次 64 位乘法 + 一次移位**完成 `/ 100000000`。

---

下面是完整的推导、求解脚本与常用除法表。

#### 7.15.1 实现原理

**情况一：D 是 2 的整数次方**

```sh
pow(2,K) = D

K = log2(D)
M = 1
```

即 `N/D` 可以直接写成 `N >> log2(D)` 的形式。

**情况二：D 不是 2 的整数次方**

```sh
N/D = (N / pow(2,K)) * (pow(2,K) / D)
    = (N * (pow(2,K) / D)) / pow(2,K)
```

`pow(2,K) / D` 不一定能整除，我们需要加上一个 delta 值使之整除，delta 值一定小于 D 且大于等于 0。

> 反证：如果 delta 小于 0，当 N 刚好被 D 整除时，此时会减去 delta 带来的附加值，整除值就比真正的值小 1，所以 delta 不能小于 0。

```sh
N/D = (N / pow(2,K)) * ((pow(2,K) + delta) / D)
    = (N / D) * ((pow(2,K) + delta) / pow(2,K))

N/D = (N * M) >> K    ( M = (pow(2,K) + delta) / D )
N/D = (N / D) + ((N * delta) / (D * pow(2,K)))
```

`N / D` 的小数部分最大为 `(D - 1) / D`，要保证上述式子成立，则小数部分必须小于 1：

```sh
((D - 1) / D) + ((N * delta) / (D * pow(2,K))) < 1
((N * delta) / (D * pow(2,K))) < (1 / D)
(N * delta) / pow(2,K) < 1
log2(N) + log2(delta) - K < 0
K > log2(N) + log2(delta)
```

综上，得到如下限制条件：

* `K > log2(N)`
* `K <= log2(N) + log2(D - 1) + 1`  ：delta 的最大值为 `D - 1`
* `delta = D - (pow(2,K) % D)`
* `M = (pow(2,K) + delta) / D`
* `N * M < pow(2,bits)`              ：bits 为计算机最大能表示的整数（64 或 128）

**已知 N 的范围和 D 的值，K 的取值范围就可以算出来；从一个确定的 K 和 D 就可以得到 M。** 符合上述限制条件时，`N/D` 就可以换成乘法形式 `((type)N * M) >> K`。

这正是"**有限定义域**"的含义：只有在 N 的取值域已知的前提下，才能把除法安全地换成乘法。

**情况三：D 存在 2 的 S 次方的公因子**

上面的限制条件中 `N * M` 可能超出计算机最大能表示的整数。如果 D 存在 2 的 S 次方的公因子，`N * M` 就可以缩小 —— 即 `((type)N * M) >> K` 可写成 `((type)(N >> S) * M) >> K` 的形式，限制条件相应变为：

```sh
NS = N >> S
DS = D >> S
K > log2(NS)
K <= log2(NS) + log2(DS - 1) + 1
delta = DS - (pow(2,K) % DS)
M = (pow(2,K) + delta) / DS
NS * M < pow(2,bits)
```

如果 `((type)(N >> S) * M)` 还是超出计算机最大能表示的整数，此时只能分解 N 和 M 再相乘了。

#### 7.15.2 python 实现

```python
import math, sys

max16 = 1<<16
max32 = 1<<32
max64 = 1<<64
max128 = 1<<128
en128 = False

def fast_div(D, maxN, S):
    DS = D >> S
    maxNS = maxN >> S

    logn = math.log(maxNS, 2)
    mink = int(logn) + 1
    maxk = int(logn + math.log(DS - 1, 2)) + 1

    for K in range(mink, maxk + 1):
        delta = DS - (1<<K) % DS
        if delta:
            deltak = int(logn + math.log(delta, 2)) + 1
        else:
            deltak = mink
        if K < deltak:
            continue

        M = ((1<<K) + delta) // DS
        maxV = maxNS * M
        bits = 0
        if maxV >= max128:
            print('%d\t%d\t%d\t%d\t%d\tinf\t: maxinum value (%d) exceeds uint128_t' % (D, maxN, S, M, K, maxV))
            return False
        elif maxV >= max64:
            bits = 128
            if not en128:
                print('%d\t%d\t%d\t%d\t%d\tu%d\t: maxinum value (%d) exceeds uint64_t' % (D, maxN, S, M, K, bits, maxV))
                return False
        elif maxV >= max32:
            bits = 64
        elif maxV >= max16:
            bits = 32
        else:
            bits = 16

        print('%d\t%d\t%d\t%d\t%d\tu%d' % (D, maxN, S, M, K, bits))
        return True
    return False

def get_fast_div(D, maxN):
    logd = math.log(D, 2)
    if logd == int(logd):
        print('%d\tany\t0\t1\t%d\tany' % (D, int(logd)))
        return True

    if fast_div(D, maxN, 0):
        return True

    S = 0
    while D % (2 ** (S + 1)) == 0:
        S += 1
    if not S:
        return False
    return fast_div(D, maxN, S)

def print_div_table():
    for i in range(1, 20):
        for j in range(i+1, 21):
            n = 10 ** j - 1
            d = 10 ** i
            if j == 10:
                get_fast_div(d, max32 - 1)
            if j == 20:
                get_fast_div(d, max64 - 1)
            get_fast_div(d, n)

print('D\tmaxN\tS\tM\tK\ttype')
if len(sys.argv) != 3:
    print_div_table()
else:
    get_fast_div(int(sys.argv[1]), int(sys.argv[2]))
print('#define FAST_DIV(N) ((((type)(N) >> S) * M) >> K)')
```

运行 `python3 fastdiv.py` 或 `python3 fastdiv.py <D> <maxN>`

#### 7.15.3 常用快速除法表

**10 的幂次（maxN = 4294967295）**

| D | maxN | S | M | K | type |
|---|------|---|---|---|------|
| 10 | 4294967295 | 0 | 3435973837 | 35 | u64 |
| 100 | 4294967295 | 0 | 1374389535 | 37 | u64 |
| 1000 | 4294967295 | 0 | 274877907 | 38 | u64 |
| 10000 | 4294967295 | 0 | 3518437209 | 45 | u64 |
| 100000 | 4294967295 | 5 | 175921861 | 39 | u64 |
| 1000000 | 4294967295 | 0 | 1125899907 | 50 | u64 |
| 10000000 | 4294967295 | 0 | 1801439851 | 54 | u64 |
| 100000000 | 4294967295 | 0 | 1441151881 | 57 | u64 |
| 1000000000 | 4294967295 | 9 | 281475 | 39 | u64 |
| 1000000000 | 18446744073709551615 | 9 | 19342813113834067 | 75 | u128 |
| 10000000000 | 18446744073709551615 | 0 | 15845632502852867519 | 97 | u128 |
| 100000000000 | 18446744073709551615 | 0 | 12676506002282294015 | 100 | u128 |
| 1000000000000 | 18446744073709551615 | 0 | 2535301200456458803 | 101 | u128 |
| 10000000000000 | 18446744073709551615 | 13 | 1980704062856609 | 81 | u128 |
| 100000000000000 | 18446744073709551615 | 0 | 811296384146066817 | 106 | u128 |
| 1000000000000000 | 18446744073709551615 | 15 | 633825300114115 | 84 | u128 |
| 10000000000000000 | 18446744073709551615 | 0 | 4153837486827862103 | 115 | u128 |
| 100000000000000000 | 18446744073709551615 | 17 | 101412048018259 | 86 | u128 |
| 1000000000000000000 | 18446744073709551615 | 18 | 81129638414607 | 88 | u128 |

**10 的幂次（maxN = 10 的幂次 − 1，即"位数刚好"的场景）**

| D | maxN | S | M | K | type |
|---|------|---|---|---|------|
| 100 | 9999 | 0 | 5243 | 19 | u32 |
| 1000 | 999999 | 0 | 536871 | 29 | u64 |
| 10000 | 99999999 | 0 | 109951163 | 40 | u64 |
| 100000 | 9999999999 | 5 | 175921861 | 39 | u64 |
| 1000000 | 999999999999 | 0 | 1152921504607 | 60 | u128 |
| 10000000 | 99999999999999 | 0 | 59029581035871 | 69 | u128 |
| 100000000 | 9999999999999999 | 0 | 3022314549036573 | 78 | u128 |
| 1000000000 | 999999999999999999 | 0 | 309485009821345069 | 88 | u128 |
| 10000000000 | 18446744073709551615 | 0 | 15845632502852867519 | 97 | u128 |

**除数 10，按 maxN 逐级变化（可直接看出 K 与定义域的关系）**

| D | maxN | S | M | K | type |
|---|------|---|---|---|------|
| 10 | 99 | 0 | 103 | 10 | u16 |
| 10 | 999 | 0 | 205 | 11 | u32 |
| 10 | 9999 | 0 | 3277 | 15 | u32 |
| 10 | 99999 | 0 | 52429 | 19 | u64 |
| 10 | 999999 | 0 | 838861 | 23 | u64 |
| 10 | 9999999 | 0 | 6710887 | 26 | u64 |
| 10 | 99999999 | 0 | 107374183 | 30 | u64 |
| 10 | 999999999 | 0 | 214748365 | 31 | u64 |
| 10 | 4294967295 | 0 | 3435973837 | 35 | u64 |
| 10 | 9999999999 | 1 | 3435973837 | 34 | u64 |
| 10 | 99999999999 | 0 | 54975581389 | 39 | u128 |
| 10 | 999999999999 | 0 | 879609302221 | 43 | u128 |
| 10 | 9999999999999 | 0 | 7036874417767 | 46 | u128 |
| 10 | 99999999999999 | 0 | 112589990684263 | 50 | u128 |
| 10 | 999999999999999 | 0 | 225179981368525 | 51 | u128 |
| 10 | 9999999999999999 | 0 | 3602879701896397 | 55 | u128 |
| 10 | 99999999999999999 | 0 | 57646075230342349 | 59 | u128 |
| 10 | 999999999999999999 | 0 | 922337203685477581 | 63 | u128 |
| 10 | 9999999999999999999 | 0 | 7378697629483820647 | 66 | u128 |
| 10 | 18446744073709551615 | 0 | 14757395258967641293 | 67 | u128 |

**常用的小整数除数（maxN = 4294967295）**

| D | maxN | S | M | K | type |
|---|------|---|---|---|------|
| 3 | 4294967295 | 0 | 2863311531 | 33 | u64 |
| 5 | 4294967295 | 0 | 3435973837 | 34 | u64 |
| 7 | 4294967295 | 0 | 4908534053 | 35 | u128 |
| 9 | 4294967295 | 0 | 954437177 | 33 | u64 |
| 11 | 4294967295 | 0 | 3123612579 | 35 | u64 |
| 13 | 4294967295 | 0 | 1321528399 | 34 | u64 |
| 15 | 4294967295 | 0 | 2290649225 | 35 | u64 |
| 17 | 4294967295 | 0 | 4042322161 | 36 | u64 |
| 19 | 4294967295 | 0 | 7233629131 | 37 | u128 |
| 21 | 4294967295 | 0 | 6544712071 | 37 | u128 |
| 23 | 4294967295 | 0 | 2987803337 | 36 | u64 |
| 31 | 4294967295 | 0 | 4433514629 | 37 | u128 |

**10 的幂次（maxN 比 D 大 10 倍，最常见的"取高位"场景）**

| D | maxN | S | M | K | type |
|---|------|---|---|---|------|
| 10 | 99 | 0 | 103 | 10 | u16 |
| 100 | 999 | 0 | 41 | 12 | u16 |
| 1000 | 9999 | 0 | 8389 | 23 | u32 |
| 10000 | 99999 | 0 | 107375 | 30 | u64 |
| 100000 | 999999 | 0 | 687195 | 36 | u64 |
| 1000000 | 9999999 | 0 | 17592187 | 44 | u64 |
| 10000000 | 99999999 | 0 | 112589991 | 50 | u64 |
| 100000000 | 999999999 | 0 | 720575941 | 56 | u64 |
| 1000000000 | 9999999999 | 9 | 18014399 | 45 | u64 |
| 10000000000 | 99999999999 | 10 | 115292151 | 50 | u64 |
| 100000000000 | 999999999999 | 11 | 184467441 | 53 | u64 |
| 1000000000000 | 9999999999999 | 12 | 1180591621 | 58 | u64 |
| 10000000000000 | 99999999999999 | 0 | 123794003928539 | 90 | u128 |
| 100000000000000 | 999999999999999 | 0 | 198070406285661 | 94 | u128 |
| 1000000000000000 | 9999999999999999 | 0 | 2535301200456459 | 101 | u128 |
| 10000000000000000 | 99999999999999999 | 0 | 32451855365842673 | 108 | u128 |
| 100000000000000000 | 999999999999999999 | 0 | 830767497365572421 | 116 | u128 |
| 1000000000000000000 | 9999999999999999999 | 0 | 1329227995784915873 | 120 | u128 |

### 7.16 批量操作：一次搬运 4/8/16 字节

**① 数字转字符串：按 4/8/16 位一组的批量填充**

```c
static inline int32_t fill_t_4_digits(char *buffer, uint32_t digits, int32_t *ptz)
{
    char *s = buffer;
    uint32_t q = FAST_DIV100(digits);          /* 高两位 */
    uint32_t r = digits - q * 100;             /* 低两位 */

    memcpy(s,     &ch_100_lut[q<<1], 2);       /* 一次 memcpy 写 2 字节 */
    memcpy(s + 2, &ch_100_lut[r<<1], 2);       /* 再写 2 字节 */
    *ptz = r ? tz_100_lut[r] : tz_100_lut[q] + 2;
    return 4;
}
```

注意 `memcpy` 固定长度 2 —— 编译器会把它优化成**一条 2 字节的 store 指令**，比两次 `*s++ = ...` 更紧凑，也没有循环开销。

往上还有 `fill_t_8_digits`（8 位）、`fill_t_16_digits`（16 位），以及变长版本 `fill_1_4_digits` / `fill_1_8_digits` / `fill_1_16_digits` / `fill_1_20_digits`。

**② 打印字符串：一次 memcpy 整段搬运**

```c
#define _PRINT_PTR_STRNCAT(str, slen) do {          \
    _PRINT_PTR_REALLOC((slen + 1));                 \
    memcpy(print_ptr->cur, str, slen);              \
    print_ptr->cur += slen;                         \
} while(0)
```

`memcpy` 会被编译器展开成最优的 `mov` 序列（甚至向量化），远快于逐字节 `while (*p) *d++ = *p++`。

**③ 格式化缩进：memset 一次填满**

```c
static inline int _print_addi_format(json_print_t *print_ptr, size_t depth, size_t addi_size)
{
    _PRINT_PTR_REALLOC((depth + 2 + addi_size));
    *print_ptr->cur++ = '\n';
    memset(print_ptr->cur, '\t', depth);      /* 一次 memset 写完所有缩进的 tab */
    print_ptr->cur += depth;
    return 0;
}
```

**④ 128 位乘法：有硬件指令就用，没有就退回纯 C**

```c
static inline u64x2_t u128_mul(uint64_t x, uint64_t y)
{
    u64x2_t ret;
#if defined(_MSC_VER)
    ret.lo = _umul128(x, y, &ret.hi);          /* MSVC 内建 */
#elif USING_U128_CALC
    const u128 p = (u128)x * (u128)y;          /* GCC/Clang 的 __int128 */
    ret.hi = (uint64_t)(p >> 64);
    ret.lo = (uint64_t)p;
#else
    /* 可移植回退：拆成 4 次 32×32 乘法 */
    const uint64_t M32 = 0XFFFFFFFF;
    const uint64_t a = x >> 32, b = x & M32;
    const uint64_t c = y >> 32, d = y & M32;
    ...
#endif
    return ret;
}
```

注意这个细节：LJSON 用编译器内建（不是 SIMD，只是整数指令），但**同时提供了纯 C 的回退实现**。这就是为什么它"不依赖 CPU 特性"却仍然很快 —— 有就用，没有就退化，行为完全一致。

### 7.17 分支预测与手动循环展开

**① `likely` / `unlikely`：把概率告诉编译器**

```c
#if defined(__GNUC__) || defined(__clang__)
#define likely(cond)                    __builtin_expect(!!(cond), 1)
#define unlikely(cond)                  __builtin_expect(!!(cond), 0)
#else
#define likely(cond)                    (cond)
#define unlikely(cond)                  (cond)
#endif
```

源码里到处可见，且**每一处都对应一个明确的概率判断**：

| 位置 | 写法 | 判断依据 |
|------|------|----------|
| 内存池分配 | `likely(剩余空间够)` | 绝大多数分配都能在当前块完成 |
| quick_stage 回落 | `unlikely(!jkey.str && ...)` | 绝大多数字符串没有转义 |
| 打印字符串 | `likely(!info->escaped)` | 绝大多数字符串不含转义字符 |
| 跳过空白 | `likely(*str)` | 绝大多数位置不是字符串末尾 |
| 解析转义 | `unlikely(_is_special_char(c))` | 绝大多数字符不是特殊字符 |
| 文件读取 | `likely(offset + read_size <= readed)` | 绝大多数读取命中已缓冲的数据 |
| 打印扩容 | `unlikely(剩余空间 < nz)` | 绝大多数写入不需要扩容 |
| 链表追加 | `unlikely(json_list_empty(head))` | 只有第一个元素时为空表 |

编译器据此把**高概率路径放在 fall-through 上**，减少跳转与指令 cache 的浪费。

**② 手动循环展开：把 8 次判断压成 1 次边界判断**

字符串解析的核心循环：

```c
loop1:
    while (1) {
        c = *str++; if (unlikely(_is_special_char((uint8_t)c))) goto next1;
#if JSON_MANUAL_LOOP_UNFOLD
        c = *str++; if (unlikely(_is_special_char((uint8_t)c))) goto next1;
        c = *str++; if (unlikely(_is_special_char((uint8_t)c))) goto next1;
        ... /* 共 8 次 */
#endif
    }
```

以及打印时的转义扫描：

```c
#if JSON_MANUAL_LOOP_UNFOLD
    while (end - str >= 8) {                 /* 8 个一组，只做一次边界判断 */
        c = *str++; ch = _is_escape_char(c); if (unlikely(ch)) goto next;
        ... /* 共 8 次 */
    }
#endif
    while (end > str) {                      /* 收尾：处理剩余不足 8 个的部分 */
        c = *str++; ch = _is_escape_char(c); if (unlikely(ch)) goto next;
    }
```

**收益**：循环控制（比较 + 跳转）从"每字符 1 次"降到"每 8 字符 1 次"，同时给编译器更大的指令调度空间。

**注意**：这个开关是**可配置的**，且 **MSVC 下默认为 0**：

```c
#ifndef JSON_MANUAL_LOOP_UNFOLD
#if defined(_MSC_VER)
/* MSVC's performance deteriorates after using unfolding */
#define JSON_MANUAL_LOOP_UNFOLD         0
#else
#define JSON_MANUAL_LOOP_UNFOLD         1
#endif
#endif
```

源码注释直白地写着"MSVC 使用展开后性能反而下降"。**这就是工程态度：不迷信任何优化手段，用实测决定。**

### 7.18 真流式：把 I/O 压进解析/打印流水线

**① 文件解析：固定窗口 + memmove 压缩**

```c
static int _get_file_parse_ptr(json_parse_t *parse_ptr, int read_offset, size_t read_size, char **sstr)
{
    offset = parse_ptr->offset + read_offset;
    size = (ssize_t)(parse_ptr->readed - offset);

    if (likely(offset + read_size <= parse_ptr->readed)) {
        *sstr = parse_ptr->str + offset;        /* ★ 快路径：数据在已读缓冲区里，直接给指针 */
    } else if (parse_ptr->read_size == 0) {
        ...                                      /* 文件读完 */
    } else {
        /* 慢路径：把未消费的数据挪到缓冲区开头，再读下一块 */
        size = (ssize_t)(parse_ptr->readed - parse_ptr->offset);
        if (size && parse_ptr->offset)
            memmove(parse_ptr->str, parse_ptr->str + parse_ptr->offset, size);
        parse_ptr->offset = 0;
        parse_ptr->readed = size;

        if (read_size > parse_ptr->size) {      /* 缓冲区不够大就扩容 */
            while (read_size > parse_ptr->size)
                parse_ptr->size += parse_ptr->read_size;
            ...
        }

        size = (ssize_t)(parse_ptr->size - parse_ptr->readed);
        if ((rsize = read(parse_ptr->fd, parse_ptr->str + parse_ptr->readed, size)) != size)
            parse_ptr->read_size = 0;           /* 文件读完 */
        parse_ptr->readed += rsize < 0 ? 0 : rsize;
        parse_ptr->str[parse_ptr->readed] = '\0';
        ...
    }
    return (int)size;
}
```

三个要点：

* **快路径占比极高**：只要所需数据已在缓冲区，就只是一次指针加法 —— `likely()` 明确告诉编译器这是常态。
* **缓冲区大小固定**（默认 `JSON_PARSE_READ_SIZE_DEF` = **8192**），与文件大小**无关** —— 这就是"常数级内存"的来源。
* **`memmove` 压缩**：只在缓冲区尾部不足时才把剩余数据挪到开头，通常挪动的字节数很小。

**② 打印到文件：缓冲满了就写一次**

```c
static int _print_file_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t len = GET_BUF_USED_SIZE(print_ptr);

    if (len > 0) {
        if (unlikely(len != (size_t)write(print_ptr->fd, print_ptr->ptr, len))) {
            JsonErr("write failed!\n");
            return -1;
        }
        print_ptr->cur = print_ptr->ptr;        /* ★ 写完后缓冲区重置，继续复用 */
    }

    len = slen + 1;
    if (print_ptr->size < len) {                /* 只有当单条数据比整个缓冲区还大时才扩容 */
        while (print_ptr->size < len)
            print_ptr->size += print_ptr->plus_size;
        ...
    }
    return 0;
}
```

**这就是"真流式"与"伪流式"的分界**：

```
伪流式：解析完 → 打印成完整字符串 → 一次性 write
真流式：解析/生成一点 → 缓冲 → 缓冲满就 write → 继续（内存始终是常数）
```

所以 1 GB 的 JSON 也能用 KB 级内存完成格式化转换 —— 而实测（citm 1.7 GB）模式 7 的总耗时只比经典 DOM 的"读+解析+格式化（不写）"多 5.8%。

> 这同时也把 **CPU 计算与磁盘 I/O 重叠**了起来：解析下一块数据时，上一块的写入已经在进行。

### 7.19 打印缓冲：几何增长 + 外部复用

打印到字符串时的扩容策略（避免过度扩容，也避免频繁扩容）：

```c
static int _print_str_ptr_realloc(json_print_t *print_ptr, size_t slen)
{
    size_t used = GET_BUF_USED_SIZE(print_ptr);
    size_t len = used + slen + 1;

    /* ① 按"已打印对象数 vs 预估总数"的比例，一次性把容量补到位 */
    while (print_ptr->item_total < print_ptr->item_count) {
        print_ptr->item_total += JSON_PRINT_NUM_PLUS_DEF;       /* 默认 +64 */
        print_ptr->size += print_ptr->plus_size >> 2;
    }
    if (print_ptr->item_total - print_ptr->item_count > print_ptr->item_count) {
        print_ptr->size += print_ptr->size;                    /* 剩余还很多 → 翻倍 */
    } else {
        print_ptr->size += (uint64_t)print_ptr->size *
            (print_ptr->item_total - print_ptr->item_count) / print_ptr->item_count;  /* 按比例精确增长 */
    }

    while (print_ptr->size < len)
        print_ptr->size += print_ptr->plus_size;               /* 默认 8192 */
    ...
}
```

**设计要点**

* **利用 `item_total` 这个先验信息**：调用者传入对象总数（或用 `json_item_total_get` 算出），库就能估算还差多少空间，**一次性扩到位**，而不是每次差一点就 realloc。
* **剩余多时翻倍（O(1) 摊还）**，剩余少时按比例精确增长（避免浪费内存）。
* 配合 `json_print_ptr_t` 外部传入，可以做到**跨多次打印完全复用同一块缓冲**（零堆分配模式的基础）。

统计一下 LJSON 里**所有"预分配 / 多分配"的地方**，可以看到同一套思路贯穿全库：

| 位置 | 预留策略 | 默认值 | 换来的收益 |
|------|----------|-------:|------------|
| 内存池块大小 | 一次申请大块 | `JSON_POOL_MEM_SIZE_DEF` = 8192 | 成百上千次分配免进 malloc |
| 打印缓冲扩容 | 几何增长 + 按对象数预估 | `JSON_PRINT_SIZE_PLUS_DEF` = 8192 | 减少 realloc 次数 |
| 打印热路径检查 | 多检查 64 字节余量 | `_PRINT_ALLOC_ADDI_SIZE` = 64 | 多次小写入免检查 |
| 对象栈数组 | 栈上先备好 16 个 | `JSON_ITEM_NUM_PLUS_DEF` = 16 | 浅层 JSON 完全不用堆 |
| 解析读缓冲 | 固定窗口 | `JSON_PARSE_READ_SIZE_DEF` = 8192 | 常数内存 + 快路径命中 |
| 内存池初始估算 | 按文件大小 1/8 | `JSON_PARSE_NUM_DIV_DEF` = 8 | 减少块扩容次数 |

### 7.20 编译期配置：把运行期开关变成不存在的代码

LJSON 用 `#if` 而不是运行时 `if` 来处理配置，例如跳过空白：

```c
static inline void _skip_blank(json_parse_t *parse_ptr)
{
    unsigned char *str = NULL;
    int cnt = 0;

    while (_get_parse_ptr(parse_ptr, cnt, 64, (char **)&str)) {
        while (IS_BLANK(*str)) {
            ++str;
            ++cnt;
        }
        if (likely(*str)) {
#if JSON_PARSE_SKIP_COMMENT                 /* ★ 编译期决定：要么存在，要么彻底不存在 */
            if (unlikely(*str == '/')) {
                if (!_skip_comment(parse_ptr, &cnt))
                    continue;
            }
#endif
            break;
        }
    }
    _UPDATE_PARSE_OFFSET(cnt);
}
```

**这是 LJSON 敢"默认关闭 JSON5"的底气**：关闭时，注释相关的代码**根本不会被编译进二进制**，热路径上连一次判断都没有。开启时才付出代价。**配置是被编译进去的，不是被判断过去的。**

同样的手法还有：

| 宏 | 关闭时的效果 |
|----|--------------|
| `JSON_PARSE_SKIP_COMMENT` | 注释分支完全不生成代码 |
| `JSON_PARSE_SPECIAL_QUOTES` | 单引号/无引号键的分支完全不生成代码 |
| `JSON_PARSE_SPECIAL_CHAR` | 特殊字符检查完全不生成代码 |
| `JSON_ERROR_PRINT_ENABLE` | 所有 `JsonErr` 变成空语句 `do {} while(0)` |
| `JSON_PRINT_UTF16_SUPPORT` | `\u00XX` 转义路径与相关查表不生成代码 |
| `JSON_MANUAL_LOOP_UNFOLD` | 不生成展开版本，只保留紧凑循环 |
| `JSON_SAX_APIS_SUPPORT` | SAX 相关代码整体不编译 |
| `USING_SMALL_LUT` | 换用小查找表，代码段小约 5 KB |

### 7.21 精度换效率：ldouble 的取舍

这是 LJSON 最具争议、也最能说明其工程哲学的一点。

**IEEE-754 的"正确"做法是什么？**

标准约定：浮点转字符串应该产生**能精确往返（round-trip）**的最短表示。为此需要 17 位十进制精度（某些情况 16 位够用），并且边界情况要按**偶数舍入**规则处理。

**LDOUBLE 的做法**

* 采用 **17 位十进制精度**
* 边界处理**追求最短表示**，而不是机械地做偶数舍入
* 结果是：**性能大幅提升，且与 `printf` 输出在精度范围内一致**

**换来的是什么？** 看数据：

| 场景 | LJSON（模式3） | cJSON | 差距 |
|------|---------------:|------:|-----:|
| canada 格式化打印 | 273 ms | 5956 ms | **21.8 倍** |
| canada 压缩打印 | 260 ms | 6007 ms | **23.1 倍** |

canada.json 是浮点密集的地理数据，几乎每个值都要走一次浮点转字符串。**打印性能差 20 倍以上，绝大部分来自这一个算法。**

**为什么这是合理的取舍？**

* 绝大多数业务场景（日志、配置、接口报文）关心的是"读起来正确、体积够小"，而不是"第 17 位是否严格符合某种舍入约定"。
* 为此付出 20 倍的性能损失，是不划算的。
* **而且这个取舍是可逆的**：想要标准行为就 `-DJSON_DTOA_ALGORITHM=1`，一个编译开关的事。

同样的哲学也体现在 JSON 解析配置上：**默认关掉影响热路径的 JSON5 特性，需要的人自己打开。** 快的默认、可选的完整 —— 这是 LJSON 一贯的取舍方式。

> 唯一"没有为性能妥协"的地方是**数字解析**：LJSON 采用了更精确的数字处理，代价是 canada 解析的领先幅度从"数量级"收敛到 20%。**正确性优先于跑分，这条线 LJSON 守住了。**

### 7.22 优化清单总表

| # | 优化手段 | 层次 | 源码位置（举例） | 消除的开销 |
|:--:|----------|:----:|------------------|------------|
| 1 | 7+4 模式矩阵 | 框架 | `json_parse_common` 的 choice 分发 | 场景与路径不匹配的浪费 |
| 2 | 自研 ldouble 算法 | 算法 | `jnum.c` / `jnum_dtoa` | 慢速的 `sprintf` 浮点转换 |
| 3 | **冷热分离（函数指针一次分派）** | 微架构 | `_skip_blank_rapid` / `_json_parse_value_rapid` | 热路径上的"数据够不够"判断 |
| 4 | **两阶段解析（quick_stage 前置）** | 微架构 | `_json_parse_string_quick_stage` | 转义处理对 i-cache 的污染 |
| 5 | **打印局部 `cur` + `SYNC` 宏** | 微架构 | `_json_print_value_format` | N 次内存读改写 |
| 6 | **多分配 64 字节** | 空间换时间 | `_PRINT_ALLOC_ADDI_SIZE` | 频繁 realloc 检查 |
| 7 | 32 字节紧凑对象 | 内存层次 | `json_object` | cache miss、内存占用 |
| 8 | 位域复用（类型塞进 ikey） | 微优化 | `json_strinfo_t` | 4 字节/节点 |
| 9 | union 共享值空间 | 内存层次 | `json_value_t` | 多类型的空间浪费 |
| 10 | 单向循环链表（head 指向尾） | 数据结构 | `json_list_add_tail` | 双向链表的 8 字节/节点；追加 O(1) |
| 11 | bump 内存池 | 减少分配 | `pjson_memory_alloc` | `malloc` 调用开销 |
| 12 | 三路内存管理器 | 内存层次 | `json_mem_t` | 对齐浪费 |
| 13 | 内存池刷新复用 | 减少分配 | `_json_mem_refresh` | 循环中的重复分配 |
| 14 | 栈上对象数组 | 减少分配 | `stack_array[JSON_ITEM_NUM_PLUS_DEF]` | 浅层 JSON 的堆分配 |
| 15 | 原地字符串复用 | 减少分配 | `_json_parse_string_reuse` | 每个字符串的 malloc + memcpy |
| 16 | 缓存字符串长度 | 空间换时间 | `info.len` | 打印时的 `strlen` |
| 17 | 缓存转义标记 | 空间换时间 | `info.escaped` | 打印时的转义扫描 |
| 18 | 缓存哈希 + 排序 | 空间换时间 | `json_item_t.hash` / `qsort` | 查找时的 O(N) 遍历与重复哈希 |
| 19 | 二分查找 | 数据结构 | `json_search_object_item` | O(N) → O(logN) |
| 20 | 二维查表（数字/十六进制/尾零） | 空间换时间 | `ch_100_lut` / `tz_100_lut` / `hex_char_lut` | 除法、取余、循环 |
| 21 | 转义字符表（256 项） | 空间换时间 | `char_escape_lut` | 多分支 switch |
| 22 | 快速整除（乘法 + 移位） | 微优化 | `FAST_DIV100` / `FAST_DIV10000` | 整数除法指令 |
| 23 | 批量 memcpy / memset | 微架构 | `_PRINT_PTR_STRNCAT` / `_print_addi_format` | 逐字节循环 |
| 24 | 硬件 128 位乘法（含 C 回退） | CPU | `u128_mul` | 64 位乘法的高位计算 |
| 25 | `likely` / `unlikely` | CPU | 全库 | 分支预测失败、指令布局 |
| 26 | 手动循环展开（可配置） | CPU | `JSON_MANUAL_LOOP_UNFOLD` | 循环控制开销 |
| 27 | 真流式分块读 | I/O | `_get_file_parse_ptr` | 峰值内存、一次性读入 |
| 28 | 真流式缓冲写 | I/O | `_print_file_ptr_realloc` | 峰值内存、一次性写出 |
| 29 | 打印缓冲几何增长 + 复用 | 减少分配 | `_print_str_ptr_realloc` | 频繁 realloc |
| 30 | 编译期配置开关 | 编译器 | `#if JSON_PARSE_xxx` | 运行期判断、死代码体积 |
| 31 | 内联与宏封装 | 编译器 | `static inline` / `INLINE_ATTR` | 函数调用开销 |
| 32 | 错误串哨兵 | 微优化 | `JSON_PARSE_ERROR_STR "Z"` | 指针为 NULL 的判断 |

> 这 32 项里，没有一项依赖 SIMD、没有一项依赖特定 CPU 指令、没有一项依赖第三方库。
> **这就是"纯 C 也能很快"的答案：性能来自设计与取舍，不来自指令集。**

## 8. 横向对比：LJSON vs yyjson / RapidJSON / cJSON

> 说明：本章的**架构性描述**来自本仓库 `notes/` 下的分析文档；**性能数字**全部来自[第六章 性能测试](#六-性能测试)的实测输出，不做推测。
> 凡是没有实测支撑的项，一律标注为"—"或说明"无实测数据"。

### 8.1 一图看懂

| 维度 | **LJSON** | yyjson | RapidJSON | cJSON |
|------|-----------|--------|-----------|-------|
| 语言 | **C99 / C11** | C99 | **C++（模板，header-only）** | C89/C99 |
| 第三方依赖 | **0** | 0 | 0 | 0 |
| 需要 C++ 编译器 | **否** | 否 | **是** | 否 |
| CPU 特性依赖（SIMD 等） | **无** | 无 | 可选（编译期开关） | 无 |
| 解析模式数量 | **7** | 2（只读 / 原地只读） | DOM / SAX | 1 |
| 打印模式数量 | **4** | 1（全缓冲） | DOM / SAX → 字符串或文件 | 1 |
| 真流式（边读边解析） | **✅** | ❌ 不支持 | ❌（需自行喂数据） | ❌ 不支持 |
| 真流式打印（边打边写） | **✅** | ❌ | ✅（`FileWriteStream`） | ❌ |
| 复用模式可编辑 | **✅ 原生可编辑** | ❌ 只读，需转换（内存翻倍） | ✅（in-situ 受限） | — |
| 内存池 / 块分配 | **✅** | ✅（大块预分配，冗余度高） | ✅（可插拔分配器） | ❌ 每节点 malloc |
| 零堆分配循环 | **✅** | 部分 | ❌ | ❌ |
| 字符串尾 | 标准 C 字符串（1 个 `\0`） | in-situ 非标准（尾 4 个 `\0`） | 标准 | 标准 |
| 键与值存储 | 一起存储 | 分开存储 | 一起 | 一起 |
| 访问加速 | **`json_items`：数组 O(1)、对象 O(logN)** | 未知 | 成员查找 O(N) | O(N) |
| 扩展类型 | **长整数 / 十六进制 / 二进制** | 部分 | 部分 | 无 |
| JSON5 | **支持（默认关闭，可开启）** | 部分 | ❌ | ❌ |
| 代码规模 | **约 1 万行** | 约 2 万行 | 较大（模板） | 小 |
| 可读性 / 可维护性 | **高** | 极低（宏密集） | 中 | 高 |

### 8.2 vs cJSON

**架构差异**

cJSON 是最经典的 C 语言 JSON 库，设计目标是可读性与易用性，性能不是它的重点：

* 每个节点一次 `malloc`，字符串一律复制一份
* 没有内存池、没有原地复用、没有流式接口
* 双向链表管理子节点（删除 O(1)，但每节点多 8 字节）

LJSON 的接口**刻意参考了 cJSON**（`json_get_object_item` / `json_get_array_item` / `json_create_xxx` 等命名与语义高度一致），所以从 cJSON 迁移到 LJSON 的成本很低。

**实测差距**（模式 3 可编辑复用 vs cJSON，单位 ms）

| 数据集 | 解析 | 格式化打印 | 压缩打印 |
|--------|-----:|-----------:|---------:|
| twitter（603 MB） | 320 vs **1512** | 234 vs **1037** | 182 vs **948** |
| citm_catalog（1.7 GB） | 825 vs **3427** | 472 vs **1677** | 364 vs **1503** |
| canada（215 MB） | 281 vs **1862** | 273 vs **5956** | 260 vs **6007** |

* 解析快 **4.2~6.6 倍**
* 打印快 **3.4~23 倍**，其中**浮点密集的 canada 差距最大（约 22 倍）** —— 自研 ldouble 数值引擎的直接收益

**什么时候仍然选 cJSON**：项目已经深度绑定 cJSON、JSON 很小很稀疏、或者你更看重"代码行数极少、人人都能读懂"。

### 8.3 vs RapidJSON

**架构差异**

* RapidJSON 是 **C++ 模板实现**的 header-only 库，**必须用 C++ 编译器**；LJSON 是纯 C，可以进 C 工程、可以交叉编译到没有 C++ 运行时的环境。
* RapidJSON 提供 DOM 与 SAX 两种风格，功能完整；LJSON 提供 7+4 种模式，并额外提供**真流式文件处理**与**零堆分配**能力。
* 模板带来的代价是编译产物较大、编译较慢；LJSON 是两个 `.c` 文件，编译极快。

**实测差距**（模式 3 可编辑复用 vs RapidJSON，单位 ms）

| 数据集 | 解析 | 格式化打印 | 压缩打印 |
|--------|-----:|-----------:|---------:|
| twitter（603 MB） | 320 vs **871** | 234 vs **752** | 182 vs **675** |
| citm_catalog（1.7 GB） | 825 vs **1411** | 472 vs **1089** | 364 vs **708** |
| canada（215 MB） | 281 vs **342** | 273 vs **769** | 260 vs **624** |

* 解析快 **1.2~2.7 倍**
* 打印快 **1.9~3.7 倍**

**什么时候仍然选 RapidJSON**：工程已经是 C++、需要 RapidJSON 的 Schema 校验或完整的 UTF-16/32 转码体系、或者更信任它的长期社区生态。

### 8.4 vs yyjson

这是最常被拿来比较的一组，也是最需要讲清楚架构边界的一组。

**核心结论**

> **yyjson ≈ LJSON 的 DOM 复用模式 + 激进只读优化。**
> 首次提交时间：LJSON **2019.10**，yyjson **2020.10**。

yyjson 在 LJSON 的"DOM 复用模式"基础上做了四项激进优化：

1. **非标准字符串尾部**：用 `\0\0\0\0`（4 个）替代标准 C 字符串尾部，减少边界判断
2. **宏展开循环**：大量使用宏手动展开解析循环，压榨指令流水
3. **预分配冗余内存**：一次性分配大块内存（冗余度较高）存储对象结构，避免频繁 realloc
4. **只读不可编辑**：解析后的结构不可修改（因此对象可以做到 16 字节）

这些手段让 yyjson 在**只读**场景下表现惊艳，但也划出了它的适用边界。

**实测表现（三种模式的胜负关系）**

| 对比 | 结果 | 数据依据 |
|------|------|----------|
| LJSON 模式3（可编辑）**vs** yyjson 可编辑模式 | **LJSON 快 1.7~2.5 倍（解析）** | twitter 320 vs 784；citm 825 vs 1890；canada 281 vs 468 |
| LJSON 模式3 **vs** yyjson 只读模式（解析） | **LJSON 快 20%~46%** | twitter 320 vs 435；citm 825 vs 1206；canada 281 vs 336 |
| LJSON **vs** yyjson 只读模式（压缩打印，citm） | **LJSON 略慢约 10%** | 模式3 364 vs 330；模式2 334 vs 330（基本持平） |

**为什么会有这三种不同的结果？**

* **yyjson 可编辑模式为什么慢？** 因为它需要**再复制转换一份数据**到可编辑对象，且**内存要翻倍**；而 LJSON 的复用模式**原生就是可编辑的**。
* **yyjson 只读模式为什么和 LJSON 互有胜负？** yyjson 不支持标准 C 字符串，遇到标准字符串时需要**先复制一份原始数据在尾部补 4 个 0** 再解析；LJSON 支持标准字符串，少了这一次复制。LJSON 则在"单一模式的激进优化"和"宏手动展开"上不如 yyjson 极致。
* **yyjson 只读复用模式为什么仍略快？** 来自四个因素：模式单一、激进资源使用（预分配冗余内存）、非标准特性（尾后 4 个 `\0`、非对齐访问）、宏循环展开。

**架构能力差异（这是选型的关键）**

| 能力 | LJSON | yyjson |
|------|:------:|:------:|
| 真流式解析（GB 级文件常数内存） | **✅** | ❌ |
| 真流式打印 | **✅** | ❌ |
| 复用模式可编辑（无需转换） | **✅** | ❌ |
| 标准 C 字符串（尾 1 个 `\0`） | **✅** | ❌（尾 4 个） |
| 大文件解析内存占用 | 可常数级 | 较高（约为 LJSON 的一倍） |
| 代码可读性 | 高（约 1 万行，结构清晰） | 极低（约 2 万行，宏密集） |

**一句话总结**

> **yyjson 是一把锋利的手术刀**（专注只读解析的极限性能）；
> **LJSON 是一套完整的工具箱**（性能、灵活性、可维护性兼得）。
>
> 如果你的场景是"只读、纯内存、追求极限解析速度"，yyjson 值得考虑；
> 如果你需要**消息处理、流式、可编辑、大文件低内存、可维护**，LJSON 是更合适的选择。

### 8.5 决策树：我该选哪个

```
你的 JSON 处理需求是什么？
│
├─ 数据在内存里，只解析一次，只读，追求极限速度
│     └─ yyjson 只读模式 或 LJSON 模式3（差距 20%~46%，LJSON 更快且可编辑）
│
├─ 数据在内存里，要修改（增删改）
│     └─ ★ LJSON 模式3（可编辑复用，实测比 yyjson 可编辑模式快 1.7~2.5 倍）
│
├─ 高频消息收发，循环解析/打印
│     └─ ★ LJSON 模式3 + SAX 打印 + 零堆分配（见 4.1 节）
│
├─ 文件很大（百 MB ~ GB 级）
│     ├─ 需要 DOM 树        → LJSON 模式5（边读边解析 + 内存池）
│     └─ 不需要 DOM 树      → ★ LJSON 模式7（常数级内存全流程）
│
├─ 要生成很大的 JSON 文件
│     └─ ★ LJSON SAX → 文件（边生成边写）
│
├─ 工程是纯 C / 无 C++ 运行时 / 交叉编译 / 嵌入式 RTOS
│     └─ ★ LJSON（纯 C99，零依赖，无 SIMD 要求）
│
└─ 需要 JSON5 / 长整数 / 十六进制 / 二进制扩展
      └─ ★ LJSON（原生支持）
```

---

## 9. 常见问题（FAQ）

**Q1：JSON5 默认支持吗？**
不默认。默认配置是**速度优先**，位于解析热路径的两条 JSON5 特性（注释 `JSON_PARSE_SKIP_COMMENT`、单引号/无引号键 `JSON_PARSE_SPECIAL_QUOTES`）默认为 **0**。需要时在 `json.c` 中置 1 重新编译。注意开启后解析速度会下降。

**Q2：解析和打印分别该用哪个模式？**
解析默认用**模式 3（复用模式）**，打印默认用 **SAX 直接打印**（不要先建 DOM）。有特殊需求才换别的模式，详见 [3.3 选型建议](#33-选型建议)。

**Q3：为什么我的程序里 LJSON 没有 README 上那么快？**
先确认三件事：① 是否用了 `-O2`；② 是否开了影响速度的 JSON5 开关（注释、特殊引号）；③ 是否选对了模式（模式 1 是最慢的，模式 3 最快）。另外请核对计时口径 —— README 中模式 4/5/7 的耗时会包含文件读写。

**Q4：内存池模式下能删除单个节点吗？**
不能。内存块只能统一申请、统一释放。需要单独释放请改用模式 1（经典 malloc）。

**Q5：复用模式把我的原始 buffer 改了，怎么办？**
这是设计使然（零拷贝的代价）。如果不能接受，改用模式 2（`json_fast_parse_str`），它会把字符串复制一份。

**Q6：v3 能直接升级到 v5 吗？**
不能。v5.0.0 起 64 位系统上对象从 40 字节压到 32 字节，与 v3.x.x **不兼容**，升级时需一并更新调用代码。

**Q7：能只用数值引擎吗？**
能。只编译 `jnum.c`，只包含 `jnum.h` 即可，不依赖 `json.h`。

**Q8：`ldouble` 的输出和 `printf` 一样吗？**
在精度范围内一致，且始终输出最短精确表示。若需要严格的标准库行为，用 `-DJSON_DTOA_ALGORITHM=1` 切到 `sprintf`。

**Q9：支持 Windows 吗？**
支持。用 Visual Studio 2022 打开 `MSC\ljson\ljson.sln`；MinGW 也可编译。MSVC 下库会自动关闭手工循环展开（`JSON_MANUAL_LOOP_UNFOLD=0`）并把 `jnum_atoi` 等改为非 inline，因为实测那样更快。

**Q10：可以处理多大的文件？**
理论上没有上限。SAX 文件流模式（模式 7）的内存占用是**常数级**，1 GB 的 JSON 也能用 KB 级内存处理完毕（实测 1.7 GB 的 citm_catalog 全流程 2946 ms）。

**Q11：为什么不用 SIMD？**
因为一旦依赖 SIMD，就要为每种架构写一套代码、运行时还要做 CPU 特性探测，可移植性和维护性都会下降。LJSON 的选择是：**把性能建立在算法与数据结构上，让它对所有架构都一样快。**

**Q12：解析固定格式的消息时，怎么再快一点？**
用 [4.5 消息解析的两种加速模式](#45-消息解析的两种加速模式) 的两种加速模式：① 遇到数组或大对象先 `json_get_items` 抽成连续数组，下标 O(1)、按 key 二分 O(logN)；② 字段顺序固定时把上一次的节点填进 `prev`，接续查找。两者可以叠加。

**Q13：零堆内存分配模式的完整代码在哪？**
在 [4.2 首选方案：零堆内存分配模式](#42-首选方案零堆内存分配模式)，那是第四章的第一节，也是高频消息收发场景的推荐写法。

**Q14：二进制扩展类型 `<len:data>` 是标准 JSON 吗？**
不是，它是 LJSON 的**扩展**。用于需要在一个 JSON 值里携带裸二进制数据的场景，标准 JSON 解析器无法识别，只在 LJSON 生态内互通时使用。

---

## 10. 参考与许可

**参考代码**

* 参考了 [cJSON](https://github.com/DaveGamble/cJSON) 的实现，其中 `utf16_literal_to_utf8` 函数完全来自 cJSON

**对比测试对象**

* [cJSON](https://github.com/DaveGamble/cJSON)
* [RapidJSON](https://github.com/Tencent/rapidjson)
* [yyjson](https://github.com/ibireme/yyjson)
* [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark)（测试样本来源）

**构建系统**

* [IMAKE / CBuild-ng](https://github.com/lengjingzju/cbuild-ng)（可选，用于 `make` 与交叉编译安装）

**许可证**

* **MIT**（见源码头部的 `SPDX-License-Identifier: MIT`）

**联系方式**

* Phone: +86 18368887550
* wx/qq: 1083936981
* Email: lengjingzju@163.com 3090101217@zju.edu.cn
