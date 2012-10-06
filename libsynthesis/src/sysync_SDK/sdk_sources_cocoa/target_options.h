// target_options.h for SySync SDK files that need it

#ifndef TARGET_OPTIONS
#define TARGET_OPTIONS

// Sysync related debug
#define SYDEBUG 2

#if DEBUG
// general app debug
#define CONSOLEDBG 1
#else
// no output to console, please
#undef CONSOLEDBG
#endif

#endif // TARGET_OPTIONS