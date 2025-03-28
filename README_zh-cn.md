# LJSON 说明

[English Edition](./README.md)

LJSON 是一个远远快于 cJSON、大幅度快于 RapidJSON 的 C 实现的 JSON 库，他是目前最快的通用 JSON 库，也支持JSON5的全部特性。
LJSON 支持 JSON 的解析、打印、编辑，提供 DOM 和 SAX 接口，I/O 支持字符串和文件，且完全支持 nativejson-benchmark 的测试用例。
LJSON 默认使用个人开发的 ldouble 算法打印浮点数，和标准库对比可能只有第15位小数的区别，是目前最快的浮点数转字符串算法；也可选择个人优化过的 grisu2 算法或 dragonbox 算法。

## 功能特点

* 更快：打印和解析速度比 cJSON 和 RapidJSON 都要快，速度最高可比 CJSON 快32倍，比 Rapid JSON 快1.5倍，见测试结果
* 更省：提供多种省内存的手段，例如内存池、文件边读边解析、边打印边写文件、SAX方式的接口，可做到内存占用是个常数
* 更强：支持DOM和SAX风格的API，提供普通模式和内存池模式JSON的接口，支持字符串和文件作为输入输出(可扩展支持其它流)，扩展支持长长整形和十六进制数字
* 更友好：C语言实现，不依赖任何库，不含平台相关代码，只有一个头文件和库文件，和cJSON对应一致的接口，代码逻辑比任何JSON库都更清晰
* JSON5：支持全部JSON5特性，例如十六进制数字、注释、数组和对象的尾元素逗号、字符串值可以使用单引号，键值可以使用单引号或无引号等
* 缓冲：如果多次解析打印JSON，可以复用已分配的内存，库内部基本不进行任何的堆内存申请；可以将某个集合类型下的所有子对象保存为数据，并使用hash和二分法加快查找

## 编译运行

### 编译方法

* 直接编译

```sh
gcc -o ljson json.c jnum.c json_test.c -lm -O2 -ffunction-sections -fdata-sections -W -Wall
```

