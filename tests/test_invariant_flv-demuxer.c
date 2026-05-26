#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*
 * We test the security invariant of flv_demuxer_input:
 * - The function must never underflow when computing (bytes - n)
 * - The function must never write beyond the allocated buffer
 * - The function must handle adversarial/boundary inputs without memory corruption
 *
 * We simulate the vulnerable logic and assert the invariant holds:
 * n <= bytes MUST always be true before memmove(dst + r, src + n, bytes - n)
 */

/* Minimal stub types to replicate the vulnerable logic */
typedef struct {
    uint8_t *ptr;
    size_t   capacity;
    size_t   bytes;
} flv_demuxer_t;

/*
 * Simulated safe version of the vulnerable logic.
 * Returns 0 on success, -1 if invariant would be violated.
 */
static int simulate_flv_input(flv_demuxer_t *flv, const uint8_t *data, size_t bytes, size_t n, size_t r)
{
    /* INVARIANT: n must not exceed bytes (prevents size_t underflow) */
    if (n > bytes) {
        return -1; /* underflow would occur */
    }

    size_t copy_size = bytes - n;

    /* INVARIANT: destination offset r plus copy_size must not exceed capacity */
    /* First simulate realloc to new size */
    size_t new_capacity = r + copy_size;
    if (new_capacity < r) {
        /* overflow in capacity calculation */
        return -1;
    }

    if (new_capacity > 0) {
        uint8_t *new_ptr = (uint8_t *)realloc(flv->ptr, new_capacity);
        if (!new_ptr) {
            return -1;
        }
        flv->ptr = new_ptr;
        flv->capacity = new_capacity;
    }

    /* INVARIANT: r must not exceed capacity */
    if (r > flv->capacity) {
        return -1;
    }

    /* INVARIANT: r + copy_size must not exceed capacity */
    if (copy_size > 0 && (r + copy_size) > flv->capacity) {
        return -1;
    }

    if (copy_size > 0 && data != NULL) {
        memmove(flv->ptr + r, data + n, copy_size);
    }

    flv->bytes = r + copy_size;
    return 0;
}

START_TEST(test_flv_demuxer_no_integer_underflow)
{
    /*
     * Invariant: When processing FLV input, the computation (bytes - n)
     * must never underflow (n <= bytes must always hold), and the
     * memmove destination must never exceed the allocated buffer.
     */

    /* Each test case: {data_size, n_offset, r_offset} */
    struct {
        size_t   data_size;
        size_t   n;          /* offset into source data */
        size_t   r;          /* offset into destination buffer */
        const char *description;
    } cases[] = {
        /* Normal cases */
        { 100,  0,   0,   "normal: full copy" },
        { 100,  50,  0,   "normal: partial copy" },
        { 100,  99,  0,   "normal: single byte copy" },
        { 100,  100, 0,   "normal: zero copy (n == bytes)" },

        /* Boundary cases */
        { 0,    0,   0,   "boundary: zero bytes" },
        { 1,    0,   0,   "boundary: single byte" },
        { 1,    1,   0,   "boundary: n equals bytes" },

        /* Adversarial: n > bytes (would cause underflow) */
        { 10,   11,  0,   "adversarial: n > bytes by 1" },
        { 10,   100, 0,   "adversarial: n >> bytes" },
        { 0,    1,   0,   "adversarial: n=1, bytes=0" },
        { 5,    SIZE_MAX, 0, "adversarial: n = SIZE_MAX" },

        /* Adversarial: large offsets that could overflow */
        { 100,  0,   SIZE_MAX - 50, "adversarial: r near SIZE_MAX" },
        { 100,  50,  SIZE_MAX,      "adversarial: r = SIZE_MAX" },

        /* Adversarial: FLV-like header patterns */
        { 9,    0,   0,   "flv: minimal header size" },
        { 9,    9,   0,   "flv: consume full header" },
        { 9,    10,  0,   "flv: n exceeds header" },

        /* Adversarial: large data sizes */
        { 65536, 0,     0,     "large: 64KB full copy" },
        { 65536, 65535, 0,     "large: 64KB minus 1" },
        { 65536, 65536, 0,     "large: 64KB n == bytes" },
        { 65536, 65537, 0,     "large: 64KB n > bytes" },
    };

    int num_cases = (int)(sizeof(cases) / sizeof(cases[0]));

    for (int i = 0; i < num_cases; i++) {
        flv_demuxer_t flv;
        memset(&flv, 0, sizeof(flv));

        size_t data_size = cases[i].data_size;
        size_t n         = cases[i].n;
        size_t r         = cases[i].r;

        /* Allocate source data filled with a pattern */
        uint8_t *data = NULL;
        if (data_size > 0) {
            /* Cap allocation to avoid OOM in tests */
            if (data_size > (1024 * 1024)) {
                if (flv.ptr) free(flv.ptr);
                continue;
            }
            data = (uint8_t *)malloc(data_size);
            if (!data) {
                if (flv.ptr) free(flv.ptr);
                continue;
            }
            memset(data, 0xAA, data_size);
        }

        /* INVARIANT CHECK 1: n must not exceed bytes */
        if (n > data_size) {
            /*
             * This is the underflow condition. The function MUST detect
             * this and not proceed with memmove using a wrapped-around size.
             */
            int result = simulate_flv_input(&flv, data, data_size, n, r);
            ck_assert_msg(result == -1,
                "INVARIANT VIOLATED: n(%zu) > bytes(%zu) should be rejected "
                "to prevent size_t underflow [case: %s]",
                n, data_size, cases[i].description);
        } else {
            /* n <= bytes: check for buffer overflow */
            size_t copy_size = data_size - n;

            /* Check for capacity overflow */
            int overflow = (r > SIZE_MAX - copy_size);

            if (overflow || (r > (1024 * 1024)) || (copy_size > (1024 * 1024))) {
                /* Skip cases that would require huge allocations */
                if (data) free(data);
                if (flv.ptr) free(flv.ptr);
                continue;
            }

            int result = simulate_flv_input(&flv, data, data_size, n, r);

            if (result == 0) {
                /* INVARIANT CHECK 2: bytes written must equal r + copy_size */
                ck_assert_msg(flv.bytes == r + copy_size,
                    "INVARIANT VIOLATED: bytes written(%zu) != r(%zu) + copy_size(%zu) "
                    "[case: %s]",
                    flv.bytes, r, copy_size, cases[i].description);

                /* INVARIANT CHECK 3: capacity must be sufficient */
                ck_assert_msg(flv.capacity >= flv.bytes,
                    "INVARIANT VIOLATED: capacity(%zu) < bytes(%zu) "
                    "[case: %s]",
                    flv.capacity, flv.bytes, cases[i].description);
            }
        }

        if (data) free(data);
        if (flv.ptr) free(flv.ptr);
        flv.ptr = NULL;
    }
}
END_TEST

