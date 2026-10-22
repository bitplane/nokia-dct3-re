-- Read-only original decoder patch-path observations in the isolated ROM4 bench.
local cpu=assert(manager.machine.devices[':maincpu'])
cpu.debug:wpset(cpu.spaces['data'],'w',0xab9,1,'1',
    'logerror "efr_history_write: pc=%04x address=%04x value=%04x bfi=%04x flags=%04x\\n",pc,wpaddr,wpdata,dw@1343,dw@ac8;g')
for _,pc in ipairs({0xc8de,0x2697,0x269e,0x2870,0x28b0,0x28dc,0x28f7,0xc8ff}) do
    cpu.debug:bpset(pc,'1',string.format(
        'logerror "efr_homing_path: pc=%%04x bfi=%%04x flags=%%04x recovery=%%04x parameter=%%04x ar2=%%04x sp=%%04x\\n",pc,dw@1343,dw@ac8,dw@ab9,dw@1371,ar2,sp;g'))
end
