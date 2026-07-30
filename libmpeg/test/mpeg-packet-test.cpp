#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

extern "C" {
#include "../source/mpeg-pes-internal.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
}

struct mpeg_packet_output_t
{
    int flags;
    int64_t pts;
    int64_t dts;
    std::string data;
};

struct mpeg_packet_test_t
{
    struct pes_t pes;
    std::vector<mpeg_packet_output_t> outputs;
    int handler_result;
    int handler_calls;
    int handler_fail_at;
};

static int mpeg_packet_onpacket(void* param, int program, int stream, int codecid, int flags,
    int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct mpeg_packet_test_t* ctx = (struct mpeg_packet_test_t*)param;
    struct mpeg_packet_output_t output;
    assert(1 == program);
    assert(0x100 == stream);
    assert(PSI_STREAM_MPEG1 == codecid);
    ++ctx->handler_calls;
    if (0 != ctx->handler_result
        && (0 == ctx->handler_fail_at || ctx->handler_calls == ctx->handler_fail_at))
    {
        return ctx->handler_result;
    }
    output.flags = flags;
    output.pts = pts;
    output.dts = dts;
    output.data.assign((const char*)data, bytes);
    ctx->outputs.push_back(output);
    return 0;
}

static void mpeg_packet_test_init(struct mpeg_packet_test_t* ctx)
{
    memset(&ctx->pes, 0, sizeof(ctx->pes));
    ctx->pes.pn = 1;
    ctx->pes.pid = 0x100;
    ctx->pes.sid = PES_SID_VIDEO;
    ctx->pes.codecid = PSI_STREAM_MPEG1;
    ctx->handler_result = 0;
    ctx->handler_calls = 0;
    ctx->handler_fail_at = 0;
}

static void mpeg_packet_test_discontinuity(struct mpeg_packet_test_t* ctx)
{
    ++ctx->pes.discontinuity;
}

static void mpeg_packet_test_input(struct mpeg_packet_test_t* ctx, const std::string& data,
    int start, int64_t pts, int flags = 0)
{
    size_t consume = 0;
    ctx->pes.pts = pts;
    ctx->pes.dts = pts;
    ctx->pes.flags = flags;
    int r = pes_packet(&ctx->pes.pkt, &ctx->pes, data.data(), data.size(), &consume,
        start, mpeg_packet_onpacket, ctx);
    assert(0 == r);
    assert(data.size() == consume);
}

static void mpeg_packet_test_flush(struct mpeg_packet_test_t* ctx)
{
    size_t consume = 1;
    int r = pes_packet(&ctx->pes.pkt, &ctx->pes, NULL, 0, &consume,
        0, mpeg_packet_onpacket, ctx);
    assert(0 == r);
    assert(0 == consume);
}

static void mpeg_packet_test_destroy(struct mpeg_packet_test_t* ctx)
{
    free(ctx->pes.pkt.data);
    ctx->pes.pkt.data = NULL;
}

