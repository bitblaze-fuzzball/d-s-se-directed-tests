let main argv =
  let prog_info = Cfgs.program_info_from_file argv.(1) in
  let src_addr = Int64.of_string argv.(2) and
      dst_addr = Int64.of_string argv.(3) in
  if (Array.length argv = 5) then (
    Cfgs.warnings_from_file argv.(4) prog_info;
    Cfgs.compute_influence_for_warning prog_info dst_addr
  );
  let ipcfg = Cfgs.construct_interproc_cfg prog_info in
    Cfgs.interproc_cfg_set_target_addr ipcfg dst_addr prog_info;
    Cfgs.interproc_cfg_compute_shortest_paths ipcfg;
    let dist = Cfgs.interproc_cfg_lookup_distance ipcfg src_addr prog_info in
      Printf.printf "Distance from 0x%08Lx to 0x%08Lx is %Ld\n"
	src_addr dst_addr dist;
      Cfgs.free_interproc_cfg ipcfg;   
      Cfgs.free_program_info prog_info
;;

main Sys.argv
