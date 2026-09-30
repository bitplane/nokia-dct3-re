// Import the portable symbol map (address,kind,name CSV) into the current program.
// Functions are created (Thumb) where absent; labels become primary symbols.
// @category Nokia3210
import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.program.model.symbol.SourceType;
import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.lang.Register;
import ghidra.program.model.lang.RegisterValue;
import java.math.BigInteger;

public class ImportSymbolsCsv extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		File csv = args.length > 0 ? new File(args[0]) : askFile("Symbol CSV", "Import");
		Register tmode = currentProgram.getProgramContext().getRegister("TMode");
		int functions = 0, labels = 0;
		try (BufferedReader reader = new BufferedReader(new FileReader(csv))) {
			String line;
			while ((line = reader.readLine()) != null) {
				String[] parts = line.trim().split(",");
				if (parts.length < 3 || parts[0].equals("address")) continue;
				long value = Long.decode(parts[0].trim());
				String kind = parts[1].trim();
				String name = parts[2].trim();
				Address addr = toAddr(value & ~1L);
				if (kind.equals("function")) {
					Function fn = getFunctionAt(addr);
					if (fn == null) {
						if (getInstructionAt(addr) == null) {
							DisassembleCommand cmd = new DisassembleCommand(new AddressSet(addr, addr), null, true);
							cmd.setInitialContext(new RegisterValue(tmode, BigInteger.ONE));
							cmd.applyTo(currentProgram, monitor);
						}
						fn = createFunction(addr, name);
					}
					if (fn != null) { fn.setName(name, SourceType.USER_DEFINED); functions++; }
					else { createLabel(addr, name, true, SourceType.USER_DEFINED); labels++; }
				} else {
					createLabel(addr, name, true, SourceType.USER_DEFINED);
					labels++;
				}
			}
		}
		printf("ImportSymbolsCsv: %d functions, %d labels from %s\n", functions, labels, csv);
	}
}
