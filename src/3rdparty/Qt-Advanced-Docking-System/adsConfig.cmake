include(CMakeFindDependencyMacro)
find_dependency(Qt5Core ${REQUIRED_QT_VERSION} REQUIRED)
find_dependency(Qt5Gui ${REQUIRED_QT_VERSION} REQUIRED)
find_dependency(Qt5Widgets ${REQUIRED_QT_VERSION} REQUIRED)
include("${CMAKE_CURRENT_LIST_DIR}/adsTargets.cmake")