// Print references to selected addresses.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.Reference;

public class FindRefs extends GhidraScript {
	private static final long[] ADDRS = new long[] {
		0x0028fe7aL,
		0x0028ff0eL,
		0x0028ff14L,
		0x0028ff38L,
		0x002900a0L,
		0x002900bcL,
		0x00290170L,
		0x00290208L,
		0x002904c0L,
		0x002904d4L,
		0x0026ec10L,
		0x0026ec5cL,
	};

	@Override
	protected void run() throws Exception {
		for (long raw : ADDRS) {
			Address to = toAddr(raw);
			Function target = getFunctionContaining(to);
			printf("REF_TARGET addr=%s fn=%s\n", to, target == null ? "<none>" : target.getName());
			int count = 0;
			for (Reference ref : getReferencesTo(to)) {
				count++;
				Function fromFn = getFunctionContaining(ref.getFromAddress());
				printf("  from=%s type=%s fn=%s\n",
						ref.getFromAddress(),
						ref.getReferenceType(),
						fromFn == null ? "<none>" : fromFn.getName());
			}
			printf("  ref_count=%d\n", count);
		}
	}
}
