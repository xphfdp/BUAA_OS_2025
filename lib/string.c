#include <types.h>

void *memcpy(void *dst, const void *src, size_t n) {
	void *dstaddr = dst;
	void *max = dst + n;

	if (((u_long)src & 3) != ((u_long)dst & 3)) {
		while (dst < max) {
			*(char *)dst++ = *(char *)src++;
		}
		return dstaddr;
	}

	while (((u_long)dst & 3) && dst < max) {
		*(char *)dst++ = *(char *)src++;
	}

	// copy machine words while possible
	while (dst + 4 <= max) {
		*(uint32_t *)dst = *(uint32_t *)src;
		dst += 4;
		src += 4;
	}

	// finish the remaining 0-3 bytes
	while (dst < max) {
		*(char *)dst++ = *(char *)src++;
	}
	return dstaddr;
}

void *memset(void *dst, int c, size_t n) {
	void *dstaddr = dst;
	void *max = dst + n;
	u_char byte = c & 0xff;
	uint32_t word = byte | byte << 8 | byte << 16 | byte << 24;

	while (((u_long)dst & 3) && dst < max) {
		*(u_char *)dst++ = byte;
	}

	// fill machine words while possible
	while (dst + 4 <= max) {
		*(uint32_t *)dst = word;
		dst += 4;
	}

	// finish the remaining 0-3 bytes
	while (dst < max) {
		*(u_char *)dst++ = byte;
	}
	return dstaddr;
}

size_t strlen(const char *s) {
	int n;

	for (n = 0; *s; s++) {
		n++;
	}

	return n;
}

char *strcpy(char *dst, const char *src) {
	char *ret = dst;

	while ((*dst++ = *src++) != 0) {
	}

	return ret;
}

const char *strchr(const char *s, int c) {
	for (; *s; s++) {
		if (*s == c) {
			return s;
		}
	}
	return 0;
}

int strcmp(const char *p, const char *q) {
	while (*p && *p == *q) {
		p++, q++;
	}

	if ((u_int)*p < (u_int)*q) {
		return -1;
	}

	if ((u_int)*p > (u_int)*q) {
		return 1;
	}

	return 0;
}

char *strcat(char *dst, const char *src) {
	char *ret = dst;

	while (*dst++);
	dst--;
	while ((*dst++ = *src++) != 0) {
	}

	return ret;
}

const char *strrchr(const char *s, int c) {
	const char *last = 0;

	for (; *s; s++) {
		if (*s == c) {
			last = s;
		}
	}

	return last;
}

char *strncpy(char *dst, const char *src, size_t n) {
	char *ret = dst;
	size_t i;

	for(i = 0; i < n && *src; i++) {
		*dst++ = *src++;
	}
	for (; i < n; i++) {
		*dst++ = 0; // pad with null bytes
	}

	return ret;
}

int isalnum(int c) {
	if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
		return 1;
	}
	return 0;
}

char *strncat(char *dest, const char *src, size_t n)
{
	char *tmp = dest;

	if (n) {
        /* 找到目标字符串结尾 */
		while (*dest)
			dest++;
        /* 从src字符串中追加n个字符到dest中 */
		while ((*dest++ = *src++) != 0) {
            /* n减小到等于0时，提前结束循环 */
			if (--n == 0) {
				*dest = '\0';
				break;
			}
		}
	}
	return tmp;
}

int strncmp(const char *str1, const char *str2, size_t n)
{
    while (n) {
        if (*str1 != *str2)
            return (*str1 - *str2);
        if (!*str1)
            break;
        str1++;
        str2++;
        n--;
    }

    return 0;
}

char *strpbrk(const char *str1, const char *str2)
{
	const char *sc1, *sc2;

	for (sc1 = str1; *sc1 != '\0'; sc1++) {
		for (sc2 = str2; *sc2 != '\0'; sc2++) {
			if (*sc1 == *sc2)
				return (char *)sc1;
		}
	}
	return NULL;
}

char *strreplace(char *str, char old, char new)
{
    char *tmp = str;
	for (; *tmp; tmp++)
		if (*tmp == old)
			*tmp = new;
	return str;
}

void* memmove(void* dest, void* src, size_t num) 
{
    //dest落在了src的左边，从前往后拷贝
    //dest落在了src的右边，同时没有超过那个重叠的边界的时候，从后往前拷贝
    void* rest = dest;
    // void* 不能直接解引用，那么如何复制呢？
    // 给了num个字节，也就是需要复制num个字节
    // 那就转换成char*，一个一个字节的复制过去
    if (dest < src) 
    //if (dest < src || dest > (char*)src + num) 
    {
    	//dest落在了src的左边，从前往后拷
    	while (num--)
    	{
    	    *(char*)dest = *(char*)src;
    	    //++(char*)dest;
    	    //++(char*)src;
    	    (char*)dest++;
    	    (char*)src++;
    	}
    }
    else 
    {
    	// 从后往前拷
    	// 找到最后一个字节
    	while (num--) 
    	{
    	    *((char*)dest + num) = *((char*)src + num);
    	}

    }
    return rest;
}


int isalpha(int c) {
    // 判断字符是否在大写字母范围内 ('A' 到 'Z')
    if (c >= 'A' && c <= 'Z') {
        return 1;
    }
    // 判断字符是否在小写字母范围内 ('a' 到 'z')
    if (c >= 'a' && c <= 'z') {
        return 1;
    }
    // 如果都不满足，则返回 false
    return 0;
}