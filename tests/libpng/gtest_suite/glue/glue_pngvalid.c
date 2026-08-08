/* Glue: expose every scenario of the upstream libpng correctness validator
 * (pngvalid.c) as an addressable test.
 *
 * pngvalid.c is included VERBATIM with its main() renamed to pngvalid_main();
 * PNG_FREESTANDING_TESTS makes it include <png.h>/<zlib.h>/<cexcept.h> from
 * the include path instead of upstream-tree relative paths. pngvalid
 * self-generates all its test images, so no external files are needed.
 *
 * Upstream's four test groups (--standard, --size, --transform, --gamma) each
 * run thousands of independent scenarios inside one loop that stops at the
 * first failure. Wrapping a group as a single GoogleTest case would collapse
 * all of them into one pass/fail bit, so this file reproduces each group's
 * loop structure and reports one verdict per scenario. The upstream file is
 * not modified: its static loop bodies (standard_test, transform_test,
 * gamma_test and the naming helpers) are called directly, because including
 * the source puts them in scope.
 *
 * ONE walk per group serves both enumeration and execution, so the order gtest
 * registers scenarios in and the order the recording run visits them cannot
 * drift apart. See pv_record.h for the recording protocol.
 *
 * The transcribed loops mirror upstream's perform_*_test() functions exactly,
 * with two deliberate differences:
 *   - the `if (fail(pm)) return;` early exits are dropped, so one failing
 *     scenario no longer hides every later one (upstream itself continues this
 *     way under its -l/--log and -v options);
 *   - each scenario's error counters are cleared before it runs, which is what
 *     turns pngvalid's run-wide error tally into a per-scenario verdict and
 *     leaves that scenario's own first message in ps->error.
 * Everything else — loop bounds, parameter sets, ordering, and the setup each
 * group needs — is upstream's.
 */
#define PNG_FREESTANDING_TESTS
#define main pngvalid_main
#include "pngvalid.c"
#undef main

#include "pv_record.h"

#include <string.h>

/* ------------------------------------------------------------------------
 * Scenario naming
 *
 * gtest test names have to survive --gtest_filter expressions, where ':'
 * separates patterns and '*'/'?' are wildcards, so upstream's descriptive
 * names ("rgb 8 bit interlaced(libpng) 3x5", "transform: +png_set_expand")
 * are reduced to [A-Za-z0-9_]. Each name keeps a zero-padded index prefix:
 * that guarantees uniqueness even where two scenarios differ only in a
 * parameter the descriptive part does not spell out, and it makes the gtest
 * listing follow upstream's execution order.
 */

/* Appends `text` to `out` with every run of non-alphanumeric characters
 * collapsed into a single underscore. */
static void pv_sanitize(char *out, size_t out_size, size_t *pos, const char *text)
{
    int last_was_sep = 0;
    for (; *text != 0 && *pos + 1 < out_size; ++text)
    {
        const char c = *text;
        const int alnum = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
                          (c >= 'A' && c <= 'Z');
        if (alnum)
        {
            out[(*pos)++] = c;
            last_was_sep = 0;
        }
        else if (!last_was_sep && *pos > 0)
        {
            out[(*pos)++] = '_';
            last_was_sep = 1;
        }
    }
    while (*pos > 0 && out[*pos - 1] == '_')
        --*pos;
    out[*pos] = 0;
}

/* Starts a scenario name with its index prefix. */
static size_t pv_name_begin(char *out, size_t out_size, int index)
{
    const int written = snprintf(out, out_size, "S%05d_", index);
    return (written > 0 && (size_t)written < out_size) ? (size_t)written : 0;
}

/* ------------------------------------------------------------------------
 * The walk
 */

typedef struct
{
    int execute;      /* 0: enumerate names only, 1: run and report verdicts */
    int index;        /* index of the scenario being visited */
    png_modifier *pm;
} pv_walk;

