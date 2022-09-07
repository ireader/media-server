#include "sdp-a-webrtc.h"
#include "sdp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if defined(OS_WINDOWS)
#define strncasecmp _strnicmp
#endif

struct sdp_string_token_t
{
    const char* p;
    int n;
};

struct sdp_string_parser_t
{
    const char* s; // start
    const char* e; // end
};


static inline int sdp_get_token(struct sdp_string_parser_t* s, struct sdp_string_token_t* t, const char* escape)
{
    // left trim
    while (s->s && s->s < s->e && ' ' == *s->s)
        ++s->s;

    t->p = s->s;
    for (t->n = 0; s->s && s->s + t->n < s->e; ++t->n)
    {
        if (strchr(escape, s->s[t->n]))
            break;
    }

    s->s += t->n + 1; // skip escape
    return t->n;
}

static inline int sdp_string_count(const char* s, int n, char c)
{
    int i;
    for(i = 0; s && n > 0; n--)
    {
        if (*s++ == c)
            i++;
    }

    return i;
}

static inline void sdp_strcpy(char* p, const char* s, int n)
{
    memcpy(p, s, n);
    p[n] = '\0';
}

static inline char* sdp_strdup(const char* s, int n)
{
    char* p;
    p = malloc(n + 1);
    if (p) 
    {
        memcpy(p, s, n);
        p[n] = '\0';
    }
    return p;
}

static char** sdp_string_split(const char* s, int n, const char* c, int* num)
{
    int i, cn;
    char* pv, **pk;
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k;

    t.s = s;
    t.e = s + n;
    *num = 0;
    if (t.s >= t.e)
        return NULL;

    cn = sdp_string_count(t.s, n, * c) + 1;
    pk = (char**)malloc(t.e - t.s + 1 + sizeof(char*) * cn);
    if (pk)
    {
        pv = (char*)(pk + cn);
        sdp_strcpy(pv, t.s, n);

        t.e = pv + (t.e - t.s);
        t.s = pv;
        for (i = 0; i < cn && sdp_get_token(&t, &k, c); i++)
        {
            assert(k.p + k.n <= t.e);
            pk[i] = (char*)k.p;
            ((char*)k.p)[k.n] = '\0';
        }

        *num = i;
    }
    return pk;
}

/// @return 0-ok, other-error
static inline int sdp_str2int(const char* s, int n, int* v)
{
    char* e;
    *v = (int)strtoul(s, &e, 10);
    return e - s == n ? 0 : -1;
}

// "extmap:" 1*5DIGIT ["/" direction]
// direction = "sendonly" / "recvonly" / "sendrecv" / "inactive"
int sdp_a_extmap(const char* s, int n, int* ext, int* direction, char url[128])
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k, v, d;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, "/ ") || 0 != sdp_str2int(k.p, k.n, ext))
        return -1;

    // direction
    if (direction)
        *direction = SDP_A_SENDRECV;

    if (t.s > s && '/' == *(t.s-1))
    {
        if (!sdp_get_token(&t, &d, " ") || d.n != 8)
            return -1;

        if (direction)
        {
            if (0 == strncmp(d.p, "sendonly", d.n))
                *direction = SDP_A_SENDONLY;
            else if (0 == strncmp(d.p, "recvonly", d.n))
                *direction = SDP_A_RECVONLY;
            else if (0 == strncmp(d.p, "sendrecv", d.n))
                *direction = SDP_A_SENDRECV;
            else if (0 == strncmp(d.p, "inactive", d.n))
                *direction = SDP_A_INACTIVE;
        }
    }

    if(!sdp_get_token(&t, &v, " ") || v.n >= 128)
        return -1;

    sdp_strcpy(url, v.p, v.n);
    return 0;
}

