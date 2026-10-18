// Writes <out dir>/<program>.functions.tsv (entry, size, namespace, name, source) and prints naming stats.
// Out dir comes from the script argument, default D:/trainz12-decomp/out.
//@category Trainz

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.symbol.SourceType;

public class ExportFunctionStats extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File dir = new File(args.length > 0 ? args[0] : "D:/trainz12-decomp/out");
		dir.mkdirs();
		File out = new File(dir, currentProgram.getName() + ".functions.tsv");
		int total = 0, named = 0, thunks = 0;
		long bytes = 0;
		try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
			w.println("entry\tsize\tnamespace\tname\tsource\tthunk");
			FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
			while (it.hasNext() && !monitor.isCancelled()) {
				Function f = it.next();
				long size = f.getBody().getNumAddresses();
				boolean isNamed = f.getSymbol().getSource() != SourceType.DEFAULT;
				total++;
				bytes += size;
				if (isNamed) named++;
				if (f.isThunk()) thunks++;
				w.printf("%s\t%d\t%s\t%s\t%s\t%b%n", f.getEntryPoint(), size,
					f.getParentNamespace().getName(true), f.getName(), f.getSymbol().getSource(), f.isThunk());
			}
		}
		println(String.format("STATS %s functions=%d named=%d (%.1f%%) thunks=%d code_bytes=%d -> %s",
			currentProgram.getName(), total, named, total == 0 ? 0.0 : 100.0 * named / total, thunks, bytes, out));
	}
}
