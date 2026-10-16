-- Passive data-write contexts, not instruction ownership or decoded PCM evidence.
local machine = manager.machine
assert(machine.system.name == 'mu4nand', 'playback bank observation requires mu4nand')
local cpu = assert(machine.devices[':cpu'])
local data = assert(cpu.spaces.data)
local contexts, context_count, overflow = {}, 0, 0
local writes, nonzero, partial = 0, 0, 0
local copy_contexts = 0
local upstream = {writes=0, nonzero=0, partial=0, contexts={}, count=0, overflow=0}
-- Inside the first processing session, after initialization and before cleanup.
-- The range is the first observed two-channel DMA block, not all decoder RAM.
local start, finish = 1.0, 2.0
machine:logerror('mu4_playback_banks_scope: first=1000 last=107f upstream_first=7000 upstream_last=73ff start=1.000000000 finish=2.000000000 boundary=data_write context_pc=not_owner observer_writes=0 audio_acceptance=0\n')
local tap = data:install_write_tap(0x1000, 0x107f, 'mu4_playback_banks', function(address, value, mask)
    local time = machine.time:as_double()
    if time < start or time >= finish then return end
    writes = writes + 1
    local driven = value & mask
    if driven ~= 0 then nonzero = nonzero + 1 end
    if mask ~= 0xffff then partial = partial + 1 end
    local pc = cpu.state.GENPC.value
    if pc == 0x2a367 and copy_contexts < 4 then
        copy_contexts = copy_contexts + 1
        local ar3, ar4 = cpu.state.AR3.value, cpu.state.AR4.value
        -- RAM-only inspection; neither live pointer is claimed as the retained read address.
        if ar3 >= 0x61 and ar4 >= 0x60 and ar4 <= 0xfff2 then
            local fields = {}
            for offset = 0, 13 do fields[#fields + 1] = string.format('%04x', data:read_u16(ar4 + offset)) end
            machine:logerror(string.format(
                'mu4_playback_copy_context: ordinal=%u destination=%04x ar3=%04x current=%04x previous=%04x ar4=%04x fields=%s context_pc=%06x source_identity=unvalidated observer_writes=0\n',
                copy_contexts, address, ar3, data:read_u16(ar3), data:read_u16(ar3 - 1), ar4,
                table.concat(fields, ','), pc))
        end
    end
    local context = contexts[pc]
    if not context and context_count < 512 then
        context = {writes=0, nonzero=0}
        contexts[pc] = context
        context_count = context_count + 1
    end
    if context then
        context.writes = context.writes + 1
        if driven ~= 0 then context.nonzero = context.nonzero + 1 end
    else
        overflow = overflow + 1
    end
    if writes <= 4 or (driven ~= 0 and nonzero <= 4) then
        machine:logerror(string.format(
            'mu4_playback_bank_write: ordinal=%u nonzero_ordinal=%u address=%04x value=%04x mask=%04x context_pc=%06x t=%.9f observer_writes=0\n',
            writes, nonzero, address, value, mask, pc, time))
    end
end)
-- Candidate source window from the bounded live AR3/object observation.
-- Its extent is a tracing selection, not a recovered physical RAM contract.
local upstream_tap = data:install_write_tap(0x7000, 0x73ff, 'mu4_playback_upstream', function(address, value, mask)
    local time = machine.time:as_double()
    if time < start or time >= finish then return end
    upstream.writes = upstream.writes + 1
    if (value & mask) ~= 0 then upstream.nonzero = upstream.nonzero + 1 end
    if mask ~= 0xffff then upstream.partial = upstream.partial + 1 end
    local pc = cpu.state.GENPC.value
    if (value & mask) ~= 0 and upstream.nonzero <= 4 then
        machine:logerror(string.format(
            'mu4_playback_upstream_write: ordinal=%u address=%04x value=%04x mask=%04x context_pc=%06x t=%.9f observer_writes=0\n',
            upstream.nonzero, address, value, mask, pc, time))
    end
    local context = upstream.contexts[pc]
    if not context and upstream.count < 512 then
        context = {writes=0, nonzero=0}
        upstream.contexts[pc] = context
        upstream.count = upstream.count + 1
    end
    if context then
        context.writes = context.writes + 1
        if (value & mask) ~= 0 then context.nonzero = context.nonzero + 1 end
    else
        upstream.overflow = upstream.overflow + 1
    end
end)
local stop = emu.add_machine_stop_notifier(function()
    local pcs = {}
    for pc in pairs(contexts) do pcs[#pcs + 1] = pc end
    table.sort(pcs)
    for _, pc in ipairs(pcs) do
        local context = contexts[pc]
        machine:logerror(string.format(
            'mu4_playback_bank_context: context_pc=%06x writes=%u nonzero_driven=%u identity=unvalidated\n',
            pc, context.writes, context.nonzero))
    end
    machine:logerror(string.format(
        'mu4_playback_banks_terminal: pc=%06x illegal=%u writes=%u nonzero_driven=%u partial=%u contexts=%u overflow=%u t=%.9f observer_writes=0 audio_acceptance=0\n',
        cpu.state.GENPC.value, cpu.state.ILLEGAL.value, writes, nonzero, partial,
        context_count, overflow, machine.time:as_double()))
    pcs = {}
    for pc in pairs(upstream.contexts) do pcs[#pcs + 1] = pc end
    table.sort(pcs)
    for _, pc in ipairs(pcs) do
        local context = upstream.contexts[pc]
        machine:logerror(string.format(
            'mu4_playback_upstream_context: context_pc=%06x writes=%u nonzero_driven=%u identity=unvalidated\n',
            pc, context.writes, context.nonzero))
    end
    machine:logerror(string.format(
        'mu4_playback_upstream_terminal: writes=%u nonzero_driven=%u partial=%u contexts=%u overflow=%u observer_writes=0 audio_acceptance=0\n',
        upstream.writes, upstream.nonzero, upstream.partial, upstream.count, upstream.overflow))
end)
_G.mu4_playback_banks_handles = {tap, upstream_tap, stop}
