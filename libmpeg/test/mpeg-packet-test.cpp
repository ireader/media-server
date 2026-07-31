#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

extern "C" {
#include "../source/mpeg-pes-internal.h"
#include "../source/mpeg-ts-internal.h"
#include "mpeg-ps.h"
#include "mpeg-ts.h"
}

struct mpeg_packet_output_t
{
    int codecid;
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
    output.codecid = codecid;
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
    pes_packet_discontinuity(&ctx->pes.pkt);
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

static void mpeg_packet_discontinuity_reset_test()
{
    struct packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.data = (uint8_t*)malloc(64);
    assert(NULL != pkt.data);
    pkt.capacity = 64;
    pkt.sid = PES_SID_VIDEO;
    pkt.codecid = PSI_STREAM_MPEG2;
    pkt.flags = MPEG_FLAG_IDR_FRAME;
    pkt.pts = 9000;
    pkt.dts = 9000;
    pkt.size = 8;
    pkt.vcl = 2;
    pkt.mpeg12_scan = 1;
    pkt.mpeg12_prefix = 2;
    pkt.mpeg12_picture = 3;
    pkt.mpeg12_prefix_flags = MPEG_FLAG_PACKET_LOST;
    pkt.mpeg12_picture_flags = MPEG_FLAG_PACKET_CORRUPT;
    pkt.mpeg12_picture_pts = 12600;
    pkt.mpeg12_picture_dts = 12600;
    pkt.mpeg12_tail_pts[0] = 16200;
    pkt.mpeg12_tail_dts[0] = 16200;
    pkt.mpeg12_tail_flags[0] = MPEG_FLAG_IDR_FRAME;
    pkt.mpeg12_have_prefix = 1;
    pkt.mpeg12_have_picture = 1;
    pkt.mpeg12_have_picture_timestamp = 1;
    pkt.mpeg12_tail_count = 1;
    pkt.mpeg12_discontinuity = 1;

    uint8_t* data = pkt.data;
    size_t capacity = pkt.capacity;
    pes_packet_discontinuity(&pkt);
    assert(data == pkt.data);
    assert(capacity == pkt.capacity);
    assert(0 == pkt.sid);
    assert(0 == pkt.codecid);
    assert(0 == pkt.flags);
    assert(0 == pkt.pts);
    assert(0 == pkt.dts);
    assert(0 == pkt.size);
    assert(0 == pkt.vcl);
    assert(0 == pkt.mpeg12_scan);
    assert(0 == pkt.mpeg12_prefix);
    assert(0 == pkt.mpeg12_picture);
    assert(0 == pkt.mpeg12_prefix_flags);
    assert(0 == pkt.mpeg12_picture_flags);
    assert(0 == pkt.mpeg12_picture_pts);
    assert(0 == pkt.mpeg12_picture_dts);
    assert(0 == pkt.mpeg12_tail_pts[0]);
    assert(0 == pkt.mpeg12_tail_dts[0]);
    assert(0 == pkt.mpeg12_tail_flags[0]);
    assert(0 == pkt.mpeg12_have_prefix);
    assert(0 == pkt.mpeg12_have_picture);
    assert(0 == pkt.mpeg12_have_picture_timestamp);
    assert(0 == pkt.mpeg12_tail_count);
    assert(0 == pkt.mpeg12_discontinuity);
    free(data);
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
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF
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
    mpeg_packet_test_input(&ctx, std::string(picture_b, sizeof(picture_b)),
        1, 12600);
    assert(ctx.outputs.empty());
    mpeg_packet_test_input(&ctx, std::string(picture_p, sizeof(picture_p)),
        1, 16200);
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_b, sizeof(picture_b)) == ctx.outputs[0].data);

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_flush_handler_error_test()
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
    int r = pes_packet(&ctx.pes.pkt, &ctx.pes, NULL, 0, &consume,
        0, mpeg_packet_onpacket, &ctx);
    assert(-123 == r);
    assert(0 == consume);
    assert(ctx.outputs.empty());

    ctx.handler_result = 0;
    mpeg_packet_test_input(&ctx, std::string(picture_p, sizeof(picture_p)),
        1, 12600);
    assert(ctx.outputs.empty());
    mpeg_packet_test_input(&ctx, std::string(picture_b, sizeof(picture_b)),
        1, 16200);
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[0].data);
    assert(12600 == ctx.outputs[0].pts);
    mpeg_packet_test_destroy(&ctx);
}

static int mpeg_packet_transition_onpacket(void* param, int /*program*/, int /*stream*/,
    int codecid, int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct mpeg_packet_test_t* ctx = (struct mpeg_packet_test_t*)param;
    struct mpeg_packet_output_t output;
    output.codecid = codecid;
    output.flags = flags;
    output.pts = pts;
    output.dts = dts;
    output.data.assign((const char*)data, bytes);
    ctx->outputs.push_back(output);
    return 0;
}

