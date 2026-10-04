set(SUBFRAMEWORK_VERSION "A")

generate_sdk_subframework(LaunchServices
	BASE_PATH ${coreservices_sdk_path}
	VERSION ${SUBFRAMEWORK_VERSION}
	HEADER "../../include/LaunchServices"
)

add_framework(LaunchServices
	FAT
	CURRENT_VERSION
	VERSION ${SUBFRAMEWORK_VERSION}
	PARENT "CoreServices"

	SOURCES
		IconsCore.c
		LaunchServices.c
		LaunchServices.cpp
		LSApplicationProxy.m
		LSApplicationWorkspace.m
		LSRunning.m
		constants.c
		LSInfo.m
		LSOpen.m
		UTType.m

	DEPENDENCIES
		CoreFoundation
		Foundation
		cxx
		icucore
		system
		CarbonCore
		FMDB
)

add_subdirectory(launchservicesd)

# macOS binaries expect NSUserActivity in CoreServices; the class itself lives in Foundation here.
# 64-bit slices only: the i386 Objective-C ABI names class symbols differently.
set_property(TARGET LaunchServices APPEND_STRING PROPERTY
	LINK_FLAGS " -Xarch_x86_64 -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/reexport.exp -Xarch_arm64 -Wl,-reexported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/reexport.exp")
