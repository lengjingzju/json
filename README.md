# LJSON Description

[中文版](./README_zh-cn.md)

LJSON is a C implemented JSON library that is much faster than cJSON and substantially faster than RapidJSON, it is currently the fastest general-purpose JSON library and supports all JSON5 features.

LJSON supports JSON parsing, printing and editing, provides DOM and SAX APIs, and I/O supports string and file, it fully supports the test cases of nativejson-benchmark.

By default, LJSON uses the personally developed ldouble algorithm to print double to string. Compared with the standard library, it may only be the 15th or 16th decimal place difference. It is currently the fastest double to string algorithm; users can also choose the personally optimized grisu2 algorithm or dragonbox algorithm.

The ldouble algorithm is an order of magnitude faster than sprintf (40 times faster for some cases), and is also faster than grisu (implemented by Google and used by the browser V8 engine) and dragonbox (used by high-performance computing libraries such as Boost and Eigen) (some cases are 100% faster than grisu2 and 60% faster than dragonbox); the ldouble algorithm can guarantee the shortest output within the precision range, which is consistent with sprintf output, while grisu and dragonbox cannot do this.

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

> Test Platform: PC | CPU: Intel i7-1260P | OS: Ubuntu 20.04 (VMWare)<br>
> Test Command: `./jnum_test <num> 10000000` # Test ten million times
> Test Result: ldouble is an order of magnitude faster than sprintf (40 times faster in some cases), and slightly faster than grisu2 and dragonbox; ldouble can guarantee the shortest output within the precision range, which is consistent with printf output, while grisu and dragonbox cannot do this