static void mpeg_packet_transition_input(struct mpeg_packet_test_t* ctx, int codecid,
    const std::string& data, int64_t pts)
{
    size_t consume = 0;
    ctx->pes.sid = PES_SID_VIDEO;
    ctx->pes.codecid = (uint8_t)codecid;
    ctx->pes.pts = pts;
    ctx->pes.dts = pts;
    ctx->pes.flags = 0;
    ctx->pes.len = 0;
    int r = pes_packet(&ctx->pes.pkt, &ctx->pes, data.data(), data.size(), &consume,
        1, mpeg_packet_transition_onpacket, ctx);
    assert(0 == r);
    assert(data.size() == consume);
}

static void mpeg_packet_codecid_transition_test()
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
    static const char mpeg4_first[] = {
        0x00, 0x00, 0x01, (char)0xB6, 0x11, 0x22
    };
    static const char mpeg4_second[] = {
        0x00, 0x00, 0x01, (char)0xB6, 0x33, 0x44
    };
    static const char h264_two_access_units[] = {
        0x00, 0x00, 0x00, 0x01, 0x09, (char)0xF0,
        0x00, 0x00, 0x00, 0x01, 0x65, (char)0x80, 0x11,
        0x00, 0x00, 0x00, 0x01, 0x09, (char)0xF0,
        0x00, 0x00, 0x00, 0x01, 0x41, (char)0x80, 0x22
    };
    static const char h265_picture_like[] = {
        0x00, 0x00, 0x00, 0x01, 0x00, 0x01, (char)0x80, 0x11
    };

    {
        struct mpeg_packet_test_t ctx;
        mpeg_packet_test_init(&ctx);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG1,
            std::string(picture_i, sizeof(picture_i)), 9000);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG2,
            std::string(picture_p, sizeof(picture_p)), 12600);
        assert(ctx.outputs.empty());
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG2,
            std::string(picture_b, sizeof(picture_b)), 16200);
        assert(1 == ctx.outputs.size());
        assert(PSI_STREAM_MPEG2 == ctx.outputs[0].codecid);
        assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        mpeg_packet_test_init(&ctx);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG1,
            std::string(picture_i, sizeof(picture_i)), 9000);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG4,
            std::string(mpeg4_first, sizeof(mpeg4_first)), 12600);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG4,
            std::string(mpeg4_second, sizeof(mpeg4_second)), 16200);
        assert(1 == ctx.outputs.size());
        assert(PSI_STREAM_MPEG4 == ctx.outputs[0].codecid);
        assert(std::string(mpeg4_first, sizeof(mpeg4_first)) == ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        mpeg_packet_test_init(&ctx);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG1,
            std::string(picture_i, sizeof(picture_i)), 9000);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_H264,
            std::string(h264_two_access_units, sizeof(h264_two_access_units)), 12600);
        assert(1 == ctx.outputs.size());
        assert(PSI_STREAM_H264 == ctx.outputs[0].codecid);
        assert(std::string(picture_i, sizeof(picture_i)) != ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        mpeg_packet_test_init(&ctx);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_H265,
            std::string(h265_picture_like, sizeof(h265_picture_like)), 9000);
        assert(ctx.outputs.empty());
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG1,
            std::string(picture_p, sizeof(picture_p)), 12600);
        assert(ctx.outputs.empty());
        assert(0 == ctx.pes.pkt.vcl);
        mpeg_packet_transition_input(&ctx, PSI_STREAM_MPEG1,
            std::string(picture_b, sizeof(picture_b)), 16200);
        assert(1 == ctx.outputs.size());
        assert(PSI_STREAM_MPEG1 == ctx.outputs[0].codecid);
        assert(std::string(picture_p, sizeof(picture_p)) == ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }
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
    assert(ctx.pes.pkt.capacity <= input.size());

    const char extra = 0;
    r = pes_packet(&ctx.pes.pkt, &ctx.pes, &extra, 1, &consume,
        0, mpeg_packet_onpacket, &ctx);
    assert(-EINVAL == r);
    assert(1 == consume);
    assert(input.size() == ctx.pes.pkt.size);

    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_growth_test()
{
    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);
    const size_t target = 10 * 1024 * 1024;
    const std::string chunk(184, (char)0xFF);
    size_t changes = 0;
    size_t previous_capacity = 0;

    for (size_t total = 0; total < target; )
    {
        size_t n = target - total < chunk.size() ? target - total : chunk.size();
        mpeg_packet_test_input(&ctx, chunk.substr(0, n), 0, 9000);
        if (ctx.pes.pkt.capacity != previous_capacity)
        {
            ++changes;
            previous_capacity = ctx.pes.pkt.capacity;
        }
        total += n;
    }

    assert(target == ctx.pes.pkt.size);
    assert(changes <= 32);
    assert(ctx.pes.pkt.capacity <= 10 * 1024 * 1024);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_pmt_clear_stream_state_test()
{
    struct pmt_t pmt;
    memset(&pmt, 0, sizeof(pmt));
    pmt.stream_count = 1;

    struct pes_t* pes = &pmt.streams[0];
    pes->codecid = PSI_STREAM_MPEG2;
    pes->have_pes_header = 1;
    pes->ts_pes_payload_remaining = 32;
    pes->ts_pes_header_bytes = 8;
    pes->ts_pes_payload_start = 1;
    pes->ts_pes_header[0] = 0x7F;
    pes->flags = MPEG_FLAG_PACKET_CORRUPT;
    pes->discontinuity = 7;
    pes->pkt.data = (uint8_t*)malloc(64);
    assert(NULL != pes->pkt.data);
    pes->pkt.size = 8;
    pes->pkt.capacity = 64;
    pes->pkt.codecid = PSI_STREAM_MPEG2;
    pes->pkt.vcl = 2;
    pes->pkt.mpeg12_scan = 5;
    pes->pkt.mpeg12_prefix = 3;
    pes->pkt.mpeg12_have_prefix = 1;
    pes->pkt.mpeg12_have_picture = 1;
    pes->pkt.mpeg12_tail_count = 3;
    pes->pkt.mpeg12_discontinuity = 6;
    pes->esinfo = (uint8_t*)malloc(4);
    assert(NULL != pes->esinfo);
    pes->esinfo_len = 4;

    pmt_clear(&pmt);
    assert(0 == pmt.stream_count);
    assert(NULL == pes->pkt.data);
    assert(0 == pes->pkt.size);
    assert(0 == pes->pkt.capacity);
    assert(0 == pes->pkt.codecid);
    assert(0 == pes->pkt.vcl);
    assert(0 == pes->pkt.mpeg12_scan);
    assert(0 == pes->pkt.mpeg12_prefix);
    assert(0 == pes->pkt.mpeg12_have_prefix);
    assert(0 == pes->pkt.mpeg12_have_picture);
    assert(0 == pes->pkt.mpeg12_tail_count);
    assert(0 == pes->pkt.mpeg12_discontinuity);
    assert(NULL == pes->esinfo);
    assert(0 == pes->esinfo_len);
    assert(0 == pes->have_pes_header);
    assert(0 == pes->ts_pes_payload_remaining);
    assert(0 == pes->ts_pes_header_bytes);
    assert(0 == pes->ts_pes_payload_start);
    assert(0 == pes->ts_pes_header[0]);
    assert(0 == pes->flags);
    assert(0 == pes->discontinuity);
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
    output.codecid = codecid;
    output.pts = pts;
    output.dts = dts;
    output.data.assign((const char*)data, bytes);
    ctx->outputs.push_back(output);
    return 0;
}

