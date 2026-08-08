/* Generator for glue/scenario_list.h — the checked-in list of pngvalid
 * scenario names the suite registers as GoogleTest cases.
 *
 * Run it whenever the upstream pngvalid.c or the walk in glue_pngvalid.c
 * changes the set of scenarios (the suite refuses to run on a stale list, so
 * this is never a silent requirement):
 *
 *     cmake -S gtest_suite -B build/gtest -DCMAKE_BUILD_TYPE=Release \
 *           -DTEST_LIB_PATH=$PWD/build/c/libpng.so
 *     cmake --build build/gtest --target harvest_scenario_list
 *     ./build/gtest/harvest_scenario_list > gtest_suite/glue/scenario_list.h
 *
 * The generator walks each group in ENUMERATE mode, which runs no libpng code
 * at all — it only follows the loop bounds and builds the names. It therefore
 * does not matter which library the generator happens to be linked against.
 *
 * The generator emits the header directly, so regeneration is one command with
 * no script in between.
 */
#include <stdio.h>

#include "pv_record.h"

/* The walk reports through these. In enumerate mode only pv_emit_name fires;
 * the other two exist because the walk is compiled once for both tools. */

static int g_count;

void pv_emit_name(int index, const char *name)
{
    (void)index;
    printf("    \"%s\",\n", name);
    ++g_count;
}

void pv_emit_result(int index, int failed, const char *message)
{
    (void)index;
    (void)failed;
    (void)message;
}

void pv_emit_count(int count)
{
    (void)count;
}

int main(void)
{
    int counts[PV_NGROUPS];
    int group;

    printf(
        "/* GENERATED FILE - DO NOT EDIT.\n"
        " *\n"
        " * The pngvalid scenarios this suite exposes as GoogleTest cases, in the\n"
        " * order the walk in glue_pngvalid.c visits them. Regenerate with\n"
        " * glue/dump_scenarios.c (see the comment at the top of that file).\n"
        " *\n"
        " * The list is checked in rather than enumerated at startup so that\n"
        " * registering the tests cannot depend on anything succeeding at run\n"
        " * time. A library under test that is broken enough to break enumeration\n"
        " * would otherwise make the suite report zero tests, which the harness\n"
        " * reads as broken infrastructure rather than as a score of zero. With a\n"
        " * static list every scenario is always registered, and a broken library\n"
        " * simply fails all of them.\n"
        " *\n"
        " * A stale list is caught, not tolerated: the recording run reports how\n"
        " * many scenarios it visited and the tests fail loudly if that disagrees\n"
        " * with the counts below.\n"
        " */\n"
        "#ifndef HARVEST_PV_SCENARIO_LIST_H\n"
        "#define HARVEST_PV_SCENARIO_LIST_H\n");

    for (group = 0; group < PV_NGROUPS; ++group)
    {
        printf("\nstatic const char *const pv_scenarios_%s[] = {\n",
            harvest_pngvalid_group_suite(group));
        g_count = 0;
        harvest_pngvalid_walk(group, 0 /*enumerate*/);
        counts[group] = g_count;
        printf("};\n");
    }

    printf("\nstatic const char *const *const pv_scenario_names[PV_NGROUPS] = {\n");
    for (group = 0; group < PV_NGROUPS; ++group)
        printf("    pv_scenarios_%s,\n", harvest_pngvalid_group_suite(group));
    printf("};\n");

    printf("\nstatic const int pv_scenario_counts[PV_NGROUPS] = {\n");
    for (group = 0; group < PV_NGROUPS; ++group)
        printf("    %d, /* %s */\n", counts[group],
            harvest_pngvalid_group_suite(group));
    printf("};\n");

    printf("\n#endif /* HARVEST_PV_SCENARIO_LIST_H */\n");
    return 0;
}
