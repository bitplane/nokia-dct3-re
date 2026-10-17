-- Staged C54x debugger hook runs at Execute entry; snapshots are context only.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'cleanup ingress requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
assert(cpu.debug, 'cleanup ingress observation requires -debug -debugger none')
machine.debugger:command('temp0=0')
machine:logerror('mu4_cleanup_ingress_scope: identity=debugger_Execute_hook operands=context_only annulment=unclaimed observer_writes=0 audio_acceptance=0\n')
local sites = {{0x29438, 0x1004}, {0x2944b, 0x7105}, {0x29471, 0x1005},
               {0x294c3, 0xf84a}, {0x294ca, 0xf847}, {0x294e6, 0x6c82},
               {0x294fd, 0x60f8}, {0x29500, 0xf830}, {0x29502, 0xf7b8}, {0x2956f, 0x60f8}}
local handles = {}
-- Debugger integers default to hex; its formatter supports %d/%x, not %u.
for _, site in ipairs(sites) do
    local address, opcode = site[1], site[2]
    handles[#handles + 1] = cpu.debug:bpset(address, string.format('temp0<1000 && pw@%x==%x', address, opcode),
        'temp0=temp0+1;logerror "mu4_cleanup_ingress: ordinal=%d address=%06x opcode=%04x cycles=%d sp=%04x f4=%04x f5=%04x f6=%04x f7=%04x f8=%04x f9=%04x control=%04x ar1=%04x ar2=%04x a=%010x b=%010x tc=%d identity=debugger_Execute_hook operands=context_only observer_writes=0\\n",' ..
        'temp0,curpc,pw@curpc,totalcycles,sp,dw@(sp+4),dw@(sp+5),dw@(sp+6),' ..
        'dw@(sp+7),dw@(sp+8),dw@(sp+9),dw@aa1a,ar1,ar2,a,b,(st0>>c)&1;g')
end
local stop = emu.add_machine_stop_notifier(function()
    machine.debugger:command('logerror "mu4_cleanup_ingress_terminal: events=%d cap=4096 observer_writes=0 audio_acceptance=0\\n",temp0')
    machine:logerror(string.format('mu4_cleanup_ingress_stop: t=%.9f illegal=%u observer_writes=0 audio_acceptance=0\n',
        machine.time:as_double(), cpu.state.ILLEGAL.value))
end)
_G.mu4_cleanup_ingress_handles = {handles, stop}
