-- Read-only own-ROM mode/consumer observations around physical call input.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki6210_outgoing_call_input.lua')
local cpu = manager.machine.devices[':maincpu']
cpu.debug:bpset(0x426eb4, nil,
    'logerror "6210_control_compiler: selector=%02x value=%04x publish=%x lr=%08x context=%02x state=%02x source=%08x cache=%08x\\n",r0,r1,r2,r14,b@16ed5c,b@16ed5a,r5,r7;g')
cpu.debug:bpset(0x303c30, nil,
    'logerror "6210_paired_mode: value=%02x\\n",r0;g')
cpu.debug:bpset(0x305258, nil,
    'logerror "6210_service_command: class=%02x command=%02x argument=%02x\\n",b@(r0+3),b@(r0+8),b@(r0+9);g')
cpu.debug:bpset(0x41a944, nil,
    'logerror "6210_parameter_context_set: value=%02x lr=%08x\\n",r0,r14;g')
cpu.debug:wpset(cpu.spaces['program'], 'w', 0x16ed5c, 1, nil,
    'logerror "6210_parameter_context_write: pc=%08x data=%08x\\n",pc,wpdata;g')
_G.npe3_catalogue_parent_observer = true
dofile(directory .. 'noki6210_upload_catalogue_observe.lua')
