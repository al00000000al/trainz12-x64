// Per function: counts of x87 / MMX / SSE / CPUID / RDTSC / FS: / LOCK instructions.
// args: <out file>
//@category Trainz

import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;

public class InsnStats extends GhidraScript {
	@Override
	protected void run() throws Exception {
		try (PrintWriter w = new PrintWriter(getScriptArgs()[0], "UTF-8")) {
			w.println("entry\tns\tname\tsize\tx87\tmmx\tsse\tcpuid\trdtsc\tfs\tlock\tint3\tfstcw");
			FunctionIterator it = currentProgram.getFunctionManager().getFunctions(true);
			while (it.hasNext()) {
				Function f = it.next();
				if (f.isThunk()) continue;
				int x87 = 0, mmx = 0, sse = 0, cpuid = 0, rdtsc = 0, fs = 0, lock = 0, int3 = 0, fcw = 0;
				InstructionIterator ii = currentProgram.getListing().getInstructions(f.getBody(), true);
				while (ii.hasNext()) {
					Instruction in = ii.next();
					String m = in.getMnemonicString().toUpperCase();
					String txt = in.toString().toUpperCase();
					if (m.startsWith("F") && !m.equals("FS")) { x87++; if (m.equals("FNSTCW") || m.equals("FLDCW") || m.equals("FSTCW")) fcw++; }
					else if (m.equals("CPUID")) cpuid++;
					else if (m.equals("RDTSC")) rdtsc++;
					else if (m.equals("INT3")) int3++;
					else if (txt.contains("XMM") ) sse++;
					else if (txt.matches(".*\bMM[0-7]\b.*") || m.equals("EMMS")) mmx++;
					if (txt.contains("FS:")) fs++;
					if (txt.startsWith("LOCK")) lock++;
				}
				if (x87 + mmx + sse + cpuid + rdtsc + fs + lock + int3 == 0) continue;
				w.printf("%s\t%s\t%s\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d%n", f.getEntryPoint(), f.getParentNamespace().getName(true), f.getName(), f.getBody().getNumAddresses(), x87, mmx, sse, cpuid, rdtsc, fs, lock, int3, fcw);
			}
		}
		println("InsnStats done");
	}
}
