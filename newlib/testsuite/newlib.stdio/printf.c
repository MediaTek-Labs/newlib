#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/reent.h>

static size_t allocated = 0;

#ifdef USE_MALLOC_DTOA

#define MAX(a,b)	((a) > (b) ? (a) : (b))
#define MIN(a,b)	((a) < (b) ? (a) : (b))

extern void *_sbrk_r (struct _reent *, size_t);

void *_malloc_r (struct _reent *r, size_t size)
{
    if (!size)
        return NULL;

    size_t to_alloc = size + sizeof(size_t) + MALLOC_ALIGNMENT - 1;

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


static const struct {
	const char *fmt;
	double f;
	const char *expect;
} fp_tests[] = {
	/* basic form, handling of exponent/precision for 0 */
	{ "%e", 0.0, "0.000000e+00" },
	{ "%f", 0.0, "0.000000" },
	{ "%g", 0.0, "0" },
	{ "%#g", 0.0, "0.00000" },
#if defined(_WANT_IO_C99_FORMATS)
	{ "%a", 0.0, "0x0p+0" },
	{ "%A", 0.0, "0X0P+0" },
	{ "%.0a", 0.0, "0x0p+0" },
	{ "%.0A", 0.0, "0X0P+0" },
#endif

	/* rounding */
	{ "%f", 1.1, "1.100000" },
	{ "%f", 1.2, "1.200000" },
	{ "%f", 1.3, "1.300000" },
	{ "%f", 1.4, "1.400000" },
	{ "%f", 1.5, "1.500000" },
	{ "%.4f", 1.06125, "1.0613" },
	{ "%.2f", 1.375, "1.38" },
	{ "%.1f", 1.375, "1.4" },
	{ "%.15f", 1.1, "1.100000000000000" },
	{ "%.16f", 1.1, "1.1000000000000001" },
	{ "%.17f", 1.1, "1.10000000000000009" },
	{ "%.2e", 1500001.0, "1.50e+06" },
	{ "%.2e", 1505000.0, "1.50e+06" },
	{ "%.2e", 1505000.00000095367431640625, "1.51e+06" },
	{ "%.2e", 1505001.0, "1.51e+06" },
	{ "%.2e", 1506000.0, "1.51e+06" },

	/* correctness in DBL_DIG places */
	{ "%.15g", 1.23456789012345, "1.23456789012345" },

	/* correct choice of notation for %g */
	{ "%g", 0.0001, "0.0001" },
	{ "%g", 0.00001, "1e-05" },
	{ "%g", 123456, "123456" },
	{ "%g", 1234567, "1.23457e+06" },
	{ "%.7g", 1234567, "1234567" },
	{ "%.7g", 12345678, "1.234568e+07" },
	{ "%.8g", 0.1, "0.1" },
	{ "%.9g", 0.1, "0.1" },
	{ "%.10g", 0.1, "0.1" },
	{ "%.11g", 0.1, "0.1" },

#if defined(_WANT_IO_C99_FORMATS)
	{ "%a", 1.0, "0x1p+0" },
	{ "%A", 1.0, "0X1P+0" },
	{ "%a", 1.1, "0x1.199999999999ap+0" },
	{ "%A", 1.1, "0X1.199999999999AP+0" },
	{ "%.1a", 1.1, "0x1.2p+0" },
	{ "%.1A", 1.1, "0X1.2P+0" },
#endif

	/* pi in double precision, printed to a few extra places */
	{ "%.15f", M_PI, "3.141592653589793" },
	{ "%.18f", M_PI, "3.141592653589793116" },

	/* exact conversion of large integers */
	{ "%.0f", 340282366920938463463374607431768211456.0,
	         "340282366920938463463374607431768211456" },

	{ NULL, 0.0, NULL }
};

int main (void)
{
	int i, j;
	int err=0;
	char b[2000], *s;

	allocated = 0;

	for (j=0; fp_tests[j].fmt; j++) {
		TEST(i, snprintf (b, sizeof b, fp_tests[j].fmt, fp_tests[j].f), strlen (b), "%d != %d");
		TEST_S(b, fp_tests[j].expect, "bad floating point conversion");
	}

	TEST(i, snprintf (0, 0, "%.4a", 1.0), 11, "%d != %d");

	if (allocated != 0)
	{
	    printf (__FILE__ ":%d: allocated [%d] != 0\n", __LINE__, (int)allocated);
	    err++;
	}

	return err;
}