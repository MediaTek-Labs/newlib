#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/reent.h>

#ifdef _MB_CAPABLE
#include <wchar.h>
#endif

static size_t allocated = 0;

#ifdef USE_MALLOC_DTOA

#define GUARD_SIZE 16

#define MAX(a,b)	((a) > (b) ? (a) : (b))
#define MIN(a,b)	((a) < (b) ? (a) : (b))

extern void *_sbrk_r (struct _reent *, size_t);

void *_malloc_r (struct _reent *r, size_t size)
{
    if (!size)
	return NULL;

    size_t to_alloc = size + sizeof(size_t) + MALLOC_ALIGNMENT - 1 + GUARD_SIZE;

    if (to_alloc < size)
	return NULL;

    void *p = _sbrk_r (r, to_alloc);

    if (p == (void *) -1)
	return NULL;

    allocated += size;

    p += sizeof(size_t);
    p += (MALLOC_ALIGNMENT - 1);
    p = (void *)(((size_t)p) & (-MALLOC_ALIGNMENT));

    ((size_t *)p)[-1] = size;

    char *q = p + size;
    for (int i = 0; i < GUARD_SIZE; i++)
	q[i] = 0xff;

    return p;
}

void *_calloc_r (struct _reent *r, size_t num, size_t size)
{
    size_t to_alloc = size * num;

    if (to_alloc < size || to_alloc < num)
	return NULL;

    void *p = _malloc_r (r, to_alloc);

    if (p)
	memset (p, 0x0, to_alloc);

    return p;
}

void _free_r (struct _reent *r, void *p)
{
    if (p)
    {
	size_t size = ((size_t *)p)[-1];

	if (size == -1)
	{
	    printf (__FILE__ ":%d: double free of %p\n", __LINE__, p);
	    abort ();
	}

	char *q = p + size;
	for (int i = 0; i < GUARD_SIZE; i++)
	{
	     if (q[i] != 0xff)
	     {
		printf (__FILE__ ":%d: overwrite of %p at %zu\n", __LINE__,
			p, size + i);
		abort ();
	     }
	}

        allocated -= size;
        memset (p, 0xdb, size);
        ((size_t *)p)[-1] = -1;
    }
}

void *_realloc_r (struct _reent *r, void *p, size_t new_size)
{
    if (!p)
	return _malloc_r (r, new_size);

    size_t size = ((size_t *)p)[-1];

    void *_new = _malloc_r (r, MAX(new_size, size));

    if (_new)
    {
	memcpy (_new, p, MIN(size, new_size));
	_free_r (r, p);
    }

    return _new;
}
#endif

/* Taken form libc-testsuite's snprintf.c */

#define TEST(r, f, x, m) ( \
((r) = (f)) == (x) || \
(printf (__FILE__ ":%d: %s failed (" m ")\n", __LINE__, #f, r, x), err++, 0) )

#define TEST_S(s, x, m) ( \
!strcmp ((s),(x)) || \
(printf (__FILE__ ":%d: [%s] != [%s] (%s)\n", __LINE__, s, x, m), err++, 0) )

#ifdef _MB_CAPABLE

#define TEST_W(s, x, m) ( \
!wcscmp ((s),(x)) || \
(printf (__FILE__ ":%d: [%ls] != [%ls] (%s)\n", __LINE__, s, x, m), err++, 0) )

#define CASE(fmt, f, expect) { fmt, expect, L ## fmt, L ## expect, f }
#else
#define CASE(fmt, f, expect) { fmt, expect, f }
#endif

