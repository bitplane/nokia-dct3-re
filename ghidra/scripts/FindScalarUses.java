// Find instructions that directly use, or reference ROM data equal to, a scalar value.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

public class FindScalarUses extends GhidraScript {
	@Override
	protected void run() throws Exception {
		long wanted = Long.decode(getScriptArgs().length > 0 ? getScriptArgs()[0] : "0x0578");
		long lower = getScriptArgs().length > 1 ? Long.decode(getScriptArgs()[1]) : Long.MIN_VALUE;
		long upper = getScriptArgs().length > 2 ? Long.decode(getScriptArgs()[2]) : Long.MAX_VALUE;
		if (wanted > 0xffff) {
			for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
				if (!block.isInitialized())
					continue;
				long start = block.getStart().getOffset();
				long end = block.getEnd().getOffset();
				for (long raw = (start + 1) & ~1L; raw + 3 <= end; raw += 2) {
					Address address = toAddr(raw);
					long word = getInt(address) & 0xffffffffL;
					long swapped = ((word & 0xffffL) << 16) | ((word >>> 16) & 0xffffL);
					if (word == wanted || swapped == wanted)
						println(String.format("pointer value=%08x at %s encoding=%s", wanted,
								address, word == wanted ? "native" : "swap16"));
				}
			}
		}
		for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
			long instructionAddress = instruction.getAddress().getOffset();
			if (instructionAddress < lower || instructionAddress >= upper)
				continue;
			boolean match = false;
			for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
				for (Object object : instruction.getOpObjects(operand)) {
					if (object instanceof Scalar && (((Scalar)object).getUnsignedValue() & 0xffff) == wanted)
						match = true;
				}
			}
			for (Reference reference : instruction.getReferencesFrom()) {
				if (!reference.isMemoryReference())
					continue;
				Address target = reference.getToAddress();
				if (!currentProgram.getMemory().contains(target))
					continue;
				long halfword = getShort(target) & 0xffffL;
				long word = getInt(target) & 0xffffffffL;
				// The normalized DCT3 image swaps 16-bit bus words. Accept either word order.
				long swappedWord = ((word & 0xffff) << 16) | ((word >>> 16) & 0xffff);
				if (halfword == wanted || word == wanted || swappedWord == wanted)
					match = true;
			}
			if (match) {
				Instruction previous = instruction.getPrevious();
				Instruction next = instruction.getNext();
				println(String.format("use value=%04x at %s%n    %s%n    %s%n    %s", wanted,
						instruction.getAddress(), previous == null ? "<none>" : previous,
						instruction, next == null ? "<none>" : next));
			}
		}
	}
}
