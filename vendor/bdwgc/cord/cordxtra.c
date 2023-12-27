/*
 * Copyright (c) 1993-1994 by Xerox Corporation.  All rights reserved.
 *
 * THIS MATERIAL IS PROVIDED AS IS, WITH ABSOLUTELY NO WARRANTY EXPRESSED
 * OR IMPLIED.  ANY USE IS AT YOUR OWN RISK.
 *
 * Permission is hereby granted to use or copy this program
 * for any purpose, provided the above notices are retained on all copies.
 * Permission to modify the code and to distribute modified code is granted,
 * provided the above notices are retained, and a notice that the code was
 * modified is included with the above copyright notice.
 */

/* These are functions on cords that do not need to understand their    */
/* implementation.  They serve also serve as example client code for    */
/* the cord basic primitives.                                           */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "gc/gc.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#ifndef CORD_BUILD
# define CORD_BUILD
#endif
#ifndef CORD_DONT_DECLARE_OOM_FN
# define CORD_DONT_DECLARE_OOM_FN
#endif
#include "gc/cord.h"
#include "gc/ec.h"

/* If available, use GCC built-in atomic load-acquire and store-release */
/* primitives to access the cache lines safely.  Otherwise, fall back   */
/* to using the GC allocation lock even during the cache lines reading. */
/* Note: for simplicity of libcord building, do not rely on GC_THREADS  */
/* macro, libatomic_ops package presence and private/gc_atomic_ops.h.   */
#if !defined(AO_DISABLE_GCC_ATOMICS) \
    && ((defined(__clang__) && __clang_major__ >= 8) /* clang 8.0+ */ \
        || (defined(__GNUC__) /* gcc 5.4+ */ \
            && (__GNUC__ > 5 || (__GNUC__ == 5 && __GNUC_MINOR__ >= 4))))
# define CORD_USE_GCC_ATOMIC
#endif

/* The standard says these are in stdio.h, but they aren't always: */
# ifndef SEEK_SET
#   define SEEK_SET 0
# endif
# ifndef SEEK_END
#   define SEEK_END 2
# endif

# define BUFSZ 2048     /* Size of stack allocated buffers when */
                        /* we want large buffers.               */

# define OUT_OF_MEMORY { CORD__call_oom_fn(); ABORT("Out of memory"); }

# define ABORT(msg) { fprintf(stderr, "%s\n", msg); abort(); }

CORD CORD_cat_char(CORD x, char c)
{
    char * string;

    if ('\0' == c) return CORD_cat(x, CORD_nul(1));
    string = (char *)GC_MALLOC_ATOMIC(2);
    if (NULL == string) OUT_OF_MEMORY;
    string[0] = c;
    string[1] = '\0';
    return CORD_cat_char_star(x, string, 1);
}

CORD CORD_catn(int nargs, ...)
{
    CORD result = CORD_EMPTY;
    va_list args;
    int i;

    va_start(args, nargs);
    for (i = 0; i < nargs; i++) {
        CORD next = va_arg(args, CORD);
        result = CORD_cat(result, next);
    }
    va_end(args);
    return result;
}

typedef struct {
    size_t len;
    size_t count;
    char * buf;
} CORD_fill_data;

static int CORD_fill_proc(char c, void * client_data)
{
    CORD_fill_data * d = (CORD_fill_data *)client_data;
    size_t count = d -> count;

    (d -> buf)[count] = c;
    d -> count = ++count;
    return count >= d -> len ? 1 : 0;
}

static int CORD_batched_fill_proc(const char * s, void * client_data)
{
    CORD_fill_data * d = (CORD_fill_data *)client_data;
    size_t count = d -> count;
    size_t max = d -> len;
    char * buf = d -> buf;
    const char * t = s;

    while ((buf[count] = *t++) != '\0') {
        count++;
        if (count >= max) {
            d -> count = count;
            return 1;
        }
    }
    d -> count = count;
    return 0;
}

/* Fill buf with len characters starting at i.  */
/* Assumes len characters are available in buf. */
/* Return 1 if buf is filled fully (and len is  */
/* non-zero), 0 otherwise.                      */
static int CORD_fill_buf(CORD x, size_t i, size_t len, char * buf)
{
    CORD_fill_data fd;

    fd.len = len;
    fd.buf = buf;
    fd.count = 0;
    return CORD_iter5(x, i, CORD_fill_proc, CORD_batched_fill_proc, &fd);
}

