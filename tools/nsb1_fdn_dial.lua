-- Own single-Navi physical dial action; no network connection claim.
local digits={[1]={2,2},[2]={3,2},[3]={4,2},[4]={2,4},[5]={3,4},[6]={4,4}}
_G.nsb1_fdn_contacts={}
for _,digit in ipairs(_G.nsb1_fdn_dial_unlisted and {5,4,3,2,1} or {1,2,3,4,6}) do
    _G.nsb1_fdn_contacts[#_G.nsb1_fdn_contacts+1]=digits[digit]
end
_G.nsb1_fdn_contacts[#_G.nsb1_fdn_contacts+1]={1,4}
_G.nsb1_fdn_frames={35,36.5,37,39,41,43}
local source=debug.getinfo(1,'S').source:sub(2)
dofile(source:match('^(.*[/\\])') .. 'nsb1_fdn_input.lua')
