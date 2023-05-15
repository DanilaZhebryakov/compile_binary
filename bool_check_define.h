#define FAILURE_TRACE

#ifdef FAILURE_TRACE
#define CHECK_BOOL(...) { \
    if(!(__VA_ARGS__)) {  \
        error_log("Fail here: %s %d %s\n", #__VA_ARGS__, __LINE__, __PRETTY_FUNCTION__); \
        return false;     \
    }                     \
}
#else
#define CHECK_BOOL(...) { \
    if(!(__VA_ARGS__)) {  \
        return false;     \
    }                     \
}
#endif