Note: The three values are "the output string", "the time taken for running ten million times", and "the percentage of speed improvement compared to sprintf".

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1264ms` | `0.1` `83ms` `1523%` | `0.1` `75ms` `1685%` | `0.1` `90ms` `1404%` |
| `0.2` | `0.2` `1217ms` | `0.2` `82ms` `1484%` | `0.2` `75ms` `1623%` | `0.2` `89ms` `1367%` |
| `0.3` | `0.3` `1222ms` | `0.3` `84ms` `1455%` | `0.30000000000000007` `201ms` `608%` | `0.30000000000000004` `86ms` `1421%` |
| `0.4` | `0.4` `1219ms` | `0.4` `83ms` `1469%` | `0.4` `71ms` `1717%` | `0.4` `90ms` `1354%` |
| `0.5` | `0.5` `1231ms` | `0.5` `82ms` `1501%` | `0.5` `70ms` `1759%` | `0.5` `83ms` `1483%` |
| `0.6` | `0.6` `1263ms` | `0.6` `75ms` `1684%` | `0.6000000000000001` `188ms` `672%` | `0.6000000000000001` `108ms` `1169%` |
| `0.7` | `0.7` `1221ms` | `0.7` `75ms` `1628%` | `0.7000000000000001` `194ms` `629%` | `0.7000000000000001` `108ms` `1131%` |
| `0.8` | `0.8` `1245ms` | `0.8` `78ms` `1596%` | `0.8` `76ms` `1638%` | `0.8` `85ms` `1465%` |
| `0.9` | `0.9` `1220ms` | `0.9` `78ms` `1564%` | `0.9` `77ms` `1584%` | `0.9` `86ms` `1419%` |
| `1.0` | `1` `1019ms` | `1.0` `24ms` `4246%` | `1.0` `58ms` `1757%` | `1.0` `28ms` `3639%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1282ms` | `0.1` `80ms` `1602%` | `0.1` `74ms` `1732%` | `0.1` `88ms` `1457%` |
| `0.12` | `0.12` `1378ms` | `0.12` `83ms` `1660%` | `0.12` `91ms` `1514%` | `0.12` `88ms` `1566%` |
| `0.123` | `0.123` `1360ms` | `0.123` `89ms` `1528%` | `0.123` `107ms` `1271%` | `0.123` `98ms` `1388%` |
| `0.1234` | `0.1234` `1290ms` | `0.1234` `85ms` `1518%` | `0.12340000000000001` `202ms` `639%` | `0.12340000000000001` `124ms` `1040%` |
| `0.12345` | `0.12345` `1306ms` | `0.12345` `95ms` `1375%` | `0.12345` `112ms` `1166%` | `0.12345` `104ms` `1256%` |
| `0.123456` | `0.123456` `1335ms` | `0.123456` `95ms` `1405%` | `0.123456` `117ms` `1141%` | `0.123456` `102ms` `1309%` |
| `0.1234567` | `0.1234567` `1337ms` | `0.1234567` `96ms` `1393%` | `0.12345669999999999` `205ms` `652%` | `0.12345669999999999` `133ms` `1005%` |
| `0.12345678` | `0.12345678` `1334ms` | `0.12345678` `94ms` `1419%` | `0.12345678` `130ms` `1026%` | `0.12345678` `109ms` `1224%` |
| `0.123456789` | `0.123456789` `1322ms` | `0.123456789` `108ms` `1224%` | `0.12345678900000001` `208ms` `636%` | `0.12345678900000001` `121ms` `1093%` |
| `0.12345678901` | `0.12345678901` `1441ms` | `0.12345678901` `107ms` `1347%` | `0.12345678901` `166ms` `868%` | `0.12345678901` `116ms` `1242%` |
| `0.123456789012` | `0.123456789012` `1386ms` | `0.123456789012` `108ms` `1283%` | `0.123456789012` `159ms` `872%` | `0.123456789012` `113ms` `1227%` |
| `0.1234567890123` | `0.1234567890123` `1334ms` | `0.1234567890123` `110ms` `1213%` | `0.1234567890123` `169ms` `789%` | `0.1234567890123` `114ms` `1170%` |
| `0.12345678901234` | `0.12345678901234` `1352ms` | `0.12345678901234` `114ms` `1186%` | `0.12345678901234` `177ms` `764%` | `0.12345678901234` `118ms` `1146%` |
| `0.123456789012345` | `0.123456789012345` `1339ms` | `0.123456789012345` `113ms` `1185%` | `0.12345678901234501` `199ms` `673%` | `0.12345678901234501` `143ms` `936%` |
| `0.1234567890123456` | `0.123456789012346` `1293ms` | `0.1234567890123455` `119ms` `1087%` | `0.1234567890123456` `191ms` `677%` | `0.1234567890123456` `120ms` `1078%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `0.1e100` | `1e+99` `2262ms` | `1e+99` `66ms` `3427%` | `1.0000000000000001e+99` `223ms` `1014%` | `1.0000000000000001e+99` `93ms` `2432%` |
| `0.12e100` | `1.2e+99` `2495ms` | `1.2e+99` `73ms` `3418%` | `1.2e+99` `84ms` `2970%` | `1.2e+99` `92ms` `2712%` |
| `0.123e100` | `1.23e+99` `3185ms` | `1.23e+99` `77ms` `4136%` | `1.23e+99` `102ms` `3123%` | `1.23e+99` `102ms` `3123%` |
| `0.1234e100` | `1.234e+99` `2628ms` | `1.234e+99` `79ms` `3327%` | `1.2340000000000002e+99` `225ms` `1168%` | `1.2340000000000002e+99` `114ms` `2305%` |
| `0.12345e100` | `1.2345e+99` `2654ms` | `1.2345e+99` `85ms` `3122%` | `1.2345000000000002e+99` `237ms` `1120%` | `1.2345000000000002e+99` `119ms` `2230%` |
| `0.123456e100` | `1.23456e+99` `2740ms` | `1.23456e+99` `83ms` `3301%` | `1.23456e+99` `150ms` `1827%` | `1.23456e+99` `110ms` `2491%` |
| `0.1234567e100` | `1.234567e+99` `3313ms` | `1.234567e+99` `88ms` `3765%` | `1.2345669999999998e+99` `251ms` `1320%` | `1.2345669999999998e+99` `133ms` `2491%` |
| `0.12345678e100` | `1.2345678e+99` `3222ms` | `1.2345678e+99` `81ms` `3978%` | `1.2345678e+99` `175ms` `1841%` | `1.2345678e+99` `105ms` `3069%` |
| `0.123456789e100` | `1.23456789e+99` `2902ms` | `1.23456789e+99` `99ms` `2931%` | `1.23456789e+99` `170ms` `1707%` | `1.23456789e+99` `118ms` `2459%` |
| `0.12345678901e100` | `1.2345678901e+99` `3242ms` | `1.2345678901e+99` `98ms` `3308%` | `1.2345678901e+99` `182ms` `1781%` | `1.2345678901e+99` `119ms` `2724%` |
| `0.123456789012e100` | `1.23456789012e+99` `3217ms` | `1.23456789012e+99` `96ms` `3351%` | `1.23456789012e+99` `199ms` `1617%` | `1.23456789012e+99` `119ms` `2703%` |
| `0.1234567890123e100` | `1.234567890123e+99` `3230ms` | `1.234567890123e+99` `102ms` `3167%` | `1.2345678901230002e+99` `237ms` `1363%` | `1.2345678901230002e+99` `133ms` `2429%` |
| `0.12345678901234e100` | `1.2345678901234e+99` `3159ms` | `1.2345678901234e+99` `101ms` `3128%` | `1.2345678901234001e+99` `226ms` `1398%` | `1.2345678901234001e+99` `131ms` `2411%` |
| `0.123456789012345e100` | `1.23456789012345e+99` `3236ms` | `1.23456789012345e+99` `97ms` `3336%` | `1.2345678901234502e+99` `221ms` `1464%` | `1.2345678901234502e+99` `132ms` `2452%` |
| `0.1234567890123456e100` | `1.23456789012346e+99` `3486ms` | `1.234567890123455e+99` `111ms` `3141%` | `1.234567890123456e+99` `216ms` `1614%` | `1.234567890123456e+99` `125ms` `2789%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `0.1e-100` | `1e-101` `1844ms` | `1e-101` `73ms` `2526%` | `1e-101` `79ms` `2334%` | `1e-101` `80ms` `2305%` |
| `0.12e-100` | `1.2e-101` `1948ms` | `1.2e-101` `77ms` `2530%` | `1.2e-101` `93ms` `2095%` | `1.2e-101` `95ms` `2051%` |
| `0.123e-100` | `1.23e-101` `1887ms` | `1.23e-101` `81ms` `2330%` | `1.23e-101` `107ms` `1764%` | `1.23e-101` `99ms` `1906%` |
| `0.1234e-100` | `1.234e-101` `1897ms` | `1.234e-101` `85ms` `2232%` | `1.2340000000000002e-101` `238ms` `797%` | `1.2340000000000002e-101` `117ms` `1621%` |
| `0.12345e-100` | `1.2345e-101` `1892ms` | `1.2345e-101` `85ms` `2226%` | `1.2345e-101` `130ms` `1455%` | `1.2345e-101` `106ms` `1785%` |
| `0.123456e-100` | `1.23456e-101` `1918ms` | `1.23456e-101` `86ms` `2230%` | `1.23456e-101` `147ms` `1305%` | `1.23456e-101` `103ms` `1862%` |
| `0.1234567e-100` | `1.234567e-101` `2093ms` | `1.234567e-101` `86ms` `2434%` | `1.2345669999999999e-101` `237ms` `883%` | `1.2345669999999999e-101` `138ms` `1517%` |
| `0.12345678e-100` | `1.2345678e-101` `1923ms` | `1.2345678e-101` `86ms` `2236%` | `1.2345678e-101` `184ms` `1045%` | `1.2345678e-101` `109ms` `1764%` |
| `0.123456789e-100` | `1.23456789e-101` `1943ms` | `1.23456789e-101` `96ms` `2024%` | `1.2345678900000001e-101` `226ms` `860%` | `1.2345678900000001e-101` `132ms` `1472%` |
| `0.12345678901e-100` | `1.2345678901e-101` `1931ms` | `1.2345678901e-101` `99ms` `1951%` | `1.2345678901e-101` `196ms` `985%` | `1.2345678901e-101` `116ms` `1665%` |
| `0.123456789012e-100` | `1.23456789012e-101` `1958ms` | `1.23456789012e-101` `99ms` `1978%` | `1.23456789012e-101` `201ms` `974%` | `1.23456789012e-101` `114ms` `1718%` |
| `0.1234567890123e-100` | `1.234567890123e-101` `1971ms` | `1.234567890123e-101` `110ms` `1792%` | `1.234567890123e-101` `201ms` `981%` | `1.234567890123e-101` `126ms` `1564%` |
| `0.12345678901234e-100` | `1.2345678901234e-101` `1942ms` | `1.2345678901234e-101` `104ms` `1867%` | `1.2345678901234001e-101` `229ms` `848%` | `1.2345678901234001e-101` `135ms` `1439%` |
| `0.123456789012345e-100` | `1.23456789012345e-101` `1890ms` | `1.23456789012345e-101` `109ms` `1734%` | `1.2345678901234502e-101` `241ms` `784%` | `1.2345678901234502e-101` `136ms` `1390%` |
| `0.1234567890123456e-100` | `1.23456789012346e-101` `1881ms` | `1.234567890123455e-101` `106ms` `1775%` | `1.2345678901234559e-101` `225ms` `836%` | `1.2345678901234559e-101` `135ms` `1393%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `1e1` | `10` `803ms` | `10.0` `26ms` `3088%` | `10.0` `61ms` `1316%` | `10.0` `31ms` `2590%` |
| `1e2` | `100` `941ms` | `100.0` `26ms` `3619%` | `100.0` `65ms` `1448%` | `100.0` `33ms` `2852%` |
| `1e3` | `1000` `1085ms` | `1000.0` `31ms` `3500%` | `1000.0` `67ms` `1619%` | `1000.0` `35ms` `3100%` |
| `1e4` | `10000` `1090ms` | `10000.0` `30ms` `3633%` | `10000.0` `74ms` `1473%` | `10000.0` `34ms` `3206%` |
| `1e5` | `100000` `1257ms` | `100000.0` `28ms` `4489%` | `100000.0` `70ms` `1796%` | `100000.0` `34ms` `3697%` |
| `1e6` | `1000000` `1240ms` | `1000000.0` `31ms` `4000%` | `1000000.0` `63ms` `1968%` | `1000000.0` `38ms` `3263%` |
| `1e7` | `10000000` `1253ms` | `10000000.0` `34ms` `3685%` | `10000000.0` `62ms` `2021%` | `10000000.0` `39ms` `3213%` |
| `1e8` | `100000000` `1306ms` | `100000000.0` `31ms` `4213%` | `100000000.0` `228ms` `573%` | `100000000.0` `35ms` `3731%` |
| `1e9` | `1000000000` `1427ms` | `1000000000.0` `34ms` `4197%` | `1000000000.0` `228ms` `626%` | `1000000000.0` `37ms` `3857%` |
| `1e10` | `10000000000` `1456ms` | `10000000000.0` `35ms` `4160%` | `10000000000.0` `227ms` `641%` | `10000000000.0` `42ms` `3467%` |
| `1e11` | `100000000000` `1549ms` | `100000000000.0` `36ms` `4303%` | `100000000000.0` `225ms` `688%` | `100000000000.0` `44ms` `3520%` |
| `1e12` | `1000000000000` `1606ms` | `1000000000000.0` `33ms` `4867%` | `1000000000000.0` `251ms` `640%` | `1000000000000.0` `40ms` `4015%` |
| `1e13` | `10000000000000` `1627ms` | `10000000000000.0` `33ms` `4930%` | `10000000000000.0` `243ms` `670%` | `10000000000000.0` `42ms` `3874%` |
| `1e14` | `100000000000000` `1725ms` | `100000000000000.0` `37ms` `4662%` | `100000000000000.0` `234ms` `737%` | `100000000000000.0` `45ms` `3833%` |
| `1e15` | `1e+15` `958ms` | `1000000000000000.0` `37ms` `2589%` | `1000000000000000.0` `152ms` `630%` | `1000000000000000.0` `49ms` `1955%` |
| `1e16` | `1e+16` `964ms` | `10000000000000000.0` `85ms` `1134%` | `10000000000000000.0` `153ms` `630%` | `10000000000000000.0` `80ms` `1205%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `1e-1` | `0.1` `1297ms` | `0.1` `80ms` `1621%` | `0.1` `73ms` `1777%` | `0.1` `88ms` `1474%` |
| `1e-2` | `0.01` `1307ms` | `0.01` `87ms` `1502%` | `0.01` `80ms` `1634%` | `0.01` `96ms` `1361%` |
| `1e-3` | `0.001` `1401ms` | `0.001` `82ms` `1709%` | `0.001` `83ms` `1688%` | `0.001` `90ms` `1557%` |
| `1e-4` | `0.0001` `1343ms` | `0.0001` `78ms` `1722%` | `0.0001` `90ms` `1492%` | `0.0001` `91ms` `1476%` |
| `1e-5` | `1e-05` `1305ms` | `0.00001` `79ms` `1652%` | `0.00001` `84ms` `1554%` | `0.00001` `93ms` `1403%` |
| `1e-6` | `1e-06` `1476ms` | `0.000001` `82ms` `1800%` | `0.000001` `86ms` `1716%` | `0.000001` `90ms` `1640%` |
| `1e-7` | `1e-07` `1390ms` | `0.0000001` `77ms` `1805%` | `1e-7` `63ms` `2206%` | `0.0000001` `92ms` `1511%` |
| `1e-8` | `1e-08` `1315ms` | `1e-8` `74ms` `1777%` | `1e-8` `62ms` `2121%` | `1e-8` `78ms` `1686%` |
| `1e-9` | `1e-09` `1401ms` | `1e-9` `71ms` `1973%` | `1e-9` `62ms` `2260%` | `1e-9` `77ms` `1819%` |
| `1e10` | `10000000000` `1417ms` | `10000000000.0` `37ms` `3830%` | `10000000000.0` `229ms` `619%` | `10000000000.0` `40ms` `3542%` |
| `1e-10` | `1e-10` `1354ms` | `1e-10` `71ms` `1907%` | `1e-10` `72ms` `1881%` | `1e-10` `77ms` `1758%` |
| `1e-11` | `1e-11` `1444ms` | `1e-11` `78ms` `1851%` | `1e-11` `80ms` `1805%` | `1e-11` `77ms` `1875%` |
| `1e-12` | `1e-12` `1522ms` | `1e-12` `73ms` `2085%` | `1e-12` `81ms` `1879%` | `1e-12` `82ms` `1856%` |
| `1e-13` | `1e-13` `1453ms` | `1e-13` `74ms` `1964%` | `1e-13` `70ms` `2076%` | `1e-13` `79ms` `1839%` |
| `1e-14` | `1e-14` `1598ms` | `1e-14` `76ms` `2103%` | `1e-14` `82ms` `1949%` | `1e-14` `82ms` `1949%` |
| `1e-15` | `1e-15` `1535ms` | `1e-15` `77ms` `1994%` | `1e-15` `76ms` `2020%` | `1e-15` `89ms` `1725%` |
| `1e-16` | `1e-16` `1454ms` | `1e-16` `78ms` `1864%` | `1e-16` `68ms` `2138%` | `1e-16` `81ms` `1795%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `0.9` | `0.9` `1265ms` | `0.9` `81ms` `1562%` | `0.9` `76ms` `1664%` | `0.9` `85ms` `1488%` |
| `0.99` | `0.99` `1320ms` | `0.99` `95ms` `1389%` | `0.99` `92ms` `1435%` | `0.99` `96ms` `1375%` |
| `0.999` | `0.999` `1305ms` | `0.999` `87ms` `1500%` | `0.999` `103ms` `1267%` | `0.999` `96ms` `1359%` |
| `0.9999` | `0.9999` `1276ms` | `0.9999` `94ms` `1357%` | `0.9999` `118ms` `1081%` | `0.9999` `99ms` `1289%` |
| `0.99999` | `0.99999` `1274ms` | `0.99999` `92ms` `1385%` | `0.99999` `113ms` `1127%` | `0.99999` `103ms` `1237%` |
| `0.999999` | `0.999999` `1260ms` | `0.999999` `101ms` `1248%` | `0.999999` `121ms` `1041%` | `0.999999` `105ms` `1200%` |
| `0.9999999` | `0.9999999` `1295ms` | `0.9999999` `96ms` `1349%` | `0.9999998999999999` `192ms` `674%` | `0.9999998999999999` `132ms` `981%` |
| `0.99999999` | `0.99999999` `1276ms` | `0.99999999` `107ms` `1193%` | `0.9999999900000001` `192ms` `665%` | `0.9999999900000001` `115ms` `1110%` |
| `0.999999999` | `0.999999999` `1275ms` | `0.999999999` `108ms` `1181%` | `0.999999999` `140ms` `911%` | `0.999999999` `113ms` `1128%` |
| `0.9999999999` | `0.9999999999` `1284ms` | `0.9999999999` `107ms` `1200%` | `0.9999999999` `148ms` `868%` | `0.9999999999` `112ms` `1146%` |
| `0.99999999999` | `0.99999999999` `1334ms` | `0.99999999999` `107ms` `1247%` | `0.9999999999899999` `190ms` `702%` | `0.9999999999899999` `134ms` `996%` |
| `0.999999999999` | `0.999999999999` `1267ms` | `0.999999999999` `111ms` `1141%` | `0.999999999999` `169ms` `750%` | `0.999999999999` `117ms` `1083%` |
| `0.9999999999999` | `0.9999999999999` `1248ms` | `0.9999999999999` `114ms` `1095%` | `0.9999999999999001` `192ms` `650%` | `0.9999999999999001` `126ms` `990%` |
| `0.99999999999999` | `0.99999999999999` `1281ms` | `0.99999999999999` `116ms` `1104%` | `0.99999999999999` `188ms` `681%` | `0.99999999999999` `123ms` `1041%` |
| `0.999999999999999` | `0.999999999999999` `1264ms` | `1.0` `77ms` `1642%` | `0.9999999999999991` `200ms` `632%` | `0.9999999999999991` `135ms` `936%` |

