// Establish Thumb context and the firmware entry before auto-analysis.
// @category Nokia3210

import java.math.BigInteger;

import ghidra.app.cmd.disassemble.DisassembleCommand;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.AddressSet;
import ghidra.program.model.lang.Register;
import ghidra.program.model.lang.RegisterValue;

public class SetNokiaThumb extends GhidraScript {
	@Override
	protected void run() throws Exception {
		Register tmode = currentProgram.getProgramContext().getRegister("TMode");
		currentProgram.getProgramContext().setValue(tmode, toAddr(0x00200000), toAddr(0x003fffff), BigInteger.ONE);
		DisassembleCommand command = new DisassembleCommand(
				new AddressSet(toAddr(0x00200040), toAddr(0x00200040)), null, true);
		command.setInitialContext(new RegisterValue(tmode, BigInteger.ONE));
		command.applyTo(currentProgram, monitor);
		if (getFunctionAt(toAddr(0x00200040)) == null)
			createFunction(toAddr(0x00200040), "firmware_entry_200040");
	}
}
