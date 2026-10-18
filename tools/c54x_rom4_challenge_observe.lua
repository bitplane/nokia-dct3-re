-- Passive ROM4 transform probe. Fetch snapshots are context, not retirement.
-- Response write taps observe real bus writes; no firmware state is changed.
local machine = manager.machine
local cpu = assert(machine.devices[':dsp_c54x:cpu'])
local space = cpu.spaces.data
local handles = {}
local active = false
local function words(first, last)
    local result = {}
    for address = first, last do result[#result + 1] = string.format('%04x', space:read_u16(address)) end
    return table.concat(result, ',')
end
for _, address in ipairs({0x4b73, 0x7f2d, 0x7ff5, 0x7ffa, 0x8000}) do
    local count = 0
    handles[#handles + 1] = cpu.spaces.program:install_read_tap(address, address, 'challenge_fetch_' .. address,
        function(offset, value)
            if offset == 0x7f2d then active = true end
            if not active then return end
            count = count + 1
            if count > 16 then return end
            local regs = {}
            for _, name in ipairs({'A', 'B', 'T', 'ST0', 'ST1', 'BRC', 'SP', 'AR2', 'AR3', 'AR4'}) do
                regs[#regs + 1] = name .. '=' .. string.format('%x', cpu.state[name].value)
            end
            machine:logerror(string.format('challenge_fetch: site=%04x ordinal=%u op=%04x %s buffer=%s serial=%s t=%.9f context_only=1\n',
                offset, count, value, table.concat(regs, ' '), words(0x1200, 0x121f), words(0x1f0c, 0x1f0f), machine.time:as_double()))
            if offset == 0x7f2d then machine:logerror('challenge_workspace: words=' .. words(0x13c0, 0x13df) .. '\n') end
        end)
end
local writes = 0
handles[#handles + 1] = space:install_write_tap(0x1200, 0x121f, 'challenge_response', function(address, value, mask)
    if not active then return end
    writes = writes + 1
    if writes > 256 then return end
    machine:logerror(string.format('challenge_write: ordinal=%u address=%04x value=%04x mask=%04x context_pc=%04x t=%.9f\n',
        writes, address, value, mask, cpu.state.PC.value, machine.time:as_double()))
end)
_G.challenge_handles = handles
_G.challenge_stop = emu.add_machine_stop_notifier(function()
    machine:logerror(string.format('challenge_summary: writes=%u captured=%u fetch_cap_per_site=16 observer_writes=0\n', writes, math.min(writes, 256)))
end)
