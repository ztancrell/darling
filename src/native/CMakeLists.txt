project(native)

include(wrap_elf)

if (COMPONENT_gui)
	set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "gui")

	wrap_elf(FreeType libfreetype.so)
	wrap_elf(jpeg libjpeg.so)
	wrap_elf(png libpng.so)
	wrap_elf(tiff libtiff.so)
	wrap_elf(gif libgif.so)
	wrap_elf(EGL libEGL.so)
	wrap_elf(fontconfig libfontconfig.so)
	wrap_elf(X11 libX11.so)
	wrap_elf(Xext libXext.so)
	wrap_elf(XRandR libXrandr.so)
	wrap_elf(Xcursor libXcursor.so)
	wrap_elf(xkbfile libxkbfile.so)
	wrap_elf(wayland-client libwayland-client.so)
	wrap_elf(wayland-cursor libwayland-cursor.so)
	wrap_elf(xkbcommon libxkbcommon.so)
	wrap_elf(cairo libcairo.so)
	wrap_elf(dbus libdbus-1.so)
	# in usr/lib/native: OpenGL.framework puts its own glFlush in front of this one
	wrap_elf(GL libGL.so)
	wrap_elf(GLU libGLU.so "System/Library/Frameworks/OpenGL.framework/Versions/A/Libraries")

	# The host's Vulkan driver, loadable by Mach-O code, with a manifest where Vulkan loaders look
	# (ANGLE's Vulkan backend brings its own loader and finds it there or via VK_DRIVER_FILES).
	# ponytail: this is Mesa's AMD driver specifically, taken from the build machine. To serve any
	# GPU, wrap the host loader (libvulkan.so.1) behind a small driver-interface shim instead.
	wrap_elf(vulkan_radeon libvulkan_radeon.so)
	# A shim in front of it that advertises VK_EXT_metal_surface; see vulkan/icd_shim.c for why.
	set(DYLIB_INSTALL_NAME "/usr/lib/native/libvulkan_darling_icd.dylib")
	add_darling_library(vulkan_darling_icd SHARED vulkan/icd_shim.c)
	target_link_libraries(vulkan_darling_icd PRIVATE system)
	make_fat(vulkan_darling_icd)
	install(TARGETS vulkan_darling_icd DESTINATION libexec/darling/usr/lib/native)
	install(FILES vulkan/radeon_icd.json DESTINATION libexec/darling/usr/share/vulkan/icd.d)
endif()