* [IMAKE](https://github.com/lengjingzju/cbuild-ng) 编译

```sh
make O=<编译输出目录> && make O=<编译输出目录> DESTDIR=<安装目录>
```

* 交叉编译

```sh
make O=<编译输出目录> CROSS_COMPILE=<交叉编译器前缀> && make O=<编译输出目录> DESTDIR=<安装目录>
```

* 选择浮点数转字符串算法 `gcc -DJSON_DTOA_ALGORITHM=n`， n可能为 0 / 1 / 2 / 3
    * 0: 个人实现的 ldouble 算法: 比谷歌的 grisu2 的默认实现快 **129%** ，比腾讯优化的 grisu2 实现快 **33%** ，比 sprintf 快 **14.6** 倍
    * 1: C标准库的 sprintf
    * 2: 个人优化的 grisu2 算法: 谷歌的 grisu2 的默认实现比 sprintf 快 **5.7** 倍，腾讯优化的 grisu2 实现比 sprintf 快 **9.1** 倍，LJSON 的优化实现比 sprintf 快 **11.4** 倍
    * 3: 个人优化的 dragonbox 算法: 性能和 ldouble 算法差，比 grisu2 算法强

### 运行方法

```sh
./ljson <json文件名> <测试序号0-7>
```

### 调试方法

* 设置 json.c 中的变量 `JSON_ERROR_PRINT_ENABLE` 的值为 `1` 后重新编译

### 解析配置

* `#define JSON_PARSE_SKIP_COMMENT         1` : 是否允许类似C语言的单行注释和多行注释(JSON5特性)
* `#define JSON_PARSE_LAST_COMMA           1` : 是否允许JSON_ARRAY或JSON_OBJECT的最后一个元素的末尾有逗号(JSON5特性)
* `#define JSON_PARSE_EMPTY_KEY            0` : 是否允许键为空字符串
* `#define JSON_PARSE_SPECIAL_CHAR         1` : 是否允许字符串中有特殊的字符，例如换行符(JSON5特性)
* `#define JSON_PARSE_SPECIAL_QUOTES       1` : 是否允许字符串值可以使用单引号，键值可以使用单引号或无引号(JSON5特性)
* `#define JSON_PARSE_HEX_NUM              1` : 是否允许十六进制的解析(JSON5特性)
* `#define JSON_PARSE_SPECIAL_NUM          1` : 是否允许特殊的数字，例如前导0，加号，无整数的浮点数等，例如 `+99` `.1234` `10.` `001` 等(JSON5特性)
* `#define JSON_PARSE_SPECIAL_DOUBLE       1` : 是否允许特殊的double值 `NaN` `Infinity` `-Infinity`(JSON5特性)
* `#define JSON_PARSE_SINGLE_VALUE         1` : 是否允许不是JSON_ARRAY或JSON_OBJECT开头的JSON值
* `#define JSON_PARSE_FINISHED_CHAR        0` : 是否解析完成后忽略检查字符串尾部的合法性

注：

* 如果需要100%符合 [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark)，需要将 `JSON_PARSE_EMPTY_KEY` 置为1，其它值全部置为0。
* `JSON_PARSE_SKIP_COMMENT` 和 `JSON_PARSE_SPECIAL_QUOTES` 置为1时会显著影响解析速度。

## 性能测试

### 测试代码

其它json的测试代码位于benchmark目录，将对应的文件放在对应json工程的根目录即可

```sh
gcc -o cjson cjson_test.c cJSON.c -O2               # cJSON
g++ -o rapidjson rapidjson_test.c -Iinclude -O2     # RapidJSON
gcc -o yyjson yyjson_test.c src/yyjson.c -Isrc -O2  # yyjson
gcc -o strdup strdup_test.c -O2                     # strdup和strlen
```

测试脚本

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

测试模式

* ljson提供7种测试模式
    * 1: 普通DOM模式，使用malloc申请内存，解析和打印都为字符串
    * 2: 快速DOM模式，申请大内存，然后内存从大内存分配(无法单独释放小内存)，解析和打印都为字符串
    * 3: 重用DOM模式，申请大内存，然后内存从大内存分配(无法单独释放小内存)，且键和字符串值重用原始解析字符串，解析和打印都为字符串
    * 4: 文件DOM模式，无需读完文件再解析或打印完再写入，使用malloc申请内存，边读文件边解析，边打印边写入文件
    * 5: 快速文件DOM模式，无需读完文件再解析或打印完再写入，申请大内存，然后内存从大内存分配(无法单独释放小内存)，边读文件边解析，边打印边写入文件
    * 6: 普通SAX模式，解析和打印都为字符串
    * 7: 文件SAX模式，无需读完文件再解析，边读文件边解析
* yyjson提供两种测试模式：unmutable 和 mutable 模式

### 测试结果

注：主要是测试速度，`O2` 优化等级且默认选项编译，测试文件来自 [nativejson-benchmark](https://github.com/miloyip/nativejson-benchmark) 项目

> 测试平台: ARM64开发板 | CPU: ARM CortexA53 | OS: Linux-5.15<br>
> 测试结果: LJSON 比cJSON 解析最快可达 475%，打印最快可达 2836%，LJSON 比 RapidJSON 解析最快可达 131%，打印最快可达 147% (耗时含文件读写时间)

![AARCH64-Linux测试结果](image/test_for_aarch64.png)

> 测试平台: PC | CPU: Intel i7-10700 | OS: Ubuntu 18.04 (VirtualBox)<br>
> 测试结果: LJSON 比cJSON 解析最快可达 560%，打印最快可达 3184%，LJSON 比 RapidJSON 解析最快可达 75%，打印最快可达 133% (耗时含文件读写时间)

![x86_64-Linux测试结果](image/test_for_x86_64.png)

![ldouble-x86_64测试结果](image/ldb_for_x86_64.png)

> 测试平台: PC | CPU: Intel i7-1260P | OS: Ubuntu 20.04 (VMWare)<br>
> 测试结果: LJSON 比cJSON 解析最快可达 510%，打印最快可达 2273%，LJSON 比 RapidJSON 解析最快可达 76%，打印最快可达 144% (耗时不含文件读写时间)

![x86_64-Linux测试结果2](image/test_for_x86_64-2.png)

> 测试平台: Nationalchip STB | CPU: CSKY | DDR3: 128MB, 533MHz | OS: ECOS<br>
> 注: 老版本测试结果，新版本删除了临时buffer，且解析速度提升了两倍

![ECOS测试结果](image/test_for_csky.png)

## 接口说明

见 `json.h` 文件。

## 参考代码

* 参考了[cJSON](https://github.com/DaveGamble/cJSON)的实现，其中utf16_literal_to_utf8函数完全来自cJSON

## 联系方式

* Phone: +86 18368887550
* wx/qq: 1083936981
* Email: lengjingzju@163.com 3090101217@zju.edu.cn
