#pragma once

#define release_assert(X) if (!(X)) do { MessageBoxA(nullptr, "assert failed", "assert failed", 0); abort(); } while (0)