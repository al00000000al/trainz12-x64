// Decompile the functions containing the given addresses into one file.
// args: <out file> <hex address> [hex address ...]
//@category Trainz

import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class DecompileAt extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		DecompInterface ifc = new DecompInterface();
		ifc.setOptions(new DecompileOptions());
		ifc.openProgram(currentProgram);
		try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
			for (int i = 1; i < args.length; i++) {
				Function f = getFunctionContaining(toAddr(args[i]));
				if (f == null) {
					w.println("// no function at " + args[i]);
					continue;
				}
				w.printf("%n// ==== %s  %s%n", f.getEntryPoint(), f.getName(true));
				DecompileResults r = ifc.decompileFunction(f, 300, monitor);
				w.print(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// FAILED " + r.getErrorMessage());
			}
		} finally {
			ifc.dispose();
		}
	}
}
