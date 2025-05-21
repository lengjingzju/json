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
| `0.1` | `0.1` `1303ms` | `0.1` `85ms` `1533%` | `0.1` `72ms` `1810%` | `0.1` `88ms` `1481%` |
| `0.2` | `0.2` `1240ms` | `0.2` `81ms` `1531%` | `0.2` `75ms` `1653%` | `0.2` `88ms` `1409%` |
| `0.3` | `0.3` `1314ms` | `0.3` `81ms` `1622%` | `0.3` `75ms` `1752%` | `0.3` `88ms` `1493%` |
| `0.4` | `0.4` `1237ms` | `0.4` `81ms` `1527%` | `0.4` `76ms` `1628%` | `0.4` `97ms` `1275%` |
| `0.5` | `0.5` `1298ms` | `0.5` `83ms` `1564%` | `0.5` `74ms` `1754%` | `0.5` `86ms` `1509%` |
| `0.6` | `0.6` `1345ms` | `0.6` `82ms` `1640%` | `0.6` `77ms` `1747%` | `0.6` `83ms` `1620%` |
| `0.7` | `0.7` `1344ms` | `0.7` `81ms` `1659%` | `0.7` `77ms` `1745%` | `0.7` `85ms` `1581%` |
| `0.8` | `0.8` `1266ms` | `0.8` `84ms` `1507%` | `0.8` `78ms` `1623%` | `0.8` `83ms` `1525%` |
| `0.9` | `0.9` `1287ms` | `0.9` `83ms` `1551%` | `0.9` `77ms` `1671%` | `0.9` `83ms` `1551%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.11` | `0.11` `1372ms` | `0.11` `81ms` `1694%` | `0.11` `90ms` `1524%` | `0.11` `91ms` `1508%` |
| `0.101` | `0.101` `1362ms` | `0.101` `93ms` `1465%` | `0.101` `107ms` `1273%` | `0.101` `96ms` `1419%` |
| `0.1001` | `0.1001` `1407ms` | `0.1001` `93ms` `1513%` | `0.1001` `123ms` `1144%` | `0.1001` `95ms` `1481%` |
| `0.10001` | `0.10001` `1341ms` | `0.10001` `95ms` `1412%` | `0.10001` `113ms` `1187%` | `0.10001` `103ms` `1302%` |
| `0.100001` | `0.100001` `1382ms` | `0.100001` `94ms` `1470%` | `0.100001` `118ms` `1171%` | `0.100001` `98ms` `1410%` |
| `0.1000001` | `0.1000001` `1408ms` | `0.1000001` `101ms` `1394%` | `0.1000001` `127ms` `1109%` | `0.1000001` `100ms` `1408%` |
| `0.10000001` | `0.10000001` `1389ms` | `0.10000001` `101ms` `1375%` | `0.10000001` `137ms` `1014%` | `0.10000001` `110ms` `1263%` |
| `0.100000001` | `0.100000001` `1388ms` | `0.100000001` `99ms` `1402%` | `0.100000001` `139ms` `999%` | `0.100000001` `105ms` `1322%` |
| `0.1000000001` | `0.1000000001` `1431ms` | `0.1000000001` `101ms` `1417%` | `0.1000000001` `149ms` `960%` | `0.1000000001` `107ms` `1337%` |
| `0.10000000001` | `0.10000000001` `1406ms` | `0.10000000001` `100ms` `1406%` | `0.10000000001` `155ms` `907%` | `0.10000000001` `107ms` `1314%` |
| `0.100000000001` | `0.100000000001` `1448ms` | `0.100000000001` `106ms` `1366%` | `0.100000000001` `164ms` `883%` | `0.100000000001` `111ms` `1305%` |
| `0.1000000000001` | `0.1000000000001` `1402ms` | `0.1000000000001` `97ms` `1445%` | `0.1000000000001` `176ms` `797%` | `0.1000000000001` `101ms` `1388%` |
| `0.10000000000001` | `0.10000000000001` `1418ms` | `0.10000000000001` `97ms` `1462%` | `0.10000000000001` `186ms` `762%` | `0.10000000000001` `98ms` `1447%` |
| `0.100000000000001` | `0.100000000000001` `1416ms` | `0.100000000000001` `97ms` `1460%` | `0.100000000000001` `192ms` `738%` | `0.100000000000001` `107ms` `1323%` |
| `0.1000000000000001` | `0.1000000000000001` `1417ms` | `0.1` `87ms` `1629%` | `0.1000000000000001` `199ms` `712%` | `0.1000000000000001` `108ms` `1312%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1.1` | `1.1` `1128ms` | `1.1` `81ms` `1393%` | `1.1` `92ms` `1226%` | `1.1` `89ms` `1267%` |
| `2.2` | `2.2` `1120ms` | `2.2` `85ms` `1318%` | `2.2` `92ms` `1217%` | `2.2` `96ms` `1167%` |
| `3.3` | `3.3` `1167ms` | `3.3` `86ms` `1357%` | `3.3` `93ms` `1255%` | `3.3` `96ms` `1216%` |
| `4.4` | `4.4` `1109ms` | `4.4` `83ms` `1336%` | `4.4` `89ms` `1246%` | `4.4` `93ms` `1192%` |
| `5.5` | `5.5` `1147ms` | `5.5` `88ms` `1303%` | `5.5` `93ms` `1233%` | `5.5` `93ms` `1233%` |
| `6.6` | `6.6` `1201ms` | `6.6` `85ms` `1413%` | `6.6` `91ms` `1320%` | `6.6` `89ms` `1349%` |
| `7.7` | `7.7` `1097ms` | `7.7` `83ms` `1322%` | `7.7` `92ms` `1192%` | `7.7` `94ms` `1167%` |
| `8.8` | `8.800000000000001` `1312ms` | `8.8` `85ms` `1544%` | `8.8` `93ms` `1411%` | `8.8` `92ms` `1426%` |
| `9.9` | `9.9` `1214ms` | `9.9` `87ms` `1395%` | `9.9` `98ms` `1239%` | `9.9` `94ms` `1291%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1390ms` | `0.1` `85ms` `1635%` | `0.1` `80ms` `1738%` | `0.1` `93ms` `1495%` |
| `0.12` | `0.12` `1440ms` | `0.12` `84ms` `1714%` | `0.12` `89ms` `1618%` | `0.12` `95ms` `1516%` |
| `0.123` | `0.123` `1446ms` | `0.123` `89ms` `1625%` | `0.123` `114ms` `1268%` | `0.123` `97ms` `1491%` |
| `0.1234` | `0.1234` `1441ms` | `0.1234` `92ms` `1566%` | `0.1234` `113ms` `1275%` | `0.1234` `97ms` `1486%` |
| `0.12345` | `0.12345` `1382ms` | `0.12345` `97ms` `1425%` | `0.12345` `110ms` `1256%` | `0.12345` `103ms` `1342%` |
| `0.123456` | `0.123456` `1412ms` | `0.123456` `97ms` `1456%` | `0.123456` `119ms` `1187%` | `0.123456` `108ms` `1307%` |
| `0.1234567` | `0.1234567` `1399ms` | `0.1234567` `100ms` `1399%` | `0.1234567` `131ms` `1068%` | `0.1234567` `107ms` `1307%` |
| `0.12345678` | `0.12345678` `1439ms` | `0.12345678` `102ms` `1411%` | `0.12345678` `132ms` `1090%` | `0.12345678` `109ms` `1320%` |
| `0.123456789` | `0.123456789` `1463ms` | `0.123456789` `108ms` `1355%` | `0.123456789` `143ms` `1023%` | `0.123456789` `114ms` `1283%` |
| `0.12345678901` | `0.12345678901` `1479ms` | `0.12345678901` `104ms` `1422%` | `0.12345678901` `169ms` `875%` | `0.12345678901` `138ms` `1072%` |
| `0.123456789012` | `0.123456789012` `1492ms` | `0.123456789012` `107ms` `1394%` | `0.123456789012` `173ms` `862%` | `0.123456789012` `121ms` `1233%` |
| `0.1234567890123` | `0.1234567890123` `1431ms` | `0.1234567890123` `117ms` `1223%` | `0.1234567890123` `176ms` `813%` | `0.1234567890123` `126ms` `1136%` |
| `0.12345678901234` | `0.12345678901234` `1442ms` | `0.12345678901234` `112ms` `1288%` | `0.12345678901234` `191ms` `755%` | `0.12345678901234` `131ms` `1101%` |
| `0.123456789012345` | `0.123456789012345` `1437ms` | `0.123456789012345` `114ms` `1261%` | `0.123456789012345` `191ms` `752%` | `0.123456789012345` `125ms` `1150%` |
| `0.1234567890123456` | `0.1234567890123456` `1441ms` | `0.1234567890123456` `124ms` `1162%` | `0.1234567890123456` `205ms` `703%` | `0.1234567890123456` `134ms` `1075%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1e100` | `1e+99` `3477ms` | `1e+99` `73ms` `4763%` | `1e+99` `71ms` `4897%` | `1e+99` `86ms` `4043%` |
| `0.12e100` | `1.2e+99` `2802ms` | `1.2e+99` `73ms` `3838%` | `1.2e+99` `92ms` `3046%` | `1.2e+99` `99ms` `2830%` |
| `0.123e100` | `1.23e+99` `3548ms` | `1.23e+99` `81ms` `4380%` | `1.23e+99` `112ms` `3168%` | `1.23e+99` `106ms` `3347%` |
| `0.1234e100` | `1.234e+99` `3508ms` | `1.234e+99` `83ms` `4227%` | `1.234e+99` `122ms` `2875%` | `1.234e+99` `109ms` `3218%` |
| `0.12345e100` | `1.2345e+99` `3539ms` | `1.2345e+99` `85ms` `4164%` | `1.2345e+99` `128ms` `2765%` | `1.2345e+99` `109ms` `3247%` |
| `0.123456e100` | `1.23456e+99` `3012ms` | `1.23456e+99` `85ms` `3544%` | `1.23456e+99` `140ms` `2151%` | `1.23456e+99` `109ms` `2763%` |
| `0.1234567e100` | `1.234567e+99` `3075ms` | `1.234567e+99` `87ms` `3534%` | `1.234567e+99` `167ms` `1841%` | `1.234567e+99` `117ms` `2628%` |
| `0.12345678e100` | `1.2345678e+99` `3568ms` | `1.2345678e+99` `84ms` `4248%` | `1.2345678e+99` `166ms` `2149%` | `1.2345678e+99` `111ms` `3214%` |
| `0.123456789e100` | `1.23456789e+99` `3316ms` | `1.23456789e+99` `102ms` `3251%` | `1.23456789e+99` `171ms` `1939%` | `1.23456789e+99` `128ms` `2591%` |
| `0.12345678901e100` | `1.2345678901e+99` `3520ms` | `1.2345678901e+99` `99ms` `3556%` | `1.2345678901e+99` `190ms` `1853%` | `1.2345678901e+99` `140ms` `2514%` |
| `0.123456789012e100` | `1.23456789012e+99` `3547ms` | `1.23456789012e+99` `100ms` `3547%` | `1.23456789012e+99` `193ms` `1838%` | `1.23456789012e+99` `137ms` `2589%` |
| `0.1234567890123e100` | `1.234567890123e+99` `3573ms` | `1.234567890123e+99` `104ms` `3436%` | `1.234567890123e+99` `202ms` `1769%` | `1.234567890123e+99` `142ms` `2516%` |
| `0.12345678901234e100` | `1.2345678901234e+99` `3622ms` | `1.2345678901234e+99` `108ms` `3354%` | `1.2345678901234e+99` `200ms` `1811%` | `1.2345678901234e+99` `130ms` `2786%` |
| `0.123456789012345e100` | `1.23456789012345e+99` `3644ms` | `1.23456789012345e+99` `105ms` `3470%` | `1.23456789012345e+99` `214ms` `1703%` | `1.23456789012345e+99` `138ms` `2641%` |
| `0.1234567890123456e100` | `1.234567890123456e+99` `3611ms` | `1.234567890123456e+99` `111ms` `3253%` | `1.234567890123456e+99` `222ms` `1627%` | `1.234567890123456e+99` `141ms` `2561%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1e-100` | `1e-101` `1974ms` | `1e-101` `82ms` `2407%` | `1e-101` `82ms` `2407%` | `1e-101` `87ms` `2269%` |
| `0.12e-100` | `1.2e-101` `2052ms` | `1.2e-101` `84ms` `2443%` | `1.2e-101` `103ms` `1992%` | `1.2e-101` `100ms` `2052%` |
| `0.123e-100` | `1.23e-101` `2029ms` | `1.23e-101` `87ms` `2332%` | `1.23e-101` `119ms` `1705%` | `1.23e-101` `108ms` `1879%` |
| `0.1234e-100` | `1.234e-101` `2013ms` | `1.234e-101` `95ms` `2119%` | `1.234e-101` `128ms` `1573%` | `1.234e-101` `99ms` `2033%` |
| `0.12345e-100` | `1.2345e-101` `2026ms` | `1.2345e-101` `93ms` `2178%` | `1.2345e-101` `143ms` `1417%` | `1.2345e-101` `111ms` `1825%` |
| `0.123456e-100` | `1.23456e-101` `2062ms` | `1.23456e-101` `93ms` `2217%` | `1.23456e-101` `162ms` `1273%` | `1.23456e-101` `113ms` `1825%` |
| `0.1234567e-100` | `1.234567e-101` `2050ms` | `1.234567e-101` `93ms` `2204%` | `1.234567e-101` `174ms` `1178%` | `1.234567e-101` `111ms` `1847%` |
| `0.12345678e-100` | `1.2345678e-101` `2105ms` | `1.2345678e-101` `94ms` `2239%` | `1.2345678e-101` `191ms` `1102%` | `1.2345678e-101` `116ms` `1815%` |
| `0.123456789e-100` | `1.23456789e-101` `2073ms` | `1.23456789e-101` `99ms` `2094%` | `1.23456789e-101` `177ms` `1171%` | `1.23456789e-101` `120ms` `1728%` |
| `0.12345678901e-100` | `1.2345678901e-101` `2112ms` | `1.2345678901e-101` `103ms` `2050%` | `1.2345678901e-101` `192ms` `1100%` | `1.2345678901e-101` `126ms` `1676%` |
| `0.123456789012e-100` | `1.23456789012e-101` `2087ms` | `1.23456789012e-101` `105ms` `1988%` | `1.23456789012e-101` `201ms` `1038%` | `1.23456789012e-101` `121ms` `1725%` |
| `0.1234567890123e-100` | `1.234567890123e-101` `2057ms` | `1.234567890123e-101` `111ms` `1853%` | `1.234567890123e-101` `204ms` `1008%` | `1.234567890123e-101` `130ms` `1582%` |
| `0.12345678901234e-100` | `1.2345678901234e-101` `2120ms` | `1.2345678901234e-101` `113ms` `1876%` | `1.2345678901234e-101` `214ms` `991%` | `1.2345678901234e-101` `130ms` `1631%` |
| `0.123456789012345e-100` | `1.23456789012345e-101` `2049ms` | `1.23456789012345e-101` `113ms` `1813%` | `1.23456789012345e-101` `220ms` `931%` | `1.23456789012345e-101` `132ms` `1552%` |
| `0.1234567890123456e-100` | `1.234567890123456e-101` `2039ms` | `1.234567890123456e-101` `116ms` `1758%` | `1.234567890123456e-101` `220ms` `927%` | `1.234567890123456e-101` `146ms` `1397%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1e1` | `10` `869ms` | `10.0` `27ms` `3219%` | `10.0` `73ms` `1190%` | `10.0` `30ms` `2897%` |
| `1e2` | `100` `1028ms` | `100.0` `30ms` `3427%` | `100.0` `74ms` `1389%` | `100.0` `34ms` `3024%` |
| `1e3` | `1000` `1185ms` | `1000.0` `31ms` `3823%` | `1000.0` `69ms` `1717%` | `1000.0` `35ms` `3386%` |
| `1e4` | `10000` `1185ms` | `10000.0` `30ms` `3950%` | `10000.0` `76ms` `1559%` | `10000.0` `39ms` `3038%` |
| `1e5` | `100000` `1322ms` | `100000.0` `29ms` `4559%` | `100000.0` `75ms` `1763%` | `100000.0` `34ms` `3888%` |
| `1e6` | `1000000` `1329ms` | `1000000.0` `33ms` `4027%` | `1000000.0` `69ms` `1926%` | `1000000.0` `38ms` `3497%` |
| `1e7` | `10000000` `1382ms` | `10000000.0` `38ms` `3637%` | `10000000.0` `71ms` `1946%` | `10000000.0` `39ms` `3544%` |
| `1e8` | `100000000` `1428ms` | `100000000.0` `34ms` `4200%` | `100000000.0` `234ms` `610%` | `100000000.0` `35ms` `4080%` |
| `1e9` | `1000000000` `1463ms` | `1000000000.0` `31ms` `4719%` | `1000000000.0` `228ms` `642%` | `1000000000.0` `39ms` `3751%` |
| `1e10` | `10000000000` `1542ms` | `10000000000.0` `34ms` `4535%` | `10000000000.0` `229ms` `673%` | `10000000000.0` `39ms` `3954%` |
| `1e11` | `100000000000` `1634ms` | `100000000000.0` `36ms` `4539%` | `100000000000.0` `233ms` `701%` | `100000000000.0` `43ms` `3800%` |
| `1e12` | `1000000000000` `1716ms` | `1000000000000.0` `33ms` `5200%` | `1000000000000.0` `250ms` `686%` | `1000000000000.0` `39ms` `4400%` |
| `1e13` | `10000000000000` `1753ms` | `10000000000000.0` `34ms` `5156%` | `10000000000000.0` `246ms` `713%` | `10000000000000.0` `39ms` `4495%` |
| `1e14` | `100000000000000` `1824ms` | `100000000000000.0` `38ms` `4800%` | `100000000000000.0` `228ms` `800%` | `100000000000000.0` `45ms` `4053%` |
| `1e15` | `1000000000000000` `1900ms` | `1000000000000000.0` `39ms` `4872%` | `1000000000000000.0` `157ms` `1210%` | `1000000000000000.0` `46ms` `4130%` |
| `1e16` | `1e+16` `1069ms` | `10000000000000000.0` `95ms` `1125%` | `10000000000000000.0` `152ms` `703%` | `10000000000000000.0` `83ms` `1288%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1e-1` | `0.1` `1370ms` | `0.1` `82ms` `1671%` | `0.1` `74ms` `1851%` | `0.1` `92ms` `1489%` |
| `1e-2` | `0.01` `1403ms` | `0.01` `85ms` `1651%` | `0.01` `82ms` `1711%` | `0.01` `93ms` `1509%` |
| `1e-3` | `0.001` `1511ms` | `0.001` `82ms` `1843%` | `0.001` `87ms` `1737%` | `0.001` `93ms` `1625%` |
| `1e-4` | `0.0001` `1421ms` | `0.0001` `85ms` `1672%` | `0.0001` `96ms` `1480%` | `0.0001` `95ms` `1496%` |
| `1e-5` | `1e-05` `1413ms` | `0.00001` `88ms` `1606%` | `0.00001` `94ms` `1503%` | `0.00001` `96ms` `1472%` |
| `1e-6` | `1e-06` `1615ms` | `0.000001` `89ms` `1815%` | `0.000001` `96ms` `1682%` | `0.000001` `94ms` `1718%` |
| `1e-7` | `1e-07` `1493ms` | `0.0000001` `92ms` `1623%` | `1e-7` `68ms` `2196%` | `0.0000001` `92ms` `1623%` |
| `1e-8` | `1e-08` `1384ms` | `1e-8` `75ms` `1845%` | `1e-8` `67ms` `2066%` | `1e-8` `82ms` `1688%` |
| `1e-9` | `1e-09` `1512ms` | `1e-9` `80ms` `1890%` | `1e-9` `70ms` `2160%` | `1e-9` `85ms` `1779%` |
| `1e-10` | `1e-10` `1484ms` | `1e-10` `78ms` `1903%` | `1e-10` `77ms` `1927%` | `1e-10` `84ms` `1767%` |
| `1e-11` | `9.999999999999999e-12` `1548ms` | `1e-11` `82ms` `1888%` | `1e-11` `81ms` `1911%` | `1e-11` `84ms` `1843%` |
| `1e-12` | `1e-12` `1628ms` | `1e-12` `74ms` `2200%` | `1e-12` `82ms` `1985%` | `1e-12` `84ms` `1938%` |
| `1e-13` | `1e-13` `1597ms` | `1e-13` `75ms` `2129%` | `1e-13` `77ms` `2074%` | `1e-13` `84ms` `1901%` |
| `1e-14` | `1e-14` `1686ms` | `1e-14` `80ms` `2108%` | `1e-14` `76ms` `2218%` | `1e-14` `85ms` `1984%` |
| `1e-15` | `1e-15` `1630ms` | `1e-15` `78ms` `2090%` | `1e-15` `79ms` `2063%` | `1e-15` `82ms` `1988%` |
| `1e-16` | `1e-16` `1504ms` | `1e-16` `74ms` `2032%` | `1e-16` `69ms` `2180%` | `1e-16` `81ms` `1857%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.9` | `0.9` `1338ms` | `0.9` `89ms` `1503%` | `0.9` `80ms` `1672%` | `0.9` `90ms` `1487%` |
| `0.99` | `0.99` `1450ms` | `0.99` `80ms` `1812%` | `0.99` `89ms` `1629%` | `0.99` `99ms` `1465%` |
| `0.999` | `0.999` `1423ms` | `0.999` `91ms` `1564%` | `0.999` `112ms` `1271%` | `0.999` `99ms` `1437%` |
| `0.9999` | `0.9999` `1359ms` | `0.9999` `94ms` `1446%` | `0.9999` `122ms` `1114%` | `0.9999` `106ms` `1282%` |
| `0.99999` | `0.99999` `1362ms` | `0.99999` `100ms` `1362%` | `0.99999` `115ms` `1184%` | `0.99999` `106ms` `1285%` |
| `0.999999` | `0.999999` `1407ms` | `0.999999` `100ms` `1407%` | `0.999999` `122ms` `1153%` | `0.999999` `104ms` `1353%` |
| `0.9999999` | `0.9999999000000001` `1400ms` | `0.9999999` `102ms` `1373%` | `0.9999999` `130ms` `1077%` | `0.9999999` `104ms` `1346%` |
| `0.99999999` | `0.9999999899999999` `1417ms` | `0.99999999` `106ms` `1337%` | `0.99999999` `138ms` `1027%` | `0.99999999` `125ms` `1134%` |
| `0.999999999` | `0.999999999` `1392ms` | `0.999999999` `109ms` `1277%` | `0.999999999` `149ms` `934%` | `0.999999999` `122ms` `1141%` |
| `0.9999999999` | `0.9999999999` `1419ms` | `0.9999999999` `110ms` `1290%` | `0.9999999999` `153ms` `927%` | `0.9999999999` `132ms` `1075%` |
| `0.99999999999` | `0.99999999999` `1441ms` | `0.99999999999` `115ms` `1253%` | `0.99999999999` `162ms` `890%` | `0.99999999999` `135ms` `1067%` |
| `0.999999999999` | `0.999999999999` `1394ms` | `0.999999999999` `105ms` `1328%` | `0.999999999999` `164ms` `850%` | `0.999999999999` `134ms` `1040%` |
| `0.9999999999999` | `0.9999999999999` `1410ms` | `0.9999999999999` `115ms` `1226%` | `0.9999999999999` `179ms` `788%` | `0.9999999999999` `132ms` `1068%` |
| `0.99999999999999` | `0.99999999999999` `1376ms` | `0.99999999999999` `122ms` `1128%` | `0.99999999999999` `194ms` `709%` | `0.99999999999999` `138ms` `997%` |
| `0.999999999999999` | `0.999999999999999` `1366ms` | `0.999999999999999` `120ms` `1138%` | `0.999999999999999` `197ms` `693%` | `0.999999999999999` `136ms` `1004%` |
| `0.9999999999999999` | `0.9999999999999999` `1385ms` | `1.0` `81ms` `1710%` | `0.9999999999999999` `200ms` `692%` | `0.9999999999999999` `136ms` `1018%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1377ms` | `0.1` `83ms` `1659%` | `0.1` `77ms` `1788%` | `0.1` `91ms` `1513%` |
| `0.11` | `0.11` `1450ms` | `0.11` `114ms` `1272%` | `0.11` `91ms` `1593%` | `0.11` `94ms` `1543%` |
| `0.111` | `0.111` `1427ms` | `0.111` `95ms` `1502%` | `0.111` `114ms` `1252%` | `0.111` `103ms` `1385%` |
| `0.1111` | `0.1111` `1426ms` | `0.1111` `97ms` `1470%` | `0.1111` `121ms` `1179%` | `0.1111` `109ms` `1308%` |
| `0.11111` | `0.11111` `1409ms` | `0.11111` `100ms` `1409%` | `0.11111` `115ms` `1225%` | `0.11111` `111ms` `1269%` |
| `0.111111` | `0.111111` `1416ms` | `0.111111` `97ms` `1460%` | `0.111111` `126ms` `1124%` | `0.111111` `111ms` `1276%` |
| `0.1111111` | `0.1111111` `1426ms` | `0.1111111` `103ms` `1384%` | `0.1111111` `134ms` `1064%` | `0.1111111` `113ms` `1262%` |
| `0.11111111` | `0.11111111` `1474ms` | `0.11111111` `102ms` `1445%` | `0.11111111` `140ms` `1053%` | `0.11111111` `112ms` `1316%` |
| `0.111111111` | `0.111111111` `1522ms` | `0.111111111` `114ms` `1335%` | `0.111111111` `147ms` `1035%` | `0.111111111` `118ms` `1290%` |
| `0.1111111111` | `0.1111111111` `1463ms` | `0.1111111111` `110ms` `1330%` | `0.1111111111` `154ms` `950%` | `0.1111111111` `121ms` `1209%` |
| `0.11111111111` | `0.11111111111` `1449ms` | `0.11111111111` `108ms` `1342%` | `0.11111111111` `160ms` `906%` | `0.11111111111` `120ms` `1208%` |
| `0.111111111111` | `0.111111111111` `1462ms` | `0.111111111111` `111ms` `1317%` | `0.111111111111` `182ms` `803%` | `0.111111111111` `120ms` `1218%` |
| `0.1111111111111` | `0.1111111111111` `1435ms` | `0.1111111111111` `119ms` `1206%` | `0.1111111111111` `176ms` `815%` | `0.1111111111111` `137ms` `1047%` |
| `0.11111111111111` | `0.11111111111111` `1449ms` | `0.11111111111111` `117ms` `1238%` | `0.11111111111111` `188ms` `771%` | `0.11111111111111` `135ms` `1073%` |
| `0.111111111111111` | `0.111111111111111` `1438ms` | `0.111111111111111` `132ms` `1089%` | `0.111111111111111` `203ms` `708%` | `0.111111111111111` `135ms` `1065%` |
| `0.1111111111111111` | `0.1111111111111111` `1443ms` | `0.111111111111111` `121ms` `1193%` | `0.1111111111111111` `205ms` `704%` | `0.1111111111111111` `128ms` `1127%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :---- | :------ | :------ | :----- | :-------- |
| `1e-306` | `1e-306` `2501ms` | `1e-306` `80ms` `3126%` | `1e-306` `80ms` `3126%` | `1e-306` `88ms` `2842%` |
| `1e-307` | `9.999999999999999e-308` `2538ms` | `1e-307` `79ms` `3213%` | `1e-307` `91ms` `2789%` | `1e-307` `87ms` `2917%` |
| `1e-308` | `9.999999999999999e-309` `2646ms` | `1e-308` `80ms` `3308%` | `1e-308` `129ms` `2051%` | `1e-308` `67ms` `3949%` |
| `1e-309` | `1.000000000000002e-309` `2770ms` | `1e-309` `76ms` `3645%` | `1e-309` `98ms` `2827%` | `1e-309` `63ms` `4397%` |
| `1e-311` | `9.999999999999475e-312` `2588ms` | `1e-311` `69ms` `3751%` | `1e-311` `102ms` `2537%` | `1e-311` `58ms` `4462%` |
| `1e-312` | `9.999999999984653e-313` `2660ms` | `1e-312` `70ms` `3800%` | `1e-312` `97ms` `2742%` | `1e-312` `60ms` `4433%` |
| `1e-313` | `1.000000000013287e-313` `2622ms` | `1e-313` `69ms` `3800%` | `1e-313` `106ms` `2474%` | `1e-313` `66ms` `3973%` |
| `1e-314` | `9.999999999638807e-315` `2832ms` | `1e-314` `70ms` `4046%` | `1e-314` `102ms` `2776%` | `1e-314` `52ms` `5446%` |
| `1e-315` | `9.999999984816838e-316` `2739ms` | `1e-315` `55ms` `4980%` | `1e-315` `134ms` `2044%` | `1e-315` `50ms` `5478%` |
| `1e-316` | `9.999999836597144e-317` `2844ms` | `1e-316` `48ms` `5925%` | `1e-316` `132ms` `2155%` | `1e-316` `58ms` `4903%` |
| `1e-317` | `1.000000230692537e-317` `2916ms` | `1e-317` `57ms` `5116%` | `1e-317` `101ms` `2887%` | `1e-317` `48ms` `6075%` |
| `1e-318` | `9.999987484955998e-319` `3005ms` | `1e-318` `50ms` `6010%` | `1e-318` `99ms` `3035%` | `1e-318` `49ms` `6133%` |
| `1e-319` | `9.99988867182683e-320` `2349ms` | `1e-319` `51ms` `4606%` | `1e-319` `101ms` `2326%` | `1e-319` `53ms` `4432%` |
| `1e-320` | `9.99988867182683e-321` `2405ms` | `1e-320` `50ms` `4810%` | `1e-320` `97ms` `2479%` | `1e-320` `49ms` `4908%` |
| `1e-321` | `9.98012604599318e-322` `2422ms` | `1e-321` `54ms` `4485%` | `1e-321` `102ms` `2375%` | `1e-321` `59ms` `4105%` |
| `1e-322` | `9.881312916824931e-323` `2587ms` | `1e-322` `61ms` `4241%` | `1e-322` `102ms` `2536%` | `1e-322` `48ms` `5390%` |
| `1e-323` | `9.881312916824931e-324` `2497ms` | `1e-323` `46ms` `5428%` | `1e-323` `133ms` `1877%` | `1e-323` `45ms` `5549%` |

## APIs

Refer to `json.h` .

## Contact

* Phone: +86 18368887550
* wx/qq: 1083936981
* Email: lengjingzju@163.com 3090101217@zju.edu.cn
