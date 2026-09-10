
#include "rtthread.h"
#include "bsp_uart.h"

/*************************************************************************MEMORY START*************************************************************************/
/**
 * @brief  This function will set the content of memory to specified value.
 *
 * @param  s is the address of source memory, point to the memory block to be filled.
 *
 * @param  c is the value to be set. The value is passed in int form, but the function
 *         uses the unsigned character form of the value when filling the memory block.
 *
 * @param  count number of bytes to be set.
 *
 * @return The address of source memory.
 */
rt_weak void *rt_memset(void *s, int c, rt_ubase_t count)
{
#ifdef RT_KSERVICE_USING_TINY_SIZE
    char *xs = (char *)s;

    while (count--)
        *xs++ = c;

    return s;
#else
#define LBLOCKSIZE      (sizeof(rt_ubase_t))
#define UNALIGNED(X)    ((long)X & (LBLOCKSIZE - 1))
#define TOO_SMALL(LEN)  ((LEN) < LBLOCKSIZE)

    unsigned int i = 0;
    char *m = (char *)s;
    unsigned long buffer = 0;
    unsigned long *aligned_addr = RT_NULL;
    unsigned char d = (unsigned int)c & (unsigned char)(-1);  /* To avoid sign extension, copy C to an
                                unsigned variable. (unsigned)((char)(-1))=0xFF for 8bit and =0xFFFF for 16bit: word independent */

    RT_ASSERT(LBLOCKSIZE == 2 || LBLOCKSIZE == 4 || LBLOCKSIZE == 8);

    if (!TOO_SMALL(count) && !UNALIGNED(s))
    {
        /* If we get this far, we know that count is large and s is word-aligned. */
        aligned_addr = (unsigned long *)s;

        /* Store d into each char sized location in buffer so that
         * we can set large blocks quickly.
         */
        for (i = 0; i < LBLOCKSIZE; i++)
        {
            *(((unsigned char *)&buffer)+i) = d;
        }

        while (count >= LBLOCKSIZE * 4)
        {
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            *aligned_addr++ = buffer;
            count -= 4 * LBLOCKSIZE;
        }

        while (count >= LBLOCKSIZE)
        {
            *aligned_addr++ = buffer;
            count -= LBLOCKSIZE;
        }

        /* Pick up the remainder with a bytewise loop. */
        m = (char *)aligned_addr;
    }

    while (count--)
    {
        *m++ = (char)d;
    }

    return s;

#undef LBLOCKSIZE
#undef UNALIGNED
#undef TOO_SMALL
#endif /* RT_KSERVICE_USING_TINY_SIZE */
}


/**
 * @brief  This function will copy memory content from source address to destination address.
 *
 * @param  dst is the address of destination memory, points to the copied content.
 *
 * @param  src  is the address of source memory, pointing to the data source to be copied.
 *
 * @param  count is the copied length.
 *
 * @return The address of destination memory
 */
rt_weak void *rt_memcpy(void *dst, const void *src, rt_ubase_t count)
{
#ifdef RT_KSERVICE_USING_TINY_SIZE
    char *tmp = (char *)dst, *s = (char *)src;
    rt_ubase_t len = 0;

    if (tmp <= s || tmp > (s + count))
    {
        while (count--)
            *tmp ++ = *s ++;
    }
    else
    {
        for (len = count; len > 0; len --)
            tmp[len - 1] = s[len - 1];
    }

    return dst;
#else

#define UNALIGNED(X, Y) \
    (((long)X & (sizeof (long) - 1)) | ((long)Y & (sizeof (long) - 1)))
#define BIGBLOCKSIZE    (sizeof (long) << 2)
#define LITTLEBLOCKSIZE (sizeof (long))
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)

    char *dst_ptr = (char *)dst;
    char *src_ptr = (char *)src;
    long *aligned_dst = RT_NULL;
    long *aligned_src = RT_NULL;
    rt_ubase_t len = count;

    /* If the size is small, or either SRC or DST is unaligned,
    then punt into the byte copy loop.  This should be rare. */
    if (!TOO_SMALL(len) && !UNALIGNED(src_ptr, dst_ptr))
    {
        aligned_dst = (long *)dst_ptr;
        aligned_src = (long *)src_ptr;

        /* Copy 4X long words at a time if possible. */
        while (len >= BIGBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            len -= BIGBLOCKSIZE;
        }

        /* Copy one long word at a time if possible. */
        while (len >= LITTLEBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            len -= LITTLEBLOCKSIZE;
        }

        /* Pick up any residual with a byte copier. */
        dst_ptr = (char *)aligned_dst;
        src_ptr = (char *)aligned_src;
    }

    while (len--)
        *dst_ptr++ = *src_ptr++;

    return dst;
