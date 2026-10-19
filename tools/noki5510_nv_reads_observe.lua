-- Own logical-NV requests after mutex acquisition; context, not bus retirement.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki5510_selftest_observe.lua')
local machine, cpu = manager.machine, manager.machine.devices[':maincpu']
assert(machine.system.name == 'nmp5hle', 'NV read inventory requires NPM-5 hybrid fixture')
machine.debugger:command('temp9=0')
machine:logerror('5510_nv_reads_scope: reader=002fd578 wrapper=003ac638 cap=1024 identity=debugger_entry operands=context_only observer_writes=0\n')
local hook = cpu.debug:bpset(0x2fd578,
    'temp9<400 && w@2fd578==b530',
    'temp9=temp9+1;logerror "5510_nv_read: ordinal=%d offset=%08x destination=%08x bytes=%08x link=%08x owner=%08x identity=debugger_entry operands=context_only observer_writes=0\\n",' ..
    'temp9,r0,r1,r2,r14,d@(r13+c);g')
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "5510_nv_reads_terminal: events=%d cap=1024 observer_writes=0\\n",temp9')
    machine:logerror(string.format('5510_nv_reads_stop: t=%.9f observer_writes=0\n',
        machine.time:as_double()))
end)
_G.noki5510_nv_reads_handles = {hook, stop}