/* Opens a scenario. Returns nonzero when the caller should run it. */
static int pv_enter(pv_walk *w, const char *name)
{
    if (!w->execute)
    {
        pv_emit_name(w->index, name);
        return 0;
    }
    /* Per-scenario verdict: pngvalid tallies errors for a whole run, and only
     * keeps the FIRST message (store_log ignores later ones). Clearing both
     * here scopes the tally and the message to this scenario. Nothing in
     * pngvalid reads these counters except fail() and the final summary,
     * neither of which this walk uses. */
    w->pm->this.nerrors = 0;
    w->pm->this.nwarnings = 0;
    w->pm->this.error[0] = 0;
    return 1;
}

/* Closes a scenario, reporting its verdict, and advances the index. */
static void pv_leave(pv_walk *w)
{
    if (w->execute)
    {
        const int failed = w->pm->this.nerrors > 0 ||
            (w->pm->this.treat_warnings_as_errors && w->pm->this.nwarnings > 0);
        pv_emit_result(w->index, failed, w->pm->this.error);
    }
    ++w->index;
}

/* ------------------------------------------------------------------------
 * Group setup
 *
 * Copied from pngvalid.c's main(), which sets these before parsing arguments.
 * The per-group block at the end applies exactly what the command line used
 * to: --standard, --size, --transform and --gamma respectively. Upstream's
 * signal handlers are deliberately not installed: a crashing library under
 * test must kill the recording child so the parent can record the scenarios
 * it never reached, rather than be caught and reported as a clean exit.
 */
/* The standard gamma values, copied from main()'s local `gammas[]` (a local
 * there, so it cannot be referenced from here). Order matters: the gamma
 * sub-modes index this array, and only the first `ngamma_tests` entries are
 * used. */
static double pv_gammas[] = {2.2, 1.0, 2.2/1.45, 1.8, 1.5, 2.4, 2.5, 2.62, 2.9};

static void pv_setup(png_modifier *pm, int group)
{
    modifier_init(pm);

    /* Preallocate the image buffer, as main() does. */
    store_ensure_image(&pm->this, NULL, 2, TRANSFORM_ROWMAX, TRANSFORM_HEIGHTMAX);

    pm->this.treat_warnings_as_errors = 1;
    pm->assume_16_bit_calculations = PNG_LIBPNG_VER == 10700;
    pm->calculations_use_input_precision = 1U;

    pm->gammas = pv_gammas;
    pm->ngammas = ARRAY_SIZE(pv_gammas);
    pm->ngamma_tests = 0; /* default to off */

#  ifdef PNG_WRITE_tRNS_SUPPORTED
        pm->test_tRNS = 1;
#  endif
    pm->test_lbg = 1;
    pm->test_lbg_gamma_threshold = 1;
    pm->test_lbg_gamma_transform = 1;
    pm->test_lbg_gamma_sbit = 1;
    pm->test_lbg_gamma_composition = PNG_LIBPNG_VER == 10700;

    pm->encodings = test_encodings;
    pm->nencodings = ARRAY_SIZE(test_encodings);

#  if PNG_LIBPNG_VER != 10700
        pm->sbitlow = 8U; /* because libpng doesn't do sBIT below 8! */
#  else
        pm->sbitlow = 1U;
#  endif

    pm->use_input_precision_16to8 = 1U;
    pm->use_input_precision_sbit = 1U;

    pm->maxout8 = .1;
    pm->maxabs8 = .00005;
    pm->maxcalc8 = 1./255;
    pm->maxpc8 = .499;
    pm->maxout16 = .499;
    pm->maxabs16 = .00005;
    pm->maxcalc16 = 1./65535;
#  if PNG_LIBPNG_VER != 10700
        pm->maxcalcG = 1./((1<<PNG_MAX_GAMMA_8)-1);
#  else
        pm->maxcalcG = 1./((1<<16)-1);
#  endif
    pm->maxpc16 = .005;

    switch (group)
    {
        case PV_GROUP_STANDARD:
            pm->test_standard = 1;
            break;
        case PV_GROUP_SIZE:
            pm->test_size = 1;
            break;
        case PV_GROUP_TRANSFORM:
            pm->test_transform = 1;
            break;
        case PV_GROUP_GAMMA:
            /* What --gamma sets: two gamma values, every sub-mode. */
            pm->ngamma_tests = 2U;
            pm->test_gamma_threshold = 1;
            pm->test_gamma_transform = 1;
            pm->test_gamma_sbit = 1;
            pm->test_gamma_scale16 = 1;
            pm->test_gamma_background = 1;
            pm->test_gamma_alpha_mode = 1;
            break;
        default:
            break;
    }
}

