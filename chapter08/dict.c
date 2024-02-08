/*
 * copyright (c) 2009 Michael Niedermayer
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <string.h>

#include "avstring.h"
#include "dict.h"
#include "internal.h"
#include "mem.h"
#include "time_internal.h"
#include "bprint.h"

#include <iconv.h>

int pre_num(unsigned char byte);
int is_utf8(unsigned char* data, int len);
int is_gbk(unsigned char* data, int len);
int is_ascii(unsigned char* data, int len);
int is_iso(unsigned char* data, int len);
int gbk_to_utf8(char *src_str, size_t src_len, char *dst_str, size_t dst_len);
int utf8_to_gbk(char *src_str, size_t src_len, char *dst_str, size_t dst_len);
int iso_to_data(unsigned char *src_str, size_t src_len, char *dst_str);

struct AVDictionary {
    int count;
    AVDictionaryEntry *elems;
};

int av_dict_count(const AVDictionary *m)
{
    return m ? m->count : 0;
}

AVDictionaryEntry *av_dict_get(const AVDictionary *m, const char *key,
                               const AVDictionaryEntry *prev, int flags)
{
    unsigned int i, j;

    if (!m)
        return NULL;

    if (prev)
        i = prev - m->elems + 1;
    else
        i = 0;

    for (; i < m->count; i++) {
        const char *s = m->elems[i].key;
        if (flags & AV_DICT_MATCH_CASE)
            for (j = 0; s[j] == key[j] && key[j]; j++)
                ;
        else
            for (j = 0; av_toupper(s[j]) == av_toupper(key[j]) && key[j]; j++)
                ;
        if (key[j])
            continue;
        if (s[j] && !(flags & AV_DICT_IGNORE_SUFFIX))
            continue;
        return &m->elems[i];
    }
    return NULL;
}

int av_dict_set(AVDictionary **pm, const char *key, const char *value,
                int flags)
{
    AVDictionary *m = *pm;
    AVDictionaryEntry *tag = NULL;
    char *oldval = NULL, *copy_key = NULL, *copy_value = NULL;

    if (!(flags & AV_DICT_MULTIKEY)) {
        tag = av_dict_get(m, key, NULL, flags);
    }
    if (flags & AV_DICT_DONT_STRDUP_KEY)
        copy_key = (void *)key;
    else
        copy_key = av_strdup(key);
    if (flags & AV_DICT_DONT_STRDUP_VAL)
        copy_value = (void *)value;
    else if (copy_key)
        copy_value = av_strdup(value);
    if (!m)
        m = *pm = av_mallocz(sizeof(*m));
    if (!m || (key && !copy_key) || (value && !copy_value))
        goto err_out;

    if (tag) {
        if (flags & AV_DICT_DONT_OVERWRITE) {
            av_free(copy_key);
            av_free(copy_value);
            return 0;
        }
        if (flags & AV_DICT_APPEND)
            oldval = tag->value;
        else
            av_free(tag->value);
        av_free(tag->key);
        *tag = m->elems[--m->count];
    } else if (copy_value) {
        AVDictionaryEntry *tmp = av_realloc_array(m->elems,
                                                  m->count + 1, sizeof(*m->elems));
        if (!tmp)
            goto err_out;
        m->elems = tmp;
    }
    if (copy_value) {
        m->elems[m->count].key = copy_key;
        m->elems[m->count].value = copy_value;
        if (oldval && flags & AV_DICT_APPEND) {
            size_t len = strlen(oldval) + strlen(copy_value) + 1;
            char *newval = av_mallocz(len);
            if (!newval)
                goto err_out;
            av_strlcat(newval, oldval, len);
            av_freep(&oldval);
            av_strlcat(newval, copy_value, len);
            m->elems[m->count].value = newval;
            av_freep(&copy_value);
        }
        m->count++;
    } else {
        av_freep(&copy_key);
    }
    if (!m->count) {
        av_freep(&m->elems);
        av_freep(pm);
    }

    return 0;

err_out:
    if (m && !m->count) {
        av_freep(&m->elems);
        av_freep(pm);
    }
    av_free(copy_key);
    av_free(copy_value);
    return AVERROR(ENOMEM);
}

int av_dict_set_int(AVDictionary **pm, const char *key, int64_t value,
                int flags)
{
    char valuestr[22];
    snprintf(valuestr, sizeof(valuestr), "%"PRId64, value);
    flags &= ~AV_DICT_DONT_STRDUP_VAL;
    return av_dict_set(pm, key, valuestr, flags);
}

static int parse_key_value_pair(AVDictionary **pm, const char **buf,
                                const char *key_val_sep, const char *pairs_sep,
                                int flags)
{
    char *key = av_get_token(buf, key_val_sep);
    char *val = NULL;
    int ret;

    if (key && *key && strspn(*buf, key_val_sep)) {
        (*buf)++;
        val = av_get_token(buf, pairs_sep);
    }

    if (key && *key && val && *val)
        ret = av_dict_set(pm, key, val, flags);
    else
        ret = AVERROR(EINVAL);

    av_freep(&key);
    av_freep(&val);

    return ret;
}

int av_dict_parse_string(AVDictionary **pm, const char *str,
                         const char *key_val_sep, const char *pairs_sep,
                         int flags)
{
    int ret;

    if (!str)
        return 0;

    /* ignore STRDUP flags */
    flags &= ~(AV_DICT_DONT_STRDUP_KEY | AV_DICT_DONT_STRDUP_VAL);

    while (*str) {
        if ((ret = parse_key_value_pair(pm, &str, key_val_sep, pairs_sep, flags)) < 0)
            return ret;

        if (*str)
            str++;
    }

    return 0;
}

