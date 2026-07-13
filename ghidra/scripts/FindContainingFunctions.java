// Report containing functions and callers for supplied firmware addresses.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class FindContainingFunctions extends GhidraScript {
	@Override
	protected void run() throws Exception {
		for (String arg : getScriptArgs()) {
			long raw = Long.decode(arg);
			Address address = toAddr(raw);
			if (currentProgram.getMemory().contains(address))
				println(String.format("value=%08x bytes=%02x %02x %02x %02x short=%04x int=%08x", raw,
						getByte(address) & 0xff, getByte(address.add(1)) & 0xff,
						getByte(address.add(2)) & 0xff, getByte(address.add(3)) & 0xff,
						getShort(address) & 0xffff, getInt(address)));
			Function function = getFunctionContaining(address);
			println(String.format("address=%08x function=%s entry=%s", raw,
					function == null ? "<none>" : function.getName(),
					function == null ? "<none>" : function.getEntryPoint()));
			if (function != null) {
				for (Reference reference : getReferencesTo(function.getEntryPoint()))
					println(String.format("  caller=%s type=%s", reference.getFromAddress(),
							reference.getReferenceType()));
			}
			for (Reference reference : getReferencesTo(address)) {
				Function owner = getFunctionContaining(reference.getFromAddress());
				println(String.format("  direct-ref=%s type=%s owner=%s entry=%s",
						reference.getFromAddress(), reference.getReferenceType(),
						owner == null ? "<none>" : owner.getName(),
						owner == null ? "<none>" : owner.getEntryPoint()));
			}
		}
	}
}
