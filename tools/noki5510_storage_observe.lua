-- Own flash-journal selection and logical-cache snapshot; observation only.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'noki5510_selftest_observe.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
cpu.debug:bpset(0x2fd954, nil,
    'logerror "5510_storage_version: index=%02x base0=%08x base1=%08x version=%04x\\n",r5,d@r7,d@(r7+18),r0;g')
cpu.debug:bpset(0x2fdb22, nil,
    'logerror "5510_storage_result: accepted=%02x flags=%02x status=%04x\\n",r6,b@100033,w@10003e;g')
