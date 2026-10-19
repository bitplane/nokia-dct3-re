-- Passive original AA99 software-notification observation, not audio acceptance.
local machine = manager.machine
local cpu = assert(machine.devices[':cpu'])
assert(machine.system.name == 'mu4nand' and cpu.debug)
machine.debugger.visible_cpu = cpu
machine.debugger:command('temp0=0')
machine:logerror('mu4_notify_scope: identity=Execute_entry operands=context_only observer_writes=0 audio_acceptance=0\n')
local handles = {}
for _, site in ipairs({{0x292c1,0x56f8,'publish'}, {0x20981,0x76f8,'callback'},
                      {0x22064,0x4a1e,'vector'}, {0x20742,0xf495,'wrapper'},
                      {0x20780,0x7102,'request'}, {0x207fa,0x76f8,'input16'},
                      {0x207f5,0x76f8,'input17'}, {0x29458,0xe781,'receive16'}}) do
    -- OVLY shares only the lower 32K; upper program words retain their page.
    local low = site[1] % 0x10000
    local physical = low < 0x8000 and low or site[1]
    handles[#handles+1] = cpu.debug:bpset(site[1],
        'temp0<1000 && pw@2942c==76f8 && pw@2942d==a9f8',
        'temp0=temp0+1;logerror "mu4_notify: ordinal=%d kind=' .. site[3] ..
        ' address=%06x opcode=%04x cycles=%d sp=%04x f2=%04x f3=%04x f5=%04x mailbox=%04x flag=%04x cbh=%04x cbl=%04x\\n",' ..
        string.format('temp0,curpc,pw@%x,totalcycles,sp,dw@(sp+2),dw@(sp+3),dw@(sp+5),dw@80,dw@12f8,dw@aa7e,dw@aa7f;g',physical))
end
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "mu4_notify_terminal: events=%d cap=4096 observer_writes=0 audio_acceptance=0\\n",temp0')
    machine:logerror(string.format('mu4_notify_stop: t=%.9f illegal=%u\n',machine.time:as_double(),cpu.state.ILLEGAL.value))
end)
_G.mu4_notify_handles={handles,stop}
machine.debugger.execution_state='run'
