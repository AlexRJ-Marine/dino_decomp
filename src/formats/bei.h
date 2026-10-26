#pragma once

#include <string>
#include <vector>

namespace dino {

// One `.bei` employee profile (`Data/Design/employees/<Name>.bei`).
// Header is 15 little-endian dwords; the body is an interleaved stream of
// numeric records and length-prefixed ASCII asset/sound names.
struct EmployeeProfile {
    std::string name;         // EmployeeList.ini entry
    std::string path;
    int kind = 0;             // 0 Nutritionist, 1 Guard
    float height = 1.7f;
    float walk_speed = 1.0f;
    float action_speed = 2.5f;
    int wage = 500;           // header dword 8: hire cost and daily wage
    float work_duration = 3.0f;  // header dword 10, used by the feed action
    std::vector<std::string> assets;   // DATA\... strings
    std::vector<std::string> meshes;   // asset .mesh paths
    std::vector<std::string> sounds;   // non-DATA strings
};

// Reads one profile; `name` is the EmployeeList name (defaults to the stem).
EmployeeProfile load_bei(const std::string& path,
                         const std::string& name = {});

// Reads `EmployeeList.ini` from `dir` and every listed `<Name>.bei`, in list
// order. Throws when the list or a profile is missing/malformed.
std::vector<EmployeeProfile> load_employee_list(const std::string& dir);

}  // namespace dino
