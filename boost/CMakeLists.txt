# Only intended to install pkg-config files for boost-headers
cmake_minimum_required(VERSION 3.10)
project(BoostPkgConfig)

include(GNUInstallDirs)

if (MSVC)
	install(FILES "pkgconfig/msvc/boost-headers.pc"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
	)
else()
	install(FILES "pkgconfig/boost-headers.pc"
		DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
	)
endif()
