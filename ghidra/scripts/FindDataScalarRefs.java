// Find instructions that reference ROM data containing a scalar value.
// Code-flow references are excluded so a matching instruction encoding cannot
// masquerade as a literal-pool use.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.symbol.Reference;

public class FindDataScalarRefs extends GhidraScript {
	@Override
	protected void run() throws Exception {
		long wanted = Long.decode(getScriptArgs().length > 0 ? getScriptArgs()[0] : "0x2f02");
		for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
			for (Reference reference : instruction.getReferencesFrom()) {
				if (!reference.isMemoryReference() || reference.getReferenceType().isFlow())
					continue;
				Address target = reference.getToAddress();
				if (!currentProgram.getMemory().contains(target))
					continue;
				long halfword = getShort(target) & 0xffffL;
				long word = getInt(target) & 0xffffffffL;
				long swapped = ((word & 0xffffL) << 16) | ((word >>> 16) & 0xffffL);
				if (halfword != wanted && word != wanted && swapped != wanted)
					continue;
				Function owner = getFunctionContaining(instruction.getAddress());
				println(String.format(
						"value=%04x use=%s target=%s halfword=%04x word=%08x owner=%s entry=%s",
						wanted, instruction.getAddress(), target, halfword, word,
						owner == null ? "<none>" : owner.getName(),
						owner == null ? "<none>" : owner.getEntryPoint()));
			}
		}
	}
}
