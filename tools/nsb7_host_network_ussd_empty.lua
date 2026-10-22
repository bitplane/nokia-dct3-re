-- Dismiss the actual editor; firmware returns an empty ReturnResult.
_G.nsb7_ussd_snapshots={23}
_G.nsb7_ussd_actions={
    {24,'End'}, {30,'Left Softkey / Menu'}, {32,'Left Softkey / Menu'},
    {34,'End'}, {40,'End'}, {42,'Left Softkey / Menu'},
}
_G.nsb7_ussd_finish=45
_G.nsb7_ussd_response_times={29,31,33,35,37,41,43}
local directory=assert(debug.getinfo(1,'S').source:match('^@(.*/)'))
dofile(directory..'nsb7_host_ussd_observe.lua')
