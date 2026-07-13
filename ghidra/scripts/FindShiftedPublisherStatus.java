// Find publisher calls preceded by a left shift into status register r0.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class FindShiftedPublisherStatus extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		Address publisher = toAddr(Long.decode(args.length > 0 ? args[0] : "0x2af798"));
		long shift = Long.decode(args.length > 1 ? args[1] : "3");
		for (Instruction call : currentProgram.getListing().getInstructions(true)) {
			boolean callsPublisher = false;
			for (Address flow : call.getFlows())
				if ((flow.getOffset() & ~1L) == (publisher.getOffset() & ~1L))
					callsPublisher = true;
			if (!callsPublisher)
				continue;

			Instruction cursor = call.getPrevious();
			StringBuilder context = new StringBuilder();
			boolean match = false;
			for (int i = 0; i < 18 && cursor != null; i++, cursor = cursor.getPrevious()) {
				context.insert(0, String.format("    %s %s%n", cursor.getAddress(), cursor));
				if (cursor.getMnemonicString().equalsIgnoreCase("lsl") && cursor.getNumOperands() >= 3 &&
						cursor.getDefaultOperandRepresentation(0).equals("r0")) {
					for (Object object : cursor.getOpObjects(2))
						if (object instanceof Scalar && ((Scalar)object).getUnsignedValue() == shift)
							match = true;
				}
			}
			if (match)
				println(String.format("shifted publisher call=%s shift=%d%n%s    %s", call.getAddress(), shift,
						context, call));
		}
	}
}
