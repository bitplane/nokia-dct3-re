-- Passive own-ROM idle predicates; no RAM/MMIO writes.
local machine = manager.machine
assert(machine.system.name == 'nam2c3')
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local sites = {
    {0x2f8688, 'idle_guard_return', 'r0'},
    {0x2fac8e, 'sleep_flag', 'r0'},
    {0x2facb8, 'sleep_clock_delta', 'r3,r0'},
    {0x2facd8, 'sleep_timer_queue', 'r0'},
    {0x2f864a, 'restore_branch', 'r0'},
    {0x2f866a, 'sleep_branch', 'r0'},
    {0x2facd8, 'timer_queue_late', 'r0', 'f800000'},
}
-- Start after roughly ten seconds at the configured 13 MHz ARM clock.
for i, site in ipairs(sites) do
    local counter = 'temp' .. (i - 1)
    machine.debugger:command(counter .. '=0')
    local format = site[3]:find(',') and 'a=%08x b=%08x' or 'a=%08x'
    machine.debugger:command(string.format(
        'bpset %x,totalcycles>%s && %s<8,{%s=%s+1;logerror "nam2_idle_%s: %s cycles=%%016x\\n",%s,totalcycles;g}',
        site[1], site[4] or '7c00000', counter, counter, counter, site[2], format, site[3]))
end
local next_time = 10
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if now < next_time then return end
    machine:logerror(string.format('nam2_idle_sample: t=%.6f pc=%08x ctrl=%02x fmask=%02x imask=%02x fiq=%02x irq=%02x nest=%02x saved=%02x clock=%02x timer0=%04x stamp=%04x\n',
        now, cpu.state['PC'].value, memory:read_u8(0x2000c),
        memory:read_u8(0x2000a), memory:read_u8(0x2000b), memory:read_u8(0x20008),
        memory:read_u8(0x20009), memory:read_u8(0x10f9f6), memory:read_u8(0x10f9f4), memory:read_u8(0x2000d),
        memory:read_u16(0x20010), memory:read_u16(0x10f9f8)))
    next_time = next_time + 1
end)
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
dofile(directory .. 'noki2100_v521_early_security_input.lua')
local late = coroutine.create(function()
    if not emu.wait(18) then return end
    local key = assert(machine.ioport.ports[':COL.2'].fields['Keypad 1'])
    machine:logerror('nam2_idle_late_key: key=Keypad 1 t=18 observer_writes=0\n')
    key:set_value(1)
    if not emu.wait(0.2) then key:set_value(0); return end
    key:set_value(0)
    if not emu.wait(2) then return end
    machine.screens[':screen']:snapshot('nam2_c3_idle_after_late_key.png')
end)
_G.nam2_c3_idle_late = late
assert(coroutine.resume(late))
