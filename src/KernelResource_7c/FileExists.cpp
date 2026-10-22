// File::Exists as the original imports it: static ResultClass<&File::results, File::Results> Exists(const char*),
// the result (0 = there) returned in memory. KernelStandard.h declares a bool wrapper under that name for Kernel_7c,
// so this unit declares the class on its own (it includes no Jet header) to get the real import.
namespace Jet {

struct ResultTable;

template <const ResultTable* const* Table, class Codes>
class ResultClass {
public:
    ResultClass(int c = 0) : code(c) {}
    int code;
};

class File {
public:
    enum Results {};
    static const ResultTable* const results;
    using Result = ResultClass<&results, Results>;
    __declspec(dllimport) static Result __cdecl Exists(const char* path);
};

}  // namespace Jet

// used by ResourceDatabase::GetFromPath
bool JetResourceFileExists(const char* path) { return Jet::File::Exists(path).code == 0; }
