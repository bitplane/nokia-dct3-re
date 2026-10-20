-- Physical card traffic and own logical-key return values; no guest writes.
if _G.nsk3_sim_observed then return end
_G.nsk3_sim_observed=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsk3_ui_observe.lua')
local machine=manager.machine
local cpu=machine.devices[':maincpu']
local memory=cpu.spaces.program
local tx,rx=0,0
local tx_limit=_G.nsk3_sim_tx_limit or 1024
_G.nsk3_sim_tx_tap=memory:install_write_tap(0x20034,0x20037,'nsk3_sim_tx',function(offset,value,mask)
    if mask & 0x0000ff00 ==0 then return end
    tx=tx+1
    if tx<=tx_limit then machine:logerror(string.format('nsk3_sim_tx: index=%d value=%02x t=%.9f observer_writes=0\n',tx,(value>>8)&0xff,machine.time:as_double())) end
end)
_G.nsk3_sim_rx_tap=memory:install_read_tap(0x20034,0x20037,'nsk3_sim_rx',function(offset,value,mask)
    if mask & 0x000000ff ==0 then return end
    rx=rx+1
    if rx<=2048 then machine:logerror(string.format('nsk3_sim_rx: index=%d value=%02x t=%.9f observer_writes=0\n',rx,value&0xff,machine.time:as_double())) end
end)
for _,site in ipairs({0x2ac2ec,0x2ac2fc}) do
    machine.debugger:command(string.format('bpset %x,temp8<100 && r0!=3e,{temp8=temp8+1;logerror "nsk3_logical_key: site=%08x key=%%02x cycles=%%016x observer_writes=0\\n",r0,totalcycles;g}',site,site))
end
