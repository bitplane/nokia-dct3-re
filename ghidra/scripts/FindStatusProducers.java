// Find calls to a status publisher whose nearby instructions load a requested value.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class FindStatusProducers extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		Address publisher = toAddr(Long.decode(args.length > 0 ? args[0] : "0x2af798"));
		long wanted = Long.decode(args.length > 1 ? args[1] : "0x1989");
		for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
			boolean callsPublisher = false;
			for (Address flow : instruction.getFlows()) {
				long normalizedFlow = flow.getOffset() & ~1L;
				if (normalizedFlow == (publisher.getOffset() & ~1L))
					callsPublisher = true;
			}
			if (!callsPublisher)
				continue;
			Instruction cursor = instruction;
			StringBuilder context = new StringBuilder();
			boolean match = wanted < 0;
			for (int i = 0; i < 12 && cursor != null; i++, cursor = cursor.getPrevious()) {
				context.insert(0, String.format("    %s %s%n", cursor.getAddress(), cursor));
				for (int operand = 0; operand < cursor.getNumOperands(); operand++) {
					for (Object object : cursor.getOpObjects(operand)) {
						if (object instanceof Scalar) {
							long scalar = ((Scalar)object).getUnsignedValue();
							if (scalar <= 0xffff && (scalar & 0x1fff) == (wanted & 0x1fff))
								match = true;
						}
					}
				}
				for (ghidra.program.model.symbol.Reference reference : cursor.getReferencesFrom()) {
					if (!reference.isMemoryReference())
						continue;
					Address target = reference.getToAddress();
					if (currentProgram.getMemory().contains(target)) {
						long halfword = getShort(target) & 0xffffL;
						long word = getInt(target) & 0xffffffffL;
						long swappedWord = ((word & 0xffff) << 16) | ((word >>> 16) & 0xffff);
						if ((halfword & 0x1fff) == (wanted & 0x1fff) ||
								(word & 0x1fff) == (wanted & 0x1fff) ||
								((word >>> 16) & 0x1fff) == (wanted & 0x1fff) ||
								(swappedWord & 0x1fff) == (wanted & 0x1fff))
							match = true;
					}
				}
			}
			if (match)
				println(String.format("producer call=%s value=%04x%n%s", instruction.getAddress(), wanted, context));
		}
	}
}
