// Count functions known to the current Ghidra program.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;

public class CountFunctions extends GhidraScript {
	@Override
	protected void run() throws Exception {
		int total = 0;
		int flash1 = 0;
		int flash2 = 0;
		int named = 0;
		FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
		for (Function function : functions) {
			total++;
			Address entry = function.getEntryPoint();
			long offset = entry.getOffset();
			if (offset >= 0x00200000L && offset < 0x00600000L) {
				flash1++;
			} else if (offset >= 0x00600000L && offset < 0x00a00000L) {
				flash2++;
			}
			if (!function.getName().startsWith("FUN_") && !function.getName().startsWith("SUB_")) {
				named++;
			}
		}
		printf("NOKIA3210_FUNCTION_COUNT total=%d flash1=%d flash2=%d named=%d\n", total, flash1, flash2, named);
	}
}
