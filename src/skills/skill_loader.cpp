#include "skill_loader.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace oop_agent::skills {
namespace {

std::string trim(std::string value) {
    const auto is_not_space = [](unsigned char character) {
        return std::isspace(character) == 0;
    };

    const auto begin = std::find_if(value.begin(), value.end(), is_not_space);
    if (begin == value.end()) {
        return {};
    }
    const auto end = std::find_if(value.rbegin(), value.rend(), is_not_space).base();
    return std::string(begin, end);
}

std::uint32_t decodeUtf8(std::string_view text, std::size_t &position) {
    const auto first = static_cast<unsigned char>(text[position++]);
    if (first < 0x80U) {
        return first;
    }

    int continuation_count = 0;
    std::uint32_t code_point = 0;
    if ((first & 0xE0U) == 0xC0U) {
        continuation_count = 1;
        code_point = first & 0x1FU;
    } else if ((first & 0xF0U) == 0xE0U) {
        continuation_count = 2;
        code_point = first & 0x0FU;
    } else if ((first & 0xF8U) == 0xF0U) {
        continuation_count = 3;
        code_point = first & 0x07U;
    } else {
        return 0;
    }

    if (position + static_cast<std::size_t>(continuation_count) > text.size()) {
        position = text.size();
        return 0;
    }

    for (int index = 0; index < continuation_count; ++index) {
        const auto continuation = static_cast<unsigned char>(text[position]);
        if ((continuation & 0xC0U) != 0x80U) {
            return 0;
        }
        ++position;
        code_point = (code_point << 6U) | (continuation & 0x3FU);
    }
    return code_point;
}

char foldVietnamese(std::uint32_t code_point) {
    if ((code_point >= 0x1EA0U && code_point <= 0x1EB7U) ||
        code_point == 0x0102U || code_point == 0x0103U ||
        (code_point >= 0x00C0U && code_point <= 0x00C5U) ||
        (code_point >= 0x00E0U && code_point <= 0x00E5U)) {
        return 'a';
    }
    if ((code_point >= 0x1EB8U && code_point <= 0x1EC7U) ||
        code_point == 0x00C8U || code_point == 0x00C9U ||
        code_point == 0x00CAU || code_point == 0x00CBU ||
        code_point == 0x00E8U || code_point == 0x00E9U ||
        code_point == 0x00EAU || code_point == 0x00EBU) {
        return 'e';
    }
    if ((code_point >= 0x1EC8U && code_point <= 0x1ECBU) ||
        (code_point >= 0x00CCU && code_point <= 0x00CFU) ||
        (code_point >= 0x00ECU && code_point <= 0x00EFU)) {
        return 'i';
    }
    if ((code_point >= 0x1ECCU && code_point <= 0x1EE3U) ||
        code_point == 0x01A0U || code_point == 0x01A1U ||
        (code_point >= 0x00D2U && code_point <= 0x00D6U) ||
        (code_point >= 0x00F2U && code_point <= 0x00F6U)) {
        return 'o';
    }
    if ((code_point >= 0x1EE4U && code_point <= 0x1EF1U) ||
        code_point == 0x01AFU || code_point == 0x01B0U ||
        (code_point >= 0x00D9U && code_point <= 0x00DCU) ||
        (code_point >= 0x00F9U && code_point <= 0x00FCU)) {
        return 'u';
    }
    if ((code_point >= 0x1EF2U && code_point <= 0x1EF9U) ||
        code_point == 0x00DDU || code_point == 0x00FDU ||
        code_point == 0x00FFU) {
        return 'y';
    }
    if (code_point == 0x0110U || code_point == 0x0111U) {
        return 'd';
    }
    return '\0';
}

std::string normalizeForMatch(std::string_view text) {
    std::string normalized;
    normalized.reserve(text.size());
    bool previous_was_space = true;

    std::size_t position = 0;
    while (position < text.size()) {
        const auto code_point = decodeUtf8(text, position);
        char character = '\0';
        if (code_point < 0x80U) {
            const auto ascii = static_cast<unsigned char>(code_point);
            if (std::isalnum(ascii) != 0) {
                character = static_cast<char>(std::tolower(ascii));
            }
        } else {
            character = foldVietnamese(code_point);
        }

        if (character != '\0') {
            normalized.push_back(character);
            previous_was_space = false;
        } else if (!previous_was_space) {
            normalized.push_back(' ');
            previous_was_space = true;
        }
    }

    if (!normalized.empty() && normalized.back() == ' ') {
        normalized.pop_back();
    }
    return normalized;
}

std::vector<std::string> parseKeywords(const std::string &line) {
    const auto normalized_line = normalizeForMatch(line);
    constexpr std::string_view prefix = "keywords ";
    if (normalized_line.rfind(prefix, 0) != 0) {
        return {};
    }

    const auto separator = line.find(':');
    if (separator == std::string::npos) {
        return {};
    }

    std::vector<std::string> keywords;
    std::size_t start = separator + 1;
    while (start <= line.size()) {
        const auto comma = line.find(',', start);
        auto keyword = trim(line.substr(start, comma - start));
        if (!keyword.empty()) {
            keywords.push_back(std::move(keyword));
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    return keywords;
}

Skill readSkill(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("could not open skill file: " + path.string());
    }

    std::string content((std::istreambuf_iterator<char>(input)),
                        std::istreambuf_iterator<char>());
    if (input.bad()) {
        throw std::runtime_error("could not read skill file: " + path.string());
    }
    if (content.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        content.erase(0, 3);
    }
    if (trim(content).empty()) {
        throw std::runtime_error("skill file is empty: " + path.string());
    }

    Skill skill;
    skill.name = path.stem().string();
    skill.source_path = path;

    std::string instructions;
    std::size_t line_start = 0;
    while (line_start <= content.size()) {
        const auto line_end = content.find('\n', line_start);
        const auto raw_line = content.substr(line_start, line_end - line_start);
        const auto keywords = parseKeywords(raw_line);
        if (!keywords.empty()) {
            skill.keywords.insert(skill.keywords.end(), keywords.begin(), keywords.end());
        } else {
            instructions.append(raw_line);
            if (line_end != std::string::npos) {
                instructions.push_back('\n');
            }
        }
        if (line_end == std::string::npos) {
            break;
        }
        line_start = line_end + 1;
    }

    if (skill.keywords.empty()) {
        std::string fallback = skill.name;
        std::replace(fallback.begin(), fallback.end(), '_', ' ');
        std::replace(fallback.begin(), fallback.end(), '-', ' ');
        skill.keywords.push_back(std::move(fallback));
    }
    skill.instructions = trim(std::move(instructions));
    return skill;
}

bool hasMarkdownExtension(const std::filesystem::path &path) {
    auto extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return extension == ".md";
}

std::size_t keywordScore(std::string_view normalized_task,
                         const std::vector<std::string> &keywords) {
    const std::string padded_task = " " + std::string(normalized_task) + " ";
    std::size_t score = 0;
    for (const auto &keyword : keywords) {
        const auto normalized_keyword = normalizeForMatch(keyword);
        if (normalized_keyword.empty()) {
            continue;
        }
        const std::string needle = " " + normalized_keyword + " ";
        if (padded_task.find(needle) != std::string::npos) {
            score += 10 + static_cast<std::size_t>(
                              std::count(normalized_keyword.begin(),
                                         normalized_keyword.end(), ' '));
        }
    }
    return score;
}

} // namespace

SkillLoader::SkillLoader(std::filesystem::path skills_directory)
    : skills_directory_(std::move(skills_directory)) {
    if (skills_directory_.empty()) {
        throw std::invalid_argument("skills directory must not be empty");
    }
}

std::size_t SkillLoader::loadSkills() {
    std::error_code error;
    if (!std::filesystem::exists(skills_directory_, error) || error) {
        throw std::runtime_error("skills directory does not exist: " +
                                 skills_directory_.string());
    }
    if (!std::filesystem::is_directory(skills_directory_, error) || error) {
        throw std::runtime_error("skills path is not a directory: " +
                                 skills_directory_.string());
    }

    std::vector<std::filesystem::path> paths;
    for (std::filesystem::directory_iterator iterator(skills_directory_, error), end;
         iterator != end; iterator.increment(error)) {
        if (error) {
            throw std::runtime_error("could not scan skills directory: " +
                                     error.message());
        }
        if (iterator->is_regular_file(error) && !error &&
            hasMarkdownExtension(iterator->path())) {
            paths.push_back(iterator->path());
        }
        error.clear();
    }
    if (error) {
        throw std::runtime_error("could not scan skills directory: " +
                                 error.message());
    }

    std::sort(paths.begin(), paths.end());
    std::vector<Skill> loaded;
    loaded.reserve(paths.size());
    for (const auto &path : paths) {
        loaded.push_back(readSkill(path));
    }
    skills_ = std::move(loaded);
    return skills_.size();
}

const std::filesystem::path &SkillLoader::skillsDirectory() const noexcept {
    return skills_directory_;
}

const std::vector<Skill> &SkillLoader::loadedSkills() const noexcept {
    return skills_;
}

std::vector<Skill> SkillLoader::selectSkills(std::string_view task,
                                             std::size_t max_skills) const {
    if (max_skills == 0 || task.empty()) {
        return {};
    }

    struct Candidate {
        std::size_t score;
        const Skill *skill;
    };

    const auto normalized_task = normalizeForMatch(task);
    std::vector<Candidate> candidates;
    for (const auto &skill : skills_) {
        const auto score = keywordScore(normalized_task, skill.keywords);
        if (score > 0) {
            candidates.push_back({score, &skill});
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate &left, const Candidate &right) {
                  if (left.score != right.score) {
                      return left.score > right.score;
                  }
                  return left.skill->name < right.skill->name;
              });

    std::vector<Skill> selected;
    selected.reserve(std::min(max_skills, candidates.size()));
    for (std::size_t index = 0;
         index < candidates.size() && index < max_skills; ++index) {
        selected.push_back(*candidates[index].skill);
    }
    return selected;
}

} // namespace oop_agent::skills
