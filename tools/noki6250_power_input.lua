-- Physical shutdown/restart with passive second-boot NV observations.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6250_runtime_observe.lua')
local machine = manager.machine
local memory = machine.devices[':maincpu'].spaces['program']
local power = assert(machine.ioport.ports[':PWR'].fields['Power'])
local actions = {{25, 1}, {29, 0}, {43, 1}, {45, 0}}
local captures = {{24, 'idle'}, {35, 'off'}, {60, 'restart'}, {75, 'settled'}}
local action_index, capture_index = 1, 1
emu.register_periodic(function()
    local time = machine.time:as_double()
    local action = actions[action_index]
    if action and time >= action[1] then
        machine:logerror(string.format('6250_power_physical: step=%d pressed=%d t=%.6f\n',
            action_index, action[2], time))
        power:set_value(action[2])
        action_index = action_index + 1
    end
    local capture = captures[capture_index]
    if capture and time >= capture[1] then
        machine.screens[':screen']:snapshot('6250_power_' .. capture[2] .. '.png')
        local faults = {}
        for index = 0, 23 do
            faults[#faults + 1] = string.format('%02x', memory:read_u8(0x17fbe0 + index))
        end
        machine:logerror(string.format('6250_power_endpoint: phase=%s faults=%s t=%.6f\n',
            capture[2], table.concat(faults), time))
        capture_index = capture_index + 1
    end
end)