void av_dict_free(AVDictionary **pm)
{
    AVDictionary *m = *pm;

    if (m) {
        while (m->count--) {
            av_freep(&m->elems[m->count].key);
            av_freep(&m->elems[m->count].value);
        }
        av_freep(&m->elems);
    }
    av_freep(pm);
}

int pre_num(unsigned char byte) {
    unsigned char mask = 0x80;
    int num = 0;
    for (int i = 0; i < 8; i++) {
        if ((byte & mask) == mask) {
            mask = mask >> 1;
            num++;
        } else {
            break;
        }
    }
    return num;
}
 
int is_utf8(unsigned char* data, int len) {
    int num = 0;
    int i = 0;
    while (i < len) {
        if ((data[i] & 0x80) == 0x00) {
            i++;
            continue;
        } else if ((num = pre_num(data[i])) > 2) {   
            i++;
            for (int j = 0; j < num - 1; j++) {
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

int is_gbk(unsigned char* data, int len) {
    int i = 0;
    while (i < len) {
        if (data[i] <= 0x7f) {
            // 编码小于等于127,只有一个字节的编码，兼容ASCII
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

int is_ascii(unsigned char* data, int len) {
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

int is_iso(unsigned char* data, int len) {
    if (len < 4) {
        return 0;
    } else if ((data[0]==0xc2 || data[0]==0xc3)
          && (data[2]==0xc2 || data[2]==0xc3)) {
        return 0;
    } else {
        return -1;
    }
}

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

int av_dict_copy(AVDictionary **dst, const AVDictionary *src, int flags)
{
//    AVDictionaryEntry *t = NULL;
//
//    while ((t = av_dict_get(src, "", t, AV_DICT_IGNORE_SUFFIX))) {
//        int ret = av_dict_set(dst, t->key, t->value, flags);
//        if (ret < 0)
//            return ret;
//    }

    const AVDictionaryEntry *tag = NULL;
    int ret;
    while ((tag = av_dict_get(src, "", tag, AV_DICT_IGNORE_SUFFIX))) {
        size_t src_len = strlen(tag->value);
        size_t tmp_len = src_len;
        char *tmp_str = malloc(tmp_len+1);
        if (is_iso((unsigned char*)tag->value, src_len) == 0) {
            iso_to_data((unsigned char*)tag->value, src_len, tmp_str);
            tmp_len = strlen(tmp_str);
        } else {
            memcpy(tmp_str, tag->value, tmp_len);
            tmp_str[tmp_len] = 0;
        }
        if (is_ascii((unsigned char*)tmp_str, tmp_len)==0 || is_utf8((unsigned char*)tmp_str, tmp_len)==0) {
            ret = av_dict_set(dst, tag->key, tmp_str, AV_DICT_IGNORE_SUFFIX);
            if (ret < 0)
                return ret;
        } else {
            int dst_len = tmp_len*3/2;
            char *dst_str = malloc(dst_len+1);
            gbk_to_utf8(tmp_str, tmp_len, dst_str, dst_len);
            ret = av_dict_set(dst, tag->key, dst_str, AV_DICT_IGNORE_SUFFIX);
            if (ret < 0)
                return ret;
        }
        free(tmp_str);
    }

    return 0;
}

int av_dict_get_string(const AVDictionary *m, char **buffer,
                       const char key_val_sep, const char pairs_sep)
{
    AVDictionaryEntry *t = NULL;
    AVBPrint bprint;
    int cnt = 0;
    char special_chars[] = {pairs_sep, key_val_sep, '\0'};

    if (!buffer || pairs_sep == '\0' || key_val_sep == '\0' || pairs_sep == key_val_sep ||
        pairs_sep == '\\' || key_val_sep == '\\')
        return AVERROR(EINVAL);

    if (!av_dict_count(m)) {
        *buffer = av_strdup("");
        return *buffer ? 0 : AVERROR(ENOMEM);
    }

    av_bprint_init(&bprint, 64, AV_BPRINT_SIZE_UNLIMITED);
    while ((t = av_dict_get(m, "", t, AV_DICT_IGNORE_SUFFIX))) {
        if (cnt++)
            av_bprint_append_data(&bprint, &pairs_sep, 1);
        av_bprint_escape(&bprint, t->key, special_chars, AV_ESCAPE_MODE_BACKSLASH, 0);
        av_bprint_append_data(&bprint, &key_val_sep, 1);
        av_bprint_escape(&bprint, t->value, special_chars, AV_ESCAPE_MODE_BACKSLASH, 0);
    }
    return av_bprint_finalize(&bprint, buffer);
}

int avpriv_dict_set_timestamp(AVDictionary **dict, const char *key, int64_t timestamp)
{
    time_t seconds = timestamp / 1000000;
    struct tm *ptm, tmbuf;
    ptm = gmtime_r(&seconds, &tmbuf);
    if (ptm) {
        char buf[32];
        if (!strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", ptm))
            return AVERROR_EXTERNAL;
        av_strlcatf(buf, sizeof(buf), ".%06dZ", (int)(timestamp % 1000000));
        return av_dict_set(dict, key, buf, 0);
    } else {
        return AVERROR_EXTERNAL;
    }
}
