// Jet declares many member functions __cdecl (`this` on the stack; mangled ...@@Q[AB]A...). Ghidra applies them
// as __thiscall, which shifts every argument in the decompiler (strings vanish, `this` becomes param_1...).
// For every function and external function whose mangled name says "non-static member, __cdecl", this script
// sets the __cdecl convention and inserts an explicit `this` pointer as the first parameter.
//@category Trainz

import ghidra.app.script.GhidraScript;
import ghidra.app.util.demangler.DemangledFunction;
import ghidra.app.util.demangler.DemangledObject;
import ghidra.app.util.demangler.DemanglerUtil;
import ghidra.program.model.data.DataType;
import ghidra.program.model.data.PointerDataType;
import ghidra.program.model.data.VoidDataType;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Parameter;
import ghidra.program.model.listing.ParameterImpl;
import ghidra.program.model.symbol.ExternalLocation;
import ghidra.program.model.symbol.ExternalLocationIterator;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;

public class FixCdeclMembers extends GhidraScript {
	private int fixed, seen;

	@Override
	protected void run() throws Exception {
		for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
			for (Symbol s : currentProgram.getSymbolTable().getSymbols(f.getEntryPoint())) fix(f, s.getName());
		}
		for (String lib : currentProgram.getExternalManager().getExternalLibraryNames()) {
			ExternalLocationIterator it = currentProgram.getExternalManager().getExternalLocations(lib);
			while (it.hasNext()) {
				ExternalLocation loc = it.next();
				Function f = loc.getFunction();
				if (f == null) continue;
				String mangled = loc.getOriginalImportedName();
				fix(f, mangled != null ? mangled : loc.getLabel());
			}
		}
		println(String.format("CDECLFIX %s mangled member functions seen=%d fixed=%d", currentProgram.getName(), seen, fixed));
	}

	private int errors;

	private void fix(Function f, String mangled) {
		try {
			fixOne(f, mangled);
		} catch (Exception e) {
			if (errors++ < 5) println("CDECLFIX skip " + mangled + ": " + e);
		}
	}

	private void fixOne(Function f, String mangled) throws Exception {
		if (mangled == null || !mangled.startsWith("?") || mangled.startsWith("??_")) return;
		int at = mangled.indexOf("@@");
		if (at < 0 || at + 4 > mangled.length()) return;
		// after "@@": a kind letter; members then have a this-qualifier letter; then the convention letter
		char kind = mangled.charAt(at + 2);
		boolean member = "ABEFIJMNQRUV".indexOf(kind) >= 0;   // non-static members (incl. virtual)
		boolean statik = "CDKLST".indexOf(kind) >= 0;        // static members
		boolean global = kind == 'Y' || kind == 'Z';         // free functions
		if (!member && !statik && !global) return;
		char conv;
		if (member) {
			if ("ABCD".indexOf(mangled.charAt(at + 3)) < 0) return;
			conv = mangled.charAt(at + 4);
		} else {
			conv = mangled.charAt(at + 3);
		}
		String want = conv == 'A' ? "__cdecl" : conv == 'E' ? "__thiscall" : conv == 'G' ? "__stdcall" :
			conv == 'I' ? "__fastcall" : null;
		if (want == null) return;
		seen++;
		boolean needThis = member && !"__thiscall".equals(want);
		boolean ok = want.equals(f.getCallingConventionName()) &&
			(!needThis || (f.getParameterCount() > 0 && "this".equals(f.getParameter(0).getName())));
		if (ok) return;
		if (!needThis) {
			// static/free or real thiscall: the convention alone fixes it (thiscall adds `this` by itself)
			f.setCallingConvention(want);
			fixed++;
			return;
		}

		// make sure the demangled parameter list is applied first, then switch convention and add `this`
		java.util.List<DemangledObject> all = DemanglerUtil.demangle(currentProgram, mangled, f.getEntryPoint());
		DemangledObject d = (all == null || all.isEmpty()) ? null : all.get(0);
		if (d instanceof DemangledFunction && !f.isExternal()) {
			try {
				d.applyTo(currentProgram, f.getEntryPoint(), new ghidra.app.util.demangler.DemanglerOptions(), monitor);
			} catch (Exception e) {
				// keep whatever signature is there
			}
		}
		f.setCallingConvention("__cdecl");
		DataType thisType = new PointerDataType(VoidDataType.dataType);
		Parameter p = new ParameterImpl("this", thisType, currentProgram, SourceType.ANALYSIS);
		f.insertParameter(0, p, SourceType.ANALYSIS);
		fixed++;
	}
}
