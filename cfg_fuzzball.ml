module FM = Fragment_machine

module SRFM = Sym_region_frag_machine.SymRegionFragMachineFunctor
  (Symbolic_domain.SymbolicDomain)
module SRFMT = Sym_region_frag_machine.SymRegionFragMachineFunctor
  (Tagged_domain.TaggedDomainFunctor(Symbolic_domain.SymbolicDomain))

module SPFM = Sym_path_frag_machine.SymPathFragMachineFunctor
  (Symbolic_domain.SymbolicDomain)
module SPFMT = Sym_path_frag_machine.SymPathFragMachineFunctor
  (Tagged_domain.TaggedDomainFunctor(Symbolic_domain.SymbolicDomain))

module IM = Exec_influence.InfluenceManagerFunctor
  (Symbolic_domain.SymbolicDomain)
module IMT = Exec_influence.InfluenceManagerFunctor
  (Tagged_domain.TaggedDomainFunctor(Symbolic_domain.SymbolicDomain))

module BDT = Binary_decision_tree

let prog_info_or = ref None
let ipcfg_or = ref None

let fm_or = ref None
let dt_or = ref None

let load_cfg fname dst_addr =
  let prog_info = Cfgs.program_info_from_file fname in
  let ipcfg = Cfgs.construct_interproc_cfg prog_info in
    Cfgs.interproc_cfg_set_target_addr ipcfg dst_addr prog_info;
    Cfgs.interproc_cfg_compute_shortest_paths ipcfg;
    prog_info_or := Some prog_info;
    ipcfg_or := Some ipcfg

let opt_trace_cjmp_heuristic = ref false

let opt_which_branch_heur = ref 0

let cjmp_heuristic_distance prog_info ipcfg eip targ1 targ2 r =
  let lookup_dist targ = 
    Cfgs.interproc_cfg_lookup_distance ipcfg targ prog_info
  in
  let lookup_infl targ = 
    Cfgs.interproc_cfg_lookup_influence ipcfg targ prog_info
  in
    if !opt_trace_cjmp_heuristic then
      Printf.printf "At 0x%08Lx, choosing between 0x%08Lx and 0x%08Lx\n%!"
	eip targ1 targ2;
    let dist1 = lookup_dist targ1 and
	dist2 = lookup_dist targ2 and
	infl1 = lookup_infl targ1 and
	infl2 = lookup_infl targ2
    in
    let fdist1 = Int64.to_float dist1 and
	fdist2 = Int64.to_float dist2 and
	finfl1 = float_of_int infl1 and
	finfl2 = float_of_int infl2
    in
    let xprod = fdist1 *. (1.0 +. log (1.0 +. finfl2))
      -. fdist2 *. (1.0 +. log (1.0 +. finfl1)) in
    let weight =
      match !opt_which_branch_heur with
	| 0 -> xprod
	| 1 -> fdist2 -. fdist1
	| _ -> failwith "Unrecognized -which-branch-heur"
    in
    let prob1 = 1.0 /. (1.0 +. 1.001 ** weight) in
      if !opt_trace_cjmp_heuristic then
	Printf.printf "Distances are %Ld to 0x%08Lx and %Ld to 0x%08Lx\n"
	  dist1 targ1 dist2 targ2;
      if !opt_trace_cjmp_heuristic then
	Printf.printf "Influences are %d on 0x%08Lx and %d on 0x%08Lx\n"
	  infl1 targ1 infl2 targ2;
      (match (dist1, dist2) with
	 | (-1L, -1L) ->
	     if !opt_trace_cjmp_heuristic then
	       Printf.printf "Neither reaches target\n";
	     None
	 | (-1L, _) ->
	     if !opt_trace_cjmp_heuristic then
	       Printf.printf "Only second reaches target\n";
	     Some false
	 | (_, -1L) ->
	     if !opt_trace_cjmp_heuristic then
	       Printf.printf "Only first reaches target\n";
	     Some true
	 | _ ->
	     if !opt_trace_cjmp_heuristic then
	       Printf.printf "Probability for first is %f\n" prob1;
	     if r < prob1 then
	       (if !opt_trace_cjmp_heuristic then
		  Printf.printf "Choosing first\n";
		Some true)
	     else
	       (if !opt_trace_cjmp_heuristic then
		  Printf.printf "Choosing second\n";
		Some false))

