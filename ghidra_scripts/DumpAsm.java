// Writes the disassembly of the functions at the given addresses (address, bytes, instruction).
// args: <out file> <hex address> [hex address ...]
//@category Trainz

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.InstructionIterator;

public class DumpAsm extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		try (PrintWriter w = new PrintWriter(args[0], "UTF-8")) {
			for (int i = 1; i < args.length; i++) {
				Function f = getFunctionContaining(toAddr(args[i]));
				if (f == null) {
					w.println("; no function at " + args[i]);
					continue;
				}
				w.printf("%n; ==== %s %s%n", f.getEntryPoint(), f.getName(true));
				AddressSetView body = f.getBody();
				InstructionIterator it = currentProgram.getListing().getInstructions(body, true);
				while (it.hasNext()) {
					Instruction ins = it.next();
					StringBuilder hex = new StringBuilder();
					for (byte b : ins.getBytes()) hex.append(String.format("%02x", b));
					// show strings and named targets the instruction points at
					StringBuilder note = new StringBuilder();
					for (ghidra.program.model.symbol.Reference r : ins.getReferencesFrom()) {
						ghidra.program.model.listing.Data d = getDataAt(r.getToAddress());
						if (d != null && d.hasStringValue()) {
							note.append(" \"").append(d.getDefaultValueRepresentation()).append('"');
						} else {
							ghidra.program.model.symbol.Symbol s = getSymbolAt(r.getToAddress());
							if (s != null) note.append(" <").append(s.getName(true)).append('>');
						}
					}
					w.printf("%s  %-20s %s%s%n", ins.getAddress(), hex, ins, note.length() > 0 ? "   ;" + note : "");
				}
			}
		}
		println("ASMDUMP done");
	}
}
