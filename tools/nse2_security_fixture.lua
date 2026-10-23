-- One physical 12345 + OK attempt; no phone/SIM verifier state is written.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_boot_observe.lua')
local machine = manager.machine
local contact_start=_G.nse2_phone_code_start or 12
local submit_time=contact_start+3
assert(machine.system.name == 'nse2r4v')
local cpu = machine.devices[':maincpu']
local space = cpu.spaces.program
local comparisons = 0
local function bytes(address, length)
    if not ((address >= 0x100000 and address + length <= 0x110000) or
            (address >= 0x200000 and address + length <= 0x400000)) then return 'outside' end
    local result = {}
    for i = 0, length - 1 do result[#result + 1] = string.format('%02x', space:read_u8(address + i)) end
    return table.concat(result)
end
_G.nse2_security_compare_tap = space:install_read_tap(0x29aab4, 0x29aab7, 'nse2_security_compare', function()
    local now = machine.time:as_double()
    if cpu.state['R15'].value ~= 0x29aab4 or now < submit_time or now > submit_time+.3 then return end
    comparisons = comparisons + 1
    if comparisons > 128 then return end
    local first, second, length = cpu.state['R0'].value, cpu.state['R1'].value, cpu.state['R2'].value
    local count = math.min(length, 32)
    machine:logerror(string.format('nse2_security_compare: first=%08x second=%08x length=%d a=%s b=%s lr=%08x t=%.9f observer_writes=0\n',
        first, second, length, bytes(first, count), bytes(second, count), cpu.state['R14'].value, now))
end)
_G.nse2_security_encode_tap = space:install_read_tap(0x294454, 0x294457, 'nse2_security_encode', function()
    local now = machine.time:as_double()
    if cpu.state['R15'].value ~= 0x294454 or now < submit_time or now > submit_time+.3 then return end
    machine:logerror(string.format('nse2_security_encode: input=%s destination=%08x lr=%08x t=%.9f observer_writes=0\n',
        bytes(cpu.state['R1'].value, 6), cpu.state['R2'].value, cpu.state['R14'].value, now))
end)
_G.nse2_security_result_tap = space:install_read_tap(0x2946a4, 0x2946a7, 'nse2_security_result', function()
    local now = machine.time:as_double()
    if cpu.state['R15'].value ~= 0x2946a4 or now < submit_time or now > submit_time+.3 then return end
    machine:logerror(string.format('nse2_security_result: comparison=%08x attempts=%02x t=%.9f observer_writes=0\n',
        cpu.state['R0'].value, space:read_u8(0x10c6fc), now))
end)
_G.nse2_security_retry_tap = space:install_read_tap(0x29461c, 0x29461f, 'nse2_security_retry', function()
    local now = machine.time:as_double()
    if cpu.state['R15'].value ~= 0x29461c or now < submit_time or now > submit_time+.3 then return end
    machine:logerror(string.format('nse2_security_retry: matched=%d attempts=%02x lr=%08x t=%.9f observer_writes=0\n',
        cpu.state['R0'].value, space:read_u8(0x10c6fc), cpu.state['R14'].value, now))
end)
_G.nse2_security_return_tap = space:install_read_tap(0x2946b0, 0x2946b3, 'nse2_security_return', function()
    local now = machine.time:as_double()
    if cpu.state['R15'].value ~= 0x2946b2 or now < submit_time or now > submit_time+.3 then return end
    machine:logerror(string.format('nse2_security_return: accepted=%d attempts=%02x t=%.9f observer_writes=0\n',
        cpu.state['R0'].value, space:read_u8(0x10c6fc), now))
end)
_G.nse2_security_identity_tap = space:install_read_tap(0x2593b8, 0x2593bb, 'nse2_security_identity', function()
    local now = machine.time:as_double()
    if cpu.state['R15'].value ~= 0x2593b8 or now < submit_time or now > submit_time+.3 then return end
    machine:logerror(string.format('nse2_security_identity: destination=%08x flags=%08x lr=%08x stack=%s t=%.9f observer_writes=0\n',
        cpu.state['R0'].value, cpu.state['R1'].value, cpu.state['R14'].value,
        bytes(cpu.state['R13'].value, 64), now))
end)
local function contact(column, mask)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == mask then return field end
    end
    error('NSE-2 security contact missing')
end
local fields = {contact(2, 2), contact(3, 2), contact(4, 2), contact(2, 4), contact(3, 4), contact(1, 4)}
local events = {}
for i, field in ipairs(fields) do
    local when = i == 6 and submit_time or contact_start + (i - 1) * 0.5
    events[#events + 1] = {when, field, 1, i}
    events[#events + 1] = {when + 0.2, field, 0, i}
end
local next_event, captured = 1, false
emu.register_frame_done(function()
    local now = machine.time:as_double()
    while next_event <= #events and now >= events[next_event][1] do
        local event = events[next_event]
        event[2]:set_value(event[3])
        machine:logerror(string.format('nse2_security_contact: index=%d down=%d t=%.9f physical_only=1\n',
            event[4], event[3], now))
        next_event = next_event + 1
    end
    if not captured and now >= contact_start+5 then
        machine.screens[':screen']:snapshot('nse2_security_result.png')
        captured = true
    end
end, 'NSE-2 one physical phone-code attempt')