int CORD_cmp(CORD x, CORD y)
{
    CORD_pos xpos;
    CORD_pos ypos;

    if (y == CORD_EMPTY) return x != CORD_EMPTY;
    if (x == CORD_EMPTY) return -1;
    if (CORD_IS_STRING(y) && CORD_IS_STRING(x)) return strcmp(x, y);
    CORD_set_pos(xpos, x, 0);
    CORD_set_pos(ypos, y, 0);
    for(;;) {
        long avail, yavail;

        if (!CORD_pos_valid(xpos)) {
            return CORD_pos_valid(ypos) ? -1 : 0;
        }
        if (!CORD_pos_valid(ypos)) return 1;
        avail = CORD_pos_chars_left(xpos);
        if (0 == avail
            || (yavail = CORD_pos_chars_left(ypos)) == 0) {
            char xcurrent = CORD_pos_fetch(xpos);
            char ycurrent = CORD_pos_fetch(ypos);
            if (xcurrent != ycurrent) return xcurrent - ycurrent;
            CORD_next(xpos);
            CORD_next(ypos);
        } else {
            /* process as many characters as we can */
            int result;

            if (avail > yavail) avail = yavail;
            result = strncmp(CORD_pos_cur_char_addr(xpos),
                             CORD_pos_cur_char_addr(ypos), (size_t)avail);
            if (result != 0) return result;
            CORD_pos_advance(xpos, (size_t)avail);
            CORD_pos_advance(ypos, (size_t)avail);
        }
    }
}

int CORD_ncmp(CORD x, size_t x_start, CORD y, size_t y_start, size_t len)
{
    CORD_pos xpos;
    CORD_pos ypos;
    size_t count;

    CORD_set_pos(xpos, x, x_start);
    CORD_set_pos(ypos, y, y_start);
    for(count = 0; count < len;) {
        long avail, yavail;

        if (!CORD_pos_valid(xpos)) {
            return CORD_pos_valid(ypos) ? -1 : 0;
        }
        if (!CORD_pos_valid(ypos)) return 1;
        if ((avail = CORD_pos_chars_left(xpos)) <= 0
            || (yavail = CORD_pos_chars_left(ypos)) <= 0) {
            char xcurrent = CORD_pos_fetch(xpos);
            char ycurrent = CORD_pos_fetch(ypos);

            if (xcurrent != ycurrent) return xcurrent - ycurrent;
            CORD_next(xpos);
            CORD_next(ypos);
            count++;
        } else {
            /* process as many characters as we can */
            int result;

            if (avail > yavail) avail = yavail;
            count += (size_t)avail;
            if (count > len)
                avail -= (long)(count - len);
            result = strncmp(CORD_pos_cur_char_addr(xpos),
                             CORD_pos_cur_char_addr(ypos), (size_t)avail);
            if (result != 0) return result;
            CORD_pos_advance(xpos, (size_t)avail);
            CORD_pos_advance(ypos, (size_t)avail);
        }
    }
    return 0;
}

char * CORD_to_char_star(CORD x)
{
    size_t len = CORD_len(x);
    char * result = (char *)GC_MALLOC_ATOMIC(len + 1);

    if (NULL == result) OUT_OF_MEMORY;
    if (len > 0 && CORD_fill_buf(x, 0, len, result) != 1)
      ABORT("CORD_fill_buf malfunction");
    result[len] = '\0';
    return result;
}

CORD CORD_from_char_star(const char *s)
{
    char * result;
    size_t len = strlen(s);

    if (0 == len) return CORD_EMPTY;
    result = (char *)GC_MALLOC_ATOMIC(len + 1);
    if (NULL == result) OUT_OF_MEMORY;
    memcpy(result, s, len+1);
    return result;
}

const char * CORD_to_const_char_star(CORD x)
{
    if (0 == x) return "";
    if (CORD_IS_STRING(x)) return (const char *)x;
    return CORD_to_char_star(x);
}

char CORD_fetch(CORD x, size_t i)
{
    CORD_pos xpos;

    CORD_set_pos(xpos, x, i);
    if (!CORD_pos_valid(xpos)) ABORT("bad index?");
    return CORD_pos_fetch(xpos);
}


