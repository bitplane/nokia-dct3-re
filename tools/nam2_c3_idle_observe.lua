-- Passive own-ROM idle predicates; no RAM/MMIO writes.
local machine = manager.machine
assert(machine.system.name == 'nam2c3')
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces.program
local owner_taps = {}
local control_commands = {}
local timer1_destination_writes = 0
owner_taps[#owner_taps + 1] = memory:install_write_tap(
    0x20004, 0x20007, 'nam2_idle_timer1_destination', function(offset, value, mask)
        if (mask & 0xffff) ~= 0 then
            timer1_destination_writes = timer1_destination_writes + 1
        end
    end)
owner_taps[#owner_taps + 1] = memory:install_write_tap(
    0x2000c, 0x2000f, 'nam2_idle_control_commands', function(offset, value, mask)
        if (mask & 0xff000000) == 0 then return end
        local command = (value >> 24) & 0xff
        control_commands[command] = (control_commands[command] or 0) + 1
    end)
for _, address in ipairs({0x10ef46, 0x10917b, 0x107b8b, 0x109173}) do
    local shift = (3 - (address & 3)) * 8
    local last = memory:read_u8(address)
    local count = 0
    owner_taps[#owner_taps + 1] = memory:install_write_tap(
        address & ~3, address | 3, 'nam2_idle_owner', function(offset, value, mask)
            if count >= 16 or (mask & (0xff << shift)) == 0 then return end
            local next_value = (value >> shift) & 0xff
            if machine.time:as_double() >= 10 and next_value ~= last then
                count = count + 1
                machine:logerror(string.format('nam2_idle_owner: address=%08x before=%02x after=%02x pc=%08x t=%.9f observer_writes=0\n',
                    address, last, next_value, cpu.state['PC'].value, machine.time:as_double()))
            end
            last = next_value
        end)
end
_G.nam2_c3_idle_owner_taps = owner_taps
local sites = {
    {0x2f8688, 'idle_guard_return', 'r0'},
    {0x2fac8e, 'sleep_flag', 'r0'},
    {0x2facb8, 'sleep_clock_delta', 'r3,r0'},
    {0x2facd8, 'sleep_timer_queue', 'r0'},
    {0x2f864a, 'restore_branch', 'r0'},
    {0x2f866a, 'sleep_branch', 'r0'},
    {0x2facd8, 'timer_queue_late', 'r0', 'f800000'},
    {0x2f8626, 'sim_idle_gate_true', 'r0', '7c00000', 'r0!=0'},
}
-- Start after roughly ten seconds at the configured 13 MHz ARM clock.
for i, site in ipairs(sites) do
    local counter = 'temp' .. (i - 1)
    machine.debugger:command(counter .. '=0')
    local format = site[3]:find(',') and 'a=%08x b=%08x' or 'a=%08x'
    machine.debugger:command(string.format(
        'bpset %x,totalcycles>%s && %s<8 && (%s),{%s=%s+1;logerror "nam2_idle_%s: %s cycles=%%016x\\n",%s,totalcycles;g}',
        site[1], site[4] or '7c00000', counter, site[5] or '1', counter, counter, site[2], format, site[3]))
end
local next_time = 10
local descriptor_sampled = false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    if not descriptor_sampled and now >= 15 then
        descriptor_sampled = true
        machine:logerror(string.format('nam2_idle_timer1_destination: writes=%d destination=%04x observer_writes=0\n',
            timer1_destination_writes, memory:read_u16(0x20006)))
        for command = 0, 255 do
            if control_commands[command] then
                machine:logerror(string.format('nam2_idle_control_command: data=%02x count=%d observer_writes=0\n',
                    command, control_commands[command]))
            end
        end
        local bytes = {}
        for i = 0, 11 do
            bytes[#bytes + 1] = string.format('%02x', memory:read_u8(0x101334 + i))
        end
        machine:logerror(string.format('nam2_idle_irq3_descriptor: address=00101334 bytes=%s observer_writes=0\n', table.concat(bytes)))
    end
    if now < next_time then return end
    machine:logerror(string.format('nam2_idle_sample: t=%.6f pc=%08x ctrl=%02x fmask=%02x imask=%02x fiq=%02x irq=%02x nest=%02x saved=%02x clock=%02x timer0=%04x stamp=%04x\n',
        now, cpu.state['PC'].value, memory:read_u8(0x2000c),
        memory:read_u8(0x2000a), memory:read_u8(0x2000b), memory:read_u8(0x20008),
        memory:read_u8(0x20009), memory:read_u8(0x10f9f6), memory:read_u8(0x10f9f4), memory:read_u8(0x2000d),
        memory:read_u16(0x20010), memory:read_u16(0x10f9f8)))
    next_time = next_time + 1
end)
local directory = assert(debug.getinfo(1, 'S').source:sub(2):match('^(.*[/])'))
if not _G.nam2_c3_idle_no_early_keys then
    dofile(directory .. 'noki2100_v521_early_security_input.lua')
end
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
