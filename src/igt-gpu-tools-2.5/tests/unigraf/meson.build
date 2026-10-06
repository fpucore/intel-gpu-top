unigraf_progs = [
	'unigraf_connectivity',
	'unigraf_lt',
]

foreach prog : unigraf_progs
	test_executables += executable(prog, prog + '.c',
				       dependencies : test_deps,
				       install_dir : unigrafdir,
				       install_rpath : unigraf_rpathdir,
				       install : true)
	test_list += join_paths('unigraf', prog)
endforeach