/* ------------------------------------------------------------------------
 * STANDARD (upstream perform_standard_test / test_standard)
 */
static void pv_walk_standard(pv_walk *w)
{
    /* The colour type / bit depth ranges of upstream perform_standard_test. */
    static const struct { png_byte colour_type; int bdlo, bdhi; } sets[] = {
        { 0, 0, READ_BDHI },
        { 2, 3, READ_BDHI },
        { 3, 0, 3 },
        { 4, 3, READ_BDHI },
        { 6, 3, READ_BDHI }
    };
    unsigned int s;

    /* main() runs these alongside the format sweep for --standard. */
    if (pv_enter(w, "S00000_interlace_macro_validation"))
        perform_interlace_macro_validation();
    pv_leave(w);

    if (pv_enter(w, "S00001_formatting"))
        perform_formatting_test(&w->pm->this);
    pv_leave(w);

    for (s = 0; s < ARRAY_SIZE(sets); ++s)
    {
        int bdlo;

        for (bdlo = sets[s].bdlo; bdlo <= sets[s].bdhi; ++bdlo)
        {
            int interlace_type;

            for (interlace_type = PNG_INTERLACE_NONE;
                 interlace_type < INTERLACE_LAST; ++interlace_type)
            {
                const png_uint_32 id = FILEID(sets[s].colour_type, DEPTH(bdlo),
                    0/*palette*/, interlace_type, 0, 0, 0);
                char name[128];
                size_t pos = pv_name_begin(name, sizeof name, w->index);
                char detail[96];

                detail[0] = 0;
                standard_name_from_id(detail, sizeof detail, 0, id);
                pv_sanitize(name, sizeof name, &pos, detail);

                if (pv_enter(w, name))
                    standard_test(&w->pm->this, id, do_read_interlace,
                        w->pm->use_update_info);
                pv_leave(w);
            }
        }
    }

    /* Error handling comes last, as it does in main(). */
    {
        char name[64];
        (void)pv_name_begin(name, sizeof name, w->index);
        strncat(name, "error_handling", sizeof name - strlen(name) - 1);
        if (pv_enter(w, name))
            perform_error_test(w->pm);
        pv_leave(w);
    }
}

/* ------------------------------------------------------------------------
 * SIZE (upstream perform_size_test / test_size)
 */

/* One size scenario. `tag` distinguishes the read/write combinations that
 * share a file id. */
static void pv_size_scenario(pv_walk *w, png_uint_32 id, int do_interlace,
    const char *tag)
{
    char name[144];
    size_t pos = pv_name_begin(name, sizeof name, w->index);
    char detail[96];

    detail[0] = 0;
    standard_name_from_id(detail, sizeof detail, 0, id);
    pv_sanitize(name, sizeof name, &pos, detail);
    pv_sanitize(name, sizeof name, &pos, tag);

    if (pv_enter(w, name))
        standard_test(&w->pm->this, id, do_interlace, w->pm->use_update_info);
    pv_leave(w);
}

