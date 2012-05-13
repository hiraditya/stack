#include "Diagnostic.h"
#include <llvm/ADT/STLExtras.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/ADT/Twine.h>
#include <llvm/Analysis/DebugInfo.h>
#include <llvm/Support/DebugLoc.h>
#include <llvm/Support/raw_ostream.h>
#include <stdlib.h>

using namespace llvm;

namespace {

class BugReporter : public DiagnosticImpl {
public:
	BugReporter(LLVMContext &VMCtx) : OS(llvm::errs()), VMCtx(VMCtx) {
		isDisplayed = OS.is_displayed();
	}
	virtual void emit(const llvm::DebugLoc &);
	virtual void emit(const llvm::Twine &Str);
	virtual llvm::raw_ostream &os() { return OS; }
private:
	raw_ostream &OS;
	LLVMContext &VMCtx;
	bool isDisplayed;
};


class BugVerifier : public DiagnosticImpl {
public:
	BugVerifier(const char *Str) : Prefix(Str) {}
	virtual void emit(const llvm::DebugLoc &);
	virtual void emit(const llvm::Twine &Str);
	virtual llvm::raw_ostream &os() { return nulls(); }
private:
	std::string Prefix, Expected;
	unsigned Line, Col;
};

} // anonymous namespace

// Diagnostic

Diagnostic::Diagnostic(LLVMContext &VMCtx) {
	if (const char *Prefix = ::getenv("VERIFY_PREFIX")) {
		Diag.reset(new BugVerifier(Prefix));
		return;
	}
	Diag.reset(new BugReporter(VMCtx));
}

// BugReporter

void BugReporter::emit(const DebugLoc &DbgLoc) {
	if (isDisplayed)
		OS.changeColor(raw_ostream::CYAN);
	llvm::MDNode *N = DbgLoc.getAsMDNode(VMCtx);
	llvm::DILocation Loc(N);
	for (;;) {
		OS << Loc.getDirectory() << Loc.getFilename() << ':';
		OS << Loc.getLineNumber() << ':';
		if (unsigned Col = Loc.getColumnNumber())
			OS << Col << ':';
		Loc = Loc.getOrigLocation();
		if (!Loc.Verify())
			break;
		OS << '\n';
	}
	if (isDisplayed)
		OS.changeColor(raw_ostream::MAGENTA);
	OS << " bug: ";
}

void BugReporter::emit(const llvm::Twine &Str) {
	if (isDisplayed)
		OS.changeColor(raw_ostream::CYAN);
	OS << Str << '\n';
	if (isDisplayed)
		OS.resetColor();
}

// BugVerifier

void BugVerifier::emit(const DebugLoc &DbgLoc) {
	Line = DbgLoc.getLine();
	Col = DbgLoc.getCol();
	// TODO: read expected string
}

void BugVerifier::emit(const Twine &Str) {
	SmallString<256> Buf;
	StringRef Actual = Str.toNullTerminatedStringRef(Buf);
	if (Actual.startswith(Expected))
		return;
	// TODO: wrong
}