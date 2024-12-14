#if !defined(PLATFORM_WINDOWS)
#include "network.h"

NetInitResult net_init(void) {
    NetInitResult result;
    result.succeeded = true;
    
    return result;
}
#endif