static void pv_walk_size_range(pv_walk *w, png_byte colour_type, int bdlo,
    int bdhi)
{
    /* Upstream's sampling steps: testing every width and height at every bit
     * depth is a waste of time, hence the hinc/winc stride tables. */
    static const png_byte hinc[] = {1, 3, 11, 1, 5};
    static const png_byte winc[] = {1, 9, 5, 7, 1};
    const int save_bdlo = bdlo;

    for (; bdlo <= bdhi; ++bdlo)
    {
        png_uint_32 h, wd;

        for (h = 1; h <= 16; h += hinc[bdlo])
        {
            for (wd = 1; wd <= 16; wd += winc[bdlo])
            {
                /* Validate the write side: four combinations against the
                 * sequential reader. */
                pv_size_scenario(w, FILEID(colour_type, DEPTH(bdlo), 0/*palette*/,
                    PNG_INTERLACE_NONE, wd, h, 0), 0/*do_interlace*/, "wr_seq");

                pv_size_scenario(w, FILEID(colour_type, DEPTH(bdlo), 0/*palette*/,
                    PNG_INTERLACE_NONE, wd, h, 1), 0/*do_interlace*/, "wr_seq_own");

                /* Now the interlaced read side; in the progressive case this
                 * changes the code used for the non-interlaced case too. */
                pv_size_scenario(w, FILEID(colour_type, DEPTH(bdlo), 0/*palette*/,
                    PNG_INTERLACE_NONE, wd, h, 0), 1/*do_interlace*/, "rd_own");

#              if CAN_WRITE_INTERLACE
                /* Validate the pngvalid code itself. */
                pv_size_scenario(w, FILEID(colour_type, DEPTH(bdlo), 0/*palette*/,
                    PNG_INTERLACE_ADAM7, wd, h, 1), 1/*do_interlace*/,
                    "selfcheck_adam7");
#              endif
            }
        }
    }

    /* libpng's own interlace handling, now that the pngvalid version is known
     * good. */
    for (bdlo = save_bdlo; bdlo <= bdhi; ++bdlo)
    {
        png_uint_32 h, wd;

        for (h = 1; h <= 16; h += hinc[bdlo])
        {
            for (wd = 1; wd <= 16; wd += winc[bdlo])
            {
#              ifdef PNG_READ_INTERLACING_SUPPORTED
#              if CAN_WRITE_INTERLACE
                /* pngvalid-generated interlaced images first: this isolates
                 * read-side errors. */
                pv_size_scenario(w, FILEID(colour_type, DEPTH(bdlo), 0/*palette*/,
                    PNG_INTERLACE_ADAM7, wd, h, 1), 0/*do_interlace*/,
                    "rd_adam7_libpng");
#              endif
#              endif /* READ_INTERLACING */

#              ifdef PNG_WRITE_INTERLACING_SUPPORTED
                /* The libpng write side against the pngvalid read side. */
                pv_size_scenario(w, FILEID(colour_type, DEPTH(bdlo), 0/*palette*/,
                    PNG_INTERLACE_ADAM7, wd, h, 0), 1/*do_interlace*/,
                    "wr_adam7_libpng");
#              endif

#              ifdef PNG_READ_INTERLACING_SUPPORTED
#              ifdef PNG_WRITE_INTERLACING_SUPPORTED
                /* Both together. */
                pv_size_scenario(w, FILEID(colour_type, DEPTH(bdlo), 0/*palette*/,
                    PNG_INTERLACE_ADAM7, wd, h, 0), 0/*do_interlace*/,
                    "rdwr_adam7_libpng");
#              endif
#              endif /* READ_INTERLACING */
            }
        }
    }
}

static void pv_walk_size(pv_walk *w)
{
    pv_walk_size_range(w, 0, 0, READ_BDHI);
    pv_walk_size_range(w, 2, 3, READ_BDHI);
    /* Upstream skips the palette range here: it costs a lot and adds little
     * over the grayscale range. */
    pv_walk_size_range(w, 4, 3, READ_BDHI);
    pv_walk_size_range(w, 6, 3, READ_BDHI);
}

