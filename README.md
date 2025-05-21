# LJSON Description

[中文版](./README_zh-cn.md)

LJSON is a C implemented JSON library that is much faster than cJSON and substantially faster than RapidJSON, it is currently the fastest general-purpose JSON library and supports all JSON5 features.

LJSON supports JSON parsing, printing and editing, provides DOM and SAX APIs, and I/O supports string and file, it fully supports the test cases of nativejson-benchmark.

By default, LJSON uses the personally developed ldouble algorithm to print double to string. Compared with the standard library, it may only be the 15th or 16th decimal place difference. It is currently the fastest double to string algorithm; users can also choose the personally optimized grisu2 algorithm or dragonbox algorithm.

The ldouble algorithm is an order of magnitude faster than sprintf (40 times faster for some cases), and is also faster than grisu (implemented by Google and used by the browser V8 engine) and dragonbox (used by high-performance computing libraries such as Boost and Eigen) (some cases are 100% faster than grisu2 and 60% faster than dragonbox); the ldouble algorithm can guarantee the shortest output within the precision range, which is consistent with sprintf output.

## Features

* Faster: Print and parse faster than both cJSON and RapidJSON, up to **32** times faster than CJSON and **1.5** time faster than Rapid JSON, refer to below test results
* Lighter: Provide a variety of methods to save memory, such as pool memory, file parsing while reading, file writing while printing, and SAX APIs. It can make memory usage a constant
* Stronger: Support DOM and SAX-style APIs, provide APIs for JSON in classic mode and memory pool mode, support string and file as input and output, is extended to support long long integer and hexadecimal number
* More friendly: C language implementation, does not depend on any other library, does not contain platform-related code, only one header file and source file, and the interface corresponding to cJSON. the code logic is clearer than any other JSON libraries
* JSON5: Supports all JSON5 features, such as hexadecimal digits, comments, array and object tail element comma, single quoted string/key and unquoted key
Buffer: If JSON is parsed and printed multiple times, the allocated memory can be reused, and there is basically no heap memory request inside the library; You can save all sub objects under a certain collection type as data and use hash and binary methods to speed up the search

## Compile and Run

### Compile Method

* Compile directly

```sh
gcc -o ljson json.c -jnum.c json_test.c -lm -O2 -ffunction-sections -fdata-sections -W -Wall
```