static void mpeg_packet_picture_spans_pes_test()
{
    static const char sequence[] = {
        0x00, 0x00, 0x01, (char)0xB3, 0x16, 0x01, 0x20, 0x13
    };
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF, (char)0xFF
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF, (char)0xFF
    };
    static const char slice1[] = {
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22, 0x33
    };
    static const char slice2[] = {
        0x44, 0x55, 0x66, 0x77
    };

    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);

    std::string first(sequence, sizeof(sequence));
    first.append(picture_i, sizeof(picture_i));
    first.append(slice1, sizeof(slice1));
    std::string second(slice2, sizeof(slice2));
    std::string third(picture_p, sizeof(picture_p));
    third.append(slice1, sizeof(slice1));

    mpeg_packet_test_input(&ctx, first, 1, 9000);
    assert(ctx.outputs.empty());

    mpeg_packet_test_input(&ctx, second, 1, 12600);
    assert(ctx.outputs.empty());

    mpeg_packet_test_input(&ctx, third, 1, 16200);
    assert(1 == ctx.outputs.size());
    assert(first + second == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(9000 == ctx.outputs[0].dts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_continuity_gap_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44
    };
    static const char damaged_prefix[] = {
        0x55, 0x66, 0x77
    };

    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);

    std::string first(picture_i, sizeof(picture_i));
    std::string recovered(damaged_prefix, sizeof(damaged_prefix));
    recovered.append(picture_p, sizeof(picture_p));
    std::string next(picture_i, sizeof(picture_i));

    mpeg_packet_test_input(&ctx, first, 1, 9000);
    mpeg_packet_test_discontinuity(&ctx);
    mpeg_packet_test_input(&ctx, recovered, 1, 12600,
        MPEG_FLAG_PACKET_LOST | MPEG_FLAG_PACKET_CORRUPT);
    assert(ctx.outputs.empty());

    mpeg_packet_test_input(&ctx, next, 1, 16200);
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[0].data);
    assert(12600 == ctx.outputs[0].pts);
    assert(0 == (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_repeated_continuity_gap_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44
    };
    static const char tail[] = { 0x55, 0x66, 0x77 };

    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);

    mpeg_packet_test_input(&ctx, std::string(picture_i, sizeof(picture_i)), 1, 9000);

    mpeg_packet_test_discontinuity(&ctx);
    mpeg_packet_test_input(&ctx, std::string(picture_p, sizeof(picture_p)), 1,
        12600, MPEG_FLAG_PACKET_LOST | MPEG_FLAG_PACKET_CORRUPT);
    mpeg_packet_test_input(&ctx, std::string(tail, sizeof(tail)), 0,
        12600, MPEG_FLAG_PACKET_LOST | MPEG_FLAG_PACKET_CORRUPT);

    mpeg_packet_test_discontinuity(&ctx);
    mpeg_packet_test_input(&ctx, std::string(picture_i, sizeof(picture_i)), 0,
        16200, MPEG_FLAG_PACKET_LOST | MPEG_FLAG_PACKET_CORRUPT);
    assert(ctx.outputs.empty());

    mpeg_packet_test_input(&ctx, std::string(picture_p, sizeof(picture_p)), 1, 19800);
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_i, sizeof(picture_i)) == ctx.outputs[0].data);
    assert(16200 == ctx.outputs[0].pts);

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_prefix_metadata_test()
{
    static const char damaged_prefix[] = { 0x55, 0x00, 0x00 };
    static const char sequence_and_picture[] = {
        0x01, (char)0xB3, 0x16, 0x01, 0x20, 0x13,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44
    };
    static const char expected[] = {
        0x00, 0x00, 0x01, (char)0xB3, 0x16, 0x01, 0x20, 0x13,
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44
    };

    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);
    mpeg_packet_test_discontinuity(&ctx);
    mpeg_packet_test_input(&ctx, std::string(damaged_prefix, sizeof(damaged_prefix)),
        1, 9000, MPEG_FLAG_PACKET_LOST | MPEG_FLAG_PACKET_CORRUPT);
    mpeg_packet_test_input(&ctx,
        std::string(sequence_and_picture, sizeof(sequence_and_picture)), 0, 12600);
    mpeg_packet_test_flush(&ctx);

    assert(1 == ctx.outputs.size());
    assert(std::string(expected, sizeof(expected)) == ctx.outputs[0].data);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_picture_header_timestamp_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44
    };

    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);

    std::string first(picture_i, sizeof(picture_i));
    std::string second(picture_p, 5);
    std::string third(picture_p + 5, sizeof(picture_p) - 5);

    mpeg_packet_test_input(&ctx, first, 1, 9000);
    mpeg_packet_test_input(&ctx, second, 1, 12600);
    assert(ctx.outputs.empty());

    mpeg_packet_test_input(&ctx, third, 1, 16200);
    assert(1 == ctx.outputs.size());
    assert(9000 == ctx.outputs[0].pts);

    mpeg_packet_test_flush(&ctx);
    assert(2 == ctx.outputs.size());
    assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[1].data);
    assert(12600 == ctx.outputs[1].pts);
    assert(0 == (ctx.outputs[1].flags & MPEG_FLAG_IDR_FRAME));

    mpeg_packet_test_flush(&ctx);
    assert(2 == ctx.outputs.size());

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_picture_header_split_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44
    };

    for (size_t split = 1; split <= 5; ++split)
    {
        struct mpeg_packet_test_t ctx;
        mpeg_packet_test_init(&ctx);

        std::string first(picture_i, sizeof(picture_i));
        std::string second(picture_p, split);
        std::string third(picture_p + split, sizeof(picture_p) - split);
        mpeg_packet_test_input(&ctx, first, 1, 9000);
        mpeg_packet_test_input(&ctx, second, 1, 12600);
        mpeg_packet_test_input(&ctx, third, 1, 16200);
        mpeg_packet_test_flush(&ctx);

        assert(2 == ctx.outputs.size());
        assert(12600 == ctx.outputs[1].pts);
        assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[1].data);

        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_start_code_byte_split_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44
    };
    static const char leading[] = { 0x55, 0x00 };
    static const char zero = 0x00;
    static const char one = 0x01;

    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);

    mpeg_packet_test_input(&ctx, std::string(picture_i, sizeof(picture_i)), 1, 9000);
    mpeg_packet_test_input(&ctx, std::string(leading, sizeof(leading)), 1, 12600);
    mpeg_packet_test_input(&ctx, std::string(&zero, 1), 1, 14400);
    mpeg_packet_test_input(&ctx, std::string(&one, 1), 1, 16200);
    mpeg_packet_test_input(&ctx,
        std::string(picture_p + 3, sizeof(picture_p) - 3), 1, 18000);
    assert(1 == ctx.outputs.size());

    mpeg_packet_test_flush(&ctx);
    assert(2 == ctx.outputs.size());
    assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[1].data);
    assert(12600 == ctx.outputs[1].pts);

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_incomplete_next_picture_flush_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10
    };

    for (size_t bytes = 4; bytes <= 5; ++bytes)
    {
        struct mpeg_packet_test_t ctx;
        mpeg_packet_test_init(&ctx);
        std::string input(picture_i, sizeof(picture_i));
        input.append(picture_p, bytes);
        mpeg_packet_test_input(&ctx, input, 1, 9000);
        mpeg_packet_test_flush(&ctx);

        assert(1 == ctx.outputs.size());
        assert(std::string(picture_i, sizeof(picture_i)) == ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_multiple_pictures_test()
{
    static const char sequence[] = {
        0x00, 0x00, 0x01, (char)0xB3, 0x16, 0x01, 0x20, 0x13
    };
    static const char gop[] = {
        0x00, 0x00, 0x01, (char)0xB8, 0x00, 0x08
    };
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF
    };
    static const char slice[] = {
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22, 0x33
    };

    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);

    std::string first(sequence, sizeof(sequence));
    first.append(gop, sizeof(gop));
    first.append(picture_i, sizeof(picture_i));
    first.append(slice, sizeof(slice));
    std::string second(picture_p, sizeof(picture_p));
    second.append(slice, sizeof(slice));
    std::string third(sequence, sizeof(sequence));
    third.append(gop, sizeof(gop));
    third.append(picture_b, sizeof(picture_b));
    third.append(slice, sizeof(slice));
    std::string input = first + second + third;

    mpeg_packet_test_input(&ctx, input, 1, 9000);
    assert(2 == ctx.outputs.size());
    mpeg_packet_test_flush(&ctx);
    assert(3 == ctx.outputs.size());

    assert(first == ctx.outputs[0].data);
    assert(second == ctx.outputs[1].data);
    assert(third == ctx.outputs[2].data);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));
    assert(0 == (ctx.outputs[1].flags & MPEG_FLAG_IDR_FRAME));
    assert(0 == (ctx.outputs[2].flags & MPEG_FLAG_IDR_FRAME));
    assert(9000 == ctx.outputs[0].pts);
    assert(9000 == ctx.outputs[1].pts);
    assert(9000 == ctx.outputs[2].pts);

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_handler_error_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF
    };

    struct mpeg_packet_test_t ctx;
    size_t consume = 0;
    mpeg_packet_test_init(&ctx);
    ctx.handler_result = -123;

    std::string input(picture_i, sizeof(picture_i));
    input.append(picture_p, sizeof(picture_p));
    ctx.pes.pts = 9000;
    ctx.pes.dts = 9000;
    int r = pes_packet(&ctx.pes.pkt, &ctx.pes, input.data(), input.size(), &consume,
        1, mpeg_packet_onpacket, &ctx);
    assert(-123 == r);
    assert(input.size() == consume);
    assert(ctx.outputs.empty());

    ctx.handler_result = 0;
    mpeg_packet_test_flush(&ctx);
    assert(2 == ctx.outputs.size());
    assert(std::string(picture_i, sizeof(picture_i)) == ctx.outputs[0].data);
    assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[1].data);

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_handler_retry_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF
    };

    struct mpeg_packet_test_t ctx;
    size_t consume = 0;
    mpeg_packet_test_init(&ctx);
    ctx.handler_result = -123;
    ctx.handler_fail_at = 2;

    std::string input(picture_i, sizeof(picture_i));
    input.append(picture_p, sizeof(picture_p));
    input.append(picture_b, sizeof(picture_b));
    ctx.pes.pts = 9000;
    ctx.pes.dts = 9000;
    int r = pes_packet(&ctx.pes.pkt, &ctx.pes, input.data(), input.size(), &consume,
        1, mpeg_packet_onpacket, &ctx);
    assert(-123 == r);
    assert(input.size() == consume);
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_i, sizeof(picture_i)) == ctx.outputs[0].data);

    ctx.handler_result = 0;
    mpeg_packet_test_flush(&ctx);
    assert(3 == ctx.outputs.size());
    assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[1].data);
    assert(std::string(picture_b, sizeof(picture_b)) == ctx.outputs[2].data);

    mpeg_packet_test_flush(&ctx);
    assert(3 == ctx.outputs.size());
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_handler_retry_before_append_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF
    };

    struct mpeg_packet_test_t ctx;
    size_t consume = 0;
    mpeg_packet_test_init(&ctx);
    ctx.handler_result = -123;
    ctx.handler_fail_at = 1;

    std::string old_pes(picture_i, sizeof(picture_i));
    old_pes.append(picture_p, sizeof(picture_p));
    old_pes.append(picture_b, sizeof(picture_b));
    old_pes.append(picture_p, sizeof(picture_p));
    ctx.pes.pts = 9000;
    ctx.pes.dts = 9000;
    ctx.pes.flags = MPEG_FLAG_PACKET_CORRUPT;
    int r = pes_packet(&ctx.pes.pkt, &ctx.pes, old_pes.data(), old_pes.size(),
        &consume, 1, mpeg_packet_onpacket, &ctx);
    assert(-123 == r);
    assert(ctx.outputs.empty());

    ctx.handler_result = 0;
    mpeg_packet_test_input(&ctx, std::string(picture_i, sizeof(picture_i)),
        1, 18000);
    assert(4 == ctx.outputs.size());
    for (size_t i = 0; i < ctx.outputs.size(); ++i)
    {
        assert(9000 == ctx.outputs[i].pts);
        assert(0 != (ctx.outputs[i].flags & MPEG_FLAG_PACKET_CORRUPT));
    }

    mpeg_packet_test_flush(&ctx);
    assert(5 == ctx.outputs.size());
    assert(18000 == ctx.outputs[4].pts);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_flush_retry_before_discontinuity_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF
    };

    struct mpeg_packet_test_t ctx;
    size_t consume = 1;
    mpeg_packet_test_init(&ctx);

    mpeg_packet_test_input(&ctx, std::string(picture_i, sizeof(picture_i)),
        1, 9000);
    ctx.handler_result = -123;
    ctx.handler_fail_at = 1;
    int r = pes_packet(&ctx.pes.pkt, &ctx.pes, NULL, 0, &consume,
        0, mpeg_packet_onpacket, &ctx);
    assert(-123 == r);
    assert(0 == consume);
    assert(ctx.outputs.empty());

    ctx.handler_result = 0;
    mpeg_packet_test_discontinuity(&ctx);
    mpeg_packet_test_input(&ctx, std::string(picture_p, sizeof(picture_p)),
        1, 12600, MPEG_FLAG_PACKET_LOST | MPEG_FLAG_PACKET_CORRUPT);
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_i, sizeof(picture_i)) == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(0 == (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));

    mpeg_packet_test_input(&ctx, std::string(picture_b, sizeof(picture_b)),
        1, 16200);
    assert(2 == ctx.outputs.size());
    assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[1].data);
    assert(12600 == ctx.outputs[1].pts);
    assert(0 != (ctx.outputs[1].flags & MPEG_FLAG_PACKET_CORRUPT));

    mpeg_packet_test_flush(&ctx);
    assert(3 == ctx.outputs.size());
    assert(std::string(picture_b, sizeof(picture_b)) == ctx.outputs[2].data);
    assert(16200 == ctx.outputs[2].pts);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_size_limit_test()
{
    struct mpeg_packet_test_t ctx;
    size_t consume = 0;
    mpeg_packet_test_init(&ctx);

    std::string input(10 * 1024 * 1024, (char)0xFF);
    ctx.pes.pts = 9000;
    ctx.pes.dts = 9000;
    int r = pes_packet(&ctx.pes.pkt, &ctx.pes, input.data(), input.size(), &consume,
        1, mpeg_packet_onpacket, &ctx);
    assert(0 == r);
    assert(input.size() == consume);
    assert(input.size() == ctx.pes.pkt.size);

    const char extra = 0;
    r = pes_packet(&ctx.pes.pkt, &ctx.pes, &extra, 1, &consume,
        0, mpeg_packet_onpacket, &ctx);
    assert(-EINVAL == r);
    assert(1 == consume);
    assert(input.size() == ctx.pes.pkt.size);

    mpeg_packet_test_destroy(&ctx);
}

