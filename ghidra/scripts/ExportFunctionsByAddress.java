// Disassemble and decompile Thumb functions supplied as address arguments.
// @category Nokia3210

import java.io.File;
import java.io.PrintWriter;
import java.math.BigInteger;

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.lang.Register;
import ghidra.program.model.lang.RegisterValue;
import ghidra.program.model.listing.Function;

public class ExportFunctionsByAddress extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 2)
			throw new IllegalArgumentException("usage: output-file address [address ...]");

		Register tmode = currentProgram.getProgramContext().getRegister("TMode");
		DecompInterface decompiler = new DecompInterface();
		decompiler.openProgram(currentProgram);

		try (PrintWriter output = new PrintWriter(new File(args[0]))) {
			for (int index = 1; index < args.length; index++) {
				Address address = toAddr(Long.decode(args[index]));
				DisassembleCommand command = new DisassembleCommand(
						new AddressSet(address, address), null, true);
				command.setInitialContext(new RegisterValue(tmode, BigInteger.ONE));
				command.applyTo(currentProgram, monitor);

				Function function = getFunctionAt(address);
				if (function == null)
					function = createFunction(address, "callback_" + address);
				output.println("/* ==== " + address + " " +
						(function == null ? "<no function>" : function.getName()) + " ==== */");
				if (function == null)
					continue;
				DecompileResults result = decompiler.decompileFunction(function, 120, monitor);
				if (!result.decompileCompleted())
					output.println("/* decompile failed: " + result.getErrorMessage() + " */");
				else
					output.println(result.getDecompiledFunction().getC());
			}
		}
		decompiler.dispose();
	}
}