int sdp_a_fmtp(const char* s, int n, int* fmt, char** param)
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k, v;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " ") || 0 != sdp_str2int(k.p, k.n, fmt) || !sdp_get_token(&t, &v, " "))
        return -1;

    *param = sdp_strdup(v.p, v.n);
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8122.html#section-5
* attribute              =/ fingerprint-attribute
  fingerprint-attribute  =  "fingerprint" ":" hash-func SP fingerprint
  hash-func              =  "sha-1" / "sha-224" / "sha-256" /
                             "sha-384" / "sha-512" /
                             "md5" / "md2" / token
                             ; Additional hash functions can only come
                             ; from updates to RFC 3279
  fingerprint            =  2UHEX *(":" 2UHEX)
                             ; Each byte in upper-case hex, separated
                             ; by colons.
  UHEX                   =  DIGIT / %x41-46 ; A-F uppercase
*/
int sdp_a_fingerprint(const char* s, int n, char hash[16], char fingerprint[128])
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k, v;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " ") || !sdp_get_token(&t, &v, " "))
        return -1;

    sdp_strcpy(hash, k.p, k.n);
    sdp_strcpy(fingerprint, v.p, v.n);
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc5888.html#section-5
* group-attribute     = "a=group:" semantics *(SP identification-tag)
* semantics           = "LS" / "FID" / semantics-extension
* semantics-extension = token ; token is defined in RFC 4566
*
* a=group:LS 1 2
* a=group:BUNDLE audio video
*/
int sdp_a_group(const char* s, int n, char semantics[32], char*** groups, int* count)
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " "))
        return -1;

    sdp_strcpy(semantics, k.p, k.n);

    *count = 0;
    *groups = sdp_string_split(t.s, (int)(intptr_t)(t.e - t.s), " ", count);
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8839#name-candidate-attribute
* candidate-attribute   = "candidate" ":" foundation SP component-id SP
                        transport SP
                        priority SP
                        connection-address SP     ;from RFC 4566
                        port         ;port from RFC 4566
                        SP cand-type
                        [SP rel-addr]
                        [SP rel-port]
                        *(SP cand-extension)

    foundation            = 1*32ice-char
    component-id          = 1*3DIGIT
    transport             = "UDP" / transport-extension
    transport-extension   = token              ; from RFC 3261
    priority              = 1*10DIGIT
    cand-type             = "typ" SP candidate-types
    candidate-types       = "host" / "srflx" / "prflx" / "relay" / token
    rel-addr              = "raddr" SP connection-address
    rel-port              = "rport" SP port
    cand-extension        = extension-att-name SP extension-att-value
    extension-att-name    = token
    extension-att-value   = *VCHAR
    ice-char              = ALPHA / DIGIT / "+" / "/"

* a=candidate:2 1 UDP 1694498815 192.0.2.3 45664 typ srflx raddr 203.0.113.141 rport 8998
*/
int sdp_a_ice_candidate(const char* s, int n, struct sdp_ice_candidate_t* c)
{
    int i, val;
    int uid, uport, upriority;
    struct sdp_string_parser_t t;
    struct sdp_string_token_t foundation, component, transport, priority, address, port, candtype;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &foundation, " ") || foundation.n >= sizeof(c->foundation)
        || !sdp_get_token(&t, &component, " ") || 0 != sdp_str2int(component.p, component.n, &uid)
        || !sdp_get_token(&t, &transport, " ") || transport.n >= sizeof(c->transport)
        || !sdp_get_token(&t, &priority, " ") || 0 != sdp_str2int(priority.p, priority.n, &upriority)
        || !sdp_get_token(&t, &address, " ") || address.n >= sizeof(c->address)
        || !sdp_get_token(&t, &port, " ") || 0 != sdp_str2int(port.p, port.n, &uport)
        || !sdp_get_token(&t, &candtype, " ") || 3 != candtype.n || 0 != strncmp("typ", candtype.p, candtype.n)
        || !sdp_get_token(&t, &candtype, " ") || candtype.n >= sizeof(c->candtype))
        return -1;

    sdp_strcpy(c->foundation, foundation.p, foundation.n);
    sdp_strcpy(c->transport, transport.p, transport.n);
    sdp_strcpy(c->candtype, candtype.p, candtype.n);
    sdp_strcpy(c->address, address.p, address.n);
    c->component = (uint16_t)uid;
    c->priority = (uint32_t)upriority;
    c->port = (uint16_t)uport;
    c->relport = 0;
    c->generation = 0;
    c->nextension = 0;

    if (t.e > t.s)
        c->extensions = sdp_string_split(t.s, (int)(intptr_t)(t.e - t.s), " ", &c->nextension);

    for (i = 0; i + 1 < c->nextension; i += 2)
    {
        if (0 == strcmp("raddr", c->extensions[i]) && strlen(c->extensions[i + 1]) < sizeof(c->reladdr))
        {
            snprintf(c->reladdr, sizeof(c->reladdr), "%s", c->extensions[i + 1]);
        }
        if (0 == strcmp("rport", c->extensions[i]) && 0 == sdp_str2int(c->extensions[i + 1], (int)strlen(c->extensions[i + 1]), &val))
        {
            c->relport = (uint16_t)val;
        }
        else if (0 == strcmp("generation", c->extensions[i]) && 0 == sdp_str2int(c->extensions[i + 1], (int)strlen(c->extensions[i + 1]), &val))
        {
            c->generation = val;
        }
    }

    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8839#name-remote-candidates-attribute
