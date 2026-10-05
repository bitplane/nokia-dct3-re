-- Read-only observations of the NSB-6 dial editor's saved-position record.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki8890_outgoing_call_input.lua')
local machine = manager.machine
local cpu = assert(machine.devices[':maincpu'])
local memory = cpu.spaces['program']
local count = 0
local tap = memory:install_write_tap(0x1324a0, 0x1324af,
    '8890_editor_saved_position', function(address, data, mask)
        if count >= 128 then return end
        count = count + 1
        machine:logerror(string.format(
            '8890_editor_saved_write: pc=%08x address=%08x data=%08x mask=%08x t=%.6f\n',
            cpu.state['PC'].value, address, data, mask, machine.time:as_double()))
    end)
_G.noki8890_editor_saved_tap = tap
cpu.debug:bpset(0x23c23c, nil,
    'logerror "8890_editor_restore: descriptor=%02x saved=%04x/%04x/%04x context=%08x\\n",b@(r5+d),w@r5,w@(r5+2),w@(r5+a),r4;g')
cpu.debug:bpset(0x25eed4, nil,
    'logerror "8890_editor_cursor_reset: context=%08x old=%04x new=%04x caller=%08x\\n",r4,w@(r4+1c),r5,r14;g')
cpu.debug:bpset(0x25ee74, nil,
    'logerror "8890_editor_save: source=%08x position=%04x/%04x caller=%08x\\n",r0,w@r0,w@(r0+2),r14;g')