#undef UNALIGNED
#undef BIGBLOCKSIZE
#undef LITTLEBLOCKSIZE
#undef TOO_SMALL
#endif /* RT_KSERVICE_USING_TINY_SIZE */
}

/**
 * @brief  This function will move memory content from source address to destination
 * address. If the destination memory does not overlap with the source memory,
 * the function is the same as memcpy().
 *
 * @param  dest is the address of destination memory, points to the copied content.
 *
 * @param  src is the address of source memory, point to the data source to be copied.
 *
 * @param  n is the copied length.
 *
 * @return The address of destination memory.
 */
void *rt_memmove(void *dest, const void *src, rt_size_t n)
{
    char *tmp = (char *)dest, *s = (char *)src;

    if (s < tmp && tmp < s + n)
    {
        tmp += n;
        s += n;

        while (n--)
            *(--tmp) = *(--s);
    }
    else
    {
        while (n--)
            *tmp++ = *s++;
    }

    return dest;
}

/**
 * @brief  This function will compare two areas of memory.
 *
 * @param  cs is a block of memory.
 *
 * @param  ct is another block of memory.
 *
 * @param  count is the size of the area.
 *
 * @return Compare the results:
 *         If the result < 0, cs is smaller than ct.
 *         If the result > 0, cs is greater than ct.
 *         If the result = 0, cs is equal to ct.
 */
rt_int32_t rt_memcmp(const void *cs, const void *ct, rt_size_t count)
{
    const unsigned char *su1 = RT_NULL, *su2 = RT_NULL;
    int res = 0;

    for (su1 = (const unsigned char *)cs, su2 = (const unsigned char *)ct; 0 < count; ++su1, ++su2, count--)
        if ((res = *su1 - *su2) != 0)
            break;

    return res;
}

/**
 * @brief  This function will return the first occurrence of a string, without the
 * terminator '\0'.
 *
 * @param  s1 is the source string.
 *
 * @param  s2 is the find string.
 *
 * @return The first occurrence of a s2 in s1, or RT_NULL if no found.
 */
char *rt_strstr(const char *s1, const char *s2)
{
    int l1 = 0, l2 = 0;

    l2 = rt_strlen(s2);
    if (!l2)
    {
        return (char *)s1;
    }

    l1 = rt_strlen(s1);
    while (l1 >= l2)
    {
        l1 --;
        if (!rt_memcmp(s1, s2, l2))
        {
            return (char *)s1;
        }

        s1 ++;
    }

    return RT_NULL;
}


/**
 * @brief  This function will compare two strings while ignoring differences in case
 *
 * @param  a is the string to be compared.
 *
 * @param  b is the string to be compared.
 *
 * @return Compare the results:
 *         If the result < 0, a is smaller than a.
 *         If the result > 0, a is greater than a.
 *         If the result = 0, a is equal to a.
 */
rt_int32_t rt_strcasecmp(const char *a, const char *b)
{
    int ca = 0, cb = 0;

    do
    {
        ca = *a++ & 0xff;
        cb = *b++ & 0xff;
        if (ca >= 'A' && ca <= 'Z')
            ca += 'a' - 'A';
        if (cb >= 'A' && cb <= 'Z')
            cb += 'a' - 'A';
    }
    while (ca == cb && ca != '\0');

    return ca - cb;
}

