#include <mruby.h>
#include <mruby/compile.h>
#include <mruby/irep.h>
#include <mruby/error.h>
#include <mruby-aux.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <libgen.h>
#include <pthread.h>
#include <geom/gate/g_gate.h>
#include <sys/bio.h>
#include <sys/param.h>
#include <sys/linker.h>

#ifndef MRB_INT64
# error "suzuna" needs `MRB_INT64` configuration.
#endif

#ifdef MRB_WITHOUT_FLOAT
# error "suzuna" conflicts `MRB_WITHOUT_FLOAT` configuration.
#endif

#define id_provider     mrb_intern_lit(mrb, "provider")
#define id_mediasize    mrb_intern_lit(mrb, "mediasize")
#define id_sectorsize   mrb_intern_lit(mrb, "sectorsize")
#define id_readat       mrb_intern_lit(mrb, "readat")
#define id_writeat      mrb_intern_lit(mrb, "writeat")
#define id_deleteat     mrb_intern_lit(mrb, "deleteat")
#define id_progname     mrb_intern_lit(mrb, "$0")

#ifndef G_GATE_FLAG_READWRITE
# define G_GATE_FLAG_READWRITE  0
#endif

static inline void
aux_stop_the_world(mrb_state *mrb, const char *file, int line, const char *func)
{
    mrb_raisef(mrb, E_NOTIMP_ERROR,
               "STOP THE WORLD!! - %s:%d:%s",
               file, line, func);
}

#define aux_stop_the_world(M) aux_stop_the_world(M, basename((char []){ __FILE__ }), __LINE__, __func__)

static int ggate_ctlfd = 0;

static void
kldload_geom_gate(char *progname)
{
    if (kldload("geom_gate") < 0) {
        switch (errno) {
        case EEXIST:
            break;
        case EPERM:
            {
                int preverr = errno;
                if (kldfind("geom_gate") >= 0) {
                    break;
                }
                errno = preverr;
            }
            /* fall through */
        default:
            fprintf(stderr, "%s: %s - %s\n", basename(progname), strerror(errno), "kldload(geom_gate)");
            exit(EXIT_FAILURE);
        }
    }
}

static bool
aux_ggate_open_ctl(void)
{
    if (ggate_ctlfd == 0) {
        ggate_ctlfd = open("/dev/ggctl", O_RDWR);
        if (ggate_ctlfd < 0) {
            return false;
        }
    } else if (ggate_ctlfd < 0) {
        return false;
    }

    return true;
}

static void
aux_ggate_close_ctl(void)
{
    if (ggate_ctlfd > 0) {
        close(ggate_ctlfd);
    }

    ggate_ctlfd = -1;
}

static void
aux_ggate_post(mrb_state *mrb, unsigned long req, void *p)
{
    if (ioctl(ggate_ctlfd, req, p) < 0) {
        mrb_raisef(mrb, E_RUNTIME_ERROR,
                   "ioctl() が失敗しました - %s",
                   strerror(errno));
    }
}

#define AUX_VALUE_LIST(...)                                         \
        -1 + MRBX_LIST(mrb_value, mrb_nil_value(), __VA_ARGS__) + 1 \

#define AUX_FUNCALL(M, R, N, ...)                       \
        mrb_funcall_argv(M, R, mrbx_symbol(M, N),       \
                AUX_VALUE_LIST(__VA_ARGS__))            \

#define AUX_TRY_FUNCALL(M, R, N, ...)                   \
        (mrb_respond_to(M, R, mrbx_symbol(M, N)) ?      \
                AUX_FUNCALL(M, R, N, __VA_ARGS__) :     \
                mrb_undef_value())                      \

static uint64_t
convertu64(mrb_state *mrb, mrb_value val)
{
    int64_t n = mrb_int(mrb, val);

    if (n < 0) {
        mrb_raisef(mrb, E_RANGE_ERROR,
                   "negative or too huge - %v",
                   val);
    }

    return (uint64_t)n;
}

