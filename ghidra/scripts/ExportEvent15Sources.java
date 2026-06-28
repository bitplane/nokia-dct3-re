// Export Nokia 3210 startup-event candidate functions.
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
import ghidra.program.model.symbol.SourceType;

public class ExportEvent15Sources extends GhidraScript {
	private static class Target {
		final String name;
		final long addr;

		Target(String name, long addr) {
			this.name = name;
			this.addr = addr;
		}
	}

	private static final Target[] TARGETS = new Target[] {
		new Target("event15_source_249470", 0x00249470L),
		new Target("event15_source_276b66", 0x00276b66L),
		new Target("event15_source_2772a4", 0x002772a4L),
		new Target("event15_source_27765c", 0x0027765cL),
		new Target("event15_helper_2b1f64", 0x002b1f64L),
		new Target("sched_post_event_delay_2697aa", 0x002697aaL),
		new Target("ccont_irq_status_service_2b08c6", 0x002b08c6L),
		new Target("startup_branch_270e1c", 0x00270e1cL),
	};

	private Function prepareThumbFunction(Target target, Register tmode, RegisterValue thumbMode) throws Exception {
		Address entry = toAddr(target.addr);
		clearListing(entry, entry.add(0x4ff));
		try {
			currentProgram.getProgramContext().setValue(tmode, entry, entry.add(0x4ff), BigInteger.ONE);
		} catch (Exception e) {
			printf("thumb context already constrained at %s: %s\n", entry, e.getMessage());
		}

		AddressSet set = new AddressSet(entry, entry);
		DisassembleCommand command = new DisassembleCommand(set, null, true);
		command.setInitialContext(thumbMode);
		command.applyTo(currentProgram, monitor);

		Function function = getFunctionAt(entry);
		if (function == null) {
			createFunction(entry, target.name);
			function = getFunctionAt(entry);
		}
		if (function == null) {
			printf("missing function at %s\n", entry);
			return null;
		}

		function.setName(target.name, SourceType.USER_DEFINED);
		return function;
	}

	@Override
	public void run() throws Exception {
		String[] args = getScriptArgs();
		File outDir = new File(args.length > 0 ? args[0] : "/tmp/event15_sources");
		outDir.mkdirs();

		DecompInterface decompiler = new DecompInterface();
		decompiler.openProgram(currentProgram);
		Register tmode = currentProgram.getProgramContext().getRegister("TMode");
		RegisterValue thumbMode = new RegisterValue(tmode, BigInteger.ONE);

		for (Target target : TARGETS)
			prepareThumbFunction(target, tmode, thumbMode);

		for (Target target : TARGETS) {
			Address entry = toAddr(target.addr);
			Function function = getFunctionAt(entry);
			if (function == null)
				continue;
			DecompileResults results = decompiler.decompileFunction(function, 60, monitor);
			File outFile = new File(outDir, target.name + ".c");
			try (PrintWriter writer = new PrintWriter(outFile)) {
				writer.printf("// %s at %s%n%n", function.getName(), entry);
				if (results.decompileCompleted()) {
					writer.println(results.getDecompiledFunction().getC());
				} else {
					writer.printf("// decompile failed: %s%n", results.getErrorMessage());
				}
			}
			printf("exported %s\n", outFile.getAbsolutePath());
		}

		decompiler.dispose();
	}
}
