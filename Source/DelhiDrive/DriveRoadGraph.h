#pragma once

// Standard C++17 so topology/routing can be tested without the Unreal toolchain.
// Coordinates are centimetres. Only shared OSM geometry vertices connect roads;
// geometrically crossing lines do not become invented junctions.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <functional>
#include <map>
#include <queue>
#include <utility>
#include <vector>

namespace DriveRoad
{
struct Point { double X = 0, Y = 0; };
struct Route
{
    std::vector<Point> Path;
    std::vector<double> Widths; // one width per path edge
    std::vector<Point> Checkpoints;
    std::vector<double> CheckpointYaw;
    std::vector<double> CheckpointWidths;
    double Length = 0;
    bool Valid() const { return Path.size() >= 2 && !Checkpoints.empty(); }
};

class Graph
{
public:
    struct Edge { int To; double Width; double Length; };
    struct Node { Point P; std::vector<Edge> Out; std::vector<int> Neighbours; };
    std::vector<Node> Nodes;

    void AddSegment(Point A, Point B, double Width, int Direction = 0)
    {
        if (!std::isfinite(A.X) || !std::isfinite(A.Y) || !std::isfinite(B.X) || !std::isfinite(B.Y)
            || !std::isfinite(Width) || Width < 300 || Width > 5000) return;
        const double Length = std::hypot(B.X - A.X, B.Y - A.Y);
        if (Length < 1) return;
        const int U = FindOrAdd(A), V = FindOrAdd(B);
        if (U == V) return;
        if (Direction >= 0) Nodes[U].Out.push_back({V, Width, Length});
        if (Direction <= 0) Nodes[V].Out.push_back({U, Width, Length});
        Nodes[U].Neighbours.push_back(V);
        Nodes[V].Neighbours.push_back(U);
    }