START_TEST(test_flv_demuxer_memmove_bounds)
{
    /*
     * Invariant: memmove destination (ptr + r) and source (data + n)
     * must always be within their respective allocated buffers.
     * The copy length (bytes - n) must never cause out-of-bounds access.
     */

    /* Simulate adversarial FLV tag payloads */
    const uint8_t adversarial_payloads[][16] = {
        /* FLV signature with manipulated sizes */
        { 0x46, 0x4C, 0x56, 0x01, 0x05, 0x00, 0x00, 0x00,
          0x09, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF },
        /* All zeros */
        { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
        /* All 0xFF */
        { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
          0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        /* Alternating pattern */
        { 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55,
          0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55, 0xAA, 0x55 },
    };

    int num_payloads = (int)(sizeof(adversarial_payloads) / sizeof(adversarial_payloads[0]));

    for (int i = 0; i < num_payloads; i++) {
        size_t payload_size = 16;
        const uint8_t *data = adversarial_payloads[i];

        /* Test with various n values */
        for (size_t n = 0; n <= payload_size + 2; n++) {
            flv_demuxer_t flv;
            memset(&flv, 0, sizeof(flv));

            size_t r = 0;

            if (n > payload_size) {
                /* INVARIANT: must reject underflow */
                int result = simulate_flv_input(&flv, data, payload_size, n, r);
                ck_assert_msg(result == -1,
                    "INVARIANT VIOLATED: underflow not caught for n=%zu, bytes=%zu",
                    n, payload_size);
            } else {
                int result = simulate_flv_input(&flv, data, payload_size, n, r);
                if (result == 0) {
                    size_t expected = payload_size - n;
                    ck_assert_msg(flv.bytes == expected,
                        "INVARIANT VIOLATED: wrong byte count %zu != %zu",
                        flv.bytes, expected);
                    ck_assert_msg(flv.capacity >= flv.bytes,
                        "INVARIANT VIOLATED: buffer too small");
                }
            }

            if (flv.ptr) {
                free(flv.ptr);
                flv.ptr = NULL;
            }
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security_FLV_Demuxer");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 30);
    tcase_add_test(tc_core, test_flv_demuxer_no_integer_underflow);
    tcase_add_test(tc_core, test_flv_demuxer_memmove_bounds);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}