use harvest_bench::{ExecutionPlan,PlannedInvocation,InvocationMode,execute_plan};
fn main(){
 let args:Vec<String>=std::env::args().collect();
 let names=["SqliteTractor.Replay/cast_cast_10_2_L521","SqliteTractor.Replay/cast_cast_10_3_L524"];
 let invocations=names.iter().map(|name|PlannedInvocation{mode:InvocationMode::Case,filter:name.to_string(),tests:vec![name.to_string()],timeout_secs:1,note:None}).collect();
 let result=execute_plan(std::path::Path::new(&args[1]),&args[2],ExecutionPlan{invocations}, |_|{});
 println!("{}",serde_json::to_string_pretty(&result).unwrap());
}
