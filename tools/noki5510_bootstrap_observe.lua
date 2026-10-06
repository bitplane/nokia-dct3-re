-- Own NPM-5 bootstrap consumers; read-only, no borrowed completion event.
local source = debug.getinfo(1, 'S').source:sub(2)
dofile(assert(source:match('^(.*[/])')) .. 'dct3_model_scout.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
assert(cpu.debug, '5510 bootstrap observation requires the debugger')
cpu.debug:bpset(0x3af680, nil,
    'logerror "5510_startup_adc: sample=%04x\\n",r0;g')
cpu.debug:bpset(0x319032, nil,
    'logerror "5510_verifier_enter: descriptor=%08x\\n",d@1237f0;g')
cpu.debug:bpset(0x319122, nil,
    'logerror "5510_verifier_result: word0=%04x word1=%04x\\n",w@10000,w@10002;g')
cpu.debug:bpset(0x319284, nil,
    'logerror "5510_loader_enter: descriptor=%08x\\n",d@123784;g')
cpu.debug:bpset(0x24ca36, nil,
    'logerror "5510_service_reply: command=%02x faults=%02x flag=%02x\\n",b@(r4+8),b@(r4+9),b@13fe5d;g')
