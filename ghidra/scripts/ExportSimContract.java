// Export the firmware functions that define the SIM transport/reply contract.
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
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.Instruction;
import ghidra.program.model.lang.Register;
import ghidra.program.model.lang.RegisterValue;
import ghidra.program.model.symbol.SourceType;
import ghidra.program.model.symbol.Reference;

public class ExportSimContract extends GhidraScript {
	private static final long[] TARGETS = {
		0x002a0060L, 0x002a01b8L, 0x002a0218L, 0x002a0268L, 0x002a02e6L, 0x002a03b4L, 0x002a0454L,
		0x002a04c8L, 0x002a0532L, 0x002a0548L, 0x002a054aL,
		0x002a05ccL, 0x002a06d8L, 0x002a06f0L, 0x002a0720L,
		0x0027defcL, 0x0027e240L, 0x0027e98cL, 0x0027ee94L, 0x0027ef0aL, 0x0027efb0L,
		0x0027c688L, 0x0027c794L, 0x0027c81cL, 0x0027c8d8L, 0x0027c9dcL, 0x0027cb28L,
		0x002aec34L,
		0x002900a0L, 0x002900b6L, 0x002902acL, 0x00293522L,
		0x002af316L, 0x002af430L, 0x002af49cL,
		0x002935c8L, 0x00293f30L, 0x00207234L,
		0x00208ee0L, 0x002091c2L, 0x00209978L, 0x00209d04L, 0x00209dc4L,
		0x0020a026L, 0x0020a8a8L, 0x0020e77aL, 0x0020eda8L,
		0x00221d3cL, 0x00221d50L, 0x00221d74L, 0x002223f0L, 0x002223f8L, 0x00225208L, 0x00225c92L,
		0x00262306L, 0x00262438L, 0x00262452L, 0x002624b8L, 0x002625acL, 0x0026265cL,
		0x002629d0L, 0x00262fa4L, 0x00262ff0L, 0x00263006L, 0x0026309cL,
		0x00263154L, 0x002632fcL, 0x002633d0L, 0x00263528L, 0x002635acL,
		0x0026b444L, 0x0026b4fcL, 0x00296e86L, 0x00298750L,
		0x00263724L, 0x00263840L, 0x00263878L, 0x00263d30L,
		0x002680f8L, 0x00268240L, 0x00268284L, 0x002689d8L, 0x00268a58L, 0x00268aecL,
		0x0026abf8L, 0x0026afe0L, 0x0026b58cL, 0x0026cc98L,
		0x002618e8L, 0x00261eb0L, 0x0026e400L, 0x0026e466L, 0x0026e4d4L, 0x0026e620L,
		0x00260018L, 0x002601e8L, 0x00260290L, 0x002602b4L, 0x002634d4L, 0x002abeb2L,
		0x0026d938L, 0x0026db44L, 0x0026dbd6L, 0x0026dc1cL, 0x0026e6f0L, 0x0026e764L,
		0x002860f6L, 0x002862f0L, 0x0028d194L, 0x0028d29cL,
		0x0028d710L, 0x002a1c88L, 0x002a1ea6L,
		0x00290756L, 0x002907c4L, 0x00290840L, 0x00290874L, 0x0029088eL, 0x002916f4L,
		0x00290904L, 0x0029099aL, 0x002909e4L, 0x002b40e6L,
		0x002793b6L, 0x0027a00cL, 0x0027a434L, 0x0027a520L, 0x0027a564L,
		0x00251c0aL, 0x00251c2aL, 0x00251d1cL, 0x00251ee8L,
		0x00251a40L, 0x00251ac4L, 0x00251d04L, 0x00251d56L, 0x00251d7cL, 0x00251f44L,
		0x002521d4L, 0x0025231cL, 0x002525ceL, 0x002525eeL, 0x00252638L,
		0x00252690L, 0x0025271cL, 0x002527a8L, 0x00252818L, 0x002528ccL,
		0x0025317cL, 0x00253230L, 0x002533fcL, 0x0025342cL, 0x00253876L,
		0x00253924L, 0x002539b8L, 0x00253c74L, 0x00253d30L,
		0x002791fcL, 0x002792deL, 0x00279320L, 0x002794d2L, 0x00279734L,
		0x00279818L, 0x0027990cL, 0x002799c6L, 0x00279a0cL, 0x00279b84L,
		0x00279c44L, 0x00279cd0L, 0x00279d48L, 0x00279e78L, 0x00279f0cL,
		0x00219f0cL,
		0x00290b54L, 0x00290cf4L, 0x00290cf8L, 0x002b1a2cL, 0x002b1a44L,
		0x00291068L, 0x002af3caL,
		0x0029e9c2L, 0x0029ea48L, 0x0029ea80L,
		0x0029ab94L, 0x0029bc00L,
		0x0029992cL, 0x00299b70L, 0x00299b9cL, 0x00299bc0L, 0x00299bc4L, 0x00299bdaL, 0x00299c0cL,
		0x00237400L, 0x00237960L, 0x00237b80L, 0x00237bb4L,
		0x00282d64L,
		0x00255df0L, 0x00255e2cL, 0x00255e44L, 0x00255ea2L, 0x00255eaaL, 0x00255fc2L, 0x0027b1c0L,
		0x00243180L, 0x00243550L, 0x00243646L, 0x0024383cL, 0x0024387aL, 0x002438e8L, 0x002b605cL,
		0x0024d24eL, 0x0024d2daL, 0x0024d588L, 0x0024d7ecL, 0x0024d8a4L, 0x0024dd3aL,
		0x0024df28L, 0x0024f1a4L, 0x0024f25cL,
		0x00253e20L, 0x00253f2cL,
		0x00277cb4L, 0x0028bddcL, 0x002a07b2L,
		0x002700c8L, 0x00283316L, 0x00283b4eL, 0x00283db6L, 0x00284316L, 0x00284534L, 0x00284f74L, 0x0028cfecL,
		0x002a9d3eL, 0x002a9d98L, 0x002a9ea6L, 0x002a9f4eL, 0x002a9f64L, 0x002aa052L,
		0x002ac3f2L, 0x002ac5ccL, 0x002aed5cL, 0x002aeda0L, 0x002aee00L, 0x002aee20L,
		0x002aeec6L, 0x002aef44L, 0x002aef7eL, 0x002aefbaL,
		0x002638e4L, 0x0024bd30L, 0x0028676cL, 0x0024cb0cL,
		0x002b12b4L, 0x002b12dcL, 0x002b13d4L, 0x002b140aL, 0x002b2560L, 0x002b257eL,
		0x002b4f98L, 0x002b4fd6L, 0x002b4feeL, 0x002b5064L,
		0x002b38a8L, 0x002b3ad0L,
		0x002af57cL, 0x002af5d2L, 0x002af5fcL, 0x002af630L, 0x002af6eaL, 0x002af744L, 0x002af77cL,
		0x002af798L
	};

	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File output = new File(args.length > 0 ? args[0] : "/tmp/sim_contract.c");
		DecompInterface decompiler = new DecompInterface();
		decompiler.openProgram(currentProgram);
		Register tmode = currentProgram.getProgramContext().getRegister("TMode");
		RegisterValue thumbMode = new RegisterValue(tmode, BigInteger.ONE);
		try (PrintWriter out = new PrintWriter(output)) {
			for (long raw : TARGETS) {
				Address address = toAddr(raw);
				out.printf("\n/* refs to %08x", raw);
				for (Reference reference : getReferencesTo(address))
					out.printf(" %s:%s", reference.getFromAddress(), reference.getReferenceType());
				out.println(" */");
				clearListing(address, address.add(0x3ff));
				currentProgram.getProgramContext().setValue(tmode, address, address.add(0x3ff), BigInteger.ONE);
				DisassembleCommand command = new DisassembleCommand(new AddressSet(address, address), null, true);
				command.setInitialContext(thumbMode);
				command.applyTo(currentProgram, monitor);
				Function function = getFunctionAt(address);
				if (function == null) {
					function = createFunction(address, "sim_" + Long.toHexString(raw));
				}
				out.printf("\n/* ===== %08x %s ===== */\n", raw,
						function == null ? "<no function>" : function.getName());
				if (function != null) {
					out.println("/* instructions */");
					for (Instruction instruction : currentProgram.getListing().getInstructions(function.getBody(), true))
						out.printf("/* %s  %-8s %s */%n", instruction.getAddress(),
								instruction.getMnemonicString(), instruction.toString().substring(instruction.getMnemonicString().length()).trim());
					DecompileResults result = decompiler.decompileFunction(function, 60, monitor);
					out.println(result.decompileCompleted()
							? result.getDecompiledFunction().getC()
							: "/* decompile failed: " + result.getErrorMessage() + " */");
				}
			}
		}
		decompiler.dispose();
	}
}