* remote-candidate-att = "remote-candidates:" remote-candidate 0*(SP remote-candidate)
* remote-candidate = component-id SP connection-address SP port
*
* a=remote-candidates:1 192.0.2.3 45664
*/
int sdp_a_ice_remote_candidates(const char* s, int n, struct sdp_ice_candidate_t* c)
{
    // TODO: multiple remote candidates
    int uid, uport;
    struct sdp_string_parser_t t;
    struct sdp_string_token_t component, address, port;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &component, " ") || 0 != sdp_str2int(component.p, component.n, &uid)
        || !sdp_get_token(&t, &address, " ") || address.n >= sizeof(c->address)
        || !sdp_get_token(&t, &port, " ") || 0 != sdp_str2int(port.p, port.n, &uport))
        return -1;

    sdp_strcpy(c->address, address.p, address.n);
    c->component = (uint16_t)uid;
    c->port = (uint16_t)uport;
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8839#name-ice-pacing-attribute
* ice-pacing-att            = "ice-pacing:" pacing-value
* pacing-value              = 1*10DIGIT
*
* a=ice-pacing:50
*/
int sdp_a_ice_pacing(const char* s, int n, int* pacing)
{
    if (0 != sdp_str2int(s, n, pacing))
        return -1;
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8839#name-ice-options-attribute
* ice-options           = "ice-options:" ice-option-tag *(SP ice-option-tag)
* ice-option-tag        = 1*ice-char
*
* a=ice-options:ice2 rtp+ecn
*/
int sdp_a_ice_options(const char* s, int n, char*** options, int* count)
{
    *count = 0;
    *options = sdp_string_split(s, n, " ", count);
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc5888.html#section-4
* mid-attribute      = "a=mid:" identification-tag
* identification-tag = token ; token is defined in RFC 4566
*/
int sdp_a_mid(const char* s, int n, char tag[256])
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " ") || k.n > 256)
        return -1;

    sdp_strcpy(tag, k.p, k.n);
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8830#name-attribute-registration-in-e
* msid-value = msid-id [ SP msid-appdata ]
* msid-id = 1*64token-char ; see RFC 4566
* msid-appdata = 1*64token-char  ; see RFC 4566
*
* a=msid:examplefoo examplebar
*/
int sdp_a_msid(const char* s, int n, char id[65], char appdata[65])
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k, v;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " ") || k.n > 64)
        return -1;

    sdp_strcpy(id, k.p, k.n);
    appdata[0] = '\0';

    if (sdp_get_token(&t, &v, " ") && v.n <= 64)
        sdp_strcpy(appdata, v.p, v.n);
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8866#name-orient-orientation
* orient-value = portrait / landscape / seascape
* a=orient:portrait
*/
int sdp_a_orient(const char* s, int n, char orient[16])
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " "))
        return -1;

    sdp_strcpy(orient, k.p, k.n);
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8851.html#name-sdp-arid-media-level-attrib
* a=rid:<rid-id> <direction> [pt=<fmt-list>;]<restriction>=<value>...
* a=rid:5 send pt=99,102;max-br=64000;max-width=1280;max-height=720;max-fps=30
*/
int sdp_a_rid(const char* s, int n, struct sdp_rid_t* rid)
{
    int i, len;
    struct sdp_string_parser_t t;
    struct sdp_string_token_t id, dir;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &id, " ") || id.n >= sizeof(rid->rid)
        || !sdp_get_token(&t, &dir, " ") || dir.n >= sizeof(rid->direction))
        return -1;

    sdp_strcpy(rid->rid, id.p, id.n);
    sdp_strcpy(rid->direction, dir.p, dir.n);
    rid->params = sdp_string_split(t.s, (int)(intptr_t)(t.e - t.s), ";", &rid->nparam);

    for (i = 0; i < rid->nparam; i++)
    {
        len = (int)strlen(rid->params[i]);
        if (len > 10 && 0 == strncmp("max-width=", rid->params[i], 10))
            rid->width = atoi(rid->params[i] + 10);
        else if (len > 11 && 0 == strncmp("max-height=", rid->params[i], 11))
            rid->height = atoi(rid->params[i] + 11);
        else if (len > 8 && 0 == strncmp("max-fps=", rid->params[i], 8))
            rid->fps = atoi(rid->params[i] + 8);
        else if (len > 7 && 0 == strncmp("max-fs=", rid->params[i], 7))
            rid->fs = atoi(rid->params[i] + 7);
        else if (len > 7 && 0 == strncmp("max-br=", rid->params[i], 7))
            rid->br = atoi(rid->params[i] + 7);
        else if (len > 8 && 0 == strncmp("max-pps=", rid->params[i], 8))
            rid->pps = atoi(rid->params[i] + 8);
        else if (len > 8 && 0 == strncmp("max-bpp=", rid->params[i], 8))
            rid->bpp = atof(rid->params[i] + 8);
        else if (len > 7 && 0 == strncmp("depend=", rid->params[i], 7))
            rid->depends = sdp_string_split(rid->params[i] + 7, len, ",", &rid->ndepend);
        else if (len > 3 && 0 == strncmp("pt=", rid->params[i], 3))
            rid->payloads = sdp_string_split(rid->params[i] + 3, len, ",", &rid->npayload);
    }

    return rid->rid ? 0 : -1;
}