static int CORD_put_proc(char c, void * client_data)
{
    FILE * f = (FILE *)client_data;

    return putc(c, f) == EOF;
}

static int CORD_batched_put_proc(const char * s, void * client_data)
{
    FILE * f = (FILE *)client_data;

    return fputs(s, f) == EOF;
}

int CORD_put(CORD x, FILE * f)
{
    if (CORD_iter5(x, 0, CORD_put_proc, CORD_batched_put_proc, f))
        return EOF;
    return 1;
}

typedef struct {
    size_t pos;     /* Current position in the cord */
    char target;    /* Character we're looking for  */
} chr_data;

static int CORD_chr_proc(char c, void * client_data)
{
    chr_data * d = (chr_data *)client_data;

    if (c == d -> target) return 1;
    (d -> pos)++;
    return 0;
}

static int CORD_rchr_proc(char c, void * client_data)
{
    chr_data * d = (chr_data *)client_data;

    if (c == d -> target) return 1;
    (d -> pos)--;
    return 0;
}

static int CORD_batched_chr_proc(const char * s, void * client_data)
{
    chr_data * d = (chr_data *)client_data;
    const char * occ = strchr(s, d -> target);

    if (NULL == occ) {
        d -> pos += strlen(s);
        return 0;
    }
    d -> pos += (size_t)(occ - s);
    return 1;
}

size_t CORD_chr(CORD x, size_t i, int c)
{
    chr_data d;

    d.pos = i;
    d.target = (char)c;
    if (CORD_iter5(x, i, CORD_chr_proc, CORD_batched_chr_proc, &d)) {
        return d.pos;
    } else {
        return CORD_NOT_FOUND;
    }
}

size_t CORD_rchr(CORD x, size_t i, int c)
{
    chr_data d;

    d.pos = i;
    d.target = (char)c;
    if (CORD_riter4(x, i, CORD_rchr_proc, &d)) {
        return d.pos;
    } else {
        return CORD_NOT_FOUND;
    }
}

/* Find the first occurrence of s in x at position start or later.      */
/* This uses an asymptotically poor algorithm, which should typically   */
/* perform acceptably.  We compare the first few characters directly,   */
/* and call CORD_ncmp whenever there is a partial match.                */
/* This has the advantage that we allocate very little, or not at all.  */
/* It's very fast if there are few close misses.                        */
size_t CORD_str(CORD x, size_t start, CORD s)
{
    CORD_pos xpos;
    size_t xlen = CORD_len(x);
    size_t slen;
    size_t start_len;
    const char * s_start;
    unsigned long s_buf = 0;    /* The first few characters of s        */
    unsigned long x_buf = 0;    /* Start of candidate substring.        */
                    /* Initialized only to make compilers   */
                    /* happy.                               */
    unsigned long mask = 0;
    size_t i;
    size_t match_pos;

    if (s == CORD_EMPTY) return start;
    if (CORD_IS_STRING(s)) {
        s_start = s;
        slen = strlen(s);
    } else {
        s_start = CORD_to_char_star(CORD_substr(s, 0, sizeof(unsigned long)));
        slen = CORD_len(s);
    }
    if (xlen < start || xlen - start < slen) return CORD_NOT_FOUND;
    start_len = slen;
    if (start_len > sizeof(unsigned long)) start_len = sizeof(unsigned long);
    CORD_set_pos(xpos, x, start);
    for (i = 0; i < start_len; i++) {
        mask <<= 8;
        mask |= 0xff;
        s_buf <<= 8;
        s_buf |= (unsigned char)s_start[i];
        x_buf <<= 8;
        x_buf |= (unsigned char)CORD_pos_fetch(xpos);
        CORD_next(xpos);
    }
    for (match_pos = start; ; match_pos++) {
        if ((x_buf & mask) == s_buf
            && (slen == start_len
                || CORD_ncmp(x, match_pos + start_len,
                             s, start_len, slen - start_len) == 0))
          break;
        if (match_pos == xlen - slen) return CORD_NOT_FOUND;
        x_buf <<= 8;
        x_buf |= (unsigned char)CORD_pos_fetch(xpos);
        CORD_next(xpos);
    }
    return match_pos;
}

