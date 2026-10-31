#pragma once
#include <tcl.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <cstdlib>
#include <unistd.h>
extern "C" int Sqlite3_Init(Tcl_Interp*);
extern "C" int Bridge_Init(Tcl_Interp*);
extern "C" int TestSession_Init(Tcl_Interp*);
extern "C" int TestRecover_Init(Tcl_Interp*);
extern "C" int Compare_Init(Tcl_Interp*);
extern "C" int harvest_environment(void);
extern "C" void harvest_environment_end(void);
// Declare the directory before the interpreter, so Tcl closes files first.
struct Directory {
  std::filesystem::path saved=std::filesystem::current_path(), path;
  Directory(){
    char pattern[]="/tmp/harvest-sqlite-XXXXXX";
    char *p=mkdtemp(pattern);
    if(!p)throw std::runtime_error("mkdtemp failed");
    path=p;std::filesystem::current_path(path);
  }
  ~Directory(){std::filesystem::current_path(saved);std::filesystem::remove_all(path);}
};
struct Interpreter {
  Tcl_Interp *p=Tcl_CreateInterp();
  Interpreter(){
    if(Tcl_Init(p)!=TCL_OK || Sqlite3_Init(p)!=TCL_OK || Compare_Init(p)!=TCL_OK || Bridge_Init(p)!=TCL_OK || TestSession_Init(p)!=TCL_OK || TestRecover_Init(p)!=TCL_OK)
      throw std::runtime_error(Tcl_GetStringResult(p));
    if(harvest_environment()!=0)throw std::runtime_error("deterministic VFS initialization failed");
    if(Tcl_SetSystemEncoding(p,"utf-8")!=TCL_OK)throw std::runtime_error("UTF-8 initialization failed");
    Tcl_SetVar(p,"suite_root",SUITE_ROOT,TCL_GLOBAL_ONLY);
    eval("set argv {}; set assertion_names {}; set finished 0; set sourced_files {}; trace add execution source enter {apply {{cmd op} {lappend ::sourced_files [file normalize [lindex $cmd end]]}}}; expr srand(1234)");
  }
  void eval(const char *s){if(Tcl_Eval(p,s)!=TCL_OK)throw std::runtime_error(Tcl_GetVar(p,"errorInfo",TCL_GLOBAL_ONLY));}
  void source(const std::string &path){if(Tcl_EvalFile(p,path.c_str())!=TCL_OK){const char*e=Tcl_GetVar(p,"errorInfo",TCL_GLOBAL_ONLY);throw std::runtime_error(e?e:Tcl_GetStringResult(p));}}
  ~Interpreter(){Tcl_DeleteInterp(p);harvest_environment_end();}
};