/*
* https://www.rfc-editor.org/rfc/rfc3605
* rtcp-attribute =  "a=rtcp:" port  [nettype space addrtype space connection-address] CRLF
* a=rtcp:53020
* a=rtcp:53020 IN IP6 2001:2345:6789:ABCD:EF01:2345:6789:ABCD
*/
int sdp_a_rtcp(const char* s, int n, struct sdp_address_t* address)
{
    int port;
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k, net, addr, conn;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " "))
        return -1;

    if (0 != sdp_str2int(k.p, k.n, &port))
        return -1;

    address->port[1] = address->port[0] = port; // copy
    if (sdp_get_token(&t, &net, " ") && sdp_get_token(&t, &addr, " ") && sdp_get_token(&t, &conn, " ")
        && net.n < sizeof(address->network) && addr.n < sizeof(address->addrtype) && conn.n < sizeof(address->address))
    {
        sdp_strcpy(address->network, net.p, net.n);
        sdp_strcpy(address->addrtype, addr.p, addr.n);
        sdp_strcpy(address->address, conn.p, conn.n);
    }

    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8859.html#name-rtcp-fb-attribute-analysis
* rtcp-fb-syntax = "a=rtcp-fb:" rtcp-fb-pt SP rtcp-fb-val CRLF
      rtcp-fb-pt         = "*"   ; wildcard: applies to all formats
                         / fmt   ; as defined in SDP spec
      rtcp-fb-val        = "ack" rtcp-fb-ack-param
                         / "nack" rtcp-fb-nack-param
                         / "trr-int" SP 1*DIGIT
                         / rtcp-fb-id rtcp-fb-param
      rtcp-fb-id         = 1*(alpha-numeric / "-" / "_")
      rtcp-fb-param      = SP "app" [SP byte-string]
                         / SP token [SP byte-string]
                         / ; empty
      rtcp-fb-ack-param  = SP "rpsi"
                         / SP "app" [SP byte-string]
                         / SP token [SP byte-string]
                         / ; empty
      rtcp-fb-nack-param = SP "pli"
                         / SP "sli"
                         / SP "rpsi"
                         / SP "app" [SP byte-string]
                         / SP token [SP byte-string]
                         / ; empty
