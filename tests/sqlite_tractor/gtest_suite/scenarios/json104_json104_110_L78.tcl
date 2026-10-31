source $::suite_root/glue/tester.tcl
do_execsql_test json104-110 {
  SELECT json_patch('{
       "title": "Goodbye!",
       "author" : {
         "givenName" : "John",
         "familyName" : "Doe"
       },
       "tags":[ "example", "sample" ],
       "content": "This will be unchanged"
     }','{
       "title": "Hello!",
       "phoneNumber": "+01-123-456-7890",
       "author": {
         "familyName": null
       },
       "tags": [ "example" ]
     }');
} {{{"title":"Hello!","author":{"givenName":"John"},"tags":["example"],"content":"This will be unchanged","phoneNumber":"+01-123-456-7890"}}}
finish_test