/* ------------------------------------------------------------------------
 * TRANSFORM (upstream perform_transform_test)
 */
static void pv_walk_transform(pv_walk *w)
{
    png_byte colour_type = 0;
    png_byte bit_depth = 0;
    unsigned int palette_number = 0;

    while (next_format(&colour_type, &bit_depth, &palette_number,
                w->pm->test_lbg, w->pm->test_tRNS))
    {
        png_uint_32 counter = 0;
        size_t base_pos;
        char upstream_name[64];

        base_pos = safecat(upstream_name, sizeof upstream_name, 0, "transform:");

        for (;;)
        {
            size_t pos = base_pos;
            const image_transform *list = 0;
            char name[160];
            size_t name_pos;

            /* 'max' is hardwired to 1, as upstream does. */
            counter = image_transform_add(&list, 1/*max*/, counter,
                upstream_name, sizeof upstream_name, &pos, colour_type,
                bit_depth);

            if (counter == 0)
                break;

            name_pos = pv_name_begin(name, sizeof name, w->index);
            pv_sanitize(name, sizeof name, &name_pos, upstream_name);
            {
                char fmt[48];
                snprintf(fmt, sizeof fmt, "_ct%u_bd%u_p%u", (unsigned)colour_type,
                    (unsigned)bit_depth, palette_number);
                pv_sanitize(name, sizeof name, &name_pos, fmt);
            }

            if (pv_enter(w, name))
            {
                /* The test may ask to be repeated with the next modifier
                 * encoding; that is one scenario, not several. */
                do
                {
                    w->pm->repeat = 0;
                    transform_test(w->pm, FILEID(colour_type, bit_depth,
                        palette_number, w->pm->interlace_type, 0, 0, 0), list,
                        upstream_name);
                }
                while (w->pm->repeat);
            }
            pv_leave(w);
        }
    }
}

/* ------------------------------------------------------------------------
 * GAMMA (upstream perform_gamma_test and its six sub-modes)
 */

/* Appends the parameter tail shared by most gamma scenario names. */
static void pv_gamma_tail(char *name, size_t size, size_t *pos,
    png_byte colour_type, png_byte bit_depth, unsigned int palette_number)
{
    char buf[48];
    snprintf(buf, sizeof buf, "_ct%u_bd%u_p%u", (unsigned)colour_type,
        (unsigned)bit_depth, palette_number);
    pv_sanitize(name, size, pos, buf);
}

static void pv_walk_gamma_threshold(pv_walk *w)
{
    png_modifier *pm = w->pm;
    png_byte colour_type = 0;
    png_byte bit_depth = 0;
    unsigned int palette_number = 0;

    while (next_format(&colour_type, &bit_depth, &palette_number,
                pm->test_lbg_gamma_threshold, pm->test_tRNS))
        if (palette_number < 2)
    {
        double test_gamma = 1.0;
        int step = 0;

        while (test_gamma >= .4)
        {
            char name[160];
            size_t pos = pv_name_begin(name, sizeof name, w->index);
            char buf[64];

            pv_sanitize(name, sizeof name, &pos, "threshold");
            pv_gamma_tail(name, sizeof name, &pos, colour_type, bit_depth,
                palette_number);
            snprintf(buf, sizeof buf, "_g%d", (int)(test_gamma * 1000. + .5));
            pv_sanitize(name, sizeof name, &pos, buf);

            if (pv_enter(w, name))
                gamma_threshold_test(pm, colour_type, bit_depth,
                    pm->interlace_type, test_gamma, 1/test_gamma);
            pv_leave(w);

            test_gamma *= .95;
            ++step;
        }

        /* And a special test for sRGB. */
        {
            char name[160];
            size_t pos = pv_name_begin(name, sizeof name, w->index);

            pv_sanitize(name, sizeof name, &pos, "threshold_sRGB");
            pv_gamma_tail(name, sizeof name, &pos, colour_type, bit_depth,
                palette_number);

            if (pv_enter(w, name))
                gamma_threshold_test(pm, colour_type, bit_depth,
                    pm->interlace_type, .45455, 2.2);
            pv_leave(w);
        }
    }
}

