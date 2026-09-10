#ifndef RTTHREAD_H_
#define RTTHREAD_H_


#ifdef __cplusplus
extern "C" {
#endif




/* RT-Thread version information */
#define RT_VERSION_MAJOR                5               /**< Major version number (X.x.x) */
#define RT_VERSION_MINOR                0               /**< Minor version number (x.X.x) */
#define RT_VERSION_PATCH                1               /**< Patch version number (x.x.X) */

/* e.g. #if (RTTHREAD_VERSION >= RT_VERSION_CHECK(4, 1, 0) */
#define RT_VERSION_CHECK(major, minor, revise)          ((major * 10000) + (minor * 100) + revise)

/* RT-Thread version */
#define RTTHREAD_VERSION                RT_VERSION_CHECK(RT_VERSION_MAJOR, RT_VERSION_MINOR, RT_VERSION_PATCH)



/* RT-Thread basic data type definitions */
typedef int                             rt_bool_t;      /**< boolean type */
typedef signed long                     rt_base_t;      /**< Nbit CPU related date type */
typedef unsigned long                   rt_ubase_t;     /**< Nbit unsigned CPU related data type */

#ifndef RT_USING_ARCH_DATA_TYPE
#ifdef RT_USING_LIBC
typedef int8_t                          rt_int8_t;      /**<  8bit integer type */
typedef int16_t                         rt_int16_t;     /**< 16bit integer type */
typedef int32_t                         rt_int32_t;     /**< 32bit integer type */
typedef uint8_t                         rt_uint8_t;     /**<  8bit unsigned integer type */
typedef uint16_t                        rt_uint16_t;    /**< 16bit unsigned integer type */
typedef uint32_t                        rt_uint32_t;    /**< 32bit unsigned integer type */
typedef int64_t                         rt_int64_t;     /**< 64bit integer type */
typedef uint64_t                        rt_uint64_t;    /**< 64bit unsigned integer type */
typedef size_t                          rt_size_t;      /**< Type for size number */
typedef ssize_t                         rt_ssize_t;     /**< Used for a count of bytes or an error indication */
#else
typedef signed   char                   rt_int8_t;      /**<  8bit integer type */
typedef signed   short                  rt_int16_t;     /**< 16bit integer type */
typedef signed   int                    rt_int32_t;     /**< 32bit integer type */
typedef unsigned char                   rt_uint8_t;     /**<  8bit unsigned integer type */
typedef unsigned short                  rt_uint16_t;    /**< 16bit unsigned integer type */
typedef unsigned int                    rt_uint32_t;    /**< 32bit unsigned integer type */
#ifdef ARCH_CPU_64BIT
typedef signed long                     rt_int64_t;     /**< 64bit integer type */
typedef unsigned long                   rt_uint64_t;    /**< 64bit unsigned integer type */
#else
typedef signed long long                rt_int64_t;     /**< 64bit integer type */
typedef unsigned long long              rt_uint64_t;    /**< 64bit unsigned integer type */
#endif /* ARCH_CPU_64BIT */
typedef rt_ubase_t                      rt_size_t;      /**< Type for size number */
typedef rt_base_t                       rt_ssize_t;     /**< Used for a count of bytes or an error indication */
#endif /* RT_USING_LIBC */
#endif /* RT_USING_ARCH_DATA_TYPE */

typedef rt_base_t                       rt_err_t;       /**< Type for error number */
typedef rt_uint32_t                     rt_time_t;      /**< Type for time stamp */
typedef rt_uint32_t                     rt_tick_t;      /**< Type for tick count */
typedef rt_base_t                       rt_flag_t;      /**< Type for flags */
typedef rt_ubase_t                      rt_dev_t;       /**< Type for device */
typedef rt_base_t                       rt_off_t;       /**< Type for offset */

/* boolean type definitions */
#define RT_TRUE                         1               /**< boolean true  */
#define RT_FALSE                        0               /**< boolean fails */

/* null pointer definition */
#define RT_NULL                         0

/* 为了保持独立性，不能调用C库里面的函数,所以不能定义  RT_USING_LIBC */


/* Common Utilities */

#define RT_UNUSED(x)                   ((void)x)

/* compile time assertion */
#define RT_CTASSERT(name, expn) typedef char _ct_assert_##name[(expn)?1:-1]

/* Compiler Related Definitions */
#if defined(__ARMCC_VERSION)           /* ARM Compiler */
#define rt_section(x)               __attribute__((section(x)))
#define rt_used                     __attribute__((used))
#define rt_align(n)                 __attribute__((aligned(n)))
#define rt_weak                     __attribute__((weak))
#define rt_inline                   static __inline
/* module compiling */
#ifdef RT_USING_MODULE
#define RTT_API                     __declspec(dllimport)
#else
#define RTT_API                     __declspec(dllexport)
#endif /* RT_USING_MODULE */
#elif defined (__IAR_SYSTEMS_ICC__)     /* for IAR Compiler */
#define rt_section(x)               @ x
#define rt_used                     __root
#define PRAGMA(x)                   _Pragma(#x)
#define rt_align(n)                    PRAGMA(data_alignment=n)
#define rt_weak                     __weak
#define rt_inline                   static inline
#define RTT_API
#elif defined (__GNUC__)                /* GNU GCC Compiler */
#ifndef RT_USING_LIBC
/* the version of GNU GCC must be greater than 4.x */
typedef __builtin_va_list           __gnuc_va_list;
typedef __gnuc_va_list              va_list;
#define va_start(v,l)               __builtin_va_start(v,l)
#define va_end(v)                   __builtin_va_end(v)
#define va_arg(v,l)                 __builtin_va_arg(v,l)
#endif /* RT_USING_LIBC */
#define __RT_STRINGIFY(x...)        #x
#define RT_STRINGIFY(x...)          __RT_STRINGIFY(x)
#define rt_section(x)               __attribute__((section(x)))
#define rt_used                     __attribute__((used))
#define rt_align(n)                 __attribute__((aligned(n)))
#define rt_weak                     __attribute__((weak))
#define rt_inline                   static __inline
#define RTT_API
#elif defined (__ADSPBLACKFIN__)        /* for VisualDSP++ Compiler */
#define rt_section(x)               __attribute__((section(x)))
#define rt_used                     __attribute__((used))
#define rt_align(n)                 __attribute__((aligned(n)))
#define rt_weak                     __attribute__((weak))
#define rt_inline                   static inline
#define RTT_API
#elif defined (_MSC_VER)
#define rt_section(x)
#define rt_used
#define rt_align(n)                 __declspec(align(n))
#define rt_weak
#define rt_inline                   static __inline
#define RTT_API
#elif defined (__TI_COMPILER_VERSION__)
/* The way that TI compiler set section is different from other(at least
    * GCC and MDK) compilers. See ARM Optimizing C/C++ Compiler 5.9.3 for more
    * details. */
#define rt_section(x)               __attribute__((section(x)))
#ifdef __TI_EABI__
#define rt_used                     __attribute__((retain)) __attribute__((used))
#else
#define rt_used                     __attribute__((used))
#endif
#define PRAGMA(x)                   _Pragma(#x)
#define rt_align(n)                 __attribute__((aligned(n)))
#ifdef __TI_EABI__
#define rt_weak                     __attribute__((weak))
#else
#define rt_weak
#endif
#define rt_inline                   static inline
#define RTT_API
#elif defined (__TASKING__)
#define rt_section(x)               __attribute__((section(x)))
#define rt_used                     __attribute__((used, protect))
#define PRAGMA(x)                   _Pragma(#x)
#define rt_align(n)                 __attribute__((__align(n)))
#define rt_weak                     __attribute__((weak))
#define rt_inline                   static inline
#define RTT_API
#else
    #error not supported tool chain
#endif /* __ARMCC_VERSION */




/**
 * @addtogroup Error
 */

/**@{*/

/* RT-Thread error code definitions */
#define RT_EOK                          0               /**< There is no error */
#define RT_ERROR                        1               /**< A generic error happens */
#define RT_ETIMEOUT                     2               /**< Timed out */
#define RT_EFULL                        3               /**< The resource is full */
#define RT_EEMPTY                       4               /**< The resource is empty */
#define RT_ENOMEM                       5               /**< No memory */
#define RT_ENOSYS                       6               /**< No system */
#define RT_EBUSY                        7               /**< Busy */
#define RT_EIO                          8               /**< IO error */
#define RT_EINTR                        9               /**< Interrupted system call */
#define RT_EINVAL                       10              /**< Invalid argument */
#define RT_ETRAP                        11              /**< Trap event */
#define RT_ENOENT                       12              /**< No entry */
#define RT_ENOSPC                       13              /**< No space left */



/*-------------------------------------------------------memory define start-----------------------------------------*/

/* 一般复制数据量都很小，满足基本使用，可以开启 RT_KSERVICE_USING_TINY_SIZE 这个定义 */
#define RT_KSERVICE_USING_TINY_SIZE

rt_weak void *rt_memset(void *s, int c, rt_ubase_t count);
rt_weak void *rt_memcpy(void *dst, const void *src, rt_ubase_t count);
void *rt_memmove(void *dest, const void *src, rt_size_t n);
rt_int32_t rt_memcmp(const void *cs, const void *ct, rt_size_t count);
char *rt_strstr(const char *s1, const char *s2);
rt_int32_t rt_strcasecmp(const char *a, const char *b);
char *rt_strncpy(char *dst, const char *src, rt_size_t n);
char *rt_strcpy(char *dst, const char *src);
rt_int32_t rt_strncmp(const char *cs, const char *ct, rt_size_t count);
rt_int32_t rt_strcmp(const char *cs, const char *ct);
rt_size_t rt_strlen(const char *s);
rt_size_t rt_strnlen(const char *s, rt_ubase_t maxlen);

/*-------------------------------------------------------memory define end-----------------------------------------*/


/*---------------------------------------------------printf define start------------------------------------------------------*/

#define RT_USING_CONSOLE
#define RT_KPRINTF_USING_LONGLONG
#define RT_PRINTF_PRECISION
#define RT_PRINTF_SPECIAL

#define RT_CONSOLEBUF_SIZE 128

//RT Debug
#define RT_ASSERT(EX)
#define RT_DEBUG_LOG(type, message)
#define RT_DEBUG_NOT_IN_INTERRUPT
#define RT_DEBUG_IN_THREAD_CONTEXT
#define RT_DEBUG_SCHEDULER_AVAILABLE(need_check)


/*
 * general kernel service
 */
#ifndef RT_USING_CONSOLE
#define rt_kprintf(...)
#define rt_kputs(str)
#else
int rt_kprintf(const char *fmt, ...);
void rt_kputs(const char *str);
#endif

int rt_vsprintf(char *dest, const char *format, va_list arg_ptr);
int rt_vsnprintf(char *buf, rt_size_t size, const char *fmt, va_list args);
int rt_sprintf(char *buf, const char *format, ...);
int rt_snprintf(char *buf, rt_size_t size, const char *format, ...);

void rt_show_version(void);
/*---------------------------------------------------printf define end------------------------------------------------------*/



#endif