static uint64_t
aux_attr_get_u64_f(mrb_state *mrb, mrb_value provider, const char *attrname)
{
    mrb_value val = AUX_FUNCALL(mrb, provider, attrname);
    return convertu64(mrb, val);
}

static uint64_t
aux_attr_get_u64(mrb_state *mrb, mrb_value provider, const char *attrname, uint64_t defolt)
{
    mrb_value val = AUX_TRY_FUNCALL(mrb, provider, attrname);

    if (mrb_undef_p(val)) {
        return defolt;
    } else {
        return convertu64(mrb, val);
    }
}

static uint32_t
convertu32(mrb_state *mrb, mrb_value val)
{
    int64_t n = mrb_int(mrb, val);

    if (n < 0 || n > UINT32_MAX) {
        mrb_raisef(mrb, E_RANGE_ERROR,
                   "negative or too huge - %v",
                   val);
    }

    return (uint32_t)n;
}

static uint32_t
aux_attr_get_u32(mrb_state *mrb, mrb_value provider, const char *attrname, uint32_t defolt)
{
    mrb_value val = AUX_TRY_FUNCALL(mrb, provider, attrname);

    if (mrb_undef_p(val)) {
        return defolt;
    } else {
        return convertu32(mrb, val);
    }
}

static uint32_t
aux_attr_get_u32_f(mrb_state *mrb, mrb_value provider, const char *attrname)
{
    mrb_value val = AUX_FUNCALL(mrb, provider, attrname);
    return convertu32(mrb, val);
}

static uint32_t
aux_attr_get_flags(mrb_state *mrb, mrb_value provider, const char *attrname, uint32_t rwmode)
{
    uint32_t flags = aux_attr_get_u32(mrb, provider, attrname, 0);

    if (((flags | rwmode) & 0x03) != rwmode) {
        mrb_raisef(mrb, E_RUNTIME_ERROR,
                   "flags and access-mode are mismatch (flags=%v, access-mode=%v)",
                   mrb_obj_value(mrbx_str_new_as_hexdigest(mrb, flags, 1)),
                   mrb_obj_value(mrbx_str_new_as_hexdigest(mrb, rwmode, 1)));
    }

    return flags;
}

static void
aux_set_string(mrb_state *mrb, mrb_value provider, const char *attrname, char *buf, size_t bufmax)
{
    mrb_value val = AUX_TRY_FUNCALL(mrb, provider, attrname);
    if (mrb_undef_p(val)) {
        return;
    }
    mrb_check_type(mrb, val, MRB_TT_STRING);

    if (RSTRING_LEN(val) >= bufmax) {
        mrb_raisef(mrb, E_RUNTIME_ERROR,
                   "string size is too long (expect %d, but acctual %d)",
                   bufmax - 1, RSTRING_LEN(val));
    }

    memcpy(buf, RSTRING_PTR(val), RSTRING_LEN(val));
    buf[RSTRING_LEN(val)] = '\0';
}

struct suzuna
{
    char *progname;
    int argc;
    char **argv;
    int verbosery;

    int timeout;
    int unitno;
    int unit;
    uint32_t accmode;
    uint64_t sectorsize;

    int exit_status;

    FILE *fp;
    mrbc_context *c;

    mrb_value provider;
    mrb_value err;
};