static void pv_walk_gamma_transform(pv_walk *w)
{
    png_modifier *pm = w->pm;
    png_byte colour_type = 0;
    png_byte bit_depth = 0;
    unsigned int palette_number = 0;

    while (next_format(&colour_type, &bit_depth, &palette_number,
                pm->test_lbg_gamma_transform, pm->test_tRNS))
    {
        unsigned int i, j;

        for (i = 0; i < pm->ngamma_tests; ++i)
        {
            for (j = 0; j < pm->ngamma_tests; ++j)
            {
                if (i != j)
                {
                    char name[160];
                    size_t pos = pv_name_begin(name, sizeof name, w->index);
                    char buf[48];

                    pv_sanitize(name, sizeof name, &pos, "transform");
                    pv_gamma_tail(name, sizeof name, &pos, colour_type,
                        bit_depth, palette_number);
                    snprintf(buf, sizeof buf, "_g%u_to_%u", i, j);
                    pv_sanitize(name, sizeof name, &pos, buf);

                    if (pv_enter(w, name))
                        gamma_transform_test(pm, colour_type, bit_depth,
                            palette_number, pm->interlace_type,
                            1/pm->gammas[i], pm->gammas[j], 0/*sBIT*/,
                            pm->use_input_precision, 0/*do not scale16*/);
                    pv_leave(w);
                }
            }
        }
    }
}

static void pv_walk_gamma_sbit(pv_walk *w)
{
    png_modifier *pm = w->pm;
    png_byte sbit;

    /* Only colour and grayscale are interesting here, and only bit depths
     * where sbit is below the bit depth. */
    for (sbit = pm->sbitlow; sbit < (1 << READ_BDHI); ++sbit)
    {
        png_byte colour_type = 0, bit_depth = 0;
        unsigned int npalette = 0;

        while (next_format(&colour_type, &bit_depth, &npalette,
                    pm->test_lbg_gamma_sbit, pm->test_tRNS))
            if ((colour_type & PNG_COLOR_MASK_ALPHA) == 0 &&
                ((colour_type == 3 && sbit < 8) ||
                 (colour_type != 3 && sbit < bit_depth)))
        {
            unsigned int i;

            for (i = 0; i < pm->ngamma_tests; ++i)
            {
                unsigned int j;

                for (j = 0; j < pm->ngamma_tests; ++j)
                {
                    if (i != j)
                    {
                        char name[160];
                        size_t pos = pv_name_begin(name, sizeof name, w->index);
                        char buf[64];

                        pv_sanitize(name, sizeof name, &pos, "sbit");
                        snprintf(buf, sizeof buf, "%u", (unsigned)sbit);
                        pv_sanitize(name, sizeof name, &pos, buf);
                        pv_gamma_tail(name, sizeof name, &pos, colour_type,
                            bit_depth, npalette);
                        snprintf(buf, sizeof buf, "_g%u_to_%u", i, j);
                        pv_sanitize(name, sizeof name, &pos, buf);

                        if (pv_enter(w, name))
                            gamma_transform_test(pm, colour_type, bit_depth,
                                npalette, pm->interlace_type, 1/pm->gammas[i],
                                pm->gammas[j], sbit,
                                pm->use_input_precision_sbit, 0/*scale16*/);
                        pv_leave(w);
                    }
                }
            }
        }
    }
}