static void mpeg_ps_packet_continuous_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22, 0x33
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01, 0x33, 0x44, 0x55
    };
    static const uint8_t descriptor[] = { 0xFF, 0x00 };
    struct ps_muxer_func_t func;
    struct mpeg_ps_packet_test_t ctx;
    int r;

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
    r = ps_muxer_input(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, picture_i, sizeof(picture_i));
    assert(0 == r);
    r = ps_muxer_input(muxer, stream, 0,
        12600, 12600, picture_p, sizeof(picture_p));
    assert(0 == r);
    r = ps_muxer_destroy(muxer);
    assert(0 == r);

    struct ps_demuxer_t* demuxer = ps_demuxer_create(mpeg_ps_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    r = ps_demuxer_input(demuxer, (const uint8_t*)ctx.data.data(), ctx.data.size());
    assert((int)ctx.data.size() == r);
    assert(1 == ctx.outputs.size());
    assert(std::string(picture_i, sizeof(picture_i)) == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));
    r = ps_demuxer_destroy(demuxer);
    assert(0 == r);

    ctx.outputs.clear();
    ctx.handler_result = 123;
    demuxer = ps_demuxer_create(mpeg_ps_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    r = ps_demuxer_input(demuxer, (const uint8_t*)ctx.data.data(), ctx.data.size());
    assert(-123 == r);
    assert(ctx.outputs.empty());
    r = ps_demuxer_destroy(demuxer);
    assert(0 == r);
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
    output.codecid = codecid;
    output.pts = pts;
    output.dts = dts;
    output.data.assign((const char*)data, bytes);
    ctx->outputs.push_back(output);
    return 0;
}