struct mpeg_ps_packet_test_t
{
    std::string data;
    std::vector<mpeg_packet_output_t> outputs;
    int handler_result;
};

static void* mpeg_ps_packet_alloc(void* /*param*/, size_t bytes)
{
    return malloc(bytes);
}

static void mpeg_ps_packet_free(void* /*param*/, void* packet)
{
    free(packet);
}

static int mpeg_ps_packet_write(void* param, int /*stream*/, void* packet, size_t bytes)
{
    struct mpeg_ps_packet_test_t* ctx = (struct mpeg_ps_packet_test_t*)param;
    ctx->data.append((const char*)packet, bytes);
    return 0;
}

static int mpeg_ps_packet_onpacket(void* param, int stream, int codecid, int flags,
    int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct mpeg_ps_packet_test_t* ctx = (struct mpeg_ps_packet_test_t*)param;
    struct mpeg_packet_output_t output;
    assert(PES_SID_VIDEO == stream);
    assert(PSI_STREAM_MPEG1 == codecid);
    if (0 != ctx->handler_result)
        return ctx->handler_result;
    output.flags = flags;
    output.pts = pts;
    output.dts = dts;
    output.data.assign((const char*)data, bytes);
    ctx->outputs.push_back(output);
    return 0;
}

static void mpeg_ps_packet_flush_test()
{
    static const char partial_start_code[] = {
        0x00, 0x00, 0x01
    };
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22, 0x33
    };
    static const uint8_t descriptor[] = { 0xFF, 0x00 };
    struct ps_muxer_func_t func;
    struct mpeg_ps_packet_test_t ctx;

    memset(&func, 0, sizeof(func));
    func.alloc = mpeg_ps_packet_alloc;
    func.free = mpeg_ps_packet_free;
    func.write = mpeg_ps_packet_write;
    ctx.handler_result = 0;

    struct ps_muxer_t* muxer = ps_muxer_create(&func, &ctx);
    assert(NULL != muxer);
    int stream = ps_muxer_add_stream(muxer, PSI_STREAM_MPEG1,
        descriptor, sizeof(descriptor));
    assert(PES_SID_VIDEO == stream);
    assert(0 == ps_muxer_input(muxer, stream, 0,
        5400, 5400, partial_start_code, sizeof(partial_start_code)));
    size_t partial_ps_size = ctx.data.size();
    assert(0 == ps_muxer_input(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, picture_i, sizeof(picture_i)));
    assert(0 == ps_muxer_destroy(muxer));

    struct ps_demuxer_t* demuxer = ps_demuxer_create(mpeg_ps_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    assert((int)partial_ps_size == ps_demuxer_input(demuxer,
        (const uint8_t*)ctx.data.data(), partial_ps_size));
    assert(ctx.outputs.empty());
    assert(0 == ps_demuxer_flush(demuxer));
    assert(ctx.outputs.empty());

    assert((int)(ctx.data.size() - partial_ps_size) == ps_demuxer_input(demuxer,
        (const uint8_t*)ctx.data.data() + partial_ps_size,
        ctx.data.size() - partial_ps_size));
    assert(ctx.outputs.empty());

    ctx.handler_result = -123;
    assert(-123 == ps_demuxer_flush(demuxer));
    assert(ctx.outputs.empty());

    ctx.handler_result = 0;
    assert(0 == ps_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_i, sizeof(picture_i)) == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));

    assert(0 == ps_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(0 == ps_demuxer_destroy(demuxer));
}

