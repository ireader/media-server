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
    int expected_codecid;
    int handler_result;
    int handler_calls;
    int handler_fail_at;
};

static std::string mpeg_packet_test_sequence(int codecid)
{
    static const char sequence[] = {
        0x00, 0x00, 0x01, (char)0xB3, 0x16, 0x01, 0x20, 0x13
    };
    static const char extension[] = {
        0x00, 0x00, 0x01, (char)0xB5, 0x10
    };
    std::string data(sequence, sizeof(sequence));
    if (PSI_STREAM_MPEG2 == codecid)
        data.append(extension, sizeof(extension));
    return data;
}

static std::string mpeg_packet_test_picture(int type)
{
    static const char pictures[][8] = {
        { 0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF },
        { 0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF },
        { 0x00, 0x00, 0x01, 0x00, 0x00, 0x18, (char)0xFF, (char)0xFF },
    };
    assert(type >= 1 && type <= 3);
    return std::string(pictures[type - 1], sizeof(pictures[0]));
}

static std::string mpeg_packet_test_slice(void)
{
    static const char slice[] = {
        0x00, 0x00, 0x01, 0x01, 0x11, 0x22, 0x33
    };
    return std::string(slice, sizeof(slice));
}

static std::string mpeg_packet_test_slice_prelude(void)
{
    static const char slices[] = {
        0x00, 0x00, 0x01, 0x05, (char)0x80,
        0x00, 0x00, 0x01, 0x07, (char)0x80,
        0x00, 0x00, 0x01, 0x08, (char)0x80,
    };
    return std::string(slices, sizeof(slices));
}

static std::string mpeg_packet_test_h264_nal(uint8_t type)
{
    std::string data("\x00\x00\x00\x01", 4);
    data.push_back((char)type);
    data.push_back((char)0x80);
    return data;
}

static void mpeg_packet_test_require(int condition)
{
    if (!condition)
        abort();
}

static std::string mpeg_packet_test_h26x_nal(int codecid,
    unsigned int nal_type, int first_slice)
{
    std::string data("\x00\x00\x00\x01", 4);
    if (PSI_STREAM_H264 == codecid)
    {
        mpeg_packet_test_require(nal_type < 32);
        data.push_back((char)nal_type);
    }
    else if (PSI_STREAM_H265 == codecid)
    {
        mpeg_packet_test_require(nal_type < 64);
        data.push_back((char)(nal_type << 1));
        data.push_back(0x01);
    }
    else
    {
        mpeg_packet_test_require(PSI_STREAM_H266 == codecid);
        mpeg_packet_test_require(nal_type < 32);
        data.push_back(0x00);
        data.push_back((char)((nal_type << 3) | 0x01));
    }

    data.push_back(first_slice ? (char)0x80 : 0);
    return data;
}

static std::string mpeg_packet_test_h26x_access_unit(int codecid,
    int idr, size_t padding)
{
    unsigned int aud_type;
    unsigned int vcl_type;
    if (PSI_STREAM_H264 == codecid)
    {
        aud_type = 9;
        vcl_type = idr ? 5 : 1;
    }
    else if (PSI_STREAM_H265 == codecid)
    {
        aud_type = 35;
        vcl_type = idr ? 19 : 1;
    }
    else
    {
        mpeg_packet_test_require(PSI_STREAM_H266 == codecid);
        aud_type = 20;
        vcl_type = idr ? 7 : 1;
    }

    std::string data = mpeg_packet_test_h26x_nal(codecid, aud_type, 0);
    data += mpeg_packet_test_h26x_nal(codecid, vcl_type, 1);
    data.append(padding, idr ? (char)0x55 : (char)0xAA);
    return data;
}

static std::string mpeg_packet_test_h26x_callback_data(int codecid,
    const std::string& access_unit)
{
    if (PSI_STREAM_H264 == codecid)
        return access_unit.substr(mpeg_packet_test_h26x_nal(codecid, 9, 0).size());
    if (PSI_STREAM_H265 == codecid)
        return access_unit.substr(mpeg_packet_test_h26x_nal(codecid, 35, 0).size());
    mpeg_packet_test_require(PSI_STREAM_H266 == codecid);
    return access_unit;
}

