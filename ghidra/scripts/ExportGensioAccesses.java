// Enumerate direct, literal-derived and scalar-candidate accesses to GENSIO registers.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;
import ghidra.program.model.symbol.Reference;

import java.util.HashSet;
import java.util.Set;

public class ExportGensioAccesses extends GhidraScript {
	private static final long IO_BASE = 0x00020000L;
	private static final int[] OFFSETS = {
		0x2c, 0x2d, 0x2e, 0x6c, 0x6d, 0x6e, 0x6f,
		0xad, 0xae, 0xaf, 0xed, 0xee, 0xef
	};

	private boolean isOffset(long value) {
		for (int offset : OFFSETS)
			if (value == offset)
				return true;
		return false;
	}

	private int offsetForAddress(long value) {
		long offset = value - IO_BASE;
		return isOffset(offset) ? (int)offset : -1;
	}

	private long effectiveWord(Address address) throws Exception {
		long word = getInt(address) & 0xffffffffL;
		return ((word & 0xffffL) << 16) | ((word >>> 16) & 0xffffL);
	}

	@Override
	protected void run() throws Exception {
		int instructions = 0;
		int resolved = 0;
		int candidates = 0;
		Set<String> emitted = new HashSet<>();

		for (Instruction instruction : currentProgram.getListing().getInstructions(true)) {
			instructions++;
			Function function = getFunctionContaining(instruction.getAddress());
			String owner = function == null ? "<none>" : function.getName();

			for (Reference reference : instruction.getReferencesFrom()) {
				if (!reference.isMemoryReference())
					continue;
				Address target = reference.getToAddress();
				int offset = offsetForAddress(target.getOffset());
				String kind = "direct";
				if (offset < 0 && currentProgram.getMemory().contains(target) &&
						target.add(3).compareTo(currentProgram.getMemory().getMaxAddress()) <= 0) {
					offset = offsetForAddress(effectiveWord(target));
					kind = "literal";
				}
				if (offset >= 0) {
					String key = instruction.getAddress() + ":" + offset + ":" + kind;
					if (emitted.add(key)) {
						printf("resolved,%s,0x%02x,%s,%s,%s\n", kind, offset,
								instruction.getAddress(), owner, instruction);
						resolved++;
					}
				}
			}

			for (int operand = 0; operand < instruction.getNumOperands(); operand++) {
				for (Object object : instruction.getOpObjects(operand)) {
					if (!(object instanceof Scalar))
						continue;
					long value = ((Scalar)object).getUnsignedValue();
					if (!isOffset(value))
						continue;
					String key = instruction.getAddress() + ":" + value + ":scalar";
					if (emitted.add(key)) {
						printf("candidate,scalar,0x%02x,%s,%s,%s\n", value,
								instruction.getAddress(), owner, instruction);
						candidates++;
					}
				}
			}
		}

		printf("coverage,instructions=%d,resolved=%d,scalar_candidates=%d\n",
				instructions, resolved, candidates);
	}
}
