-- Read-only execution-entry scores; the shared RF input stays unchanged.
local directory = debug.getinfo(1, 'S').source:sub(2):match('(.*/)') or ''
dofile(directory .. 'c54x_rom4_rf_score.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
assert(cpu.debug, 'RF execution probes require -debug -debugger none')
-- Debugger action expressions resolve registers in the visible CPU context.
machine.debugger.visible_cpu = cpu
for _, address in ipairs({0x0ad1, 0x0add, 0x0ae4}) do
    cpu.debug:bpset(address, nil, string.format(
        'logerror "rom4_rf_execute: pc=%04x a=%%010x\\n",a;g', address))
end
machine:logerror('rom4_rf_execution_probe: points=0ad1,0add,0ae4 phase=instruction_entry\n')
machine.debugger.execution_state = 'run'