/**
 * @brief  This function will copy string no more than n bytes.
 *
 * @param  dst points to the address used to store the copied content.
 *
 * @param  src is the string to be copied.
 *
 * @param  n is the maximum copied length.
 *
 * @return The address where the copied content is stored.
 */
char *rt_strncpy(char *dst, const char *src, rt_size_t n)
{
    if (n != 0)
    {
        char *d = dst;
        const char *s = src;

        do
        {
            if ((*d++ = *s++) == 0)
            {
                /* NUL pad the remaining n-1 bytes */
                while (--n != 0)
                {
                    *d++ = 0;
                }

                break;
            }
        } while (--n != 0);
    }

    return (dst);
}

/**
 * @brief  This function will copy string.
 *
 * @param  dst points to the address used to store the copied content.
 *
 * @param  src is the string to be copied.
 *
 * @return The address where the copied content is stored.
 */
char *rt_strcpy(char *dst, const char *src)
{
    char *dest = dst;

    while (*src != '\0')
    {
        *dst = *src;
        dst++;
        src++;
    }

    *dst = '\0';
    return dest;
}

/**
 * @brief  This function will compare two strings with specified maximum length.
 *
 * @param  cs is the string to be compared.
 *
 * @param  ct is the string to be compared.
 *
 * @param  count is the maximum compare length.
 *
 * @return Compare the results:
 *         If the result < 0, cs is smaller than ct.
 *         If the result > 0, cs is greater than ct.
 *         If the result = 0, cs is equal to ct.
 */
rt_int32_t rt_strncmp(const char *cs, const char *ct, rt_size_t count)
{
    signed char __res = 0;

    while (count)
    {
        if ((__res = *cs - *ct++) != 0 || !*cs++)
        {
            break;
        }

        count --;
    }

    return __res;
}

/**
 * @brief  This function will compare two strings without specified length.
 *
 * @param  cs is the string to be compared.
 *
 * @param  ct is the string to be compared.
 *
 * @return Compare the results:
 *         If the result < 0, cs is smaller than ct.
 *         If the result > 0, cs is greater than ct.
 *         If the result = 0, cs is equal to ct.
 */
rt_int32_t rt_strcmp(const char *cs, const char *ct)
{
    while (*cs && *cs == *ct)
    {
        cs++;
        ct++;
    }

    return (*cs - *ct);
}

/**
 * @brief  This function will return the length of a string, which terminate will
 * null character.
 *
 * @param  s is the string
 *
 * @return The length of string.
 */
rt_size_t rt_strlen(const char *s)
{
    const char *sc = RT_NULL;

    for (sc = s; *sc != '\0'; ++sc) /* nothing */
        ;

    return sc - s;
}

/**
 * @brief  The  strnlen()  function  returns the number of characters in the
 * string pointed to by s, excluding the terminating null byte ('\0'),
 * but at most maxlen.  In doing this, strnlen() looks only at the
 * first maxlen characters in the string pointed to by s and never
 * beyond s+maxlen.
 *
 * @param  s is the string.
 *
 * @param  maxlen is the max size.
 *
 * @return The length of string.
 */
rt_size_t rt_strnlen(const char *s, rt_ubase_t maxlen)
{
    const char *sc;

    for (sc = s; *sc != '\0' && (rt_ubase_t)(sc - s) < maxlen; ++sc) /* nothing */
        ;

    return sc - s;
}







/*************************************************************************MEMORY END*************************************************************************/


/************************************************************************USE PRINTF DEFINE FUNCTION START*****************************************************************************/
/**
 * @brief This function will show the version of rt-thread rtos
 */
void rt_show_version(void)
{
    rt_kprintf("\n \\ | /\n");
#ifdef RT_USING_SMART
    rt_kprintf("- RT -     Thread Smart Operating System\n");
#else
    rt_kprintf("- RT -     Thread Operating System\n");
#endif
    rt_kprintf(" / | \\     %d.%d.%d build %s %s\n",
               (rt_int32_t)RT_VERSION_MAJOR, (rt_int32_t)RT_VERSION_MINOR, (rt_int32_t)RT_VERSION_PATCH, __DATE__, __TIME__);
    rt_kprintf(" 2006 - 2022 Copyright by RT-Thread team\n");
}