*/
int sdp_a_rtcp_fb(const char* s, int n, struct sdp_rtcp_fb_t* fb)
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t fmt, k, v;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &fmt, " ") || !sdp_get_token(&t, &k, " ") || k.n >= sizeof(fb->feedback))
        return -1;

    if (fmt.n == 1 && '*' == *fmt.p)
        fb->fmt = -1;
    else if (0 != sdp_str2int(fmt.p, fmt.n, &fb->fmt))
        return -1;

    sdp_strcpy(fb->feedback, k.p, k.n);

    if (sdp_get_token(&t, &v, " ") && v.n < sizeof(fb->param))
    {
        sdp_strcpy(fb->param, v.p, v.n);
        if (3 == k.n && 0 == strncmp(k.p, "ack", 3))
        {

        }
        else if (4 == k.n && 0 == strncmp(k.p, "nack", 4))
        {

        }
        else if (7 == k.n && 0 == strncmp(k.p, "trr-int", 7))
        {
            if (0 != sdp_str2int(v.p, v.n, &fb->trr_int))
                return -1;
        }
        else
        {

        }
    }
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc8853.html#name-detailed-description
* a=simulcast:recv 1;2 send 4
sc-value     = ( sc-send [SP sc-recv] ) / ( sc-recv [SP sc-send] )
sc-send      = %s"send" SP sc-str-list
sc-recv      = %s"recv" SP sc-str-list
sc-str-list  = sc-alt-list *( ";" sc-alt-list )
sc-alt-list  = sc-id *( "," sc-id )
sc-id-paused = "~"
sc-id        = [sc-id-paused] rid-id
                ; SP defined in [RFC5234]
                ; rid-id defined in [RFC8851]
*/
int sdp_a_simulcast(const char* s, int n, struct sdp_simulcast_t* simulcast)
{
    char** pk;
    int i, num, direction;
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k, v;

    t.s = s;
    t.e = s + n;
    simulcast->nrecv = simulcast->nsend = 0;
    simulcast->recvs = simulcast->sends = NULL;

    for (i = 0; i < 2; i++)
    {
        if (!sdp_get_token(&t, &k, " ") || !sdp_get_token(&t, &v, " "))
            break;

        if (4 == k.n && 0 == memcmp(k.p, "send", 4))
            direction = 1;
        else if (4 == k.n && 0 == memcmp(k.p, "recv", 4))
            direction = 2;
        else
            continue;

        pk = sdp_string_split(v.p, v.n, ";", &num);
        if (1 == direction)
        {
            simulcast->sends = pk;
            simulcast->nsend = num;
        }
        else if (2 == direction)
        {
            simulcast->recvs = pk;
            simulcast->nrecv = num;
        }
        else
        {
            assert(0);
        }
    }

    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc5576.html#section-4.1
* a=ssrc:<ssrc-id> <attribute>
* a=ssrc:<ssrc-id> <attribute>:<value>
*/
int sdp_a_ssrc(const char* s, int n, uint32_t* ssrc, char attribute[64], char value[128])
{
    int u;
    struct sdp_string_parser_t t;
    struct sdp_string_token_t id, k, v;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &id, " ") || !sdp_get_token(&t, &k, ":") || k.n >= 64)
        return -1;

    if (0 != sdp_str2int(id.p, id.n, &u))
        return -1;

    *ssrc = (uint32_t)u;
    sdp_strcpy(attribute, k.p, k.n);
    if(sdp_get_token(&t, &v, "\r\n") && v.n < 128)
        sdp_strcpy(value, v.p, v.n);
    else
        *value = '\0';
    return 0;
}

