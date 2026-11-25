#include "../../Source/DelhiDrive/DriveRoadGraph.h"
#include <cassert>
#include <iostream>
#include <set>
#include <fstream>

static bool Same(DriveRoad::Point A, DriveRoad::Point B) { return std::hypot(A.X-B.X,A.Y-B.Y)<1; }
int main(int argc, char** argv)
{
    using namespace DriveRoad;
    if (argc > 1)
    {
        Graph Real;
        std::ifstream Input(argv[1]);
        double AX,AY,BX,BY,Width; int Direction;
        while (Input >> AX >> AY >> BX >> BY >> Width >> Direction)
            Real.AddSegment({AX,AY},{BX,BY},Width,Direction);
        assert(Real.Nodes.size() > 10);
        for (unsigned Seed : {0u, 1u, 42u, 831u})
        {
            const Route RoadRoute = Real.MakeRoute(8,Seed);
            assert(RoadRoute.Valid() && RoadRoute.Checkpoints.size()==8);
            assert(RoadRoute.Length > 24000);
            double Sum=0;
            for (std::size_t I=0; I+1<RoadRoute.Path.size(); ++I)
            {
                bool Found=false;
                for(const auto& N:Real.Nodes) if(Same(N.P,RoadRoute.Path[I]))
                    for(const auto& E:N.Out) if(Same(Real.Nodes[E.To].P,RoadRoute.Path[I+1])) Found=true;
                assert(Found);
                Sum+=std::hypot(RoadRoute.Path[I+1].X-RoadRoute.Path[I].X,RoadRoute.Path[I+1].Y-RoadRoute.Path[I].Y);
            }
            assert(std::abs(Sum-RoadRoute.Length)<1);
            for(const Point& CP:RoadRoute.Checkpoints)
            {
                double Closest=1e20;
                for(std::size_t I=0;I+1<RoadRoute.Path.size();++I)
                {
                    const Point A=RoadRoute.Path[I], B=RoadRoute.Path[I+1];
                    const double DX=B.X-A.X,DY=B.Y-A.Y;
                    const double T=std::clamp(((CP.X-A.X)*DX+(CP.Y-A.Y)*DY)/(DX*DX+DY*DY),0.0,1.0);
                    Closest=std::min(Closest,std::hypot(CP.X-A.X-DX*T,CP.Y-A.Y-DY*T));
                }
                assert(Closest<1); // checkpoint centre never crosses a block
            }
            std::cout<<"seed="<<Seed<<" graph_nodes="<<Real.Nodes.size()<<" path_nodes="<<RoadRoute.Path.size()<<" length_m="<<RoadRoute.Length/100<<" checkpoints="<<RoadRoute.Checkpoints.size()<<"\n";
        }
        return 0;
    }
    Graph G;
    // Main connected bent road; an island sits nearer the origin.
    G.AddSegment({0,0},{0,4000},500);
    G.AddSegment({1000,0},{21000,0},600);
    G.AddSegment({21000,0},{21000,30000},600);
    G.AddSegment({21000,30000},{51000,30000},600);
    const Route R = G.MakeRoute(8,42);
    assert(R.Valid() && R.Checkpoints.size()==8 && R.Widths.size()+1==R.Path.size());
    assert(R.Length>50000);
    assert(std::abs(R.Checkpoints.front().X-7000)<1); // 60m from main-component start
    for (std::size_t I=0; I+1<R.Path.size(); ++I)
    {
        bool Found=false;
        for (const auto& N:G.Nodes) if (Same(N.P,R.Path[I]))
            for(const auto& E:N.Out) if(Same(G.Nodes[E.To].P,R.Path[I+1])) Found=true;
        assert(Found); // every route step is a real graph edge
    }
    assert(Same(R.Path[0],G.MakeRoute(8,42).Path[0]));
    // A geometric crossing without a shared OSM vertex must remain disconnected.
    Graph Crossing;
    Crossing.AddSegment({-10000,0},{10000,0},600);
    Crossing.AddSegment({0,-10000},{0,10000},600);
    assert(Crossing.Nodes.size()==4);
    assert(Crossing.MakeRoute().Path.size()==2);
    // Explicit one-way orientation may never be reversed by path construction.
    Graph OneWay;
    OneWay.AddSegment({0,0},{10000,0},600,1);
    OneWay.AddSegment({10000,0},{20000,0},600,1);
    OneWay.AddSegment({20000,0},{30000,0},600,1);
    const Route Directed=OneWay.MakeRoute(3,0);
    assert(Directed.Valid());
    for(std::size_t I=1;I<Directed.Path.size();++I) assert(Directed.Path[I].X>Directed.Path[I-1].X);
    // Zero-length and invalid-width segments never produce malformed graph state.
    Graph Invalid;
    Invalid.AddSegment({0,0},{0,0},600);
    Invalid.AddSegment({0,0},{1000,0},0);
    assert(!Invalid.MakeRoute().Valid());
    std::cout<<"Road graph scenarios passed: disconnected islands, ordered route, one-way, geometric crossing, invalid inputs\n";
}
