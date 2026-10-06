-- Own NSM-3 bootstrap selection, observation only.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'dct3_model_scout.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
assert(cpu.debug, '8210 staged observation requires the debugger')
cpu.debug:bpset(0x2cac80, nil,
    'logerror "8210_verifier_descriptor: pointer=%08x\\n",d@135808;g')
cpu.debug:bpset(0x2cadd0, nil,
    'logerror "8210_verifier_result: result0=%04x result1=%04x\\n",w@10000,w@10002;g')
cpu.debug:bpset(0x240e20, 'temp9<8',
    'temp9=temp9+1;logerror "8210_selftest_reply: command=%02x faults=%02x flag=%02x\\n",b@(r4+8),b@(r4+9),b@13fde1;g')
cpu.debug:bpset(0x240ec4, 'temp6<16',
    'temp6=temp6+1;logerror "8210_service_return: command=%02x faults=%02x/%02x/%02x\\n",b@(r4+8),b@13fbef,b@13fbf0,b@13fbf1;g')
