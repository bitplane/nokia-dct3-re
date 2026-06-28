// Locate selected ASCII strings and print references to their addresses.
// @category Nokia3210

import java.nio.charset.StandardCharsets;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSetView;
import ghidra.program.model.listing.Function;
import ghidra.program.model.mem.Memory;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;

public class FindStringRefs extends GhidraScript {
	private static final String[] NEEDLES = new String[] {
		"NOKIA",
		"MENU",
		"Snake",
		"Welcome",
		"Insert SIM",
	};

	@Override
	protected void run() throws Exception {
		Memory memory = currentProgram.getMemory();
		for (String needle : NEEDLES) {
			byte[] bytes = needle.getBytes(StandardCharsets.US_ASCII);
			printf("STRING_ANCHOR needle=\"%s\"\n", needle);
			int found = 0;
			for (MemoryBlock block : memory.getBlocks()) {
				Address start = block.getStart();
				Address end = block.getEnd();
				Address cursor = start;
				while (cursor != null && cursor.compareTo(end) <= 0) {
					Address hit = memory.findBytes(cursor, end, bytes, null, true, monitor);
					if (hit == null) {
						break;
					}
					found++;
					printf("  hit addr=%s offset=%08x\n", hit, hit.getOffset());
					int refs = 0;
					for (Reference ref : getReferencesTo(hit)) {
						refs++;
						Function fn = getFunctionContaining(ref.getFromAddress());
						printf("    ref from=%s type=%s fn=%s\n",
								ref.getFromAddress(),
								ref.getReferenceType(),
								fn == null ? "<none>" : fn.getName());
					}
					if (refs == 0) {
						printf("    refs=0\n");
					}
					cursor = hit.add(bytes.length);
					if (found >= 20) {
						printf("  truncated after 20 hits\n");
						break;
					}
				}
				if (found >= 20) {
					break;
				}
			}
			printf("  total_hits_shown=%d\n", found);
		}
	}
}
