find_path(NSS_INCLUDE_DIR nss/nss.h)
find_path(NSPR_INCLUDE_DIR nspr/nspr.h)

set(NSS_INCLUDE_DIRS "${NSS_INCLUDE_DIR}/nss" "${NSPR_INCLUDE_DIR}/nspr")

find_library(NSS3_LIBRARY nss3)
find_library(NSPR4_LIBRARY nspr4)

set(NSS_LIBRARIES "${NSS3_LIBRARY}" "${NSPR4_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(NSS DEFAULT_MSG
    NSS3_LIBRARY NSS_INCLUDE_DIR NSPR4_LIBRARY NSPR_INCLUDE_DIR)

mark_as_advanced(NSS_INCLUDE_DIR NSPR_INCLUDE_DIR NSS3_LIBRARY NSPR4_LIBRARY)
