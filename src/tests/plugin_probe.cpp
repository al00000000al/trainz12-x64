// Dumps everything the plugin system answers for the real plugins.ini: collections by group and by name,
// type lookups by id and by name (these load the plugin libraries), remaps, and the kernel's own LZSS and
// BlowFish plugins. Linked against "Kernel_7c.dll" by name, so the same exe runs against the original module
// (a directory where Kernel_7c.dll is the original) and against ours; the two dumps must be identical.
//
// usage: plugin_probe.exe <plugins.ini> <out.txt>   (run from the directory holding the modules)
#include <jet/Plugin.h>

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace Jet;

struct Probe : PluginBase {
    using PluginBase::LookupPluginType;
};

static FILE* out;

static std::string Str(const UId& id)
{
    const PString s = id.GetString();
    return s.Text() ? s.Text() : "(null)";
}

static std::string Str(const PString& s) { return s.Text() ? std::string(s.Text()) : "(null)"; }

static void DumpConfig(const PluginConfig* c, int depth)
{
    std::fprintf(out, "%*s%s %s parent=%s group=%d missing=%d children=%d\n", depth * 2, "", Str(c->GetName()).c_str(),
                 Str(c->GetId()).c_str(), c->GetParent() ? Str(c->GetParentId()).c_str() : "-", c->IsGroup(),
                 c->IsMissing(), c->HasChildren());
}

static void DumpList(PluginConfigList& list)
{
    std::fprintf(out, "  count=%u\n", list.Count());
    for (auto i = list.First(); i != list.End(); ++i) {
        DumpConfig(*i, 2);
        PluginConfigList sub;
        (*i)->Enumerate(sub);
        for (auto j = sub.First(); j != sub.End(); ++j) DumpConfig(*j, 4);
    }
}

static void DumpType(const Result& r, PluginType* t)
{
    std::fprintf(out, " -> result=%d", r.code);
    if (t) {
        std::fprintf(out, " type=%s %s", Str(t->GetName()).c_str(), Str(t->GetId()).c_str());
        if (t->GetParent()) std::fprintf(out, " parent=%s", Str(t->GetParentId()).c_str());
        t->ReleaseReference();
    }
    std::fprintf(out, "\n");
}

static unsigned long Hash(const void* p, unsigned long n)
{
    unsigned long h = 2166136261u;
    for (unsigned long i = 0; i < n; ++i) h = (h ^ static_cast<const unsigned char*>(p)[i]) * 16777619u;
    return h;
}

