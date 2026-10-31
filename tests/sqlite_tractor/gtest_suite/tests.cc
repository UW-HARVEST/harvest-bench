#include <gtest/gtest.h>
#include <dlfcn.h>
#include <clocale>
#include <ctime>
#include "glue/runtime.h"
struct Scenario {const char *id;const char *path;const char *trace;bool fixed_rowids;};
#include "scenario_table.h"
#include "public_abi.h"
static std::string missing_abi;
class SqliteTractor : public testing::TestWithParam<Scenario> {};
TEST_P(SqliteTractor, Replay) {
  ASSERT_TRUE(missing_abi.empty()) << missing_abi;
  const auto &s=GetParam();
  SCOPED_TRACE(s.path);
  try {
    Directory directory;
    Interpreter interpreter;
    auto *i=interpreter.p;
    std::string source=std::string(SUITE_ROOT)+"/"+s.path;
    Tcl_SetVar(i,"argv0",source.c_str(),TCL_GLOBAL_ONLY);
    Tcl_SetVar(i,"testdir",SUITE_ROOT "/upstream/test",TCL_GLOBAL_ONLY);
    if(s.fixed_rowids)interpreter.eval("namespace eval ::harvest_inputs {variable enabled 1; variable state 0x9e3779b97f4a7c15}");
    interpreter.source(source);
    interpreter.eval(R"tcl(if {[llength $missing_commands]} {error "missing test commands: $missing_commands"}; if {!$finished || $TC(errors) || !$TC(count)} {error {incomplete upstream scenario}})tcl");
    RecordProperty("sources",Tcl_GetVar(i,"sourced_files",TCL_GLOBAL_ONLY));
    std::string actual=Tcl_GetVar(i,"assertion_names",TCL_GLOBAL_ONLY);
    // Compare lists, not their optional Tcl brace/whitespace representations.
    interpreter.source(std::string(SUITE_ROOT)+"/"+s.trace);
    Tcl_SetVar(i,"actual_assertions",actual.c_str(),TCL_GLOBAL_ONLY);
    interpreter.eval("if {[llength $actual_assertions] != [llength $expected_assertions]} {error {upstream assertion count changed}}; apply {{actual expected} {foreach a $actual e $expected {if {$a ne $e} {error {upstream assertion trace changed}}}}} $actual_assertions $expected_assertions");
  } catch(const std::exception &e) { FAIL()<<e.what(); }
}
INSTANTIATE_TEST_SUITE_P(,SqliteTractor,testing::ValuesIn(kScenarios),
 [](const testing::TestParamInfo<Scenario> &info){return info.param.id;});
int main(int argc,char **argv){
  setenv("TZ","UTC",1);setenv("LC_ALL","C",1);tzset();std::setlocale(LC_ALL,"C");
  Tcl_FindExecutable(argv[0]);
  testing::InitGoogleTest(&argc,argv);
  void *candidate=dlopen(CANDIDATE_PATH,RTLD_NOW|RTLD_LOCAL);
  if(!candidate)missing_abi=dlerror();
  else for(const char *symbol:kPublicAbi){
    if(!dlsym(candidate,symbol))missing_abi+=std::string("missing public ABI: ")+symbol+"\n";
  }
  int result=RUN_ALL_TESTS();
  if(candidate)dlclose(candidate);
  Tcl_Finalize();
  return result;
}
