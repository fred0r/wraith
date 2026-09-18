#ifndef _MODULE_H
#define _MODULE_H

#include <cstddef>
#include <memory>
#include <vector>

namespace wraith {

class Module {
public:
	virtual ~Module() = default;
	virtual void init() = 0;
	virtual const char *name() const = 0;
};

class ModuleRegistry {
public:
	static ModuleRegistry& instance() {
		static ModuleRegistry registry;
		return registry;
	}

	void add(std::unique_ptr<Module> module) {
		modules_.push_back(std::move(module));
	}

	void init_pending() {
		for (; next_ < modules_.size(); ++next_)
			modules_[next_]->init();
	}

	size_t size() const { return modules_.size(); }

	const Module *at(size_t i) const { return modules_[i].get(); }

private:
	ModuleRegistry() = default;

	std::vector<std::unique_ptr<Module>> modules_;
	size_t next_ = 0;
};

} /* namespace wraith */

#endif /* !_MODULE_H */
