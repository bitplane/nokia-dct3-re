-- Read-only first-speech history observations in the isolated ROM4 bench.
local cpu = assert(manager.machine.devices[':maincpu'])
for _, pc in ipairs({0xc78f, 0xc793, 0xc79b, 0xc79d}) do
    cpu.debug:bpset(pc, '1', string.format(
        'logerror "efr_dtx_history: pc=%%04x flags=%%04x history=%%04x state=%%04x st0=%%04x a=%%08x\\n",pc,dw@9c0,dw@ab9,dw@aba,st0,a;g'))
end
for _, pc in ipairs({0x28b0, 0x28f7}) do
    cpu.debug:bpset(pc, '1', string.format(
        'logerror "efr_dtx_recovery: pc=%%04x flags=%%04x history=%%04x bfi=%%04x sid=%%04x\\n",pc,dw@9c0,dw@ab9,dw@1343,dw@79;g'))
end