static int
aux_ggate_create(mrb_state *mrb, struct suzuna *p)
{
    if (p->unitno < 0) {
        p->unitno = G_GATE_UNIT_AUTO;
    }

    struct g_gate_ctl_create req = { 0 };
    req.gctl_version = G_GATE_VERSION;
    req.gctl_mediasize = aux_attr_get_u64_f(mrb, p->provider, "mediasize");
    req.gctl_sectorsize = aux_attr_get_u32_f(mrb, p->provider, "sectorsize");
    req.gctl_flags = aux_attr_get_flags(mrb, p->provider, "flags", p->accmode);
    //req.gctl_maxcount = aux_attr_get_u32(mrb, p->provider, "maxcount", 0);
    req.gctl_timeout = p->timeout;
    aux_set_string(mrb, p->provider, "name", req.gctl_name, NAME_MAX);
    aux_set_string(mrb, p->provider, "info", req.gctl_info, G_GATE_INFOSIZE);
    aux_set_string(mrb, p->provider, "readprov", req.gctl_readprov, NAME_MAX);
    req.gctl_readoffset = aux_attr_get_u64(mrb, p->provider, "readoffset", 0);
    req.gctl_unit = p->unitno;
    aux_ggate_post(mrb, G_GATE_CMD_CREATE, &req);
    p->unit = req.gctl_unit;

    return req.gctl_unit;
}

static mrb_value
suzuna_s_unit(mrb_state *mrb, mrb_value self)
{
    mrb_value provider;
    mrb_get_args(mrb, "o", &provider);

    if (!mrb_respond_to(mrb, provider, id_mediasize) ||
        !mrb_respond_to(mrb, provider, id_sectorsize) ||
        !mrb_respond_to(mrb, provider, id_readat) ||
        !mrb_respond_to(mrb, provider, id_writeat) ||
        !mrb_respond_to(mrb, provider, id_deleteat)) {

        mrb_raisef(mrb, E_TYPE_ERROR,
                   "メソッドを実装する必要があります - %v (#mediasize, #sectorsize, #readat, #writeat, #deleteat)",
                   mrb_any_to_s(mrb, provider));
    }

    mrb_iv_set(mrb, self, id_provider, provider);

    return self;
}

static struct RObject *
suzuna_init(mrb_state *mrb)
{
    struct RClass *suzuna = mrb_define_module(mrb, "Suzuna");
    mrb_define_class_method(mrb, suzuna, "unit", suzuna_s_unit, MRB_ARGS_ANY());
    mrb_define_const(mrb, suzuna, "FLAG_READWRITE", mrb_fixnum_value(G_GATE_FLAG_READWRITE));
    mrb_define_const(mrb, suzuna, "FLAG_READONLY", mrb_fixnum_value(G_GATE_FLAG_READONLY));
    mrb_define_const(mrb, suzuna, "FLAG_WRITEONLY", mrb_fixnum_value(G_GATE_FLAG_WRITEONLY));

    struct RClass *consts = mrb_define_module_under(mrb, suzuna, "Constants");
    mrb_define_const(mrb, consts, "G_GATE_FLAG_READWRITE", mrb_fixnum_value(G_GATE_FLAG_READWRITE));
    mrb_define_const(mrb, consts, "G_GATE_FLAG_READONLY", mrb_fixnum_value(G_GATE_FLAG_READONLY));
    mrb_define_const(mrb, consts, "G_GATE_FLAG_WRITEONLY", mrb_fixnum_value(G_GATE_FLAG_WRITEONLY));
    mrb_include_module(mrb, suzuna, consts);

    return (struct RObject *)suzuna;
}

#define AUX_CALL2ERR(M, R, ID, ...)                     \
        aux_call_and_error(M, R, mrbx_symbol(M, ID),    \
                           AUX_VALUE_LIST(__VA_ARGS__)) \

struct aux_call_and_error
{
    int err;
    mrb_value recv;
    mrb_sym mid;
    int argc;
    const mrb_value *argv;
};

static mrb_value
aux_call_and_error_trial(mrb_state *mrb, mrb_value args)
{
    struct aux_call_and_error *p = (struct aux_call_and_error *)mrb_cptr(args);
    mrb_value err = mrb_funcall_argv(mrb, p->recv, p->mid, p->argc, p->argv);

    if (mrb_fixnum_p(err)) {
        p->err = mrb_fixnum(err);
    } else if (mrb_nil_p(err)) {
        p->err = 0;
    } else if (mrb_exception_p(err)) {
        p->err = EIO;
    } else {
        p->err = EIO;
    }

    return mrb_nil_value();
}

