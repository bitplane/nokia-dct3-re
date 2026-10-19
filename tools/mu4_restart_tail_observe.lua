-- Context-only restart selection plus a separately owned cleanup snapshot.
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'mu4_cleanup_buffer_observe.lua')
dofile(directory .. 'mu4_playback_lifecycle_observe.lua')
local machine, cpu = manager.machine, manager.machine.devices[':cpu']
machine.debugger:command('temp3=0')
machine:logerror('mu4_restart_scope: identity=debugger_Execute_hook operands=context_only observer_writes=0 audio_acceptance=0\n')
local handles = {}
for _, site in ipairs({{0x3a98f, 0x60f8}, {0x3a9a2, 0x6ce2},
                       {0x3a9a5, 0x76f8}, {0x3a9aa, 0xf983}, {0x3a9af, 0x68f8}}) do
    local address, opcode = site[1], site[2]
    handles[#handles+1] = cpu.debug:bpset(address,
        string.format('temp0==1 && temp3<20 && pw@%x==%x', address, opcode),
        'temp3=temp3+1;logerror "mu4_restart_context: ordinal=%d address=%06x cycles=%d ' ..
        'mode=%04x track=%04x count=%04x lifecycle=%04x continuation=%04x ar2=%04x ' ..
        'identity=debugger_Execute_hook operands=context_only observer_writes=0\\n",' ..
        'temp3,curpc,totalcycles,dw@3740,dw@3765,dw@3768,dw@373e,dw@3762,ar2;g')
end
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "mu4_restart_terminal: events=%d cap=32 observer_writes=0 audio_acceptance=0\\n",temp3')
end)
_G.mu4_restart_tail_handles = {handles, stop}
