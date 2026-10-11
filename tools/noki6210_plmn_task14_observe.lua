-- Bounded own-ROM task-14 publication census; observations only.
local machine = manager.machine
assert(machine.system.name == 'npe3hle', 'requires NPE-3 research-HLE composition')
local cpu = assert(machine.devices[':maincpu'])
local space = cpu.spaces['program']
local busy, posts, inputs = false, 0, 0
local input_taps, watched = {}, {}
local tap = space:install_read_tap(0x3c22c0, 0x3c22c3,
    'plmn_task14_census', function(address)
        if busy or address ~= 0x3c22c0 or cpu.state['PC'].value ~= 0x3c22c0 then return end
        local task = cpu.state['R0'].value
        if task ~= 14 and inputs >= 24 then return end
        local object = cpu.state['R1'].value
        if object < 0x100000 or object + 9 >= 0x180000 then return end
        busy = true
        local status = space:read_u16(object)
        busy = false
        if (status == 0x7d2 or status == 0x7d4 or status == 0x9c6 or status == 0x9c8)
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
        if status ~= 0x3ea and status ~= 0x3ec then return end
        publishers = publishers + 1
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
    tap = tap, publisher_tap = publisher_tap, switch_tap = switch_tap,
    input_taps = input_taps, dispatcher_tap = dispatcher_tap,
    selection_tap = selection_tap, translator_tap = translator_tap}
