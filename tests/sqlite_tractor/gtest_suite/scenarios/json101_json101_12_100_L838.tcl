source $::suite_root/glue/tester.tcl
do_execsql_test json101-12.100 {
  CREATE TABLE t12(x);
  INSERT INTO t12(x) VALUES(
    '{"settings":
        {"layer2":
           {"hapax.legomenon":
              {"forceDisplay":true,
               "transliterate":true,
               "add.footnote":true,
               "summary.report":true},
            "dis.legomenon":
              {"forceDisplay":true,
               "transliterate":false,
               "add.footnote":false,
               "summary.report":true},
            "tris.legomenon":
              {"forceDisplay":true,
               "transliterate":false,
               "add.footnote":false,
               "summary.report":false}
           }
        }
     }');
} {}
finish_test
