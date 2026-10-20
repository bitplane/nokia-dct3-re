-- Read-only NAM-2 v5.21 startup observer. Run with -debug -debugger none.
local machine = manager.machine
assert(machine.system.name == 'noki2100' or machine.system.name == 'nam2c3')
local debugger = machine.debugger
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local commands = {
    {0x2e6998, 'pmm_erase', 'index=%08x p0=%08x p1=%08x p2=%08x p3=%08x p4=%08x p5=%08x p6=%08x p7=%08x',
        'r0,d@10b27c,d@10b294,d@10b2ac,d@10b2c4,d@10b2dc,d@10b2f4,d@10b30c,d@10b324'},
    {0x305b82, 'flash_family', 'family=%08x id0=%04x maker=%04x device=%04x third=%04x',
        'r0,w@r13,w@(r13+2),w@(r13+4),w@(r13+6)'},
    {0x305bea, 'flash_dispatch', 'program=%08x erase=%08x extra0=%08x extra1=%08x',
        'd@10fac8,d@10facc,d@10fad0,d@10fad4'},
    {0x291b24, 'checksum_input', 'root=%08x identity0=%08x identity1=%08x callback=%08x',
        'r4,d@(r4+c),d@(r4+10),d@10fad4'},
    {0x291b3e, 'checksum', 'computed=%08x stored=%08x root=%08x identity0=%08x identity1=%08x',
        'r0,r1,r4,d@(r4+c),d@(r4+10)'},
    {0x2530a4, 'reference', 'computed=%04x stored=%04x secondary=%04x',
        'r5,r0,w@(r13+2)'},
    {0x25312a, 'failed_item', 'index=%d value=%02x pointer=%08x', 'r1,r0,r2'},
    {0x2f84aa, 'release', 'result=%08x flag=%02x recv_wait=%02x service=%02x mode=%02x',
        'r0,b@13fdaf,b@10ef46,b@10e9f4,b@10fbf1'},
    {0x304c2e, 'sleep_arg', 'arg=%08x caller=%08x ctrl=%02x fmask=%02x imask=%02x',
        'r0,r14,b@2000c,b@2000a,b@2000b'},
    {0x304c56, 'sleep_write', 'value=%08x base=%08x caller=%08x',
        'r0,r1,d@(r13+4)'},
}
for index, site in ipairs(commands) do
    local counter = 'temp' .. (index - 1)
    debugger:command(counter .. '=0')
    debugger:command(string.format(
        'bpset %x,%s<32,{%s=%s+1;logerror "nam2_v521_%s: %s cycles=%%016x observer_writes=0\\n",%s,totalcycles;g}',
        site[1], counter, counter, counter, site[2], site[3], site[4]))
end
local times = {0.5, 2, 4, 8, 12}
local index = 1
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if index > #times or now < times[index] then return end
    machine:logerror(string.format('nam2_v521_cold: t=%.9f pc=%08x sp=%08x observer_writes=0\n',
        now, cpu.state['PC'].value, cpu.state['R13'].value))
    machine.screens[':screen']:snapshot(string.format('nam2_v521_%02d.png', index))
    index = index + 1
end)
local flag_writes = 0
local handle = memory:install_write_tap(0x13fdac, 0x13fdaf, 'nam2_v521_flag',
    function(offset, value, mask)
        if flag_writes >= 32 or (mask & 0xff) == 0 then return end
        flag_writes = flag_writes + 1
        machine:logerror(string.format('nam2_v521_flag: pc=%08x value=%08x mask=%08x t=%.9f observer_writes=0\n',
            cpu.state['PC'].value, value, mask, machine.time:as_double()))
    end)
_G.nam2_v521_flag_handle = handle
debugger:command('g')
