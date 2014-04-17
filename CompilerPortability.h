// --------------------------------------------------------------------
// Compiler abstraction layer
// --------------------------------------------------------------------

#ifndef COMPILER_PORTABILITY_H
#define COMPILER_PORTABILITY_H

#if defined(_MSC_VER)
// --------------------------------------------------------------------
// Microsoft compiler specific code
// --------------------------------------------------------------------

#define THREADLOCAL __declspec( thread )

#elif defined(__GNUC__)
// --------------------------------------------------------------------
// GNU-C/C++ specific code
// --------------------------------------------------------------------

#define THREADLOCAL __thread

#elif defined(ICC)
// --------------------------------------------------------------------
// Intel compiler specific code
// --------------------------------------------------------------------

#define THREADLOCAL __thread

#else
#error "Unsupported platform."
#endif

#endif // COMPILER_PORTABILITY_H
