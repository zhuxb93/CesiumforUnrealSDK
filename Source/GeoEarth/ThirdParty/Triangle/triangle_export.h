#ifndef TRIANGLE_EXPORT_H
#define TRIANGLE_EXPORT_H

#ifdef _WIN32
    #ifdef TRIANGLE_EXPORTS
        #define TRIANGLE_EXPORT __declspec(dllexport)
    #else
        #define TRIANGLE_EXPORT
    #endif
#else
    #define TRIANGLE_EXPORT
#endif

#endif /* TRIANGLE_EXPORT_H */