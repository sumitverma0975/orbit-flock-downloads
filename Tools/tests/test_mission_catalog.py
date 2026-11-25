#!/usr/bin/env python3
"""Compile real catalog/rule code with a tiny CoreMinimal shim; no Unreal build claimed."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "Source" / "DelhiDrive"


def main():
    compiler = shutil.which("c++") or shutil.which("g++")
    if not compiler:
        raise SystemExit("A C++17 compiler is required")
    with tempfile.TemporaryDirectory(prefix="delhidrive-missions-") as temp:
        temp = Path(temp)
        (temp / "CoreMinimal.h").write_text('''#pragma once
#include <cstdint>
#include <algorithm>
using uint8 = uint8_t;
using int32 = int32_t;
using TCHAR = char;
#define TEXT(x) x
#define UE_ARRAY_COUNT(x) (sizeof(x)/sizeof((x)[0]))
struct FMath { template<class T> static T Clamp(T x,T lo,T hi) {return std::clamp(x,lo,hi);} };
''')
        (temp / "catalog_test.cpp").write_text('''#include "LevelCatalog.h"
#include <cassert>
#include <set>
#include <iostream>
int main() {
 assert(DriveLevels::NumLevels()==12);
 assert(DriveLevels::NumLessons()==6);
 std::set<EDriveObjective> Goals;
 for(int Mode=0;Mode<2;++Mode) {
  int N=Mode?DriveLevels::NumLessons():DriveLevels::NumLevels();
  for(int i=0;i<N;++i) {
   const auto& L=Mode?DriveLevels::Lesson(i):DriveLevels::Level(i);
   assert(L.Name && *L.Name && L.Instruction && *L.Instruction);
   assert(L.Checkpoints>=1 && L.Checkpoints<=8);
   assert(L.TimeMult>0.f && L.ScoreMult>0.f);
   assert(L.TrafficMult>=0.f && L.PotholeMult>=0.f);
   assert(L.SpeedLimitKmh>=25.f && L.SpeedLimitKmh<=60.f);
   assert(L.SpeedingAllowanceSeconds>0.f && L.MaxCrashes>0 && L.MaxHarshEvents>0);
   assert(L.StartingFuel>L.RequiredFuel && L.StartingFuel<=100.f && L.RequiredFuel>=0.f);
   if(L.Objective==EDriveObjective::ControlledStop || L.Objective==EDriveObjective::Parking) assert(L.StopHoldSeconds>=2.f);
   if(L.Objective==EDriveObjective::FuelSave) assert(L.StartingFuel<50.f && L.RequiredFuel>0.f);
   Goals.insert(L.Objective);
  }
 }
 assert(Goals.size()==6);
 assert(&DriveLevels::Level(-1)==&DriveLevels::Level(0));
 assert(&DriveLevels::Lesson(999)==&DriveLevels::Lesson(5));
 assert(DriveLevels::Lesson(0).TrafficMult==0.f && DriveLevels::Lesson(0).PotholeMult==0.f);
 std::cout<<"Catalog: 12 career challenges and 6 lessons passed all rule invariants\\n";
}
''')
        for test, sources in [
            ("catalog", [temp / "catalog_test.cpp", SOURCE / "LevelCatalog.cpp"]),
            ("rules", [ROOT / "Tools/tests/test_mission_rules.cpp"]),
        ]:
            executable = temp / test
            subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-I", str(temp), "-I", str(SOURCE), *map(str, sources), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