/*
* https://www.rfc-editor.org/rfc/rfc5576.html#section-4.2
* a=ssrc-group:<semantics> <ssrc-id> ...
*
* a=ssrc-group:FID 3720618323 2524685008
* a=ssrc:3720618323 cname:VQFSeIb012G+W2KA
* a=ssrc:3720618323 msid:LCMSv0 LCMSv0
* a=ssrc:3720618323 mslabel:LCMSv0
* a=ssrc:3720618323 label:LCMSv0
* a=ssrc:2524685008 cname:VQFSeIb012G+W2KA
* a=ssrc:2524685008 msid:LCMSv0 LCMSv0
* a=ssrc:2524685008 mslabel:LCMSv0
* a=ssrc:2524685008 label:LCMSv0
*/
int sdp_a_ssrc_group(const char* s, int n, struct sdp_ssrc_group_t* group)
{
    struct sdp_string_parser_t t;
    struct sdp_string_token_t k;

    t.s = s;
    t.e = s + n;
    if (!sdp_get_token(&t, &k, " ") || k.n >= sizeof(group->key))
        return -1;

    sdp_strcpy(group->key, k.p, k.n);
    group->values = sdp_string_split(t.s, (int)(intptr_t)(t.e - t.s), " ", &group->count);
    return group->key ? 0 : -1;
}

#if defined(DEBUG) || defined(_DEBUG)
static void sdp_a_extmap_test(void)
{
    char url[128];
    int ext, direction;
    const char* s = "6/recvonly http://www.webrtc.org/experiments/rtp-hdrext/playout-delay";
    assert(0 == sdp_a_extmap(s, strlen(s), &ext, &direction, url));
    assert(6 == ext && SDP_A_RECVONLY == direction && 0 == strcmp("http://www.webrtc.org/experiments/rtp-hdrext/playout-delay", url));

    s = "7 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01";
    assert(0 == sdp_a_extmap(s, strlen(s), &ext, &direction, url));
    assert(7 == ext && SDP_A_SENDRECV == direction && 0 == strcmp("http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01", url));
}

static void sdp_a_simulcast_test(void)
{
    struct sdp_simulcast_t simulcast;
    memset(&simulcast, 0, sizeof(simulcast));
    sdp_a_simulcast("recv 1;2 send 4", 15, &simulcast);
    assert(2 == simulcast.nrecv && 1 == simulcast.nsend && 0 == strcmp(simulcast.recvs[0], "1") && 0 == strcmp(simulcast.recvs[1], "2") && 0 == strcmp(simulcast.sends[0], "4"));
    free(simulcast.recvs); free(simulcast.sends);

    memset(&simulcast, 0, sizeof(simulcast));
    sdp_a_simulcast("send 4 recv 1;2", 15, &simulcast);
    assert(2 == simulcast.nrecv && 1 == simulcast.nsend && 0 == strcmp(simulcast.recvs[0], "1") && 0 == strcmp(simulcast.recvs[1], "2") && 0 == strcmp(simulcast.sends[0], "4"));
    free(simulcast.recvs); free(simulcast.sends);
}

