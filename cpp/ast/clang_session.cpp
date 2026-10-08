#include "clang_session.h"

#include <clang/Basic/Diagnostic.h>
#include <clang/Basic/DiagnosticIDs.h>
#include <clang/Basic/DiagnosticOptions.h>
#include <clang/Basic/FileManager.h>
#include <clang/Basic/FileSystemOptions.h>
#include <clang/Basic/SourceManager.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/Tooling.h>

#include <llvm/ADT/IntrusiveRefCntPtr.h>
#include <llvm/ADT/SmallString.h>

#include <fcntl.h>
#include <unistd.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace cpp2model {
namespace {

class CollectingDiagConsumer : public clang::DiagnosticConsumer {
public:
    ParseSummary summary;

    void HandleDiagnostic(clang::DiagnosticsEngine::Level level,
                          const clang::Diagnostic& info) override {
        std::string severity;
        switch (level) {
            case clang::DiagnosticsEngine::Ignored: return;
            case clang::DiagnosticsEngine::Note:   severity = "note";   break;
            case clang::DiagnosticsEngine::Remark: severity = "remark"; break;
            case clang::DiagnosticsEngine::Warning: severity = "warning"; ++summary.warnings; break;
            case clang::DiagnosticsEngine::Error:   severity = "error";   ++summary.errors; break;
            case clang::DiagnosticsEngine::Fatal:   severity = "fatal";   ++summary.errors; break;
        }

        std::string loc;
        clang::SourceLocation sl = info.getLocation();
        if (sl.isValid() && info.hasSourceManager()) {
            clang::PresumedLoc pl = info.getSourceManager().getPresumedLoc(sl);
            if (pl.isValid()) {
                std::ostringstream os;
                os << pl.getLine() << ":" << pl.getColumn();
                loc = os.str();
            }
        }

        llvm::SmallString<256> buf;
        info.FormatDiagnostic(buf);

        std::string msg;
        if (!loc.empty()) msg += loc + ": ";
        msg += severity + ": " + buf.str().str();
        summary.diagnostics.push_back(msg);
        clang::DiagnosticConsumer::HandleDiagnostic(level, info);
    }
};

}  // namespace

bool run_tool_on_code(const std::string& code,
                      std::unique_ptr<clang::FrontendAction> action,
                      ParseSummary& out) {
    char tmpl[] = "/tmp/cpp-ast-input-XXXXXX.cc";
    int fd = ::mkstemps(tmpl, 3);   // suffix ".cc"
    if (fd < 0) return false;
    std::string path(tmpl);

    size_t off = 0;
    while (off < code.size()) {
        ssize_t n = ::write(fd, code.data() + off, code.size() - off);
        if (n <= 0) {
            ::close(fd);
            ::unlink(path.c_str());
            return false;
        }
        off += static_cast<size_t>(n);
    }
    ::close(fd);

    std::vector<std::string> cmd{"clang-tool", path, "-fsyntax-only", "-std=c++20"};
    llvm::IntrusiveRefCntPtr<clang::FileManager> files(
        new clang::FileManager(clang::FileSystemOptions()));

    CollectingDiagConsumer diags;
    clang::tooling::ToolInvocation inv(cmd, std::move(action), files.get());
    inv.setDiagnosticConsumer(&diags);
    bool ok = inv.run();
    ::unlink(path.c_str());

    out = std::move(diags.summary);
    return ok;
}

}  // namespace cpp2model