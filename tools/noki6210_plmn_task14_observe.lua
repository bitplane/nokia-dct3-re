-- Bounded own-ROM task-14 publication census; observations only.
local machine = manager.machine
assert(machine.system.name == 'npe3hle', 'requires NPE-3 research-HLE composition')
local cpu = assert(machine.devices[':maincpu'])
local space = cpu.spaces['program']
assert(cpu.debug, 'PLMN publisher instruction probes require the debugger')
for _, pc in ipairs({0x31555e, 0x315560, 0x315564}) do
    cpu.debug:bpset(pc, nil, string.format(
        'logerror "plmn_publisher_instruction: pc=%08x r0=%%08x r14=%%08x\\n",r0,r14;g', pc))
end
cpu.debug:bpset(0x3cc660, 'r0==3f5 || r0==3fa',
    'logerror "plmn_publisher_instruction: pc=003cc660 r0=%08x r14=%08x\\n",r0,r14;g')
cpu.debug:bpset(0x3131f0, nil,
    'logerror "plmn_03f5_selector: input=%08x\\n",d@(r13+8);g')
cpu.debug:bpset(0x2a393c, nil,
    'logerror "plmn_07d7_selector: input=%08x\\n",d@(r13+4);g')
cpu.debug:bpset(0x4cfc34, nil,
    'logerror "plmn_search_stage_entry: input=%08x stage=%04x\\n",r0,w@175454;g')
cpu.debug:bpset(0x4cfdb0, nil,
    'logerror "plmn_search_stage_return: result=%08x stage=%04x\\n",r0,w@175454;g')
local busy, posts, inputs = false, 0, 0
for _, pc in ipairs({0x38b6ae, 0x38b79c}) do
    cpu.debug:bpset(pc, nil, string.format(
        'logerror "plmn_measurement_counts: pc=%08x r0=%%08x r1=%%08x owner=%%08x state=%%08x counts=%%04x,%%04x,%%04x\\n",r0,r1,d@1711dc,d@(d@1711dc),w@(d@1711dc+4),w@(d@1711dc+6),w@(d@1711dc+8);g', pc))
end
local controller_posts = 0
local search_requests = 0
local input_taps, watched = {}, {}
local owner_taps, owner_watched = {}, {}
local owner_pointer_tap = space:install_write_tap(0x1711dc, 0x1711df,
    'plmn_controller_owner', function(address, value, mask)
        if address ~= 0x1711dc or mask ~= 0xffffffff then return end
        local owner = value & 0xffffffff
        if owner < 0x100000 or owner + 4 >= 0x180000 or owner_watched[owner] then return end
        owner_watched[owner] = true
        machine:logerror(string.format('plmn_controller_owner: owner=%08x pc=%08x t=%.9f\n',
            owner, cpu.state['PC'].value, machine.time:as_double()))
        local writes = 0
        owner_taps[#owner_taps + 1] = space:install_write_tap(owner, owner + 3,
            'plmn_controller_state_' .. owner, function(offset, data, lanes)
                if busy or offset ~= owner or writes >= 24 then return end
                busy = true
                local current = space:read_u32(0x1711dc)
                busy = false
                if current ~= owner then return end
                writes = writes + 1
                machine:logerror(string.format(
                    'plmn_controller_state_write: owner=%08x value=%08x mask=%08x pc=%08x t=%.9f\n',
                    owner, data, lanes, cpu.state['PC'].value, machine.time:as_double()))
            end)
    end)
