// For each string given as script argument, print where it is referenced and the instructions around the
// reference (or the data around it), to learn how GS native names are bound to their implementations.
//@category Trainz

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;
import ghidra.program.util.string.FoundString;

public class ProbeStringRefs extends GhidraScript {
	@Override
	protected void run() throws Exception {
		// an argument "@file" means: one string per line from that file (cmd mangles quotes and parentheses)
		java.util.List<String> wantedList = new java.util.ArrayList<>();
		for (String a : getScriptArgs()) {
			if (a.startsWith("@")) wantedList.addAll(java.nio.file.Files.readAllLines(java.nio.file.Path.of(a.substring(1))));
			else wantedList.add(a);
		}
		for (String wanted : wantedList) {
			if (wanted.isBlank()) continue;
			// escape the regex metacharacters that occur in GS signatures like "$string@Asset::GetCreatorName()"
			String re = wanted.replaceAll("([$()\\[\\].,*+?|^])", "\\\\$1");
			Address[] hits = findBytes(currentProgram.getMinAddress(), re + "\\x00", 5);
			println("### " + wanted + " : " + hits.length + " hits");
			for (Address s : hits) {
				Reference[] refs = getReferencesTo(s);
				println("  string at " + s + ", refs=" + refs.length);
				for (Reference r : refs) {
					Address from = r.getFromAddress();
					Function f = getFunctionContaining(from);
					println("    ref from " + from + " in " + (f == null ? "<no function>" : f.getName(true) + "@" + f.getEntryPoint()));
					Instruction ins = getInstructionAt(from);
					if (ins != null) {
						Instruction p = ins;
						for (int i = 0; i < 6 && p.getPrevious() != null; i++) p = p.getPrevious();
						for (int i = 0; i < 14 && p != null; i++, p = p.getNext())
							println("      " + p.getAddress() + (p.getAddress().equals(from) ? " >> " : "    ") + p);
					} else {
						Data d = getDataContaining(from);
						for (int k = -8; k <= 16; k += 4) {
							Address a = from.add(k);
							println("      " + a + "  " + String.format("%08x", getInt(a)));
						}
					}
				}
			}
		}
	}
}