static void sdp_a_ssrc_test(void)
{
    uint32_t ssrc;
    char attr[64], value[128];
    assert(0 == sdp_a_ssrc("123456 key", 10, &ssrc, attr, value) && ssrc == 123456 && 0 == strcmp(attr, "key") && !*value);    
    assert(0 == sdp_a_ssrc("123456 key:value", 16, &ssrc, attr, value) && ssrc == 123456 && 0 == strcmp(attr, "key") && 0 == strcmp(value, "value"));
    assert(0 == sdp_a_ssrc("3720618323 msid:LCMSv0 LCMSv0", 29, &ssrc, attr, value) && ssrc == 3720618323 && 0 == strcmp(attr, "msid") && 0 == strcmp(value, "LCMSv0 LCMSv0"));
    assert(0 != sdp_a_ssrc("123456f key:value", 17, &ssrc, attr, value));
}

static void sdp_a_ssrc_group_test(void)
{
    struct sdp_ssrc_group_t group;
    memset(&group, 0, sizeof(group));
    assert(0 == sdp_a_ssrc_group("FID 3720618323 2524685008", 25, &group) && 0 == strcmp(group.key, "FID") && 2 == group.count && 0 == strcmp(group.values[0], "3720618323") && 0 == strcmp(group.values[1], "2524685008"));
    free(group.values);
}

static void sdp_a_rtcp_test(void)
{
    struct sdp_address_t addr;
    memset(&addr, 0, sizeof(addr));
    assert(0 == sdp_a_rtcp("53020", 5, &addr) && 53020 == addr.port[0]);
    assert(0 == sdp_a_rtcp("53020 IN IP6 2001:2345:6789:ABCD:EF01:2345:6789:ABCD", 52, &addr) && 53020 == addr.port[0] && 0 == strcmp(addr.network, "IN") && 0 == strcmp(addr.addrtype, "IP6") && 0 == strcmp(addr.address, "2001:2345:6789:ABCD:EF01:2345:6789:ABCD"));
}

static void sdp_a_rid_test(void)
{
    struct sdp_rid_t rid;
    memset(&rid, 0, sizeof(rid));
    assert(0 == sdp_a_rid("5 send", 6, &rid) && 0 == strcmp("5", rid.rid) && 0 == strcmp("send", rid.direction) && 0 == rid.nparam && 0 == rid.npayload && 0 == rid.ndepend);
    assert(0 == sdp_a_rid("5 send pt=99,102;max-br=64000;max-width=1280;max-height=720;max-fps=30", 70, &rid) && 0 == strcmp("5", rid.rid) && 0 == strcmp("send", rid.direction) && 2 == rid.npayload && 0 == strcmp("99", rid.payloads[0]) && 0 == strcmp("102", rid.payloads[1]) && 5 == rid.nparam && 64000 == rid.br && 1280 == rid.width && 720 == rid.height && 30 == rid.fps && 0 == rid.ndepend);
    free(rid.params);
}

static void sdp_a_fingerprint_test(void)
{
    char hash[16];
    char fingerprint[128];
    assert(0 == sdp_a_fingerprint("sha-256 47:5A:AC:E6:8B:47:2A:A8:BC:AE:08:0D:E2:11:31:A5:38:F7:AE:F6:89:07:40:6C:CB:CC:99:CF:5F:C5:B8:4F", 103, hash, fingerprint) && 0 == strcmp("sha-256", hash) && 0 == strcmp("47:5A:AC:E6:8B:47:2A:A8:BC:AE:08:0D:E2:11:31:A5:38:F7:AE:F6:89:07:40:6C:CB:CC:99:CF:5F:C5:B8:4F", fingerprint));
}

static void sdp_a_msid_test(void)
{
    char msid[65];
    char appdata[65];
    assert(0 == sdp_a_msid("5jbA0s68fQKkMHiM7ZDVxoVOox1mXyMvC7bR 22ceddc2-d518-4ed4-b9ad-708c87720561", 73, msid, appdata) && 0 == strcmp("5jbA0s68fQKkMHiM7ZDVxoVOox1mXyMvC7bR", msid) && 0 == strcmp("22ceddc2-d518-4ed4-b9ad-708c87720561", appdata));
}