static int mpeg_packet_onpacket(void* param, int program, int stream, int codecid, int flags,
    int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct mpeg_packet_test_t* ctx = (struct mpeg_packet_test_t*)param;
    struct mpeg_packet_output_t output;
    assert(1 == program);
    assert(0x100 == stream);
    assert(ctx->expected_codecid < 0 || ctx->expected_codecid == codecid);
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
    ctx->expected_codecid = PSI_STREAM_MPEG1;
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

#if defined(MPEG_PACKET_INVALID_PES_TEST)
struct mpeg_packet_state_snapshot_t
{
    struct pes_t pes;
    std::string data;
};

static struct mpeg_packet_state_snapshot_t mpeg_packet_state_snapshot(
    const struct pes_t* pes)
{
    struct mpeg_packet_state_snapshot_t snapshot;
    snapshot.pes = *pes;
    if (pes->pkt.size > 0)
        snapshot.data.assign((const char*)pes->pkt.data, pes->pkt.size);
    return snapshot;
}

static void mpeg_packet_state_assert_equal(const struct pes_t* pes,
    const struct mpeg_packet_state_snapshot_t* snapshot)
{
    mpeg_packet_test_require(0 == memcmp(pes, &snapshot->pes, sizeof(*pes)));
    if (pes->pkt.size > 0)
        mpeg_packet_test_require(snapshot->data == std::string(
            (const char*)pes->pkt.data, pes->pkt.size));
}

static void mpeg_packet_test_input_require(struct mpeg_packet_test_t* ctx,
    const std::string& data, int start, int64_t pts, int flags = 0)
{
    size_t consume = 0;
    ctx->pes.pts = pts;
    ctx->pes.dts = pts;
    ctx->pes.flags = flags;
    int r = pes_packet(&ctx->pes.pkt, &ctx->pes, data.data(), data.size(),
        &consume, start, mpeg_packet_onpacket, ctx);
    mpeg_packet_test_require(0 == r);
    mpeg_packet_test_require(data.size() == consume);
}

static void mpeg_packet_invalid_cross_pes_span_test()
{
    struct mpeg_packet_test_t ctx;
    struct mpeg_packet_state_snapshot_t snapshot;
    const char trailing_prefix = 0;
    size_t consume = 0;
    int handler_calls;
    int r;

    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_RESERVED;
    ctx.expected_codecid = PSI_STREAM_RESERVED;
    ctx.pes.pts = 9000;
    ctx.pes.dts = 9000;
    r = pes_packet(&ctx.pes.pkt, &ctx.pes, &trailing_prefix, 1, &consume,
        1, mpeg_packet_onpacket, &ctx);
    mpeg_packet_test_require(0 == r);
    mpeg_packet_test_require(1 == consume);
    mpeg_packet_test_require(1 == ctx.pes.pkt.size);

    ctx.pes.pts = 12600;
    ctx.pes.dts = 12600;
    ctx.pes.flags = MPEG_FLAG_PACKET_CORRUPT;
    snapshot = mpeg_packet_state_snapshot(&ctx.pes);
    handler_calls = ctx.handler_calls;
    r = pes_packet(&ctx.pes.pkt, &ctx.pes, NULL, 1, &consume,
        1, mpeg_packet_onpacket, &ctx);
    mpeg_packet_test_require(-EINVAL == r);
    mpeg_packet_test_require(1 == consume);
    mpeg_packet_test_require(handler_calls == ctx.handler_calls);
    mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_maximum_input_after_packet_split_test()
{
    struct mpeg_packet_test_t ctx;
    const char first = 0x7F;
    std::string largest(10 * 1024 * 1024, (char)0xFF);

    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_MPEG4;
    ctx.expected_codecid = PSI_STREAM_MPEG4;
    mpeg_packet_test_input_require(&ctx, std::string(&first, 1), 1, 9000);
    mpeg_packet_test_input_require(&ctx, largest, 1, 12600);
    mpeg_packet_test_require(1 == ctx.handler_calls);
    mpeg_packet_test_require(largest.size() == ctx.pes.pkt.size);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_invalid_contract_test()
{
    const char input = 0x7F;
    const size_t max_size = 10 * 1024 * 1024;

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, &input, 1, NULL, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(NULL, &ctx.pes, &input, 1, &consume, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct packet_t pkt;
        struct mpeg_packet_test_t ctx;
        struct packet_t snapshot;
        size_t consume = 0;
        int r;
        memset(&pkt, 0, sizeof(pkt));
        mpeg_packet_test_init(&ctx);
        snapshot = pkt;
        r = pes_packet(&pkt, NULL, &input, 1, &consume, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_test_require(0 == memcmp(&pkt, &snapshot, sizeof(pkt)));
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, &input, 1, &consume, 1,
            NULL, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, NULL, 1, &consume, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        mpeg_packet_test_input_require(&ctx, std::string(&input, 1), 1, 9000);
        ctx.pes.pts = 12600;
        ctx.pes.dts = 12600;
        ctx.pes.flags = MPEG_FLAG_PACKET_LOST;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, NULL, 1, &consume, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        ctx.pes.pkt.data = (uint8_t*)malloc(1);
        mpeg_packet_test_require(NULL != ctx.pes.pkt.data);
        ctx.pes.pkt.data[0] = 0;
        ctx.pes.pkt.size = 1;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, &input, 1, &consume, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        ctx.pes.pkt.capacity = 1;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, &input, 1, &consume, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        std::string oversized(max_size + 1, (char)0xFF);
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, oversized.data(), oversized.size(),
            &consume, 1, mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(oversized.size() == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        ctx.pes.pkt.data = (uint8_t*)malloc(max_size + 1);
        mpeg_packet_test_require(NULL != ctx.pes.pkt.data);
        memset(ctx.pes.pkt.data, 0, max_size + 1);
        ctx.pes.pkt.size = max_size + 1;
        ctx.pes.pkt.capacity = max_size + 1;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, &input, 1, &consume, 0,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        struct mpeg_packet_state_snapshot_t snapshot;
        size_t consume = 0;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        ctx.pes.pkt.data = (uint8_t*)malloc(max_size + 1);
        mpeg_packet_test_require(NULL != ctx.pes.pkt.data);
        ctx.pes.pkt.capacity = max_size + 1;
        snapshot = mpeg_packet_state_snapshot(&ctx.pes);
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, &input, 1, &consume, 1,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(-EINVAL == r);
        mpeg_packet_test_require(1 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_state_assert_equal(&ctx.pes, &snapshot);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        size_t consume = 1;
        int r;
        mpeg_packet_test_init(&ctx);
        ctx.pes.pts = 9000;
        ctx.pes.dts = 9000;
        r = pes_packet(&ctx.pes.pkt, &ctx.pes, NULL, 0, &consume, 0,
            mpeg_packet_onpacket, &ctx);
        mpeg_packet_test_require(0 == r);
        mpeg_packet_test_require(0 == consume);
        mpeg_packet_test_require(0 == ctx.handler_calls);
        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_invalid_pes_test()
{
    mpeg_packet_invalid_cross_pes_span_test();
    mpeg_packet_maximum_input_after_packet_split_test();
    mpeg_packet_invalid_contract_test();
}
#endif

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
    for (size_t i = 0; i < 3; ++i)
    {
        pkt.mpeg12_tail_pts[i] = 16200 + (int64_t)i;
        pkt.mpeg12_tail_dts[i] = 16200 + (int64_t)i;
        pkt.mpeg12_tail_flags[i] = MPEG_FLAG_IDR_FRAME;
    }
    pkt.mpeg12_have_prefix = 1;
    pkt.mpeg12_have_picture = 1;
    pkt.mpeg12_have_picture_timestamp = 1;
    pkt.mpeg12_tail_count = 1;

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
    for (size_t i = 0; i < 3; ++i)
    {
        assert(0 == pkt.mpeg12_tail_pts[i]);
        assert(0 == pkt.mpeg12_tail_dts[i]);
        assert(0 == pkt.mpeg12_tail_flags[i]);
    }
    assert(0 == pkt.mpeg12_have_prefix);
    assert(0 == pkt.mpeg12_have_picture);
    assert(0 == pkt.mpeg12_have_picture_timestamp);
    assert(0 == pkt.mpeg12_tail_count);
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

    for (size_t bytes = 3; bytes <= 5; ++bytes)
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

static void mpeg_packet_reserved_mpeg12_test(int expected_codecid, int finite_pes)
{
    struct mpeg_packet_test_t ctx;
    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_RESERVED;
    ctx.expected_codecid = expected_codecid;

    std::string first = mpeg_packet_test_sequence(expected_codecid);
    first += mpeg_packet_test_picture(1);
    std::string second = mpeg_packet_test_picture(2);

    if (finite_pes)
        ctx.pes.len = (uint32_t)first.size();
    mpeg_packet_test_input(&ctx, first, 1, 9000);
    assert(ctx.outputs.empty());
    assert((finite_pes ? expected_codecid : PSI_STREAM_RESERVED) == ctx.pes.codecid);

    if (finite_pes)
        ctx.pes.len = (uint32_t)second.size();
    mpeg_packet_test_input(&ctx, second, 1, 12600);
    assert(expected_codecid == ctx.pes.codecid);
    assert(1 == ctx.outputs.size());
    assert(expected_codecid == ctx.outputs[0].codecid);
    assert(first == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(9000 == ctx.outputs[0].dts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_reserved_mpeg12_negative_test()
{
    std::vector<std::string> inputs;
    std::string incomplete_sequence = mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
    incomplete_sequence.resize(4);
    inputs.push_back(incomplete_sequence);

    std::string incomplete_extension = mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
    incomplete_extension.append("\x00\x00\x01\xB5", 4);
    inputs.push_back(incomplete_extension);

    inputs.push_back(mpeg_packet_test_picture(1) + mpeg_packet_test_slice());

    for (size_t i = 0; i < inputs.size(); ++i)
    {
        struct mpeg_packet_test_t ctx;
        mpeg_packet_test_init(&ctx);
        ctx.pes.codecid = PSI_STREAM_RESERVED;
        ctx.expected_codecid = PSI_STREAM_RESERVED;
        mpeg_packet_test_input(&ctx, inputs[i], 1, 9000);
        mpeg_packet_test_input(&ctx, std::string("x", 1), 1, 12600);
        assert(PSI_STREAM_RESERVED == ctx.pes.codecid);
        assert(1 == ctx.outputs.size());
        assert(inputs[i] == ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        std::string first = mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
        first.append("\x00\x00\x01\xB5\x20", 5);
        first.append("\x00\x00\x01\xB8", 4);
        first += mpeg_packet_test_picture(1);
        mpeg_packet_test_init(&ctx);
        ctx.pes.codecid = PSI_STREAM_RESERVED;
        ctx.expected_codecid = PSI_STREAM_MPEG1;
        mpeg_packet_test_input(&ctx, first, 1, 9000);
        mpeg_packet_test_input(&ctx, mpeg_packet_test_picture(2), 1, 12600);
        assert(PSI_STREAM_MPEG1 == ctx.pes.codecid);
        assert(1 == ctx.outputs.size());
        assert(first == ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_reserved_mpeg12_fragmented_test(int expected_codecid)
{
    struct mpeg_packet_test_t ctx;
    std::string first = mpeg_packet_test_sequence(expected_codecid);
    first += mpeg_packet_test_picture(1);
    std::string second = mpeg_packet_test_picture(2);
    size_t sequence_end = mpeg_packet_test_sequence(PSI_STREAM_MPEG1).size();
    size_t extension_end = mpeg_packet_test_sequence(expected_codecid).size();
    size_t picture_split = extension_end + 4;

    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_RESERVED;
    ctx.expected_codecid = expected_codecid;
    mpeg_packet_test_input(&ctx, first.substr(0, sequence_end), 1, 9000);
    if (PSI_STREAM_MPEG2 == expected_codecid)
    {
        mpeg_packet_test_input(&ctx, first.substr(sequence_end, 2), 0, 9000);
        mpeg_packet_test_input(&ctx,
            first.substr(sequence_end + 2, picture_split - sequence_end - 2), 0, 9000);
        mpeg_packet_test_input(&ctx, first.substr(picture_split), 0, 9000);
    }
    else
    {
        mpeg_packet_test_input(&ctx, first.substr(sequence_end, 4), 0, 9000);
        mpeg_packet_test_input(&ctx, first.substr(sequence_end + 4), 0, 9000);
    }
    mpeg_packet_test_input(&ctx, second, 1, 12600);
    assert(expected_codecid == ctx.pes.codecid);
    assert(1 == ctx.outputs.size());
    assert(first == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(9000 == ctx.outputs[0].dts);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_reserved_mpeg12_cross_pes_probe_test()
{
    static const int codecs[] = { PSI_STREAM_MPEG1, PSI_STREAM_MPEG2 };
    for (size_t i = 0; i < sizeof(codecs) / sizeof(codecs[0]); ++i)
    {
        struct mpeg_packet_test_t ctx;
        const int codecid = codecs[i];
        std::string sequence = mpeg_packet_test_sequence(codecid);
        std::string first;
        std::string second;
        if (PSI_STREAM_MPEG1 == codecid)
        {
            first = sequence.substr(0, 3);
            second = sequence.substr(3);
        }
        else
        {
            first = sequence.substr(0, sequence.size() - 1);
            second = sequence.substr(sequence.size() - 1);
        }
        second += mpeg_packet_test_picture(1);
        second += mpeg_packet_test_slice();

        mpeg_packet_test_init(&ctx);
        ctx.pes.codecid = PSI_STREAM_RESERVED;
        ctx.expected_codecid = PSI_STREAM_RESERVED;
        ctx.pes.len = (uint32_t)first.size();
        mpeg_packet_test_input(&ctx, first, 1, 9000, MPEG_FLAG_PACKET_LOST);
        assert(ctx.outputs.empty());
        assert(PSI_STREAM_RESERVED == ctx.pes.codecid);

        ctx.pes.len = (uint32_t)second.size();
        mpeg_packet_test_input(&ctx, second, 1, 12600,
            MPEG_FLAG_PACKET_CORRUPT);
        assert(ctx.outputs.empty());
        assert(codecid == ctx.pes.codecid);

        ctx.expected_codecid = codecid;
        std::string third = mpeg_packet_test_picture(2);
        ctx.pes.len = (uint32_t)third.size();
        mpeg_packet_test_input(&ctx, third, 1, 16200);
        assert(1 == ctx.outputs.size());
        assert(first + second == ctx.outputs[0].data);
        assert(12600 == ctx.outputs[0].pts);
        assert(12600 == ctx.outputs[0].dts);
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));
        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_reserved_mpeg12_independent_probe_test()
{
    std::vector<std::string> inputs;
    inputs.push_back(std::string("opaque\0", 7));
    std::string ambiguous = mpeg_packet_test_slice_prelude();
    ambiguous.push_back(0);
    inputs.push_back(ambiguous);
    std::string second = mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
    second += mpeg_packet_test_picture(1);
    second += mpeg_packet_test_slice();

    for (size_t i = 0; i < inputs.size(); ++i)
    {
        struct mpeg_packet_test_t ctx;
        const std::string& first = inputs[i];
        mpeg_packet_test_init(&ctx);
        ctx.pes.codecid = PSI_STREAM_RESERVED;
        ctx.expected_codecid = -1;
        ctx.pes.len = (uint32_t)first.size();
        mpeg_packet_test_input(&ctx, first, 1, 9000,
            MPEG_FLAG_PACKET_LOST);
        assert(ctx.outputs.empty());

        ctx.pes.len = (uint32_t)second.size();
        mpeg_packet_test_input(&ctx, second, 1, 12600,
            MPEG_FLAG_PACKET_CORRUPT);
        assert(1 == ctx.outputs.size());
        assert(PSI_STREAM_RESERVED == ctx.outputs[0].codecid);
        assert(first == ctx.outputs[0].data);
        assert(9000 == ctx.outputs[0].pts);
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
        assert(PSI_STREAM_MPEG1 == ctx.pes.codecid);

        ctx.expected_codecid = PSI_STREAM_MPEG1;
        std::string third = mpeg_packet_test_picture(2);
        ctx.pes.len = (uint32_t)third.size();
        mpeg_packet_test_input(&ctx, third, 1, 16200);
        assert(2 == ctx.outputs.size());
        assert(second == ctx.outputs[1].data);
        assert(12600 == ctx.outputs[1].pts);
        assert(0 != (ctx.outputs[1].flags & MPEG_FLAG_PACKET_CORRUPT));
        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_reserved_mpeg12_need_more_precedes_h26x_test()
{
    const std::string sequence =
        mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
    std::string picture = mpeg_packet_test_picture(1);
    picture += mpeg_packet_test_slice();
    static const size_t splits[] = { 1, 2, 3, 8 };
    assert(sequence.size() == splits[3]);
    for (size_t i = 0; i < sizeof(splits) / sizeof(splits[0]); ++i)
    {
        struct mpeg_packet_test_t ctx;
        const size_t split = splits[i];
        std::string first = mpeg_packet_test_slice_prelude();
        first.append(sequence, 0, split);
        std::string second = sequence.substr(split);
        second += picture;

        mpeg_packet_test_init(&ctx);
        ctx.pes.codecid = PSI_STREAM_RESERVED;
        ctx.expected_codecid = PSI_STREAM_H264;
        ctx.pes.len = (uint32_t)first.size();
        mpeg_packet_test_input(&ctx, first, 1, 9000,
            MPEG_FLAG_PACKET_LOST);
        assert(ctx.outputs.empty());
        assert(PSI_STREAM_RESERVED == ctx.pes.codecid);
        assert(PSI_STREAM_RESERVED == ctx.pes.pkt.codecid);

        ctx.expected_codecid = PSI_STREAM_MPEG1;
        ctx.pes.len = (uint32_t)second.size();
        mpeg_packet_test_input(&ctx, second, 1, 12600,
            MPEG_FLAG_PACKET_CORRUPT);
        assert(ctx.outputs.empty());
        assert(PSI_STREAM_MPEG1 == ctx.pes.codecid);

        std::string third = mpeg_packet_test_picture(2);
        ctx.pes.len = (uint32_t)third.size();
        mpeg_packet_test_input(&ctx, third, 1, 16200);
        assert(1 == ctx.outputs.size());
        assert(sequence + picture == ctx.outputs[0].data);
        assert(12600 == ctx.outputs[0].pts);
        assert(12600 == ctx.outputs[0].dts);
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));
        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_reserved_mpeg12_need_more_bounded_test()
{
    struct mpeg_packet_test_t ctx;
    const std::string sequence =
        mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
    std::string first = mpeg_packet_test_slice_prelude();
    first += sequence;

    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_RESERVED;
    ctx.expected_codecid = -1;
    ctx.pes.len = (uint32_t)first.size();
    mpeg_packet_test_input(&ctx, first, 1, 9000,
        MPEG_FLAG_PACKET_LOST);
    assert(ctx.outputs.empty());

    const std::string second("x", 1);
    ctx.pes.len = (uint32_t)second.size();
    mpeg_packet_test_input(&ctx, second, 1, 12600,
        MPEG_FLAG_PACKET_CORRUPT);
    assert(1 == ctx.outputs.size());
    assert(PSI_STREAM_RESERVED == ctx.outputs[0].codecid);
    assert(first == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
    assert(PSI_STREAM_RESERVED == ctx.pes.codecid);
    assert(PSI_STREAM_RESERVED == ctx.pes.pkt.codecid);

    std::string third = sequence;
    third += mpeg_packet_test_picture(1);
    third += mpeg_packet_test_slice();
    ctx.pes.len = (uint32_t)third.size();
    mpeg_packet_test_input(&ctx, third, 1, 16200);
    assert(2 == ctx.outputs.size());
    assert(PSI_STREAM_RESERVED == ctx.outputs[1].codecid);
    assert(second == ctx.outputs[1].data);
    assert(PSI_STREAM_MPEG1 == ctx.pes.codecid);

    ctx.expected_codecid = PSI_STREAM_MPEG1;
    std::string fourth = mpeg_packet_test_picture(2);
    ctx.pes.len = (uint32_t)fourth.size();
    mpeg_packet_test_input(&ctx, fourth, 1, 19800);
    assert(3 == ctx.outputs.size());
    assert(third == ctx.outputs[2].data);
    assert(16200 == ctx.outputs[2].pts);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_reserved_mpeg12_metadata_test()
{
    struct mpeg_packet_test_t ctx;
    std::string first = mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
    first += mpeg_packet_test_picture(1);

    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_RESERVED;
    ctx.expected_codecid = PSI_STREAM_MPEG1;
    mpeg_packet_test_input(&ctx, first, 1, 9000, MPEG_FLAG_PACKET_LOST);
    mpeg_packet_test_input(&ctx, mpeg_packet_test_picture(2), 1, 12600,
        MPEG_FLAG_PACKET_CORRUPT);
    assert(1 == ctx.outputs.size());
    assert(first == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(9000 == ctx.outputs[0].dts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
    assert(0 == (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_reserved_mpeg12_handler_error_test()
{
    struct mpeg_packet_test_t ctx;
    std::string first = mpeg_packet_test_sequence(PSI_STREAM_MPEG1);
    std::string second = mpeg_packet_test_picture(2);
    size_t consume = 0;
    int r;
    first += mpeg_packet_test_picture(1);

    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_RESERVED;
    ctx.expected_codecid = PSI_STREAM_MPEG1;
    mpeg_packet_test_input(&ctx, first, 1, 9000);
    ctx.handler_result = -789;
    ctx.pes.pts = 12600;
    ctx.pes.dts = 12600;
    r = pes_packet(&ctx.pes.pkt, &ctx.pes, second.data(), second.size(), &consume,
        1, mpeg_packet_onpacket, &ctx);
    assert(-789 == r);
    assert(second.size() == consume);
    assert(1 == ctx.handler_calls);
    assert(0 == ctx.pes.pkt.size);
    assert(0 == ctx.pes.pkt.mpeg12_have_picture);
    assert(0 == ctx.pes.pkt.mpeg12_have_prefix);

    ctx.handler_result = 0;
    mpeg_packet_test_input(&ctx, first, 1, 16200);
    mpeg_packet_test_input(&ctx, second, 1, 19800);
    assert(2 == ctx.handler_calls);
    assert(1 == ctx.outputs.size());
    assert(first == ctx.outputs[0].data);
    assert(16200 == ctx.outputs[0].pts);
    mpeg_packet_test_destroy(&ctx);
}

static void mpeg_packet_reserved_h264_and_mpeg4_test()
{
    {
        struct mpeg_packet_test_t ctx;
        std::string first = mpeg_packet_test_h264_nal(0x67);
        first += mpeg_packet_test_h264_nal(0x68);
        first += mpeg_packet_test_h264_nal(0x09);
        first += mpeg_packet_test_h264_nal(0x65);
        std::string second = mpeg_packet_test_h264_nal(0x09);
        second += mpeg_packet_test_h264_nal(0x41);

        mpeg_packet_test_init(&ctx);
        ctx.pes.codecid = PSI_STREAM_RESERVED;
        ctx.expected_codecid = PSI_STREAM_H264;
        ctx.pes.len = (uint32_t)first.size();
        mpeg_packet_test_input(&ctx, first, 1, 9000);
        assert(PSI_STREAM_H264 == ctx.pes.codecid);
        mpeg_packet_test_input(&ctx, second, 1, 12600);
        assert(PSI_STREAM_H264 == ctx.pes.codecid);
        assert(!ctx.outputs.empty());
        assert(PSI_STREAM_H264 == ctx.outputs[0].codecid);
        mpeg_packet_test_destroy(&ctx);
    }

    {
        struct mpeg_packet_test_t ctx;
        std::string mpeg4("\x00\x00\x01\xB0\x00", 5);

        mpeg_packet_test_init(&ctx);
        ctx.pes.codecid = PSI_STREAM_RESERVED;
        ctx.expected_codecid = PSI_STREAM_MPEG4;
        ctx.pes.len = (uint32_t)mpeg4.size();
        mpeg_packet_test_input(&ctx, mpeg4, 1, 9000);
        assert(PSI_STREAM_MPEG4 == ctx.pes.codecid);
        mpeg_packet_test_input(&ctx, std::string("x", 1), 1, 12600);
        assert(1 == ctx.outputs.size());
        assert(PSI_STREAM_MPEG4 == ctx.outputs[0].codecid);
        assert(mpeg4 == ctx.outputs[0].data);
        mpeg_packet_test_destroy(&ctx);
    }
}

static void mpeg_packet_reserved_h264_append_failure_atomic_test()
{
    struct mpeg_packet_test_t ctx;
    std::string old = mpeg_packet_test_h264_nal(0x67);
    old += mpeg_packet_test_h264_nal(0x68);
    old += mpeg_packet_test_h264_nal(0x09);
    old += mpeg_packet_test_h264_nal(0x65);
    old += mpeg_packet_test_h264_nal(0x09);
    old += mpeg_packet_test_h264_nal(0x41);
    old.resize(10 * 1024 * 1024, (char)0xFF);

    mpeg_packet_test_init(&ctx);
    ctx.pes.codecid = PSI_STREAM_RESERVED;
    ctx.expected_codecid = PSI_STREAM_RESERVED;
    mpeg_packet_test_input(&ctx, old, 1, 9000, MPEG_FLAG_PACKET_LOST);
    assert(PSI_STREAM_RESERVED == ctx.pes.codecid);
    assert(PSI_STREAM_RESERVED == ctx.pes.pkt.codecid);

    uint8_t* data = ctx.pes.pkt.data;
    const size_t size = ctx.pes.pkt.size;
    const size_t capacity = ctx.pes.pkt.capacity;
    const int64_t pts = ctx.pes.pkt.pts;
    const int64_t dts = ctx.pes.pkt.dts;
    const int flags = ctx.pes.pkt.flags;
    const char extra = 0;
    size_t consume = 0;
    ctx.pes.pts = 12600;
    ctx.pes.dts = 12600;
    ctx.pes.flags = MPEG_FLAG_PACKET_CORRUPT;
    int r = pes_packet(&ctx.pes.pkt, &ctx.pes, &extra, 1, &consume,
        1, mpeg_packet_onpacket, &ctx);
    assert(-EINVAL == r);
    assert(1 == consume);
    assert(PSI_STREAM_RESERVED == ctx.pes.codecid);
    assert(PSI_STREAM_RESERVED == ctx.pes.pkt.codecid);
    assert(data == ctx.pes.pkt.data);
    assert(size == ctx.pes.pkt.size);
    assert(capacity == ctx.pes.pkt.capacity);
    assert(pts == ctx.pes.pkt.pts);
    assert(dts == ctx.pes.pkt.dts);
    assert(flags == ctx.pes.pkt.flags);

    ctx.expected_codecid = PSI_STREAM_H264;
    consume = 1;
    r = pes_packet(&ctx.pes.pkt, &ctx.pes, NULL, 0, &consume,
        1, mpeg_packet_onpacket, &ctx);
    assert(0 == r);
    assert(0 == consume);
    assert(PSI_STREAM_H264 == ctx.pes.codecid);
    assert(PSI_STREAM_H264 == ctx.pes.pkt.codecid);
    assert(!ctx.outputs.empty());
    assert(9000 == ctx.outputs[0].pts);
    assert(9000 == ctx.outputs[0].dts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
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
    struct ts_pes_state_t* state = &pmt.stream_states[0];
    pes->codecid = PSI_STREAM_MPEG2;
    pes->flags = MPEG_FLAG_PACKET_CORRUPT;
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
    pes->esinfo = (uint8_t*)malloc(4);
    assert(NULL != pes->esinfo);
    pes->esinfo_len = 4;
    state->payload_remaining = 32;
    state->header_bytes = 8;
    state->have_header = 1;
    state->payload_start = 1;
    state->have_payload_cc = 1;
    state->payload_cc = 7;
    memset(state->header, 0x7F, sizeof(state->header));

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
    assert(NULL == pes->esinfo);
    assert(0 == pes->esinfo_len);
    assert(0 == pes->flags);
    struct ts_pes_state_t empty;
    memset(&empty, 0, sizeof(empty));
    assert(0 == memcmp(state, &empty, sizeof(empty)));
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

static int mpeg_ps_packet_onpacket_any(void* param, int /*stream*/, int codecid,
    int flags, int64_t pts, int64_t dts, const void* data, size_t bytes)
{
    struct mpeg_ps_packet_test_t* ctx = (struct mpeg_ps_packet_test_t*)param;
    struct mpeg_packet_output_t output;
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

static void mpeg_ps_packet_remove_psm(std::string* data)
{
    static const char start[] = { 0x00, 0x00, 0x01, (char)0xBC };
    const std::string marker(start, sizeof(start));
    size_t offset = 0;
    size_t removed = 0;
    while (std::string::npos != (offset = data->find(marker, offset)))
    {
        assert(offset + 6 <= data->size());
        const uint8_t* p = (const uint8_t*)data->data() + offset;
        size_t bytes = 6 + (((size_t)p[4] << 8) | p[5]);
        assert(offset + bytes <= data->size());
        data->erase(offset, bytes);
        ++removed;
    }
    assert(removed > 0);
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

static void mpeg_ps_packet_no_psm_variable_chunks_test()
{
    static const int codecs[] = { PSI_STREAM_MPEG1, PSI_STREAM_MPEG2 };
    static const size_t chunks[] = { 1, 7, 2, 19, 3, 31, 5, 64, 11 };
    static const uint8_t descriptor[] = { 0xFF, 0x00 };
    for (size_t n = 0; n < sizeof(codecs) / sizeof(codecs[0]); ++n)
    {
        struct ps_muxer_func_t func;
        struct mpeg_ps_packet_test_t ctx;
        memset(&func, 0, sizeof(func));
        func.alloc = mpeg_ps_packet_alloc;
        func.free = mpeg_ps_packet_free;
        func.write = mpeg_ps_packet_write;
        ctx.handler_result = 0;

        const int codecid = codecs[n];
        std::string sequence = mpeg_packet_test_sequence(codecid);
        std::string first;
        std::string second;
        if (PSI_STREAM_MPEG1 == codecid)
        {
            first = sequence.substr(0, 3);
            second = sequence.substr(3);
        }
        else
        {
            first = sequence.substr(0, sequence.size() - 1);
            second = sequence.substr(sequence.size() - 1);
        }
        second += mpeg_packet_test_picture(1);
        second += mpeg_packet_test_slice();
        std::string third = mpeg_packet_test_picture(2);
        third += mpeg_packet_test_slice();

        struct ps_muxer_t* muxer = ps_muxer_create(&func, &ctx);
        assert(NULL != muxer);
        int stream = ps_muxer_add_stream(muxer, codecid,
            descriptor, sizeof(descriptor));
        assert(PES_SID_VIDEO == stream);
        assert(0 == ps_muxer_input(muxer, stream, MPEG_FLAG_IDR_FRAME,
            9000, 8100, first.data(), first.size()));
        assert(0 == ps_muxer_input(muxer, stream, MPEG_FLAG_IDR_FRAME,
            12600, 11700, second.data(), second.size()));
        assert(0 == ps_muxer_input(muxer, stream, 0,
            16200, 15300, third.data(), third.size()));
        assert(0 == ps_muxer_destroy(muxer));
        mpeg_ps_packet_remove_psm(&ctx.data);

        struct ps_demuxer_t* demuxer = ps_demuxer_create(
            mpeg_ps_packet_onpacket_any, &ctx);
        assert(NULL != demuxer);
        std::string pending;
        size_t offset = 0;
        size_t chunk = 0;
        while (offset < ctx.data.size())
        {
            size_t bytes = chunks[chunk % (sizeof(chunks) / sizeof(chunks[0]))];
            if (bytes > ctx.data.size() - offset)
                bytes = ctx.data.size() - offset;
            pending.append(ctx.data, offset, bytes);
            offset += bytes;
            ++chunk;

            int r = ps_demuxer_input(demuxer,
                (const uint8_t*)pending.data(), pending.size());
            assert(r >= 0 && (size_t)r <= pending.size());
            assert(r > 0);
            pending.erase(0, (size_t)r);
        }
        assert(pending.empty());
        assert(1 == ctx.outputs.size());
        assert(codecid == ctx.outputs[0].codecid);
        assert(first + second == ctx.outputs[0].data);
        assert(12600 == ctx.outputs[0].pts);
        assert(11700 == ctx.outputs[0].dts);
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_IDR_FRAME));
        assert(0 == ps_demuxer_destroy(demuxer));
    }
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

static void mpeg_ts_demux_packets(const std::string& data,
    struct mpeg_ps_packet_test_t* ctx)
{
    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)data.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_initial_cc_test()
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
    struct mpeg_ps_packet_test_t mux;

    memset(&func, 0, sizeof(func));
    func.alloc = mpeg_ps_packet_alloc;
    func.free = mpeg_ps_packet_free;
    func.write = mpeg_ts_packet_write;
    mux.handler_result = 0;

    void* muxer = mpeg_ts_create(&func, &mux);
    assert(NULL != muxer);
    int stream = mpeg_ts_add_stream(muxer, PSI_STREAM_MPEG2, NULL, 0);
    assert(stream > 0);
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string second(picture_p, sizeof(picture_p));
    second.append(512, (char)0x22);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, second.data(), second.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    struct mpeg_ps_packet_test_t baseline;
    baseline.handler_result = 0;
    mpeg_ts_demux_packets(mux.data, &baseline);
    assert(2 == baseline.outputs.size());

    static const int initial_cc[] = { 0, 1, 15 };
    for (size_t n = 0; n < sizeof(initial_cc) / sizeof(initial_cc[0]); ++n)
    {
        std::string rewritten = mux.data;
        int cc = initial_cc[n];
        bool found = false;
        for (size_t i = 0; i < rewritten.size(); i += 188)
        {
            uint8_t* packet = (uint8_t*)&rewritten[i];
            int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
            if (packet_pid != stream || 0 == (packet[3] & 0x10))
                continue;
            packet[3] = (uint8_t)((packet[3] & 0xF0) | cc);
            cc = (cc + 1) & 0x0F;
            found = true;
        }
        assert(found);

        struct mpeg_ps_packet_test_t actual;
        actual.handler_result = 0;
        mpeg_ts_demux_packets(rewritten, &actual);
        assert(baseline.outputs.size() == actual.outputs.size());
        for (size_t i = 0; i < baseline.outputs.size(); ++i)
        {
            assert(baseline.outputs[i].data == actual.outputs[i].data);
            assert(baseline.outputs[i].pts == actual.outputs[i].pts);
            assert(baseline.outputs[i].dts == actual.outputs[i].dts);
            assert(baseline.outputs[i].flags == actual.outputs[i].flags);
        }
    }
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

static void mpeg_ts_packet_duplicate_payload_test()
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
    struct mpeg_ps_packet_test_t mux;

    memset(&func, 0, sizeof(func));
    func.alloc = mpeg_ps_packet_alloc;
    func.free = mpeg_ps_packet_free;
    func.write = mpeg_ts_packet_write;
    mux.handler_result = 0;

    void* muxer = mpeg_ts_create(&func, &mux);
    assert(NULL != muxer);
    int stream = mpeg_ts_add_stream(muxer, PSI_STREAM_MPEG2, NULL, 0);
    assert(stream > 0);
    std::string first(picture_i, sizeof(picture_i));
    first.append(128, (char)0x11);
    first.append(128, (char)0x22);
    first.append(128, (char)0x33);
    first.append(128, (char)0x44);
    std::string next(picture_p, sizeof(picture_p));
    next.append(512, (char)0x22);
    size_t first_begin = mux.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    size_t first_end = mux.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    const std::string baseline_input = mux.data;
    std::string duplicate_input = baseline_input;
    size_t duplicate = first_end;
    for (size_t i = first_begin; i < first_end; i += 188)
    {
        const uint8_t* packet = (const uint8_t*)baseline_input.data() + i;
        int pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (stream == pid && 0 == (packet[1] & 0x40)
            && 0 != (packet[3] & 0x10))
        {
            duplicate = i;
            break;
        }
    }
    assert(first_end != duplicate);
    duplicate_input.insert(duplicate + 188, baseline_input, duplicate, 188);

    struct mpeg_ps_packet_test_t baseline;
    baseline.handler_result = 0;
    mpeg_ts_demux_packets(baseline_input, &baseline);
    struct mpeg_ps_packet_test_t actual;
    actual.handler_result = 0;
    mpeg_ts_demux_packets(duplicate_input, &actual);

    assert(baseline.outputs.size() == actual.outputs.size());
    for (size_t i = 0; i < baseline.outputs.size(); ++i)
    {
        assert(baseline.outputs[i].data == actual.outputs[i].data);
        assert(baseline.outputs[i].flags == actual.outputs[i].flags);
        assert(baseline.outputs[i].pts == actual.outputs[i].pts);
        assert(baseline.outputs[i].dts == actual.outputs[i].dts);
    }
}

static void mpeg_ts_packet_adaptation_only_cc_test()
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
    struct mpeg_ps_packet_test_t mux;
    memset(&func, 0, sizeof(func));
    func.alloc = mpeg_ps_packet_alloc;
    func.free = mpeg_ps_packet_free;
    func.write = mpeg_ts_packet_write;
    mux.handler_result = 0;

    void* muxer = mpeg_ts_create(&func, &mux);
    assert(NULL != muxer);
    int stream = mpeg_ts_add_stream(muxer, PSI_STREAM_MPEG2, NULL, 0);
    assert(stream > 0);
    std::string first(picture_i, sizeof(picture_i));
    first.append(512, (char)0x11);
    std::string next(picture_p, sizeof(picture_p));
    next.append(512, (char)0x22);
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 9000, first.data(), first.size()));
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 12600, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    size_t insert = mux.data.size();
    int previous_cc = -1;
    for (size_t i = 0; i < mux.data.size(); i += 188)
    {
        const uint8_t* packet = (const uint8_t*)mux.data.data() + i;
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid != stream || 0 == (packet[3] & 0x10))
            continue;
        if (previous_cc >= 0)
        {
            insert = i;
            break;
        }
        previous_cc = packet[3] & 0x0F;
    }
    assert(insert < mux.data.size() && previous_cc >= 0);

    std::string adaptation(188, (char)0xFF);
    uint8_t* packet = (uint8_t*)&adaptation[0];
    packet[0] = 0x47;
    packet[1] = (uint8_t)((stream >> 8) & 0x1F);
    packet[2] = (uint8_t)stream;
    packet[3] = (uint8_t)(0x20 | previous_cc);
    packet[4] = 183;
    packet[5] = 0;
    std::string with_adaptation = mux.data;
    with_adaptation.insert(insert, adaptation);

    struct mpeg_ps_packet_test_t baseline;
    baseline.handler_result = 0;
    mpeg_ts_demux_packets(mux.data, &baseline);
    struct mpeg_ps_packet_test_t actual;
    actual.handler_result = 0;
    mpeg_ts_demux_packets(with_adaptation, &actual);
    assert(baseline.outputs.size() == actual.outputs.size());
    for (size_t i = 0; i < baseline.outputs.size(); ++i)
    {
        assert(baseline.outputs[i].data == actual.outputs[i].data);
        assert(baseline.outputs[i].flags == actual.outputs[i].flags);
        assert(baseline.outputs[i].pts == actual.outputs[i].pts);
        assert(baseline.outputs[i].dts == actual.outputs[i].dts);
    }
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

static void mpeg_ts_packet_error_retransmission_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    for (int invalid_adaptation = 0; invalid_adaptation < 2;
        ++invalid_adaptation)
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

        size_t pusi = mpeg_ts_find_pusi_packet(ctx.data, pes_begin, pes_end,
            stream);
        std::string error = ctx.data.substr(pusi, 188);
        uint8_t* packet = (uint8_t*)&error[0];
        if (invalid_adaptation)
        {
            packet[3] = (uint8_t)((packet[3] & 0xCF) | 0x30);
            packet[4] = 0xFF;
        }
        else
        {
            packet[1] |= 0x80;
        }

        std::string input = ctx.data.substr(0, pusi);
        input += error;
        input.append(ctx.data, pusi, pes_end - pusi);
        struct ts_demuxer_t* demuxer = ts_demuxer_create(
            mpeg_ts_packet_onpacket, &ctx);
        assert(NULL != demuxer);
        for (size_t i = 0; i < input.size(); i += 188)
        {
            assert(0 == ts_demuxer_input(demuxer,
                (const uint8_t*)input.data() + i, 188));
        }
        assert(0 == ts_demuxer_flush(demuxer));
        assert(1 == ctx.outputs.size());
        assert(picture == ctx.outputs[0].data);
        assert(9000 == ctx.outputs[0].pts);
        assert(8100 == ctx.outputs[0].dts);
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
        assert(0 == (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));
        assert(0 == ts_demuxer_destroy(demuxer));
    }
}

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
    size_t boundary_end = ctx.data.size();
    size_t next_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 16200, next.data(), next.size()));
    assert(0 == mpeg_ts_destroy(muxer));

    size_t boundary_pusi = mpeg_ts_find_pusi_packet(ctx.data,
        boundary_begin, boundary_end, stream);
    size_t discontinuity_insert = boundary_pusi + 188;
    assert(discontinuity_insert < boundary_end);
    assert(next_begin >= boundary_end);
    int last_cc = -1;
    for (size_t i = 0; i < discontinuity_insert; i += 188)
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

    struct ts_demuxer_t* demuxer = ts_demuxer_create(mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < discontinuity_insert; i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(0 == ts_demuxer_input(demuxer,
        (const uint8_t*)discontinuity.data(), discontinuity.size()));
    for (size_t i = discontinuity_insert; i < ctx.data.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(1 == ctx.outputs.size());
    assert(first == ctx.outputs[0].data);
    assert(0 == ts_demuxer_flush(demuxer));
    assert(2 == ctx.outputs.size());
    assert(next == ctx.outputs[1].data);
    assert(0 != (ctx.outputs[1].flags & MPEG_FLAG_PACKET_LOST));
    assert(0 == (ctx.outputs[1].flags & MPEG_FLAG_PACKET_CORRUPT));
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

static void mpeg_ts_packet_header_only_flush_preserves_previous_test()
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
        9000, 8100, first.data(), first.size()));
    size_t first_end = ctx.data.size();
    size_t next_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        12600, 11700, next.data(), next.size()));
    size_t next_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t next_pusi = mpeg_ts_find_pusi_packet(ctx.data, next_begin,
        next_end, stream);
    std::string pes = mpeg_ts_collect_pid_payload(ctx.data, next_pusi,
        next_end, stream);
    size_t header = 9 + (uint8_t)pes[8];
    assert(header < pes.size() && header <= 184);
    int cc = ((const uint8_t*)ctx.data.data())[next_pusi + 3] & 0x0F;
    std::string header_packet = mpeg_ts_make_packet(stream, cc, 1,
        pes.data(), header);

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < first_end; i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(ctx.outputs.empty());
    assert(0 == ts_demuxer_input(demuxer,
        (const uint8_t*)header_packet.data(), header_packet.size()));
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(first == ctx.outputs[0].data);
    assert(9000 == ctx.outputs[0].pts);
    assert(8100 == ctx.outputs[0].dts);
    assert(0 == (ctx.outputs[0].flags
        & (MPEG_FLAG_PACKET_CORRUPT | MPEG_FLAG_PACKET_LOST)));
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_incomplete_finite_pes_flush_test()
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
    std::string incomplete(picture_i, sizeof(picture_i));
    incomplete.append(1200, (char)0x11);
    std::string next(picture_p, sizeof(picture_p));
    next.append(512, (char)0x22);
    size_t incomplete_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 8100, incomplete.data(), incomplete.size()));
    size_t incomplete_end = ctx.data.size();
    size_t next_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 15300, next.data(), next.size()));
    size_t next_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t incomplete_pusi = mpeg_ts_find_pusi_packet(ctx.data,
        incomplete_begin, incomplete_end, stream);
    size_t next_pusi = mpeg_ts_find_pusi_packet(ctx.data, next_begin,
        next_end, stream);
    const uint8_t* first_packet =
        (const uint8_t*)ctx.data.data() + incomplete_pusi;
    int cc = first_packet[3] & 0x0F;
    std::string next_packets = ctx.data.substr(next_pusi,
        next_end - next_pusi);
    for (size_t i = 0; i < next_packets.size(); i += 188)
    {
        uint8_t* packet = (uint8_t*)&next_packets[i];
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
        {
            cc = (cc + 1) & 0x0F;
            packet[3] = (uint8_t)((packet[3] & 0xF0) | cc);
        }
    }

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < incomplete_pusi + 188; i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(ctx.outputs.empty());

    for (size_t i = 0; i < next_packets.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)next_packets.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(next == ctx.outputs[0].data);
    assert(16200 == ctx.outputs[0].pts);
    assert(15300 == ctx.outputs[0].dts);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
    assert(0 == (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));
    assert(0 == ts_demuxer_destroy(demuxer));
}

static void mpeg_ts_packet_incomplete_audio_flush_test()
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
    std::string incomplete(1200, (char)0x11);
    std::string next(96, (char)0x22);
    size_t incomplete_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        9000, 9000, incomplete.data(), incomplete.size()));
    size_t incomplete_end = ctx.data.size();
    size_t next_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 16200, next.data(), next.size()));
    size_t next_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t incomplete_pusi = mpeg_ts_find_pusi_packet(ctx.data,
        incomplete_begin, incomplete_end, stream);
    size_t next_pusi = mpeg_ts_find_pusi_packet(ctx.data, next_begin,
        next_end, stream);
    const uint8_t* first_packet =
        (const uint8_t*)ctx.data.data() + incomplete_pusi;
    int cc = first_packet[3] & 0x0F;
    std::string next_packets = ctx.data.substr(next_pusi,
        next_end - next_pusi);
    for (size_t i = 0; i < next_packets.size(); i += 188)
    {
        uint8_t* packet = (uint8_t*)&next_packets[i];
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (packet[3] & 0x10))
        {
            cc = (cc + 1) & 0x0F;
            packet[3] = (uint8_t)((packet[3] & 0xF0) | cc);
        }
    }

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket_any, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < incomplete_pusi + 188; i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)ctx.data.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(ctx.outputs.empty());
    for (size_t i = 0; i < next_packets.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)next_packets.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(PSI_STREAM_AAC == ctx.outputs[0].codecid);
    assert(next == ctx.outputs[0].data);
    assert(16200 == ctx.outputs[0].pts);
    assert(16200 == ctx.outputs[0].dts);
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

static void mpeg_ts_packet_incomplete_finite_pes_pusi_test()
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
    std::string incomplete(picture_i, sizeof(picture_i));
    incomplete.append(1200, (char)0x11);
    std::string next(picture_p, sizeof(picture_p));
    next.append(512, (char)0x22);
    size_t incomplete_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
        9000, 8100, incomplete.data(), incomplete.size()));
    size_t incomplete_end = ctx.data.size();
    size_t next_begin = ctx.data.size();
    assert(0 == mpeg_ts_write(muxer, stream, 0,
        16200, 15300, next.data(), next.size()));
    size_t next_end = ctx.data.size();
    assert(0 == mpeg_ts_destroy(muxer));

    size_t incomplete_pusi = mpeg_ts_find_pusi_packet(ctx.data,
        incomplete_begin, incomplete_end, stream);
    size_t next_pusi = mpeg_ts_find_pusi_packet(ctx.data,
        next_begin, next_end, stream);
    assert(incomplete_pusi + 188 < incomplete_end);
    const uint8_t* packet =
        (const uint8_t*)ctx.data.data() + incomplete_pusi;
    int cc = packet[3] & 0x0F;
    std::string input = ctx.data.substr(0, incomplete_pusi + 188);
    std::string next_packets = ctx.data.substr(next_pusi, next_end - next_pusi);
    for (size_t i = 0; i < next_packets.size(); i += 188)
    {
        uint8_t* current = (uint8_t*)&next_packets[i];
        int packet_pid = ((current[1] << 8) | current[2]) & 0x1FFF;
        if (packet_pid == stream && 0 != (current[3] & 0x10))
        {
            cc = (cc + 1) & 0x0F;
            current[3] = (uint8_t)((current[3] & 0xF0) | cc);
        }
    }
    input += next_packets;

    struct ts_demuxer_t* demuxer = ts_demuxer_create(
        mpeg_ts_packet_onpacket, &ctx);
    assert(NULL != demuxer);
    for (size_t i = 0; i < input.size(); i += 188)
    {
        assert(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)input.data() + i, 188));
    }
    assert(0 == ts_demuxer_flush(demuxer));
    assert(1 == ctx.outputs.size());
    assert(next == ctx.outputs[0].data);
    assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
    assert(0 == (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));
    assert(16200 == ctx.outputs[0].pts);
    assert(15300 == ctx.outputs[0].dts);
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

static void mpeg_ts_packet_invalid_afc_test()
{
    static const char picture_i[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x08, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    static const char picture_p[] = {
        0x00, 0x00, 0x01, 0x00, 0x00, 0x10, (char)0xFF, (char)0xFF,
        0x00, 0x00, 0x01, 0x01
    };
    for (int mode = 0; mode < 3; ++mode)
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
        std::string first(picture_i, sizeof(picture_i));
        first.append(512, (char)0x11);
        std::string next(picture_p, sizeof(picture_p));
        next.append(512, (char)0x22);
        assert(0 == mpeg_ts_write(muxer, stream, MPEG_FLAG_IDR_FRAME,
            9000, 8100, first.data(), first.size()));
        size_t next_begin = ctx.data.size();
        assert(0 == mpeg_ts_write(muxer, stream, 0,
            12600, 11700, next.data(), next.size()));
        size_t next_end = ctx.data.size();
        assert(0 == mpeg_ts_destroy(muxer));

        size_t next_pusi = mpeg_ts_find_pusi_packet(ctx.data, next_begin,
            next_end, stream);
        const uint8_t* clean =
            (const uint8_t*)ctx.data.data() + next_pusi;
        std::string invalid(188, (char)0xFF);
        uint8_t* packet = (uint8_t*)&invalid[0];
        packet[0] = 0x47;
        packet[1] = (uint8_t)((stream >> 8) & 0x1F);
        packet[2] = (uint8_t)stream;
        if (0 == mode)
        {
            packet[3] = (uint8_t)(clean[3] & 0x0F);
        }
        else if (1 == mode)
        {
            packet[3] = (uint8_t)(0x20 | (clean[3] & 0x0F));
            packet[4] = 0;
        }
        else
        {
            packet[3] = (uint8_t)(0x30 | (clean[3] & 0x0F));
            packet[4] = 183;
            packet[5] = 0;
        }
        ctx.data.insert(next_pusi, invalid);

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
        assert(next == ctx.outputs[0].data);
        assert(12600 == ctx.outputs[0].pts);
        assert(11700 == ctx.outputs[0].dts);
        assert(0 != (ctx.outputs[0].flags & MPEG_FLAG_PACKET_LOST));
        assert(0 == (ctx.outputs[0].flags & MPEG_FLAG_PACKET_CORRUPT));
        assert(0 == ts_demuxer_destroy(demuxer));
    }
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

static size_t mpeg_ts_h26x_payload_offset(const uint8_t packet[188])
{
    size_t offset = 4;
    int adaptation_field_control = (packet[3] >> 4) & 0x03;
    mpeg_packet_test_require(0 != (adaptation_field_control & 0x01));
    if (0 != (adaptation_field_control & 0x02))
        offset += 1 + packet[4];
    mpeg_packet_test_require(offset < 188);
    return offset;
}

static std::string mpeg_ts_h26x_collect_pid_payload(const std::string& data,
    size_t begin, size_t end, int pid)
{
    std::string payload;
    mpeg_packet_test_require(begin <= end && end <= data.size());
    for (size_t i = begin; i + 188 <= end; i += 188)
    {
        const uint8_t* packet = (const uint8_t*)data.data() + i;
        int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
        if (packet_pid != pid || 0 == (packet[3] & 0x10))
            continue;
        size_t offset = mpeg_ts_h26x_payload_offset(packet);
        payload.append((const char*)packet + offset, 188 - offset);
    }
    return payload;
}

static void mpeg_ts_packet_incomplete_finite_h26x_flush_test()
{
    static const int codecs[] = {
        PSI_STREAM_H264, PSI_STREAM_H265, PSI_STREAM_H266,
    };

    for (size_t n = 0; n < sizeof(codecs) / sizeof(codecs[0]); ++n)
    {
        const int codecid = codecs[n];
        struct mpeg_ts_func_t func;
        struct mpeg_ps_packet_test_t ctx;
        memset(&func, 0, sizeof(func));
        func.alloc = mpeg_ps_packet_alloc;
        func.free = mpeg_ps_packet_free;
        func.write = mpeg_ts_packet_write;
        ctx.handler_result = 0;

        void* muxer = mpeg_ts_create(&func, &ctx);
        mpeg_packet_test_require(NULL != muxer);
        int stream = mpeg_ts_add_stream(muxer, codecid, NULL, 0);
        mpeg_packet_test_require(stream > 0);
        std::string incomplete = mpeg_packet_test_h26x_access_unit(codecid,
            1, 512);
        std::string next = mpeg_packet_test_h26x_access_unit(codecid, 0, 64);
        std::string expected_next = mpeg_packet_test_h26x_callback_data(codecid,
            next);
        mpeg_packet_test_require(incomplete.size() > 184);
        size_t incomplete_begin = ctx.data.size();
        mpeg_packet_test_require(0 == mpeg_ts_write(muxer, stream,
            MPEG_FLAG_IDR_FRAME, 9000, 8100, incomplete.data(),
            incomplete.size()));
        size_t incomplete_end = ctx.data.size();
        size_t next_begin = ctx.data.size();
        mpeg_packet_test_require(0 == mpeg_ts_write(muxer, stream, 0,
            16200, 15300, next.data(), next.size()));
        size_t next_end = ctx.data.size();
        mpeg_packet_test_require(0 == mpeg_ts_destroy(muxer));

        size_t incomplete_pusi = mpeg_ts_find_pusi_packet(ctx.data,
            incomplete_begin, incomplete_end, stream);
        size_t next_pusi = mpeg_ts_find_pusi_packet(ctx.data, next_begin,
            next_end, stream);
        mpeg_packet_test_require(incomplete_pusi < incomplete_end);
        mpeg_packet_test_require(next_pusi < next_end);
        const uint8_t* first_packet =
            (const uint8_t*)ctx.data.data() + incomplete_pusi;
        int cc = first_packet[3] & 0x0F;
        std::string next_packets = ctx.data.substr(next_pusi,
            next_end - next_pusi);
        mpeg_packet_test_require(0 == next_packets.size() % 188);
        for (size_t i = 0; i < next_packets.size(); i += 188)
        {
            uint8_t* packet = (uint8_t*)&next_packets[i];
            int packet_pid = ((packet[1] << 8) | packet[2]) & 0x1FFF;
            if (packet_pid == stream && 0 != (packet[3] & 0x10))
            {
                cc = (cc + 1) & 0x0F;
                packet[3] = (uint8_t)((packet[3] & 0xF0) | cc);
            }
        }

        struct ts_demuxer_t* demuxer = ts_demuxer_create(
            mpeg_ts_packet_onpacket_any, &ctx);
        mpeg_packet_test_require(NULL != demuxer);
        for (size_t i = 0; i < incomplete_pusi + 188; i += 188)
        {
            mpeg_packet_test_require(0 == ts_demuxer_input(demuxer,
                (const uint8_t*)ctx.data.data() + i, 188));
        }
        mpeg_packet_test_require(0 == ts_demuxer_flush(demuxer));
        mpeg_packet_test_require(ctx.outputs.empty());
        mpeg_packet_test_require(0 == ts_demuxer_flush(demuxer));
        mpeg_packet_test_require(ctx.outputs.empty());

        for (size_t i = 0; i < next_packets.size(); i += 188)
        {
            mpeg_packet_test_require(0 == ts_demuxer_input(demuxer,
                (const uint8_t*)next_packets.data() + i, 188));
        }
        mpeg_packet_test_require(0 == ts_demuxer_flush(demuxer));
        mpeg_packet_test_require(1 == ctx.outputs.size());
        mpeg_packet_test_require(codecid == ctx.outputs[0].codecid);
        mpeg_packet_test_require(expected_next == ctx.outputs[0].data);
        mpeg_packet_test_require(16200 == ctx.outputs[0].pts);
        mpeg_packet_test_require(15300 == ctx.outputs[0].dts);
        mpeg_packet_test_require(0 != (ctx.outputs[0].flags
            & MPEG_FLAG_PACKET_LOST));
        mpeg_packet_test_require(0 == (ctx.outputs[0].flags
            & MPEG_FLAG_PACKET_CORRUPT));
        mpeg_packet_test_require(0 == ts_demuxer_flush(demuxer));
        mpeg_packet_test_require(1 == ctx.outputs.size());
        mpeg_packet_test_require(0 == ts_demuxer_destroy(demuxer));
    }
}

static void mpeg_ts_packet_h26x_header_only_flush_preserves_previous_test()
{
    static const int codecs[] = {
        PSI_STREAM_H264, PSI_STREAM_H265, PSI_STREAM_H266,
    };

    for (size_t n = 0; n < sizeof(codecs) / sizeof(codecs[0]); ++n)
    {
        const int codecid = codecs[n];
        struct mpeg_ts_func_t func;
        struct mpeg_ps_packet_test_t ctx;
        memset(&func, 0, sizeof(func));
        func.alloc = mpeg_ps_packet_alloc;
        func.free = mpeg_ps_packet_free;
        func.write = mpeg_ts_packet_write;
        ctx.handler_result = 0;

        void* muxer = mpeg_ts_create(&func, &ctx);
        mpeg_packet_test_require(NULL != muxer);
        int stream = mpeg_ts_add_stream(muxer, codecid, NULL, 0);
        mpeg_packet_test_require(stream > 0);
        std::string first = mpeg_packet_test_h26x_access_unit(codecid, 1,
            64);
        std::string next = mpeg_packet_test_h26x_access_unit(codecid, 0,
            64);
        std::string expected_first = mpeg_packet_test_h26x_callback_data(codecid,
            first);
        mpeg_packet_test_require(0 == mpeg_ts_write(muxer, stream,
            MPEG_FLAG_IDR_FRAME, 9000, 8100, first.data(), first.size()));
        size_t first_end = ctx.data.size();
        size_t next_begin = ctx.data.size();
        mpeg_packet_test_require(0 == mpeg_ts_write(muxer, stream, 0,
            12600, 11700, next.data(), next.size()));
        size_t next_end = ctx.data.size();
        mpeg_packet_test_require(0 == mpeg_ts_destroy(muxer));

        size_t next_pusi = mpeg_ts_find_pusi_packet(ctx.data, next_begin,
            next_end, stream);
        mpeg_packet_test_require(next_pusi < next_end);
        std::string pes = mpeg_ts_h26x_collect_pid_payload(ctx.data,
            next_pusi, next_end, stream);
        mpeg_packet_test_require(pes.size() > 8);
        size_t header = 9 + (uint8_t)pes[8];
        mpeg_packet_test_require(header < pes.size() && header <= 184);
        int cc = ((const uint8_t*)ctx.data.data())[next_pusi + 3] & 0x0F;
        std::string header_packet = mpeg_ts_make_packet(stream, cc, 1,
            pes.data(), header);

        struct ts_demuxer_t* demuxer = ts_demuxer_create(
            mpeg_ts_packet_onpacket_any, &ctx);
        mpeg_packet_test_require(NULL != demuxer);
        for (size_t i = 0; i < first_end; i += 188)
        {
            mpeg_packet_test_require(0 == ts_demuxer_input(demuxer,
                (const uint8_t*)ctx.data.data() + i, 188));
        }
        mpeg_packet_test_require(ctx.outputs.empty());
        mpeg_packet_test_require(0 == ts_demuxer_input(demuxer,
            (const uint8_t*)header_packet.data(), header_packet.size()));
        mpeg_packet_test_require(0 == ts_demuxer_flush(demuxer));
        mpeg_packet_test_require(1 == ctx.outputs.size());
        mpeg_packet_test_require(codecid == ctx.outputs[0].codecid);
        mpeg_packet_test_require(expected_first == ctx.outputs[0].data);
        mpeg_packet_test_require(9000 == ctx.outputs[0].pts);
        mpeg_packet_test_require(8100 == ctx.outputs[0].dts);
        mpeg_packet_test_require(MPEG_FLAG_IDR_FRAME == ctx.outputs[0].flags);
        mpeg_packet_test_require(0 == ts_demuxer_flush(demuxer));
        mpeg_packet_test_require(1 == ctx.outputs.size());
        mpeg_packet_test_require(0 == ts_demuxer_destroy(demuxer));
    }
}

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

enum mpeg1_pes_timestamp_mode_t
{
    MPEG1_PES_NO_TIMESTAMP,
    MPEG1_PES_PTS,
    MPEG1_PES_PTS_DTS,
};

static void mpeg1_pes_timestamp_append(std::string* data, int prefix,
    int64_t timestamp)
{
    data->push_back((char)((prefix << 4)
        | (((timestamp >> 30) & 0x07) << 1) | 0x01));
    data->push_back((char)(timestamp >> 22));
    data->push_back((char)(((timestamp >> 14) & 0xFE) | 0x01));
    data->push_back((char)(timestamp >> 7));
    data->push_back((char)(((timestamp << 1) & 0xFE) | 0x01));
}

static std::string mpeg1_pes_header_make(int stuffing, int have_std,
    enum mpeg1_pes_timestamp_mode_t mode, int64_t pts, int64_t dts,
    uint16_t payload_bytes)
{
    std::string data(2, '\0');
    data.append((size_t)stuffing, (char)0xFF);
    if (have_std)
    {
        data.push_back((char)0x40);
        data.push_back((char)0x00);
    }

    if (MPEG1_PES_NO_TIMESTAMP == mode)
        data.push_back((char)0x0F);
    else if (MPEG1_PES_PTS == mode)
        mpeg1_pes_timestamp_append(&data, 2, pts);
    else
    {
        mpeg1_pes_timestamp_append(&data, 3, pts);
        mpeg1_pes_timestamp_append(&data, 1, dts);
    }

    size_t optional_bytes = data.size() - 2;
    assert(optional_bytes + payload_bytes <= UINT16_MAX);
    uint16_t packet_bytes = (uint16_t)(optional_bytes + payload_bytes);
    data[0] = (char)(packet_bytes >> 8);
    data[1] = (char)packet_bytes;
    return data;
}

static void mpeg1_pes_header_fragment_case(const std::string& header,
    int64_t expected_pts, int64_t expected_dts, uint16_t payload_bytes)
{
    for (size_t bytes = 0; bytes < header.size(); ++bytes)
    {
        struct pes_t pes;
        struct mpeg_bits_t reader;
        memset(&pes, 0, sizeof(pes));
        pes.pts = pes.dts = PTS_NO_VALUE;
        mpeg_bits_init(&reader, header.data(), bytes);
        assert(MPEG_ERROR_NEED_MORE_DATA
            == pes_read_mpeg1_header(&pes, &reader));
    }

    struct pes_t pes;
    struct mpeg_bits_t reader;
    memset(&pes, 0, sizeof(pes));
    pes.pts = pes.dts = PTS_NO_VALUE;
    mpeg_bits_init(&reader, header.data(), header.size());
    assert(MPEG_ERROR_OK == pes_read_mpeg1_header(&pes, &reader));
    assert(payload_bytes == pes.len);
    assert(expected_pts == pes.pts);
    assert(expected_dts == pes.dts);
}

static void mpeg1_pes_header_fragment_test()
{
    const int64_t pts = INT64_C(0x123456789);
    const int64_t dts = INT64_C(0x102345678);
    const uint16_t payload_bytes = 17;

    mpeg1_pes_header_fragment_case(mpeg1_pes_header_make(0, 0,
        MPEG1_PES_NO_TIMESTAMP, pts, dts, payload_bytes),
        PTS_NO_VALUE, PTS_NO_VALUE, payload_bytes);
    mpeg1_pes_header_fragment_case(mpeg1_pes_header_make(2, 0,
        MPEG1_PES_NO_TIMESTAMP, pts, dts, payload_bytes),
        PTS_NO_VALUE, PTS_NO_VALUE, payload_bytes);
    mpeg1_pes_header_fragment_case(mpeg1_pes_header_make(0, 1,
        MPEG1_PES_NO_TIMESTAMP, pts, dts, payload_bytes),
        PTS_NO_VALUE, PTS_NO_VALUE, payload_bytes);
    mpeg1_pes_header_fragment_case(mpeg1_pes_header_make(1, 0,
        MPEG1_PES_PTS, pts, dts, payload_bytes),
        pts, PTS_NO_VALUE, payload_bytes);
    mpeg1_pes_header_fragment_case(mpeg1_pes_header_make(0, 1,
        MPEG1_PES_PTS_DTS, pts, dts, payload_bytes),
        pts, dts, payload_bytes);
}

static void mpeg1_pes_header_invalid_marker_test()
{
    struct pes_t pes;
    struct mpeg_bits_t reader;
    std::string header = mpeg1_pes_header_make(0, 0, MPEG1_PES_NO_TIMESTAMP,
        0, 0, 17);
    header[header.size() - 1] = 0;

    memset(&pes, 0, sizeof(pes));
    pes.pts = pes.dts = PTS_NO_VALUE;
    mpeg_bits_init(&reader, header.data(), header.size());
    assert(MPEG_ERROR_INVALID_DATA == pes_read_mpeg1_header(&pes, &reader));
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
    mpeg_packet_reserved_mpeg12_test(PSI_STREAM_MPEG1, 0);
    mpeg_packet_reserved_mpeg12_test(PSI_STREAM_MPEG1, 1);
    mpeg_packet_reserved_mpeg12_test(PSI_STREAM_MPEG2, 0);
    mpeg_packet_reserved_mpeg12_test(PSI_STREAM_MPEG2, 1);
    mpeg_packet_reserved_mpeg12_negative_test();
    mpeg_packet_reserved_mpeg12_fragmented_test(PSI_STREAM_MPEG1);
    mpeg_packet_reserved_mpeg12_fragmented_test(PSI_STREAM_MPEG2);
    mpeg_packet_reserved_mpeg12_cross_pes_probe_test();
    mpeg_packet_reserved_mpeg12_independent_probe_test();
    mpeg_packet_reserved_mpeg12_need_more_precedes_h26x_test();
    mpeg_packet_reserved_mpeg12_need_more_bounded_test();
    mpeg_packet_reserved_mpeg12_metadata_test();
    mpeg_packet_reserved_mpeg12_handler_error_test();
    mpeg_packet_reserved_h264_and_mpeg4_test();
    mpeg_packet_reserved_h264_append_failure_atomic_test();
    mpeg_packet_codecid_transition_test();
    mpeg_packet_size_limit_test();
    mpeg_packet_growth_test();
    mpeg_ts_packet_initial_cc_test();
    mpeg_ts_packet_duplicate_payload_test();
    mpeg_ts_packet_adaptation_only_cc_test();
    mpeg_ts_packet_error_retransmission_test();
    mpeg_pmt_clear_stream_state_test();
    mpeg_ps_packet_continuous_test();
    mpeg_ps_packet_no_psm_variable_chunks_test();
    mpeg_ts_packet_incomplete_finite_h26x_flush_test();
    mpeg_ts_packet_h26x_header_only_flush_preserves_previous_test();
#if defined(MPEG_PACKET_INVALID_PES_TEST)
    mpeg_packet_invalid_pes_test();
    mpeg_ts_packet_split_pes_header_test();
    mpeg_ts_packet_header_only_start_test();
    mpeg_ts_packet_header_only_flush_preserves_previous_test();
    mpeg_ts_packet_incomplete_finite_pes_flush_test();
    mpeg_ts_packet_incomplete_audio_flush_test();
    mpeg_ts_packet_unbounded_pes_test();
    mpeg_ts_packet_incomplete_finite_pes_pusi_test();
    mpeg_ts_packet_interleaved_streams_test();
    mpeg_ts_packet_handler_error_state_test();
    mpeg_ts_packet_invalid_pes_continuation_test();
    mpeg_ts_packet_invalid_pes_flush_test();
    mpeg_ts_packet_pes_payload_cc_gap_test();
    mpeg_ts_packet_audio_pes_payload_cc_gap_test();
    mpeg_ts_packet_invalid_afc_test();
    mpeg_ts_packet_invalid_adaptation_field_test();
#endif
    mpeg_ts_packet_pes_payload_boundary_test();
    mpeg_ts_packet_adaptation_only_discontinuity_test();
    mpeg_ts_packet_discontinuity_indicator_test();
    mpeg_ts_packet_tei_test();
    mpeg_ts_packet_flush_test();
    mpeg1_pes_header_fragment_test();
    mpeg1_pes_header_invalid_marker_test();
}

#if defined(MPEG_PACKET_H26X_FLUSH_TEST_ONLY)
int main()
{
    mpeg_ts_packet_incomplete_finite_h26x_flush_test();
    mpeg_ts_packet_h26x_header_only_flush_preserves_previous_test();
    return 0;
}
#elif defined(MPEG_PACKET_TEST_MAIN)
int main()
{
#if defined(MPEG_PACKET_INVALID_PES_TEST) && defined(NDEBUG)
    mpeg_packet_invalid_pes_test();
    mpeg_ts_packet_incomplete_finite_h26x_flush_test();
    mpeg_ts_packet_h26x_header_only_flush_preserves_previous_test();
#else
    mpeg_packet_test();
#endif
    return 0;
}
#endif
