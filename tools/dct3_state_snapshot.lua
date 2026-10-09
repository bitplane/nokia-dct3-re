-- Read-only architectural snapshot for the modeled 512-KiB MCU RAM window.
return function(machine, cpu, prefix)
    local memory = cpu.spaces['program']
    return function(event)
        local sum = 0
        for address = 0x100000, 0x17fffc, 4 do
            sum = ((sum << 5) - sum + memory:read_u32(address)) & 0xffffffff
        end
        local state = {machine.time:as_double(), cpu.state['R15'].value,
            cpu.state['R13'].value, sum}
        machine:logerror(string.format('%s: event=%s pc=%08x sp=%08x ram=%08x t=%.9f\n',
            prefix, event, state[2], state[3], state[4], state[1]))
        return state
    end
end
