// Lists every reference into an address range: from-address, reference type, containing function.
// args: <out file> <start hex> <length hex>
//@category Trainz

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class XrefsTo extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] a = getScriptArgs();
		Address start = toAddr(a[1]);
		long len = Long.parseLong(a[2], 16);
		try (PrintWriter w = new PrintWriter(a[0], "UTF-8")) {
			for (long off = 0; off < len; off++) {
				Address t = start.add(off);
				for (Reference r : getReferencesTo(t)) {
					Function f = getFunctionContaining(r.getFromAddress());
					w.printf("%s <- %s %s in %s%n", t, r.getFromAddress(), r.getReferenceType(),
						f == null ? "?" : f.getName(true) + "@" + f.getEntryPoint());
				}
			}
		}
		println("XREFS done");
	}
}