static int mpeg_ts_packet_onpacket_any(void* param, int program, int /*stream*/, int codecid,
    int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct mpeg_ps_packet_test_t* ctx = (struct mpeg_ps_packet_test_t*)param;
    struct mpeg_packet_output_t output;
    assert(1 == program);
    if (0 != ctx->handler_result)
        return ctx->handler_result;
    output.flags = flags;
    output.codecid = codecid;
    output.pts = pts;
    output.dts = dts;
    output.data.assign((const char*)data, bytes);
    ctx->outputs.push_back(output);
    return 0;
}

static size_t mpeg_ts_find_pusi_packet(const std::string& data, size_t begin,
    size_t end, int pid)
{
    for (size_t i = begin; i + 188 <= end; i += 188)
    {
        const uint8_t* packet = (const uint8_t*)data.data() + i;
        assert(0x47 == packet[0]);
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == pid && 0 != (packet[1] & 0x40))
            return i;
    }
    assert(0);
    return end;
}

#if defined(MPEG_PACKET_INVALID_PES_TEST)
static size_t mpeg_ts_payload_offset(const uint8_t packet[188])
{
    size_t offset = 4;
    int adaptation_field_control = (packet[3] >> 4) & 0x03;
    assert(0 != (adaptation_field_control & 0x01));
    if (0 != (adaptation_field_control & 0x02))
        offset += 1 + packet[4];
    assert(offset < 188);
    return offset;
}
#endif

static std::string mpeg_ts_make_packet(int pid, int cc, int pusi,
    const char* payload, size_t bytes)
{
    assert(bytes > 0 && bytes <= 184);
    std::string packet(188, (char)0xFF);
    uint8_t* ptr = (uint8_t*)&packet[0];
    ptr[0] = 0x47;
    ptr[1] = (uint8_t)(((pid >> 8) & 0x1F) | (pusi ? 0x40 : 0));
    ptr[2] = (uint8_t)pid;
    size_t offset;
    if (184 == bytes)
    {
        ptr[3] = (uint8_t)(0x10 | (cc & 0x0F));
        offset = 4;
    }
    else
    {
        ptr[3] = (uint8_t)(0x30 | (cc & 0x0F));
        ptr[4] = (uint8_t)(183 - bytes);
        if (ptr[4] > 0)
            ptr[5] = 0;
        offset = 5 + ptr[4];
    }
    assert(offset + bytes == packet.size());
    memcpy(ptr + offset, payload, bytes);
    return packet;
}

#if defined(MPEG_PACKET_INVALID_PES_TEST)
static std::string mpeg_ts_collect_pid_payload(const std::string& data,
    size_t begin, size_t end, int pid)
{
    std::string payload;
    for (size_t i = begin; i + 188 <= end; i += 188)
    {
        const uint8_t* packet = (const uint8_t*)data.data() + i;
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid != pid || 0 == (packet[3] & 0x10))
            continue;
        size_t offset = mpeg_ts_payload_offset(packet);
        payload.append((const char*)packet + offset, 188 - offset);
    }
    return payload;
}
#endif

