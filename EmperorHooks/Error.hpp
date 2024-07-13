#pragma once

#define RASSERT_STRINGIFY(X) #X
#define RASSERT_STRINGIFY_2(X) RASSERT_STRINGIFY(X)

#define release_assert(X) if (!(X)) do { MessageBoxA(nullptr, "assert failed: " RASSERT_STRINGIFY(X) " at " __FILE__ ":" RASSERT_STRINGIFY_2(__LINE__), "assert failed", 0); abort(); } while (0)