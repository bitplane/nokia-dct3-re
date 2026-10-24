-- Read-only semantic boundaries in the isolated lost-SID fixtures.
local cpu = assert(manager.machine.devices[':maincpu'])
local actions = {}
local function observe(pc, kind, address, count)
    local args = {}
    for i = 0, count - 1 do args[#args + 1] = string.format('dw@%x', address + i) end
    actions[pc] = (actions[pc] or '') .. string.format('logerror "efr_boundary: kind=%s values=%s\\n",%s;',
        kind, string.rep('%04x', count), table.concat(args, ','))
end
cpu.debug:bpset(0xc8de, '1', 'logerror "efr_boundary: kind=entry\\n";g')
observe(0xc7a0, 'R2', 0xb80, 10)
observe(0xc7a0, 'LSF', 0xb94, 10)
observe(0xc809, 'pitch', 0x1303, 1)
observe(0xc85a, 'code', 0x1302, 1)
-- Preserve the full accumulator; debugger shift literals use hexadecimal radix.
cpu.debug:bpset(0xc4d0, '1', 'logerror "efr_gain_policy: previous=%04x gain=%04x candidate_b=%010x\\n",dw@ab9,dw@abe,b;g')
for pc, action in pairs(actions) do cpu.debug:bpset(pc, '1', action .. 'g') end