static void mpeg_ts_packet_tei_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string corrupt(picture_p, sizeof(picture_p));
    corrupt.append(512, (char)0x22);
    std::string next(picture_b, sizeof(picture_b));
    next.append(512, (char)0x33);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t corrupt_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, corrupt.data(), corrupt.size()));
    size_t corrupt_end = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 16200, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    size_t corrupt_packet = mpeg_ts_find_pusi_packet(ctx.data,
        corrupt_begin, corrupt_end, stream);
    ctx.data[corrupt_packet + 1] |= (char)0x80;

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(next == ctx.outputs[0].data);
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_discontinuity_indicator_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string boundary(picture_p, sizeof(picture_p));
    boundary.append(512, (char)0x22);
    std::string next(picture_b, sizeof(picture_b));
    next.append(512, (char)0x33);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t boundary_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        12600, 12600, boundary.data(), boundary.size()));
    size_t boundary_end = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 16200, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    size_t boundary_packet = mpeg_ts_find_pusi_packet(ctx.data,
        boundary_begin, boundary_end, stream);
    uint8_t* packet = (uint8_t*)&ctx.data[boundary_packet];
    assert(3 == ((packet[3] >> 4) & 0x03));
    assert(packet[4] > 0);
    packet[5] |= 0x80;

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(1 == ctx.outputs.size());
    assert(boundary == ctx.outputs[0].data);
    assert(0 == ts_demuxer_flush(demuxer));
    assert(2 == ctx.outputs.size());
    assert(next == ctx.outputs[1].data);
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_adaptation_only_discontinuity_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string boundary(picture_p, sizeof(picture_p));
    boundary.append(512, (char)0x22);
    std::string next(picture_b, sizeof(picture_b));
    next.append(512, (char)0x33);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t boundary_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, boundary.data(), boundary.size()));
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 16200, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    int last_cc = -1;
    for (size_t i = 0; i < boundary_begin; i += 188)
    {
        uint8_t* packet = (uint8_t*)&ctx.data[i];
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
            last_cc = packet[3] & 0x0F;
    }
    assert(last_cc >= 0);
    int reset_cc = (last_cc + 5) & 0x0F;

    std::string discontinuity(188, (char)0xFF);
    uint8_t* discontinuity_packet = (uint8_t*)&discontinuity[0];
    discontinuity_packet[0] = 0x47;
    discontinuity_packet[1] = (uint8_t)((stream >> 8) & 0x1F);
    discontinuity_packet[2] = (uint8_t)stream;
    discontinuity_packet[3] = (uint8_t)(0x20 | reset_cc);
    discontinuity_packet[4] = 183;
    discontinuity_packet[5] = 0x80;

    int cc = reset_cc;
    for (size_t i = boundary_begin; i < ctx.data.size(); i += 188)
    {
        uint8_t* packet = (uint8_t*)&ctx.data[i];
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
        {
            cc = (cc + 1) & 0x0F;
            packet[3] = (uint8_t)((packet[3] & 0xF0) | cc);
        }
    }

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < boundary_begin; i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(0 == ts_demuxer_input(demuxer,
        (const uint8_t*)discontinuity.data(), discontinuity.size()));
    for (size_t i = boundary_begin; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(1 == ctx.outputs.size());
    assert(boundary == ctx.outputs[0].data);
    assert(0 == (ctx.outputs[0].flags
        & (MPEG_FLAG_PACKET_CORRUPT | MPEG_FLAG_PACKET_LOST)));
    assert(0 == ts_demuxer_destroy(demuxer));
}

#if defined(MPEG_PACKET_INVALID_PES_TEST)
static void mpeg_ts_packet_split_pes_header_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const size_t cuts[] = { 1, 4, 8, 13, 184 };

    for (size_t n = 0; n < sizeof(cuts) / sizeof(cuts[0]); ++n)
    {
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
        std::string picture(picture_i, sizeof(picture_i));
        picture.append(512, (char)0x11);
        size_t pes_begin = ctx.data.size();
        assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
            9000, 8100, picture.data(), picture.size()));
        size_t pes_end = ctx.data.size();
        assert(0 == mpeg_ts_destroy(muxer));

        size_t pusi = mpeg_ts_find_pusi_packet(ctx.data, pes_begin, pes_end, stream);
        std::string pes = mpeg_ts_collect_pid_payload(ctx.data, pusi, pes_end, stream);
        if (184 == cuts[n])
        {
            uint8_t header_data_length = (uint8_t)pes[8];
            size_t header_length = 9 + header_data_length;
            size_t stuffing = 255 - header_data_length;
            uint16_t packet_length = (uint16_t)(
                ((uint16_t)(uint8_t)pes[4] << 8) | (uint8_t)pes[5]);
            assert((size_t)packet_length + stuffing <= UINT16_MAX);
            packet_length = (uint16_t)(packet_length + stuffing);
            pes.insert(header_length, stuffing, (char)0xFF);
            pes[4] = (char)(packet_length >> 8);
            pes[5] = (char)packet_length;
            pes[8] = (char)0xFF;
        }
        assert(cuts[n] < pes.size());
        int cc = ((const uint8_t*)ctx.data.data())[pusi + 3] & 0x0F;
        std::string split = ctx.data.substr(0, pusi);
        split += mpeg_ts_make_packet(stream, cc, 1, pes.data(), cuts[n]);
        size_t offset = cuts[n];
        while (offset < pes.size())
        {
            size_t bytes = pes.size() - offset < 184 ? pes.size() - offset : 184;
            cc = (cc + 1) & 0x0F;
            split += mpeg_ts_make_packet(stream, cc, 0, pes.data() + offset, bytes);
            offset += bytes;
        }

        struct ts_demuxer_t* demuxer = ts_demuxer_create(
            mpeg_ts_packet_onpacket, &ctx);
        assert(NULL != demuxer);
        for (size_t i = 0; i < split.size(); i += 188)
        {
            assert(0 == ts_demuxer_input(demuxer,
                (const uint8_t*)split.data() + i, 188));
        }
        assert(0 == ts_demuxer_flush(demuxer));
        assert(1 == ctx.outputs.size());
        assert(picture == ctx.outputs[0].data);
        assert(9000 == ctx.outputs[0].pts);
        assert(8100 == ctx.outputs[0].dts);
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));
        assert(0 == ts_demuxer_destroy(demuxer));
    }
}

