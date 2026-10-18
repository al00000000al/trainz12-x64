// For every function that still has a default FUN_ name: its size, named callers, named callees and the
// strings it references. Neighbours show which subsystem an unnamed helper belongs to.
// args: <out tsv>
//@category Trainz

import java.io.PrintWriter;
import java.util.LinkedHashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.data.StringDataInstance;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;

public class NeighboursOfUnnamed extends GhidraScript {
	@Override
	protected void run() throws Exception {
		try (PrintWriter w = new PrintWriter(getScriptArgs()[0], "UTF-8")) {
			w.println("entry\tbytes\tcallers\tcallees\tstrings");
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				if (f.isThunk() || f.getSymbol().getSource() != SourceType.DEFAULT) continue;
				Set<String> callers = new LinkedHashSet<>(), callees = new LinkedHashSet<>(), strings = new LinkedHashSet<>();
				for (Function c : f.getCallingFunctions(monitor)) callers.add(c.getName(true));
				for (Function c : f.getCalledFunctions(monitor)) callees.add(c.getName(true));
				InstructionIterator it = currentProgram.getListing().getInstructions(f.getBody(), true);
				while (it.hasNext()) {
					Instruction ins = it.next();
					for (Reference r : ins.getReferencesFrom()) {
						Data d = getDataAt(r.getToAddress());
						if (d != null && d.hasStringValue()) {
							String s = StringDataInstance.getStringDataInstance(d).getStringValue();
							if (s != null) strings.add(s.replace('\t', ' ').replace('\n', ' '));
						}
					}
				}
				w.printf("%s\t%d\t%s\t%s\t%s%n", f.getEntryPoint(), f.getBody().getNumAddresses(),
					String.join(", ", callers), String.join(", ", callees), String.join(" | ", strings));
			}
		}
		println("NEIGHBOURS done");
	}
}
