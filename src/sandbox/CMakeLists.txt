project(libsandbox)

set(DYLIB_INSTALL_NAME "/usr/lib/system/libsystem_sandbox.dylib")
# The profile reader and TinyScheme are internal: hidden, so that TinyScheme's generic names
# (putstr, eqv, ...) are not exported through libSystem. INLINE is emptied because scheme.c's
# accessors are used from profile.c and C99 `inline` alone would emit no definition for them.
set_source_files_properties(profile.c tinyscheme/scheme.c PROPERTIES
	COMPILE_FLAGS "-fvisibility=hidden -DSTANDALONE=0 -DUSE_DL=0 -DUSE_MATH=0 -DINLINE= -w")

add_circular(system_sandbox FAT
	SOURCES
	sandbox.c
	profile.c
	landlock.c
	tinyscheme/scheme.c

	SIBLINGS
		system_dyld
	DEPENDENCIES
		system_c
		system_kernel
		system_malloc
		platform
)

install(TARGETS system_sandbox DESTINATION libexec/darling/usr/lib/system)