static const struct {
	const char *fmt;
	const char *expect;
#ifdef _MB_CAPABLE
	const wchar_t *wfmt;
	const wchar_t *wexpect;
#endif
	double f;
} fp_tests[] = {
	/* basic form, handling of exponent/precision for 0 */
	CASE( "%e", 0.0, "0.000000e+00" ),
	CASE( "%f", 0.0, "0.000000" ),
	CASE( "%g", 0.0, "0" ),
	CASE( "%#g", 0.0, "0.00000" ),
#if defined(_WANT_IO_C99_FORMATS)
	CASE( "%a", 0.0, "0x0p+0" ),
	CASE( "%A", 0.0, "0X0P+0" ),
	CASE( "%.0a", 0.0, "0x0p+0" ),
	CASE( "%.0A", 0.0, "0X0P+0" ),
#endif

	/* rounding */
	CASE( "%f", 1.1, "1.100000" ),
	CASE( "%f", 1.2, "1.200000" ),
	CASE( "%f", 1.3, "1.300000" ),
	CASE( "%f", 1.4, "1.400000" ),
	CASE( "%f", 1.5, "1.500000" ),
	CASE( "%.4f", 1.06125, "1.0613" ),
	CASE( "%.2f", 1.375, "1.38" ),
	CASE( "%.1f", 1.375, "1.4" ),
	CASE( "%.15f", 1.1, "1.100000000000000" ),
	CASE( "%.16f", 1.1, "1.1000000000000001" ),
	CASE( "%.17f", 1.1, "1.10000000000000009" ),
	CASE( "%.2e", 1500001.0, "1.50e+06" ),
	CASE( "%.2e", 1505000.0, "1.50e+06" ),
	CASE( "%.2e", 1505000.00000095367431640625, "1.51e+06" ),
	CASE( "%.2e", 1505001.0, "1.51e+06" ),
	CASE( "%.2e", 1506000.0, "1.51e+06" ),

	/* correctness in DBL_DIG places */
	CASE( "%.15g", 1.23456789012345, "1.23456789012345" ),

	/* correct choice of notation for %g */
	CASE( "%g", 0.0001, "0.0001" ),
	CASE( "%g", 0.00001, "1e-05" ),
	CASE( "%g", 123456, "123456" ),
	CASE( "%g", 1234567, "1.23457e+06" ),
	CASE( "%.7g", 1234567, "1234567" ),
	CASE( "%.7g", 12345678, "1.234568e+07" ),
	CASE( "%.8g", 0.1, "0.1" ),
	CASE( "%.9g", 0.1, "0.1" ),
	CASE( "%.10g", 0.1, "0.1" ),
	CASE( "%.11g", 0.1, "0.1" ),

#if defined(_WANT_IO_C99_FORMATS)
	CASE( "%a", 1.0, "0x1p+0" ),
	CASE( "%A", 1.0, "0X1P+0" ),
	CASE( "%a", 1.1, "0x1.199999999999ap+0" ),
	CASE( "%A", 1.1, "0X1.199999999999AP+0" ),
	CASE( "%.1a", 1.1, "0x1.2p+0" ),
	CASE( "%.1A", 1.1, "0X1.2P+0" ),
#endif

	/* pi in double precision, printed to a few extra places */
	CASE( "%.15f", M_PI, "3.141592653589793" ),
	CASE( "%.18f", M_PI, "3.141592653589793116" ),

	/* exact conversion of large integers */
	CASE( "%.0f", 340282366920938463463374607431768211456.0,
	         "340282366920938463463374607431768211456" ),

	{ NULL }
};

int main (void)
{
    int i, j;
    int err=0;
    char b[2000];

    allocated = 0;

    for (j=0; fp_tests[j].fmt; j++) {
	TEST(i, snprintf (b, sizeof b, fp_tests[j].fmt, fp_tests[j].f), strlen (b), "%d != %d");
	TEST_S(b, fp_tests[j].expect, "bad floating point conversion");
    }

    TEST(i, snprintf (0, 0, "%.4a", 1.0), 11, "%d != %d");

#ifdef _MB_CAPABLE
    wchar_t wb[sizeof b];

    for (j=0; fp_tests[j].fmt; j++) {
	TEST(i, swprintf (wb, sizeof b, fp_tests[j].wfmt, fp_tests[j].f), wcslen (wb), "%d != %d");
	TEST_W(wb, fp_tests[j].wexpect, "bad floating point conversion");
    }
#endif

    if (allocated != 0)
    {
	printf (__FILE__ ":%d: allocated [%d] != 0\n", __LINE__, (int)allocated);
	err++;
    }

    return err;
}