/* private function */
#define _ISDIGIT(c)  ((unsigned)((c) - '0') < 10)

/**
 * @brief  This function will duplicate a string.
 *
 * @param  n is the string to be duplicated.
 *
 * @param  base is support divide instructions value.
 *
 * @return the duplicated string pointer.
 */
#ifdef RT_KPRINTF_USING_LONGLONG
rt_inline int divide(unsigned long long *n, int base)
#else
rt_inline int divide(unsigned long *n, int base)
#endif /* RT_KPRINTF_USING_LONGLONG */
{
    int res;

    /* optimized for processor which does not support divide instructions. */
#ifdef RT_KPRINTF_USING_LONGLONG
    res = (int)((*n) % base);
    *n = (long long)((*n) / base);
#else
    res = (int)((*n) % base);
    *n = (long)((*n) / base);
#endif

    return res;
}


/* rt_vsnprintf used */
rt_inline int skip_atoi(const char **s)
{
    int i = 0;
    while (_ISDIGIT(**s))
        i = i * 10 + *((*s)++) - '0';

    return i;
}

#define ZEROPAD     (1 << 0)    /* pad with zero */
#define SIGN        (1 << 1)    /* unsigned/signed long */
#define PLUS        (1 << 2)    /* show plus */
#define SPACE       (1 << 3)    /* space if plus */
#define LEFT        (1 << 4)    /* left justified */
#define SPECIAL     (1 << 5)    /* 0x */
#define LARGE       (1 << 6)    /* use 'ABCDEF' instead of 'abcdef' */


/* rt_vsnprintf used */
static char *print_number(char *buf,
                          char *end,
#ifdef RT_KPRINTF_USING_LONGLONG
                          unsigned long long  num,
#else
                          unsigned long  num,
#endif /* RT_KPRINTF_USING_LONGLONG */
                          int   base,
                          int   qualifier,
                          int   s,
#ifdef RT_PRINTF_PRECISION
                          int   precision,
#endif /* RT_PRINTF_PRECISION */
                          int   type)
{
    char c = 0, sign = 0;
#ifdef RT_KPRINTF_USING_LONGLONG
    char tmp[64] = {0};
#else
    char tmp[32] = {0};
#endif /* RT_KPRINTF_USING_LONGLONG */
    int precision_bak = precision;
    const char *digits = RT_NULL;
    static const char small_digits[] = "0123456789abcdef";
    static const char large_digits[] = "0123456789ABCDEF";
    int i = 0;
    int size = 0;

    size = s;

    digits = (type & LARGE) ? large_digits : small_digits;
    if (type & LEFT)
    {
        type &= ~ZEROPAD;
    }

    c = (type & ZEROPAD) ? '0' : ' ';

    /* get sign */
    sign = 0;
    if (type & SIGN)
    {
        switch (qualifier)
        {
        case 'h':
            if ((rt_int16_t)num < 0)
            {
                sign = '-';
                num = (rt_uint16_t)-num;
            }
            break;
        case 'L':
        case 'l':
            if ((long)num < 0)
            {
                sign = '-';
                num = (unsigned long)-num;
            }
            break;
        case 0:
        default:
            if ((rt_int32_t)num < 0)
            {
                sign = '-';
                num = (rt_uint32_t)-num;
            }
            break;
        }

        if (sign != '-')
        {
            if (type & PLUS)
            {
                sign = '+';
            }
            else if (type & SPACE)
            {
                sign = ' ';
            }
        }
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 2 || base == 16)
        {
            size -= 2;
        }
        else if (base == 8)
        {
            size--;
        }
    }
#endif /* RT_PRINTF_SPECIAL */

    i = 0;
    if (num == 0)
    {
        tmp[i++] = '0';
    }
    else
    {
        while (num != 0)
            tmp[i++] = digits[divide(&num, base)];
    }

