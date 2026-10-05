-- Own-ROM DSP RX probes; inputs remain physical and the payload is peer-owned.
local source = debug.getinfo(1, 'S').source:sub(2)
local directory = assert(source:match('^(.*[/])'))
dofile(directory .. 'noki8850_security_input.lua')
local cpu = assert(manager.machine.devices[':maincpu'])
cpu.debug:bpset(0x307352, 'temp8<200',
    'temp8=temp8+1;logerror "8850_radio_dispatch type=%02x length=%02x\\n",b@(r4+3),b@(r4+2);g')
for _, entry in ipairs({
    {0x2df198, 'received_block'},
    {0x2df56c, 'rssi_result'},
    {0x2df2f8, 'channel_confirmation'},
}) do
    cpu.debug:bpset(entry[1], nil,
        'logerror "8850_radio_handler ' .. entry[2] ..
        ' type=%02x body0=%02x\\n",b@(r0+3),b@(r0+4);g')
end
