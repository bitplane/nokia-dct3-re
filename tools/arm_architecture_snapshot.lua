-- Read-only ARM7 state for acceptance fixtures; no product or input policy.
return function(machine)
    local cpu = assert(machine.devices[':maincpu'])
    local memory, sum = cpu.spaces['program'], 0
    for address = 0x100000, 0x17fffc, 4 do
        sum = ((sum << 5) - sum + memory:read_u32(address)) & 0xffffffff
    end
    local registers = {}
    for index = 0, 15 do
        registers[#registers + 1] = string.format('%08x', cpu.state['R' .. index].value)
    end
    for _, name in ipairs({'CPSR', 'FR8', 'FR9', 'FR10', 'FR11', 'FR12', 'FR13', 'FR14', 'FR16',
                           'IR13', 'IR14', 'IR16', 'SR13', 'SR14', 'SR16',
                           'AR13', 'AR14', 'AR16', 'UR13', 'UR14', 'UR16'}) do
        registers[#registers + 1] = string.format('%08x', cpu.state[name].value)
    end
    -- R15 is the saved register; named PC can be a debugger cache.
    return {time=machine.time:as_double(), pc=cpu.state['R15'].value,
            sp=cpu.state['R13'].value, ram=sum, cpu=table.concat(registers, ',')}
end
