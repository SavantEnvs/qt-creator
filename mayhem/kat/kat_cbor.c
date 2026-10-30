// Known-answer probe for the CmdBridge CBOR codec (src/libs/gocmdbridge/server/c/cbor.c).
// Built with the project's NORMAL flags by mayhem/build.sh and driven by mayhem/test.sh, which
// compares stdout against exact expected values.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#pragma GCC diagnostic ignored "-Wunused-function"
#include "cbor.c"

static void dump(const char *tag, const value *v)
{
    if (!v) {
        printf("%s=NULL\n", tag);
        return;
    }
    switch (v->type) {
    case V_STRING: printf("%s=str:%s\n", tag, v->str); break;
    case V_UINT: printf("%s=uint:%llu\n", tag, (unsigned long long) v->u64); break;
    case V_INT: printf("%s=int:%lld\n", tag, (long long) v->i64); break;
    case V_BOOL: printf("%s=bool:%d\n", tag, v->bval); break;
    case V_NULL: printf("%s=null\n", tag); break;
    case V_BYTES: printf("%s=bytes:%zu\n", tag, (size_t) 0); break;
    case V_ARRAY: printf("%s=array:%zu\n", tag, v->nkids); break;
    case V_MAP: printf("%s=map:%zu\n", tag, v->nkids / 2); break;
    }
}

static void hex(const char *tag, const uint8_t *b, size_t n)
{
    printf("%s=", tag);
    for (size_t i = 0; i < n; i++)
        printf("%02x", b[i]);
    printf("\n");
}

static const char *scan_name(cbor_scan_result r)
{
    return r == CBOR_OK ? "ok" : r == CBOR_INCOMPLETE ? "incomplete" : "invalid";
}

int main(void)
{
    /* {"Type": "ping", "Id": "7"} */
    const uint8_t cmd[] = {0xA2, 0x64, 'T', 'y', 'p', 'e', 0x64, 'p', 'i', 'n', 'g', 0x62, 'I', 'd', 0x61, '7'};
    value *m = decode(cmd, sizeof(cmd));
    dump("cmd", m);
    printf("cmd.Type=%s\n", mstr(m, "Type") ? mstr(m, "Type") : "(none)");
    printf("cmd.Id=%s\n", mstr(m, "Id") ? mstr(m, "Id") : "(none)");
    printf("cmd.Missing=%s\n", mstr(m, "Missing") ? "present" : "absent");
    vfree(m);

    /* [1, 42, -1, true, null] */
    const uint8_t arr[] = {0x85, 0x01, 0x18, 0x2A, 0x20, 0xF5, 0xF6};
    value *a = decode(arr, sizeof(arr));
    dump("arr", a);
    if (a && a->type == V_ARRAY) {
        for (size_t i = 0; i < a->nkids; i++) {
            char tag[16];
            snprintf(tag, sizeof(tag), "arr[%zu]", i);
            dump(tag, a->kids[i]);
        }
    }
    vfree(a);

    /* scan classification */
    size_t sz = 0;
    const uint8_t trunc_str[] = {0x63, 'H', 'e'};
    const uint8_t full_str[] = {0x63, 'H', 'e', 'y'};
    const uint8_t bad[] = {0x1C};
    printf("scan.trunc=%s\n", scan_name(cbor_scan(trunc_str, sizeof(trunc_str), &sz)));
    printf("scan.full=%s size=%zu\n", scan_name(cbor_scan(full_str, sizeof(full_str), &sz)), sz);
    printf("scan.bad=%s\n", scan_name(cbor_scan(bad, sizeof(bad), &sz)));
    printf("scan.empty=%s\n", scan_name(cbor_scan(bad, 0, &sz)));

    /* encode known values */
    size_t n = 0;
    value *s = vs("hello");
    uint8_t *e = encode(s, &n);
    hex("enc.hello", e, n);
    free(e);
    vfree(s);

    value *u = vu(1000);
    e = encode(u, &n);
    hex("enc.1000", e, n);
    free(e);
    vfree(u);

    value *round = mk2("Type", vs("pong"), "N", vu(300));
    e = encode(round, &n);
    hex("enc.map", e, n);
    value *back = decode(e, n);
    printf("round.Type=%s round.N=%lld\n", mstr(back, "Type"), (long long) mint(back, "N", -1));
    free(e);
    vfree(back);
    vfree(round);
    return 0;
}
