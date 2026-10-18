// Maps every TrainzScript signature string in trainz.exe to its C++ handler and names the handler.
//
// Natives are registered as: signature string ("$float@Vehicle::GetMass()") interned via PStringCache, then a
// handler address passed on (PUSH imm in the Asset registrar, MOV reg,imm in the Vehicle one), then a register
// call. So for every reference to a "$type@Class::Method(...)" string we scan forward, until the next signature
// reference, for the first immediate that points into code: that is the handler.
// References with no code immediate are engine -> script calls (callbacks such as Vehicle::NotifyTrainChanged).
//
// args: <out tsv>
//@category Trainz

import java.io.PrintWriter;
import java.util.HashSet;
import java.util.Set;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.SourceType;

public class MapGsNatives extends GhidraScript {
	private static final String SIG_RE =
		"\\$[A-Za-z_][A-Za-z0-9_\\[\\]]*@[A-Za-z_][A-Za-z0-9_]*::[A-Za-z_][A-Za-z0-9_]*\\(";

	@Override
	protected void run() throws Exception {
		String out = getScriptArgs()[0];
		Set<Address> sigStrings = new HashSet<>();
		for (MemoryBlock b : currentProgram.getMemory().getBlocks()) {
			if (b.isExecute() || !b.isInitialized()) continue;
			for (Address s : findBytes(b.getStart(), SIG_RE, 100000))
				if (b.contains(s)) sigStrings.add(s);
		}

		int natives = 0, callbacks = 0, renamed = 0, refsTotal = 0;
		try (PrintWriter w = new PrintWriter(out, "UTF-8")) {
			w.println("kind\tsignature\tclass_method\thandler\tref_at\tin_function");
			for (Address s : sigStrings) {
				String sig = readCString(s);
				if (sig == null) continue;
				String classMethod = sig.substring(sig.indexOf('@') + 1, sig.indexOf('('));
				for (Reference r : getReferencesTo(s)) {
					refsTotal++;
					Address from = r.getFromAddress();
					Function owner = getFunctionContaining(from);
					Address handler = findHandler(from, sigStrings);
					String ownerS = owner == null ? "?" : owner.getName() + "@" + owner.getEntryPoint();
					if (handler == null) {
						callbacks++;
						w.printf("callback\t%s\t%s\t\t%s\t%s%n", sig, classMethod, from, ownerS);
						continue;
					}
					natives++;
					w.printf("native\t%s\t%s\t%s\t%s\t%s%n", sig, classMethod, handler, from, ownerS);
					Function f = getFunctionAt(handler);
					if (f == null) {
						disassemble(handler);
						f = createFunction(handler, null);
					}
					if (f != null && f.getSymbol().getSource() == SourceType.DEFAULT) {
						f.setName("gs_" + classMethod.replace("::", "_"), SourceType.ANALYSIS);
						f.setComment("TrainzScript native: " + sig);
						renamed++;
					}
				}
			}
		}
		println(String.format("GSMAP signature_strings=%d refs=%d natives=%d callbacks=%d renamed=%d",
			sigStrings.size(), refsTotal, natives, callbacks, renamed));
	}

	/** first immediate pointing into code after `from`, stopping at the next signature reference */
	private Address findHandler(Address from, Set<Address> sigStrings) {
		Instruction ins = getInstructionAt(from);
		if (ins == null) return null;
		ins = ins.getNext();
		for (int i = 0; i < 40 && ins != null; i++, ins = ins.getNext()) {
			if (ins.getFlowType().isTerminal()) return null;
			for (int op = 0; op < ins.getNumOperands(); op++) {
				for (Object o : ins.getOpObjects(op)) {
					if (!(o instanceof Scalar)) continue;
					long v = ((Scalar) o).getUnsignedValue();
					if (v < 0x400000L) continue;
					Address a = toAddr(v);
					if (sigStrings.contains(a)) return null;  // next registration starts
					MemoryBlock b = getMemoryBlock(a);
					if (b != null && b.isExecute() && !ins.getFlowType().isCall()) return a;
				}
			}
		}
		return null;
	}

	private String readCString(Address a) {
		try {
			StringBuilder sb = new StringBuilder();
			for (int i = 0; i < 400; i++) {
				byte c = getByte(a.add(i));
				if (c == 0) return sb.toString();
				if (c < 0x20 || c > 0x7e) return null;
				sb.append((char) c);
			}
		} catch (Exception e) {
			// unreadable address
		}
		return null;
	}
}
