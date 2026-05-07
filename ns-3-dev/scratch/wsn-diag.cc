#include "ns3/core-module.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>
using namespace ns3;
NS_LOG_COMPONENT_DEFINE("WsnDiag");

static constexpr double   kEelec=50e-9, kEamp=100e-12, kEidle=40e-9;
static constexpr double   kInit=5.0, kTE=0.1, kPT=10.0;
static constexpr uint32_t kData=800, kCtrl=160, kFrames=5, kMCS=15;
static constexpr double   kLP=0.05, kLECP=0.10;

enum Proto { LEACH, LECMAC };

struct Log { std::string op; double cost; };
struct Node {
    double x,y,e,e0;
    bool alive,isCH;
    int chId;
    uint32_t lastCH;
    std::vector<int> members;
    std::vector<Log> log;
};

static double D(double x1,double y1,double x2,double y2)
{ double a=x1-x2,b=y1-y2; return std::sqrt(a*a+b*b); }
static double TX(uint32_t b,double d){ return b*kEelec+b*kEamp*d*d; }
static double RX(uint32_t b){ return b*kEelec; }

static void use(std::vector<Node>&ns,uint32_t id,double amt,const std::string&op){
    if(id>=ns.size()||!ns[id].alive)return;
    ns[id].e-=amt; ns[id].log.push_back({op,amt});
    if(ns[id].e<=0){ns[id].e=0;ns[id].alive=false;}
}

static std::vector<Node> run(Proto proto, uint32_t seed){
    RngSeedManager::SetSeed(seed); RngSeedManager::SetRun(1);
    auto rnd=CreateObject<UniformRandomVariable>();
    rnd->SetAttribute("Min",DoubleValue(0)); rnd->SetAttribute("Max",DoubleValue(100));
    std::vector<Node> ns(200);
    for(auto&n:ns){n.x=rnd->GetValue();n.y=rnd->GetValue();n.e=n.e0=kInit;n.alive=true;n.isCH=false;n.chId=-1;n.lastCH=0;}
    rnd->SetAttribute("Max",DoubleValue(1));

    std::vector<uint32_t> chs;
    if(proto==LEACH){
        double thr=kLP; // round 1 rMod=0 => thr=kLP/1=kLP
        for(uint32_t i=0;i<200;++i){
            if(!ns[i].alive)continue;
            if(rnd->GetValue()<thr){ns[i].isCH=true;ns[i].lastCH=1;chs.push_back(i);}
        }
    } else {
        for(uint32_t i=0;i<200;++i){
            auto&n=ns[i];
            if(!n.alive||n.e<=kTE)continue;
            if(rnd->GetValue()>=kLECP)continue;
            bool close=false;
            for(uint32_t c:chs) if(D(n.x,n.y,ns[c].x,ns[c].y)<kPT){close=true;break;}
            if(close)continue;
            n.isCH=true; chs.push_back(i);
        }
    }
    if(chs.empty()){
        double best=-1;uint32_t bi=0;
        for(uint32_t i=0;i<200;++i) if(ns[i].alive&&ns[i].e>best){best=ns[i].e;bi=i;}
        ns[bi].isCH=true;chs.push_back(bi);
    }

    // join
    for(uint32_t i=0;i<200;++i){
        auto&n=ns[i]; if(!n.alive||n.isCH)continue;
        double best=1e18; int bc=-1;
        for(uint32_t c:chs){
            if(!ns[c].alive)continue;
            if(proto==LECMAC&&ns[c].members.size()>=kMCS)continue;
            double d=D(n.x,n.y,ns[c].x,ns[c].y);
            if(d<best){best=d;bc=(int)c;}
        }
        n.chId=bc; if(bc>=0)ns[bc].members.push_back((int)i);
    }

    double bcast=std::sqrt(2.0)*100;
    // setup
    for(uint32_t c:chs){
        if(!ns[c].alive)continue;
        use(ns,c,TX(kCtrl,bcast),"ADV-TX");
        use(ns,c,TX(kCtrl,bcast),"INV-TX");
        use(ns,c,TX(kData,bcast),"SCHED-TX");
    }
    for(uint32_t i=0;i<200;++i){
        auto&n=ns[i]; if(!n.alive||n.isCH)continue;
        use(ns,i,RX(kCtrl),"ADV-RX");
        use(ns,i,RX(kCtrl),"INV-RX");
        if(n.chId>=0){
            double d=D(n.x,n.y,ns[n.chId].x,ns[n.chId].y);
            use(ns,i,TX(kCtrl,d),"JOIN-TX");
            use(ns,(uint32_t)n.chId,RX(kCtrl),"JOIN-RX");
            use(ns,i,RX(kData),"SCHED-RX");
        }
    }

    // steady state
    for(uint32_t f=0;f<kFrames;++f){
        // fix2: unassigned -> direct BS
        for(uint32_t i=0;i<200;++i){
            auto&n=ns[i]; if(!n.alive||n.isCH)continue;
            if(n.chId==-1){double d=D(n.x,n.y,50,150);use(ns,i,TX(kData,d),"DIRECT-BS[f"+std::to_string(f)+"]");}
        }
        for(uint32_t c=0;c<200;++c){
            if(!ns[c].alive||!ns[c].isCH)continue;
            uint32_t act=0,tot=(uint32_t)ns[c].members.size();
            for(int m:ns[c].members){
                if(!ns[m].alive)continue;
                bool skip=false;
                if(proto==LECMAC){ double ev=D(ns[m].x,ns[m].y,50,50); if(ev>50)skip=true; }
                if(!skip){
                    double d=D(ns[m].x,ns[m].y,ns[c].x,ns[c].y);
                    use(ns,(uint32_t)m,TX(kData,d),"DATA-TX[f"+std::to_string(f)+"]");
                    use(ns,c,RX(kData),"DATA-RX[f"+std::to_string(f)+"]");
                    ++act;
                } else {
                    use(ns,(uint32_t)m,kEidle*kData,"IDLE[f"+std::to_string(f)+"]");
                }
            }
            double dbs=D(ns[c].x,ns[c].y,50,150);
            use(ns,c,TX(kData,dbs),"AGG-BS[f"+std::to_string(f)+"]");
            uint32_t sil=(tot>act)?(tot-act):0;
            if(proto==LEACH) use(ns,c,kEidle*kData*std::max(1u,tot),"CH-IDLE[f"+std::to_string(f)+"]");
            else             use(ns,c,kEidle*kData*(sil+1u),        "CH-IDLE[f"+std::to_string(f)+"]");
        }
    }
    return ns;
}

