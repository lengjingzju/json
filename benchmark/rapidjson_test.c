#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#define _fmalloc       malloc
#define _ffree         free
#define _fclose_fp(fp) do {if (fp) fclose(fp); fp = NULL; } while(0)
#define _free_ptr(ptr) do {if (ptr) _ffree(ptr); ptr = NULL; } while(0)

static unsigned int _system_ms_get(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

int read_file_to_data(const char *src, char **data, size_t *size)
{
    FILE *rfp = NULL;
    size_t total = 0;

    if (!src || !data)
        return -1;

    if (!size)
        size = &total;
    *data = NULL, *size = 0;

    if ((rfp = fopen(src, "r")) == NULL)
        return -1;
    fseek(rfp, 0, SEEK_END);
    *size = ftell(rfp);
    fseek(rfp, 0, SEEK_SET);
    if (*size == 0)
        goto err;

    if ((*data = (char*)_fmalloc(*size + 1)) == NULL)
        goto err;
    if (*size != fread(*data, 1, *size, rfp))
        goto err;

    (*data)[*size] = 0;
    _fclose_fp(rfp);
    return 0;
err:
    _fclose_fp(rfp);
    _free_ptr(*data);
    *size = 0;
    return -1;
}

int read_file_data_free(char **data, size_t *size)
{
    if (data)
        _free_ptr(*data);
    if (size)
        *size = 0;
    return 0;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    size_t size = 0;
    char *file = NULL;
    char *data = NULL;
    unsigned int ms[5] = {0};

    file = argv[1];
    if (strlen(file) == 0 || access(file, F_OK) != 0) {
        printf("%s is not exist!\n", file);
        return -1;
    }
    ms[0] = _system_ms_get();
    if (read_file_to_data(file, &data, &size) < 0) {
        printf("read file %s failed!\n", file);
        return -1;
    }

    ms[1] = _system_ms_get();

    rapidjson::Document document;
    document.Parse(data);

    ms[2] = _system_ms_get();

    rapidjson::StringBuffer formattedBuffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> prettyWriter(formattedBuffer);
    document.Accept(prettyWriter);

    ms[3] = _system_ms_get();

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    document.Accept(writer);

    ms[4] = _system_ms_get();
    printf("[dom] read=%-5d parse=%-5d format=%-5d unformat=%-5d\n",
        ms[1]-ms[0], ms[2]-ms[1], ms[3]-ms[2], ms[4]-ms[3]);

    if (data)
        read_file_data_free(&data, &size);
    return ret;
}