| Input | sprintf | ldouble | grisu2 | dragonbox |
| :--- | :------ | :------ | :----- | :-------- |
| `0.1` | `0.1` `1299ms` | `0.1` `83ms` `1565%` | `0.1` `79ms` `1644%` | `0.1` `91ms` `1427%` |
| `0.11` | `0.11` `1319ms` | `0.11` `83ms` `1589%` | `0.11` `95ms` `1388%` | `0.11` `93ms` `1418%` |
| `0.111` | `0.111` `1336ms` | `0.111` `90ms` `1484%` | `0.111` `102ms` `1310%` | `0.111` `94ms` `1421%` |
| `0.1111` | `0.1111` `1363ms` | `0.1111` `90ms` `1514%` | `0.1111` `118ms` `1155%` | `0.1111` `96ms` `1420%` |
| `0.11111` | `0.11111` `1302ms` | `0.11111` `100ms` `1302%` | `0.11111000000000002` `203ms` `641%` | `0.11111000000000001` `114ms` `1142%` |
| `0.111111` | `0.111111` `1317ms` | `0.111111` `96ms` `1372%` | `0.111111` `122ms` `1080%` | `0.111111` `102ms` `1291%` |
| `0.1111111` | `0.1111111` `1332ms` | `0.1111111` `96ms` `1388%` | `0.11111109999999999` `204ms` `653%` | `0.11111109999999999` `133ms` `1002%` |
| `0.11111111` | `0.11111111` `1388ms` | `0.11111111` `95ms` `1461%` | `0.11111111` `132ms` `1052%` | `0.11111111` `104ms` `1335%` |
| `0.111111111` | `0.111111111` `1315ms` | `0.111111111` `107ms` `1229%` | `0.11111111100000001` `198ms` `664%` | `0.11111111100000001` `118ms` `1114%` |
| `0.1111111111` | `0.1111111111` `1335ms` | `0.1111111111` `112ms` `1192%` | `0.1111111111` `145ms` `921%` | `0.1111111111` `117ms` `1141%` |
| `0.11111111111` | `0.11111111111` `1351ms` | `0.11111111111` `105ms` `1287%` | `0.11111111111` `153ms` `883%` | `0.11111111111` `112ms` `1206%` |
| `0.111111111111` | `0.111111111111` `1344ms` | `0.111111111111` `106ms` `1268%` | `0.111111111111` `161ms` `835%` | `0.111111111111` `113ms` `1189%` |
| `0.1111111111111` | `0.1111111111111` `1311ms` | `0.1111111111111` `109ms` `1203%` | `0.1111111111111` `163ms` `804%` | `0.1111111111111` `118ms` `1111%` |
| `0.11111111111111` | `0.11111111111111` `1321ms` | `0.11111111111111` `111ms` `1190%` | `0.11111111111111` `181ms` `730%` | `0.11111111111111` `121ms` `1092%` |
| `0.111111111111111` | `0.111111111111111` `1376ms` | `0.111111111111111` `116ms` `1186%` | `0.11111111111111101` `205ms` `671%` | `0.11111111111111101` `131ms` `1050%` |
| `0.1111111111111111` | `0.111111111111111` `1314ms` | `0.111111111111111` `117ms` `1123%` | `0.11111111111111109` `205ms` `641%` | `0.11111111111111109` `134ms` `981%` |

## APIs

Refer to `json.h` .

## Contact

* Phone: +86 18368887550
* wx/qq: 1083936981
* Email: lengjingzju@163.com 3090101217@zju.edu.cn