let opt_loop_weights = Hashtbl.create 11
let opt_loop_rank_exprs = Hashtbl.create 11

let num_iters prog_info ipcfg loop_cond =
  if Cfgs.interproc_cfg_is_interesting_loop ipcfg loop_cond prog_info then
    (if !opt_trace_cjmp_heuristic then
       Printf.printf "This loop is interesting for the warning.\n";
     100.0)
  else
    try
      Hashtbl.find opt_loop_weights loop_cond
    with
      | Not_found -> 2.0

let cjmp_heuristic_loop prog_info ipcfg eip targ1 targ2 r =
  let (exit_dir, exit_targ, cont_targ) = if
    Cfgs.interproc_cfg_edge_is_loop_exit ipcfg eip targ1 prog_info
  then
    (true, targ1, targ2)
  else
    (false, targ2, targ1)
  in
    if !opt_trace_cjmp_heuristic then
      Printf.printf ("Loop at 0x%08Lx, choices 0x%08Lx (exit)"
		     ^^" vs. 0x%08Lx (continue)\n")
	eip exit_targ cont_targ;
    let do_exit = (r < 1.0 /. (num_iters prog_info ipcfg eip)) in
      if do_exit then
	(if !opt_trace_cjmp_heuristic then
	   Printf.printf "Choosing to exit\n";
	 Some exit_dir)
      else
	(if !opt_trace_cjmp_heuristic then
	   Printf.printf "Choosing to continue\n";
	 Some (not exit_dir))

let opt_trace_rank = ref false
let opt_rank_base = ref 1

let init_rank_or = ref None

let states_pq = Array.init 100000 (fun _ -> [])

let highest_rank_seen = ref (-1)

let cjmp_heuristic_rank fm dt prog_info ipcfg eip targ1 targ2 exp r =
  let rank_fn = fm#eval_expr_to_int64 exp in
  let init_rank = match !init_rank_or with
    | Some r -> r
    | None -> init_rank_or := Some rank_fn; rank_fn
  in
  let cur_rank =
    1000 * (!opt_rank_base + Int64.to_int (Int64.sub rank_fn init_rank))
    - dt#get_depth
  in
  let cur_id = dt#cur_ident in
  let rec reachable_search_loop l =
    match l with
      | [] -> None
      | i :: rest ->
	  match dt#cur_can_reach_ident i with
	    | Some b -> 
		if !opt_trace_rank then
		  Printf.printf "Can reach %d by going %b\n" i b;
		Some b
	    | None -> reachable_search_loop rest
  in
  let rec rank_search_loop i =
    if i < 0 || i < cur_rank then
      None
    else 
      let l = states_pq.(i) in
      let l' = List.filter dt#is_live_ident l in
	states_pq.(i) <- l';
	match reachable_search_loop l' with
	  | Some b ->
	      if !opt_trace_rank then
		Printf.printf "Can reach state with rank %d by going %b\n" i b;
	      Some b
	  | None -> rank_search_loop (i - 1)
  in
    (if !opt_trace_rank then
       Printf.printf "Branch at 0x%08Lx, node %d, current rank is %d\n"
	 eip cur_id cur_rank);
    if cur_rank < 0 then
      raise Exec_exceptions.UnproductivePath (* None *)
    else
      (if cur_rank > !highest_rank_seen then
	 (if !opt_trace_rank then 
	    Printf.printf "New highest rank: %d\n" cur_rank;
	  highest_rank_seen := cur_rank);
       if not (List.mem cur_id states_pq.(cur_rank)) then
	 states_pq.(cur_rank) <- cur_id :: states_pq.(cur_rank);
       if r < 0.0 then
	 None
       else
	 rank_search_loop !highest_rank_seen)

type loop_pattern_info = {
  header : int64;
  feasibility_counts : (int64, int) Hashtbl.t;
  mutable cur_path_choice : int64 option;
  mutable followed_path : int64;
  mutable cur_pattern : int64 list option;
  mutable pattern_iteration : int}