#ifdef DO_16BIT
static void pv_walk_gamma_scale16(pv_walk *w)
{
    png_modifier *pm = w->pm;
    /* The colour types upstream runs through the 16-to-8 reduction. */
    static const png_byte colour_types[] = { 0, 2, 4, 6 };
    unsigned int i, j;

    for (i = 0; i < pm->ngamma_tests; ++i)
    {
        for (j = 0; j < pm->ngamma_tests; ++j)
        {
            if (i != j &&
                fabs(pm->gammas[j]/pm->gammas[i]-1) >= PNG_GAMMA_THRESHOLD)
            {
                unsigned int c;

                for (c = 0; c < ARRAY_SIZE(colour_types); ++c)
                {
                    char name[160];
                    size_t pos = pv_name_begin(name, sizeof name, w->index);
                    char buf[64];

                    pv_sanitize(name, sizeof name, &pos, "scale16");
                    pv_gamma_tail(name, sizeof name, &pos, colour_types[c], 16, 0);
                    snprintf(buf, sizeof buf, "_g%u_to_%u", i, j);
                    pv_sanitize(name, sizeof name, &pos, buf);

                    if (pv_enter(w, name))
                        gamma_transform_test(pm, colour_types[c], 16, 0,
                            pm->interlace_type, 1/pm->gammas[i], pm->gammas[j],
                            SBIT_16_TO_8, pm->use_input_precision_16to8,
                            1 /*scale16*/);
                    pv_leave(w);
                }
            }
        }
    }
}
#endif /* DO_16BIT */

#if defined(PNG_READ_BACKGROUND_SUPPORTED) || defined(PNG_READ_ALPHA_MODE_SUPPORTED)
static void pv_walk_gamma_composition(pv_walk *w, int do_background,
    int expand_16, const char *label)
{
    png_modifier *pm = w->pm;
    png_byte colour_type = 0;
    png_byte bit_depth = 0;
    unsigned int palette_number = 0;

    /* Upstream skips the formats that have no transparency to compose. */
    while (next_format(&colour_type, &bit_depth, &palette_number,
                pm->test_lbg_gamma_composition, pm->test_tRNS))
        if ((colour_type & PNG_COLOR_MASK_ALPHA) != 0
            || (colour_type != 3 && palette_number != 0))
    {
        unsigned int i, j;

        /* The i==j case is relevant here, so it is not skipped. */
        for (i = 0; i < pm->ngamma_tests; ++i)
        {
            for (j = 0; j < pm->ngamma_tests; ++j)
            {
                char name[176];
                size_t pos = pv_name_begin(name, sizeof name, w->index);
                char buf[64];

                pv_sanitize(name, sizeof name, &pos, label);
                snprintf(buf, sizeof buf, "%d", do_background);
                pv_sanitize(name, sizeof name, &pos, buf);
                pv_gamma_tail(name, sizeof name, &pos, colour_type, bit_depth,
                    palette_number);
                snprintf(buf, sizeof buf, "_g%u_to_%u", i, j);
                pv_sanitize(name, sizeof name, &pos, buf);

                if (pv_enter(w, name))
                    gamma_composition_test(pm, colour_type, bit_depth,
                        palette_number, pm->interlace_type, 1/pm->gammas[i],
                        pm->gammas[j], pm->use_input_precision, do_background,
                        expand_16);
                pv_leave(w);
            }
        }
    }
}
#endif /* READ_BACKGROUND || READ_ALPHA_MODE */

/* Mirrors upstream perform_gamma_test, including the temporary overrides it
 * applies around individual sub-modes. The summary printing is dropped: the
 * verdicts are the output here. */
