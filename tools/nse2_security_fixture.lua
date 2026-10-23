-- One physical 12345 + OK attempt; no phone/SIM verifier state is written.
local directory = debug.getinfo(1, 'S').source:sub(2):match('^(.*[/\\])')
dofile(directory .. 'nse2_boot_observe.lua')
local machine = manager.machine
assert(machine.system.name == 'nse2r4v')
local function contact(column, mask)
    for _, field in pairs(machine.ioport.ports[':COL.' .. column].fields) do
        if field.mask == mask then return field end
    end
    error('NSE-2 security contact missing')
end
local fields = {contact(2, 2), contact(3, 2), contact(4, 2), contact(2, 4), contact(3, 4), contact(1, 4)}
local events = {}
for i, field in ipairs(fields) do
    local when = i == 6 and 15 or 12 + (i - 1) * 0.5
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
    if not captured and now >= 17 then
        machine.screens[':screen']:snapshot('nse2_security_result.png')
        captured = true
    end
end, 'NSE-2 one physical phone-code attempt')