void CORD_ec_flush_buf(CORD_ec x)
{
    size_t len = (size_t)(x[0].ec_bufptr - x[0].ec_buf);
    char * s;

    if (len == 0) return;
    s = (char *)GC_MALLOC_ATOMIC(len + 1);
    if (NULL == s) OUT_OF_MEMORY;
    memcpy(s, x[0].ec_buf, len);
    s[len] = '\0';
    x[0].ec_cord = CORD_cat_char_star(x[0].ec_cord, s, len);
    x[0].ec_bufptr = x[0].ec_buf;
}

void CORD_ec_append_cord(CORD_ec x, CORD s)
{
    CORD_ec_flush_buf(x);
    x[0].ec_cord = CORD_cat(x[0].ec_cord, s);
}

static char CORD_nul_func(size_t i, void * client_data)
{
    (void)i;
    return (char)(GC_word)client_data;
}

CORD CORD_chars(char c, size_t i)
{
    return CORD_from_fn(CORD_nul_func, (void *)(GC_word)(unsigned char)c, i);
}

CORD CORD_from_file_eager(FILE * f)
{
    CORD_ec ecord;

    CORD_ec_init(ecord);
    for(;;) {
        int c = getc(f);

        if (c == 0) {
          /* Append the right number of NULs                            */
          /* Note that any string of NULs is represented in 4 words,    */
          /* independent of its length.                                 */
            size_t count = 1;

            CORD_ec_flush_buf(ecord);
            while ((c = getc(f)) == 0) count++;
            ecord[0].ec_cord = CORD_cat(ecord[0].ec_cord, CORD_nul(count));
        }
        if (c == EOF) break;
        CORD_ec_append(ecord, (char)c);
    }
    (void) fclose(f);
    return CORD_balance(CORD_ec_to_cord(ecord));
}

/* The state maintained for a lazily read file consists primarily       */
/* of a large direct-mapped cache of previously read values.            */
/* We could rely more on stdio buffering.  That would have 2            */
/* disadvantages:                                                       */
/*  1) Empirically, not all fseek implementations preserve the          */
/*     buffer whenever they could.                                      */
/*  2) It would fail if 2 different sections of a long cord             */
/*     were being read alternately.                                     */
/* We do use the stdio buffer for read ahead.                           */
/* To guarantee thread safety in the presence of atomic pointer         */
/* writes, cache lines are always replaced, and never modified in       */
/* place.                                                               */

# define LOG_CACHE_SZ 14
# define LOG_LINE_SZ 9
# define LAZY_THRESHOLD (128*1024L+1)

# define CACHE_SZ (1 << LOG_CACHE_SZ)
# define LINE_SZ (1 << LOG_LINE_SZ)

typedef struct {
    size_t tag;
    char data[LINE_SZ];
        /* data[i%LINE_SZ] is i-th char in file if tag is i/LINE_SZ */
} cache_line;

typedef struct {
    FILE * lf_file;
    size_t lf_current;  /* Current file pointer value */
    cache_line * volatile lf_cache[CACHE_SZ/LINE_SZ];
} lf_state;

# define MOD_CACHE_SZ(n) ((n) & (CACHE_SZ - 1))
# define DIV_CACHE_SZ(n) ((n) >> LOG_CACHE_SZ)
# define MOD_LINE_SZ(n) ((n) & (LINE_SZ - 1))
# define DIV_LINE_SZ(n) ((n) >> LOG_LINE_SZ)
# define LINE_START(n) ((n) & ~(size_t)(LINE_SZ - 1))

typedef struct {
    lf_state * state;
    size_t file_pos;    /* Position of needed character. */
    cache_line * new_cache;
} refill_data;

