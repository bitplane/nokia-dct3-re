"""Execute the retained register-only schedules and independent integer oracles."""
import shutil
import subprocess
import unittest

from tools.test_c54x_case_fixture import compiled_cpp


class RegisterUnaryTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('c++'), 'C++ compiler required')
    def test_numeric_domains_replay_classes_and_reference_results(self):
        with compiled_cpp(r'''
#include <cstdint>
#include <cassert>
#include <map>
#include <tuple>
using u16=std::uint16_t; using u32=std::uint32_t; using u64=std::uint64_t; using s64=std::int64_t;
constexpr bool BIT(u64 value,unsigned bit) { return (value>>bit)&1; }
constexpr int STATE_GENPC=-1;
enum {AS_PROGRAM,AS_DATA};
struct emu_fatalerror { template<class... A> emu_fatalerror(char const *,A...) {} };
struct r { enum {STATE_A,STATE_B,STATE_ST0,STATE_ST1,STATE_ILLEGAL,
    STATE_IDLE,STATE_PMST,STATE_TCR,STATE_IFR,STATE_IMR}; };
#include "tests/c54x/tms320c54x_test_register_unary.h"
using namespace c54x_test;
constexpr u64 mask=0xffffffffffULL;
u64 reg(instruction_case const &c,int index) {
    for(auto f:c.expected) if(f.index==index) return f.value;
    assert(false); return 0;
}
int main() {
    for(auto kind:{register_unary_kind::logic,register_unary_kind::shift,
                  register_unary_kind::negate,register_unary_kind::absolute}) {
        unsigned const count=register_unary_count(kind);
        bool const logic=kind==register_unary_kind::logic,shift=kind==register_unary_kind::shift;
        unsigned const boundaries=logic||shift?2:3;
        std::vector<unsigned> seen(count);
        std::map<std::array<unsigned,4>,std::array<unsigned,3>> groups;
        ordered_case_digest digest;
        for(unsigned n=0;n<count;++n) {
            unsigned key=register_unary_key(n,kind),point;
            if(logic) {
                point=n<3072?key/3072:(key-6144)/32;
                assert((n<3072?key%3072:3072+(key-6144)%32)==n);
            } else { point=key/count; assert(key%count==n); }
            assert(!seen[n]++ && point<boundaries); digest.add(key);
            auto c=register_unary_case<r>(n,kind);
            auto const &initial=c.states.front().observation,&last=c.states.back().observation;
            u64 a=reg(initial,r::STATE_A),b=reg(initial,r::STATE_B),st0=reg(initial,r::STATE_ST0);
            unsigned selector,flags=unsigned(reg(initial,r::STATE_ST1)),group=0;
            if(logic) {
                bool const cmpl=key>=6144;
                selector=cmpl?(key-6144)%4:(key/3)%4;
                u64 x=selector&2?b:a,y=selector&1?b:a;
                int amount=cmpl?0:int((key/12)%32)-16;
                u64 moved=amount<0?x>>-amount:(x<<amount)&mask;
                u64 result=cmpl?(~x&mask):key%3==0?y&moved:key%3==1?y|moved:y^moved;
                assert(reg(last,selector&1?r::STATE_B:r::STATE_A)==result);
                assert(reg(last,selector&1?r::STATE_A:r::STATE_B)==(selector&1?a:b));
                assert(reg(last,r::STATE_ST0)==st0); group=cmpl?3:key%3;
            } else if(!shift) {
                auto f=negate_case(key,kind==register_unary_kind::absolute);
                selector=(f.opcode>>8)&3;
                bool const negative=bool(f.source&(u64(1)<<39));
                u64 magnitude=negative?((~f.source)&mask)+1:f.source;
                s64 number=kind==register_unary_kind::absolute||negative?s64(magnitude):-s64(magnitude);
                bool overflow=number>0x7fffffffLL||number<-0x80000000LL;
                s64 saturated=number>0x7fffffffLL?0x7fffffffLL:number<-0x80000000LL?-0x80000000LL:number;
                u64 result=u64(overflow&&(flags&0x200)?saturated:number)&mask;
                assert(f.result==result && reg(last,selector&1?r::STATE_B:r::STATE_A)==result);
                assert(f.result_st0==((f.st0&~0x800)|(!f.source?0x800:0)|(overflow?(selector&1?0x200:0x400):0)));
                assert(c.states.size()==(f.prefix?3:2)); group=f.prefix;
            } else {
                selector=(key/2)%4;
                u64 x=selector&2?b:a;
                int amount=int((key/8)%32)-16;
                bool logical=key%2;
                s64 signed_value=x&(u64(1)<<39)?-s64(((~x)&mask)+1):s64(x);
                u64 result;
                if(logical) result=amount<0?u32(x)>>-amount:u32(u32(x)<<amount);
                else if(amount>=0) result=u64(signed_value*(s64(1)<<amount))&mask;
                else if((flags&0x100)&&signed_value<0)
                    result=u64(-((-signed_value+(s64(1)<<-amount)-1)/(s64(1)<<-amount)))&mask;
                else result=x>>-amount;
                if(!logical&&amount>0) {
                    s64 scaled=signed_value*(s64(1)<<amount);
                    if((flags&0x200)&&(scaled<-0x80000000LL||scaled>0x7fffffffLL))
                        result=scaled<0?0xff80000000ULL:0x007fffffffULL;
                }
                u64 status=st0;
                if(logical||amount) {
                    bool outgoing=amount&&((x>>(amount<0?-amount-1:(logical?32:39)-amount))&1);
                    status=(status&~0x800)|(outgoing?0x800:0);
                }
                if(!logical&&amount>0) {
                    s64 scaled=signed_value*(s64(1)<<amount);
                    if(scaled<-0x80000000LL||scaled>0x7fffffffLL) status|=selector&1?0x200:0x400;
                }
                assert(reg(last,r::STATE_ST0)==status);
                assert(reg(last,selector&1?r::STATE_B:r::STATE_A)==result); group=logical;
            }
            ++groups[{selector,flags,unsigned(st0),group}][point];
            assert(c.identity==key && c.forbid_data_access && c.single_fetch);
            assert(c.states.front().tick==0 && c.states[1].tick==6);
            assert(c.checkpoint<c.finish && !c.states.front().observation.reset_before);
            auto const &program=c.states.front().observation.program;
            unsigned offset=unsigned(*c.single_fetch-program.front().address);
            assert(program[offset].value==initial.opcode && program[offset+1].value==0xf4e1);
        }
        for(auto const &entry:groups) for(unsigned p=0;p<boundaries;++p) assert(entry.second[p]);
        std::printf("kind=%u cases=%u digest=%016llx\n",unsigned(kind),count,(unsigned long long)digest.value);
    }
}
''') as program:
            subprocess.run([str(program)], check=True)


if __name__ == '__main__':
    unittest.main()
