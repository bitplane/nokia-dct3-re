-- Shared read-only SIM UART observer for composed physical-input fixtures.
if _G.nsm1_sim_observed then return end
_G.nsm1_sim_observed=true
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsm1_analog_observe.lua')
local machine=manager.machine
local memory=machine.devices[':maincpu'].spaces.program
local tx=0
_G.nsm1_sim_tx_tap=memory:install_write_tap(0x20034,0x20037,'nsm1_sim_tx',function(_,value,mask)
    if mask & 0x0000ff00==0 then return end
    tx=tx+1
    if tx<=4096 then machine:logerror(string.format('nsm1_sim_tx: index=%d value=%02x observer_writes=0\n',tx,(value>>8)&0xff)) end
end)
