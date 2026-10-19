-- Staged Execute-hook observations of ordinary completions and teardown only.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'refill completion requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
assert(cpu.debug, 'refill completion requires -debug -debugger none')
machine.debugger:command('temp2=0')
machine:logerror('mu4_refill_completion_scope: identity=debugger_Execute_hook operands=context_only observer_writes=0 audio_acceptance=0\n')
local handles = {}
for _, site in ipairs({{0x20862, 0x10f8, 'entry'}, {0x20893, 0x10f8, 'updated'}, {0x207ea, 0x1003, 'control19'}}) do
    local address, opcode, kind = site[1], site[2], site[3]
    -- Low logical program addresses use OVLY's page-zero program backing.
    local physical = address % 0x10000
    handles[#handles+1] = cpu.debug:bpset(address, string.format(
        'temp2<2000 && pw@%x==%x && pw@2942c==76f8 && pw@2942d==a9f8', physical, opcode),
        'temp2=temp2+1;logerror "mu4_refill_completion: ordinal=%d kind=' .. kind ..
        ' address=%06x cycles=%d pending=%04x supplied=%04x target=%04x field6=%04x identity=debugger_Execute_hook operands=context_only observer_writes=0\\n",' ..
        'temp2,curpc,totalcycles,dw@15e0,dw@(sp+3),dw@15e7,dw@((dw@15e7)+6);g')
end
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "mu4_refill_completion_terminal: events=%d cap=8192 observer_writes=0 audio_acceptance=0\\n",temp2')
    machine:logerror(string.format('mu4_refill_completion_stop: t=%.9f illegal=%u observer_writes=0 audio_acceptance=0\n',
        machine.time:as_double(), cpu.state.ILLEGAL.value))
end)
_G.mu4_refill_completion_handles = {handles, stop}
