#include <stdio.h>

// 之所以增加__cplusplus的宏定义，是为了同时兼容gcc编译器和g++编译器
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
#ifdef __cplusplus
};
#endif

#include <iconv.h> // iconv用于字符内码转换

int pre_num(unsigned char byte) {
    unsigned char mask = 0x80;
    int num = 0;
    int i = 0;
    while (i++ < 8) {
        if ((byte & mask) == mask) {
            mask = mask >> 1;
            num++;
        } else {
            break;
        }
    }
    return num;
}

// 是否为UTF-8编码
int is_utf8(unsigned char *data, int len) {
    int num = 0;
    int i = 0;
    while (i < len) {
        if ((data[i] & 0x80) == 0x00) {
            i++;
            continue;
        } else if ((num = pre_num(data[i])) > 2) {
            i++;
            int j = 0;
            while (j++ < num-1) {
                // 判断后面num - 1 个字节是不是都是10开
                if ((data[i] & 0xc0) != 0x80) {
                    return -1;
                }
                i++;
            }
        } else {
            // 其他情况说明不是utf-8
            return -1;
        }
    }
    return 0;
}

// 是否为GBK编码
int is_gbk(unsigned char *data, int len) {
    int i = 0;
    while (i < len) {
        if (data[i] <= 0x7f) {
            // 编码小于等于127，只有一个字节的编码，兼容ASCII
            i++;
            continue;
        } else {
            // 大于127的使用双字节编码
            if (data[i] >= 0x81 &&
                data[i] <= 0xfe &&
                data[i + 1] >= 0x40 &&
                data[i + 1] <= 0xfe) {
                i += 2;
                continue;
            } else {
                return -1;
            }
        }
    }
    return 0;
}

// 是否为ASCII编码
int is_ascii(unsigned char *data, int len) {
    int i = 0;
    while (i < len) {
        if (data[i] <= 0x7f) {
            // 编码小于等于127,只有一个字节的编码，兼容ASCII
            i++;
            continue;
        } else {
            return -1;
        }
    }
    return 0;
}

// 判断是否为iso编码
int is_iso(unsigned char *data, int len) {
    if (len < 4) {
        return 0;
    } else if ((data[0]==0xc2 || data[0]==0xc3)
          && (data[2]==0xc2 || data[2]==0xc3)) {
        return 0;
    } else {
        return -1;
    }
}

// 把字符串从GBK编码改为UTF-8编码
int gbk_to_utf8(char *src_str, size_t src_len, char *dst_str, size_t dst_len) {
    iconv_t cd;
    char **pin = &src_str;
    char **pout = &dst_str;
    cd = iconv_open("UTF-8", "GBK");
    if (cd == NULL)
        return -1;
    memset(dst_str, 0, dst_len);
    if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
        return -1;
    iconv_close(cd);
    *pout[0] = '\0';
    return 0;
}

// 把字符串从UTF-8编码改为GBK编码
int utf8_to_gbk(char *src_str, size_t src_len, char *dst_str, size_t dst_len) {
    iconv_t cd;
    char **pin = &src_str;
    char **pout = &dst_str;
    cd = iconv_open("GBK", "UTF-8");
    if (cd == NULL)
        return -1;
    memset(dst_str, 0, dst_len);
    if (iconv(cd, pin, &src_len, pout, &dst_len) == -1)
        return -1;
    iconv_close(cd);
    *pout[0] = '\0';
    return 0;
}

// 把iso编码转换为原来的内码
int iso_to_data(unsigned char *src_str, size_t src_len, char *dst_str) {
    int src_pos = 0, dst_pos = 0;
    for (; src_pos<src_len; src_pos++, dst_pos++) {
        if (src_str[src_pos] == 0xc2) {
            src_pos++;
            dst_str[dst_pos] = src_str[src_pos];
        } else if (src_str[src_pos] == 0xc3) {
            src_pos++;
            dst_str[dst_pos] = src_str[src_pos]+0x40;
        } else {
            dst_str[dst_pos] = src_str[src_pos];
        }
    }
    dst_str[dst_pos] = 0;
    return 0;
}

// 获取中文文本
void get_zh(char *key, char *src_str, char *dst_str, size_t dst_len) {
    size_t src_len = strlen(src_str);
    int flag_ascii = is_ascii((unsigned char*)src_str, src_len);
    if (flag_ascii == 0) { // 采用ASCII编码，直接返回
        snprintf(dst_str, dst_len, "%s", src_str);
        return;
    }
    char tmp_str[src_len+1];
    size_t tmp_len = src_len;
    int flag_iso = is_iso((unsigned char*)src_str, src_len);
    if (flag_iso == 0) {
        iso_to_data((unsigned char*)src_str, src_len, tmp_str);
        tmp_len = strlen(tmp_str);
    } else {
        memcpy(tmp_str, src_str, tmp_len);
        tmp_str[tmp_len] = 0;
    }
    int flag_utf8 = is_utf8((unsigned char*)tmp_str, tmp_len);
#ifdef _WIN32
    if (flag_utf8 == 0) { // 采用UTF-8编码，要改成Windows默认的GBK
        printf("Windows UTF-8\n");
        utf8_to_gbk(tmp_str, tmp_len, dst_str, dst_len);
    } else {
        printf("Windows GBK\n");
        snprintf(dst_str, dst_len, "%s", tmp_str);
    }
#else
    if (flag_utf8 == 0) {
        printf("Linux UTF-8\n");
        snprintf(dst_str, dst_len, "%s", tmp_str);
    } else { // 采用GBK编码，要改成Linux默认的UTF-8
        printf("Linux GBK\n");
        gbk_to_utf8(tmp_str, tmp_len, dst_str, dst_len);
    }
#endif
    printf("metadata %s=%s, length=%d, is_iso=%s, is_gbk=%s, is_utf8=%s\n", 
        key, tmp_str, tmp_len, flag_iso==0?"true":"false", 
        flag_utf8!=0?"true":"false", flag_utf8==0?"true":"false");
    return;
}

int main(int argc, char **argv) {
    const char *filename = "../fuzhou.mp4";
    if (argc > 1) {
        filename = argv[1];
    }
    AVFormatContext *fmt_ctx = NULL;
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't open file %s.\n", filename);
        return -1;
    }
    av_log(NULL, AV_LOG_INFO, "Success open input_file %s.\n", filename);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Can't find stream information.\n");
        return -1;
    }
    const AVDictionaryEntry *tag = NULL;
    // 遍历音视频文件的元数据，并转换元数据的中文内码
    while ((tag = av_dict_get(fmt_ctx->metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        int flag_ascii = is_ascii((unsigned char*)tag->value, strlen(tag->value));
        if (flag_ascii == 0) { // 为ASCII内码则无需转换
            continue;
        }
        int len = strlen(tag->value)*3/2;
        char dst_str[len];
        // 获取可在操作系统命令行正常显示的中文文本
        get_zh(tag->key, tag->value, dst_str, sizeof(dst_str));
        printf("command zh-cn is: %s\n", dst_str);
    }
    avformat_close_input(&fmt_ctx); // 关闭音视频文件
    return 0;
}