static int
aux_call_and_error(mrb_state *mrb, mrb_value recv, mrb_sym mid, int argc, const mrb_value argv[])
{
    struct aux_call_and_error args = { 0, recv, mid, argc, argv };
    mrb_bool has_err;
    mrb_protect(mrb, aux_call_and_error_trial, mrb_cptr_value(mrb, &args), &has_err);

    if (has_err) {
        return EIO;
    } else {
        return args.err;
    }
}

static void
loop_event(mrb_state *mrb, struct suzuna *p)
{
    // MEMO: もしかしたら、mrb_funcall 中に realloc などでアドレスが変更される RString::ptr は使えない？

    size_t bufsize = 16 * 1024;
    mrb_value buf = mrb_str_new(mrb, NULL, bufsize);
    struct g_gate_ctl_io ctlio = { 0 };
    ctlio.gctl_version = G_GATE_VERSION;
    ctlio.gctl_unit = p->unit;

    const char *file = basename((char []){ __FILE__ });

    for (;;) {
        struct RString *bufp = mrb_str_ptr(buf);
        mrb_str_modify(mrb, bufp);
        ctlio.gctl_cmd = 0;
        ctlio.gctl_offset = 0;
        ctlio.gctl_data = RSTR_PTR(bufp);
        ctlio.gctl_length = RSTR_CAPA(bufp);
        ctlio.gctl_error = 0;
        aux_ggate_post(mrb, G_GATE_CMD_START, &ctlio);

        fprintf(stderr, "%s:%d:%s: G_GATE_CMD_START - unit:%d, seq:%zd, cmd:%d, err:%d, off:%zd, len:%zd\n",
                file, __LINE__, __func__,
                ctlio.gctl_unit, ctlio.gctl_seq, ctlio.gctl_cmd, ctlio.gctl_error,
                ctlio.gctl_offset, ctlio.gctl_length);

        switch (ctlio.gctl_error) {
        default:
            AUX_TRY_FUNCALL(mrb, p->provider, "cleanup");
            mrb_raisef(mrb, E_RUNTIME_ERROR,
                       "システムコールエラー: %s",
                       strerror(ctlio.gctl_error));
            break;
        case ECANCELED:
        case ENXIO:
            AUX_TRY_FUNCALL(mrb, p->provider, "cleanup");
            mrb_raisef(mrb, E_RUNTIME_ERROR,
                       "/dev/ggate%d が破棄されました",
                       p->unit);
            break;
        case ENOMEM:
            mrb_str_resize(mrb, buf, ctlio.gctl_length);
            continue;
        case 0:
            switch (ctlio.gctl_cmd) {
            case BIO_READ:
                if (RSTR_CAPA(bufp) < ctlio.gctl_length) {
                    mrb_str_resize(mrb, buf, ctlio.gctl_length);
                }
                RSTR_SET_LEN(bufp, 0);
                ctlio.gctl_error = AUX_CALL2ERR(
                        mrb, p->provider, "readat",
                        mrb_fixnum_value(ctlio.gctl_offset),
                        mrb_fixnum_value(ctlio.gctl_length),
                        buf);
                if (RSTR_CAPA(bufp) < ctlio.gctl_length) {
                    mrb_str_resize(mrb, buf, ctlio.gctl_length);
                }
                memset(RSTR_PTR(bufp) + RSTR_LEN(bufp), 0, ctlio.gctl_length - RSTR_LEN(bufp));
                ctlio.gctl_data = RSTR_PTR(bufp);
                ctlio.gctl_length = RSTR_LEN(bufp);
                break;
            case BIO_WRITE:
                RSTR_SET_LEN(bufp, ctlio.gctl_length);
                ctlio.gctl_error = AUX_CALL2ERR(
                        mrb, p->provider, "writeat",
                        mrb_fixnum_value(ctlio.gctl_offset),
                        buf);
                break;
            case BIO_DELETE:
                ctlio.gctl_error = AUX_CALL2ERR(
                        mrb, p->provider, "deleteat",
                        mrb_fixnum_value(ctlio.gctl_offset),
                        mrb_fixnum_value(ctlio.gctl_length));
                break;
            default:
                ctlio.gctl_error = EOPNOTSUPP;
                break;
            }

            aux_ggate_post(mrb, G_GATE_CMD_DONE, &ctlio);
        }
    }
}

