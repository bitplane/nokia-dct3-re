// List functions and direct references in an address interval.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class ListFunctionsInRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		Address start = toAddr(Long.decode(args.length > 0 ? args[0] : "0x255000"));
		Address end = toAddr(Long.decode(args.length > 1 ? args[1] : "0x256000"));
		for (Function function : currentProgram.getFunctionManager().getFunctions(start, true)) {
			if (function.getEntryPoint().compareTo(end) >= 0)
				break;
			println(String.format("function=%s entry=%s end=%s", function.getName(), function.getEntryPoint(),
					function.getBody().getMaxAddress()));
			for (Reference reference : getReferencesTo(function.getEntryPoint()))
				println(String.format("  caller=%s type=%s", reference.getFromAddress(), reference.getReferenceType()));
		}
	}
}