let new_loop_pattern_info haddr =
  { header = haddr;
    feasibility_counts = Hashtbl.create 1001;
    cur_path_choice = None;
    followed_path = 0L;
    cur_pattern = None;
    pattern_iteration = 0;
  }

let opt_loop_pattern = Hashtbl.create 11

let opt_trace_pattern = ref false
let opt_trace_pattern_detailed = ref false

let load_cfg_warn cfg_fname warn_fname warn_addr targ_addr =
  let prog_info = Cfgs.program_info_from_file cfg_fname in
  let ipcfg = Cfgs.construct_interproc_cfg prog_info in
    Cfgs.interproc_cfg_set_target_warning ipcfg targ_addr prog_info
      warn_fname;
    Cfgs.compute_influence_for_warning prog_info targ_addr;
    Cfgs.interproc_cfg_compute_shortest_paths ipcfg;
    prog_info_or := Some prog_info;
    ipcfg_or := Some ipcfg;
    let header = Cfgs.interproc_cfg_get_component ipcfg warn_addr prog_info in
      Hashtbl.replace opt_loop_pattern header (new_loop_pattern_info header)

let pattern_new_choice lpi pth0 header r =
  let counts = lpi.feasibility_counts in
  let uniform r =
    let c = Int64.of_float (r *. (Int64.to_float pth0)) in
      if !opt_trace_pattern_detailed then 
	Printf.printf "Arbitrary path: %Ld\n" c;
      c
  in
  let num_seen = Hashtbl.length counts in
  let seen r =
    let n = int_of_float (r *. (float_of_int num_seen)) in
    let i = ref 0 in
    let the_path = ref (-1L) in
      if !opt_trace_pattern_detailed then
	Printf.printf "Seen path set: {";
      Hashtbl.iter
	(fun k v ->
	   if !i = n then
	     the_path := k;
	   if !opt_trace_pattern_detailed then
	     Printf.printf "%s%Ld" (if !i = 0 then "" else " ") k;
	   incr i) counts;
      if !opt_trace_pattern_detailed then
	Printf.printf "}\n";
      let c = !the_path in
 	if !opt_trace_pattern_detailed then 
	  Printf.printf "Picking seen path %Ld\n" c;
	c
  in
  let no_pattern r = 
    if num_seen = 0 then
      uniform r
    else if r < 0.5 then
      seen (2.0 *. r)
    else
      uniform (2.0 *. (r -. 0.5))
  in
  let split_random r =
    let s = r *. 1000000.0 in
    let (r2, _) = modf s in
      (r, r2)
  in
  let new_pattern i r =
    lpi.pattern_iteration <- lpi.pattern_iteration + 1;
    let p =
      if i < 5 then
	[]
      else
	let ir = ref i in
	let pr = ref [] in
	let rr = ref r in
	  while !ir land 1 = 0 do
	    let (r1, r2) = split_random !rr in
	      pr := (seen r1) :: !pr;
	      ir := !ir asr 1;
	      rr := r2
	  done;
	  !pr
    in
      if !opt_trace_pattern then
	(Printf.printf "On loop 0x%08Lx iter %d, using pattern [%s]\n"
	   header i (String.concat " " (List.map Int64.to_string p));
	 Printf.printf "Seen paths are {%s} (%d total)\n"
	   (String.concat " "
	      (List.map Int64.to_string
		 (Hashtbl.fold (fun k _ l -> k :: l) counts [])))
	   (Hashtbl.length counts)
	);
      lpi.cur_pattern <- Some p;
      p
  in
  let (pattern, r') = match lpi.cur_pattern with
    | Some p -> (p, r)
    | None ->
	let (r1, r2) = split_random r in
	  ((new_pattern lpi.pattern_iteration r2), r1)
  in
  let c =
    match pattern with
      | [] -> no_pattern r'
      | c :: rest ->
	  lpi.cur_pattern <- Some (rest @ [c]);
	  c
  in
  let c' = c (* Int64.rem c pth0 *) in
    if !opt_trace_pattern_detailed then
      Printf.printf "Starting with choice %Ld\n" c;
    lpi.cur_path_choice <- Some c';
    lpi.followed_path <- 0L;
    c'

let last_pth0 = ref 0L

let cjmp_heuristic_pattern fm dt prog_info ipcfg header eip targ1 targ2 r dir =
  let lpi = try Hashtbl.find opt_loop_pattern header
  with Not_found ->
    let lpi = new_loop_pattern_info header in
      Hashtbl.replace opt_loop_pattern header lpi;
      lpi
  in
  let counts = lpi.feasibility_counts in
  let bool_option_to_string = function 
    | None -> "none"
    | Some true -> "true"
    | Some false -> "false"
  in
  let lookup_here eip =
    Cfgs.interproc_cfg_lookup_pth ipcfg eip prog_info
  in
  let lookup_or_outside targ =
    if Cfgs.interproc_cfg_get_component ipcfg targ prog_info = header then
      (if Cfgs.interproc_cfg_same_bb ipcfg targ header prog_info then
	 1L
       else
	 lookup_here targ)
    else
      1L
  in
  let pth0 = lookup_here eip and
      pth1 = lookup_or_outside targ1 and
      pth2 = lookup_or_outside targ2 in
  let make_choice c =
    Some (c < pth1)
  in
  let update_choice the_choice c =
    assert(pth0 > 0L);
    assert(c >= 0L);
    (* Later, debug why this assertion is failing:
       assert(c < pth0); *)
    let c = Int64.rem c pth0 in
    let our_choice = (c < pth1) in
      if the_choice = false then
	lpi.followed_path <- Int64.add lpi.followed_path pth1;
      let c' =
	if our_choice = the_choice then
	  (let c' = if the_choice then c else Int64.sub c pth1 in
	     if !opt_trace_pattern_detailed then 
	       Printf.printf "Choice %Ld -> [%b] -> %Ld\n" c the_choice c';
	     c')
	else
	  (if !opt_trace_pattern_detailed then 
	     Printf.printf "Our choice %b was unsat.\n" our_choice;
	   let c_not = Int64.add (if the_choice then 0L else pth1)
	     (Int64.rem c (if the_choice then pth1 else pth2))
	   in
	     if !opt_trace_pattern_detailed then 
	       Printf.printf "Flipping choice from %Ld to %Ld\n" c c_not;
	     assert((c_not < pth1) = the_choice);
	     let c' = if the_choice then c_not else Int64.sub c_not pth1 in
	       if !opt_trace_pattern_detailed then 
		 Printf.printf "Choice %Ld -> [%b] -> %Ld\n"
		   c_not the_choice c';
	       c')
      in
      let finished = (if the_choice then pth1 else pth2) = 1L in
	lpi.cur_path_choice <- if finished then None else Some c';
	if finished then
	  (if !opt_trace_pattern then 
	     Printf.printf "Finished on path %Ld\n" lpi.followed_path;
	   try
	     Hashtbl.replace counts lpi.followed_path
	       (1 + (Hashtbl.find counts lpi.followed_path))
	   with
	     | Not_found ->
		 Hashtbl.replace counts lpi.followed_path 1)
  in
    if !opt_trace_pattern_detailed then 
      Printf.printf "At 0x%08Lx(%Ld), 0x%08Lx(%Ld) versus 0x%08Lx(%Ld) %s\n"
	eip pth0 targ1 pth1 targ2 pth2 (bool_option_to_string dir);
    if pth0 <> Int64.add pth1 pth2 then
      if !opt_trace_pattern_detailed then 
	Printf.printf "PTH sum invariant violation.\n";
    let c = 
      if pth0 >= !last_pth0 then
	(last_pth0 := Int64.succ pth0;
	 pattern_new_choice lpi pth0 header r)
      else
	(match lpi.cur_path_choice with
	   | Some c -> c
	   | None ->
	       pattern_new_choice lpi pth0 header r
	       (* failwith "Missing choice" *))
    in
      lpi.cur_path_choice <- Some c;
      if dir <> None then
	last_pth0 := pth0;
      match dir with
	| None -> make_choice c
	| Some b -> update_choice b c; None

let opt_fixed_pattern = ref None

let total_iter_count = ref 1

let reset_heuristics () =
  Hashtbl.iter
    (fun k lpi -> lpi.cur_pattern <- !opt_fixed_pattern)
    opt_loop_pattern;
  incr total_iter_count;
  ()

let apply_pattern ipcfg header prog_info =
  let b =
    (Hashtbl.mem opt_loop_pattern header) ||
      (!total_iter_count mod 3 = 0) && (header <> 0L) &&
      Cfgs.interproc_cfg_is_interesting_loop ipcfg header prog_info
  in
    if !opt_trace_pattern_detailed then
      Printf.printf "On iter %d with header 0x%08Lx, apply_pattern is %b\n"
	!total_iter_count header b;
    b

let cjmp_heuristic fm dt prog_info ipcfg eip targ1 targ2 r dir =
  let header = Cfgs.interproc_cfg_get_component ipcfg eip prog_info in
    if !opt_trace_rank then
      Printf.printf "At 0x%08Lx, header is 0x%08Lx\n" eip header;
    if dir = None && Hashtbl.mem opt_loop_rank_exprs header then
      match (let (_, eor) = Hashtbl.find opt_loop_rank_exprs header
	     in !eor) with
	| Some exp ->
	    cjmp_heuristic_rank fm dt prog_info ipcfg eip targ1 targ2 exp r
	| _ -> failwith "Missing expression in opt_loop_rank_exprs"
    else if apply_pattern ipcfg header prog_info then
      cjmp_heuristic_pattern fm dt prog_info ipcfg header eip targ1 targ2 r dir
    else if false && dir = None && 
      Cfgs.interproc_cfg_is_loop_exit_cond ipcfg eip prog_info
    then
      cjmp_heuristic_loop prog_info ipcfg eip targ1 targ2 r
    else if dir = None then
      cjmp_heuristic_distance prog_info ipcfg eip targ1 targ2 r
    else
      None

let cjmp_heuristic_wrapper eip targ1 targ2 r dir =
  match (!prog_info_or, !ipcfg_or, !fm_or, !dt_or) with
    | (Some prog_info, Some ipcfg, Some fm, Some dt) ->
	cjmp_heuristic fm dt prog_info ipcfg eip targ1 targ2 r dir
    | _ ->
	None

let opt_cfg_fname = ref None
let opt_warn_fname = ref None

(* When the warning is in a library function, warn_addr is the address
   in the library, and target_addr is the one in the main program the
   warning is attributed to. *)
let opt_warn_addr = ref None
let opt_target_addr = ref None

let main argv = 
  Arg.parse
    (Arg.align
       (Exec_set_options.cmdline_opts
	@ Options_linux.linux_cmdline_opts
	@ State_loader.state_loader_cmdline_opts
	@ Exec_set_options.concrete_state_cmdline_opts
	@ Exec_set_options.symbolic_state_cmdline_opts	
	@ Exec_set_options.concolic_state_cmdline_opts	
	@ Exec_set_options.explore_cmdline_opts
	@ Exec_set_options.tags_cmdline_opts
	@ Exec_set_options.fuzzball_cmdline_opts
	@ Options_solver.solver_cmdline_opts
	@ Exec_set_options.influence_cmdline_opts
	@ [
	  ("-trace-cjmp-heuristic", Arg.Set(opt_trace_cjmp_heuristic),
	   " Trace how decisions are made at branches");
	  ("-trace-rank", Arg.Set(opt_trace_rank),
	   " Trace ranking-function guidance");
	  ("-trace-pattern", Arg.Set(opt_trace_pattern),
	   " Trace body-pattern guidance");
	  ("-trace-pattern-detailed",
	   Arg.Set(opt_trace_pattern_detailed),
	   " Trace more about body-pattern guidance");
	  ("-cfg", Arg.String
	     (fun s -> opt_cfg_fname := Some s),
	   "filename Load specified serialized CFG");
	  ("-target-addr", Arg.String
	     (fun s -> opt_target_addr := Some (Int64.of_string s)),
	   "addr Address in program you would like to cover");
	  ("-loop-weight", Arg.String
	     (fun s -> 
		let (s1, s2) = Exec_options.split_string '=' s in
		  Hashtbl.replace opt_loop_weights
		    (Int64.of_string s1) (float_of_string s2)),
	   "condpc=weight Target loop to execute around WEIGHT times");
	  ("-loop-rank", Arg.String
	     (fun s ->
		let (s1, s2) = Exec_options.split_string ':' s in
		  Hashtbl.replace opt_loop_rank_exprs
		    (Int64.of_string s1) (s2, ref None)),
	   "headpc:rankexpr Try to increase an expr. inside loop");
	  ("-loop-pattern", Arg.String
	     (fun s ->
		let addr = Int64.of_string s in
		  Hashtbl.replace opt_loop_pattern addr
		    (new_loop_pattern_info addr)),
	   "headpc Try body patterns for loop");
	  ("-fixed-pattern", Arg.String
	     (fun s ->
		let (s1, s2) = Exec_options.split_string ',' s in
		  opt_fixed_pattern :=
		    Some [(Int64.of_string s1); (Int64.of_string s2)]),
	   "p1,p2 Use the given pattern with -loop-pattern");
	  ("-rank-base", Arg.Set_int(opt_rank_base),
	   "N Allow rank to decrease by N before stopping path");
 	  ("-warn-addr", Arg.String
 	     (fun s -> opt_warn_addr := Some (Int64.of_string s)),
 	   "addr Warning address you would like to cover");
	  ("-warn-file", Arg.String
	     (fun s -> opt_warn_fname := Some s),
	   "filename Load static warnings from file");
	  ("-which-branch-heur", Arg.Set_int(opt_which_branch_heur),
	   "i i=0: xprod+log; i=1: distance difference");
	]))
    (fun arg -> Exec_set_options.set_program_name_guess_arch arg)
    "cfg_fuzzball [options]* program\n";
  let bdt = new BDT.binary_decision_tree in
  let dt = (bdt :> Decision_tree.decision_tree)#init in
  let (fm, infl_man) = 
    (let srfm = new SRFM.sym_region_frag_machine dt in
     let spfm = (srfm :> SPFM.sym_path_frag_machine) in
     let im = new IM.influence_manager spfm in
       srfm#set_influence_manager im;
       ((srfm :> FM.fragment_machine),
	(im :> Exec_no_influence.influence_manager)))
  in
  let dl = Asmir.decls_for_arch Asmir.arch_i386 in
  let asmir_gamma = Asmir.gamma_create 
    (List.find (fun (i, s, t) -> s = "mem") dl) dl
  in
    (match (!opt_cfg_fname, !opt_warn_fname, !opt_target_addr,
	    !opt_warn_addr) with
       | (None, None, None, None) -> ()
       | (Some fname, None, Some addr, None) -> load_cfg fname addr
       | (Some fname, Some warn_fname, Some taddr, Some waddr)
	 -> load_cfg_warn fname warn_fname waddr taddr
       | _ ->
	   failwith
	     "Bad combination of -cfg, -warn-file, -warn-addr, -target-addr");
    fm#init_prog (dl, []);
    fm#set_cjmp_heuristic cjmp_heuristic_wrapper;
    Exec_set_options.default_on_missing := (fun fm -> fm#on_missing_symbol);
    Exec_set_options.apply_cmdline_opts_early fm dl;
    Options_linux.apply_linux_cmdline_opts fm;
    Options_solver.apply_solver_cmdline_opts fm;
    State_loader.apply_state_loader_cmdline_opts fm;
    Exec_set_options.apply_cmdline_opts_late fm;
    Hashtbl.iter
      (fun _ (exp_str, er) ->
	 er := Some (Vine_parser.parse_exp_from_string dl exp_str))
      opt_loop_rank_exprs;
    fm_or := Some fm;
    dt_or := Some bdt;
    let symbolic_init = Exec_set_options.make_symbolic_init fm infl_man in
    let (start_addr, fuzz_start) = Exec_set_options.decide_start_addrs () in
      Exec_fuzzloop.fuzz start_addr fuzz_start
	!Exec_options.opt_fuzz_end_addrs fm asmir_gamma symbolic_init
	reset_heuristics
;;

main Sys.argv;;