static mrb_value
suzuna_main(mrb_state *mrb, struct suzuna *p)
{
    struct RObject *suzuna = suzuna_init(mrb);
    p->c = mrbc_context_new(mrb);

    mrbc_filename(mrb, p->c, p->argv[0]);
    mrb_gv_set(mrb, id_progname, mrb_str_new_cstr(mrb, p->argv[0]));

    mrb_value mrbargv = mrb_ary_new_capa(mrb, p->argc);
    mrb_define_global_const(mrb, "ARGV", mrbargv);
    for (int i = 1; i < p->argc; i ++) {
        mrb_ary_push(mrb, mrbargv, mrb_str_new_cstr_frozen(mrb, p->argv[i]));
    }

    mrb_value ret = mrb_load_file_cxt(mrb, p->fp, p->c);
    if (mrb_nil_p(ret) && mrb->exc) {
        ret = mrb_obj_value(mrb->exc);
    }

    mrbc_context_free(mrb, p->c);
    p->c = NULL;

    fclose(p->fp);
    p->fp = NULL;

    if (mrb_exception_p(ret) || mrb_break_p(ret)) {
        mrb_exc_raise(mrb, ret);
    }

    p->provider = mrb_obj_iv_get(mrb, suzuna, id_provider);

    //aux_stop_the_world(mrb);

    aux_ggate_create(mrb, p);

    if (p->unitno < 0) {
        printf(G_GATE_PROVIDER_NAME "%d\n", p->unit);
    }

    loop_event(mrb, p);

    return mrb_nil_value();
}

static mrb_value
suzuna_main_wrapper2(mrb_state *mrb, mrb_value args)
{
    struct suzuna *p = (struct suzuna *)mrb_cptr(args);
    suzuna_main(mrb, p);
    return mrb_nil_value();
}

static mrb_value
suzuna_dump_error(mrb_state *mrb, mrb_value args)
{
    struct suzuna *p = (struct suzuna *)mrb_cptr(args);
    fprintf(stderr, "%s: %s\n", p->argv[0], RSTRING_CSTR(mrb, mrb_str_to_str(mrb, p->err)));
    // TODO: バックトレースも表示する
    return mrb_nil_value();
}

static mrb_value
suzuna_main_wrapper(mrb_state *mrb, mrb_value args)
{
    struct suzuna *p = (struct suzuna *)mrb_cptr(args);
    mrb_bool haserr;
    mrb_value ret = mrb_protect(mrb, suzuna_main_wrapper2, args, &haserr);
    if (p->fp) { fclose(p->fp); }
    if (p->c) { mrbc_context_free(mrb, p->c); }
    if (haserr) {
        p->err = ret;
        p->exit_status = EXIT_FAILURE;
        mrb_protect(mrb, suzuna_dump_error, args, NULL);
    } else {
        p->exit_status = EXIT_SUCCESS;
    }
    return mrb_nil_value();
}

