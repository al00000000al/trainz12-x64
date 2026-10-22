// Dumps per-function call edges and data references (for module-splitting analysis).
// args: <out dir>
//@category Trainz

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.*;

public class ClassGraph extends GhidraScript {
	@Override
	protected void run() throws Exception {
		File dir = new File(getScriptArgs()[0]);
		dir.mkdirs();
		try (PrintWriter c = new PrintWriter(new File(dir, "calls.tsv"), "UTF-8");
			PrintWriter d = new PrintWriter(new File(dir, "datarefs.tsv"), "UTF-8");
			PrintWriter s = new PrintWriter(new File(dir, "datasyms.tsv"), "UTF-8")) {
			c.println("caller\tcaller_ns\tcaller_name\tsize\tcallee\tcallee_ns\tcallee_name\tkind");
			d.println("func\tfunc_ns\tfunc_name\tdata_addr\tblock\ttype\tsym");
			FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
			while (it.hasNext()) {
				Function f = it.next();
				if (f.isThunk()) continue;
				InstructionIterator ii = currentProgram.getListing().getInstructions(f.getBody(), true);
				long size = f.getBody().getNumAddresses();
				String ns = f.getParentNamespace().getName(true);
				while (ii.hasNext()) {
					Instruction in = ii.next();
					for (Reference r : in.getReferencesFrom()) {
						Address to = r.getToAddress();
						if (r.getReferenceType().isCall() || r.getReferenceType().isJump() && !f.getBody().contains(to) && r.getReferenceType().isComputed() == false && getFunctionAt(to) != null) {
							Function g = getFunctionAt(to);
							if (g != null && g.isThunk() && g.getThunkedFunction(true) != null) g = g.getThunkedFunction(true);
							String gn = g == null ? "?" : g.getName();
							String gns = g == null ? "?" : g.getParentNamespace().getName(true);
							String kind = r.getReferenceType().isCall() ? "call" : "tail";
							if (to.isExternalAddress() || (g != null && g.isExternal())) kind += "-ext";
							c.printf("%s\t%s\t%s\t%d\t%s\t%s\t%s\t%s%n", f.getEntryPoint(), ns, f.getName(), size, to, gns, gn, kind);
						} else if (to.isMemoryAddress()) {
							MemoryBlock b = currentProgram.getMemory().getBlock(to);
							if (b == null) continue;
							String bn = b.getName();
							if (bn.equals(".text")) continue;
							Symbol sy = currentProgram.getSymbolTable().getPrimarySymbol(to);
							d.printf("%s\t%s\t%s\t%s\t%s\t%s\t%s%n", f.getEntryPoint(), ns, f.getName(), to, bn, r.getReferenceType(), sy == null ? "" : sy.getName(true));
						}
					}
				}
			}
			SymbolIterator si = currentProgram.getSymbolTable().getAllSymbols(true);
			while (si.hasNext()) {
				Symbol y = si.next();
				Address a = y.getAddress();
				if (!a.isMemoryAddress()) continue;
				MemoryBlock b = currentProgram.getMemory().getBlock(a);
				if (b == null || b.getName().equals(".text")) continue;
				s.printf("%s\t%s\t%s\t%s%n", a, b.getName(), y.getSymbolType(), y.getName(true));
			}
		}
		println("ClassGraph done");
	}
}
