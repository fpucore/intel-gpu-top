asahi_progs = [
	'asahi_get_params',
	'asahi_get_time',
]

foreach prog : asahi_progs
	test_executables += executable(prog,
				       prog + '.c',
				       dependencies : test_deps,
				       install_dir : asahidir,
				       install_rpath : asahi_rpathdir,
				       install : true)
	test_list += join_paths('asahi', prog)
endforeach