static void pv_walk_gamma(pv_walk *w)
{
    png_modifier *pm = w->pm;
    const unsigned int calculations_use_input_precision =
        pm->calculations_use_input_precision;
#  ifdef PNG_READ_BACKGROUND_SUPPORTED
        const double maxout8 = pm->maxout8;
#  endif

    if (!pm->this.speed && pm->test_gamma_threshold)
        pv_walk_gamma_threshold(w);

    if (pm->test_gamma_transform)
    {
        init_gamma_errors(pm);
        if (pm->test_gamma_expand16)
            pm->calculations_use_input_precision = 1;
        pv_walk_gamma_transform(w);
        if (!calculations_use_input_precision)
            pm->calculations_use_input_precision = 0;
    }

    if (pm->test_gamma_sbit)
    {
        init_gamma_errors(pm);
        pv_walk_gamma_sbit(w);
    }

#ifdef DO_16BIT
    if (pm->test_gamma_scale16)
    {
        init_gamma_errors(pm);
        pv_walk_gamma_scale16(w);
    }
#endif

#ifdef PNG_READ_BACKGROUND_SUPPORTED
    if (pm->test_gamma_background)
    {
        init_gamma_errors(pm);
        if (pm->test_gamma_expand16)
        {
            pm->calculations_use_input_precision = 1;
            pm->maxout8 = .499; /* because the 16 bit background is smashed */
        }
        pv_walk_gamma_composition(w, PNG_BACKGROUND_GAMMA_UNIQUE,
            pm->test_gamma_expand16, "background");
        if (!calculations_use_input_precision)
            pm->calculations_use_input_precision = 0;
        pm->maxout8 = maxout8;
    }
#endif

#ifdef PNG_READ_ALPHA_MODE_SUPPORTED
    if (pm->test_gamma_alpha_mode)
    {
        int do_background;

        init_gamma_errors(pm);
        if (pm->test_gamma_expand16)
            pm->calculations_use_input_precision = 1;
        for (do_background = ALPHA_MODE_OFFSET + PNG_ALPHA_STANDARD;
             do_background <= ALPHA_MODE_OFFSET + PNG_ALPHA_BROKEN;
             ++do_background)
            pv_walk_gamma_composition(w, do_background,
                pm->test_gamma_expand16, "alphamode");
        if (!calculations_use_input_precision)
            pm->calculations_use_input_precision = 0;
    }
#endif
}

/* ------------------------------------------------------------------------
 * Entry points
 */

const char *harvest_pngvalid_group_suite(int group)
{
    switch (group)
    {
        case PV_GROUP_STANDARD:  return "LibpngStandard";
        case PV_GROUP_SIZE:      return "LibpngSize";
        case PV_GROUP_TRANSFORM: return "LibpngTransform";
        case PV_GROUP_GAMMA:     return "LibpngGamma";
        default:                 return "LibpngUnknown";
    }
}

int harvest_pngvalid_walk(int group, int execute)
{
    png_modifier pm;
    pv_walk w;

    if (group < 0 || group >= PV_NGROUPS)
        return 1;

    pv_setup(&pm, group);

    w.execute = execute;
    w.index = 0;
    w.pm = &pm;

    /* Enumeration must not run any libpng code: it only walks the loops to
     * report names, so it skips the image generation below (which is what
     * seeds pngvalid's pseudo-random stream). Enumeration also runs in its own
     * forked child, so the recording child still starts from the state a plain
     * pngvalid run has. */
    if (execute)
    {
        /* main() builds the transform images before any group runs, and the
         * size images additionally before the size group. */
        make_transform_images(&pm);
        if (group == PV_GROUP_SIZE)
            make_size_images(&pm.this);
    }

    switch (group)
    {
        case PV_GROUP_STANDARD:  pv_walk_standard(&w);  break;
        case PV_GROUP_SIZE:      pv_walk_size(&w);      break;
        case PV_GROUP_TRANSFORM: pv_walk_transform(&w); break;
        case PV_GROUP_GAMMA:     pv_walk_gamma(&w);     break;
        default: break;
    }

    if (execute)
        store_delete(&pm.this);

    /* Report the scenario count last, so the parent can tell a stale
     * scenario_list.h from a run that simply died early. */
    pv_emit_count(w.index);

    return 0;
}