template <class F>
static void Guard(const char* what, F f)
{
    __try {
        f();
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        std::fprintf(out, " %s: EXCEPTION %08lx\n", what, GetExceptionCode());
    }
}

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
    out = std::strcmp(argv[2], "-") == 0 ? stderr : std::fopen(argv[2], "w");

    // ids, groups and (parent, name) pairs from the ini
    std::set<std::string> ids, parents, groups;
    std::vector<std::pair<std::string, std::string>> named;
    std::set<std::string> groupNames;  // "parent/name" of groups
    std::ifstream ini(argv[1]);
    for (std::string line; std::getline(ini, line);) {
        const size_t eq = line.find('=');
        if (eq == std::string::npos || line.find("Plugin") == std::string::npos) continue;
        std::vector<std::string> f;
        std::stringstream ss(line.substr(eq + 1));
        for (std::string t; std::getline(ss, t, ':');) f.push_back(t);
        const bool plugin = line.find("+Plugin=") != std::string::npos;
        const size_t nameAt = plugin ? 1 : 0;
        if (line.find("PluginRemap") != std::string::npos) {
            for (auto& x : f) ids.insert(x);
            continue;
        }
        if (f.size() <= nameAt + 1) continue;
        ids.insert(f[nameAt + 1]);
        if (f.size() > nameAt + 2) {
            parents.insert(f[nameAt + 2]);
            named.emplace_back(f[nameAt + 2], f[nameAt]);
        }
        if (!plugin) {
            parents.insert(f[nameAt + 1]);
            groups.insert(f[nameAt + 1]);
            if (f.size() > nameAt + 2) groupNames.insert(f[nameAt + 2] + "/" + f[nameAt]);
        }
    }
    ids.insert("8DBDFEB0-52F4-4c1b-BA91-6A7B1CCC6D3E");
    ids.insert("8DBDFEB1-52F4-4c1b-BA91-6A7B1CCC6D3E");
    ids.insert("AAF296E0-17A7-11d5-A9C2-00E01898A4A9");
    ids.insert("AAF296E1-17A7-11d5-A9C2-00E01898A4A9");
    ids.insert("00000000-0000-0000-0000-000000000000");
    named.emplace_back("8DBDFEB0-52F4-4c1b-BA91-6A7B1CCC6D3E", "LZSS");
    named.emplace_back("AAF296E0-17A7-11d5-A9C2-00E01898A4A9", "BlowFish");
    named.emplace_back("8DBDFEB0-52F4-4c1b-BA91-6A7B1CCC6D3E", "NoSuchPlugin");

    auto collections = [&](const char* phase) {
        for (auto& p : parents)
            Guard("collection", [&] {
                std::fprintf(out, "[%s] collection %s\n", phase, p.c_str());
                PluginCollection c{UId(p.c_str())};
                DumpList(c);
            });
        for (auto& [p, n] : named)
            Guard("collection/name", [&] {
                std::fprintf(out, "[%s] collection %s / %s\n", phase, p.c_str(), n.c_str());
                PluginCollection c{UId(p.c_str()), PString(n.c_str())};
                DumpList(c);
            });
    };

    // Like the game, hold a system reference for the whole run (otherwise the system and its "missing" and
    // "failed" flags are rebuilt for every call). Lookups of groups are left out: the original dereferences the
    // group's null library there and crashes (ours returns 1 by id and 3 by name).
    PluginCollection* keep = new PluginCollection(UId("00000000-0000-0000-0000-000000000000"));
    collections("before");
    for (auto& id : ids)
        Guard("remap", [&] {
            const UId u(id.c_str());
            std::fprintf(out, "remap %s -> %d %s\n", id.c_str(), PluginBase::IsRemapped(u), Str(PluginBase::GetRemap(u)).c_str());
        });
    for (auto& id : ids)
        if (!groups.count(id)) Guard("lookup id", [&] {
            PluginType* t = nullptr;
            std::fprintf(out, "lookup %s", id.c_str());
            const Result r = Probe::LookupPluginType(&t, UId(id.c_str()));
            DumpType(r, t);
        });
    for (auto& [p, n] : named)
        if (!groupNames.count(p + "/" + n)) Guard("lookup name", [&] {
            PluginType* t = nullptr;
            std::fprintf(out, "lookup %s / %s", p.c_str(), n.c_str());
            const Result r = Probe::LookupPluginType(&t, UId(p.c_str()), PString(n.c_str()));
            DumpType(r, t);
        });
    collections("after");

    // the kernel's codecs through the plugin interface
    Guard("codecs", [&] {
        std::vector<unsigned char> src(20000), packed(30000), back(20000);
        unsigned seed = 1;
        for (size_t i = 0; i < src.size(); ++i) {
            seed = seed * 1103515245 + 12345;
            src[i] = static_cast<unsigned char>((i / 7) % 13 + ((seed >> 16) & 3));
        }
        for (const char* name : {"LZSS", "NoSuchPlugin"}) {
            Compress* c = Compress::CreateInstance(PString(name));
            std::fprintf(out, "compress %s -> %s\n", name, c ? "instance" : "null");
            if (!c) continue;
            for (unsigned level : {0u, 1u, 5u, 9u}) {
                const unsigned long n = c->CompressBlock(packed.data(), 30000, src.data(), 20000, level);
                const unsigned long m = c->DecompressBlock(back.data(), 20000, packed.data(), n);
                std::fprintf(out, "  level %u: %lu bytes hash %08lx, back %lu %s\n", level, n, Hash(packed.data(), n), m,
                             m == 20000 && back == src ? "ok" : "MISMATCH");
            }
            static_cast<const ReferenceCount*>(c)->ReleaseReference();
        }
        Compress* d = Compress::CreateInstance();
        std::fprintf(out, "compress default -> %s\n", d ? "instance" : "null");
        if (d) static_cast<const ReferenceCount*>(d)->ReleaseReference();

        Encrypt* e = Encrypt::CreateInstance(PString("BlowFish"));
        std::fprintf(out, "encrypt BlowFish -> %s\n", e ? "instance" : "null");
        if (e) {
            Encrypt::Caps caps{};
            e->GetCaps(caps);
            std::fprintf(out, "  caps %lu %lu %lu %lu %lu\n", caps.keyMinSize, caps.keyMaxSize, caps.bufferMinSize,
                         caps.bufferMaxSize, caps.bufferAlignment);
            const char key[] = "probe key";
            std::fprintf(out, "  setkey %d optimal %lu\n", e->SetKey(key, sizeof key - 1).code, e->GetOptimalSize(1001));
            std::vector<unsigned char> buf(src.begin(), src.begin() + 4096);
            const int er = e->EncryptData(buf.data(), 4096).code;
            std::fprintf(out, "  encrypt %d hash %08lx", er, Hash(buf.data(), 4096));
            const int dr = e->DecryptData(buf.data(), 4096).code;
            std::fprintf(out, " decrypt %d %s\n", dr, std::memcmp(buf.data(), src.data(), 4096) == 0 ? "ok" : "MISMATCH");
            static_cast<const ReferenceCount*>(e)->ReleaseReference();
        }
    });
    delete keep;
    std::fprintf(out, "done\n");
    std::fclose(out);
    return 0;
}