    Route MakeRoute(int Count = 8, unsigned Seed = 42) const
    {
        Route Result;
        Count = std::clamp(Count, 1, 16);
        if (Nodes.size() < 2) return Result;
        // Retain the largest connected road component instead of choosing a
        // nearby service-road island. Directed reachability is checked below.
        std::vector<int> Component(Nodes.size(), -1), Sizes;
        for (int I = 0; I < static_cast<int>(Nodes.size()); ++I)
        {
            if (Component[I] >= 0) continue;
            const int Id = static_cast<int>(Sizes.size());
            std::vector<int> Queue{I};
            Component[I] = Id;
            for (std::size_t Head = 0; Head < Queue.size(); ++Head)
                for (int Next : Nodes[Queue[Head]].Neighbours)
                    if (Component[Next] < 0) { Component[Next] = Id; Queue.push_back(Next); }
            Sizes.push_back(static_cast<int>(Queue.size()));
        }
        const int Largest = static_cast<int>(std::max_element(Sizes.begin(), Sizes.end()) - Sizes.begin());
        std::vector<int> Starts;
        for (int I = 0; I < static_cast<int>(Nodes.size()); ++I)
            if (Component[I] == Largest && !Nodes[I].Out.empty()) Starts.push_back(I);
        std::sort(Starts.begin(), Starts.end(), [&](int A, int B) {
            return std::hypot(Nodes[A].P.X, Nodes[A].P.Y) < std::hypot(Nodes[B].P.X, Nodes[B].P.Y);
        });
        // Try up to twelve close starts: a one-way cul-de-sac must not strand
        // the player even when it is the closest mapped vertex to the origin.
        double BestLength = -1;
        std::vector<int> BestPath;
        std::vector<double> BestWidths;
        const double Infinity = std::numeric_limits<double>::infinity();
        for (int Attempt = 0; Attempt < std::min(12, static_cast<int>(Starts.size())); ++Attempt)
        {
            const int Start = Starts[Attempt];
            std::vector<double> Distance(Nodes.size(), Infinity), ParentWidth(Nodes.size(), 0);
            std::vector<int> Parent(Nodes.size(), -1);
            using Item = std::pair<double, int>;
            std::priority_queue<Item, std::vector<Item>, std::greater<Item>> Heap;
            Distance[Start] = 0; Heap.push({0, Start});
            while (!Heap.empty())
            {
                const auto Current = Heap.top(); Heap.pop();
                if (Current.first != Distance[Current.second]) continue;
                for (const Edge& E : Nodes[Current.second].Out)
                {
                    const double Candidate = Current.first + E.Length;
                    if (Candidate < Distance[E.To])
                    {
                        Distance[E.To] = Candidate; Parent[E.To] = Current.second;
                        ParentWidth[E.To] = E.Width; Heap.push({Candidate, E.To});
                    }
                }
            }
            std::vector<int> Destinations;
            for (int I = 0; I < static_cast<int>(Nodes.size()); ++I)
                if (I != Start && std::isfinite(Distance[I]) && Distance[I] > 3000) Destinations.push_back(I);
            if (Destinations.empty()) continue;
            std::sort(Destinations.begin(), Destinations.end(), [&](int A, int B) { return Distance[A] > Distance[B]; });
            const int Pick = static_cast<int>(Seed % static_cast<unsigned>(std::min(8, static_cast<int>(Destinations.size()))));
            const int End = Destinations[Pick];
            // Stop at a useful near-origin start once it can support the full
            // course; only search further if directed reachability is poor.
            if (Distance[End] > BestLength)
            {
                BestLength = Distance[End]; BestPath.clear(); BestWidths.clear();
                for (int At = End; At >= 0; At = Parent[At])
                {
                    BestPath.push_back(At);
                    if (Parent[At] >= 0) BestWidths.push_back(ParentWidth[At]);
                }
                std::reverse(BestPath.begin(), BestPath.end());
                std::reverse(BestWidths.begin(), BestWidths.end());
            }
            if (Distance[End] >= 70000) break;
        }
        if (BestPath.size() < 2) return Result;
        for (int Index : BestPath) Result.Path.push_back(Nodes[Index].P);
        Result.Widths = BestWidths;
        Result.Length = BestLength;
        // First two nearby targets teach steering/parking. Later checkpoints
        // cover progressively more of the connected course rather than jumping
        // across blocks. All checkpoints remain on the road centre line.
        for (int Index = 0; Index < Count; ++Index)
        {
            double Along;
            if (Count < 3 || BestLength < 24000)
                Along = BestLength * (Index + 1.0) / (Count + 0.4);
            else if (Index == 0) Along = 6000;
            else if (Index == 1) Along = 12000;
            else Along = 12000 + (BestLength - 13000) * (Index - 1.0) / (Count - 2.0);
            Along = std::clamp(Along, 100.0, BestLength - 100.0);
            double Accumulated = 0;
            for (std::size_t EdgeIndex = 0; EdgeIndex + 1 < Result.Path.size(); ++EdgeIndex)
            {
                const Point A = Result.Path[EdgeIndex], B = Result.Path[EdgeIndex + 1];
                const double Length = std::hypot(B.X - A.X, B.Y - A.Y);
                if (Accumulated + Length >= Along || EdgeIndex + 2 == Result.Path.size())
                {
                    const double T = std::clamp((Along - Accumulated) / Length, 0.0, 1.0);
                    Result.Checkpoints.push_back({A.X + (B.X - A.X) * T, A.Y + (B.Y - A.Y) * T});
                    Result.CheckpointYaw.push_back(std::atan2(B.Y - A.Y, B.X - A.X));
                    Result.CheckpointWidths.push_back(Result.Widths[EdgeIndex]);
                    break;
                }
                Accumulated += Length;
            }
        }
        return Result;
    }

private:
    std::map<std::pair<long long, long long>, int> Lookup;
    int FindOrAdd(Point P)
    {
        // The source exporter rounds to 0.1cm; a centimetre key absorbs only
        // numeric rounding, never snaps two neighbouring parallel carriageways.
        const auto Key = std::make_pair(std::llround(P.X), std::llround(P.Y));
        const auto Found = Lookup.find(Key);
        if (Found != Lookup.end()) return Found->second;
        const int Index = static_cast<int>(Nodes.size());
        Nodes.push_back({P, {}, {}}); Lookup.emplace(Key, Index); return Index;
    }
};
}