/* Executed with allocation lock. */
static void * GC_CALLBACK refill_cache(void * client_data)
{
    lf_state * state = ((refill_data *)client_data) -> state;
    size_t file_pos = ((refill_data *)client_data) -> file_pos;
    FILE *f = state -> lf_file;
    size_t line_start = LINE_START(file_pos);
    size_t line_no = DIV_LINE_SZ(MOD_CACHE_SZ(file_pos));
    cache_line * new_cache = ((refill_data *)client_data) -> new_cache;

    if (line_start != state -> lf_current
        && fseek(f, (long)line_start, SEEK_SET) != 0) {
            ABORT("fseek failed");
    }
    if (fread(new_cache -> data, sizeof(char), LINE_SZ, f)
        <= file_pos - line_start) {
        ABORT("fread failed");
    }
    new_cache -> tag = DIV_LINE_SZ(file_pos);
#   ifdef CORD_USE_GCC_ATOMIC
        __atomic_store_n(&(state -> lf_cache[line_no]), new_cache,
                         __ATOMIC_RELEASE);
#   endif
    GC_END_STUBBORN_CHANGE((/* no volatile */ void *)
                                (GC_word)(state -> lf_cache + line_no));
    state -> lf_current = line_start + LINE_SZ;
    return (void *)((GC_word)new_cache->data[MOD_LINE_SZ(file_pos)]);
}

#ifndef CORD_USE_GCC_ATOMIC
  static void * GC_CALLBACK get_cache_line(void * client_data)
  {
    return (void *)(*(cache_line **)client_data);
  }
#endif

static char CORD_lf_func(size_t i, void * client_data)
{
    lf_state * state = (lf_state *)client_data;
    cache_line * volatile * cl_addr =
                        &(state -> lf_cache[DIV_LINE_SZ(MOD_CACHE_SZ(i))]);
#   ifdef CORD_USE_GCC_ATOMIC
        cache_line * cl = (cache_line *)__atomic_load_n(cl_addr,
                                                        __ATOMIC_ACQUIRE);
#   else
        cache_line * cl = (cache_line *)GC_call_with_alloc_lock(
                                            get_cache_line, (void *)cl_addr);
#   endif

    if (cl == 0 || cl -> tag != DIV_LINE_SZ(i)) {
        /* Cache miss */
        refill_data rd;

        rd.state = state;
        rd.file_pos =  i;
        rd.new_cache = GC_NEW_ATOMIC(cache_line);
        if (NULL == rd.new_cache) OUT_OF_MEMORY;
        return (char)((GC_word)GC_call_with_alloc_lock(refill_cache, &rd));
    }
    return cl -> data[MOD_LINE_SZ(i)];
}

#ifndef GC_NO_FINALIZATION
  static void CORD_lf_close_proc(void * obj, void * client_data)
  {
    (void)client_data;
    if (fclose(((lf_state *)obj) -> lf_file) != 0)
        ABORT("CORD_lf_close_proc: fclose failed");
  }
#endif

static CORD CORD_from_file_lazy_inner(FILE * f, size_t len)
{
    lf_state * state = GC_NEW(lf_state);
    int i;

    if (NULL == state) OUT_OF_MEMORY;
    if (len != 0) {
        /* Dummy read to force buffer allocation.       */
        /* This greatly increases the probability       */
        /* of avoiding deadlock if buffer allocation    */
        /* is redirected to GC_malloc and the           */
        /* world is multi-threaded.                     */
        char buf[1];

        if (fread(buf, 1, 1, f) > 1
            || fseek(f, 0l, SEEK_SET) != 0) {
            ABORT("Bad f argument or I/O failure");
        }
    }
    state -> lf_file = f;
    for (i = 0; i < CACHE_SZ/LINE_SZ; i++) {
        state -> lf_cache[i] = 0;
    }
    state -> lf_current = 0;
#   ifndef GC_NO_FINALIZATION
      GC_REGISTER_FINALIZER(state, CORD_lf_close_proc, 0, 0, 0);
#   endif
    return CORD_from_fn(CORD_lf_func, state, len);
}

CORD CORD_from_file_lazy(FILE * f)
{
    long len;

    if (fseek(f, 0l, SEEK_END) != 0
        || (len = ftell(f)) < 0
        || fseek(f, 0l, SEEK_SET) != 0) {
        ABORT("Bad f argument or I/O failure");
    }
    return CORD_from_file_lazy_inner(f, (size_t)len);
}

CORD CORD_from_file(FILE * f)
{
    long len;

    if (fseek(f, 0l, SEEK_END) != 0
        || (len = ftell(f)) < 0
        || fseek(f, 0l, SEEK_SET) != 0) {
        ABORT("Bad f argument or I/O failure");
    }
    if (len < LAZY_THRESHOLD) {
        return CORD_from_file_eager(f);
    }
    return CORD_from_file_lazy_inner(f, (size_t)len);
}