static void mpeg_ts_packet_header_only_start_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string picture(picture_i, sizeof(picture_i));
    picture.append(512, (char)0x11);
    size_t pes_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 8100, picture.data(), picture.size()));
    size_t pes_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t pusi = mpeg_ts_find_pusi_packet(ctx.data, pes_begin, pes_end, stream);
    std::string pes = mpeg_ts_collect_pid_payload(ctx.data, pusi, pes_end, stream);
    size_t header = 9 + (uint8_t)pes[8];
    assert(header < pes.size());
    int cc = ((const uint8_t*)ctx.data.data())[pusi + 3] & 0x0F;
    std::string split = ctx.data.substr(0, pusi);
    split += mpeg_ts_make_packet(stream, cc, 1, pes.data(), header);
    size_t offset = header;
    while (offset < pes.size())
    {
        size_t bytes = pes.size() - offset < 184 ? pes.size() - offset : 184;
        cc = (cc + 1) & 0x0F;
        split += mpeg_ts_make_packet(stream, cc, 0, pes.data() + offset, bytes);
        offset += bytes;
    }

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < split.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)split.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(picture == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(8100 == ctx.outputs[0].dts);
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_unbounded_pes_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string picture(picture_i, sizeof(picture_i));
    picture.append(512, (char)0x11);
    size_t pes_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, picture.data(), picture.size()));
    size_t pes_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t pusi = mpeg_ts_find_pusi_packet(ctx.data, pes_begin, pes_end, stream);
    std::string pes = mpeg_ts_collect_pid_payload(ctx.data, pusi, pes_end, stream);
    pes[4] = 0;
    pes[5] = 0;
    int cc = ((const uint8_t*)ctx.data.data())[pusi + 3] & 0x0F;
    std::string rebuilt = ctx.data.substr(0, pusi);
    size_t offset = 0;
    while (offset < pes.size())
    {
        size_t bytes = pes.size() - offset < 184 ? pes.size() - offset : 184;
        rebuilt += mpeg_ts_make_packet(stream, cc, 0 == offset,
            pes.data() + offset, bytes);
        cc = (cc + 1) & 0x0F;
        offset += bytes;
    }

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < rebuilt.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)rebuilt.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(picture == ctx.outputs[0].data);
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_interleaved_streams_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    int stream1 = mpeg_ts_add_stream(muxer, PSI_STREAM_MPEG2, NULL, 0);
    int stream2 = mpeg_ts_add_stream(muxer, PSI_STREAM_MPEG2, NULL, 0);
    assert(stream1 > 0 && stream2 > 0 && stream1 != stream2);
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string second(picture_p, sizeof(picture_p));
    second.append(512, (char)0x22);
    size_t first_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream1, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t first_end = ctx.data.size();
    size_t second_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream2, 0,
        12600, 12600, second.data(), second.size()));
    size_t second_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t first_pusi = mpeg_ts_find_pusi_packet(
        ctx.data, first_begin, first_end, stream1);
    size_t second_pusi = mpeg_ts_find_pusi_packet(
        ctx.data, second_begin, second_end, stream2);
    std::string interleaved = ctx.data.substr(0, first_pusi);
    interleaved.append(ctx.data, first_end, second_pusi - first_end);
    size_t i = first_pusi;
    size_t j = second_pusi;
    while (i < first_end || j < second_end)
    {
        if (i < first_end)
        {
            interleaved.append(ctx.data, i, 188);
            i += 188;
        }
        if (j < second_end)
        {
            interleaved.append(ctx.data, j, 188);
            j += 188;
        }
    }

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t n = 0; n < interleaved.size(); n += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)interleaved.data() + n, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(2 == ctx.outputs.size());
    assert((first == ctx.outputs[0].data && second == ctx.outputs[1].data)
        || (second == ctx.outputs[0].data && first == ctx.outputs[1].data));
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_handler_error_state_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string second(picture_p, sizeof(picture_p));
    second.append(512, (char)0x22);
    std::string third(picture_b, sizeof(picture_b));
    third.append(512, (char)0x33);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t second_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, second.data(), second.size()));
    size_t second_end = ctx.data.size();
    size_t third_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 16200, third.data(), third.size()));
    size_t third_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t second_pusi = mpeg_ts_find_pusi_packet(
        ctx.data, second_begin, second_end, stream);
    size_t third_pusi = mpeg_ts_find_pusi_packet(
        ctx.data, third_begin, third_end, stream);
    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    ctx.handler_result = 123;
    int r = 0;
    size_t offset = 0;
    for (; offset < third_pusi && 0 == r; offset += 188)
        r = ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + offset, 188);
    assert(second_pusi < offset);
    assert(123 == r);
    assert(ctx.outputs.empty());

    ctx.handler_result = 0;
    for (offset = third_pusi; offset < ctx.data.size(); offset += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + offset, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(third == ctx.outputs[0].data);
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_invalid_pes_continuation_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_b[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string invalid(picture_p, sizeof(picture_p));
    invalid.append(512, (char)0x22);
    std::string next(picture_b, sizeof(picture_b));
    next.append(512, (char)0x33);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t invalid_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, invalid.data(), invalid.size()));
    size_t invalid_end = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 16200, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    size_t invalid_packet = mpeg_ts_find_pusi_packet(ctx.data,
        invalid_begin, invalid_end, stream);
    uint8_t* packet = (uint8_t*)&ctx.data[invalid_packet];
    packet[mpeg_ts_payload_offset(packet)] = 0x7F;

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(1 == ctx.outputs.size());
    assert(first == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(0 == ts_demuxer_flush(demuxer));
    assert(2 == ctx.outputs.size());
    assert(next == ctx.outputs[1].data);
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_invalid_pes_flush_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string invalid(picture_p, sizeof(picture_p));
    invalid.append(512, (char)0x22);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t invalid_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, invalid.data(), invalid.size()));
    size_t invalid_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t invalid_packet = mpeg_ts_find_pusi_packet(ctx.data,
        invalid_begin, invalid_end, stream);
    uint8_t* packet = (uint8_t*)&ctx.data[invalid_packet];
    packet[3] = (uint8_t)((packet[3] & 0xF0) | ((packet[3] + 1) & 0x0F));
    packet[mpeg_ts_payload_offset(packet)] = 0x7F;

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < invalid_begin; i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_input(demuxer,
        (const uint8_t*)ctx.data.data() + invalid_packet, 188));
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_flush(demuxer));
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_pes_payload_cc_gap_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string damaged(picture_i, sizeof(picture_i));
    damaged.append(1200, (char)0x11);
    size_t pes_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, damaged.data(), damaged.size()));
    size_t pes_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t pusi = mpeg_ts_find_pusi_packet(ctx.data, pes_begin, pes_end, stream);
    size_t dropped = pusi + 188 * 2;
    const uint8_t* dropped_packet = (const uint8_t*)ctx.data.data() + dropped;
    assert(stream == (((dropped_packet[1] << 8) | dropped_packet[2]) & 0x1FFF));
    assert(0 == (dropped_packet[1] & 0x40));
    assert(0x10 == (dropped_packet[3] & 0x30));
    ctx.data.erase(dropped, 188);

    int last_cc = -1;
    for (size_t i = ctx.data.size() - 188; ; i -= 188)
    {
        const uint8_t* packet = (const uint8_t*)ctx.data.data() + i;
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
        {
            last_cc = packet[3] & 0x0F;
            break;
        }
        assert(i >= 188);
    }
    assert(last_cc >= 0);

    std::string recovered(picture_p, sizeof(picture_p));
    recovered.append(272, (char)0x22);
    ctx.data += mpeg_ts_make_packet(stream, (last_cc + 1) & 0x0F,
        0, recovered.data(), 184);
    ctx.data += mpeg_ts_make_packet(stream, (last_cc + 2) & 0x0F,
        0, recovered.data() + 184, recovered.size() - 184);

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(recovered == ctx.outputs[0].data);
    assert(0 != (ctx.outputs[0].flags
        & (MPEG_FLAG_PACKET_CORRUPT | MPEG_FLAG_PACKET_LOST)));
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_audio_pes_payload_cc_gap_test()
{
    struct mpeg_ts_func_t func;
    struct mpeg_ps_packet_test_t ctx;

    memset(&func, 0, sizeof(func));
    func.alloc = mpeg_ps_packet_alloc;
    func.free = mpeg_ps_packet_free;
    func.write = mpeg_ts_packet_write;
    ctx.handler_result = 0;

    void* muxer = mpeg_ts_create(&func, &ctx);
    assert(NULL != muxer);
    int stream = mpeg_ts_add_stream(muxer, PSI_STREAM_AAC, NULL, 0);
    assert(stream > 0);
    std::string damaged(1200, (char)0x11);
    size_t pes_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        9000, 9000, damaged.data(), damaged.size()));
    size_t pes_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t pusi = mpeg_ts_find_pusi_packet(ctx.data, pes_begin, pes_end, stream);
    size_t dropped = pusi + 188 * 2;
    const uint8_t* dropped_packet = (const uint8_t*)ctx.data.data() + dropped;
    assert(stream == (((dropped_packet[1] << 8) | dropped_packet[2]) & 0x1FFF));
    assert(0 == (dropped_packet[1] & 0x40));
    assert(0x10 == (dropped_packet[3] & 0x30));
    ctx.data.erase(dropped, 188);

    int last_cc = -1;
    for (size_t i = ctx.data.size() - 188; ; i -= 188)
    {
        const uint8_t* packet = (const uint8_t*)ctx.data.data() + i;
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
        {
            last_cc = packet[3] & 0x0F;
            break;
        }
        assert(i >= 188);
    }
    assert(last_cc >= 0);

    const std::string continuation(184, (char)0x22);
    ctx.data += mpeg_ts_make_packet(stream, (last_cc + 1) & 0x0F,
        0, continuation.data(), continuation.size());

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket_any, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_invalid_adaptation_field_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string next(picture_p, sizeof(picture_p));
    next.append(512, (char)0x22);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t insert = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    int last_cc = -1;
    for (size_t i = 0; i < insert; i += 188)
    {
        const uint8_t* packet = (const uint8_t*)ctx.data.data() + i;
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
            last_cc = packet[3] & 0x0F;
    }
    assert(last_cc >= 0);

    std::string invalid(188, (char)0xFF);
    uint8_t* packet = (uint8_t*)&invalid[0];
    packet[0] = 0x47;
    packet[1] = (uint8_t)((stream >> 8) & 0x1F);
    packet[2] = (uint8_t)stream;
    packet[3] = (uint8_t)(0x20 | last_cc);
    packet[4] = 0xFF;
    ctx.data.insert(insert, invalid);

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(next == ctx.outputs[0].data);
    assert(0 == ts_demuxer_destroy(demuxer));
}
#endif

