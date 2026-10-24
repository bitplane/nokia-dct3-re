-- Read-only own-ROM mode/consumer observations around physical call input.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki6210_outgoing_call_input.lua')
local cpu = manager.machine.devices[':maincpu']
cpu.debug:bpset(0x426eb4, nil,
    'logerror "6210_control_compiler: selector=%02x value=%04x publish=%x lr=%08x\\n",r0,r1,r2,r14;g')
cpu.debug:bpset(0x303c30, nil,
    'logerror "6210_paired_mode: value=%02x\\n",r0;g')
cpu.debug:bpset(0x305258, nil,
    'logerror "6210_service_command: class=%02x command=%02x argument=%02x\\n",b@(r0+3),b@(r0+8),b@(r0+9);g')
