-- Passive own-ROM descriptor/wire comparison; no firmware or MMIO writes.
local machine = manager.machine
local cpu = machine.devices[':maincpu']
local memory = cpu.spaces['program']
local handles = {}
local function bytes(pointer, count)
    local result = {}
    for index = 0, count - 1 do
        result[#result + 1] = string.format('%02x', memory:read_u8(pointer + index))
    end
    return table.concat(result)
end
for _, address in ipairs({0x2b3910, 0x3052b2}) do
    local count = 0
    handles[#handles + 1] = memory:install_read_tap(address & ~3,
        (address & ~3) + 3, 'channel_config_' .. address,
        function(offset, value, mask)
            if cpu.state['PC'].value ~= address or count >= 64 then return end
            if address == 0x3052b2 and cpu.state['R14'].value ~= 0x2b39b5 then return end
            count = count + 1
            local descriptor = cpu.state[address == 0x2b3910 and 'R0' or 'R5'].value
            if descriptor < 0x100000 or descriptor >= 0x17ffe8 then return end
            local packet = 'none'
            if address == 0x3052b2 then
                local pointer = cpu.state['R0'].value
                if pointer >= 0x100000 and pointer < 0x17ffe8 then
                    packet = bytes(pointer, 24)
                end
            end
            machine:logerror(string.format(
                '8210_channel_config: pc=%08x descriptor=%08x data=%s packet=%s t=%.6f\n',
                address, descriptor, bytes(descriptor, 24), packet,
                machine.time:as_double()))
        end)
end
_G.nsm3_channel_config_handles = handles
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8210_registration_input.lua')
