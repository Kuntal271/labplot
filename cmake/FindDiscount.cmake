# - Find Discount
# Find the Discount markdown library.
#
# This module defines
#  Discount_FOUND - whether the Discount library was found
#  Discount_LIBRARIES - the Discount library
#  Discount_INCLUDE_DIR - the include path of the Discount library

# SPDX-FileCopyrightText: 2017 Julian Wolff <wolff@julianwolff.de>
# SPDX-FileCopyrightText: 2018 Sune Vuorela <sune@kde.org>
# SPDX-License-Identifier: BSD-3-Clause

find_package(PkgConfig QUIET)
pkg_check_modules(PC_LIBMARKDOWN libmarkdown QUIET)

set(Discount_VERSION ${PC_LIBMARKDOWN_VERSION})

if (Discount_INCLUDE_DIR AND Discount_LIBRARIES)

  # Already in cache
  set (Discount_FOUND TRUE)

else (Discount_INCLUDE_DIR AND Discount_LIBRARIES)

  find_library (Discount_LIBRARIES
    NAMES markdown libmarkdown
    HINTS ${PC_LIBMARKDOWN_LIBRARY_DIRS}
  )

  find_path (Discount_INCLUDE_DIR
    NAMES mkdio.h
    HINTS ${PC_LIBMARKDOWN_INCLUDE_DIRS}
  )

  include (FindPackageHandleStandardArgs)
  find_package_handle_standard_args (Discount DEFAULT_MSG Discount_LIBRARIES Discount_INCLUDE_DIR)

endif (Discount_INCLUDE_DIR AND Discount_LIBRARIES)

mark_as_advanced(Discount_INCLUDE_DIR Discount_LIBRARIES Discount_VERSION)

if (Discount_FOUND)
   add_library(Discount::Lib UNKNOWN IMPORTED)
   set_target_properties(Discount::Lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES ${Discount_INCLUDE_DIR} IMPORTED_LOCATION ${Discount_LIBRARIES})
endif()
