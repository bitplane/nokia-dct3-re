// Export references to the Nokia 3210 scheduler post routine.
// @category Nokia3210

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class ExportSchedulerRefs extends GhidraScript {
	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		File outFile = new File(args.length > 0 ? args[0] : "/tmp/scheduler_refs.txt");
		Address target = toAddr(0x002697aaL);

		try (PrintWriter writer = new PrintWriter(outFile)) {
			writer.printf("References to %s%n", target);
			for (Reference ref : getReferencesTo(target)) {
				Address from = ref.getFromAddress();
				Function function = getFunctionContaining(from);
				writer.printf("%s %s %s%n",
						from,
						ref.getReferenceType(),
						function == null ? "<no-function>" : function.getName() + "@" + function.getEntryPoint());
				Instruction ins = getInstructionAt(from);
				for (int i = 0; i < 8 && ins != null; i++) {
					ins = ins.getPrevious();
				}
				for (int i = 0; i < 12 && ins != null; i++) {
					writer.printf("  %s  %s%n", ins.getAddress(), ins);
					if (ins.getAddress().equals(from)) {
						break;
					}
					ins = ins.getNext();
				}
			}
		}
		printf("exported %s\n", outFile.getAbsolutePath());
	}
}
