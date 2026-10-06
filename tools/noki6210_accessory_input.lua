-- Physical Menu fixture plus read-only accessory decision observation.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki6210_menu_input.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
cpu.debug:bpset(0x39d6b2, nil,
    'logerror "6210_accessory_decision: state=%02x sample=%04x\\n",b@r5,w@r6;g')