local tap = space:install_read_tap(0x3c22c0, 0x3c22c3,
    'plmn_task14_census', function(address)
        if busy or address ~= 0x3c22c0 or cpu.state['PC'].value ~= 0x3c22c0 then return end
        local task = cpu.state['R0'].value
        if task ~= 14 and inputs >= 24 and controller_posts >= 24 then return end
        local object = cpu.state['R1'].value
        if object < 0x100000 or object + 9 >= 0x180000 then return end
        busy = true
        local status = space:read_u16(object)
        busy = false
        if task == 3 and status == 2 and search_requests < 12 then
            busy = true
            local kind = space:read_u8(object + 3)
            local words = {space:read_u32(object), space:read_u32(object + 4)}
            busy = false
            if kind == 0x57 then
                search_requests = search_requests + 1
                machine:logerror(string.format(
                    'plmn_search_request: object=%08x data=%08x%08x caller=%08x t=%.9f\n',
                    object, words[1], words[2], cpu.state['R14'].value, machine.time:as_double()))
                local read = false
                input_taps[#input_taps + 1] = space:install_read_tap(object, object + 3,
                    'plmn_search_request_' .. search_requests, function(offset, data, lanes)
                        if busy or read then return end
                        read = true
                        machine:logerror(string.format(
                            'plmn_search_request_read: object=%08x pc=%08x t=%.9f\n',
                            object, cpu.state['PC'].value, machine.time:as_double()))
                    end)
            end
        end
        if status == 0x3f5 and cpu.state['R14'].value == 0x3cc9fd then
            local stack = cpu.state['R13'].value
            if stack >= 0x100000 and stack + 20 < 0x180000 then
                busy = true
                local saved = space:read_u32(stack + 16)
                busy = false
                machine:logerror(string.format(
                    'plmn_publisher_return: status=%04x task=%d saved=%08x t=%.9f\n',
                    status, task, saved, machine.time:as_double()))
            end
        end
        local time = machine.time:as_double()
        if (status == 0x3f9 or status == 0x1802) and time >= 9 and time <= 15
            and controller_posts < 24 then
            controller_posts = controller_posts + 1
            busy = true
            local kind = space:read_u8(object + 3)
            busy = false
            machine:logerror(string.format(
                'plmn_controller_post: status=%04x kind=%02x task=%d object=%08x caller=%08x t=%.9f\n',
                status, kind, task, object, cpu.state['R14'].value, time))
        end
        if (status == 0x7d2 or status == 0x7d4 or status == 0x9c6 or status == 0x9c8
            or status == 0x1589 or status == 0x9fd or status == 0x419 or status == 0x7f4
            or status == 0x9cb or status == 0x7d7 or status == 0x9c9)
            and inputs < 24 then
            inputs = inputs + 1
            machine:logerror(string.format(
                'plmn_input_post: status=%04x task=%d object=%08x caller=%08x t=%.9f\n',
                status, task, object, cpu.state['R14'].value, machine.time:as_double()))
            if not watched[object] then
                watched[object] = true
                local reads = 0
                input_taps[#input_taps + 1] = space:install_read_tap(object, object + 3,
                    'plmn_input_header_' .. object, function(offset, value, mask)
                        -- Later reads may belong to an allocator reuse, not this message.
                        if busy or reads >= 1 then return end
                        reads = reads + 1
                        machine:logerror(string.format(
                            'plmn_input_read: object=%08x pc=%08x value=%08x mask=%08x t=%.9f\n',
                            object, cpu.state['PC'].value, value, mask, machine.time:as_double()))
                    end)
            end
        end
        if task ~= 14 or posts >= 160 then return end
        posts = posts + 1
        machine:logerror(string.format(
            'plmn_task14_census: status=%04x caller=%08x t=%.9f\n',
            status, cpu.state['R14'].value, machine.time:as_double()))
    end)
local publishers = 0
local publisher_tap = space:install_read_tap(0x3cc660, 0x3cc663,
    'plmn_status_publisher', function(address)
        if address ~= 0x3cc660 or cpu.state['PC'].value ~= 0x3cc660
            or publishers >= 24 then return end
        local status = cpu.state['R0'].value
        if status ~= 0x3ea and status ~= 0x3ec and status ~= 0x3f5 then return end
        publishers = publishers + 1
        if status == 0x3f5 and not busy then
            busy = true
            local literal = space:read_u32(0x3158fc)
            local code = space:read_u32(0x31555c)
            busy = false
            machine:logerror(string.format(
                'plmn_publisher_code: literal=%08x code=%08x t=%.9f\n',
                literal, code, machine.time:as_double()))
        end
        machine:logerror(string.format(
            'plmn_status_publisher: status=%04x caller=%08x t=%.9f\n',
            status, cpu.state['R14'].value, machine.time:as_double()))
    end)