* Compile with [IMAKE](https://github.com/lengjingzju/cbuild-ng)

```sh
make O=<output path> && make O=<output path> DESTDIR=<install path>
```

* Cross Compile

```sh
make O=<output path> CROSS_COMPILE=<tool prefix> && make O=<output path> DESTDIR=<install path>
```

* Select double to string algorithm `gcc -DJSON_DTOA_ALGORITHM=n`, n may be 0 / 1 / 2 / 3
    * 0: Personal implementation of ldouble algorithm: faster than Google's default implementation of grisu2 **129%** , faster than Tencent optimized grisu2 implementation **33%**, faster than sprintf **14.6** times
    * 1: C standard library sprintf
    * 2: Personal optimized grisu2 algorithm: Google's grisu2 default implementation is **5.7** times faster than sprintf, Tencent optimized grisu2 implementation is **9.1** times faster than sprintf, LJSON optimized implementation is faster than sprintf **11.4** times
    * 3: Personal optimized dragonbox algorithm: the speed performance is slower than ldouble algorithm, but faster than grisu2 algorithm

### Run Method

```sh
./ljson <json filename> <test index:0-7>
```

### Debug Method

* Set the value of the variable `JSON_ERROR_PRINT_ENABLE` in `json.c` to `1` and then re-compile

### Parse Config

* `#define JSON_PARSE_SKIP_COMMENT         1` : Whether to allow C-like single-line comments and multi-line comments(JSON5 feature)
* `#define JSON_PARSE_LAST_COMMA           1` : Whether to allow comma in last element of array or object(JSON5 feature)
* `#define JSON_PARSE_EMPTY_KEY            0` : Whether to allow empty key
* `#define JSON_PARSE_SPECIAL_CHAR         1` : Whether to allow special characters such as newline in the string(JSON5 feature)
* `#define JSON_PARSE_SPECIAL_QUOTES       1` : Whether to allow single quoted string/key and unquoted key(JSON5 feature)
* `#define JSON_PARSE_HEX_NUM              1` : Whether to allow HEX number(JSON5 feature)
* `#define JSON_PARSE_SPECIAL_NUM          1` :Whether to allow special number such as starting with '.', '+', '0', for example: `+99` `.1234` `10.` `001`(JSON5 feature)
* `#define JSON_PARSE_SPECIAL_DOUBLE       1` : Whether to allow special double such as `NaN`, `Infinity`, `-Infinity`(JSON5 feature)
* `#define JSON_PARSE_SINGLE_VALUE         1` : Whether to allow json starting with non-array and non-object
* `#define JSON_PARSE_FINISHED_CHAR        0` : Whether to allow characters other than spaces after finishing parsing

Note:
* It 100% matches the test cases of [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) when only `JSON_PARSE_EMPTY_KEY` is set to 1, all others are set to 0.
* Setting `JSON_PARSE_SKIP_COMMENT` and `JSON_PARSE_SPECIAL_QUOTES` to 1 will significantly affect the parsing speed.

## Speed Test

### Test Code

Other json test codes are located in the benchmark directory. Just put the corresponding source file in the root directory of the corresponding json project.

```sh
gcc -o cjson cjson_test.c cJSON.c -O2               # cJSON
g++ -o rapidjson rapidjson_test.c -Iinclude -O2     # RapidJSON
gcc -o yyjson yyjson_test.c src/yyjson.c -Isrc -O2  # yyjson
gcc -o strdup strdup_test.c -O2                     # strdup和strlen
```

Test Script

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
	run_cmd ./ljson $src $i
done

run_cmd ./cjson $src
run_cmd ./rapidjson $src
run_cmd ./yyjson $src
run_cmd ./yyjson $src 1
run_cmd ./strdup $src
```

Test Mode

* ljson provides 7 test modes
    * 1: Normal DOM mode, use malloc to apply for memory, parse and print string
    * 2: Fast DOM mode, apply for large memory, then allocate memory from large memory (cannot free small memory alone), parse and print string
    * 3: Reuse DOM mode, apply for large memory, then allocate memory from large memory (cannot free small memory alone), and reuse the original parsed string for key and string value, parse and print string
    * 4: File DOM mode, no need to read the file before parsing or print before writing, use malloc to apply for memory, read file and parse json at the same time, and print json and write file at the same time
    * 5: Fast file DOM mode, no need to read the file before parsing or print before writing, apply for large memory, then allocate memory from large memory (cannot free small memory alone), read file and parse json at the same time, and print json and write file at the same time
    * 6: Normal SAX mode, parse and print string
    * 7: File SAX mode, no need to read the file before parsing, read file and parse json at the same time
* yyjson provides two test modes: unmutable and mutable

### Json Test Result

Note: 'O2' optimization level and default option compilation, the test files come from the [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) project.

> Test Platform: ARM64 Development Board | CPU: ARM CortexA53 | OS: Linux-5.15<br>
> Test Result: LJSON parses 475% faster and prints 2836% faster than cJSON, LJSON parses 131% faster and prints 147% faster than RapidJSON (include file reading and writing)

![AARCH64-Linux Test Result](image/test_for_aarch64.png)

> Test Platform: PC | CPU: Intel i7-10700 | OS: Ubuntu 18.04 (VirtualBox)<br>
> Test Result: LJSON parses 560% faster and prints 3184% faster than cJSON, LJSON parses 75% faster and prints 133% faster than RapidJSON
(include file reading and writing)

![x86_64-Linux Test Result](image/test_for_x86_64.png)

> Test Platform: PC | CPU: Intel i7-1260P | OS: Ubuntu 20.04 (VMWare)<br>
> Test Result: LJSON parses 510% faster and prints 2273% faster than cJSON, LJSON parses 76% faster and prints 144% faster than RapidJSON
(not include file reading and writing)

![x86_64-Linux Test Result2](image/test_for_x86_64-2.png)

### Floating point number to string algorithm Test Result

**Test Script**

```sh
#!/bin/bash

dname=$1

info="\
0.1 \
0.2 \
0.3 \
0.4 \
0.5 \
0.6 \
0.7 \
0.8 \
0.9 \
0.11 \
0.101 \
0.1001 \
0.10001 \
0.100001 \
0.1000001 \
0.10000001 \
0.100000001 \
0.1000000001 \
0.10000000001 \
0.100000000001 \
0.1000000000001 \
0.10000000000001 \
0.100000000000001 \
0.1000000000000001 \
1.1 \
2.2 \
3.3 \
4.4 \
5.5 \
6.6 \
7.7 \
8.8 \
9.9 \
0.1 \
0.12 \
0.123 \
0.1234 \
0.12345 \
0.123456 \
0.1234567 \
0.12345678 \
0.123456789 \
0.12345678901 \
0.123456789012 \
0.1234567890123 \
0.12345678901234 \
0.123456789012345 \
0.1234567890123456 \
0.1e100 \
0.12e100 \
0.123e100 \
0.1234e100 \
0.12345e100 \
0.123456e100 \
0.1234567e100 \
0.12345678e100 \
0.123456789e100 \
0.12345678901e100 \
0.123456789012e100 \
0.1234567890123e100 \
0.12345678901234e100 \
0.123456789012345e100 \
0.1234567890123456e100 \
0.1e-100 \
0.12e-100 \
0.123e-100 \
0.1234e-100 \
0.12345e-100 \
0.123456e-100 \
0.1234567e-100 \
0.12345678e-100 \
0.123456789e-100 \
0.12345678901e-100 \
0.123456789012e-100 \
0.1234567890123e-100 \
0.12345678901234e-100 \
0.123456789012345e-100 \
0.1234567890123456e-100 \
1e1 \
1e2 \
1e3 \
1e4 \
1e5 \
1e6 \
1e7 \
1e8 \
1e9 \
1e10 \
1e11 \
1e12 \
1e13 \
1e14 \
1e15 \
1e16 \
1e-1 \
1e-2 \
1e-3 \
1e-4 \
1e-5 \
1e-6 \
1e-7 \
1e-8 \
1e-9 \
1e-10 \
1e-11 \
1e-12 \
1e-13 \
1e-14 \
1e-15 \
1e-16 \
0.9 \
0.99 \
0.999 \
0.9999 \
0.99999 \
0.999999 \
0.9999999 \
0.99999999 \
0.999999999 \
0.9999999999 \
0.99999999999 \
0.999999999999 \
0.9999999999999 \
0.99999999999999 \
0.999999999999999 \
0.9999999999999999 \
0.1 \
0.11 \
0.111 \
0.1111 \
0.11111 \
0.111111 \
0.1111111 \
0.11111111 \
0.111111111 \
0.1111111111 \
0.11111111111 \
0.111111111111 \
0.1111111111111 \
0.11111111111111 \
0.111111111111111 \
0.1111111111111111 \
1e-306 \
1e-307 \
1e-308 \
1e-309 \
1e-311 \
1e-312 \
1e-313 \
1e-314 \
1e-315 \
1e-316 \
1e-317 \
1e-318 \
1e-319 \
1e-320 \
1e-321 \
1e-322 \
1e-323 \
"

for i in $info; do
    #./$dname/jnum_test $i 10000000
    ./$dname/jnum_test $i 10000000 | sed -e 's/: /:| `/g' -e 's/$/`/g' -e 's/\t/` `/g' | cut -d ':' -f 2 |xargs | sed -e 's/$/ |/g'

done
```

> Test Platform: PC | CPU: Intel i7-1260P | OS: Ubuntu 20.04 (VMWare)<br>
> Test Command: `./jnum_test <num> 10000000` # Test ten million times
> Test Result: ldouble is an order of magnitude faster than sprintf (40 times faster in some cases), and slightly faster than grisu2 and dragonbox; ldouble can guarantee the shortest output within the precision range, which is consistent with printf output.

Note: The three values are "the output string", "the time taken for running ten million times", and "the percentage of speed improvement compared to sprintf".

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1313ms` | `0.1` `82ms` `1601%` | `0.1` `73ms` `1799%` | `0.1` `93ms` `1412%` |
| `0.2` | `0.2` `1264ms` | `0.2` `79ms` `1600%` | `0.2` `74ms` `1708%` | `0.2` `93ms` `1359%` |
| `0.3` | `0.3` `1344ms` | `0.3` `84ms` `1600%` | `0.3` `79ms` `1701%` | `0.3` `95ms` `1415%` |
| `0.4` | `0.4` `1253ms` | `0.4` `80ms` `1566%` | `0.4` `74ms` `1693%` | `0.4` `91ms` `1377%` |
| `0.5` | `0.5` `1277ms` | `0.5` `77ms` `1658%` | `0.5` `72ms` `1774%` | `0.5` `84ms` `1520%` |
| `0.6` | `0.6` `1293ms` | `0.6` `78ms` `1658%` | `0.6` `72ms` `1796%` | `0.6` `83ms` `1558%` |
| `0.7` | `0.7` `1303ms` | `0.7` `81ms` `1609%` | `0.7` `72ms` `1810%` | `0.7` `83ms` `1570%` |
| `0.8` | `0.8` `1248ms` | `0.8` `79ms` `1580%` | `0.8` `78ms` `1600%` | `0.8` `85ms` `1468%` |
| `0.9` | `0.9` `1258ms` | `0.9` `83ms` `1516%` | `0.9` `75ms` `1677%` | `0.9` `86ms` `1463%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.11` | `0.11` `1304ms` | `0.11` `80ms` `1630%` | `0.11` `91ms` `1433%` | `0.11` `90ms` `1449%` |
| `0.101` | `0.101` `1358ms` | `0.101` `82ms` `1656%` | `0.101` `103ms` `1318%` | `0.101` `94ms` `1445%` |
| `0.1001` | `0.1001` `1347ms` | `0.1001` `86ms` `1566%` | `0.1001` `118ms` `1142%` | `0.1001` `98ms` `1374%` |
| `0.10001` | `0.10001` `1326ms` | `0.10001` `95ms` `1396%` | `0.10001` `112ms` `1184%` | `0.10001` `102ms` `1300%` |
| `0.100001` | `0.100001` `1330ms` | `0.100001` `99ms` `1343%` | `0.100001` `120ms` `1108%` | `0.100001` `102ms` `1304%` |
| `0.1000001` | `0.1000001` `1382ms` | `0.1000001` `89ms` `1553%` | `0.1000001` `126ms` `1097%` | `0.1000001` `111ms` `1245%` |
| `0.10000001` | `0.10000001` `1296ms` | `0.10000001` `93ms` `1394%` | `0.10000001` `123ms` `1054%` | `0.10000001` `101ms` `1283%` |
| `0.100000001` | `0.100000001` `1367ms` | `0.100000001` `94ms` `1454%` | `0.100000001` `135ms` `1013%` | `0.100000001` `108ms` `1266%` |
| `0.1000000001` | `0.1000000001` `1359ms` | `0.1000000001` `102ms` `1332%` | `0.1000000001` `143ms` `950%` | `0.1000000001` `105ms` `1294%` |
| `0.10000000001` | `0.10000000001` `1379ms` | `0.10000000001` `107ms` `1289%` | `0.10000000001` `157ms` `878%` | `0.10000000001` `103ms` `1339%` |
| `0.100000000001` | `0.100000000001` `1373ms` | `0.100000000001` `103ms` `1333%` | `0.100000000001` `160ms` `858%` | `0.100000000001` `105ms` `1308%` |
| `0.1000000000001` | `0.1000000000001` `1375ms` | `0.1000000000001` `92ms` `1495%` | `0.1000000000001` `180ms` `764%` | `0.1000000000001` `104ms` `1322%` |
| `0.10000000000001` | `0.10000000000001` `1362ms` | `0.10000000000001` `101ms` `1349%` | `0.10000000000001` `184ms` `740%` | `0.10000000000001` `105ms` `1297%` |
| `0.100000000000001` | `0.100000000000001` `1348ms` | `0.100000000000001` `91ms` `1481%` | `0.100000000000001` `186ms` `725%` | `0.100000000000001` `108ms` `1248%` |
| `0.1000000000000001` | `0.1000000000000001` `1307ms` | `0.1` `78ms` `1676%` | `0.1000000000000001` `193ms` `677%` | `0.1000000000000001` `101ms` `1294%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1.1` | `1.1` `1099ms` | `1.1` `79ms` `1391%` | `1.1` `92ms` `1195%` | `1.1` `99ms` `1110%` |
| `2.2` | `2.2` `1098ms` | `2.2` `79ms` `1390%` | `2.2` `89ms` `1234%` | `2.2` `90ms` `1220%` |
| `3.3` | `3.3` `1129ms` | `3.3` `81ms` `1394%` | `3.3` `88ms` `1283%` | `3.3` `91ms` `1241%` |
| `4.4` | `4.4` `1117ms` | `4.4` `78ms` `1432%` | `4.4` `91ms` `1227%` | `4.4` `98ms` `1140%` |
| `5.5` | `5.5` `1092ms` | `5.5` `82ms` `1332%` | `5.5` `90ms` `1213%` | `5.5` `96ms` `1138%` |
| `6.6` | `6.6` `1169ms` | `6.6` `81ms` `1443%` | `6.6` `95ms` `1231%` | `6.6` `99ms` `1181%` |
| `7.7` | `7.7` `1116ms` | `7.7` `86ms` `1298%` | `7.7` `87ms` `1283%` | `7.7` `98ms` `1139%` |
| `8.8` | `8.800000000000001` `1227ms` | `8.8` `82ms` `1496%` | `8.8` `88ms` `1394%` | `8.8` `94ms` `1305%` |
| `9.9` | `9.9` `1224ms` | `9.9` `81ms` `1511%` | `9.9` `88ms` `1391%` | `9.9` `98ms` `1249%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1344ms` | `0.1` `80ms` `1680%` | `0.1` `78ms` `1723%` | `0.1` `94ms` `1430%` |
| `0.12` | `0.12` `1374ms` | `0.12` `82ms` `1676%` | `0.12` `88ms` `1561%` | `0.12` `89ms` `1544%` |
| `0.123` | `0.123` `1388ms` | `0.123` `88ms` `1577%` | `0.123` `108ms` `1285%` | `0.123` `95ms` `1461%` |
| `0.1234` | `0.1234` `1394ms` | `0.1234` `89ms` `1566%` | `0.1234` `120ms` `1162%` | `0.1234` `98ms` `1422%` |
| `0.12345` | `0.12345` `1342ms` | `0.12345` `108ms` `1243%` | `0.12345` `127ms` `1057%` | `0.12345` `122ms` `1100%` |
| `0.123456` | `0.123456` `1427ms` | `0.123456` `97ms` `1471%` | `0.123456` `118ms` `1209%` | `0.123456` `106ms` `1346%` |
| `0.1234567` | `0.1234567` `1359ms` | `0.1234567` `97ms` `1401%` | `0.1234567` `128ms` `1062%` | `0.1234567` `107ms` `1270%` |
| `0.12345678` | `0.12345678` `1400ms` | `0.12345678` `100ms` `1400%` | `0.12345678` `133ms` `1053%` | `0.12345678` `108ms` `1296%` |
| `0.123456789` | `0.123456789` `1408ms` | `0.123456789` `103ms` `1367%` | `0.123456789` `138ms` `1020%` | `0.123456789` `119ms` `1183%` |
| `0.12345678901` | `0.12345678901` `1422ms` | `0.12345678901` `110ms` `1293%` | `0.12345678901` `162ms` `878%` | `0.12345678901` `116ms` `1226%` |
| `0.123456789012` | `0.123456789012` `1415ms` | `0.123456789012` `114ms` `1241%` | `0.123456789012` `167ms` `847%` | `0.123456789012` `118ms` `1199%` |
| `0.1234567890123` | `0.1234567890123` `1403ms` | `0.1234567890123` `118ms` `1189%` | `0.1234567890123` `174ms` `806%` | `0.1234567890123` `125ms` `1122%` |
| `0.12345678901234` | `0.12345678901234` `1389ms` | `0.12345678901234` `113ms` `1229%` | `0.12345678901234` `189ms` `735%` | `0.12345678901234` `127ms` `1094%` |
| `0.123456789012345` | `0.123456789012345` `1378ms` | `0.123456789012345` `126ms` `1094%` | `0.123456789012345` `193ms` `714%` | `0.123456789012345` `121ms` `1139%` |
| `0.1234567890123456` | `0.1234567890123456` `1383ms` | `0.1234567890123456` `117ms` `1182%` | `0.1234567890123456` `203ms` `681%` | `0.1234567890123456` `127ms` `1089%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1e100` | `1e+99` `3280ms` | `1e+99` `71ms` `4620%` | `1e+99` `71ms` `4620%` | `1e+99` `79ms` `4152%` |
| `0.12e100` | `1.2e+99` `2673ms` | `1.2e+99` `72ms` `3712%` | `1.2e+99` `92ms` `2905%` | `1.2e+99` `91ms` `2937%` |
| `0.123e100` | `1.23e+99` `3369ms` | `1.23e+99` `78ms` `4319%` | `1.23e+99` `110ms` `3063%` | `1.23e+99` `110ms` `3063%` |
| `0.1234e100` | `1.234e+99` `3380ms` | `1.234e+99` `77ms` `4390%` | `1.234e+99` `122ms` `2770%` | `1.234e+99` `99ms` `3414%` |
| `0.12345e100` | `1.2345e+99` `3377ms` | `1.2345e+99` `79ms` `4275%` | `1.2345e+99` `130ms` `2598%` | `1.2345e+99` `123ms` `2746%` |
| `0.123456e100` | `1.23456e+99` `2882ms` | `1.23456e+99` `84ms` `3431%` | `1.23456e+99` `153ms` `1884%` | `1.23456e+99` `105ms` `2745%` |
| `0.1234567e100` | `1.234567e+99` `3039ms` | `1.234567e+99` `80ms` `3799%` | `1.234567e+99` `165ms` `1842%` | `1.234567e+99` `110ms` `2763%` |
| `0.12345678e100` | `1.2345678e+99` `3408ms` | `1.2345678e+99` `85ms` `4009%` | `1.2345678e+99` `180ms` `1893%` | `1.2345678e+99` `108ms` `3156%` |
| `0.123456789e100` | `1.23456789e+99` `3139ms` | `1.23456789e+99` `95ms` `3304%` | `1.23456789e+99` `168ms` `1868%` | `1.23456789e+99` `115ms` `2730%` |
| `0.12345678901e100` | `1.2345678901e+99` `3395ms` | `1.2345678901e+99` `96ms` `3536%` | `1.2345678901e+99` `188ms` `1806%` | `1.2345678901e+99` `132ms` `2572%` |
| `0.123456789012e100` | `1.23456789012e+99` `3425ms` | `1.23456789012e+99` `98ms` `3495%` | `1.23456789012e+99` `195ms` `1756%` | `1.23456789012e+99` `125ms` `2740%` |
| `0.1234567890123e100` | `1.234567890123e+99` `3448ms` | `1.234567890123e+99` `108ms` `3193%` | `1.234567890123e+99` `221ms` `1560%` | `1.234567890123e+99` `130ms` `2652%` |
| `0.12345678901234e100` | `1.2345678901234e+99` `3517ms` | `1.2345678901234e+99` `115ms` `3058%` | `1.2345678901234e+99` `210ms` `1675%` | `1.2345678901234e+99` `137ms` `2567%` |
| `0.123456789012345e100` | `1.23456789012345e+99` `3511ms` | `1.23456789012345e+99` `109ms` `3221%` | `1.23456789012345e+99` `211ms` `1664%` | `1.23456789012345e+99` `134ms` `2620%` |
| `0.1234567890123456e100` | `1.234567890123456e+99` `3510ms` | `1.234567890123456e+99` `111ms` `3162%` | `1.234567890123456e+99` `222ms` `1581%` | `1.234567890123456e+99` `130ms` `2700%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1e-100` | `1e-101` `1929ms` | `1e-101` `74ms` `2607%` | `1e-101` `80ms` `2411%` | `1e-101` `81ms` `2381%` |
| `0.12e-100` | `1.2e-101` `2059ms` | `1.2e-101` `76ms` `2709%` | `1.2e-101` `102ms` `2019%` | `1.2e-101` `96ms` `2145%` |
| `0.123e-100` | `1.23e-101` `1990ms` | `1.23e-101` `84ms` `2369%` | `1.23e-101` `114ms` `1746%` | `1.23e-101` `111ms` `1793%` |
| `0.1234e-100` | `1.234e-101` `2028ms` | `1.234e-101` `83ms` `2443%` | `1.234e-101` `126ms` `1610%` | `1.234e-101` `107ms` `1895%` |
| `0.12345e-100` | `1.2345e-101` `2017ms` | `1.2345e-101` `88ms` `2292%` | `1.2345e-101` `140ms` `1441%` | `1.2345e-101` `114ms` `1769%` |
| `0.123456e-100` | `1.23456e-101` `2019ms` | `1.23456e-101` `90ms` `2243%` | `1.23456e-101` `159ms` `1270%` | `1.23456e-101` `116ms` `1741%` |
| `0.1234567e-100` | `1.234567e-101` `2024ms` | `1.234567e-101` `95ms` `2131%` | `1.234567e-101` `181ms` `1118%` | `1.234567e-101` `122ms` `1659%` |
| `0.12345678e-100` | `1.2345678e-101` `2077ms` | `1.2345678e-101` `90ms` `2308%` | `1.2345678e-101` `183ms` `1135%` | `1.2345678e-101` `112ms` `1854%` |
| `0.123456789e-100` | `1.23456789e-101` `2030ms` | `1.23456789e-101` `97ms` `2093%` | `1.23456789e-101` `174ms` `1167%` | `1.23456789e-101` `120ms` `1692%` |
| `0.12345678901e-100` | `1.2345678901e-101` `2055ms` | `1.2345678901e-101` `103ms` `1995%` | `1.2345678901e-101` `198ms` `1038%` | `1.2345678901e-101` `122ms` `1684%` |
| `0.123456789012e-100` | `1.23456789012e-101` `2046ms` | `1.23456789012e-101` `109ms` `1877%` | `1.23456789012e-101` `198ms` `1033%` | `1.23456789012e-101` `123ms` `1663%` |
| `0.1234567890123e-100` | `1.234567890123e-101` `2073ms` | `1.234567890123e-101` `113ms` `1835%` | `1.234567890123e-101` `206ms` `1006%` | `1.234567890123e-101` `128ms` `1620%` |
| `0.12345678901234e-100` | `1.2345678901234e-101` `2053ms` | `1.2345678901234e-101` `111ms` `1850%` | `1.2345678901234e-101` `219ms` `937%` | `1.2345678901234e-101` `128ms` `1604%` |
| `0.123456789012345e-100` | `1.23456789012345e-101` `2037ms` | `1.23456789012345e-101` `107ms` `1904%` | `1.23456789012345e-101` `222ms` `918%` | `1.23456789012345e-101` `133ms` `1532%` |
| `0.1234567890123456e-100` | `1.234567890123456e-101` `1992ms` | `1.234567890123456e-101` `109ms` `1828%` | `1.234567890123456e-101` `226ms` `881%` | `1.234567890123456e-101` `131ms` `1521%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1e1` | `10` `849ms` | `10.0` `26ms` `3265%` | `10.0` `67ms` `1267%` | `10.0` `31ms` `2739%` |
| `1e2` | `100` `1010ms` | `100.0` `30ms` `3367%` | `100.0` `72ms` `1403%` | `100.0` `32ms` `3156%` |
| `1e3` | `1000` `1174ms` | `1000.0` `30ms` `3913%` | `1000.0` `76ms` `1545%` | `1000.0` `41ms` `2863%` |
| `1e4` | `10000` `1188ms` | `10000.0` `30ms` `3960%` | `10000.0` `77ms` `1543%` | `10000.0` `35ms` `3394%` |
| `1e5` | `100000` `1325ms` | `100000.0` `28ms` `4732%` | `100000.0` `79ms` `1677%` | `100000.0` `33ms` `4015%` |
| `1e6` | `1000000` `1303ms` | `1000000.0` `34ms` `3832%` | `1000000.0` `70ms` `1861%` | `1000000.0` `38ms` `3429%` |
| `1e7` | `10000000` `1341ms` | `10000000.0` `35ms` `3831%` | `10000000.0` `69ms` `1943%` | `10000000.0` `41ms` `3271%` |
| `1e8` | `100000000` `1433ms` | `100000000.0` `32ms` `4478%` | `100000000.0` `228ms` `629%` | `100000000.0` `35ms` `4094%` |
| `1e9` | `1000000000` `1512ms` | `1000000000.0` `37ms` `4086%` | `1000000000.0` `235ms` `643%` | `1000000000.0` `43ms` `3516%` |
| `1e10` | `10000000000` `1554ms` | `10000000000.0` `37ms` `4200%` | `10000000000.0` `227ms` `685%` | `10000000000.0` `41ms` `3790%` |
| `1e11` | `100000000000` `1651ms` | `100000000000.0` `36ms` `4586%` | `100000000000.0` `235ms` `703%` | `100000000000.0` `43ms` `3840%` |
| `1e12` | `1000000000000` `1700ms` | `1000000000000.0` `33ms` `5152%` | `1000000000000.0` `240ms` `708%` | `1000000000000.0` `39ms` `4359%` |
| `1e13` | `10000000000000` `1762ms` | `10000000000000.0` `36ms` `4894%` | `10000000000000.0` `249ms` `708%` | `10000000000000.0` `42ms` `4195%` |
| `1e14` | `100000000000000` `1843ms` | `100000000000000.0` `39ms` `4726%` | `100000000000000.0` `237ms` `778%` | `100000000000000.0` `44ms` `4189%` |
| `1e15` | `1000000000000000` `1043ms` | `1000000000000000.0` `38ms` `2745%` | `1000000000000000.0` `165ms` `632%` | `1000000000000000.0` `48ms` `2173%` |
| `1e16` | `1e+16` `1074ms` | `10000000000000000.0` `90ms` `1193%` | `10000000000000000.0` `163ms` `659%` | `10000000000000000.0` `89ms` `1207%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1e-1` | `0.1` `1332ms` | `0.1` `85ms` `1567%` | `0.1` `83ms` `1605%` | `0.1` `93ms` `1432%` |
| `1e-2` | `0.01` `1404ms` | `0.01` `91ms` `1543%` | `0.01` `82ms` `1712%` | `0.01` `103ms` `1363%` |
| `1e-3` | `0.001` `1511ms` | `0.001` `86ms` `1757%` | `0.001` `97ms` `1558%` | `0.001` `99ms` `1526%` |
| `1e-4` | `0.0001` `1402ms` | `0.0001` `86ms` `1630%` | `0.0001` `94ms` `1491%` | `0.0001` `94ms` `1491%` |
| `1e-5` | `1e-05` `1404ms` | `0.00001` `94ms` `1494%` | `0.00001` `95ms` `1478%` | `0.00001` `94ms` `1494%` |
| `1e-6` | `1e-06` `1634ms` | `0.000001` `81ms` `2017%` | `0.000001` `92ms` `1776%` | `0.000001` `98ms` `1667%` |
| `1e-7` | `1e-07` `1460ms` | `0.0000001` `88ms` `1659%` | `1e-7` `65ms` `2246%` | `0.0000001` `97ms` `1505%` |
| `1e-8` | `1e-08` `1359ms` | `1e-8` `74ms` `1836%` | `1e-8` `69ms` `1970%` | `1e-8` `84ms` `1618%` |
| `1e-9` | `1e-09` `1480ms` | `1e-9` `72ms` `2056%` | `1e-9` `66ms` `2242%` | `1e-9` `83ms` `1783%` |
| `1e-10` | `1e-10` `1444ms` | `1e-10` `75ms` `1925%` | `1e-10` `74ms` `1951%` | `1e-10` `83ms` `1740%` |
| `1e-11` | `9.999999999999999e-12` `1574ms` | `1e-11` `76ms` `2071%` | `1e-11` `86ms` `1830%` | `1e-11` `84ms` `1874%` |
| `1e-12` | `1e-12` `1638ms` | `1e-12` `75ms` `2184%` | `1e-12` `88ms` `1861%` | `1e-12` `87ms` `1883%` |
| `1e-13` | `1e-13` `1576ms` | `1e-13` `78ms` `2021%` | `1e-13` `81ms` `1946%` | `1e-13` `83ms` `1899%` |
| `1e-14` | `1e-14` `1652ms` | `1e-14` `73ms` `2263%` | `1e-14` `73ms` `2263%` | `1e-14` `82ms` `2015%` |
| `1e-15` | `1e-15` `1618ms` | `1e-15` `75ms` `2157%` | `1e-15` `77ms` `2101%` | `1e-15` `88ms` `1839%` |
| `1e-16` | `1e-16` `1533ms` | `1e-16` `77ms` `1991%` | `1e-16` `72ms` `2129%` | `1e-16` `84ms` `1825%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.9` | `0.9` `1307ms` | `0.9` `86ms` `1520%` | `0.9` `91ms` `1436%` | `0.9` `95ms` `1376%` |
| `0.99` | `0.99` `1389ms` | `0.99` `84ms` `1654%` | `0.99` `98ms` `1417%` | `0.99` `102ms` `1362%` |
| `0.999` | `0.999` `1427ms` | `0.999` `88ms` `1622%` | `0.999` `112ms` `1274%` | `0.999` `107ms` `1334%` |
| `0.9999` | `0.9999` `1333ms` | `0.9999` `94ms` `1418%` | `0.9999` `127ms` `1050%` | `0.9999` `109ms` `1223%` |
| `0.99999` | `0.99999` `1341ms` | `0.99999` `97ms` `1382%` | `0.99999` `114ms` `1176%` | `0.99999` `111ms` `1208%` |
| `0.999999` | `0.999999` `1375ms` | `0.999999` `98ms` `1403%` | `0.999999` `122ms` `1127%` | `0.999999` `106ms` `1297%` |
| `0.9999999` | `0.9999999000000001` `1328ms` | `0.9999999` `100ms` `1328%` | `0.9999999` `129ms` `1029%` | `0.9999999` `109ms` `1218%` |
| `0.99999999` | `0.9999999899999999` `1363ms` | `0.99999999` `102ms` `1336%` | `0.99999999` `131ms` `1040%` | `0.99999999` `117ms` `1165%` |
| `0.999999999` | `0.999999999` `1285ms` | `0.999999999` `111ms` `1158%` | `0.999999999` `141ms` `911%` | `0.999999999` `119ms` `1080%` |
| `0.9999999999` | `0.9999999999` `1400ms` | `0.9999999999` `107ms` `1308%` | `0.9999999999` `158ms` `886%` | `0.9999999999` `122ms` `1148%` |
| `0.99999999999` | `0.99999999999` `1397ms` | `0.99999999999` `114ms` `1225%` | `0.99999999999` `169ms` `827%` | `0.99999999999` `119ms` `1174%` |
| `0.999999999999` | `0.999999999999` `1437ms` | `0.999999999999` `123ms` `1168%` | `0.999999999999` `197ms` `729%` | `0.999999999999` `137ms` `1049%` |
| `0.9999999999999` | `0.9999999999999` `1536ms` | `0.9999999999999` `138ms` `1113%` | `0.9999999999999` `212ms` `725%` | `0.9999999999999` `146ms` `1052%` |
| `0.99999999999999` | `0.99999999999999` `1569ms` | `0.99999999999999` `124ms` `1265%` | `0.99999999999999` `215ms` `730%` | `0.99999999999999` `134ms` `1171%` |
| `0.999999999999999` | `0.999999999999999` `1497ms` | `0.999999999999999` `141ms` `1062%` | `0.999999999999999` `214ms` `700%` | `0.999999999999999` `150ms` `998%` |
| `0.9999999999999999` | `0.9999999999999999` `1443ms` | `1.0` `89ms` `1621%` | `0.9999999999999999` `222ms` `650%` | `0.9999999999999999` `156ms` `925%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1522ms` | `0.1` `88ms` `1730%` | `0.1` `80ms` `1902%` | `0.1` `97ms` `1569%` |
| `0.11` | `0.11` `1468ms` | `0.11` `86ms` `1707%` | `0.11` `97ms` `1513%` | `0.11` `97ms` `1513%` |
| `0.111` | `0.111` `1498ms` | `0.111` `96ms` `1560%` | `0.111` `112ms` `1338%` | `0.111` `111ms` `1350%` |
| `0.1111` | `0.1111` `1513ms` | `0.1111` `102ms` `1483%` | `0.1111` `130ms` `1164%` | `0.1111` `109ms` `1388%` |
| `0.11111` | `0.11111` `1516ms` | `0.11111` `117ms` `1296%` | `0.11111` `131ms` `1157%` | `0.11111` `110ms` `1378%` |
| `0.111111` | `0.111111` `1521ms` | `0.111111` `114ms` `1334%` | `0.111111` `141ms` `1079%` | `0.111111` `123ms` `1237%` |
| `0.1111111` | `0.1111111` `1562ms` | `0.1111111` `115ms` `1358%` | `0.1111111` `143ms` `1092%` | `0.1111111` `123ms` `1270%` |
| `0.11111111` | `0.11111111` `1581ms` | `0.11111111` `125ms` `1265%` | `0.11111111` `152ms` `1040%` | `0.11111111` `118ms` `1340%` |
| `0.111111111` | `0.111111111` `1583ms` | `0.111111111` `116ms` `1365%` | `0.111111111` `150ms` `1055%` | `0.111111111` `131ms` `1208%` |
| `0.1111111111` | `0.1111111111` `1517ms` | `0.1111111111` `123ms` `1233%` | `0.1111111111` `167ms` `908%` | `0.1111111111` `128ms` `1185%` |
| `0.11111111111` | `0.11111111111` `1569ms` | `0.11111111111` `113ms` `1388%` | `0.11111111111` `171ms` `918%` | `0.11111111111` `129ms` `1216%` |
| `0.111111111111` | `0.111111111111` `1548ms` | `0.111111111111` `119ms` `1301%` | `0.111111111111` `187ms` `828%` | `0.111111111111` `131ms` `1182%` |
| `0.1111111111111` | `0.1111111111111` `1472ms` | `0.1111111111111` `128ms` `1150%` | `0.1111111111111` `190ms` `775%` | `0.1111111111111` `131ms` `1124%` |
| `0.11111111111111` | `0.11111111111111` `1460ms` | `0.11111111111111` `124ms` `1177%` | `0.11111111111111` `195ms` `749%` | `0.11111111111111` `142ms` `1028%` |
| `0.111111111111111` | `0.111111111111111` `1449ms` | `0.111111111111111` `123ms` `1178%` | `0.111111111111111` `202ms` `717%` | `0.111111111111111` `134ms` `1081%` |
| `0.1111111111111111` | `0.1111111111111111` `1487ms` | `0.111111111111111` `131ms` `1135%` | `0.1111111111111111` `211ms` `705%` | `0.1111111111111111` `136ms` `1093%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1e-306` | `1e-306` `2540ms` | `1e-306` `82ms` `3098%` | `1e-306` `89ms` `2854%` | `1e-306` `89ms` `2854%` |
| `1e-307` | `9.999999999999999e-308` `2587ms` | `1e-307` `77ms` `3360%` | `1e-307` `91ms` `2843%` | `1e-307` `92ms` `2812%` |
| `1e-308` | `9.999999999999999e-309` `2674ms` | `1e-308` `81ms` `3301%` | `1e-308` `129ms` `2073%` | `1e-308` `66ms` `4052%` |
| `1e-309` | `1.000000000000002e-309` `2706ms` | `1e-309` `75ms` `3608%` | `1e-309` `97ms` `2790%` | `1e-309` `62ms` `4365%` |
| `1e-311` | `9.999999999999475e-312` `2589ms` | `1e-311` `72ms` `3596%` | `1e-311` `102ms` `2538%` | `1e-311` `55ms` `4707%` |
| `1e-312` | `9.999999999984653e-313` `2679ms` | `1e-312` `70ms` `3827%` | `1e-312` `100ms` `2679%` | `1e-312` `62ms` `4321%` |
| `1e-313` | `1.000000000013287e-313` `2769ms` | `1e-313` `66ms` `4195%` | `1e-313` `97ms` `2855%` | `1e-313` `59ms` `4693%` |
| `1e-314` | `9.999999999638807e-315` `2805ms` | `1e-314` `69ms` `4065%` | `1e-314` `104ms` `2697%` | `1e-314` `50ms` `5610%` |
| `1e-315` | `9.999999984816838e-316` `2700ms` | `1e-315` `57ms` `4737%` | `1e-315` `143ms` `1888%` | `1e-315` `46ms` `5870%` |
| `1e-316` | `9.999999836597144e-317` `2777ms` | `1e-316` `49ms` `5667%` | `1e-316` `134ms` `2072%` | `1e-316` `56ms` `4959%` |
| `1e-317` | `1.000000230692537e-317` `2923ms` | `1e-317` `56ms` `5220%` | `1e-317` `101ms` `2894%` | `1e-317` `48ms` `6090%` |
| `1e-318` | `9.999987484955998e-319` `3074ms` | `1e-318` `52ms` `5912%` | `1e-318` `105ms` `2928%` | `1e-318` `53ms` `5800%` |
| `1e-319` | `9.99988867182683e-320` `2263ms` | `1e-319` `56ms` `4041%` | `1e-319` `101ms` `2241%` | `1e-319` `45ms` `5029%` |
| `1e-320` | `9.99988867182683e-321` `2368ms` | `1e-320` `52ms` `4554%` | `1e-320` `101ms` `2345%` | `1e-320` `52ms` `4554%` |
| `1e-321` | `9.98012604599318e-322` `2473ms` | `1e-321` `53ms` `4666%` | `1e-321` `100ms` `2473%` | `1e-321` `45ms` `5496%` |
| `1e-322` | `9.881312916824931e-323` `2592ms` | `1e-322` `58ms` `4469%` | `1e-322` `95ms` `2728%` | `1e-322` `48ms` `5400%` |
| `1e-323` | `9.881312916824931e-324` `2492ms` | `1e-323` `46ms` `5417%` | `1e-323` `130ms` `1917%` | `1e-323` `42ms` `5933%` |

## APIs

Refer to `json.h` .

## Contact

* Phone: +86 18368887550
* wx/qq: 1083936981
* Email: lengjingzju@163.com 3090101217@zju.edu.cn
