-- One bounded RAM snapshot at the original cleanup entry; never a replay input.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'cleanup buffer observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
assert(cpu.debug, 'cleanup buffer observation requires -debug -debugger none')
machine.debugger:command('temp0=0')
machine:logerror('mu4_cleanup_buffer_scope: first=0080 words=65408 encoding=le16 identity=debugger_Execute_hook operands=context_only observer_writes=0 audio_acceptance=0\n')
local breakpoint = cpu.debug:bpset(0x2956f,
    'temp0==0 && pw@2956f==60f8 && pw@29570==373e && pw@29571==4',
    'temp0=temp0+1;saved mu4_cleanup_input.le16,80,ff80;' ..
    'logerror "mu4_cleanup_buffer: events=%d address=%06x cycles=%d object=%04x cursor=%04x limit=%04x file_buffer=%04x lifecycle=%04x identity=debugger_Execute_hook operands=context_only observer_writes=0\\n",' ..
    'temp0,curpc,totalcycles,dw@15e9,dw@(dw@15e9),dw@((dw@15e9)+1),dw@15e7,dw@373e;g')
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "mu4_cleanup_buffer_terminal: events=%d observer_writes=0 audio_acceptance=0\\n",temp0')
    machine:logerror(string.format('mu4_cleanup_buffer_stop: t=%.9f illegal=%u observer_writes=0 audio_acceptance=0\n',
        machine.time:as_double(), cpu.state.ILLEGAL.value))
end)
_G.mu4_cleanup_buffer_handles = {breakpoint, stop}
