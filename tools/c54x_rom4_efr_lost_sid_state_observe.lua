-- Read-only semantic boundaries in the isolated lost-SID fixtures.
local cpu = assert(manager.machine.devices[':maincpu'])
local actions = {}
local function observe(pc, kind, address, count)
    -- Debugger commands accept at most 128 parameters; RAW contains 160 words.
    for first = 0, count - 1, 16 do
        local width = math.min(16, count - first)
        local args = {}
        for i = first, first + width - 1 do args[#args + 1] = string.format('dw@%x', address + i) end
        local prefix = first == 0 and ('efr_boundary: kind=' .. kind .. ' values=') or ''
        local suffix = first + width == count and '\\n' or ''
        actions[pc] = (actions[pc] or '') .. string.format('logerror "%s%s%s",%s;',
            prefix, string.rep('%04x', width), suffix, table.concat(args, ','))
    end
end
cpu.debug:bpset(0xc8de, '1', 'logerror "efr_boundary: kind=entry\\n";g')
observe(0xc7a0, 'R2', 0xb80, 10)
observe(0xc7a0, 'LSF', 0xb94, 10)
observe(0xc809, 'pitch', 0x1303, 1)
observe(0xc85a, 'code', 0x1302, 1)
-- Original decoder return, before the caller's postprocessing.
observe(0xc907, 'RAW', 0x1388, 160)
observe(0xc907, 'LPC', 0x1317, 44)
-- Preserve the full accumulator; debugger shift literals use hexadecimal radix.
cpu.debug:bpset(0xc4d0, '1', 'logerror "efr_gain_policy: previous=%04x gain=%04x candidate_b=%010x\\n",dw@ab9,dw@abe,b;g')
for pc, action in pairs(actions) do cpu.debug:bpset(pc, '1', action .. 'g') end
