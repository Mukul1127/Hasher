set(WOLFSSL_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(WOLFSSL_INCLUDE_DIR "${WOLFSSL_DIR}/include")
set(WOLFSSL_LIB_DIR "${WOLFSSL_DIR}/lib")
set(WOLFSSL_LIB "${WOLFSSL_LIB_DIR}/wolfssl.lib")
include_directories(${WOLFSSL_INCLUDE_DIR})
link_directories(${WOLFSSL_LIB_DIR})
add_library(wolfssl STATIC IMPORTED)
set_target_properties(wolfssl PROPERTIES IMPORTED_LOCATION "${WOLFSSL_LIB}")