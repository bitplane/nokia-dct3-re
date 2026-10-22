-- Read-only native serial control; no PC redirects or guest writes.
local machine=manager.machine
local cpu=assert(machine.devices[':cpu'])
assert(machine.system.name=='mu4nand' and cpu.debug)
machine.debugger.visible_cpu=cpu
machine.debugger:command('temp0=0')
machine:logerror('mu4_play_control_scope: identity=Execute_entry operands=context_only observer_writes=0 audio_acceptance=0\n')
local handles={}
for _,site in ipairs({
    {0x38d03,'serial30'}, {0x28498,'control30_04'}, {0x2905d,'build03'},
    {0x20780,'request'}, {0x208ce,'input03'}, {0x208e5,'pending3'},
    {0x20727,'send16'}, {0x29458,'receive16'}, {0x291d3,'hook16'},
    {0x28922,'resident_idle3'},
}) do
    local low=site[1]%0x10000
    local physical=low<0x8000 and low or site[1]
    -- Debugger literals are hexadecimal: 400 is the declared 1024 events.
    handles[#handles+1]=cpu.debug:bpset(site[1],
        site[2]=='resident_idle3' and 'temp0<400' or
            'temp0<400 && pw@2942c==76f8 && pw@2942d==a9f8',
        'temp0=temp0+1;logerror "mu4_play_control: ordinal=%d kind='..site[2]..
        ' pc=%06x opcode=%04x cycles=%d sp=%04x f2=%04x f3=%04x f4=%04x f5=%04x pending=%04x active=%04x state=%04x lifecycle=%04x parameter=%04x identity=Execute_entry operands=context_only observer_writes=0\\n",'..
        string.format('temp0,curpc,pw@%x,totalcycles,sp,dw@(sp+2),dw@(sp+3),dw@(sp+4),dw@(sp+5),dw@12fb,dw@12fd,dw@a825,dw@373e,dw@1189;g',physical))
end
local stopped=emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "mu4_play_control_terminal: events=%d cap=1024 observer_writes=0 audio_acceptance=0\\n",temp0')
end)
_G.mu4_play_control_handles={handles,stopped}
machine.debugger.execution_state='run'