static void mpeg_ts_packet_pes_payload_boundary_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
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
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string next(picture_p, sizeof(picture_p));
    next.append(512, (char)0x22);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t insert = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    int last_cc = -1;
    for (size_t i = 0; i < insert; i += 188)
    {
        uint8_t* packet = (uint8_t*)&ctx.data[i];
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
            last_cc = packet[3] & 0x0F;
    }
    assert(last_cc >= 0);
    const std::string extra(32, (char)0x55);
    std::string packet = mpeg_ts_make_packet(stream, (last_cc + 1) & 0x0F,
        0, extra.data(), extra.size());
    ctx.data.insert(insert, packet);
    for (size_t i = insert + 188; i < ctx.data.size(); i += 188)
    {
        uint8_t* current = (uint8_t*)&ctx.data[i];
        int packet_pid = ((current[1] << 8) | current[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (current[3] & 0x10))
            current[3] = (uint8_t)((current[3] & 0xF0) | ((current[3] + 1) & 0x0F));
    }

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(1 == ctx.outputs.size());
    assert(first == ctx.outputs[0].data);
    assert(0 == ts_demuxer_flush(demuxer));
    assert(2 == ctx.outputs.size());
    assert(next == ctx.outputs[1].data);
    assert(0 == ts_demuxer_destroy(demuxer));
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
    mpeg_packet_discontinuity_reset_test();
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
    mpeg_packet_flush_handler_error_test();
    mpeg_packet_codecid_transition_test();
    mpeg_packet_size_limit_test();
    mpeg_packet_growth_test();
    mpeg_pmt_clear_stream_state_test();
    mpeg_ps_packet_continuous_test();
#if defined(MPEG_PACKET_INVALID_PES_TEST)
    mpeg_ts_packet_split_pes_header_test();
    mpeg_ts_packet_header_only_start_test();
    mpeg_ts_packet_unbounded_pes_test();
    mpeg_ts_packet_interleaved_streams_test();
    mpeg_ts_packet_handler_error_state_test();
    mpeg_ts_packet_invalid_pes_continuation_test();
    mpeg_ts_packet_invalid_pes_flush_test();
    mpeg_ts_packet_pes_payload_cc_gap_test();
    mpeg_ts_packet_audio_pes_payload_cc_gap_test();
    mpeg_ts_packet_invalid_adaptation_field_test();
#endif
    mpeg_ts_packet_pes_payload_boundary_test();
    mpeg_ts_packet_adaptation_only_discontinuity_test();
    mpeg_ts_packet_discontinuity_indicator_test();
    mpeg_ts_packet_tei_test();
    mpeg_ts_packet_flush_test();
}

#if defined(MPEG_PACKET_TEST_MAIN)
int main()
{
    mpeg_packet_test();
    return 0;
}
#endif
