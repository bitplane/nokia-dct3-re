// Find calls to a publisher whose first argument register was recently assigned a requested scalar.
// @category Nokia3210

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.scalar.Scalar;

public class FindPublisherArgument extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		Address publisher = toAddr(Long.decode(args.length > 0 ? args[0] : "0x2af798"));
		long wanted = Long.decode(args.length > 1 ? args[1] : "7");
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
			for (int i = 0; i < 14 && cursor != null; i++, cursor = cursor.getPrevious()) {
				context.insert(0, String.format("    %s %s%n", cursor.getAddress(), cursor));
				if (cursor.getNumOperands() > 0 && cursor.getDefaultOperandRepresentation(0).equals("r1")) {
					for (int operand = 1; operand < cursor.getNumOperands(); operand++)
						for (Object object : cursor.getOpObjects(operand))
							if (object instanceof Scalar && ((Scalar)object).getUnsignedValue() == wanted)
								match = true;
					break;
				}
			}
			if (match)
				println(String.format("publisher argument call=%s r1=%x%n%s    %s", call.getAddress(), wanted,
						context, call));
		}
	}
}