#ifdef RT_PRINTF_PRECISION
    if (i > precision)
    {
        precision = i;
    }
    size -= precision;
#else
    size -= i;
#endif /* RT_PRINTF_PRECISION */

    if (!(type & (ZEROPAD | LEFT)))
    {
        if ((sign) && (size > 0))
        {
            size--;
        }

        while (size-- > 0)
        {
            if (buf < end)
            {
                *buf = ' ';
            }

            ++ buf;
        }
    }

    if (sign)
    {
        if (buf < end)
        {
            *buf = sign;
        }
        -- size;
        ++ buf;
    }

#ifdef RT_PRINTF_SPECIAL
    if (type & SPECIAL)
    {
        if (base == 2)
        {
            if (buf < end)
                *buf = '0';
            ++ buf;
            if (buf < end)
                *buf = 'b';
            ++ buf;
        }
        else if (base == 8)
        {
            if (buf < end)
                *buf = '0';
            ++ buf;
        }
        else if (base == 16)
        {
            if (buf < end)
            {
                *buf = '0';
            }

            ++ buf;
            if (buf < end)
            {
                *buf = type & LARGE ? 'X' : 'x';
            }
            ++ buf;
        }
    }
#endif /* RT_PRINTF_SPECIAL */

    /* no align to the left */
    if (!(type & LEFT))
    {
        while (size-- > 0)
        {
            if (buf < end)
            {
                *buf = c;
            }

            ++ buf;
        }
    }

#ifdef RT_PRINTF_PRECISION
    while (i < precision--)
    {
        if (buf < end)
        {
            *buf = '0';
        }

        ++ buf;
    }
#endif /* RT_PRINTF_PRECISION */

    /* put number in the temporary buffer */
    while (i-- > 0 && (precision_bak != 0))
    {
        if (buf < end)
        {
            *buf = tmp[i];
        }

        ++ buf;
    }

    while (size-- > 0)
    {
        if (buf < end)
        {
            *buf = ' ';
        }

        ++ buf;
    }

    return buf;
}

/**
 * @brief  This function will fill a formatted string to buffer.
 *
 * @param  buf is the buffer to save formatted string.
 *
 * @param  size is the size of buffer.
 *
 * @param  fmt is the format parameters.
 *
 * @param  args is a list of variable parameters.
 *
 * @return The number of characters actually written to buffer.
 */
