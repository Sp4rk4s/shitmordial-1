#include <windows.h>
#include <cstdio>
int main() {
    // Disposable host: the test controls its lifetime through stdin.
    if (std::getchar() == EOF) return 2;
    return GetModuleHandleW(L"injector_probe.dll") ? 0 : 1;
}
