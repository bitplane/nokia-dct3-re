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
cpu.debug:bpset(0x37ce, 'ar4==0x8a && ar5==2',
    'logerror "rom4_rf_execution_publication: type=%04x bytes=%d words=%04x\\n",ar4,ar5,dw@ar3;g')
cpu.debug:bpset(0x3807, 'ar4==0x8a && ar5==2',
    'logerror "rom4_rf_execution_publish_exit: address=3807 type=%04x bytes=%d producer=%04x consumer=%04x\\n",ar4,ar5,dw@0x08e4,dw@0x08e5;g')
machine:logerror('rom4_rf_publication_probe: points=37ce,3807 phase=instruction_entry\n')
for _, address in ipairs({0x3ffb, 0x400f, 0x402e, 0x403a, 0x4048}) do
    cpu.debug:bpset(address, 'dw@0x1867==0x6d', string.format(
        'logerror "rom4_rf_execution_tuning: address=%04x a=%%010x b=%%010x ar2=%%04x flags6be=%%04x flags216a=%%04x addend216b=%%04x field60=%%04x selected1867=%%04x\\n",a,b,ar2,dw@0x06be,dw@0x216a,dw@0x216b,dw@0x0060,dw@0x1867;g', address))
end
machine:logerror('rom4_rf_tuning_probe: points=3ffb,400f,402e,403a,4048 phase=instruction_entry\n')
machine.debugger:command('temp0=0')
for _, address in ipairs({0x330a, 0x3310, 0x3316, 0x331c}) do
    cpu.debug:bpset(address, 'temp0<0x40', string.format(
        'temp0=temp0+1;logerror "rom4_rf_execution_filter: index=%%d pc=%%04x opcode=%%04x port=%%04x\\n",temp0,pc,pw@0x%04x,pw@0x%04x;g', address, address + 1))
end
machine:logerror('rom4_rf_filter_probe: entries=64 phase=instruction_entry\n')
machine.debugger.execution_state = 'run'
