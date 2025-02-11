# GTK+ 2.x UI frontend for:
# - Thunar 1.6 (XFCE)
CMAKE_MINIMUM_REQUIRED(VERSION 3.0)
CMAKE_POLICY(SET CMP0048 NEW)
IF(POLICY CMP0063)
	# CMake 3.3: Enable symbol visibility presets for all
	# target types, including static libraries and executables.
	CMAKE_POLICY(SET CMP0063 NEW)
ENDIF(POLICY CMP0063)
PROJECT(rom-properties-xfce LANGUAGES C CXX)

# Disable glib deprecation warnings.
# - GTypeDebugFlags is deprecated as of glib-2.36.
# - GTimeVal is deprecated as of glib-2.62.
# GTK+ 2.x uses both of these, and since it's unlikely that
# GTK+ 2.x will ever be updated to use the non-deprecated
# versions, simply remove the deprecation warning.
ADD_DEFINITIONS(-DGLIB_DISABLE_DEPRECATION_WARNINGS)

# Find packages.
FIND_PACKAGE(GLib2 ${REQUIRE_GTK2} ${GLIB_MIN_VERSION})
FIND_PACKAGE(GObject2 ${REQUIRE_GTK2} ${GLIB_MIN_VERSION})
FIND_PACKAGE(GIO ${REQUIRE_GTK2} ${GLIB_MIN_VERSION})
FIND_PACKAGE(GIO-UNIX ${REQUIRE_GTK2} ${GLIB_MIN_VERSION})
FIND_PACKAGE(GdkPixbuf2 ${REQUIRE_GTK2})
# GTK2 minimum is 2.0.0, so no version check is needed.
FIND_PACKAGE(GTK2 ${REQUIRE_GTK2} COMPONENTS gtk)
IF(GLib2_FOUND AND GObject2_FOUND AND GIO_FOUND AND GIO-UNIX_FOUND AND GdkPixbuf2_FOUND AND GTK2_FOUND)
	# We're not going to have an explicit dependency on libnautilus-extension.
	# It will be dlopen()'d at runtime based on which
	# initialization function is called.

	# ThunarX2 (XFCE)
	FIND_PACKAGE(ThunarX2)
	IF(NOT ThunarX2_FOUND)
		MESSAGE(STATUS "ThunarX2 not found. Using the default extensions directory.")
		SET(ThunarX2_EXTENSIONS_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/thunarx-2")
	ENDIF(NOT ThunarX2_FOUND)
ELSE(GLib2_FOUND AND GObject2_FOUND AND GIO_FOUND AND GIO-UNIX_FOUND AND GdkPixbuf2_FOUND AND GTK2_FOUND)
	# GTK+ 2.x (or required dependencies) were not found.
	# Disable the XFCE frontend.
	SET(BUILD_GTK2 OFF CACHE INTERNAL "Build the XFCE (GTK+ 2.x) plugin. (Thunar 1.7 and earlier)" FORCE)
ENDIF(GLib2_FOUND AND GObject2_FOUND AND GIO_FOUND AND GIO-UNIX_FOUND AND GdkPixbuf2_FOUND AND GTK2_FOUND)

# Sources and headers. (GTK+ common)
STRING(REGEX REPLACE "([^;]+)" "../\\1" ${PROJECT_NAME}_SRCS "${rom-properties-gtk_SRCS}")
STRING(REGEX REPLACE "([^;]+)" "../\\1" ${PROJECT_NAME}_H    "${rom-properties-gtk_H}")

# GdkImageConv (GTK+ 2.x)
SET(${PROJECT_NAME}_SRCS ${${PROJECT_NAME}_SRCS} GdkImageConv.cpp)
SET(${PROJECT_NAME}_H    ${${PROJECT_NAME}_H}    GdkImageConv.hpp)

IF(ENABLE_ACHIEVEMENTS)
	STRING(REGEX REPLACE "([^;]+)" "../\\1" ${PROJECT_NAME}-notify_SRCS "${rom-properties-gtk-notify_SRCS}")
	STRING(REGEX REPLACE "([^;]+)" "../\\1" ${PROJECT_NAME}-notify_H    "${rom-properties-gtk-notify_H}")

	# D-Bus bindings for notifications.
	# NOTE: This has to be in this file, not the GTK+ CMakeLists.txt.
	SET(DBUS_XML_FILENAME "${CMAKE_CURRENT_SOURCE_DIR}/../../dbus/org.freedesktop.Notifications.xml")
	ADD_CUSTOM_COMMAND(
		OUTPUT Notifications.c Notifications.h
		COMMAND "${GDBUS_CODEGEN}"
			--generate-c-code Notifications
			"${DBUS_XML_FILENAME}"
		WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
		DEPENDS "${DBUS_XML_FILENAME}"
		VERBATIM
		)

	SET(${PROJECT_NAME}-notify_SRCS
		${${PROJECT_NAME}-notify_SRCS}
		${CMAKE_CURRENT_BINARY_DIR}/Notifications.c
		)

	# Disable unused parameter warnings in generated D-Bus sources.
	INCLUDE(CheckCCompilerFlag)
	CHECK_C_COMPILER_FLAG("-Wno-unused-parameter" CFLAG_Wno_unused_parameter)
	IF(CFLAG_Wno_unused_parameter)
		SET_SOURCE_FILES_PROPERTIES(${CMAKE_CURRENT_BINARY_DIR}/Notifications.c
			APPEND_STRING PROPERTIES COMPILE_FLAGS " -Wno-unused-parameter ")
	ENDIF(CFLAG_Wno_unused_parameter)
ENDIF(ENABLE_ACHIEVEMENTS)

# CPU-specific and optimized sources.
INCLUDE(CPUInstructionSetFlags)
IF(CPU_i386 OR CPU_amd64)
	# IFUNC functionality
	INCLUDE(CheckIfuncSupport)
	CHECK_IFUNC_SUPPORT()
	IF(HAVE_IFUNC)
		SET(${PROJECT_NAME}_IFUNC_SRCS GdkImageConv_ifunc.cpp)

		# Disable LTO on the IFUNC files if LTO is known to be broken.
		IF(GCC_5xx_LTO_ISSUES)
			SET_SOURCE_FILES_PROPERTIES(${${PROJECT_NAME}_IFUNC_SRCS}
				APPEND_STRING PROPERTIES COMPILE_FLAGS " -fno-lto ")
		ENDIF(GCC_5xx_LTO_ISSUES)
	ENDIF(HAVE_IFUNC)

	# NOTE: SSSE3 flags are set in subprojects, not here.
	SET(${PROJECT_NAME}_SSSE3_SRCS GdkImageConv_ssse3.cpp)

	IF(MSVC AND NOT CMAKE_CL_64)
		SET(SSSE3_FLAG "/arch:SSE2")
	ELSEIF(NOT MSVC)
		# TODO: Other compilers?
		SET(SSSE3_FLAG "-mssse3")
	ENDIF()
	IF(SSSE3_FLAG)
		SET_SOURCE_FILES_PROPERTIES(${${PROJECT_NAME}_SSSE3_SRCS}
			APPEND_STRING PROPERTIES COMPILE_FLAGS " ${SSSE3_FLAG} ")
	ENDIF(SSSE3_FLAG)
ENDIF()
UNSET(arch)

# Sources and headers. (XFCE-specific)
SET(${PROJECT_NAME}_SRCS
	${${PROJECT_NAME}_CSRCS} ${${PROJECT_NAME}_SRCS}
	${${PROJECT_NAME}-notify_SRCS} ${${PROJECT_NAME}-notify_H}
	${${PROJECT_NAME}_IFUNC_SRCS}
	${${PROJECT_NAME}_SSSE3_SRCS}
	../gtk3/RpThunarPlugin.cpp
	../gtk3/RpThunarProvider.cpp
	)
SET(${PROJECT_NAME}_H
	${${PROJECT_NAME}_CH} ${${PROJECT_NAME}_H}
	../gtk3/RpThunarPlugin.hpp
	../gtk3/RpThunarProvider.hpp
	../gtk3/plugin-helper.h
	../gtk3/thunarx-mini.h
	)

IF(HAVE_GSOUND OR (HAVE_LIBCANBERRA AND HAVE_LIBCANBERRA_GTK2))
	SET(${PROJECT_NAME}_SRCS ${${PROJECT_NAME}_SRCS} ../MessageSound.cpp)
	SET(${PROJECT_NAME}_H    ${${PROJECT_NAME}_H}    ../MessageSound.hpp)
ENDIF(HAVE_GSOUND OR (HAVE_LIBCANBERRA AND HAVE_LIBCANBERRA_GTK2))

IF(ENABLE_PCH)
	# Precompiled headers.
	INCLUDE(PrecompiledHeader)
	SET(${PROJECT_NAME}_PCH_C ../stdafx.c)
	SET(${PROJECT_NAME}_PCH_CXX ../stdafx.cpp)
	SET(${PROJECT_NAME}_PCH_SRC ${${PROJECT_NAME}_PCH_C} ${${PROJECT_NAME}_PCH_CXX})
	SET(${PROJECT_NAME}_PCH_H ../stdafx.h)
ENDIF(ENABLE_PCH)

IF(BUILD_GTK2)
	ADD_LIBRARY(${PROJECT_NAME} MODULE
		${${PROJECT_NAME}_PCH_SRC} ${${PROJECT_NAME}_PCH_H}
		${${PROJECT_NAME}_SRCS}
		)
	IF(ENABLE_PCH)
		ADD_PRECOMPILED_HEADER(${PROJECT_NAME} ${${PROJECT_NAME}_PCH_H}
			SOURCE_C ${${PROJECT_NAME}_PCH_C}
			SOURCE_CXX ${${PROJECT_NAME}_PCH_CXX}
			)
	ENDIF(ENABLE_PCH)
	SET_TARGET_PROPERTIES(${PROJECT_NAME} PROPERTIES PREFIX "")
	DO_SPLIT_DEBUG(${PROJECT_NAME})
	TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME}
		PUBLIC	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>		# xfce
			$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>		# xfce
		PRIVATE	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>	# gtk
			$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/..>	# gtk
			$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/src>		# src
			$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src>		# src
		)
	TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${GTK2_INCLUDE_DIRS})

	TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE glibresources)
	TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE rpcpu romdata rpfile rpbase)
	IF(ENABLE_NLS)
		TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE i18n)
	ENDIF(ENABLE_NLS)
	TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE GdkPixbuf2::gdkpixbuf2)
	TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC ${GTK2_LIBRARIES} GLib2::gio-unix GLib2::gio GLib2::gobject GLib2::glib)
	IF(HAVE_GSOUND)
		TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE GSound::gsound)
		TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE ENABLE_MESSAGESOUND)
	ELSEIF(HAVE_LIBCANBERRA AND HAVE_LIBCANBERRA_GTK2)
		TARGET_LINK_LIBRARIES(${PROJECT_NAME} PRIVATE LibCanberra::gtk2 LibCanberra::libcanberra)
		TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE ENABLE_MESSAGESOUND)
	ENDIF()

	# TODO: Move GTK2_DEFINITIONS to TARGET_COMPILE_DEFINITIONS.
	# (Requires removing the "-D" switches.)
	ADD_DEFINITIONS(${GTK2_DEFINITIONS})
	TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE RP_UI_XFCE)
ENDIF(BUILD_GTK2)

#######################
# Install the plugin. #
#######################

IF(BUILD_GTK2)
	# FIXME: ${ThunarX2_EXTENSIONS_DIR} always uses the system prefix.
	# This should be adjusted to ${CMAKE_INSTALL_PREFIX}.
	INSTALL(TARGETS ${PROJECT_NAME}
		LIBRARY DESTINATION "${ThunarX2_EXTENSIONS_DIR}"
		COMPONENT "plugin"
		)

	# Check if a split debug file should be installed.
	IF(INSTALL_DEBUG)
		# FIXME: Generator expression $<TARGET_PROPERTY:${_target},PDB> didn't work with CPack-3.6.1.
		GET_TARGET_PROPERTY(DEBUG_FILENAME ${PROJECT_NAME} PDB)
		IF(DEBUG_FILENAME)
			INSTALL(FILES "${DEBUG_FILENAME}"
				DESTINATION "lib/debug/${ThunarX2_EXTENSIONS_DIR}"
				COMPONENT "debug"
				)
		ENDIF(DEBUG_FILENAME)
	ENDIF(INSTALL_DEBUG)
ENDIF(BUILD_GTK2)
