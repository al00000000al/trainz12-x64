// Names unnamed functions after the class whose vftable points at them, using the vftables that Ghidra's
// RTTI analyzer already labelled. A function in slot N of Class's vftable becomes Class::vfN (in the class
// namespace) when it still has a default FUN_ name. A function shared by several vftables (inherited, not
// overridden) keeps the first class found and gets a comment listing all of them.
// Writes a TSV of everything it named to the script argument path, if given.
//@category Trainz

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Namespace;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class NameVtableFunctions extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Map<Address, List<String>> owners = new LinkedHashMap<>();
		Map<Address, Namespace> firstNs = new LinkedHashMap<>();
		Map<Address, Integer> firstSlot = new LinkedHashMap<>();
		int tables = 0;

		SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
		while (it.hasNext() && !monitor.isCancelled()) {
			Symbol s = it.next();
			if (!s.getName().startsWith("vftable")) continue;
			Namespace cls = s.getParentNamespace();
			if (cls == null || cls.isGlobal()) continue;
			tables++;
			Address a = s.getAddress();
			for (int slot = 0; slot < 512; slot++) {
				Address entry = a.add(slot * 4L);
				// stop at the next labelled thing (next vftable / RTTI locator) after slot 0
				if (slot > 0 && currentProgram.getSymbolTable().getPrimarySymbol(entry) != null) break;
				long target;
				try {
					target = getInt(entry) & 0xffffffffL;
				} catch (Exception e) {
					break;
				}
				Address t = toAddr(target);
				MemoryBlock b = getMemoryBlock(t);
				if (b == null || !b.isExecute()) break;
				Function f = getFunctionAt(t);
				if (f == null) {
					disassemble(t);
					f = createFunction(t, null);
					if (f == null) continue;
				}
				String label = cls.getName(true) + "::vf" + slot;
				owners.computeIfAbsent(f.getEntryPoint(), k -> new ArrayList<>()).add(label);
				firstNs.putIfAbsent(f.getEntryPoint(), cls);
				firstSlot.putIfAbsent(f.getEntryPoint(), slot);
			}
		}

		int renamed = 0;
		PrintWriter w = getScriptArgs().length > 0 ? new PrintWriter(getScriptArgs()[0], "UTF-8") : null;
		if (w != null) w.println("entry\tnew_name\tall_vftable_slots");
		for (Map.Entry<Address, List<String>> e : owners.entrySet()) {
			Function f = getFunctionAt(e.getKey());
			if (f == null || f.getSymbol().getSource() != SourceType.DEFAULT) continue;
			Namespace ns = firstNs.get(e.getKey());
			String name = "vf" + firstSlot.get(e.getKey());
			f.setParentNamespace(ns);
			f.setName(name, SourceType.ANALYSIS);
			if (e.getValue().size() > 1) f.setComment("in vftables: " + String.join(", ", e.getValue()));
			renamed++;
			if (w != null) w.printf("%s\t%s::%s\t%s%n", e.getKey(), ns.getName(true), name, String.join(", ", e.getValue()));
		}
		if (w != null) w.close();
		println(String.format("VTNAMES %s vftables=%d functions_in_vftables=%d renamed=%d",
			currentProgram.getName(), tables, owners.size(), renamed));
	}
}