static int mpeg_ts_packet_write(void* param, const void* packet, size_t bytes)
{
    struct mpeg_ps_packet_test_t* ctx = (struct mpeg_ps_packet_test_t*)param;
    ctx->data.append((const char*)packet, bytes);
    return 0;
}

static int mpeg_ts_packet_onpacket(void* param, int program, int /*stream*/, int codecid,
    int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct mpeg_ps_packet_test_t* ctx = (struct mpeg_ps_packet_test_t*)param;
    struct mpeg_packet_output_t output;
    assert(1 == program);
    assert(PSI_STREAM_MPEG2 == codecid);
    if (0 != ctx->handler_result)
        return ctx->handler_result;
    output.flags = flags;
    output.pts = pts;
    output.dts = dts;
    output.data.assign((const char*)data, bytes);
    ctx->outputs.push_back(output);
    return 0;
}

static void mpeg_ts_packet_flush_test()
{
    static const char partial_start_code[] = {
        0x00, 0x00, 0x01
    };
    static const char picture_header[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22, 0x33
    };
    struct mpeg_ts_func_t func;
    struct mpeg_ps_packet_test_t ctx;

    memset(&func, 0, sizeof(func));
    func.alloc = mpeg_ps_packet_alloc;
    func.free = mpeg_ps_packet_free;
    func.write = mpeg_ts_packet_write;
    ctx.handler_result = 0;

    void* muxer = mpeg_ts_create(&func, &ctx);
    assert(NULL != muxer);
    int stream = mpeg_ts_add_stream(muxer, PSI_STREAM_MPEG2, NULL, 0);
    assert(stream > 0);
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        5400, 5400, partial_start_code, sizeof(partial_start_code)));
    size_t partial_ts_size = ctx.data.size();
    assert(0 == partial_ts_size % 188);

    std::string picture(picture_header, sizeof(picture_header));
    picture.append(512, (char)0x55);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, picture.data(), picture.size()));
    assert(0 == mpeg_ts_destroy(muxer));
    assert(0 == ctx.data.size() % 188);

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < partial_ts_size; i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_flush(demuxer));
    assert(ctx.outputs.empty());

    for (size_t i = partial_ts_size; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(ctx.outputs.empty());
    ctx.handler_result = -123;
    assert(-123 == ts_demuxer_flush(demuxer));
    assert(ctx.outputs.empty());

    ctx.handler_result = 0;
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(picture == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));

    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(0 == ts_demuxer_destroy(demuxer));
}

extern "C" void mpeg_packet_test(void)
{
    mpeg_packet_picture_spans_pes_test();
    mpeg_packet_continuity_gap_test();
    mpeg_packet_repeated_continuity_gap_test();
    mpeg_packet_prefix_metadata_test();
    mpeg_packet_picture_header_timestamp_test();
    mpeg_packet_picture_header_split_test();
    mpeg_packet_start_code_byte_split_test();
    mpeg_packet_incomplete_next_picture_flush_test();
    mpeg_packet_multiple_pictures_test();
    mpeg_packet_handler_error_test();
    mpeg_packet_handler_retry_test();
    mpeg_packet_handler_retry_before_append_test();
    mpeg_packet_flush_retry_before_discontinuity_test();
    mpeg_packet_size_limit_test();
    mpeg_ps_packet_flush_test();
    mpeg_ts_packet_flush_test();
}

#if defined(MPEG_PACKET_TEST_MAIN)
int main()
{
    mpeg_packet_test();
    return 0;
}
#endif
