#include <string>
#include <future>
#include <optional>

namespace yakka {
class task_database {
public:
  task_database();
  void load(const std::string path);
  void save(const std::string path);
};
} // namespace yakka