rt_weak int rt_vsnprintf(char *buf, rt_size_t size, const char *fmt, va_list args)
{

#ifdef RT_KPRINTF_USING_LONGLONG
    unsigned long long num = 0;
#else
    unsigned long num = 0;
#endif /* RT_KPRINTF_USING_LONGLONG */
    int i = 0, len = 0;
    char *str = RT_NULL, *end = RT_NULL, c = 0;
    const char *s = RT_NULL;

    rt_uint8_t base = 0;            /* the base of number */
    rt_uint8_t flags = 0;           /* flags to print number */
    rt_uint8_t qualifier = 0;       /* 'h', 'l', or 'L' for integer fields */
    rt_int32_t field_width = 0;     /* width of output field */

#ifdef RT_PRINTF_PRECISION
    int precision = 0;      /* min. # of digits for integers and max for a string */
#endif /* RT_PRINTF_PRECISION */

    str = buf;
    end = buf + size;

    /* Make sure end is always >= buf */
    if (end < buf)
    {
        end  = ((char *) - 1);
        size = end - buf;
    }

    for (; *fmt ; ++fmt)
    {
        if (*fmt != '%')
        {
            if (str < end)
            {
                *str = *fmt;
            }

            ++ str;
            continue;
        }

        /* process flags */
        flags = 0;

        while (1)
        {
            /* skips the first '%' also */
            ++ fmt;
            if (*fmt == '-') flags |= LEFT;
            else if (*fmt == '+') flags |= PLUS;
            else if (*fmt == ' ') flags |= SPACE;
            else if (*fmt == '#') flags |= SPECIAL;
            else if (*fmt == '0') flags |= ZEROPAD;
            else break;
        }

        /* get field width */
        field_width = -1;
        if (_ISDIGIT(*fmt))
        {
            field_width = skip_atoi(&fmt);
        }
        else if (*fmt == '*')
        {
            ++ fmt;
            /* it's the next argument */
            field_width = va_arg(args, int);
            if (field_width < 0)
            {
                field_width = -field_width;
                flags |= LEFT;
            }
        }

#ifdef RT_PRINTF_PRECISION
        /* get the precision */
        precision = -1;
        if (*fmt == '.')
        {
            ++ fmt;
            if (_ISDIGIT(*fmt))
            {
                precision = skip_atoi(&fmt);
            }
            else if (*fmt == '*')
            {
                ++ fmt;
                /* it's the next argument */
                precision = va_arg(args, int);
            }
            if (precision < 0)
            {
                precision = 0;
            }
        }
#endif /* RT_PRINTF_PRECISION */
        /* get the conversion qualifier */
        qualifier = 0;
#ifdef RT_KPRINTF_USING_LONGLONG
        if (*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
#else
        if (*fmt == 'h' || *fmt == 'l')
#endif /* RT_KPRINTF_USING_LONGLONG */
        {
            qualifier = *fmt;
            ++ fmt;
#ifdef RT_KPRINTF_USING_LONGLONG
            if (qualifier == 'l' && *fmt == 'l')
            {
                qualifier = 'L';
                ++ fmt;
            }
#endif /* RT_KPRINTF_USING_LONGLONG */
        }

        /* the default base */
        base = 10;

        switch (*fmt)
        {
        case 'c':
            if (!(flags & LEFT))
            {
                while (--field_width > 0)
                {
                    if (str < end) *str = ' ';
                    ++ str;
                }
            }

            /* get character */
            c = (rt_uint8_t)va_arg(args, int);
            if (str < end)
            {
                *str = c;
            }
            ++ str;

            /* put width */
            while (--field_width > 0)
            {
                if (str < end) *str = ' ';
                ++ str;
            }
            continue;

        case 's':
            s = va_arg(args, char *);
            if (!s)
            {
                s = "(NULL)";
            }

            for (len = 0; (len != field_width) && (s[len] != '\0'); len++);
#ifdef RT_PRINTF_PRECISION
            if (precision > 0 && len > precision)
            {
                len = precision;
            }
#endif /* RT_PRINTF_PRECISION */

            if (!(flags & LEFT))
            {
                while (len < field_width--)
                {
                    if (str < end) *str = ' ';
                    ++ str;
                }
            }

            for (i = 0; i < len; ++i)
            {
                if (str < end) *str = *s;
                ++ str;
                ++ s;
            }

            while (len < field_width--)
            {
                if (str < end) *str = ' ';
                ++ str;
            }
            continue;

        case 'p':
            if (field_width == -1)
            {
                field_width = sizeof(void *) << 1;
#ifdef RT_PRINTF_SPECIAL
                field_width += 2; /* `0x` prefix */
                flags |= SPECIAL;
#endif
                flags |= ZEROPAD;
            }
#ifdef RT_PRINTF_PRECISION
            str = print_number(str, end,
                               (unsigned long)va_arg(args, void *),
                               16, qualifier, field_width, precision, flags);
#else
            str = print_number(str, end,
                               (unsigned long)va_arg(args, void *),
                               16, qualifier, field_width, flags);
#endif
            continue;

        case '%':
            if (str < end)
            {
                *str = '%';
            }
            ++ str;
            continue;

        /* integer number formats - set up the flags and "break" */
        case 'b':
            base = 2;
            break;
        case 'o':
            base = 8;
            break;

        case 'X':
            flags |= LARGE;
        case 'x':
            base = 16;
            break;

        case 'd':
        case 'i':
            flags |= SIGN;
        case 'u':
            break;

        default:
            if (str < end)
            {
                *str = '%';
            }
            ++ str;

            if (*fmt)
            {
                if (str < end)
                {
                    *str = *fmt;
                }
                ++ str;
            }
            else
            {
                -- fmt;
            }
            continue;
        }

#ifdef RT_KPRINTF_USING_LONGLONG
        if (qualifier == 'L')
        {
            num = va_arg(args, unsigned long long);
        }
        else if (qualifier == 'l')
#else
        if (qualifier == 'l')
#endif /* RT_KPRINTF_USING_LONGLONG */
        {
            num = va_arg(args, unsigned long);
        }
        else if (qualifier == 'h')
        {
            num = (rt_uint16_t)va_arg(args, rt_int32_t);
            if (flags & SIGN)
            {
                num = (rt_int16_t)num;
            }
        }
        else
        {
            num = (rt_uint32_t)va_arg(args, unsigned long);
        }
#ifdef RT_PRINTF_PRECISION
        str = print_number(str, end, num, base, qualifier, field_width, precision, flags);
#else
        str = print_number(str, end, num, base, qualifier, field_width, flags);
#endif
    }

    if (size > 0)
    {
        if (str < end)
        {
            *str = '\0';
        }
        else
        {
            end[-1] = '\0';
        }
    }

    /* the trailing null byte doesn't count towards the total
    * ++str;
    */
    return str - buf;
}


/**
 * @brief  This function will fill a formatted string to buffer.
 *
 * @param  buf is the buffer to save formatted string.
 *
 * @param  size is the size of buffer.
 *
 * @param  fmt is the format parameters.
 *
 * @return The number of characters actually written to buffer.
 */
int rt_snprintf(char *buf, rt_size_t size, const char *fmt, ...)
{
    rt_int32_t n = 0;
    va_list args;

    va_start(args, fmt);
    n = rt_vsnprintf(buf, size, fmt, args);
    va_end(args);

    return n;
}

/**
 * @brief  This function will fill a formatted string to buffer.
 *
 * @param  buf is the buffer to save formatted string.
 *
 * @param  format is the format parameters.
 *
 * @param  arg_ptr is a list of variable parameters.
 *
 * @return The number of characters actually written to buffer.
 */
int rt_vsprintf(char *buf, const char *format, va_list arg_ptr)
{
    return rt_vsnprintf(buf, (rt_size_t) - 1, format, arg_ptr);
}

/**
 * @brief  This function will fill a formatted string to buffer
 *
 * @param  buf the buffer to save formatted string.
 *
 * @param  format is the format parameters.
 *
 * @return The number of characters actually written to buffer.
 */
int rt_sprintf(char *buf, const char *format, ...)
{
    rt_int32_t n = 0;
    va_list arg_ptr;

    va_start(arg_ptr, format);
    n = rt_vsprintf(buf, format, arg_ptr);
    va_end(arg_ptr);

    return n;
}


rt_weak void rt_hw_console_output(const char *str)
{
    /* empty console output */

    myputstr(str);

}

/**
 * @brief This function will put string to the console.
 *
 * @param str is the string output to the console.
 */
void rt_kputs(const char *str)
{
    if (!str)
    {
        return;
    }

    rt_hw_console_output(str);

}

/**
 * @brief This function will print a formatted string on system console.
 *
 * @param fmt is the format parameters.
 *
 * @return The number of characters actually written to buffer.
 */
rt_weak int rt_kprintf(const char *fmt, ...)
{
    va_list args;
    rt_size_t length = 0;
    static char rt_log_buf[RT_CONSOLEBUF_SIZE];

    va_start(args, fmt);
    /* the return value of vsnprintf is the number of bytes that would be
     * written to buffer had if the size of the buffer been sufficiently
     * large excluding the terminating null byte. If the output string
     * would be larger than the rt_log_buf, we have to adjust the output
     * length. */
    length = rt_vsnprintf(rt_log_buf, sizeof(rt_log_buf) - 1, fmt, args);
    if (length > RT_CONSOLEBUF_SIZE - 1)
    {
        length = RT_CONSOLEBUF_SIZE - 1;
    }

    rt_hw_console_output(rt_log_buf);

    va_end(args);

    return length;
}
/************************************************************************USE PRINTF DEFINE FUNCTION END*****************************************************************************/