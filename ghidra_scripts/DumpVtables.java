// Prints every slot of the vftables whose class name matches a regex: slot, target address, RVA, function name,
// size. Stops a table at the next labelled address or at the first non-code pointer.
// args: <class regex> [out file]
//@category Trainz

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.regex.Pattern;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class DumpVtables extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Pattern cls = Pattern.compile(getScriptArgs()[0]);
		long base = currentProgram.getImageBase().getOffset();
		StringWriter sw = new StringWriter();
		PrintWriter w = new PrintWriter(sw);
		SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
		while (it.hasNext()) {
			Symbol s = it.next();
			if (!s.getName().startsWith("vftable") || s.getParentNamespace().isGlobal()) continue;
			String owner = s.getParentNamespace().getName(true);
			if (!cls.matcher(owner).find()) continue;
			w.printf("== %s::%s at %s (rva 0x%x)%n", owner, s.getName(), s.getAddress(), s.getAddress().getOffset() - base);
			for (int slot = 0; slot < 256; slot++) {
				Address e = s.getAddress().add(slot * 4L);
				if (slot > 0 && currentProgram.getSymbolTable().getPrimarySymbol(e) != null) break;
				Address t = toAddr(getInt(e) & 0xffffffffL);
				MemoryBlock b = getMemoryBlock(t);
				if (b == null || !b.isExecute()) break;
				Function f = getFunctionAt(t);
				w.printf("  [%2d] +0x%02x  %s  rva 0x%05x  %s  %d bytes%n", slot, slot * 4, t, t.getOffset() - base,
					f == null ? "?" : f.getName(true), f == null ? 0 : f.getBody().getNumAddresses());
			}
		}
		w.flush();
		if (getScriptArgs().length > 1) {
			try (PrintWriter out = new PrintWriter(getScriptArgs()[1], "UTF-8")) {
				out.print(sw);
			}
		}
		println("VTDUMP done");
	}
}