static void sdp_a_mid_test(void)
{
    char tag[256];
    assert(0 == sdp_a_mid("audio", 5, tag) && 0 == strcmp("audio", tag));
}

static void sdp_a_group_test(void)
{
    int count;
    char** groups;
    char semantics[32];
    assert(0 == sdp_a_group("LS 1 2", 6, semantics, &groups, &count) && 0 == strcmp("LS", semantics) && 2 == count && 0 == strcmp("1", groups[0]) && 0 == strcmp("2", groups[1]));
    free(groups);

    assert(0 == sdp_a_group("BUNDLE audio video", 18, semantics, &groups, &count) && 0 == strcmp("BUNDLE", semantics) && 2 == count && 0 == strcmp("audio", groups[0]) && 0 == strcmp("video", groups[1]));
    free(groups);
}

static void sdp_a_ice_candidate_test(void)
{
    struct sdp_ice_candidate_t c;
    memset(&c, 0, sizeof(c));
    assert(0 == sdp_a_ice_candidate("2 1 UDP 1694498815 192.0.2.3 45664 typ srflx raddr 203.0.113.141 rport 8998", 75, &c)
        && 0 == strcmp(c.foundation, "2") && 1 == c.component && 0 == strcmp(c.transport, "UDP") && 1694498815 == c.priority
        && 0 == strcmp(c.address, "192.0.2.3") && 45664 == c.port && 0 == strcmp(c.candtype, "srflx") && 0 == strcmp(c.reladdr, "203.0.113.141") && 8998 == c.relport && 4 == c.nextension);

    memset(&c, 0, sizeof(c));
    assert(0 == sdp_a_ice_candidate("1 1 udp 2013266431 47.95.197.19 50858 typ host generation 0", 59, &c)
        && 0 == strcmp(c.foundation, "1") && 1 == c.component && 0 == strcmp(c.transport, "udp") && 2013266431 == c.priority
        && 0 == strcmp(c.address, "47.95.197.19") && 50858 == c.port && 0 == strcmp(c.candtype, "host") && 0 == strcmp(c.reladdr, "") && 0 == c.relport 
        && 2 == c.nextension && 0 == strcmp("generation", c.extensions[0]) && 0 == strcmp("0", c.extensions[1]) && 0 == c.generation);
    free(c.extensions);
}

static void sdp_a_ice_remote_candidates_test(void)
{
    struct sdp_ice_candidate_t c;
    memset(&c, 0, sizeof(c));
    assert(0 == sdp_a_ice_remote_candidates("1 192.0.2.3 45664", 17, &c) && 1 == c.component && 0 == strcmp("192.0.2.3", c.address) && 45664 == c.port);
}

static void sdp_a_rtcp_fb_test(void)
{
    struct sdp_rtcp_fb_t fb;
    memset(&fb, 0, sizeof(fb));
    assert(0 == sdp_a_rtcp_fb("111 nack", 8, &fb) && 111 == fb.fmt && 0 == strcmp("nack", fb.feedback) && !*fb.param);
    assert(0 == sdp_a_rtcp_fb("96 nack pli", 11, &fb) && 96 == fb.fmt && 0 == strcmp("nack", fb.feedback) && 0 == strcmp("pli", fb.param));
    assert(0 == sdp_a_rtcp_fb("96 ccm fir", 10, &fb) && 96 == fb.fmt && 0 == strcmp("ccm", fb.feedback) && 0 == strcmp("fir", fb.param));
}

void sdp_a_webrtc_test(void)
{
    sdp_a_extmap_test();
    sdp_a_fingerprint_test();
    sdp_a_group_test();
    sdp_a_ice_candidate_test();
    sdp_a_ice_remote_candidates_test();
    sdp_a_mid_test();
    sdp_a_msid_test();
    sdp_a_rid_test();
    sdp_a_rtcp_test();
    sdp_a_rtcp_fb_test();
    sdp_a_simulcast_test();
    sdp_a_ssrc_test();
    sdp_a_ssrc_group_test();
}
#endif
