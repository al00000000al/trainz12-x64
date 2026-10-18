// Decompiles every non-thunk function of the current program into <out dir>/<program>.c,
// one block per function headed by its address, signature source and size.
// Out dir comes from the script argument, default D:/trainz12-decomp/decomp.
//@category Trainz

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class DecompileAll extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args.length > 0 ? args[0] : "D:/trainz12-decomp/decomp");
		dir.mkdirs();
		File out = new File(dir, currentProgram.getName() + ".c");

		DecompInterface ifc = new DecompInterface();
		ifc.setOptions(new DecompileOptions());
		ifc.toggleCCode(true);
		ifc.toggleSyntaxTree(false);
		ifc.setSimplificationStyle("decompile");
		if (!ifc.openProgram(currentProgram)) {
			printerr("decompiler failed to open program: " + ifc.getLastMessage());
			return;
		}
		int ok = 0, failed = 0;
		try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
			FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
			while (it.hasNext() && !monitor.isCancelled()) {
				Function f = it.next();
				if (f.isThunk() || f.isExternal()) continue;
				w.printf("%n// ==== %s  %s  [%s, %d bytes]%n", f.getEntryPoint(), f.getName(true),
					f.getSymbol().getSource(), f.getBody().getNumAddresses());
				DecompileResults r = ifc.decompileFunction(f, 60, monitor);
				if (r != null && r.decompileCompleted()) {
					w.print(r.getDecompiledFunction().getC());
					ok++;
				} else {
					w.printf("// DECOMPILE FAILED: %s%n", r == null ? "null" : r.getErrorMessage());
					failed++;
				}
			}
		} finally {
			ifc.dispose();
		}
		println(String.format("DECOMP %s ok=%d failed=%d -> %s", currentProgram.getName(), ok, failed, out));
	}
}
