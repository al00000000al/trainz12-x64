// Disassembles and creates functions at the given addresses (code reached only through data, e.g. tables of
// member-function pointers filled by static initialisers). args: <hex address> [...]
//@category Trainz

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;

public class CreateFunctionsAt extends GhidraScript {
	@Override
	protected void run() throws Exception {
		for (String s : getScriptArgs()) {
			Address a = toAddr(s);
			Function f = getFunctionAt(a);
			if (f == null) {
				disassemble(a);
				f = createFunction(a, null);
			}
			println("FUNC " + s + " -> " + (f == null ? "failed" : f.getName() + " " + f.getBody().getNumAddresses() + " bytes"));
		}
	}
}
