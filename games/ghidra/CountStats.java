// Print function count and named/unnamed split for the current program.
// @category Nokia3210
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;
public class CountStats extends GhidraScript {
	@Override
	protected void run() throws Exception {
		int total = 0, named = 0;
		for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
			total++;
			if (!fn.getName().startsWith("FUN_") && !fn.getName().startsWith("thunk_FUN_")) named++;
		}
		printf("STATS functions=%d named=%d\n", total, named);
	}
}