static void
usage(char *progname, bool verbose)
{
    FILE *out = (verbose ? stdout : stderr);

    fprintf(out, "%s: %s [-hqv] [-o <rw|ro|wo>] [-t n] [-u n] [-r rb] <%s> <%s>...\n",
            "使い方", basename(progname),
            "Ruby スクリプトファイル", "Ruby スクリプト引数");

    if (verbose) {
        fputs("\n"
              "汎用スイッチ:\n"
              " -h     使い方表示 (今出てるやつのこと)\n"
              " -q     (未実装) 冗長表示を無効にする\n"
              " -v     (未実装) 冗長表示にする\n"
              " -vv    (未実装) もっとうるさく！\n"
              " -vvv   (未実装) おおいにうるさく！\n"
              " -vvv.. (未実装) どこまでもうるさく！\n"
              "\n"
              "GEOM Gate スイッチ:\n"
              " -o rw  読み書き可能にする (既定の動作)\n"
              " -o ro  読み込み専用にする\n"
              " -o wo  書き出し専用にする\n"
              " -t n   I/O 要求の制限時間を秒数で指定する (既定は 30)\n"
              " -u n   ggate ユニット番号を指定する (既定は -1)\n"
              "\n"
              "mruby スイッチ:\n"
              " -r rb  (未実装) 事前に読み込む Ruby スクリプトファイルを指定する\n",
              out);
        exit(EXIT_SUCCESS);
    } else {
        fputs("        --help を与えると詳しい情報が出力されます。\n", out);
        exit(EXIT_FAILURE);
    }
}

int
main(int argc, char *argv[])
{
    struct suzuna suz = { 0 };
    suz.unitno = G_GATE_UNIT_AUTO;
    suz.accmode = G_GATE_FLAG_READONLY;
    suz.progname = argv[0];
    suz.verbosery = 1;

    int ch;
    while ((ch = getopt(argc, argv, "hqvo:t:u:r:-:")) != -1) {
        switch (ch) {
        case '-':
            if (strcmp(optarg, "help") != 0) {
                goto unknown_switch;
            }
            /* FALLTHROUGH */
        case 'h':
            usage(suz.progname, true);
            break;
        case 'q':
            suz.verbosery = 0;
            break;
        case 'v':
            suz.verbosery ++;
            break;
        case 'o':
            if (strcmp(optarg, "rw") == 0) {
                suz.accmode = 0;
            } else if (strcmp(optarg, "ro") == 0) {
                suz.accmode = G_GATE_FLAG_READONLY;
            } else if (strcmp(optarg, "wo") == 0) {
                suz.accmode = G_GATE_FLAG_WRITEONLY;
            } else {
                goto unknown_switch;
            }

            break;
        case 't':
            suz.timeout = strtol(optarg, NULL, 10);

            if (suz.timeout < 1) {
                suz.timeout = 30;
            } else if (suz.timeout > 1000) {
                suz.timeout = 500;
            }

            break;
        case 'u':
            suz.unitno = strtol(optarg, NULL, 10);
            break;
        case 'r':
            // TODO: require ruby を追加する
            break;
        case '?':
        default:
unknown_switch:
            usage(suz.progname, false);
        }
    }

    argc -= optind;
    argv += optind;

    if (argc < 1) {
        usage(suz.progname, false);
    }

    kldload_geom_gate(suz.progname);
    if (!aux_ggate_open_ctl()) {
        fprintf(stderr, "%s: %s - %s\n", basename(suz.progname), strerror(errno), "/dev/ggctl");
        return EXIT_FAILURE;
    }

    suz.fp = fopen(argv[0], "r");
    if (suz.fp == NULL) {
        fprintf(stderr, "%s: %s - %s\n", basename(suz.progname), strerror(errno), argv[0]);
        return EXIT_FAILURE;
    }

    suz.argc = argc;
    suz.argv = argv;

    mrb_state *mrb = mrb_open();
    mrb_protect(mrb, suzuna_main_wrapper, mrb_cptr_value(mrb, &suz), NULL);
    mrb_close(mrb);

    return suz.exit_status;
}

/*
 * 資料:
 *  https://github.com/freebsd/freebsd/blob/releng/12.1/sbin/ggate/ggatel/ggatel.c
 *  https://github.com/freebsd/freebsd/blob/releng/12.1/sys/geom/gate/g_gate.h
 *  https://github.com/freebsd/freebsd/blob/releng/12.1/sys/geom/gate/g_gate.c
 */