local switches = 0
local switch_tap = space:install_read_tap(0x3151d8, 0x3151db,
    'plmn_status_switch', function(address)
        if busy or address ~= 0x3151d8 or cpu.state['PC'].value ~= 0x3151da
            or switches >= 12 then return end
        local stack = cpu.state['R13'].value
        if stack < 0x100000 or stack + 12 >= 0x180000 then return end
        busy = true
        local input = space:read_u32(stack + 8)
        busy = false
        switches = switches + 1
        machine:logerror(string.format(
            'plmn_status_switch: input=%08x context=%08x t=%.9f\n',
            input, cpu.state['R4'].value, machine.time:as_double()))
    end)
local dispatches = 0
local dispatcher_tap = space:install_read_tap(0x29ffb8, 0x29ffbb,
    'plmn_input_dispatcher', function(address)
        local time = machine.time:as_double()
        if address ~= 0x29ffb8 or cpu.state['PC'].value ~= 0x29ffb8
            or time < 9 or time > 11 or dispatches >= 64 then return end
        dispatches = dispatches + 1
        machine:logerror(string.format(
            'plmn_input_dispatcher: input=%08x arg1=%08x caller=%08x t=%.9f\n',
            cpu.state['R0'].value, cpu.state['R1'].value,
            cpu.state['R14'].value, time))
    end)
local decision_taps = {}
for _, pc in ipairs({0x2e6da6, 0x2e6e18, 0x2e6ec0, 0x2e374a, 0x2e374c,
                    0x2e321c, 0x2e3222, 0x2e6e38, 0x2e6e44, 0x2e6e9a,
                    0x2e6e72, 0x2e6eec, 0x2e6e50, 0x2e4f70,
                    0x2e4f7c, 0x2e50c4, 0x2e6046, 0x2e60a2, 0x2e5c74,
                    0x2e4e62, 0x2e4e70, 0x2e6e68, 0x2a31ca, 0x2a3948,
                    0x2d1118, 0x2d111a, 0x4cfdd4, 0x313218,
                    0x2d0d50, 0x2d1048, 0x2d2952, 0x2d2528, 0x2d0ee4,
                    0x2d0f6e, 0x38b5c6, 0x2d1068, 0x2d0902}) do
    local hits = 0
    decision_taps[#decision_taps + 1] = space:install_read_tap(pc & ~3, (pc & ~3) + 3,
        'plmn_decision_' .. pc, function(address)
            if address ~= (pc & ~3) or cpu.state['PC'].value ~= pc or hits >= 12 then return end
            hits = hits + 1
            machine:logerror(string.format(
                'plmn_decision: pc=%08x r0=%08x r4=%08x t=%.9f\n',
                pc, cpu.state['R0'].value, cpu.state['R4'].value, machine.time:as_double()))
            if pc == 0x2e6e68 then
                machine:logerror(string.format('plmn_state2_input: status=%08x t=%.9f\n',
                    cpu.state['R6'].value, machine.time:as_double()))
            end
            if pc == 0x38b5c6 then
                machine:logerror(string.format(
                    'plmn_controller_reset_call: input=%08x caller=%08x t=%.9f\n',
                    cpu.state['R0'].value, cpu.state['R14'].value, machine.time:as_double()))
            end
            if pc == 0x2d0902 then
                machine:logerror(string.format('plmn_controller_entry: status=%08x t=%.9f\n',
                    cpu.state['R1'].value, machine.time:as_double()))
            end
            if not busy and pc == 0x2d0f6e then
                busy = true
                local owner = space:read_u32(0x1711dc)
                local state = nil
                if owner >= 0x100000 and owner + 4 < 0x180000 then
                    state = space:read_u32(owner)
                end
                busy = false
                machine:logerror(string.format(
                    'plmn_controller_state: owner=%08x state=%s t=%.9f\n',
                    owner, state and string.format('%08x', state) or 'unknown',
                    machine.time:as_double()))
            end
            if not busy and pc == 0x2d2528 then
                local context = cpu.state['R4'].value
                if context >= 0x100000 and context + 12 < 0x180000 then
                    busy = true
                    local header = space:read_u16(context + 4)
                    local kind = space:read_u8(context)
                    local object = space:read_u32(context + 8)
                    local owner = space:read_u32(0x1711dc)
                    local state = nil
                    if owner >= 0x100000 and owner + 4 < 0x180000 then
                        state = space:read_u32(owner)
                    end
                    busy = false
                    machine:logerror(string.format(
                        'plmn_controller_input: status=%08x header=%04x kind=%02x object=%08x t=%.9f\n',
                        cpu.state['R1'].value, header, kind, object, machine.time:as_double()))
                    machine:logerror(string.format(
                        'plmn_controller_guard: owner=%08x state=%s t=%.9f\n',
                        owner, state and string.format('%08x', state) or 'unknown',
                        machine.time:as_double()))
                end
            end
            if not busy and pc == 0x2a31ca then
                local stack = cpu.state['R13'].value
                if stack >= 0x100000 and stack + 8 < 0x180000 then
                    busy = true
                    local status = space:read_u32(stack + 4)
                    busy = false
                    machine:logerror(string.format('plmn_completion_dispatch: input=%08x t=%.9f\n',
                        status, machine.time:as_double()))
                end
            end
            local context = cpu.state['R4'].value
            if not busy and (pc == 0x2e6e44 or pc == 0x2e4f70 or pc == 0x2e4f7c
                or pc == 0x2e50c4) and context >= 0x100000 and context + 0x64 < 0x180000 then
                busy = true
                local word60, word38, word50 = space:read_u32(context + 0x60),
                    space:read_u32(context + 0x38), space:read_u32(context + 0x50)
                busy = false
                machine:logerror(string.format(
                    'plmn_continuation_context: pc=%08x word60=%08x word38=%08x word50=%08x t=%.9f\n',
                    pc, word60, word38, word50, machine.time:as_double()))
            end
            if not busy and (pc == 0x2e374a or pc == 0x2e374c) then
                local stack = cpu.state['R13'].value
                if stack >= 0x100000 and stack + 12 < 0x180000 then
                    busy = true
                    local stored = space:read_u16(0x16fe90)
                    local first = space:read_u16(stack + 4)
                    local second = space:read_u16(stack + 8)
                    busy = false
                    machine:logerror(string.format(
                        'plmn_compare_pairs: pc=%08x stored=%04x first=%04x second=%04x t=%.9f\n',
                        pc, stored, first, second, machine.time:as_double()))
                end
            end
        end)
