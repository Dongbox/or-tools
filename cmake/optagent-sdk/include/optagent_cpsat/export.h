#ifndef OPTAGENT_CPSAT_EXPORT_H_
#define OPTAGENT_CPSAT_EXPORT_H_

#if defined(_WIN32) && !defined(OPTAGENT_CPSAT_STATIC)
#if defined(optagent_cpsat_EXPORTS)
#define OPTAGENT_CPSAT_EXPORT __declspec(dllexport)
#else
#define OPTAGENT_CPSAT_EXPORT __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define OPTAGENT_CPSAT_EXPORT __attribute__((visibility("default")))
#else
#define OPTAGENT_CPSAT_EXPORT
#endif

#endif  // OPTAGENT_CPSAT_EXPORT_H_
