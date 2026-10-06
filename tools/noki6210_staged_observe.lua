-- Own NPE-3 verifier result consumption; observation only.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'dct3_model_scout.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
assert(cpu.debug, '6210 staged observation requires the debugger')
cpu.debug:bpset(0x426cca, nil,
    'logerror "6210_verifier_result: result0=%04x result1=%04x\\n",w@10000,w@10002;g')
cpu.debug:bpset(0x302a52, 'temp9<8',
    'temp9=temp9+1;logerror "6210_selftest_reply: command=%02x faults=%02x flag=%02x\\n",b@(r4+8),b@(r4+9),b@17fd99;g')
cpu.debug:bpset(0x302b10, 'temp6<16',
    'temp6=temp6+1;logerror "6210_service_return: command=%02x faults=%02x/%02x/%02x\\n",b@(r4+8),b@17fbef,b@17fbf0,b@17fbf1;g')
cpu.debug:bpset(0x3d72ca, 'temp5<16',
    'temp5=temp5+1;logerror "6210_analog_window: accepted=%02x sample0=%04x sample1=%04x gain=%08x offset=%08x\\n",r0,w@r5,w@r4,d@17fce0,d@17fce4;g')
