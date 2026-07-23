#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace oop_agent::skills {

struct Skill {
    std::string name;
    std::filesystem::path source_path;
    std::string instructions;
    std::vector<std::string> keywords;
};

class SkillLoader {
  public:
    explicit SkillLoader(std::filesystem::path skills_directory = "skills");

    // Reloading before an agent run lets newly added markdown skills become
    // available without recompiling the application.
    std::size_t loadSkills();

    const std::filesystem::path &skillsDirectory() const noexcept;
    const std::vector<Skill> &loadedSkills() const noexcept;

    std::vector<Skill> selectSkills(std::string_view task,
                                    std::size_t max_skills = 3) const;

  private:
    std::filesystem::path skills_directory_;
    std::vector<Skill> skills_;
};

} // namespace oop_agent::skills