static void report(const std::vector<Node>&ns, const char*name){
    // find top drainer
    uint32_t top=0; double topD=0;
    for(uint32_t i=0;i<ns.size();++i){ double d=ns[i].e0-ns[i].e; if(d>topD){topD=d;top=i;} }
    const auto&n=ns[top];
    std::cout<<"\n=== "<<name<<" — top drainer: node "<<top<<" ===\n";
    std::cout<<"  Role      : "<<(n.isCH?"CH":"member")<<"\n";
    std::cout<<"  chId      : "<<n.chId<<(n.chId==-1?" (UNASSIGNED->direct BS)":"")<<"\n";
    if(!n.isCH&&n.chId>=0) std::cout<<"  CH members: "<<ns[n.chId].members.size()<<"\n";
    if(n.isCH) std::cout<<"  Members   : "<<n.members.size()<<"\n";
    std::cout<<"  Pos       : ("<<n.x<<","<<n.y<<")\n";
    std::cout<<"  E start   : "<<std::scientific<<n.e0<<" J\n";
    std::cout<<"  E end     : "<<n.e<<" J\n";
    std::cout<<"  Drain     : "<<topD<<" J\n";
    std::cout<<"  Per-op log:\n";
    double sum=0;
    for(auto&l:n.log){ std::cout<<"    "<<std::left<<std::setw(28)<<l.op<<"  "<<l.cost<<"\n"; sum+=l.cost; }
    std::cout<<"    TOTAL                         "<<sum<<"\n";

    // cluster size histogram
    std::map<int,int> hist;
    uint32_t unassigned=0;
    for(auto&nd:ns){ if(nd.isCH)hist[(int)nd.members.size()]++; else if(nd.chId==-1)++unassigned; }
    std::cout<<"  CHs="<<hist.size()<<" unassigned="<<unassigned<<"\n";
    for(auto&[sz,cnt]:hist) std::cout<<"    cluster_size="<<sz<<"  count="<<cnt<<"\n";

    // avg drain by role
    double chD=0,memD=0,unD=0; uint32_t nCH=0,nMem=0,nUn=0;
    for(auto&nd:ns){
        double d=nd.e0-nd.e;
        if(nd.isCH){chD+=d;++nCH;}
        else if(nd.chId==-1){unD+=d;++nUn;}
        else{memD+=d;++nMem;}
    }
    std::cout<<"  Avg drain CH("<<nCH<<")="<<(nCH?chD/nCH:0)<<" J  mem("<<nMem<<")="<<(nMem?memD/nMem:0)<<" J  unassigned("<<nUn<<")="<<(nUn?unD/nUn:0)<<" J\n";
}

int main(){
    auto L=run(LEACH,42);
    auto E=run(LECMAC,42);
    report(L,"LEACH");
    report(E,"LECMAC");
    return 0;
}