end
local constructors = 0
local constructor_tap = space:install_read_tap(0x2e2e60, 0x2e2e63,
    'plmn_input_constructor', function(address)
        if address ~= 0x2e2e60 or cpu.state['PC'].value ~= 0x2e2e60
            or constructors >= 24 then return end
        local status = cpu.state['R0'].value
        if status ~= 0x9c6 and status ~= 0x9c8 and status ~= 0x9cb and status ~= 0x9c9 then return end
        constructors = constructors + 1
        machine:logerror(string.format(
            'plmn_input_constructor: status=%08x caller=%08x t=%.9f\n',
            status, cpu.state['R14'].value, machine.time:as_double()))
    end)
local translations = 0
local translator_tap = space:install_read_tap(0x2a086c, 0x2a086f,
    'plmn_input_translator', function(address)
        if address ~= 0x2a086c or cpu.state['PC'].value ~= 0x2a086c
            or translations >= 32 then return end
        translations = translations + 1
        machine:logerror(string.format(
            'plmn_input_translator: input=%08x caller=%08x t=%.9f\n',
            cpu.state['R0'].value, cpu.state['R14'].value, machine.time:as_double()))
    end)
local selections = 0
local selection_tap = space:install_read_tap(0x2a0604, 0x2a0607,
    'plmn_input_selection', function(address)
        if address ~= 0x2a0604 or cpu.state['PC'].value ~= 0x2a0606
            or selections >= 24 then return end
        selections = selections + 1
        machine:logerror(string.format(
            'plmn_input_selection: input=%08x status=%08x t=%.9f\n',
            cpu.state['R5'].value, cpu.state['R4'].value, machine.time:as_double()))
    end)
_G.plmn_task14_census_observer = {
    owner_pointer_tap = owner_pointer_tap, owner_taps = owner_taps,
    tap = tap, publisher_tap = publisher_tap, switch_tap = switch_tap,
    input_taps = input_taps, dispatcher_tap = dispatcher_tap,
    selection_tap = selection_tap, translator_tap = translator_tap,
    constructor_tap = constructor_tap, decision_taps = decision_taps}
