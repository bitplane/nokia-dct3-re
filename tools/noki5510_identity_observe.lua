-- Observe original identity formatting with existing NV; never call it ourselves.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki5510_nv_reads_observe.lua')
local machine, cpu = manager.machine, manager.machine.devices[':maincpu']
machine.debugger:command('temp6=0')
machine:logerror('5510_identity_scope: entry=002a05cc cap=16 identity=debugger_entry operands=context_only observer_writes=0\n')
local handles = {}
handles[1] = cpu.debug:bpset(0x2a05cc, 'temp6<10 && w@2a05cc==b570',
    'temp6=temp6+1;temp7=r2;logerror "5510_identity_entry: ordinal=%d selector=%d sp=%08x source=%08x destination=%08x packed=%08x%08x observer_writes=0\\n",' ..
    'temp6,temp7,r13,r0,r1,d@r0,d@(r0+4);g')
for _, site in ipairs({{0x2a060a,8},{0x2a063e,9}}) do
    local address, bytes = site[1], site[2]
    handles[#handles+1] = cpu.debug:bpset(address, 'temp6>0 && temp6<11',
        'logerror "5510_identity_return: ordinal=%d selector=%d address=%08x sp=%08x source=%08x destination=%08x output=%08x%08x' ..
        (bytes == 9 and '%02x' or '') .. ' observer_writes=0\\n",' ..
        'temp6,temp7,pc,r13,r4,r5,d@r5,d@(r5+4)' .. (bytes == 9 and ',b@(r5+8)' or '') .. ';g')
end
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "5510_identity_terminal: entries=%d cap=16 observer_writes=0\\n",temp6')
end)
_G.noki5510_identity_handles = {handles, stop}
